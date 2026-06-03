# Arquitectura del Proyecto: IoT MQTT TLS (ESP32)

## ¿Qué hace este proyecto?

Firmware para **ESP32** que publica datos de **temperatura y humedad** (sensor SHT21) a un broker **MQTT con cifrado TLS (puerto 8883)**. Incluye pantalla OLED, OTA vía GitHub Actions, y provisioning Wi-Fi por portal web.

---

## Estructura de archivos

```
iot-mqtt-tls/
├── platformio.ini          # Configuración de PlatformIO (board, librerías, variables)
├── .env                    # Credenciales locales (NO subir a git)
├── scripts/
│   ├── add_env_defines.py  # Inyecta variables .env como #defines en la compilación
│   └── build_with_env.py   # Script helper: carga .env y ejecuta pio
├── src/
│   ├── main.cpp            # Punto de entrada: setup() y loop()
│   ├── secrets.cpp/h       # Credenciales MQTT, tópicos, certificado Root CA
│   ├── libiot.cpp/h        # Lógica MQTT TLS + sensor SHT21
│   ├── libwifi.cpp/h       # Conexión y reconexión Wi-Fi
│   ├── libprovision.cpp/h  # Portal AP para provisionar Wi-Fi por primera vez
│   ├── libdisplay.cpp/h    # Pantalla OLED SSD1306
│   ├── libota.cpp/h        # Actualizaciones OTA
│   └── libstorage.cpp/h    # NVS: guarda credenciales Wi-Fi y versión firmware
└── lib/
    └── libwifi.cpp/h       # (copia/alternativa de libwifi para lib/)
```

---

## Cómo se conecta el ESP32 paso a paso

### Flujo de `setup()`

```
1. Serial.begin()           → Puerto serie a 115200 bps
2. GPIO0 (botón BOOT)       → Si se mantiene 3s → factory reset (borra NVS)
3. listWiFiNetworks()       → Escanea redes Wi-Fi disponibles
4. startDisplay()           → Inicia pantalla OLED
5. hasWiFiCredentials()?
   ├─ NO  → startProvisioningAP()   ← Modo AP: crea red "ESP32-Setup-XXXXXX"
   │                                    Portal en http://192.168.4.1
   │                                    Usuario ingresa SSID+pass → guardado en NVS
   └─ SÍ  → startWiFi()            ← Conecta como cliente a la red guardada
6. setupIoT()               → Carga Root CA, configura WiFiClientSecure + PubSubClient
7. setTime()                → Sincroniza hora via SNTP (pool.ntp.org)
```

### Flujo de `loop()`

```
loop()
  ├── isProvisioning()?  → provisioningLoop()  (atiende portal AP)
  ├── checkWiFi()        → Reconecta Wi-Fi si se cae
  ├── checkMQTT()        → Reconecta MQTT si se cae + client.loop() (recibe mensajes)
  ├── checkAlert()       → Lee alertas recibidas por suscripción MQTT
  └── measure()?         → Cada MEASURE_INTERVAL segundos (2s por defecto)
        ├── displayLoop()      → Actualiza pantalla OLED
        └── sendSensorData()   → Publica JSON con temperatura y humedad
```

---

## Arquitectura de conexión de red

```
┌─────────────────────────────────────────────────────────┐
│                        ESP32                            │
│                                                         │
│  Sensor SHT21 (I2C) ──► libiot                         │
│  Pantalla OLED (I2C) ◄─ libdisplay                     │
│                                                         │
│  libwifi ──► WiFi.h ──► Router/AP                       │
│                              │                          │
│  libiot ──► WiFiClientSecure ┘                          │
│          └► PubSubClient (MQTT)                         │
└──────────────────────────────────────┬──────────────────┘
                                       │ TLS (puerto 8883)
                                       ▼
                              ┌─────────────────┐
                              │  Broker MQTT    │
                              │  (ej. Mosquitto)│
                              └────────┬────────┘
                                       │
                    ┌──────────────────┴──────────────────┐
                    │                                      │
              Publica en:                          Suscrito a:
    <país>/<estado>/<ciudad>/<user>/out   <país>/<estado>/<ciudad>/<user>/in
              (datos sensor)                      (alertas/comandos)
```

---

## Seguridad: ¿cómo funciona TLS?

1. El ESP32 usa `WiFiClientSecure` con el **Root CA** (certificado de la autoridad firmante del broker).
2. Al conectar, verifica que el servidor sea legítimo antes de transmitir datos.
3. Todo el tráfico MQTT va **cifrado**, incluyendo usuario y contraseña.
4. El certificado Root CA se almacena en `secrets.cpp` como string PEM y se configura vía `.env`.

---

## Provisioning Wi-Fi (primera vez)

```
ESP32 arranca sin credenciales
        │
        ▼
Crea AP: "ESP32-Setup-XXXXXX"
        │
        ▼
Tu celular/PC se conecta al AP
        │
        ▼
Abre http://192.168.4.1
        │
        ▼
Ingresa SSID + contraseña de tu red
        │
        ▼
ESP32 guarda en NVS y reinicia
        │
        ▼
ESP32 se conecta como cliente Wi-Fi normal
```

Para reconfigurar: **mantén BOOT (GPIO0) presionado 3+ segundos al encender**.

---

## Tópicos MQTT

| Dirección | Tópico | Uso |
|-----------|--------|-----|
| ESP32 → Broker | `<país>/<estado>/<ciudad>/<user>/out` | Publica temperatura y humedad en JSON |
| Broker → ESP32 | `<país>/<estado>/<ciudad>/<user>/in`  | Recibe alertas o comandos externos |

**Ejemplo de payload publicado:**
```json
{
  "mac": "AA:BB:CC:DD:EE:FF",
  "temperatura": 24.5,
  "humedad": 60.2,
  "timestamp": 1714000000
}
```

---

## Variables de configuración (archivo `.env`)

| Variable | Descripción |
|----------|-------------|
| `MQTT_SERVER` | Dirección del broker MQTT |
| `MQTT_PORT` | Puerto (default: `8883` TLS) |
| `MQTT_USER` | Usuario MQTT |
| `MQTT_PASSWORD` | Contraseña MQTT |
| `COUNTRY` / `STATE` / `CITY` | Componen el tópico MQTT |
| `WIFI_SSID` / `WIFI_PASSWORD` | Wi-Fi inicial (opcional, se puede provisionar luego) |

---

## OTA (Over-The-Air)

```
git tag v1.2.0 && git push origin v1.2.0
        │
        ▼
GitHub Actions compila el firmware
        │
        ▼
Sube el .bin a GitHub Releases
        │
        ▼
ESP32 descarga y aplica la actualización automáticamente
(las credenciales Wi-Fi persisten en NVS)
```
