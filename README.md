# IoT ESP32-S3-CAM - Monitoreo de Movimiento + Ambiente

Proyecto actualizado para usar el hardware de tus fotos y construir un sistema local que:

- detecta movimiento con la camara OV2640,
- mide temperatura y humedad con SHT3X,
- muestra todo en una OLED SSD1306,
- expone un dashboard web con camara y telemetria casi en tiempo real.

## Hardware usado

- ESP32-S3-CAM GOOUUU (OV2640)
- Pantalla OLED SSD1306 I2C 0.96" (GND, VDD, SCL/SCK, SDA)
- Sensor SHT3X I2C (SCL, SDA, VCC, GND)
- Protoboard y cables dupont
- Cable USB-C conectado al puerto TTL de la placa

## Arquitectura del sistema

```text
OV2640 (grayscale) -> deteccion por diferencia de frames -> estado de movimiento
SHT3X (I2C) -> temperatura/humedad

Movimiento + Temp/Hum -> OLED SSD1306 (128x64)
                     -> Monitor serie (115200)
                     -> Dashboard Web (ESP32, HTTP)
```

## Cableado paso a paso

El bus I2C se comparte entre OLED y SHT3X, por eso ambos van a los mismos pines SDA/SCL del ESP32.

### 1) Alimentacion comun

- ESP32 3V3 -> OLED VDD
- ESP32 3V3 -> SHT3X VCC
- ESP32 GND -> OLED GND
- ESP32 GND -> SHT3X GND

### 2) Lineas I2C

- ESP32 G21 (SDA) -> OLED SDA
- ESP32 G20 (SCL) -> OLED SCL o SCK (depende del rotulo del modulo)
- ESP32 G21 (SDA) -> SHT3X SDA
- ESP32 G20 (SCL) -> SHT3X SCL

### 3) Recomendaciones importantes

- Usa el puerto USB-C TTL para programar y monitor serie.
- No uses GPIO8/GPIO9 para I2C: esos pines participan en la interfaz de camara.
- En el SHT3X verifica el orden exacto en la serigrafia del modulo antes de energizar.

## Pantallas y comportamiento

La OLED muestra:

- Temp: temperatura en C
- Hum: humedad relativa en %
- Mov: ACTIVO o QUIETO
- Nivel: porcentaje de cambio entre frames

Indicadores de error:

- CAM! en esquina superior: no se pudo iniciar la camara
- SHT! en esquina inferior: no se detecto o no responde el sensor

## Estructura de firmware relevante

- src/main.cpp
  - Inicializa I2C, OLED, SHT3X y camara
  - Lee SHT3X cada 2 s
  - Actualiza OLED en bucle
- src/libcam.cpp
  - Captura frames en escala de grises
  - Detecta movimiento por diferencia con frame previo
  - Entrega snapshots JPEG para la web
- src/libdisplay.cpp
  - Renderiza la pantalla de monitoreo
- src/libcam.h
  - Pines de camara y parametros de sensibilidad
- src/libweb.cpp
  - Conecta a WiFi
  - Sirve dashboard web
  - Endpoint de telemetria JSON y endpoint de camara JPEG

## Configuracion WiFi para dashboard

Edita estas lineas en src/main.cpp:

```cpp
#define WIFI_SSID     "TU_RED_WIFI"
#define WIFI_PASSWORD "TU_PASSWORD_WIFI"
```

Cuando el equipo arranca correctamente, el monitor serie muestra:

```text
[WEB] WiFi OK. IP: 192.168.x.x
[WEB] Dashboard: http://192.168.x.x
```

Abre esa URL en tu navegador (PC o celular en la misma red).

## Endpoints del dashboard

- `/` Dashboard HTML
- `/api/telemetry` JSON con temperatura, humedad, movimiento, estados y uptime
- `/api/stats` JSON con estadisticas acumuladas de registro
- `/api/gps` JSON con GPS virtual (latitud, longitud, velocidad, distancia, precision)
- `/api/ip-location` JSON con ubicacion estimada por IP publica de la red
- `/camera.jpg` snapshot JPEG actualizado continuamente por el frontend

## Ubicacion estimada por IP de red

Si quieres aproximar donde esta instalada la ESP sin GPS fisico, el sistema ahora consulta una API de geolocalizacion por IP publica.

Como funciona:

- la ESP obtiene la IP publica de salida de la red,
- consulta ciudad/region/pais y coordenadas estimadas,
- publica estos datos en `/api/ip-location`,
- el dashboard muestra esta ubicacion y un enlace a mapa.

Limitaciones importantes:

- es una ubicacion estimada (normalmente a nivel ciudad o zona),
- puede reflejar la ubicacion del ISP y no la direccion exacta,
- si hay VPN o CGNAT, la precision puede bajar.

Aun asi, para demo, reporte o trazabilidad general suele ser suficiente.

## GPS sin hardware fisico

Si no tienes modulo GPS, el proyecto ahora incluye un GPS virtual para demo.

Que hace:

- genera coordenadas simuladas a partir de una posicion base,
- ajusta movimiento virtual usando la actividad detectada por camara,
- calcula velocidad y distancia acumulada,
- muestra un enlace para abrir la posicion en OpenStreetMap.

Configuracion en src/main.cpp:

```cpp
#define GPS_SIM_ENABLED true
#define GPS_BASE_LAT    -12.0464
#define GPS_BASE_LON    -77.0428
```

Puedes poner las coordenadas de tu ciudad o de la zona donde haras la demostracion.

## Apartado de estadisticas

El dashboard ahora incluye un panel de estadisticas acumuladas desde el ultimo reinicio del ESP32.

Metricas incluidas:

- temperatura promedio
- temperatura minima y maxima
- humedad promedio
- humedad minima y maxima
- cantidad de eventos de movimiento
- porcentaje de actividad de movimiento
- pico maximo de nivel de movimiento
- tiempo de uptime del ultimo movimiento detectado

Esto te permite presentar tendencias y resumen historico, no solo el valor instantaneo.

## Configuracion de sensibilidad de movimiento

En src/libcam.h puedes ajustar:

- CAM_CAPTURE_INTERVAL: periodo de captura de frames (ms)
- MOTION_PIXEL_DIFF_THRESHOLD: diferencia minima por pixel para contar cambio
- MOTION_MIN_CHANGED_SAMPLES: cantidad minima de muestras cambiadas para marcar ACTIVO
- MOTION_SAMPLE_STRIDE: submuestreo para reducir carga de CPU

Si detecta mucho ruido:

- sube MOTION_PIXEL_DIFF_THRESHOLD,
- sube MOTION_MIN_CHANGED_SAMPLES.

Si detecta poco movimiento:

- baja MOTION_PIXEL_DIFF_THRESHOLD,
- baja MOTION_MIN_CHANGED_SAMPLES.

## Compilar y cargar

Con PlatformIO (VS Code o CLI):

```bash
pio run -t upload
```

Si no tienes pio global, usa el boton Upload de PlatformIO en VS Code.

## Prueba funcional paso a paso

### Prueba 1 - arranque base

1. Conecta la placa por USB-C TTL.
2. Carga firmware.
3. Abre monitor serie a 115200.
4. Verifica mensajes de inicio:
   - [SHT3X] Sensor detectado en 0x44 o 0x45
   - [CAM] Camara OV2640 iniciada (QQVGA, grayscale)
  - [WEB] Dashboard: http://IP_DE_TU_ESP32

### Prueba 2 - lectura ambiental

1. Deja el sensor en reposo unos segundos.
2. Verifica en serie lineas como:
   - [SHT3X] Temp: xx.xx C | Hum: yy.yy %
3. Verifica en OLED que Temp/Hum cambien.
4. Verifica en dashboard que Temp/Hum se actualicen cada segundo.

### Prueba 3 - deteccion de movimiento

1. Coloca la camara apuntando a una escena estable.
2. Observa Mov: QUIETO.
3. Mueve la mano frente a la camara.
4. Debe cambiar a Mov: ACTIVO y subir Nivel.
5. Verifica que el dashboard muestre el cambio en el estado y barra de nivel.

### Prueba 4 - vista de camara web

1. Abre el dashboard en el navegador.
2. Confirma que la imagen de la camara se refresca automaticamente.
3. Mueve objetos frente a la camara y confirma cambio visual en pantalla web.

### Prueba 5 - estadisticas del dashboard

1. Deja correr el dispositivo al menos 1-2 minutos.
2. Revisa el bloque "Estadisticas de registro" en el dashboard.
3. Verifica que min/max/promedios cambien con el ambiente.
4. Genera movimiento varias veces y confirma aumento en "Eventos de movimiento".
5. Comprueba que el "Pico de movimiento" guarda el valor mas alto alcanzado.

### Prueba 6 - GPS virtual

1. Verifica en dashboard el bloque "GPS virtual (sin hardware)".
2. Confirma que aparecen latitud/longitud, velocidad y distancia.
3. Genera movimiento delante de la camara y revisa cambios de velocidad/distancia.
4. Haz click en "Abrir en OpenStreetMap" para ver el punto en mapa.

### Prueba 7 - ubicacion por IP de red

1. Espera 10-20 segundos despues del arranque.
2. Revisa el bloque "Ubicacion estimada por IP de red" en el dashboard.
3. Verifica que aparezcan IP publica, ciudad/region/pais y coordenadas.
4. Abre el enlace de mapa para validar la zona estimada.

### Prueba 8 - robustez minima

1. Tapa parcialmente la camara y retira la mano.
2. Comprueba que vuelve a QUIETO en pocos ciclos.
3. Desconecta solo SHT3X y reinicia: debe aparecer SHT!.
4. Si WiFi falla, el firmware sigue mostrando datos en OLED (modo local).

## Troubleshooting rapido

- OLED en blanco:
  - revisa direccion (0x3C/0x3D ya se prueba automaticamente),
  - revisa GND/VDD y orden SDA/SCL.
- SHT3X no detectado:
  - revisa serigrafia de pines,
  - confirma 3V3 (no 5V),
  - prueba cable corto y GND comun.
- Dashboard no abre:
  - confirma SSID/PASSWORD en src/main.cpp,
  - revisa que celular/PC y ESP32 esten en la misma red,
  - usa la IP exacta mostrada en el monitor serie.
- GPS virtual no aparece:
  - confirma `GPS_SIM_ENABLED true` en src/main.cpp,
  - revisa endpoint `/api/gps` en el navegador,
  - recarga la pagina del dashboard.
- Ubicacion por IP vacia:
  - confirma salida a internet desde la red donde esta la ESP,
  - prueba endpoint `/api/ip-location`,
  - espera el siguiente refresco (se actualiza periodicamente).
- Camara no actualiza en dashboard:
  - reinicia la placa y vuelve a abrir `/camera.jpg`,
  - reduce ruido de iluminacion,
  - verifica que la camara haya iniciado sin error en serie.
- Movimiento siempre ACTIVO:
  - reduce vibracion de la placa,
  - mejora iluminacion,
  - aumenta umbrales en src/libcam.h.
- Movimiento nunca ACTIVO:
  - baja umbrales en src/libcam.h,
  - verifica que la camara este bien enfocada y con luz.

## Nota sobre el modulo microSD de la foto

El adaptador microSD que muestras es util para una siguiente fase (registro historico local). Esta version actual no escribe a SD, pero la base ya queda lista para extenderlo con almacenamiento de eventos.
