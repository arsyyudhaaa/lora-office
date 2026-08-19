#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_INA219.h>
#include <Ultrasonic.h>
#include <ArduinoJson.h>
#include <U8g2lib.h> // Library for OLED Display

// --- LORA WIO SX1262 PINS ---
#define DIO1_PIN  D1
#define NRST_PIN  D2
#define BUSY_PIN  D3
#define NSS_PIN   D4
#define RF_SW_PIN D5

// --- I2C PINS (BME280, INA219, OLED) ---
#define I2C_SDA_PIN D0
#define I2C_SCL_PIN D7

// --- GROVE ULTRASONIC PIN ---
#define ULTRASONIC_PIN D6

// --- INITIALIZE SENSORS & MODULES ---
SX1262 radio = new Module(NSS_PIN, DIO1_PIN, NRST_PIN, BUSY_PIN);
Adafruit_BME280 bme; 
Adafruit_INA219 ina219;
Ultrasonic ultrasonic(ULTRASONIC_PIN);

// Initialize 1.3" OLED Display (SH1106 Chip via Hardware I2C)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- CALIBRATION SETTINGS (FUEL LEVEL) ---
// Adjust these values based on your physical fuel tank size (in Centimeters)
const int JARAK_KOSONG = 100; // Distance when tank is completely EMPTY (e.g., 100 cm)
const int JARAK_PENUH  = 20;  // Distance when tank is FULL (e.g., 20 cm from sensor)

// --- TIMING SETTINGS ---
unsigned long previousMillis = 0;
const long sendInterval = 30000; // Transmit & Update Screen every 30,000 ms (30 seconds)

void setup() {
  Serial.begin(115200);
  delay(3000); 
  Serial.println("\n=== SENDER NODE: GENERATOR 1 ===");

  // 1. Setup LoRa Wio SX1262 Antenna Switch
  pinMode(RF_SW_PIN, OUTPUT);
  digitalWrite(RF_SW_PIN, HIGH);

  // 2. Initialize LoRa Radio (915 MHz or 921 MHz)
  Serial.print("[LoRa] Initializing radio... ");
  int status = radio.begin(921.0, 125.0, 10, 5, 0x12, 22);
  radio.setPreambleLength(16);

  if (status == RADIOLIB_ERR_NONE) {
    Serial.println("Success!");
  } else {
    Serial.print("Failed, error code: ");
    Serial.println(status);
    while (1); 
  }

  // 3. Configure and Initialize I2C Pins
  Serial.print("[I2C] Setting up I2C pins... ");
  #if defined(NRF52840_XXAA) || defined(NRF52)
    Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  #endif
  Wire.begin();

  // 4. Initialize BME280
  if (!bme.begin(0x76, &Wire)) {
    if (!bme.begin(0x77, &Wire)) {
      Serial.println("\n[ERROR] Failed to find BME280 sensor!");
    }
  } else {
    Serial.println("BME280 initialized!");
  }

  // 5. Initialize INA219
  if (!ina219.begin(&Wire)) {
    Serial.println("[ERROR] Failed to find INA219 sensor!");
  } else {
    Serial.println("INA219 initialized!");
  }

  // 6. Initialize OLED
  u8g2.begin();
  u8g2.setI2CAddress(0x78); // Use 0x78 for U8g2 (shifted from 0x3C)
  
  // Display Splash Screen
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr); 
  u8g2.drawStr(15, 30, "Starting System...");
  u8g2.drawStr(15, 45, "Generator 1 Node");
  u8g2.sendBuffer();
  delay(2000);
}

void loop() {
  unsigned long currentMillis = millis();

  // Execute this routine based on sendInterval
  if (currentMillis - previousMillis >= sendInterval) {
    previousMillis = currentMillis;
    Serial.println("\n--- Fetching latest data ---");

    // ==========================================
    // 1. READ SENSORS
    // ==========================================
    
    // Read BME280 (Temperature)
    float suhu = bme.readTemperature();
    float kelembapan = bme.readHumidity();

    // Read INA219 (Battery Voltage)
    float volt_aki = ina219.getBusVoltage_V();
    
    // Determine Generator Status based on voltage
    // Normal 12V battery rests at ~12.4V to 12.8V. When charging (ON), it jumps to > 13.5V
    String statusGenset = (volt_aki > 13.5) ? "ON" : "OFF";

    // Read Grove Ultrasonic (Fuel Level)
    long jarak_cm = ultrasonic.MeasureInCentimeters();
    
    // Map distance to Percentage (0% - 100%)
    int solar_persen = map(jarak_cm, JARAK_KOSONG, JARAK_PENUH, 0, 100);
    solar_persen = constrain(solar_persen, 0, 100); // Prevent negative or >100% values

    // ==========================================
    // 2. PREPARE JSON & TRANSMIT LORA
    // ==========================================
    
    JsonDocument doc;
    doc["node"]         = "Ruang Genset 1";
    doc["suhu"]         = serialized(String(suhu, 1));
    doc["kelembapan"]   = serialized(String(kelembapan, 1));
    doc["volt_aki"]     = serialized(String(volt_aki, 2));
    doc["solar_persen"] = solar_persen;
    doc["status"]       = statusGenset;

    String txData;
    serializeJson(doc, txData);
    
    Serial.print("📤 Transmit: ");
    Serial.println(txData);

    int transmitStatus = radio.transmit(txData);
    String loraStatus = (transmitStatus == RADIOLIB_ERR_NONE) ? "OK" : "FAIL";

    if (transmitStatus == RADIOLIB_ERR_NONE) {
      Serial.println("✅ Data transmitted successfully!");
    } else {
      Serial.print("❌ Failed to transmit, error: ");
      Serial.println(transmitStatus);
    }

    // ==========================================
    // 3. UPDATE OLED DISPLAY
    // ==========================================
    
    u8g2.clearBuffer(); 
    
    // Header
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(3, 10, "GENSET Type 1");

    // Environment & Battery Data
    u8g2.setCursor(3, 25);
    u8g2.print("Solar: "); u8g2.print(solar_persen); u8g2.print(" %");

    u8g2.setCursor(3, 38);
    u8g2.print("Aki  : "); u8g2.print(volt_aki, 2); u8g2.print(" V");

    // Fuel Percentage
    u8g2.setCursor(3, 51);
    u8g2.print("Suhu : "); u8g2.print(suhu, 1); u8g2.print(" C");

    // Status (ON / OFF) & LoRa Status aligned to the right
    u8g2.setCursor(83, 38);
    u8g2.print(statusGenset);

    u8g2.setCursor(3, 62);
    u8g2.print("Status LoRa : ");
    u8g2.print(loraStatus);

    u8g2.sendBuffer(); // Push data to screen
  }
}
