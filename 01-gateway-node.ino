#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// --- WIFI & STATIC IP ---
const char* ssid     = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD_WIFI";

// Static IP Configuration
IPAddress local_IP(192, 168, 18, 113);       
IPAddress gateway(192, 168, 18, 1);         
IPAddress subnet(255, 255, 255, 0);        
IPAddress primaryDNS(8, 8, 8, 8);          
IPAddress secondaryDNS(8, 8, 4, 4);        

// --- LED INDICATOR SETTINGS ---
#define LED_PIN 2 // Built-in ESP32 LED pin
unsigned long ledOnSince = 0;
const unsigned long blinkDuration = 100; // LED turns on for 100 milliseconds

// Initialize WebServer on port 80
WebServer server(80);

// Global JSON Database to store data from all nodes
JsonDocument db;

void setup() {
  // 1. Setup LED Pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure LED is off at startup

  // 2. Serial for PC Debugging (USB)
  Serial.begin(115200);
  
  // 3. Serial2 for UART communication with XIAO LoRa Receiver (RX=16, TX=17)
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  
  delay(1000);
  Serial.println("\n=== ESP32 IoT AGGREGATOR (STATIC IP + LED) ===");

  // 4. Configure Static IP before connecting to Wi-Fi
  Serial.println("Configuring Static IP...");
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("❌ Failed to configure Static IP!");
  }

  // 5. Connecting to Wi-Fi
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi Connected!");
  Serial.print("🌐 ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // 6. Web Server Routing (API Endpoint)
  server.on("/", handleRoot);          
  server.on("/api/data", handleAPI);        

  // Start Web Server
  server.begin();
  Serial.println("🚀 Web Server is running!");
  
  // Blink LED 3 times quickly to indicate system is ready
  for(int i=0; i<3; i++){
    digitalWrite(LED_PIN, HIGH); delay(100);
    digitalWrite(LED_PIN, LOW); delay(100);
  }
}

void loop() {
  // Handle web client requests (Browser/Dashboard)
  server.handleClient();

  // Read incoming UART data from XIAO LoRa Receiver
  if (Serial2.available()) {
    String incomingData = Serial2.readStringUntil('\n');
    incomingData.trim(); // Remove excess spaces/newlines
    
    if (incomingData.length() > 0) {
      Serial.print("📥 Data received from LoRa: ");
      Serial.println(incomingData);

      // Turn on LED and record the time when data arrives
      digitalWrite(LED_PIN, HIGH);
      ledOnSince = millis();

      // Parse incoming JSON
      JsonDocument tempDoc;
      DeserializationError error = deserializeJson(tempDoc, incomingData);

      if (!error) {
        // Get node name
        String nodeName = tempDoc["node"].as<String>();

        if (nodeName != "null" && nodeName != "") {
          
          // --- UPDATE LOGIKA DINAMIS ---
          // Salin SEMUA key-value dari tempDoc ke database global ESP32, 
          // kecuali key "node" (karena sudah jadi nama laci utamanya)
          JsonObject incomingObj = tempDoc.as<JsonObject>();
          for (JsonPair kv : incomingObj) {
            String key = kv.key().c_str();
            if (key != "node") {
              db[nodeName][key] = kv.value();
            }
          }
          
          // Record the time when data was last received (in milliseconds)
          db[nodeName]["last_update_ms"] = millis();
          
          Serial.println("✅ Memory successfully updated for: " + nodeName);
        }
      } else {
        Serial.print("❌ Failed to parse incoming JSON: ");
        Serial.println(error.f_str());
      }
    }
  }

  // --- LOGIC TO TURN OFF LED (NON-BLOCKING) ---
  if (digitalRead(LED_PIN) == HIGH && (millis() - ledOnSince >= blinkDuration)) {
    digitalWrite(LED_PIN, LOW); // Turn off LED
  }
}

// ==========================================
// FUNCTION TO DISPLAY JSON TO BROWSER
// ==========================================
void handleAPI() {
  JsonDocument responseDoc = db;
  unsigned long currentMs = millis();

  for (JsonPair kv : responseDoc.as<JsonObject>()) {
    unsigned long lastMs = kv.value()["last_update_ms"];
    unsigned long diffSeconds = (currentMs - lastMs) / 1000;
    
    kv.value()["last_update"] = String(diffSeconds) + " seconds ago";
    kv.value().remove("last_update_ms");
  }

  String jsonOutput;
  serializeJsonPretty(responseDoc, jsonOutput);
  server.send(200, "application/json", jsonOutput);
}

// ==========================================
// FUNCTION FOR ROOT PAGE
// ==========================================
void handleRoot() {
  String html = "<h1>ESP32 IoT Aggregator</h1>";
  html += "<p>Please access <b><a href='/api/data'>/api/data</a></b> to view the JSON data.</p>";
  server.send(200, "text/html", html);
}
