#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>

WebServer server(80);
WiFiManager wifiManager("ESP32_Setup", "12345678", "esp32", server);

void setup()
{
  Serial.begin(115200);
  wifiManager.begin();
  if (!wifiManager.connectIfStored())
  {
    Serial.println("Không kết nối được WiFi. Chuyển sang AP mode");
    wifiManager.startAPMode();
  }
  Serial.print("Đã kết nối WiFi: ");
  Serial.println(WiFi.localIP());
}

void loop()
{
  wifiManager.handleClient();
}
