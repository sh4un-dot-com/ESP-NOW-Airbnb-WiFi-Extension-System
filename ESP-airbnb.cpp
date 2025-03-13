#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiClient.h>

// Replace with your hotel WiFi credentials (for controller)
const char *ssid = "HotelWiFi";
const char *password = "HotelPassword";

// Controller MAC address (replace with your controller's MAC)
uint8_t controllerMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Change this!

// ESP-NOW channel
const int espNowChannel = 1;

// Web server port
const int webServerPort = 80;

// Device role (0 = Controller, 1 = Repeater)
int deviceRole = 1; // Default to repeater

// Repeater variables
uint8_t repeaterMAC[6];
bool repeaterRegistered = false;
int repeaterChannel = espNowChannel;
WiFiClient repeaterClient;
WebServer server(webServerPort);
DNSServer dnsServer;

// Controller variables
struct peerInfo {
  uint8_t macAddr[6];
  int channel;
  bool active;
};

std::vector<peerInfo> peers;

// ESP-NOW data structure
typedef struct struct_message {
  uint8_t senderMAC[6];
  int messageType; // 0 = Registration, 1 = Routing, 2 = Data, 3= Guest Data
  char payload[250];
} struct_message;

struct_message myData;

// Function prototypes
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);
void sendRegistrationRequest();
void handleRegistration(const uint8_t *mac, const char *payload);
void sendRoutingInfo(uint8_t *mac);
void handleRoutingInfo(const char *payload);
void setupRepeater();
void setupController();
void handleRoot();
void handleNotFound();
void sendData(uint8_t *destMAC, const char *data);
void processData(const uint8_t *mac, const char *payload);
void setupAP();
void handleCaptivePortal();
void sendGuestData(uint8_t *destMAC, const char *data);
void processGuestData(const uint8_t *mac, const char *payload);

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Check if controller or repeater
  if (WiFi.macAddress() == String(controllerMAC[0], HEX) + ":" + String(controllerMAC[1], HEX) + ":" + String(controllerMAC[2], HEX) + ":" + String(controllerMAC[3], HEX) + ":" + String(controllerMAC[4], HEX) + ":" + String(controllerMAC[5], HEX)) {
    deviceRole = 0; // Controller
    setupController();
  } else {
    setupRepeater();
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
}

void loop() {
  if (deviceRole == 0) {
    server.handleClient();
  } else if (deviceRole == 1 && repeaterRegistered) {
    dnsServer.processNextRequest();
    server.handleClient();
  }
}

void setupRepeater() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  sendRegistrationRequest();
}

void setupController() {
  Serial.println("Controller setup");
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
}

void sendRegistrationRequest() {
  myData.messageType = 0; // Registration
  memcpy(myData.senderMAC, WiFi.macAddress().c_str(), 6);
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, controllerMAC, 6);
  peerInfo.channel = espNowChannel;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  esp_now_send(controllerMAC, (uint8_t *)&myData, sizeof(myData));
  Serial.println("Registration request sent");
}

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  if (myData.messageType == 0 && deviceRole == 0) {
    handleRegistration(myData.senderMAC, myData.payload);
  } else if (myData.messageType == 1 && deviceRole == 1) {
    handleRoutingInfo(myData.payload);
  } else if (myData.messageType == 2){
    processData(myData.senderMAC, myData.payload);
  } else if (myData.messageType == 3){
    processGuestData(myData.senderMAC, myData.payload);
  }

}

void handleRegistration(const uint8_t *mac, const char *payload) {
  Serial.print("Registration request from: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  peerInfo newPeer;
  memcpy(newPeer.macAddr, mac, 6);
  newPeer.channel = espNowChannel; // Assign a channel
  newPeer.active = true;
  peers.push_back(newPeer);
  sendRoutingInfo(mac);
}

void sendRoutingInfo(uint8_t *mac) {
  StaticJsonDocument doc;
  doc["channel"] = espNowChannel;
  char buffer[200];
  serializeJson(doc, buffer);
  myData.messageType = 1; // Routing info
  strcpy(myData.payload, buffer);
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = espNowChannel;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  esp_now_send(mac, (uint8_t *)&myData, sizeof(myData));
  Serial.println("Routing info sent");
}

void handleRoutingInfo(const char *payload) {
  StaticJsonDocument doc;
  deserializeJson(doc, payload);
  repeaterChannel = doc["channel"];
  Serial.print("Routing info received, channel: ");
  Serial.println(repeaterChannel);
  repeaterRegistered = true;
  setupAP();
}

void handleRoot() {
  String html = "<h1>ESP-NOW Mesh Controller</h1>";
  for (auto peer : peers) {
    html += "<p>MAC: ";
    for (int i = 0; i < 6; i++) {
      html += String(peer.macAddr[i], HEX);
      if (i < 5) html += ":";
    }
    html += ", Channel: " + String(peer.channel) + "</p>";
  }
  server.send(200, "text/html", html);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void sendData(uint8_t *destMAC, const char *data){
  myData.messageType = 2;
  strcpy(myData.payload, data);
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, destMAC, 6);
  peerInfo.channel = espNowChannel;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  esp_now_send(destMAC, (uint8_t *)&myData, sizeof(myData));
}

void processData(const uint8_t *mac, const char *payload){
  Serial.print("Data from: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.print(", Payload: ");
  Serial.println(payload);
}

void setupAP(){
  if(repeaterRegistered){
    WiFi.mode(WIFI_AP_STA);
    String apSSID = "Guest_Airbnb_" + String(WiFi.macAddress().substring(12), HEX);
    WiFi.softAP(apSSID.c_str(), "password123"); // Consider a more secure password
    Serial.print("AP SSID: ");
    Serial.println(apSSID);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    dnsServer.start(53, "*", WiFi.softAPIP());
    server.onNotFound(handleCaptivePortal);
    server.begin();
  }
}

void handleCaptivePortal() {
  if (!shouldRedirectToPortal()) {
    server.send(404, "text/plain", "Not Found");
    return;
  }
  String html = "<h1>Welcome to Airbnb Guest WiFi</h1>";
  html += "<p>Please agree to the terms of service.</p>";
  html += "<a href='/connect'>Connect</a>";
  server.send(200, "text/html", html);
}

bool shouldRedirectToPortal() {
  if (server.client().localIP() == WiFi.softAPIP()) {
    return false; // Don't redirect if the request is from the AP itself.
  }

  String hostHeader = server.header("Host");
  if (hostHeader.indexOf(WiFi.softAPIP().toString()) != -1) {
    return false; // Don't redirect if the host is the AP IP.
  }
  return true;
}

void sendGuestData(uint8_t *destMAC, const char *data){
  myData.messageType = 3;
  strcpy(myData.payload, data);
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, destMAC, 6);
  peerInfo.channel = espNowChannel;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  esp_now_send(destMAC, (uint8_t *)&myData, sizeof(myData));
}

void processGuestData(const uint8_t *mac, const char *payload){
  Serial.print("Guest Data from: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.print(", Payload: ");
  Serial.println(payload);
}
