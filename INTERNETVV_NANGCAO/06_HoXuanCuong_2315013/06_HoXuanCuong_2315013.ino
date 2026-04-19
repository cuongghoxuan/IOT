#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// WiFi & MQTT
const char* ssid = "Anh BE may mắn";
const char* password = "00000000";
const char* mqtt_server = "53096db1c4e64e078478b373c96ab3a8.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "XUANCUONG";
const char* mqtt_pass = "Hoxuancuong26082005@";
const char* topic_sensor = "group7/irrigation/sensor";
const char* topic_command = "group7/irrigation/command";

// Pins
#define TRIG_PIN 5
#define ECHO_PIN 18
#define SOIL_PIN 34
#define DHT_PIN 4
#define PUMP1_PIN 26
#define PUMP2_PIN 27

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT11);
PubSubClient client(espClient);

float soilMoisture, temp, hum, waterLevel;
bool pump1 = false, pump2 = false;
unsigned long lastMsg = 0;

void setup() {
  Serial.begin(115200);
  
  pinMode(PUMP1_PIN, OUTPUT);
  pinMode(PUMP2_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  digitalWrite(PUMP1_PIN, LOW);
  digitalWrite(PUMP2_PIN, LOW);
  
  dht.begin();
  lcd.init();
  lcd.backlight();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(200);
  
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  while (!client.connected()) {
    if (client.connect("ESP32", mqtt_user, mqtt_pass)) {
      client.subscribe(topic_command);
    } else delay(2000);
  }
}

float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  float distance = duration * 0.0343 / 2;
  return constrain(distance, 2, 10);
}

float readSoil() {
  int val = analogRead(SOIL_PIN);
  return constrain(map(val, 4095, 1200, 0, 100), 0, 100);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  
  if (msg == "PUMP_ON") {
    digitalWrite(PUMP1_PIN, HIGH);
    pump1 = true;
    Serial.println("AI: Pump ON");
  }
  else if (msg == "PUMP_OFF") {
    digitalWrite(PUMP1_PIN, LOW);
    pump1 = false;
    Serial.println("AI: Pump OFF");
  }
}

void loop() {
  if (!client.connected()) {
    client.connect("ESP32", mqtt_user, mqtt_pass);
    client.subscribe(topic_command);
  }
  client.loop();
  
  if (millis() - lastMsg > 2000) {
    // Đọc cảm biến
    temp = dht.readTemperature();
    hum = dht.readHumidity();
    if (isnan(temp)) temp = 0;
    if (isnan(hum)) hum = 0;
    
    soilMoisture = readSoil();
    float distance = readDistance();
    waterLevel = map(distance, 10, 2, 0, 100);
    waterLevel = constrain(waterLevel, 0, 100);
    
    // BƠM CẤP NƯỚC (ESP32 tự động)
    if (waterLevel < 20 && !pump2) {
      digitalWrite(PUMP2_PIN, HIGH);
      pump2 = true;
      Serial.println("Water pump ON");
    } else if (waterLevel >= 80 && pump2) {
      digitalWrite(PUMP2_PIN, LOW);
      pump2 = false;
      Serial.println("Water pump OFF");
    }
    
    // BƠM TƯỚI: KHÔNG tự động bật/tắt - CHỈ NHẬN LỆNH TỪ AI
    
    // Gửi JSON
    String payload = "{\"temp\":" + String(temp) + ",\"hum\":" + String(hum) + 
                     ",\"soil\":" + String(soilMoisture) + ",\"water_percent\":" + String(waterLevel) + 
                     ",\"pump1\":" + String(pump1 ? 1 : 0) + ",\"pump2\":" + String(pump2 ? 1 : 0) + "}";
    client.publish(topic_sensor, payload.c_str());
    
    // LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printf("T:%.0fC H:%.0f%%", temp, hum);
    lcd.setCursor(0, 1);
    lcd.printf("W:%.0f%% S:%.0f%%", waterLevel, soilMoisture);
    lcd.setCursor(12, 1);
    lcd.print(pump1 ? "ON" : "OF");
    
    lastMsg = millis();
  }
}