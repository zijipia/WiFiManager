#include "WiFiSetting.h"
#include <DNSServer.h>

#define EEPROM_SIZE 96
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MAX_SSID 32
#define MAX_PASS 64

/**
 * @brief Tạo một WiFiSetting với tên AP, mật khẩu AP, mDNS name và WebServer.
 *
 * @param apSSID Tên SSID khi tạo chế độ AP.
 * @param apPassword Mật khẩu AP mode.
 * @param serverRef Tham chiếu đến WebServer đang dùng.
 */
WiFiSetting::WiFiSetting(const char *apSSID, const char *apPassword, WebServer &serverRef)
    : ap_ssid(apSSID), ap_password(apPassword), server(serverRef) {}

/**
 * @brief Khởi tạo EEPROM, nên gọi ở trong `setup()`.
 */
void WiFiSetting::begin()
{
    EEPROM.begin(EEPROM_SIZE);
}

/**
 * @brief Kết nối WiFi sử dụng SSID và mật khẩu lưu trong EEPROM.
 *
 * Nếu kết nối thành công, sẽ khởi động mDNS với tên được cung cấp.
 *
 * @return true Nếu kết nối thành công
 * @return false Nếu không kết nối được WiFi
 */
bool WiFiSetting::connectIfStored()
{
    String ssid, pass;
    readWiFiFromEEPROM(ssid, pass);
    if (ssid.length() > 0)
    {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 20)
        {
            delay(1000);
            Serial.print(".");
            retry++;
        }
    }
    return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief Bật chế độ Access Point (AP) và tạo portal cấu hình WiFi qua web.
 *
 * Giao diện web: http://192.168.4.1 hoặc mDNS.local đã set trong class (hoặc theo IP của AP ESP32)
 */
void WiFiSetting::startAPMode()
{
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    delay(100);

    WiFi.softAP(ap_ssid, ap_password);

    DNSServer dnsServer;
    dnsServer.start(53, "*", WiFi.softAPIP());

    delay(500);
    server.on("/", HTTP_GET, std::bind(&WiFiSetting::handleRoot, this));
    server.on("/setup", HTTP_POST, std::bind(&WiFiSetting::handleSetup, this));
    server.on("/generate_204", HTTP_GET, std::bind(&WiFiSetting::handleRoot, this));        // Android
    server.on("/hotspot-detect.html", HTTP_GET, std::bind(&WiFiSetting::handleRoot, this)); // Apple
    server.on("/fwlink", HTTP_GET, std::bind(&WiFiSetting::handleRoot, this));              // Windows
    server.onNotFound(std::bind(&WiFiSetting::handleRoot, this));                           // tất cả các route khác

    server.begin();
    while (true)
    {
        server.handleClient();
        delay(10);
    }
}

/**
 * @brief Gọi trong vòng lặp `loop()` để xử lý các request HTTP khi đã kết nối WiFi.
 */
void WiFiSetting::handleClient()
{
    server.handleClient();
}

/**
 * @brief Trang giao diện chính (GET /), trả về HTML chọn SSID và nhập mật khẩu.
 */
void WiFiSetting::handleRoot()
{
    String html = R"rawliteral(
    <!DOCTYPE html><html><head><meta charset="UTF-8"><title>WiFi Setup</title>
    <style>
      body { font-family: Arial; background: #f0f2f5; padding: 20px; display: flex; justify-content: center; }
      .container { background: white; padding: 20px 30px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); max-width: 400px; width: 100%; }
      h1 { text-align: center; color: #333; }
      label { font-weight: bold; }
      select, input[type='password'], input[type='submit'] { width: 100%; height: 40px; padding: 10px; margin: 8px 0 16px 0; border: 1px solid #ccc; border-radius: 5px;   box-sizing: border-box; }
      input[type='submit'] { background-color: #007bff; color: white; border: none; cursor: pointer; }
      input[type='submit']:hover { background-color: #0056b3; }
    </style></head><body>
    <div class='container'><h1>WiFi Setup</h1>
    <form action='/setup' method='POST'>
      <label for='ssid'>SSID:</label>
      <select name='ssid'>
  )rawliteral";

    html += generateWiFiOptions();

    html += R"rawliteral(
      </select>
      <label for='password'>Password:</label>
      <input type='password' name='password' placeholder='WiFi password'>
      <input type='submit' value='Connect'>
    </form></div></body></html>
  )rawliteral";

    server.send(200, "text/html", html);
}

/**
 * @brief Xử lý POST từ form cấu hình WiFi, lưu vào EEPROM và khởi động lại.
 */
void WiFiSetting::handleSetup()
{
    if (server.hasArg("ssid") && server.hasArg("password"))
    {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        saveWiFiToEEPROM(ssid, password);
        server.send(200, "text/html; charset=utf-8", "Đã nhận thông tin, thiết bị sẽ thử kết nối lại...");
        delay(1000);
        ESP.restart();
    }
    else
    {
        server.send(400, "text/html", "Missing SSID or Password");
    }
}

/**
 * @brief Sinh ra danh sách `<option>` SSID quét được cho dropdown trong HTML.
 *
 * Sử dụng WiFi quét không đồng bộ để tăng hiệu suất.
 *
 * @return Chuỗi HTML danh sách các mạng WiFi.
 */
String WiFiSetting::generateWiFiOptions()
{

    String options;
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i)
    {
        options += "<option value='" + WiFi.SSID(i) + "'>";
        options += WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
    }
    return options;
}

/**
 * @brief Lưu SSID và mật khẩu vào EEPROM.
 *
 * @param ssid Tên WiFi.
 * @param pass Mật khẩu WiFi.
 */
void WiFiSetting::saveWiFiToEEPROM(const String &ssid, const String &pass)
{
    EEPROM.writeString(SSID_ADDR, ssid);
    EEPROM.writeString(PASS_ADDR, pass);
    EEPROM.commit();
}

/**
 * @brief Đọc SSID và mật khẩu đã lưu từ EEPROM.
 *
 * @param ssid Tham chiếu để nhận SSID.
 * @param pass Tham chiếu để nhận mật khẩu.
 */
void WiFiSetting::readWiFiFromEEPROM(String &ssid, String &pass)
{
    ssid = EEPROM.readString(SSID_ADDR);
    pass = EEPROM.readString(PASS_ADDR);
}
