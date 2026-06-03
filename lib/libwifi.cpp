#include "libwifi.h"

void startWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
}

void checkWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
    }
}

void listWiFiNetworks() {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
        // En una implementación real, aquí se procesarían las redes encontradas
    }
} 