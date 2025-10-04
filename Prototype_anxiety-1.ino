#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ------------------- WiFi & Firebase Config -------------------
#define WIFI_SSID "1122"
#define WIFI_PASSWORD "123456789"
#define API_KEY "Apikey"
#define DATABASE_URL "https://iot-esp-pi-gateway-default-rtdb.asia-southeast1.firebasedatabase.app/"

// ------------------- Sensor Pin Config -------------------
#define ECG_PIN 36      // AD8232 ECG Module
#define GSR_PIN 39      // Grove GSR Sensor
#define PIEZO_PIN 34    // Grove Piezoelectric Sensor

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// NTP for timestamp
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);  // GMT+5:30 (India), update every 60s

// Timing
const unsigned long historicalCheckInterval = 10000L;  // 10 seconds
unsigned long lastHistoricalDataTime = 0;

// Sensor value variables
int ecgValue = 0;
int gsrValue = 0;
int piezoValue = 0;

// --------- Functions ---------

// Get current epoch timestamp
String getCurrentTimestamp() {
  timeClient.update();
  return String(timeClient.getEpochTime());
}

// Read sensors and send real-time data to Firebase
void sendImmediateSensorData() {
  ecgValue = analogRead(ECG_PIN);
  gsrValue = analogRead(GSR_PIN);
  piezoValue = analogRead(PIEZO_PIN);

  Serial.print("ECG:"); Serial.println(ecgValue);  
  Serial.print("GSR:"); Serial.println(gsrValue);
  Serial.print("Piezo:"); Serial.println(piezoValue);

  // Send to Firebase
  Firebase.RTDB.setInt(&fbdo, "BioSignals/ECG", ecgValue);
  Firebase.RTDB.setInt(&fbdo, "BioSignals/GSR", gsrValue);
  Firebase.RTDB.setInt(&fbdo, "BioSignals/Piezo", piezoValue);
}

// Send periodic (historical) data with timestamp
void sendHistoricalData() {
  String timestamp = getCurrentTimestamp();

  FirebaseJson json;
  json.set("ECG", ecgValue);
  json.set("GSR", gsrValue);
  json.set("Piezo", piezoValue);
  json.set("timestamp", timestamp);

  String path = "historicalData/" + timestamp;
  if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
    Serial.println("Historical data sent successfully");
  } else {
    Serial.println("Failed to send historical data");
    Serial.println(fbdo.errorReason());
  }
}

void setup() {
  Serial.begin(115200);

  // Sensor pin modes
  pinMode(ECG_PIN, INPUT);
  pinMode(GSR_PIN, INPUT);
  pinMode(PIEZO_PIN, INPUT);

  // WiFi Connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Firebase Setup
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase sign-up successful");
  } else {
    Serial.printf("Firebase sign-up failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // NTP Time
  timeClient.begin();
}

void loop() {
  if (Firebase.ready()) {
    sendImmediateSensorData();

    // Send historical data every 10 seconds
    unsigned long currentTime = millis();
    if (currentTime - lastHistoricalDataTime >= historicalCheckInterval) {
      lastHistoricalDataTime = currentTime;
      sendHistoricalData();
    }
  } else {
    Serial.println("Firebase not ready");
  }

  delay(1000);  // Optional delay to prevent spamming Firebase
}
