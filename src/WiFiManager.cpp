#include "WiFiManager.h"
#include <ESPmDNS.h>

#define EEPROM_SIZE 96
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MAX_SSID 32
#define MAX_PASS 64

/**
 * @brief Tạo một WiFiManager với tên AP, mật khẩu AP, mDNS name và WebServer.
 *
 * @param apSSID Tên SSID khi tạo chế độ AP.
 * @param apPassword Mật khẩu AP mode.
 * @param Ns Tên mDNS (ví dụ: "esp32" sẽ tạo http://esp32.local).
 * @param serverRef Tham chiếu đến WebServer đang dùng.
 */
WiFiManager::WiFiManager(const char *apSSID, const char *apPassword, const char *Ns, WebServer &serverRef)
    : ap_ssid(apSSID), ap_password(apPassword), mdns_name(Ns), server(serverRef) {}

/**
 * @brief Khởi tạo EEPROM, nên gọi ở trong `setup()`.
 */
void WiFiManager::begin()
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
bool WiFiManager::connectIfStored()
{
    String ssid, pass;
    readWiFiFromEEPROM(ssid, pass);
    if (ssid.length() > 0)
    {
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
 * Giao diện web: http://192.168.4.1 (hoặc theo IP của AP ESP32)
 */
void WiFiManager::startAPMode()
{
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    delay(100);

    if (!MDNS.begin(mdns_name))
    {
        Serial.println("❌ mDNS khởi tạo thất bại");
    }
    else
    {
        Serial.print("✅ mDNS chạy tại: http://");
        Serial.print(mdns_name);
        Serial.println(".local");
    }

    WiFi.softAP(ap_ssid, ap_password);
    delay(500);
    server.on("/", HTTP_GET, std::bind(&WiFiManager::handleRoot, this));
    server.on("/setup", HTTP_POST, std::bind(&WiFiManager::handleSetup, this));
    server.on("/generate_204", HTTP_GET, std::bind(&WiFiManager::handleRoot, this));        // Android
    server.on("/hotspot-detect.html", HTTP_GET, std::bind(&WiFiManager::handleRoot, this)); // Apple
    server.on("/fwlink", HTTP_GET, std::bind(&WiFiManager::handleRoot, this));              // Windows
    server.onNotFound(std::bind(&WiFiManager::handleRoot, this));                           // tất cả các route khác

    server.begin();
    MDNS.addService("http", "tcp", 80);

    while (true)
    {
        server.handleClient();
        delay(10);
    }
}

/**
 * @brief Gọi trong vòng lặp `loop()` để xử lý các request HTTP khi đã kết nối WiFi.
 */
void WiFiManager::handleClient()
{
    server.handleClient();
}

/**
 * @brief Trang giao diện chính (GET /), trả về HTML chọn SSID và nhập mật khẩu.
 */
void WiFiManager::handleRoot()
{
    String html = R"rawliteral(
    <!DOCTYPE html><html><head><meta charset="UTF-8"><title>WiFi Setup</title>
    <style>
      body { font-family: Arial; background: #f0f2f5; padding: 20px; display: flex; justify-content: center; }
      .container { background: white; padding: 20px 30px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); max-width: 400px; width: 100%; }
      h1 { text-align: center; color: #333; }
      label { font-weight: bold; }
      select, input[type='password'], input[type='submit'] { width: 100%; padding: 10px; margin: 8px 0 16px 0; border: 1px solid #ccc; border-radius: 5px; }
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
void WiFiManager::handleSetup()
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
String WiFiManager::generateWiFiOptions()
{
    String options;

    int scanStatus = WiFi.scanComplete();
    if (scanStatus == -2)
    {
        WiFi.scanNetworks(true);
        options += "<option disabled>Đang quét mạng WiFi...</option>";
    }
    else if (scanStatus >= 0)
    {
        for (int i = 0; i < scanStatus; ++i)
        {
            options += "<option value='" + WiFi.SSID(i) + "'>";
            options += WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
        }
        WiFi.scanDelete();
    }
    else
    {
        options += "<option disabled>Đang quét mạng WiFi...</option>";
    }

    return options;
}

/**
 * @brief Lưu SSID và mật khẩu vào EEPROM.
 *
 * @param ssid Tên WiFi.
 * @param pass Mật khẩu WiFi.
 */
void WiFiManager::saveWiFiToEEPROM(const String &ssid, const String &pass)
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
void WiFiManager::readWiFiFromEEPROM(String &ssid, String &pass)
{
    ssid = EEPROM.readString(SSID_ADDR);
    pass = EEPROM.readString(PASS_ADDR);
}
