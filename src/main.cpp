/*
 * main.cpp — Monitoreo ambiental con deteccion de movimiento
 *
 * Hardware:
 *   - ESP32-S3-CAM GOOUUU (OV2640)
 *   - OLED SSD1306 (I2C, 128x64)
 *   - SHT3X (temperatura y humedad, I2C)
 *
 * Flujo:
 *   1. Inicializa bus I2C y OLED
 *   2. Inicializa sensor SHT3X
 *   3. Inicializa camara OV2640
 *   4. Detecta movimiento comparando frames en grises
 *   5. Muestra temp/humedad/movimiento en OLED
 */

#include <Wire.h>
#include <libcam.h>
#include <libdisplay.h>
#include <libweb.h>

#define SHT3X_ADDR_PRIMARY   0x44
#define SHT3X_ADDR_SECONDARY 0x45
#define SENSOR_READ_INTERVAL 2000UL

// Configura aqui tu red para habilitar dashboard web
#define WIFI_SSID     "SANTI"
#define WIFI_PASSWORD "Dianita1680"

// GPS virtual para demo sin hardware fisico
#define GPS_SIM_ENABLED true
#define GPS_BASE_LAT    -12.0464
#define GPS_BASE_LON    -77.0428

static uint8_t g_sht3xAddress = 0;
static bool g_sensorReady = false;
static bool g_cameraReady = false;
static bool g_webReady = false;
static float g_temperatureC = 0.0f;
static float g_humidityRH = 0.0f;
static unsigned long g_lastSensorRead = 0;

static TelemetrySnapshot snapshotProvider() {
  TelemetrySnapshot s;
  s.temperatureC = g_temperatureC;
  s.humidityRH = g_humidityRH;
  s.sensorReady = g_sensorReady;
  s.cameraReady = g_cameraReady;
  s.motionDetected = motionDetected;
  s.motionLevel = motionLevel;
  s.uptimeMs = millis();
  return s;
}

static uint8_t crc8Sht3x(const uint8_t *data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

static bool probeSht3xAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

static bool findSht3x() {
  if (probeSht3xAddress(SHT3X_ADDR_PRIMARY)) {
    g_sht3xAddress = SHT3X_ADDR_PRIMARY;
    return true;
  }
  if (probeSht3xAddress(SHT3X_ADDR_SECONDARY)) {
    g_sht3xAddress = SHT3X_ADDR_SECONDARY;
    return true;
  }
  return false;
}

static bool readSht3x(float &temperatureC, float &humidityRH) {
  if (g_sht3xAddress == 0) {
    return false;
  }

  Wire.beginTransmission(g_sht3xAddress);
  Wire.write(0x2C);
  Wire.write(0x06);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(16);

  const uint8_t expectedBytes = 6;
  if (Wire.requestFrom((int)g_sht3xAddress, (int)expectedBytes) != expectedBytes) {
    return false;
  }

  uint8_t raw[6] = {0};
  for (uint8_t i = 0; i < expectedBytes; i++) {
    raw[i] = Wire.read();
  }

  if (crc8Sht3x(raw, 2) != raw[2] || crc8Sht3x(raw + 3, 2) != raw[5]) {
    return false;
  }

  uint16_t rawTemp = (static_cast<uint16_t>(raw[0]) << 8) | raw[1];
  uint16_t rawHum  = (static_cast<uint16_t>(raw[3]) << 8) | raw[4];

  temperatureC = -45.0f + (175.0f * static_cast<float>(rawTemp) / 65535.0f);
  humidityRH   = 100.0f * static_cast<float>(rawHum) / 65535.0f;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(OLED_SDA, OLED_SCL);
  startDisplay();

  g_sensorReady = findSht3x();
  g_cameraReady = initCamera();

  if (g_sensorReady) {
    Serial.printf("[SHT3X] Sensor detectado en 0x%02X\n", g_sht3xAddress);
  } else {
    Serial.println("[SHT3X] No se detecto sensor en 0x44/0x45");
  }

  g_webReady = initWebDashboard(
    WIFI_SSID,
    WIFI_PASSWORD,
    snapshotProvider,
    GPS_BASE_LAT,
    GPS_BASE_LON,
    GPS_SIM_ENABLED
  );
  if (g_webReady) {
    Serial.println("[WEB] Dashboard: " + getDashboardUrl());
  } else {
    Serial.println("[WEB] Dashboard deshabilitado por falta de WiFi");
  }
}

void loop() {
  handleWebDashboard();
  updateMotionDetection();

  unsigned long now = millis();
  if (g_sensorReady && (now - g_lastSensorRead) >= SENSOR_READ_INTERVAL) {
    g_lastSensorRead = now;
    if (!readSht3x(g_temperatureC, g_humidityRH)) {
      Serial.println("[SHT3X] Lectura fallida");
      g_sensorReady = false;
    } else {
      Serial.printf("[SHT3X] Temp: %.2f C | Hum: %.2f %%\n", g_temperatureC, g_humidityRH);
    }
  }

  displayMonitoring(
    g_temperatureC,
    g_humidityRH,
    motionDetected,
    motionLevel,
    g_sensorReady,
    g_cameraReady
  );

  delay(80);
}

