#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// --- WIFI & IP STATIC ---
const char* ssid     = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD_WIFI";

// Pengaturan IP Static
IPAddress local_IP(192, 168, 18, 113);       
IPAddress gateway(192, 168, 18, 1);         
IPAddress subnet(255, 255, 255, 0);        
IPAddress primaryDNS(8, 8, 8, 8);          
IPAddress secondaryDNS(8, 8, 4, 4);        

// --- PENGATURAN LED INDIKATOR ---
#define LED_PIN 2 // Pin LED bawaan ESP32
unsigned long ledMenyalaSejak = 0;
const unsigned long durasiKedip = 100; // LED menyala selama 100 milidetik

// Inisialisasi WebServer pada port 80
WebServer server(80);

// Database JSON Global untuk menampung data dari semua node
JsonDocument db;

void setup() {
  // 1. Setup Pin LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Pastikan LED mati saat awal

  // 2. Serial untuk Debugging ke PC (USB)
  Serial.begin(115200);
  
  // 3. Serial2 untuk komunikasi UART dengan XIAO LoRa Receiver (RX=16, TX=17)
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  
  delay(1000);
  Serial.println("\n=== ESP32 IoT AGGREGATOR (STATIC IP + LED) ===");

  // 4. Konfigurasi IP Static sebelum menghubungkan ke Wi-Fi
  Serial.println("Mengonfigurasi IP Static...");
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("❌ Gagal mengonfigurasi IP Static!");
  }

  // 5. Menghubungkan ke Wi-Fi
  Serial.print("Menghubungkan ke Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi Terhubung!");
  Serial.print("🌐 Alamat IP ESP32: ");
  Serial.println(WiFi.localIP());

  // 6. Routing Web Server (Endpoint API)
  server.on("/", handleRoot);          
  server.on("/api/data", handleAPI);        

  // Mulai Web Server
  server.begin();
  Serial.println("🚀 Web Server berjalan!");
  
  // Beri sinyal LED berkedip cepat 3 kali tanda sistem siap
  for(int i=0; i<3; i++){
    digitalWrite(LED_PIN, HIGH); delay(100);
    digitalWrite(LED_PIN, LOW); delay(100);
  }
}

void loop() {
  // Tangani permintaan klien web (Browser/Dashboard)
  server.handleClient();

  // Membaca data UART yang masuk dari XIAO LoRa Receiver
  if (Serial2.available()) {
    String incomingData = Serial2.readStringUntil('\n');
    incomingData.trim(); // Hapus spasi/enter berlebih
    
    if (incomingData.length() > 0) {
      Serial.print("📥 Data diterima dari LoRa: ");
      Serial.println(incomingData);

      // Nyalakan LED dan catat waktunya saat data masuk
      digitalWrite(LED_PIN, HIGH);
      ledMenyalaSejak = millis();

      // Parsing JSON yang masuk
      JsonDocument tempDoc;
      DeserializationError error = deserializeJson(tempDoc, incomingData);

      if (!error) {
        // Ambil nama node
        String nodeName = tempDoc["node"].as<String>();

        if (nodeName != "null" && nodeName != "") {
          // Simpan atau Perbarui data di Database Global ESP32
          db[nodeName]["suhu"]       = tempDoc["suhu"];
          db[nodeName]["kelembapan"] = tempDoc["kelembapan"];
          db[nodeName]["tekanan"]    = tempDoc["tekanan"];
          db[nodeName]["gas_iaq"]    = tempDoc["gas_iaq"];
          
          // Catat waktu kapan data terakhir diterima (dalam milidetik)
          db[nodeName]["last_update_ms"] = millis();
          
          Serial.println("✅ Memori berhasil diperbarui untuk: " + nodeName);
        }
      } else {
        Serial.print("❌ Gagal parsing JSON masuk: ");
        Serial.println(error.f_str());
      }
    }
  }

  // --- LOGIKA UNTUK MEMATIKAN LED (NON-BLOCKING) ---
  // Mengecek apakah LED sedang menyala DAN apakah durasi 100ms sudah terlewati
  if (digitalRead(LED_PIN) == HIGH && (millis() - ledMenyalaSejak >= durasiKedip)) {
    digitalWrite(LED_PIN, LOW); // Matikan LED
  }
}

// ==========================================
// FUNGSI UNTUK MENAMPILKAN JSON KE BROWSER
// ==========================================
void handleAPI() {
  JsonDocument responseDoc = db;
  unsigned long currentMs = millis();

  for (JsonPair kv : responseDoc.as<JsonObject>()) {
    unsigned long lastMs = kv.value()["last_update_ms"];
    unsigned long diffSeconds = (currentMs - lastMs) / 1000;
    
    kv.value()["last_update"] = String(diffSeconds) + " detik lalu";
    kv.value().remove("last_update_ms");
  }

  String jsonOutput;
  serializeJsonPretty(responseDoc, jsonOutput);
  server.send(200, "application/json", jsonOutput);
}

// ==========================================
// FUNGSI UNTUK HALAMAN ROOT
// ==========================================
void handleRoot() {
  String html = "<h1>ESP32 IoT Aggregator</h1>";
  html += "<p>Silakan akses <b><a href='/api/data'>/api</a></b> untuk melihat data JSON.</p>";
  server.send(200, "text/html", html);
}
