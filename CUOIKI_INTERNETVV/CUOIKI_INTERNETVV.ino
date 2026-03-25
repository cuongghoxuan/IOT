#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DS1302.h>
#include "DHT.h"
#include "ThingSpeak.h" // --- THÊM THƯ VIỆN ---

// ===== WIFI =====
const char* ssid = "iPhone";
const char* password = "00000000@";

// ===== MQTT =====
const char* mqtt_server = "53096db1c4e64e078478b373c96ab3a8.s1.eu.hivemq.cloud";
const int   mqtt_port = 8883;
const char* mqtt_user = "XUANCUONG";
const char* mqtt_pass = "Hoxuancuong26082005@";
const char* mqtt_topic_pub = "garden/data";
const char* mqtt_topic_sub = "garden/control";

// ===== THINGSPEAK (THÊM MỚI) =====
unsigned long myChannelNumber = 3208238;      // Thay bằng Channel ID của bạn
const char * myWriteAPIKey = "BJVFGOYMTS55PAKG";  // Thay bằng Write API Key của bạn
unsigned long lastTimeThingSpeak = 0;         // Biến đếm thời gian
const unsigned long postingInterval = 15000;  // Gửi mỗi 15 giây

// ===== PIN =====
#define DHTPIN 4
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define LDR_PIN 35
#define RELAY_PUMP 13
#define RELAY_LIGHT 27

// ===== RTC DS1302 =====
#define RTC_CE 5
#define RTC_IO 18
#define RTC_SCLK 19

DHT dht(DHTPIN, DHTTYPE);
DS1302 rtc(RTC_CE, RTC_IO, RTC_SCLK);
LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClientSecure espClient;
PubSubClient client(espClient);
WiFiClient tsClient; // --- CLIENT CHO THINGSPEAK ---

// ===== BIẾN =====
int soilThreshold = 40; 
int startHour = 7;
int startMin  = 0;
int endHour   = 7;
int endMin    = 10;

bool isDark = false;
bool scheduledWateringActive = false; 

// ===== MQTT CALLBACK =====
void callback(char* topic, byte* message, unsigned int length) {
    String msg;
    for (int i = 0; i < length; i++) msg += (char)message[i];
    msg.trim();
    
    Serial.printf("<<< MQTT Received [%s]: %s\n", topic, msg.c_str());

    if (msg == "ON") {
        digitalWrite(RELAY_PUMP, HIGH);
        Serial.println(">>> Pump set to ON (MANUAL OVERRIDE)");
    }
    else if (msg == "OFF") {
        digitalWrite(RELAY_PUMP, LOW);
        Serial.println(">>> Pump set to OFF (MANUAL OVERRIDE)");
    }
    
    if (msg.startsWith("WATER:")) {
        int h, m, d;
        if (sscanf(msg.c_str(), "WATER:%d:%d:%d", &h, &m, &d) == 3) {
            startHour = h; startMin = m;
            endHour = h; endMin = m + d;
            if (endMin >= 60) { endMin -= 60; endHour++; if (endHour >= 24) endHour = 0; }
            Serial.printf("✅ LICH TUOI CAP NHAT: %02d:%02d -> %02d:%02d\n", startHour, startMin, endHour, endMin);
        }
    }
}

// ===== MQTT CONNECT =====
void reconnect() {
    Serial.print("Connecting to MQTT...");
    while (!client.connected()) {
        if (client.connect("ESP32", mqtt_user, mqtt_pass)) {
            client.subscribe(mqtt_topic_sub);
            Serial.println(" connected!");
        } else {
            delay(3000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    dht.begin();
    rtc.halt(false);
    rtc.writeProtect(false);

    pinMode(RELAY_PUMP, OUTPUT);
    pinMode(RELAY_LIGHT, OUTPUT);
    digitalWrite(RELAY_PUMP, LOW);
    digitalWrite(RELAY_LIGHT, LOW);

    lcd.init();
    lcd.backlight();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

    espClient.setInsecure();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    
    ThingSpeak.begin(tsClient); // --- KẾT NỐI THINGSPEAK ---
    reconnect();
}

void loop() {
    setTimeFromSerial();
    if (!client.connected()) reconnect();
    client.loop();

    Time t = rtc.time();
    float h = dht.readHumidity();
    float temp = dht.readTemperature();
    int soil = map(analogRead(SOIL_PIN), 0, 4095, 100, 0);
    int light = analogRead(LDR_PIN);
    bool currentPumpState = digitalRead(RELAY_PUMP); 

    if (isnan(h) || isnan(temp)) Serial.println("⚠️ DHT Sensor Read Error!");

    // ===== ĐÈN =====
    isDark = (light > 2000);
    digitalWrite(RELAY_LIGHT, isDark ? HIGH : LOW);
    bool lightState = digitalRead(RELAY_LIGHT);

    // ===== LOGIC BƠM =====
    bool afterStart = (t.hr > startHour) || (t.hr == startHour && t.min >= startMin);
    bool beforeEnd = (t.hr < endHour) || (t.hr == endHour && t.min < endMin);
    bool wateringTime = afterStart && beforeEnd;
    
    if (currentPumpState == HIGH && !scheduledWateringActive && !wateringTime) {
    } else if (currentPumpState == LOW && scheduledWateringActive && !wateringTime) {
    } else {
        if (wateringTime) { digitalWrite(RELAY_PUMP, HIGH); scheduledWateringActive = true; }
        else { digitalWrite(RELAY_PUMP, LOW); scheduledWateringActive = false; }
    }
    currentPumpState = digitalRead(RELAY_PUMP); 

    // ===== LCD =====
    lcd.setCursor(0, 0);
    lcd.printf("%02d/%02d/%02d %02d:%02d", t.date, t.mon, t.yr, t.hr, t.min);
    lcd.setCursor(0, 1);
    lcd.print(" BOM:"); lcd.print(currentPumpState ? "ON " : "OFF");
    
    // ===== SERIAL =====
    Serial.printf("3. Nhiet Do/Do Am KK: %.1f C / %.1f %%\n", temp, h);
    Serial.printf("4. Do Am Dat: %d %%\n", soil);
    Serial.printf("8. TRANG THAI BOM: %s\n", (currentPumpState ? "ON" : "OFF"));

    // ===== MQTT JSON =====
    String payload = "{";
    payload += "\"nhietdo\":" + String(temp,1) + ",";
    payload += "\"doamkk\":" + String(h,1) + ",";
    payload += "\"doamdat\":" + String(soil) + ",";
    payload += "\"bom\":" + String(currentPumpState ? 1 : 0) + ",";
    payload += "\"den\":" + String(lightState ? 1 : 0) + ",";
    payload += "\"chedo\":\"TIMER\",";
    payload += "\"lich\":\"" + String(startHour) + ":" + String(startMin) + "-" + String(endHour) + ":" + String(endMin) + "\"}";
    client.publish(mqtt_topic_pub, payload.c_str());

    // ===== THINGSPEAK SEND (THÊM MỚI) =====
    if (millis() - lastTimeThingSpeak > postingInterval) {
        ThingSpeak.setField(1, temp);
        ThingSpeak.setField(2, h);
        ThingSpeak.setField(3, soil);
        ThingSpeak.setField(4, currentPumpState ? 1 : 0);
        ThingSpeak.setField(5, lightState ? 1 : 0);
        
        int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        if(x == 200) Serial.println("✅ ThingSpeak Update Successful.");
        else Serial.println("❌ ThingSpeak Error: " + String(x));
        
        lastTimeThingSpeak = millis();
    }

    delay(3000);
}

void setTimeFromSerial() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n'); cmd.trim();
    if (!cmd.startsWith("SET:")) return;
    int d, m, y, hh, mm, ss;
    if (sscanf(cmd.c_str(), "SET:%d/%d/%d-%d:%d:%d", &d, &m, &y, &hh, &mm, &ss) == 6) {
        Time t(y, m, d, hh, mm, ss, Time::kSunday);
        rtc.time(t);
        Serial.println("✅ DA CAP NHAT RTC");
    }
}