#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

const char* WIFI_SSID = "ssid";
const char* WIFI_PASSWORD = "pass";

const char* BOT_TOKEN = "";
const char* CHAT_ID = "5316471518";
const int DOOR_PIN = 27;
const int GROUND_PIN = 26;

const long GMT_OFFSET_SEC = 6 * 60 * 60;
const int DAYLIGHT_OFFSET_SEC = 0;

bool lastReading = HIGH;
bool stableState = HIGH;

bool wifiReadyMessageSent = false;
bool nightModePreviouslyActive = false;
bool doorAlertActive = false;

unsigned long doorOpenMillis = 0;
String doorOpenDateTime = "";

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 60;

String urlEncode(const String &text) {
  String encoded = "";

  for (unsigned int i = 0; i < text.length(); i++) {
    unsigned char c = text[i];

    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
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

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("Telegram server connection failed.");
    return false;
  }

  String url = "/bot";
  url += BOT_TOKEN;
  url += "/sendMessage?chat_id=";
  url += CHAT_ID;
  url += "&parse_mode=HTML&text=";
  url += urlEncode(message);

  client.print("GET ");
  client.print(url);
  client.print(" HTTP/1.1\r\n");
  client.print("Host: api.telegram.org\r\n");
  client.print("Connection: close\r\n\r\n");

  String response = "";
  unsigned long startTime = millis();

  while (millis() - startTime < 8000) {
    while (client.available()) {
      response += client.readStringUntil('\n');
    }

    if (!client.connected()) {
      break;
    }
  }

  client.stop();

  if (response.indexOf("200 OK") >= 0) {
    Serial.println("Telegram message sent.");
    return true;
  }

  Serial.println("Telegram message failed.");
  return false;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  wifiReadyMessageSent = false;

  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // Get Bangladesh time from internet
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC,
               "pool.ntp.org", "time.google.com");

    struct tm timeinfo;
    unsigned long timeStart = millis();

    while (!getLocalTime(&timeinfo) &&
           millis() - timeStart < 10000) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("\nTime sync complete.");
  } else {
    Serial.println("\nWi-Fi connection failed.");
  }
}

String getDateTime() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 2000)) {
    return "Time not synced";
  }

  char buffer[40];

  strftime(buffer, sizeof(buffer),
           "%d-%m-%Y | %I:%M:%S %p", &timeinfo);

  return String(buffer);
}
String getDuration(unsigned long totalSeconds) {
  unsigned long days = totalSeconds / 86400;
  totalSeconds %= 86400;

  unsigned long hours = totalSeconds / 3600;
  totalSeconds %= 3600;

  unsigned long minutes = totalSeconds / 60;
  unsigned long seconds = totalSeconds % 60;

  String result = "";

  if (days > 0) {
    result += String(days);
    result += " day ";
  }

  if (hours > 0) {
    result += String(hours);
    result += " hour ";
  }

  if (minutes > 0) {
    result += String(minutes);
    result += " minute ";
  }

  result += String(seconds);
  result += " second";

  return result;
}

bool isNightMode() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 1000)) {
    return false;
  }

  int currentHour = timeinfo.tm_hour;

  // 20 = 8 PM, 7 = 7 AM
  if (currentHour >= 20 || currentHour < 7) {
    return true;
  }

  return false;
}

String getSystemOnlineMessage() {
  String message = "";

  message += "🚀 <b>ESP32 SYSTEM ONLINE</b>\n";
  message += "━━━━━━━━━━━━━━━━━━━━\n";
  message += "✅ <b>Your setup is ready to use.</b>\n\n";

  message += "📡 <b>NETWORK INFORMATION</b>\n";
  message += "🌐 Wi-Fi: <code>";
  message += WIFI_SSID;
  message += "</code>\n";

  message += "📍 Local IP: <code>";
  message += WiFi.localIP().toString();
  message += "</code>\n";

  message += "📶 Signal: <code>";
  message += String(WiFi.RSSI());
  message += " dBm</code>\n";

  message += "🆔 MAC: <code>";
  message += WiFi.macAddress();
  message += "</code>\n\n";

  message += "📅 Date & Time: <code>";
  message += getDateTime();
  message += "</code>\n\n";

  message += "👨‍💻 Developer: <b>MD.NAHIDUL ISLAM</b>";

  return message;
}
String getNightModeMessage() {
  String message = "";

  message += "🌙 <b>NIGHT MODE ACTIVATED</b>\n";
  message += "━━━━━━━━━━━━━━━━━━━━\n";
  message += "🛡️ Security system is now armed.\n\n";

  message += "📅 Date & Time: <code>";
  message += getDateTime();
  message += "</code>\n";

  message += "🌐 IP: <code>";
  message += WiFi.localIP().toString();
  message += "</code>\n";

  message += "👨‍💻 Developer: <b>MD.NAHIDUL ISLAM</b>";

  return message;
}

String getDoorOpenMessage() {
  String message = "";

  message += "𓆩𓆩 <b>𝙷𝙸 𝚈𝙾𝚄 𝙷𝙰𝚅𝙴 𝙽𝙴𝚆 𝙷𝙸𝚃</b> ツ.𓆪𓆪\n\n";
  message += "🐉 ⊚━━━━━━━━━━━━━━━━━━⊚ 🐉\n";
  message += "🚨 <b>DOOR / TRIGGER CONNECTED!</b>\n\n";

  message += "📅 Connected Time: <code>";
  message += doorOpenDateTime;
  message += "</code>\n";

  message += "🌐 IP: <code>";
  message += WiFi.localIP().toString();
  message += "</code>\n";

  message += "📡 Wi-Fi: <code>";
  message += WIFI_SSID;
  message += "</code>\n";

  message += "📶 Signal: <code>";
  message += String(WiFi.RSSI());
  message += " dBm</code>\n\n";

  message += "🐉 ⊚━━━━━━━━━━━━━━━━━━⊚ 🐉\n";
  message += "👨‍💻 Developer: <b>MD.NAHIDUL ISLAM</b>";

  return message;
}

String getDoorClosedMessage(String duration) {
  String message = "";

  message += "✅ <b>DOOR / TRIGGER DISCONNECTED</b>\n";
  message += "━━━━━━━━━━━━━━━━━━━━\n\n";

  message += "📅 Connected At: <code>";
  message += doorOpenDateTime;
  message += "</code>\n";

  message += "📅 Disconnected At: <code>";
  message += getDateTime();
  message += "</code>\n";

  message += "⏱ Connected Duration: <b>";
  message += duration;
  message += "</b>\n\n";

  message += "🌐 IP: <code>";
  message += WiFi.localIP().toString();
  message += "</code>\n";

  message += "👨‍💻 Developer: <b>MD.NAHIDUL ISLAM</b>";

  return message;
}

void setup() {
  Serial.begin(115200);

  // GPIO26 always LOW: works as virtual GND
  pinMode(GROUND_PIN, OUTPUT);
  digitalWrite(GROUND_PIN, LOW);

  // GPIO27 normally HIGH
  pinMode(DOOR_PIN, INPUT_PULLUP);

  lastReading = digitalRead(DOOR_PIN);
  stableState = lastReading;

  connectWiFi();
}

void loop() {
  // Auto reconnect Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Wi-Fi connected হলে একবার online message
  if (WiFi.status() == WL_CONNECTED && !wifiReadyMessageSent) {
    if (sendTelegramMessage(getSystemOnlineMessage())) {
      wifiReadyMessageSent = true;
    }
  }

  bool nightModeActive = isNightMode();

  // রাত 8 PM হলে armed message
  if (nightModeActive &&
      !nightModePreviouslyActive &&
      WiFi.status() == WL_CONNECTED) {
    sendTelegramMessage(getNightModeMessage());
  }

  // সকাল 7 AM হলে alert বন্ধ
  if (!nightModeActive && nightModePreviouslyActive) {
    doorAlertActive = false;
    Serial.println("Day mode: Door alerts disabled.");
  }

  nightModePreviouslyActive = nightModeActive;

  // GPIO27 read
  bool reading = digitalRead(DOOR_PIN);

  // Debounce
  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != stableState) {
      stableState = reading;

      // শুধু Night Mode-এ alert যাবে
      if (nightModeActive && WiFi.status() == WL_CONNECTED) {

        // GPIO27 and GPIO26 connected
        if (stableState == LOW && !doorAlertActive) {
          doorAlertActive = true;
          doorOpenMillis = millis();
          doorOpenDateTime = getDateTime();

          sendTelegramMessage(getDoorOpenMessage());
        }

        // GPIO27 and GPIO26 disconnected
        else if (stableState == HIGH && doorAlertActive) {
          unsigned long totalOpenSeconds;
          totalOpenSeconds = (millis() - doorOpenMillis) / 1000;

          String duration = getDuration(totalOpenSeconds);

          sendTelegramMessage(getDoorClosedMessage(duration));

          doorAlertActive = false;
          doorOpenMillis = 0;
          doorOpenDateTime = "";
        }
      }
    }
  }

  lastReading = reading;
  delay(10);
}
