## Configuración de Secrets y .env (Build local y OTA)

Este proyecto usa un archivo `.env` local y GitHub Secrets para manejar credenciales de forma segura. El código no contiene secretos reales.

### Variables soportadas
- `COUNTRY`, `STATE`, `CITY`: etiquetas para formar tópicos MQTT.
- `MQTT_SERVER`, `MQTT_PORT`, `MQTT_USER` (opcional), `MQTT_PASSWORD` (opcional)
- `WIFI_SSID`, `WIFI_PASSWORD` (solo como valores iniciales; en producción se usa aprovisionamiento por AP y NVS)
- `ROOT_CA`: certificado raíz PEM en una sola línea con `\n` entre líneas.

### Crear y llenar `.env`
1) En la raíz del proyecto, crea `.env` y agrega:

```ini
COUNTRY=colombia
STATE=valle
CITY=tulua
MQTT_SERVER=mqtt.tu-dominio.com
MQTT_PORT=8883
MQTT_USER=miuser
MQTT_PASSWORD=supersecreto
WIFI_SSID=MiWiFiInicial
WIFI_PASSWORD=MiPassInicial
ROOT_CA=-----BEGIN CERTIFICATE-----\nMIIF...\n-----END CERTIFICATE-----
```

Notas:
- `ROOT_CA` debe quedar en una sola línea con `\n` donde corresponda salto de línea. Si no lo agregas se toma por defecto el certificado root de LetsEncrypt.
- `WIFI_SSID`/`WIFI_PASSWORD` son opcionales: el dispositivo guarda credenciales en NVS mediante el portal AP.

### Cómo compilar y subir localmente
- Compilar:
```bash
python scripts/build_with_env.py
```
- Subir al dispositivo:
```bash
python scripts/build_with_env.py upload
```

El archivo `platformio.ini` ejecuta `scripts/add_env_defines.py` como `extra_scripts`, que lee `.env` y exporta las claves como macros de compilación (sin tener que pasar `-D` manualmente).

### Configurar GitHub Secrets (para CI/CD y OTA)
Ve a GitHub → repo → Settings → Secrets and variables → Actions → New repository secret y añade:

Secrets para build (si tu workflow los usa):
- `COUNTRY`, `STATE`, `CITY`
- `MQTT_SERVER`, `MQTT_PORT`, `MQTT_USER` (opcional), `MQTT_PASSWORD` (opcional)
- `ROOT_CA` (opcional si usas CA pública embebida)

Secrets para OTA (usados por `.github/workflows/ota-update.yml`):
- `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`, `AWS_REGION`, `S3_BUCKET_NAME`
- `MQTT_SERVER`, `MQTT_PORT`, `MQTT_USER` (opcional), `MQTT_PASSWORD` (opcional), `MQTT_TLS` (`true`/`false`)

El workflow compila, sube `firmware_*.bin` a S3 y publica un mensaje MQTT al tópico `OTA_TOPIC` definido en `src/libota.h` (por defecto `dispositivo/device1/ota`).

### Buenas prácticas de seguridad
- No commitees `.env` (está en `.gitignore`).
- Usa credenciales por entorno (dev/stg/prod) con repos/branches separados o distintos secrets.
- Habilita Flash Encryption y Secure Boot en producción en los dispositivos.

### Referencias
- `src/secrets.cpp`: defaults vacíos; los valores vienen de `.env` o de provisioning.
- `scripts/add_env_defines.py`: extra script que inyecta `CPPDEFINES` desde `.env`/entorno.
- `.github/workflows/ota-update.yml`: workflow de compilación y OTA por MQTT.