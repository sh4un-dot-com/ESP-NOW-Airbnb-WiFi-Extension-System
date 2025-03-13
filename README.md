# ESP-NOW Airbnb WiFi Extension System

This project provides a cost-effective and easy-to-deploy WiFi extension solution for Airbnb hosts, leveraging ESP32 microcontrollers and the ESP-NOW protocol.

## Overview

This system uses a mesh network of ESP32 devices to extend the range of an existing WiFi network. One ESP32 acts as a controller, connecting to the main WiFi network, while other ESP32s act as repeaters, creating separate guest WiFi access points. The system is designed to be "plug-and-play," minimizing the need for complex configuration.

## Features

* **Easy Setup:** Pre-configured kits and simple instructions for quick deployment.
* **Guest Network Isolation:** Separate guest WiFi network for enhanced security and privacy.
* **Captive Portal:** Basic captive portal for guest agreement to terms of service.
* **Web-Based Monitoring (Controller):** Simple web interface for monitoring network status and connected repeaters.
* **Remote Management (Potential future feature):** Ability to remotely reboot repeaters and change settings.
* **ESP-NOW Mesh Networking:** Reliable and low-latency communication between ESP32 devices.
* **OTA Updates (Potential future feature):** Over-the-air firmware updates for easy maintenance.

## Hardware Requirements

* ESP32 development boards (number depends on the size of the property).
* Power supplies for each ESP32.

## Software Requirements

* Arduino IDE with ESP32 board support.
* Required Arduino libraries:
    * WiFi
    * ESP-NOW
    * ArduinoJson
    * WebServer
    * DNSServer

## Installation

1.  **Arduino IDE Setup:**
    * Install the Arduino IDE.
    * Add ESP32 board support to the Arduino IDE.
    * Install the required libraries.
2.  **Code Upload:**
    * Open the `main.cpp` file in the Arduino IDE.
    * **Controller Setup:**
        * Update the `ssid` and `password` variables with your main WiFi credentials.
        * Obtain the MAC address of the ESP32 that will act as the controller.
        * Update the `controllerMAC` variable with that MAC address.
        * Upload the code to the controller ESP32.
    * **Repeater Setup:**
        * Obtain the MAC address of the controller ESP32.
        * Update the `controllerMAC` variable with that MAC address.
        * Upload the code to the repeater ESP32s.
3.  **Deployment:**
    * Power on the controller ESP32 and connect it to your main WiFi network.
    * Place the repeater ESP32s in strategic locations throughout the property.
    * The repeaters should automatically register with the controller and create their guest WiFi access points.

## Usage

* **Guest Access:** Guests can connect to the "Guest\_Airbnb\_[MAC Address]" WiFi network.
* **Controller Monitoring:** Access the controller's web interface by navigating to its IP address in a web browser.
* **Captive Portal:** Guests will be redirected to a captive portal page to agree to the terms of service before accessing the internet.

## Important Notes

* This project is a work in progress.
* Security is crucial. Ensure strong passwords and consider implementing additional security measures.
* Thoroughly test the system in a real-world environment.
* Consider legal and ethical implications before deploying any network extension system.
* This project is designed for small to medium sized properties. Larger properties may require more robust solutions.
* ESP-NOW is not a mesh networking protocol. It is used to create a mesh like network in this project, and therefore has limitations.

## Future Improvements

* Implement a more advanced captive portal with guest information collection and terms of service management.
* Develop a mobile app for easier monitoring and management.
* Add remote management capabilities for repeaters.
* Implement automatic channel selection and interference mitigation.
* Improve error handling and reliability.
* Implement OTA updates.
* Add more detailed network statistics to the web interface.
* Implement bandwidth management (QoS).
* Add stronger security features.
* Add a QR code based setup for the controller.
* Add a QR code based wifi access for the guests.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues.
