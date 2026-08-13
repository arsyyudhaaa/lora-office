# 📡 LoRa-Based Office Environment Monitoring System

A wireless server room environment monitoring system based on LoRa (Long Range) technology. This project is designed to operate independently of the local Wi-Fi network in the server area, providing early warnings against potential component overheating or electrical short circuits. It features a server architecture that supports the dynamic addition of new devices (Dynamic JSON Allocation).

---

## 🌟 Key Features
* **Long-Range Communication (LoRa):** Penetrates thick server room walls without relying on local Wi-Fi routers.
* **Early Short Circuit Detection (IAQ):** Uses the BME680 sensor to "smell" organic gases (VOCs) released when cable insulation begins to melt due to heat, long before thick smoke or fire appears.
* **Dynamic Node Allocation:** The ESP32 gateway automatically registers new rooms into the JSON database without needing to modify or hardcode the server-side code.
* **Static IP API Endpoint:** Facilitates easy integration with third-party monitoring dashboards.

---

## 🛠️ Hardware Components

**1. Gateway & Web Server Node**
* 1x Seeed Studio XIAO nRF52840 (as LoRa Receiver)
* 1x Wio SX1262 LoRa Kit
* 1x ESP32 Development Board (as Aggregator & Web Server)
  
**2. Server Room Node**
* 1x Seeed Studio XIAO nRF52840
* 1x Wio SX1262 LoRa Kit
* 1x BME680 Environmental & Gas Sensor (I2C)
* 1x 1.3" OLED Display (I2C)



---

## 🔌 Wiring Guide

### Server Room Node
Due to pin limitations on the XIAO nRF52840 when paired with the Wio SX1262, the sensor and OLED display are connected in parallel on the I2C bus.

| Component | Pin (XIAO nRF52840) |
| :--- | :--- |
| **BME680 SDA** | `D0` |
| **BME680 SCL** | `D7` |
| **OLED SDA** | `D0` |
| **OLED SCL** | `D7` |
| **VCC & GND** | `3V3` & `GND` |

### Gateway Node (XIAO to ESP32)
The XIAO nRF52840 acts as a radio receiver and sends raw data via Serial (UART) to the ESP32.

| Pin (XIAO nRF52840) | Pin (ESP32) |
| :--- | :--- |
| `TX` | `RX2` (GPIO 16) |
| `RX` | `TX2` (GPIO 17) |
| `GND` | `GND` |

---

## 📚 Required Arduino IDE Libraries
Make sure you have installed the following libraries via the **Library Manager** in the Arduino IDE before compiling:
1. `ArduinoJson` (by Benoit Blanchon) - For dynamic memory management and data parsing.
2. `Adafruit BME680 Library` - To read temperature, humidity, pressure, and gas.
3. `U8g2` - To control the OLED display.
4. `RadioLib` - For LoRa communication via Wio SX1262.

---

## 🚀 Installation & Usage

1. Clone this repository to your computer:
   ```bash
   git clone [https://github.com/YOUR_USERNAME/LoRa-Server-Monitoring.git](https://github.com/YOUR_USERNAME/LoRa-Server-Monitoring.git)

2. Open the Gateway_ESP32 folder and edit the .ino file. Change the Wi-Fi credentials and Static IP settings according to your local network:
   ```bash
   #C++
   
   const char* ssid     = "YOUR_WIFI_NAME_HERE";
   const char* password = "YOUR_WIFI_PASSWORD_HERE";
   IPAddress local_IP(192, 168, 18, 113); // Adjust to your network
   ```
3. Upload the Sensor Node code to the first XIAO nRF52840.
4. Upload the LoRa Receiver code to the second XIAO nRF52840.
5. Upload the Gateway Aggregator code to the ESP32.
6. Power on all devices. Open a browser on a computer/phone connected to the same network and access http://192.168.18.113/api/data (or the IP you configured).

---

## 📡 JSON API Output Example
When the system is running, the ESP32 will serve data in a clean JSON format ready to be fetched by dashboards (like Grafana or Node-RED):
```bash
{
  "Ruang Server": {
    "suhu": "21.50",
    "kelembapan": "45.20",
    "tekanan": "1012.30",
    "gas_iaq": "150",
    "last_update": "3 seconds ago"
  }
}
```
*Note: You can add new LoRa Nodes (e.g., "Generator_Room") without needing to edit the code on the ESP32 at all!*

---

## ⚠️ Security
This repository does not include real network passwords or keys. Never upload your actual Wi-Fi credentials to a public repository.
