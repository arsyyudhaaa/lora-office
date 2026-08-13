#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <ArduinoJson.h>
#include <U8g2lib.h> // Library for OLED Display

// --- LORA WIO SX1262 PINS ---
#define DIO1_PIN  D1
#define NRST_PIN  D2
#define BUSY_PIN  D3
#define NSS_PIN   D4
#define RF_SW_PIN D5

// --- I2C PINS (BME680 & OLED) ---
#define I2C_SDA_PIN D0
#define I2C_SCL_PIN D7

// Initialize SX1262 Radio
SX1262 radio = new Module(NSS_PIN, DIO1_PIN, NRST_PIN, BUSY_PIN);

// Initialize BME680 Sensor
Adafruit_BME680 bme; 

// Initialize 1.3" OLED Display (SH1106 Chip via Hardware I2C)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- TIMING SETTINGS ---
unsigned long previousMillis = 0;
const long sendInterval = 30000; // Transmit & Update Screen every 30,000 ms (30 seconds)

// ===================================================================
// IAQ CALCULATOR FUNCTION (Air Quality Index 0-500)
// ===================================================================
float calculateIAQ(float gas_resistance, float humidity) {
  // Reference value for initial gas resistance (Clean Air Condition)
  static float gas_reference = 50000.0; 
  
  // If the sensor detects cleaner air, update the reference value
  if (gas_resistance > gas_reference) {
    gas_reference = gas_resistance;
  }

  // Weighting: Gas contributes 75%, Humidity 25% to air quality
  float gas_score = 0;
  float hum_score = 0;

  // A. Calculate Humidity Score (Ideally 40%)
  if (humidity >= 38 && humidity <= 42) {
    hum_score = 25.0; 
  } else if (humidity < 38) {
    hum_score = (25.0 / 40.0) * humidity;
  } else {
    hum_score = (25.0 / 60.0) * (100.0 - humidity);
  }

  // B. Calculate Gas Score
  float gas_ratio = gas_resistance / gas_reference;
  gas_score = gas_ratio * 75.0;
  if (gas_score > 75.0) gas_score = 75.0;

  // Combine Air Quality Score (0-100)
  float air_quality_score = hum_score + gas_score;

  // Convert to Standard IAQ Index (0-500, where 0 = Cleanest)
  float iaq_index = (100.0 - air_quality_score) * 5.0;
  
  if (iaq_index < 0) iaq_index = 0;
  if (iaq_index > 500) iaq_index = 500;
  
  return iaq_index;
}
// ===================================================================

void setup() {
  Serial.begin(115200);
  
  // Wait 3 seconds at startup
  delay(3000); 
  Serial.println("\n=== SENDER NODE: SERVER ROOM (BME680 + OLED) ===");

  // 1. Setup LoRa Wio SX1262 Antenna Switch
  pinMode(RF_SW_PIN, OUTPUT);
  digitalWrite(RF_SW_PIN, HIGH);

  // 2. Initialize LoRa Radio (Updated to 921 MHz)
  Serial.print("[LoRa] Initializing radio (921 MHz)... ");
  int status = radio.begin(921.0, 125.0, 10, 5, 0x12, 22);
  radio.setPreambleLength(16);

  if (status == RADIOLIB_ERR_NONE) {
    Serial.println("Success!");
  } else {
    Serial.print("Failed, error code: ");
    Serial.println(status);
    while (1); 
  }

  // 3. Configure and Initialize I2C (For BME680 & OLED)
  Serial.print("[I2C] Setting up I2C pins... ");
  #if defined(NRF52840_XXAA) || defined(NRF52)
    Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  #endif
  Wire.begin();

  // Initialize BME680
  if (!bme.begin(0x76, &Wire)) {
    if (!bme.begin(0x77, &Wire)) {
      Serial.println("\n[ERROR] Failed to find BME680 sensor!");
      while (1); 
    }
  }
  Serial.println("BME680 found!");

  // Initialize OLED
  u8g2.begin();
  u8g2.setI2CAddress(0x78);
  
  // Display Initial Splash Screen on OLED
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr); 
  u8g2.drawStr(15, 30, "Starting System...");
  u8g2.drawStr(15, 45, "Server Room Node");
  u8g2.sendBuffer();
  delay(2000);

  // 4. BME680 Precision Settings
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); 
}

void loop() {
  unsigned long currentMillis = millis();

  // Execute this routine every 30 seconds (Based on sendInterval)
  if (currentMillis - previousMillis >= sendInterval) {
    previousMillis = currentMillis;

    Serial.println("\n[BME680] Fetching latest readings...");
    
    // Command BME680 to perform reading
    if (!bme.performReading()) {
      Serial.println("Failed to read BME680 sensor!");
      return; 
    }

    // --- CALCULATE IAQ INDEX ---
    float iaq_value = calculateIAQ(bme.gas_resistance, bme.humidity);

    // 1. Prepare JSON Document
    JsonDocument doc;

    // 2. Insert Sensor Data into JSON format
    doc["node"]       = "Ruang Server"; // Kept in Indonesian to match your dashboard
    doc["suhu"]       = serialized(String(bme.temperature, 2));         
    doc["kelembapan"] = serialized(String(bme.humidity, 2));            
    doc["tekanan"]    = serialized(String(bme.pressure / 100.0, 2));    
    doc["gas_iaq"]    = serialized(String(iaq_value, 0)); // IAQ Index (0-500)

    // 3. Convert JSON to String format
    String txData;
    serializeJson(doc, txData);

    Serial.print("📤 Transmit: ");
    Serial.println(txData);

    // 4. Transmit data over the air
    int transmitStatus = radio.transmit(txData);
    String loraStatus = (transmitStatus == RADIOLIB_ERR_NONE) ? "LoRa: OK" : "LoRa: FAIL";

    if (transmitStatus == RADIOLIB_ERR_NONE) {
      Serial.println("✅ Data transmitted successfully!");
    } else {
      Serial.print("❌ Failed to transmit, error code: ");
      Serial.println(transmitStatus);
    }

    // 5. UPDATE OLED DISPLAY WITH LATEST DATA
    u8g2.clearBuffer(); // Clear screen memory
    
    // Title
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(3, 11, "Server Room Status");

    // Sensor Data
    u8g2.setCursor(3, 23);
    u8g2.print("Temp: ");
    u8g2.print(bme.temperature, 1); 
    u8g2.print(" C");

    u8g2.setCursor(3, 35);
    u8g2.print("Hum:  ");
    u8g2.print(bme.humidity, 1);
    u8g2.print(" %");

    // Display IAQ
    u8g2.setCursor(3, 47);
    u8g2.print("IAQ:  ");
    u8g2.print(iaq_value, 0); // Display as integer without decimals

    // LoRa Transmission Status (Bottom right corner)
    u8g2.setCursor(3, 59);
    u8g2.print("Status: ");
    u8g2.print(loraStatus);

    u8g2.sendBuffer(); // Send data to screen to be displayed
  }
}
