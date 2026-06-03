/*
 * Storage utilities for persisting credentials in NVS
 */

#ifndef LIBSTORAGE_H
#define LIBSTORAGE_H

#include <Arduino.h>

// Wi‑Fi credentials
bool saveWiFiCredentials(const String &ssid, const String &password);
bool loadWiFiCredentials(String &outSsid, String &outPassword);
bool clearWiFiCredentials();
bool hasWiFiCredentials();

// Firmware version
bool saveFirmwareVersion(const String &version);
bool loadFirmwareVersion(String &outVersion);
String getFirmwareVersion(); // Retorna la versión guardada o la constante por defecto

#endif /* LIBSTORAGE_H */
