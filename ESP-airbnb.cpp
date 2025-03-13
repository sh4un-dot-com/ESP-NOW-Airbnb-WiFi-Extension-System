#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <qrcode.h>
#include <esp_random.h>

// Configuration
#define EEPROM_SIZE 512
#define CONFIG_SSID_ADDR 0
#define CONFIG_PASSWORD_ADDR 32
#define CONFIG_CONTROLLER_MAC_ADDR 64
#define CONFIG_REPEATER_NAME_ADDR 72
#define CONFIG_GUEST_PASSWORD_ADDR 128

// Controller MAC address (replace with your controller's MAC)
uint8_t controllerMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Change this!

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
char repeaterName[32] = "Repeater";
char guestPassword[32] = "password123";

// Controller variables
struct peerInfo {
  uint8_t macAddr[6];
  int channel;
  bool active;
  char name[32];
};

std::vector<peerInfo> peers;

// ESP-NOW data structure
typedef struct struct_message {
  uint8_t senderMAC[6];
  int messageType; // 0 = Registration, 1 = Routing, 2 = Data, 3 = Guest Data, 4 = QR Data
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
bool shouldRedirectToPortal();
void sendGuestData(uint8_t *destMAC, const char *data);
void processGuestData(const uint8_t *mac, const char *payload);
void loadConfig();
void saveConfig();
void generateQRCode(const char *data);
void handleQRData(const char *payload);

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();
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

void loadConfig() {
  EEPROM.get(CONFIG_CONTROLLER_MAC_ADDR, controllerMAC);
  EEPROM.get(CONFIG_REPEATER_NAME_ADDR, repeaterName);
  EEPROM.get(CONFIG_GUEST_PASSWORD_ADDR, guestPassword);
}

void saveConfig() {
  EEPROM.put(CONFIG_CONTROLLER_MAC_ADDR, controllerMAC);
  EEPROM.put(CONFIG_REPEATER_NAME_ADDR, repeaterName);
  EEPROM.put(CONFIG_GUEST_PASSWORD_ADDR, guestPassword);
  EEPROM.commit();
}

void setupRepeater() {
  char ssid[32];
  char password[64];
  EEPROM.get(CONFIG_SSID_ADDR, ssid);
  EEPROM.get(CONFIG_PASSWORD_ADDR, password);
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
  server.on("/setup", handleSetup);
  server.on("/qrcode", handleQRCode);
  server.onNotFound(handleNotFound);
  server.begin();
}

void handleSetup() {
  if (server.method() == HTTP_POST) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    ssid.toCharArray((char*)EEPROM.getDataPtr() + CONFIG_SSID_ADDR, 32);
    password.toCharArray((char*)EEPROM.getDataPtr() + CONFIG_PASSWORD_ADDR, 64);
    EEPROM.commit();
    server.send(200, "text/plain", "Setup saved");
  } else {
    String html = "<h1>Controller Setup</h1><form method='POST'>SSID: <input name='ssid'><br>Password: <input name='password'><br><input type='submit'></form>";
    server.send(200, "text/html", html);
  }
}

void handleQRCode() {
    String qrData = "WIFI:S:" + String((char*)EEPROM.getDataPtr() + CONFIG_SSID_ADDR) + ";T:WPA;P:" + String((char*)EEPROM.getDataPtr() + CONFIG_PASSWORD_ADDR) + ";;";
    generateQRCode(qrData.c_str());
}

void generateQRCode(const char *data) {
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(3)];
    qrcode_initText(&qrcode, qrcodeData, 3, 0, data);

    String html = "<html><body><img src='data:image/svg+xml;utf8,";
    html += "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 ";
    html += qrcode.size;
    html += " ";
    html += qrcode.size;
    html += "'>";

    for (byte y = 0; y < qrcode.size; y++) {
        for (byte x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                html += "<rect x='";
                html += x;
                html += "' y='";
                html += y;
                html += "' width='1' height='1' fill='black'/>";
            }
        }
    }

    html += "</svg>'></body></html>";
    server.send(200, "text/html", html);
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
  } else if (myData.messageType == 4){
    handleQRData(myData.payload);
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
  strcpy(newPeer.name, "Repeater");
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
  html += "<a href='/setup'>Controller Setup</a><br>";
  html += "<a href='/qrcode'>Generate WiFi QR Code</a><br>";
  for (auto peer : peers) {
    html += "<p>MAC: ";
    for (int i = 0; i < 6; i++) {
      html += String(peer.macAddr[i], HEX);
      if (i < 5) html += ":";
    }
    html += ", Channel: " + String(peer.channel) + ", Name: " + String(peer.name) + "</p>";
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
    String apSSID = "Guest_Airbnb_" + String(repeaterName);
    WiFi.softAP(apSSID.c_str(), guestPassword);
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

void handleQRData(const char *payload) {
  StaticJsonDocument doc;
  deserializeJson(doc, payload);
  if (doc.containsKey("ssid") && doc.containsKey("password")) {
    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();
    ssid.toCharArray((char*)EEPROM.getDataPtr() + CONFIG_SSID_ADDR, 32);
    password.toCharArray((char*)EEPROM.getDataPtr() + CONFIG_PASSWORD_ADDR, 64);
    EEPROM.commit();
    Serial.println("QR Code WiFi Config Received and saved");
    WiFi.disconnect();
    setupRepeater();
  }
}
