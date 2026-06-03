# Inicio Rápido - Compilar y Subir Firmware

## Configuración Inicial

1. Crea un archivo `.env` en la raíz del proyecto con tus variables:
```env
COUNTRY=colombia
STATE=valle
CITY=tulua
MQTT_SERVER=mqtt.tu-servidor.com
MQTT_PORT=8883
MQTT_USER=tu_usuario
MQTT_PASSWORD=tu_contraseña
WIFI_SSID=tu_red
WIFI_PASSWORD=tu_password
```

## Compilar y Subir

**Opción 1: Script automático (Recomendado)**
```bash
python scripts/build_with_env.py upload
```

**Opción 2: Manual**
```bash
set -a && source .env && set +a
pio run -t upload
```

## Solo Compilar (sin subir)

```bash
python scripts/build_with_env.py
# o
set -a && source .env && set +a && pio run
```

## Notas

- El script `build_with_env.py` carga automáticamente las variables del `.env`
- `ROOT_CA` usa el valor por defecto del código (edita `secrets.cpp` si necesitas cambiarlo)
- Las variables se pasan como defines de compilación, no quedan en el código fuente
