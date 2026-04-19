import paho.mqtt.client as mqtt
import time
import json

MQTT_BROKER = "53096db1c4e64e078478b373c96ab3a8.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USER = "XUANCUONG"
MQTT_PASS = "Hoxuancuong26082005@"
TOPIC_SENSOR = "group7/irrigation/sensor"
TOPIC_COMMAND = "group7/irrigation/command"

class AI:
    def __init__(self):
        self.client = mqtt.Client()
        self.client.tls_set()
        self.client.username_pw_set(MQTT_USER, MQTT_PASS)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.temp = self.hum = self.soil = 0
        self.pump = False
        self.last_cmd = 0
    
    def on_connect(self, client, userdata, flags, rc):
        print(f"✅ Connected (RC: {rc})")
        client.subscribe(TOPIC_SENSOR)
        print("🤖 AI Smart Irrigation Started\n")
    
    def predict_threshold(self):
        """Dự đoán ngưỡng độ ẩm tắt bơm dựa vào nhiệt độ và độ ẩm KK"""
        threshold = 65
        
        # Điều chỉnh theo nhiệt độ
        if self.temp > 35:
            threshold += 12
        elif self.temp > 32:
            threshold += 8
        elif self.temp > 28:
            threshold += 5
        elif self.temp > 25:
            threshold += 2
        elif self.temp < 18:
            threshold -= 5
        elif self.temp < 15:
            threshold -= 8
        elif self.temp < 12:
            threshold -= 12
            
        # Điều chỉnh theo độ ẩm không khí
        if self.hum < 35:
            threshold += 10
        elif self.hum < 50:
            threshold += 6
        elif self.hum < 65:
            threshold += 3
        elif self.hum > 85:
            threshold -= 10
        elif self.hum > 75:
            threshold -= 6
        elif self.hum > 65:
            threshold -= 3
            
        return max(50, min(85, threshold))
    
    def on_message(self, client, userdata, msg):
        data = json.loads(msg.payload)
        self.temp = data['temp']
        self.hum = data['hum']
        self.soil = data['soil']
        self.pump = data['pump1'] == 1
        water_level = data['water_percent']
        
        print(f"\n{'='*55}")
        print(f"🌡️ Nhiệt độ: {self.temp}°C  |  💧 Độ ẩm KK: {self.hum}%")
        print(f"🌱 Độ ẩm đất: {self.soil}%  |  🚰 Bơm tưới: {'BẬT' if self.pump else 'TẮT'}")
        print(f"💦 Mực nước: {water_level}%")
        
        # Kiểm tra nước bồn
        if water_level < 20 and self.pump:
            client.publish(TOPIC_COMMAND, "PUMP_OFF")
            print(f"⚠️ TẮT BƠM (Nước bồn thấp)")
            print(f"{'='*55}")
            return
        
        # Chỉ xử lý khi bơm đang BẬT
        if self.pump:
            # Dự đoán ngưỡng khi bơm đang bật
            threshold = self.predict_threshold()
            print(f"🎯 Ngưỡng AI: {threshold}%")
            
            # Gửi ngưỡng lên dashboard
            client.publish(TOPIC_COMMAND, f"PREDICT:{threshold:.0f}%")
            
            # Kiểm tra tắt bơm
            if self.soil >= threshold:
                if time.time() - self.last_cmd > 2:
                    client.publish(TOPIC_COMMAND, "PUMP_OFF")
                    self.last_cmd = time.time()
                    print(f"🔴 TẮT BƠM (Đạt {self.soil}% >= {threshold}%)")
            else:
                need = threshold - self.soil
                print(f"⏳ Cần tưới thêm {need:.0f}% nữa")
        
        else:
            # Bơm đang TẮT - chỉ hiển thị trạng thái, không dự đoán
            print(f"💤 Bơm đang TẮT - Chờ độ ẩm < 40% để bật")
            
            # Gửi trạng thái bơm lên dashboard
            client.publish(TOPIC_COMMAND, "PUMP_IDLE")
            
            # Kiểm tra bật bơm
            if self.soil < 40 and water_level >= 20:
                client.publish(TOPIC_COMMAND, "PUMP_ON")
                print(f"🔵 BẬT BƠM (Độ ẩm {self.soil}% < 40%)")
        
        print(f"{'='*55}")
    
    def run(self):
        self.client.connect(MQTT_BROKER, MQTT_PORT)
        print("🚀 System is running...")
        self.client.loop_forever()

if __name__ == "__main__":
    AI().run()