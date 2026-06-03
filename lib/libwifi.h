#ifndef LIBWIFI_H
#define LIBWIFI_H

#include <Arduino.h>
#include <WiFi.h>

// Función para iniciar la conexión WiFi
void startWiFi(const char* hostname = "");

// Función para verificar el estado de la conexión WiFi
void checkWiFi();

// Función para listar las redes WiFi disponibles
void listWiFiNetworks();

#endif // LIBWIFI_H 