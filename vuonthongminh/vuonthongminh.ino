#include <WiFi.h>
#include <WiFiClientSecure.h> // Dùng thư viện Secure cho HiveMQ Cloud
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ────────────────────────────────────────────────
// WiFi & HiveMQ Cloud Config (Thay thông tin của bạn vào đây)
// ────────────────────────────────────────────────
const char* ssid     = "Wokwi-GUEST"; 
const char* password = "";

// Lấy từ Console HiveMQ Cloud (Ví dụ: xxxxxxxx.s1.eu.hivemq.cloud)
const char* mqtt_server   = "53096db1c4e64e078478b373c96ab3a8.s1.eu.hivemq.cloud"; 
const int   mqtt_port     = 8883; 
const char* mqtt_user     = "XUANCUONG"; 
const char* mqtt_pass     = "Hoxuancuong26082005@"; 

const char* topic_sensor  = "group7/irrigation/sensor";
const char* topic_command = "group7/irrigation/command";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// ────────────────────────────────────────────────
// Pins
// ────────────────────────────────────────────────
#define TRIG_PIN       5
#define ECHO_PIN       18
#define SOIL_PIN       34
#define DHT_PIN        4
#define DHT_TYPE       DHT22
#define PUMP1_PIN      26   // Bơm tưới cây
#define PUMP2_PIN      27   // Bơm cấp nước vào bồn

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);

// ────────────────────────────────────────────────
// Biến toàn cục
// ────────────────────────────────────────────────
float soilMoisture = 0, temperature = 0, humidity = 0, waterLevelPercent = 0;
const float TANK_HEIGHT_CM   = 100.0;
const float MIN_WATER_LEVEL  = 20.0;
const float MAX_WATER_LEVEL  = 90.0;
const int   SOIL_DRY_THRESHOLD = 40;

unsigned long lastMsg = 0;
const long interval = 5000; 
bool isPump1Active = false;

// ────────────────────────────────────────────────
// SETUP
// ────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  
  pinMode(PUMP1_PIN, OUTPUT);
  pinMode(PUMP2_PIN, OUTPUT);
  digitalWrite(PUMP1_PIN, HIGH); // Tắt relay (active LOW)
  digitalWrite(PUMP2_PIN, HIGH);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  dht.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED failed"));
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  setup_wifi();
  
  // Quan trọng: Cấu hình SSL cho HiveMQ Cloud
  espClient.setInsecure(); // Bỏ qua kiểm tra CA certificate để chạy nhanh trên Wokwi

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void setup_wifi() {
  Serial.print("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to HiveMQ Cloud...");
    String clientId = "ESP32_Client_" + String(random(0xffff), HEX);
    
    // Kết nối với Username và Password
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("CONNECTED");
      client.subscribe(topic_command);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5s");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];
  Serial.println("Command from AI: " + message);

  if (message.indexOf("PUMP1:OFF") >= 0) {
    digitalWrite(PUMP1_PIN, HIGH);
    isPump1Active = false;
    displayPumpStatus("OFF (AI)");
  }
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg >= interval) {
    lastMsg = now;

    // Đọc cảm biến
    soilMoisture = readSoilMoisture();
    temperature  = dht.readTemperature();
    humidity     = dht.readHumidity();
    waterLevelPercent = calculateWaterLevelPercent(readUltrasonicDistance());

    displayDataOnOLED();

    // Gửi dữ liệu JSON (đúng định dạng cho Node-RED)
    String payload = "{";
    payload += "\"soil\":" + String(soilMoisture, 1) + ",";
    payload += "\"temp\":" + String(temperature, 1) + ",";
    payload += "\"hum\":" + String(humidity, 1) + ",";
    payload += "\"water_percent\":" + String(waterLevelPercent, 0);
    payload += "}";

    Serial.print("Publishing: "); Serial.println(payload);
    client.publish(topic_sensor, payload.c_str());

    localAutoStartIrrigation();
    manageWaterRefill();
  }
}

// ────────────────────────────────────────────────
// HÀM PHỤ TRỢ (Dựa trên code gốc của bạn)
// ────────────────────────────────────────────────
float readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return (duration == 0) ? -1 : duration * 0.034 / 2.0;
}

float calculateWaterLevelPercent(float dist) {
  if (dist <= 0 || dist >= TANK_HEIGHT_CM) return 0;
  float percent = ((TANK_HEIGHT_CM - dist) / TANK_HEIGHT_CM) * 100.0;
  return constrain(percent, 0, 100);
}

float readSoilMoisture() {
  int raw = analogRead(SOIL_PIN);
  float percent = map(raw, 4095, 1200, 0, 100);
  return constrain(percent, 0, 100);
}

void displayDataOnOLED() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.printf("Soil: %.1f%%\nTemp: %.1fC\nHum:  %.1f%%\nWater:%.0f%%", 
                  soilMoisture, temperature, humidity, waterLevelPercent);
  display.setCursor(0,50);
  display.print(isPump1Active ? "Pump: ON" : "Pump: OFF");
  display.display();
}

void displayPumpStatus(String msg) {
  display.setCursor(0,54);
  display.print("CMD: " + msg);
  display.display();
}

void localAutoStartIrrigation() {
  if (soilMoisture < SOIL_DRY_THRESHOLD && waterLevelPercent > MIN_WATER_LEVEL && !isPump1Active) {
    digitalWrite(PUMP1_PIN, LOW);
    isPump1Active = true;
    Serial.println("Auto Start Irrigation");
  }
}

void manageWaterRefill() {
  if (waterLevelPercent < MIN_WATER_LEVEL) {
    if (isPump1Active) { 
        digitalWrite(PUMP1_PIN, HIGH); 
        isPump1Active = false; 
    }
    digitalWrite(PUMP2_PIN, LOW); // Bật bơm cấp
  } else if (waterLevelPercent > MAX_WATER_LEVEL) {
    digitalWrite(PUMP2_PIN, HIGH); // Tắt bơm cấp
  }
}