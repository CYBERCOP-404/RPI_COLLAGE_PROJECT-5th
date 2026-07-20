#include <WiFi.h>
#include <WiFiClientSecure.h>
const char* WIFI_SSID = "NAHID";
const char* WIFI_PASSWORD = "12345678";

const char* BOT_TOKEN = "8601054908:AAFDsk6YDyoHI8Qd24szdb8fGxYuwWQKSVs";
const char* CHAT_ID = "5316471518";

const int DOOR_PIN = 27;
const int GROUND_PIN = 26;
bool lastReading = HIGH;
bool stableState = HIGH;

bool wifiReadyMessageSent = false;
bool doorAlertActive = false;

unsigned long doorOpenMillis = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 60;
String urlEncode(const String &text) {
  String encoded = "";
  for (unsigned int i = 0; i < text.length(); i++) {
    unsigned char c = text[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += (char)c;
    } else {
      char hex[4];
      sprintf(hex, "%%%02X", c);
      encoded += hex;
    }
  }
  return encoded;
}
bool sendTelegramMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi not connected.");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(3);

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("Telegram server connection failed.");
    client.stop();
    return false;
  }

  String url = "/bot" + String(BOT_TOKEN) + "/sendMessage?chat_id=" + String(CHAT_ID) + "&parse_mode=HTML&text=" + urlEncode(message);

  client.print("GET " + url + " HTTP/1.1\r\n");
  client.print("Host: api.telegram.org\r\n");
  client.print("Connection: close\r\n\r\n");

  unsigned long startTime = millis();
  bool success = false;
  
  while (millis() - startTime < 3000) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      if (line.indexOf("200 OK") >= 0) {
        success = true;
        break;
      }
    }
    if (success || !client.connected()) break;
    delay(10);
  }
  
  client.flush();
  client.stop();

  if (success) {
    Serial.println("Telegram message sent.");
    return true;
  }

  Serial.println("Telegram message failed.");
  return false;
}
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  wifiReadyMessageSent = false;
  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi connection failed.");
  }
}
String getDuration(unsigned long totalSeconds) {
  unsigned long days = totalSeconds / 86400;
  totalSeconds %= 86400;
  unsigned long hours = totalSeconds / 3600;
  totalSeconds %= 3600;
  unsigned long minutes = totalSeconds / 60;
  unsigned long seconds = totalSeconds % 60;

  String result = "";
  if (days > 0) result += String(days) + " day ";
  if (hours > 0) result += String(hours) + " hour ";
  if (minutes > 0) result += String(minutes) + " minute ";
  result += String(seconds) + " second";
  
  return result;
}

String getSystemOnlineMessage() {
  String message = "";
  message += "🚀 <b>ESP32 SYSTEM ONLINE</b>\n";
  message += "━━━━━━━━━━━━━━━━━━━━\n";
  message += "✅ <b>Your setup is ready to use.</b>\n\n";
  message += "📡 <b>NETWORK INFORMATION</b>\n";
  message += "🌐 Wi-Fi: <code>" + String(WIFI_SSID) + "</code>\n";
  message += "📍 Local IP: <code>" + WiFi.localIP().toString() + "</code>\n";
  message += "📶 Signal: <code>" + String(WiFi.RSSI()) + " dBm</code>\n";
  message += "🆔 MAC: <code>" + WiFi.macAddress() + "</code>\n\n";
  message += "👨‍💻 Developer: <b>MD.NAHIDUL ISLAM</b>";
  return message;
}
String getDoorOpenMessage() {
  String message = "";
  message += "𓆩𓆩 <b>🚨 ALERT: DOOR OPENED 🚨</b> ツ.𓆪𓆪\n\n";
  message += "🐉 ⊚━━━━━━━━━━━━━━━━━━⊚ 🐉\n";
  message += "🔓 <b>STATUS: ACCESS GRANTED / TRIGGERED</b>\n\n";
  message += "🌐 <b>IP Address:</b> <code>" + WiFi.localIP().toString() + "</code>\n";
  message += "📡 <b>Wi-Fi Network:</b> <code>" + String(WIFI_SSID) + "</code>\n";
  message += "📶 <b>Signal Strength:</b> <code>" + String(WiFi.RSSI()) + " dBm</code>\n\n";
  message += "🐉 ⊚━━━━━━━━━━━━━━━━━━⊚ 🐉\n";
  message += "👨‍💻 Developer: <b>MD.NAHIDUL ISLAM</b>";
  return message;
}
String getDoorClosedMessage(String duration) {
  String message = "";
  message += "𓆩𓆩 <b>🔒 NOTICE: DOOR CLOSED 🔒</b> ツ.𓆪𓆪\n\n";
  message += "🐉 ⊚━━━━━━━━━━━━━━━━━━⊚ 🐉\n";
  message += "✅ <b>STATUS: SECURED / DISCONNECTED</b>\n\n";
  message += "⏱️ <b>Open Duration:</b> <code>" + duration + "</code>\n";
  message += "🌐 <b>IP Address:</b> <code>" + WiFi.localIP().toString() + "</code>\n\n";
  message += "🐉 ⊚━━━━━━━━━━━━━━━━━━⊚ 🐉\n";
  message += "👨‍💻 Developer: <b>MD.NAHIDUL ISLAM</b>";
  return message;
}

void setup() {
  Serial.begin(115200);

  // জিপিআইও কনফিগারেশন
  pinMode(GROUND_PIN, OUTPUT);
  digitalWrite(GROUND_PIN, LOW);
  pinMode(DOOR_PIN, INPUT_PULLUP);

  lastReading = digitalRead(DOOR_PIN);
  stableState = lastReading;

  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (WiFi.status() == WL_CONNECTED && !wifiReadyMessageSent) {
    if (sendTelegramMessage(getSystemOnlineMessage())) {
      wifiReadyMessageSent = true;
    }
  }
  bool reading = digitalRead(DOOR_PIN);
  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != stableState) {
      stableState = reading;
      if (WiFi.status() == WL_CONNECTED) {
        if (stableState == LOW && !doorAlertActive) {
          doorAlertActive = true;
          doorOpenMillis = millis();

          sendTelegramMessage(getDoorOpenMessage());
        }
        else if (stableState == HIGH && doorAlertActive) {
          unsigned long totalOpenSeconds = (millis() - doorOpenMillis) / 1000;
          String duration = getDuration(totalOpenSeconds);

          sendTelegramMessage(getDoorClosedMessage(duration));
          doorAlertActive = false;
          doorOpenMillis = 0;
        }
      }
    }
  }

  lastReading = reading;
  delay(10);
}
// Developer: MD.NAHIDUL ISLAM
