#include <DHT.h>
#include <HardwareSerial.h>

#define DHTPIN 21
#define DHTTYPE DHT11
#define LEDPIN 33
#define BUZZERPIN 32
#define SIM_EN 5
#define MODEM_RX 16
#define MODEM_TX 17

DHT dht(DHTPIN, DHTTYPE);
HardwareSerial gsmSerial(1);

bool smsSent = false;
unsigned long lastSMSTime = 0;
unsigned long resendInterval = 60000; // 60 seconds

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(LEDPIN, OUTPUT);
  pinMode(BUZZERPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);
  digitalWrite(BUZZERPIN, LOW);

  pinMode(SIM_EN, OUTPUT);
  digitalWrite(SIM_EN, HIGH);
  delay(3000);

  gsmSerial.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  sendAT("AT");
  sendAT("ATE0");
  sendAT("AT+CMGF=1");
  sendAT("AT+CSCS=\"GSM\"");
  sendAT("AT+CNMI=1,2,0,0,0");
  sendAT("AT+CSCA=\"+919830380000\"");

  setupGPRS();
  setupMQTT();

  Serial.println("✅ System Ready");
}

void loop() {
  float temp = dht.readTemperature();
  if (isnan(temp)) {
    Serial.println("❌ Failed to read temperature.");
    return;
  }

  Serial.print("🌡️ Temperature: ");
  Serial.print(temp);
  if (temp > 29.0) {
    Serial.println("°C - ❌ Not Safe");

    for (int i = 0; i < 6; i++) {
      digitalWrite(LEDPIN, HIGH);
      digitalWrite(BUZZERPIN, HIGH);
      delay(500);
      digitalWrite(LEDPIN, LOW);
      digitalWrite(BUZZERPIN, LOW);
      delay(500);
    }

    if (!smsSent || millis() - lastSMSTime >= resendInterval) {
      String msg = "🚨 ALERT: High Temperature Detected! 🌡️ Temp: " + String(temp, 1) + "°C 🌡️";
      sendSMS("+919345611558", msg);
      smsSent = true;
      lastSMSTime = millis();
    }

    publishMQTT("navz/led", "ON");
    publishMQTT("navz/temp", String(temp, 1));
    publishMQTT("navz/buzzer", "ON");
    
  } else {
    Serial.println("°C - ✅ Safe");
    digitalWrite(LEDPIN, LOW);
    digitalWrite(BUZZERPIN, LOW);
    smsSent = false;
    publishMQTT("navz/led", "OFF");
    publishMQTT("navz/temp", String(temp, 1));
    publishMQTT("navz/buzzer", "OFF");
    Serial.println("📡 MQTT Published");
  }
  delay(5000);
}

void sendSMS(String number, String message) {
  gsmSerial.println("AT+CMGF=1");
  delay(1000);
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(number);
  gsmSerial.println("\"");
  delay(1000);
  gsmSerial.print(message);
  delay(500);
  gsmSerial.write(26);
  delay(7000);
  Serial.println("✅ SMS sent");
}

void sendAT(String cmd) {
  gsmSerial.println(cmd);
  delay(1000);
  while (gsmSerial.available()) Serial.write(gsmSerial.read());
  Serial.println();
}

void setupGPRS() {
  sendAT("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  sendAT("AT+SAPBR=3,1,\"APN\",\"www\"");
  sendAT("AT+SAPBR=1,1");
  delay(3000);
  sendAT("AT+SAPBR=2,1");
}

void setupMQTT() {
  sendAT("AT+CMQTTSTART");
  delay(3000);
  sendAT("AT+CMQTTACCQ=0,\"navzclient\"");
  sendAT("AT+CMQTTCONNECT=0,\"tcp://broker.hivemq.com:1883\",60,1");
  delay(5000);
}

void publishMQTT(String topic, String payload) {
  String cmd = "AT+CMQTTTOPIC=0," + String(topic.length());
  gsmSerial.println(cmd);
  delay(100);
  gsmSerial.print(topic);
  delay(100);

  cmd = "AT+CMQTTPAYLOAD=0," + String(payload.length());
  gsmSerial.println(cmd);
  delay(100);
  gsmSerial.print(payload);
  delay(100);

  gsmSerial.println("AT+CMQTTPUB=0,1,60");
  delay(3000);
}
