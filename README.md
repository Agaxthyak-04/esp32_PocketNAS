# ☁️ ESP32 Pocket NAS

A lightweight, fully functional Network Attached Storage (NAS) built on an ESP32. This project turns an ESP32 and a MicroSD card into a private local cloud complete with a sleek dark-mode web dashboard.

## ✨ Features
* **Modern Web UI:** Dark-mode interface with responsive design.
* **Drag & Drop Uploads:** Easily upload files directly from your browser.
* **Media Streaming:** Stream `.mp4` and `.mp3` files directly in the browser via a modal player without having to download them first (supports chunked streaming).
* **File Management:** Delete files, browse directories, and search for specific files.
* **Storage Size:** Supports standard MicroSD cards (FAT32 recommended).

## 🛠️ Hardware Requirements
* ESP32 Development Board (e.g., NodeMCU ESP32, ESP32 WROOM)
* MicroSD Card Module (SPI interface)
* MicroSD Card (Formatted to FAT32)
* Jumper wires

## 🔌 Wiring Guide
Connect your MicroSD Card Module to the ESP32 using the standard VSPI pins:
| SD Module Pin | ESP32 Pin |
| :--- | :--- |
| VCC / 5V | VIN or 5V (Do not use 3.3V if your module has a regulator) |
| GND | GND |
| CS | GPIO 5 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| SCK | GPIO 18 |

## 💻 Installation
1. Download or clone this repository.
2. Open the `.ino` file in the Arduino IDE.
3. Install the required libraries via the Library Manager:
   * `SdFat` (by Bill Greiman)
   * `ArduinoJson` (by Benoit Blanchon)
   * `ESPAsyncWebServer` (Mathieu Carbou's updated fork recommended for ESP32 Core 3.x)
   * `AsyncTCP` (Mathieu Carbou's updated fork recommended for ESP32 Core 3.x)
4. Update the Wi-Fi credentials in the code:
   ```cpp
   const char* ssid     = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";

5.Compile and upload to your ESP32.

6.Open the Arduino Serial Monitor (115200 baud) to find the IP address.

7.Enter the IP address into your web browser to access your NAS!

⚠️ Troubleshooting
Boot Loop / TCP Panic: If your ESP32 constantly reboots with a TCP_alloc panic, you are likely using the ESP32 v3.x core with the old, unmaintained version of ESPAsyncWebServer and AsyncTCP. Ensure you either downgrade your ESP32 board package to v2.0.17 OR use the updated forks of the libraries mentioned above.

SD Mount Failed: Try lowering the SPI speed in the code from 25 to 16, ensure your SD card is formatted to FAT32, and double-check that your SD module is getting 5V power if it requires it.
