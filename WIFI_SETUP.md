## Configurar el Wi‑Fi del dispositivo

### Requisitos
- Un celular o laptop con Wi‑Fi
- El dispositivo encendido y cerca

### Paso 1: Entrar al modo de configuración (si hace falta)
- Si el dispositivo es nuevo o fue restablecido, creará su propia red Wi‑Fi automáticamente.
- Si ya estaba configurado y quieres cambiar la red:
  - Apaga el dispositivo.
  - Mantén presionado el botón BOOT (GPIO0) mientras lo enciendes durante al menos 3 segundos.
  - Suelta el botón: el equipo entra al modo de configuración.

### Paso 2: Conectarte al punto de acceso del dispositivo
- En tu celular/PC, abre la lista de redes Wi‑Fi.
- Busca una red con nombre parecido a: `ESP32-Setup-XXXXXX`
- Conéctate a esa red. (No requiere clave a menos que se indique lo contrario.)

### Paso 3: Abrir el portal de configuración
- Abre el navegador e ingresa la dirección: `http://192.168.4.1`
- Si no abre a la primera, desactiva los datos móviles y vuelve a intentar.

### Paso 4: Introducir tu red y contraseña
- Verás un formulario simple.
- Completa:
  - SSID: nombre de tu red Wi‑Fi
  - Password: contraseña de tu red
- Pulsa “Guardar”.
- El dispositivo se reiniciará automáticamente.

### Paso 5: Verificar la conexión
- Tras reiniciar, el dispositivo intentará conectarse a la red que ingresaste.
- Si tiene pantalla, mostrará el SSID y la IP.
- Si no, verifique en el monitor serie o en el servidor.

### Reconfigurar más adelante
- Si necesitas cambiar la red:
  - Apaga el equipo.
  - Presiona y mantén BOOT (GPIO0) por 3+ segundos al encender.
  - Repite los pasos desde el Paso 2.

### Consejos si algo falla
- Asegúrate de escribir correctamente el SSID y la contraseña (mayúsculas y minúsculas importan).
- Acércate al router.
- Intenta otra vez el portal en `http://192.168.4.1`.
- Desactiva datos móviles si estás en celular.
- Si el AP del dispositivo no aparece, apágalo y enciéndelo de nuevo (o restablece con BOOT 3s).

### Privacidad
- El SSID y la contraseña se guardan solo dentro del dispositivo y no se envían a internet.
