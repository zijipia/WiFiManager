#include <WiFi.h>
#include <WebServer.h>
#include <WiFiSetting.h>

WebServer server(80);
WiFiSetting wifiSetting("ESP32_Setup", "12345678", server);

void setup()
{
  Serial.begin(115200);
  wifiSetting.begin();
  if (!wifiSetting.connectIfStored())
  {
    Serial.println("Không kết nối được WiFi. Chuyển sang AP mode");
    wifiSetting.startAPMode();
  }
  Serial.print("Đã kết nối WiFi: ");
  Serial.println(WiFi.localIP());
}

void loop()
{
  wifiSetting.handleClient();
}
