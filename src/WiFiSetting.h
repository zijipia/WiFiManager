#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

class WiFiSetting
{
public:
    WiFiSetting(const char *apSSID, const char *apPassword, WebServer &serverRef);

    void begin();
    bool connectIfStored();
    void startAPMode();
    void handleClient();

private:
    const char *ap_ssid;
    const char *ap_password;
    WebServer &server;
    void handleRoot();
    void handleSetup();
    String generateWiFiOptions();

    void saveWiFiToEEPROM(const String &ssid, const String &pass);
    void readWiFiFromEEPROM(String &ssid, String &pass);
};

#endif
