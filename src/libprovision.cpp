#include <WiFi.h>
#include <WebServer.h>
#include <libstorage.h>
#include <libprovision.h>

static WebServer server(80);
static bool s_isProvisioning = false;

static const char FORM_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>
<style>body{font-family:sans-serif;margin:24px}input,button{font-size:16px;padding:8px;margin:6px 0;width:100%}form{max-width:420px}</style>
</head><body>
  <h3>Configurar Wi-Fi</h3>
  <form method='POST' action='/save'>
    <label>SSID</label>
    <input name='ssid' required>
    <label>Password</label>
    <input name='password' type='password'>
    <button type='submit'>Guardar</button>
  </form>
</body></html>
)HTML";

static void handleRoot() { server.send(200, "text/html", FORM_HTML); }

static void handleSave() {
  if (!server.hasArg("ssid")) { server.send(400, "text/plain", "ssid requerido"); return; }
  String ssid = server.arg("ssid");
  String pwd  = server.arg("password");
  if (!saveWiFiCredentials(ssid, pwd)) { server.send(500, "text/plain", "no se pudo guardar"); return; }
  server.send(200, "text/plain", "Guardado. Reiniciando...");
  delay(500);
  ESP.restart();
}

void startProvisioningAP() {
  WiFi.mode(WIFI_AP);
  String apName = String("ESP32-Setup-") + String((uint32_t)ESP.getEfuseMac(), HEX);
  WiFi.softAP(apName.c_str());
  IPAddress ip = WiFi.softAPIP();
  Serial.print("Provisioning AP "); Serial.print(apName); Serial.print(" IP "); Serial.println(ip);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  s_isProvisioning = true;
}

void provisioningLoop() {
  if (!s_isProvisioning) return;
  server.handleClient();
}

bool isProvisioning() { return s_isProvisioning; }
