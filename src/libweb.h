#ifndef LIBWEB_H
#define LIBWEB_H

#include <Arduino.h>

struct TelemetrySnapshot {
  float temperatureC;
  float humidityRH;
  bool sensorReady;
  bool cameraReady;
  bool motionDetected;
  float motionLevel;
  unsigned long uptimeMs;
};

typedef TelemetrySnapshot (*TelemetryProvider)();

bool initWebDashboard(const char *ssid, const char *password, TelemetryProvider provider,
                      double gpsBaseLat = -12.0464, double gpsBaseLon = -77.0428,
                      bool gpsSimulationEnabled = true);
void handleWebDashboard();
String getDashboardUrl();

#endif /* LIBWEB_H */
