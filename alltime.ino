#include <WiFi.h>
#include <WiFiClientSecure.h>

// ================= YOUR SETTINGS =================
const char* WIFI_SSID = "ESPTEST";
const char* WIFI_PASSWORD = "12345678";

const char* BOT_TOKEN = "8601054908:AAFDsk6YDyoHI8Qd24szdb8fGxYuwWQKSVs";
const char* CHAT_ID = "5316471518";

const int TRIGGER_PIN = 27;
const int GROUND_PIN = 26;
// =================================================

bool lastReading = HIGH;
bool stableState = HIGH;

bool wifiReadyMessageSent = false;
bool triggerActive = false;
bool triggerOpenMessageSent = false;

unsigned long triggerStartMillis = 0;
unsigned long lastDebounceTime = 0;

const unsigned long DEBOUNCE_DELAY = 60;

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
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.telegram.org", 443)) {
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

  return response.indexOf("200 OK") >= 0;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  wifiReadyMessageSent = false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting Wi-Fi...");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected.");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi connection failed.");
  }
}

String getDuration(unsigned long seconds) {
  unsigned long hours = seconds / 3600;
  seconds %= 3600;

  unsigned long minutes = seconds / 60;
  seconds %= 60;

  String result = "";

  if (hours > 0) {
    result += String(hours) + " hour ";
  }

  if (minutes > 0) {
    result += String(minutes) + " minute ";
  }

  result += String(seconds) + " second";

  return result;
}

String getReadyMessage() {
  String message = "";

  message += "<b>ESP32 SYSTEM ONLINE</b>\n";
  message += "--------------------------\n";
  message += "Your setup is ready to use.\n\n";

  message += "Wi-Fi: <code>";
  message += WIFI_SSID;
  message += "</code>\n";

  message += "IP: <code>";
  message += WiFi.localIP().toString();
  message += "</code>\n";

  message += "Signal: <code>";
  message += String(WiFi.RSSI());
  message += " dBm</code>\n\n";

  message += "Developer: <b>MD.NAHIDUL ISLAM</b>";

  return message;
}

String getConnectedMessage() {
  String message = "";

  message += "<b>NEW TRIGGER DETECTED</b>\n";
  message += "--------------------------\n";
  message += "GPIO27 and GPIO26 are CONNECTED.\n\n";

  message += "Status: <b>CONNECTED</b>\n";
  message += "IP: <code>";
  message += WiFi.localIP().toString();
  message += "</code>\n";

  message += "Developer: <b>MD.NAHIDUL ISLAM</b>";

  return message;
}

String getDisconnectedMessage(String duration) {
  String message = "";

  message += "<b>TRIGGER DISCONNECTED</b>\n";
  message += "--------------------------\n";
  message += "GPIO27 and GPIO26 are disconnected.\n\n";

  message += "Connected Duration: <b>";
  message += duration;
  message += "</b>\n\n";

  message += "IP: <code>";
  message += WiFi.localIP().toString();
  message += "</code>\n";

  message += "Developer: <b>MD.NAHIDUL ISLAM</b>";

  return message;
}

void startTrigger() {
  triggerActive = true;
  triggerOpenMessageSent = false;
  triggerStartMillis = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (sendTelegramMessage(getConnectedMessage())) {
      triggerOpenMessageSent = true;
    }
  }
}

void stopTrigger() {
  unsigned long durationSeconds =
    (millis() - triggerStartMillis) / 1000;

  if (WiFi.status() == WL_CONNECTED) {
    sendTelegramMessage(
      getDisconnectedMessage(getDuration(durationSeconds))
    );
  }

  triggerActive = false;
  triggerOpenMessageSent = false;
  triggerStartMillis = 0;
}

void setup() {
  Serial.begin(115200);

  // GPIO26 stays LOW
  pinMode(GROUND_PIN, OUTPUT);
  digitalWrite(GROUND_PIN, LOW);

  // GPIO27 stays HIGH until connected with GPIO26
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  lastReading = digitalRead(TRIGGER_PIN);
  stableState = lastReading;

  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Wi-Fi connect message
  if (WiFi.status() == WL_CONNECTED &&
      !wifiReadyMessageSent) {
    if (sendTelegramMessage(getReadyMessage())) {
      wifiReadyMessageSent = true;
    }
  }

  bool reading = digitalRead(TRIGGER_PIN);

  // Debounce
  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != stableState) {
      stableState = reading;

      // GPIO27 connected to GPIO26
      if (stableState == LOW && !triggerActive) {
        startTrigger();
      }

      // GPIO27 disconnected from GPIO26
      else if (stableState == HIGH && triggerActive) {
        stopTrigger();
      }
    }
  }

  // If connection happened while Wi-Fi was offline,
  // send connected message after Wi-Fi returns
  if (WiFi.status() == WL_CONNECTED &&
      triggerActive &&
      !triggerOpenMessageSent) {
    if (sendTelegramMessage(getConnectedMessage())) {
      triggerOpenMessageSent = true;
    }
  }

  lastReading = reading;
  delay(10);
}
