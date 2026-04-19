import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Connected successfully to HiveMQ!")
    else:
        print(f"❌ Connection failed with code {rc}")

client = mqtt.Client()
client.on_connect = on_connect

# BẮT BUỘC: Phải có dòng này để chạy cổng 8883 (SSL)
client.tls_set() 

# Nhập đúng User/Pass bạn vừa tạo trên Web
client.username_pw_set("XUANCUONG", "Hoxuancuong26082005@")

# Kết nối
client.connect("53096db1c4e64e078478b373c96ab3a8.s1.eu.hivemq.cloud", 8883)
client.loop_forever()