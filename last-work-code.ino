#include <WiFi.h>

void setup() {
  Serial.begin(9600);
  delay(3000);

  Serial.println("ESP32 WIFI AP TEST");

  WiFi.mode(WIFI_AP);

  bool ok = WiFi.softAP("ESP32_TEST_24G", "12345678", 1);

  if (ok) {
    Serial.println("AP STARTED: YES");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("AP STARTED: NO");
  }
}

void loop() {
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  delay(2000);
}
