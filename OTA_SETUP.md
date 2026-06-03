## Cómo disparar una actualización OTA desde GitHub

Estas instrucciones envían una OTA a los dispositivos suscritos al tópico `dispositivo/device1/ota` cuando publicas un tag de versión.

### Requisitos previos
- Secrets configurados en el repositorio (en Settings → Secrets and variables → Actions):
  - `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`, `AWS_REGION`, `S3_BUCKET_NAME`
  - `MQTT_SERVER`, `MQTT_PORT`, `MQTT_USER` (opcional), `MQTT_PASSWORD` (opcional), `MQTT_TLS` (`true`/`false`)
- Los dispositivos ya conectados a Wi‑Fi/MQTT y escuchando `dispositivo/device1/ota`.

### Qué dispara la OTA
- El flujo `.github/workflows/ota-update.yml` se ejecuta cuando:
  - Haces push a `main` (compila), y/o
  - Publicas un tag con formato `vX.Y.Z` (compila, nombra el binario con esa versión y envía mensaje OTA por MQTT).

### Pasos cuando tengas código nuevo
1) Asegúrate de haber hecho commit y push de tus cambios a `main`:
```bash
git add .
git commit -m "feat: cambio en sensor X"
git push origin main
```
2) Crea un tag de versión (sigue el formato `vX.Y.Z`, por ejemplo `v1.2.0`):
```bash
git tag v1.2.0
git push origin v1.2.0
```

### ¿Qué ocurre en GitHub Actions?
- Compila el firmware con PlatformIO.
- Sube el binario a S3 con nombre `firmware_v1.2.0.bin`.
- Publica un mensaje MQTT al tópico `dispositivo/device1/ota` con el payload:
```json
{"version":"v1.2.0","url":"https://<bucket>.s3.<region>.amazonaws.com/firmware_v1.2.0.bin"}
```
- Los dispositivos reciben el mensaje y se actualizan.

### Verificar que todo salió bien
- En GitHub → Actions → “OTA Update to ESP32 via MQTT TLS”: revisa el job del tag.
- Debe mostrar “✅ Mensaje OTA enviado”.
- En el dispositivo: verás logs de descarga y reinicio; la nueva versión se muestra en el serial/pantalla si está implementado.

### Consejos
- Usa una nueva versión cada vez (`v1.2.1`, `v1.2.2`, …). No reutilices tags.
- Si falla, revisa:
  - Secrets ausentes o incorrectos.
  - `MQTT_SERVER/MQTT_PORT` accesibles desde GitHub Actions.
  - El dispositivo conectado al broker y suscrito a `dispositivo/device1/ota`.
  - Que el binario sea accesible en S3 (bucket/region correctos).
