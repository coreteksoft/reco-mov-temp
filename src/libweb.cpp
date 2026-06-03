#include <libweb.h>

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <math.h>
#include <ArduinoJson.h>

#include <libcam.h>

static WebServer g_server(80);
static TelemetryProvider g_provider = nullptr;
static bool g_wifiReady = false;

#define IP_GEO_REFRESH_MS 600000UL
#define IP_GEO_API_URL "http://ip-api.com/json/?fields=status,message,country,regionName,city,lat,lon,query"

struct VirtualGpsState {
  bool enabled;
  bool initialized;
  double baseLat;
  double baseLon;
  double currentLat;
  double currentLon;
  double totalDistanceM;
  float currentSpeedKmh;
  float maxSpeedKmh;
  float hdop;
  uint8_t satellites;
  unsigned long lastUpdateMs;
};

static VirtualGpsState g_gps = {true, false, -12.0464, -77.0428, -12.0464, -77.0428,
                                0.0, 0.0f, 0.0f, 1.2f, 8, 0};

struct IpGeoState {
  bool available;
  bool requested;
  String publicIp;
  String city;
  String region;
  String country;
  double lat;
  double lon;
  unsigned long lastUpdateMs;
};

static IpGeoState g_ipGeo = {false, false, "", "", "", "", 0.0, 0.0, 0};

static bool refreshIpGeo() {
  HTTPClient http;
  http.begin(IP_GEO_API_URL);
  http.setTimeout(6000);
  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<768> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    return false;
  }

  const char *status = doc["status"] | "fail";
  if (String(status) != "success") {
    return false;
  }

  g_ipGeo.publicIp = String(doc["query"] | "");
  g_ipGeo.city = String(doc["city"] | "");
  g_ipGeo.region = String(doc["regionName"] | "");
  g_ipGeo.country = String(doc["country"] | "");
  g_ipGeo.lat = doc["lat"] | 0.0;
  g_ipGeo.lon = doc["lon"] | 0.0;
  g_ipGeo.available = true;
  g_ipGeo.requested = true;
  g_ipGeo.lastUpdateMs = millis();

  Serial.printf("[IP-GEO] %s, %s, %s (%.5f, %.5f)\n",
                g_ipGeo.city.c_str(),
                g_ipGeo.region.c_str(),
                g_ipGeo.country.c_str(),
                g_ipGeo.lat,
                g_ipGeo.lon);

  return true;
}

static double degToRad(double deg) {
  return deg * 3.14159265358979323846 / 180.0;
}

static double haversineMeters(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0;
  double dLat = degToRad(lat2 - lat1);
  double dLon = degToRad(lon2 - lon1);
  double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
             cos(degToRad(lat1)) * cos(degToRad(lat2)) *
             sin(dLon / 2.0) * sin(dLon / 2.0);
  double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return R * c;
}

static void updateVirtualGps(const TelemetrySnapshot &s) {
  if (!g_gps.enabled) {
    return;
  }

  if (!g_gps.initialized) {
    g_gps.currentLat = g_gps.baseLat;
    g_gps.currentLon = g_gps.baseLon;
    g_gps.lastUpdateMs = s.uptimeMs;
    g_gps.initialized = true;
    return;
  }

  unsigned long now = s.uptimeMs;
  float dtSec = static_cast<float>(now - g_gps.lastUpdateMs) / 1000.0f;
  if (dtSec <= 0.05f) {
    return;
  }
  g_gps.lastUpdateMs = now;

  float motionFactor = s.motionDetected ? (0.4f + s.motionLevel / 100.0f) : 0.03f;
  float speedKmh = motionFactor * 5.5f;
  double speedMps = speedKmh / 3.6;
  double distanceM = speedMps * dtSec;

  double t = static_cast<double>(now) / 1000.0;
  double heading = fmod((t * (s.motionDetected ? 0.55 : 0.15)) + 1.2, 6.283185307179586);

  double latMeters = distanceM * cos(heading);
  double lonMeters = distanceM * sin(heading);

  double dLat = latMeters / 111320.0;
  double dLon = lonMeters / (111320.0 * cos(degToRad(g_gps.currentLat)));

  double prevLat = g_gps.currentLat;
  double prevLon = g_gps.currentLon;
  g_gps.currentLat += dLat;
  g_gps.currentLon += dLon;

  double realStepM = haversineMeters(prevLat, prevLon, g_gps.currentLat, g_gps.currentLon);
  g_gps.totalDistanceM += realStepM;

  g_gps.currentSpeedKmh = speedKmh;
  if (speedKmh > g_gps.maxSpeedKmh) {
    g_gps.maxSpeedKmh = speedKmh;
  }

  g_gps.hdop = s.motionDetected ? 0.9f : 1.6f;
  g_gps.satellites = s.motionDetected ? 10 : 8;
}

struct DashboardStats {
  bool initialized;
  unsigned long sampleCount;
  unsigned long sensorSampleCount;
  float tempMin;
  float tempMax;
  float tempSum;
  float humMin;
  float humMax;
  float humSum;
  float motionMax;
  unsigned long motionActiveSamples;
  unsigned long motionEvents;
  bool previousMotionState;
  unsigned long lastMotionUptimeMs;
};

static DashboardStats g_stats = {false, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, false, 0};

static void updateStats(const TelemetrySnapshot &s) {
  g_stats.sampleCount++;

  if (s.sensorReady) {
    g_stats.sensorSampleCount++;
    if (!g_stats.initialized) {
      g_stats.tempMin = s.temperatureC;
      g_stats.tempMax = s.temperatureC;
      g_stats.humMin = s.humidityRH;
      g_stats.humMax = s.humidityRH;
      g_stats.initialized = true;
    } else {
      if (s.temperatureC < g_stats.tempMin) g_stats.tempMin = s.temperatureC;
      if (s.temperatureC > g_stats.tempMax) g_stats.tempMax = s.temperatureC;
      if (s.humidityRH < g_stats.humMin) g_stats.humMin = s.humidityRH;
      if (s.humidityRH > g_stats.humMax) g_stats.humMax = s.humidityRH;
    }
    g_stats.tempSum += s.temperatureC;
    g_stats.humSum += s.humidityRH;
  }

  if (s.motionLevel > g_stats.motionMax) {
    g_stats.motionMax = s.motionLevel;
  }

  if (s.motionDetected) {
    g_stats.motionActiveSamples++;
    g_stats.lastMotionUptimeMs = s.uptimeMs;
  }

  if (s.motionDetected && !g_stats.previousMotionState) {
    g_stats.motionEvents++;
  }
  g_stats.previousMotionState = s.motionDetected;
}

static const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>IoT Monitor ESP32</title>
  <style>
    :root {
      --bg-1: #f1efe7;
      --bg-2: #d7e8f5;
      --card: rgba(255, 255, 255, 0.86);
      --ink: #1f2a37;
      --ok: #117733;
      --warn: #bb2211;
      --accent: #005f8f;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "Segoe UI", "Trebuchet MS", sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at 15% 20%, #ffe6b5 0, rgba(255,230,181,0) 35%),
        radial-gradient(circle at 80% 15%, #b6e3ff 0, rgba(182,227,255,0) 40%),
        linear-gradient(140deg, var(--bg-1), var(--bg-2));
      min-height: 100vh;
      padding: 20px;
    }
    .wrap {
      max-width: 1100px;
      margin: 0 auto;
      display: grid;
      gap: 16px;
      grid-template-columns: 1.3fr 1fr;
    }
    .card {
      background: var(--card);
      border: 1px solid rgba(255,255,255,0.55);
      border-radius: 16px;
      box-shadow: 0 12px 28px rgba(17,35,52,0.18);
      backdrop-filter: blur(6px);
      padding: 16px;
    }
    .full {
      grid-column: 1 / -1;
    }
    h1 {
      margin: 0 0 6px;
      font-size: 1.5rem;
      letter-spacing: 0.4px;
    }
    .sub { margin: 0; opacity: 0.75; font-size: 0.95rem; }
    .camera {
      width: 100%;
      border-radius: 12px;
      border: 1px solid rgba(0,0,0,0.12);
      background: #222;
      display: block;
      min-height: 240px;
      object-fit: cover;
    }
    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      margin-top: 10px;
    }
    .kpi {
      border-radius: 12px;
      padding: 12px;
      background: rgba(255,255,255,0.75);
      border: 1px solid rgba(0,0,0,0.08);
    }
    .kpi span { display: block; opacity: 0.7; font-size: 0.8rem; }
    .kpi strong { font-size: 1.4rem; }
    .state {
      margin-top: 10px;
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
    }
    .pill {
      border-radius: 999px;
      padding: 6px 10px;
      font-size: 0.82rem;
      border: 1px solid rgba(0,0,0,0.15);
      background: rgba(255,255,255,0.75);
    }
    .good { color: var(--ok); border-color: rgba(17,119,51,0.35); }
    .bad  { color: var(--warn); border-color: rgba(187,34,17,0.35); }
    .bar {
      margin-top: 10px;
      width: 100%;
      height: 10px;
      border-radius: 999px;
      background: rgba(0,0,0,0.08);
      overflow: hidden;
    }
    .bar > div {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, #2b90d9, #00b06f);
      transition: width 140ms linear;
    }
    .row { display: flex; justify-content: space-between; margin-top: 6px; font-size: 0.85rem; }
    .stats {
      margin-top: 8px;
      display: grid;
      grid-template-columns: repeat(4, minmax(140px, 1fr));
      gap: 10px;
    }
    .gps {
      margin-top: 8px;
      display: grid;
      grid-template-columns: repeat(3, minmax(140px, 1fr));
      gap: 10px;
    }
    .stat {
      background: rgba(255,255,255,0.75);
      border: 1px solid rgba(0,0,0,0.08);
      border-radius: 12px;
      padding: 10px;
    }
    .stat span {
      display: block;
      font-size: 0.78rem;
      opacity: 0.72;
    }
    .stat strong {
      display: block;
      margin-top: 4px;
      font-size: 1rem;
    }
    @media (max-width: 900px) {
      .wrap { grid-template-columns: 1fr; }
      .stats { grid-template-columns: 1fr 1fr; }
      .gps { grid-template-columns: 1fr 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <section class="card">
      <h1>Camara en vivo</h1>
      <p class="sub">Actualizacion por snapshots JPEG continuos (casi en tiempo real).</p>
      <img id="cam" class="camera" alt="Camara ESP32" src="/camera.jpg" />
    </section>
    <section class="card">
      <h1>Telemetria</h1>
      <p id="ip" class="sub">Conectando...</p>

      <div class="grid">
        <div class="kpi"><span>Temperatura</span><strong id="temp">--.- C</strong></div>
        <div class="kpi"><span>Humedad</span><strong id="hum">--.- %</strong></div>
        <div class="kpi"><span>Movimiento</span><strong id="mov">--</strong></div>
        <div class="kpi"><span>Uptime</span><strong id="upt">--</strong></div>
      </div>

      <div class="row"><span>Nivel de movimiento</span><span id="lvlTxt">0.0%</span></div>
      <div class="bar"><div id="lvlBar"></div></div>

      <div class="state">
        <span id="sSensor" class="pill">Sensor</span>
        <span id="sCam" class="pill">Camara</span>
      </div>
    </section>

    <section class="card full">
      <h1>Estadisticas de registro</h1>
      <p class="sub">Acumulados desde el ultimo reinicio del dispositivo.</p>
      <div class="stats">
        <div class="stat"><span>Temperatura promedio</span><strong id="stTempAvg">--.- C</strong></div>
        <div class="stat"><span>Temperatura min / max</span><strong id="stTempRange">-- / --</strong></div>
        <div class="stat"><span>Humedad promedio</span><strong id="stHumAvg">--.- %</strong></div>
        <div class="stat"><span>Humedad min / max</span><strong id="stHumRange">-- / --</strong></div>
        <div class="stat"><span>Eventos de movimiento</span><strong id="stEvents">0</strong></div>
        <div class="stat"><span>Actividad de movimiento</span><strong id="stDuty">0.0 %</strong></div>
        <div class="stat"><span>Pico de movimiento</span><strong id="stMotionPeak">0.0 %</strong></div>
        <div class="stat"><span>Ultimo movimiento</span><strong id="stLastMove">N/A</strong></div>
      </div>
    </section>

    <section class="card full">
      <h1>GPS virtual (sin hardware)</h1>
      <p class="sub">Ubicacion simulada para demo y presentaciones sin modulo GPS fisico.</p>
      <div class="gps">
        <div class="stat"><span>Modo</span><strong id="gpsMode">Simulado</strong></div>
        <div class="stat"><span>Latitud</span><strong id="gpsLat">--</strong></div>
        <div class="stat"><span>Longitud</span><strong id="gpsLon">--</strong></div>
        <div class="stat"><span>Velocidad</span><strong id="gpsSpeed">0.0 km/h</strong></div>
        <div class="stat"><span>Distancia total</span><strong id="gpsDist">0.0 m</strong></div>
        <div class="stat"><span>Precision (HDOP)</span><strong id="gpsHdop">--</strong></div>
      </div>
      <div class="row"><span>Satelites virtuales</span><span id="gpsSats">--</span></div>
      <div class="row"><span>Mapa</span><a id="gpsMap" href="#" target="_blank">Abrir en OpenStreetMap</a></div>
    </section>

    <section class="card full">
      <h1>Ubicacion estimada por IP de red</h1>
      <p class="sub">Estimacion basada en IP publica de la red donde se instala la ESP.</p>
      <div class="gps">
        <div class="stat"><span>Estado</span><strong id="ipgState">Pendiente</strong></div>
        <div class="stat"><span>IP publica</span><strong id="ipgIp">--</strong></div>
        <div class="stat"><span>Ciudad / Region</span><strong id="ipgPlace">--</strong></div>
        <div class="stat"><span>Pais</span><strong id="ipgCountry">--</strong></div>
        <div class="stat"><span>Latitud</span><strong id="ipgLat">--</strong></div>
        <div class="stat"><span>Longitud</span><strong id="ipgLon">--</strong></div>
      </div>
      <div class="row"><span>Mapa estimado</span><a id="ipgMap" href="#" target="_blank">Abrir ubicacion estimada</a></div>
    </section>
  </div>

  <script>
    const cam = document.getElementById('cam');
    const ip = document.getElementById('ip');
    const temp = document.getElementById('temp');
    const hum = document.getElementById('hum');
    const mov = document.getElementById('mov');
    const upt = document.getElementById('upt');
    const lvlTxt = document.getElementById('lvlTxt');
    const lvlBar = document.getElementById('lvlBar');
    const sSensor = document.getElementById('sSensor');
    const sCam = document.getElementById('sCam');
    const stTempAvg = document.getElementById('stTempAvg');
    const stTempRange = document.getElementById('stTempRange');
    const stHumAvg = document.getElementById('stHumAvg');
    const stHumRange = document.getElementById('stHumRange');
    const stEvents = document.getElementById('stEvents');
    const stDuty = document.getElementById('stDuty');
    const stMotionPeak = document.getElementById('stMotionPeak');
    const stLastMove = document.getElementById('stLastMove');
    const gpsMode = document.getElementById('gpsMode');
    const gpsLat = document.getElementById('gpsLat');
    const gpsLon = document.getElementById('gpsLon');
    const gpsSpeed = document.getElementById('gpsSpeed');
    const gpsDist = document.getElementById('gpsDist');
    const gpsHdop = document.getElementById('gpsHdop');
    const gpsSats = document.getElementById('gpsSats');
    const gpsMap = document.getElementById('gpsMap');
    const ipgState = document.getElementById('ipgState');
    const ipgIp = document.getElementById('ipgIp');
    const ipgPlace = document.getElementById('ipgPlace');
    const ipgCountry = document.getElementById('ipgCountry');
    const ipgLat = document.getElementById('ipgLat');
    const ipgLon = document.getElementById('ipgLon');
    const ipgMap = document.getElementById('ipgMap');

    function fmtUptime(ms) {
      const s = Math.floor(ms / 1000);
      const h = Math.floor(s / 3600);
      const m = Math.floor((s % 3600) / 60);
      const r = s % 60;
      return `${h}h ${m}m ${r}s`;
    }

    async function loadTelemetry() {
      try {
        const r = await fetch('/api/telemetry', { cache: 'no-store' });
        const d = await r.json();
        ip.textContent = `IP: ${location.host}`;
        temp.textContent = d.sensorReady ? `${d.temperatureC.toFixed(1)} C` : '--.- C';
        hum.textContent = d.sensorReady ? `${d.humidityRH.toFixed(1)} %` : '--.- %';
        mov.textContent = d.motionDetected ? 'ACTIVO' : 'QUIETO';
        upt.textContent = fmtUptime(d.uptimeMs || 0);
        lvlTxt.textContent = `${d.motionLevel.toFixed(1)}%`;
        lvlBar.style.width = `${Math.max(0, Math.min(100, d.motionLevel))}%`;

        sSensor.textContent = d.sensorReady ? 'Sensor OK' : 'Sensor FAIL';
        sSensor.className = `pill ${d.sensorReady ? 'good' : 'bad'}`;
        sCam.textContent = d.cameraReady ? 'Camara OK' : 'Camara FAIL';
        sCam.className = `pill ${d.cameraReady ? 'good' : 'bad'}`;
      } catch (_e) {
        ip.textContent = 'Sin conexion con ESP32';
      }
    }

    async function loadStats() {
      try {
        const r = await fetch('/api/stats', { cache: 'no-store' });
        const s = await r.json();

        stTempAvg.textContent = s.sensorReady ? `${s.tempAvg.toFixed(1)} C` : '--.- C';
        stTempRange.textContent = s.sensorReady
          ? `${s.tempMin.toFixed(1)} / ${s.tempMax.toFixed(1)} C`
          : '-- / --';
        stHumAvg.textContent = s.sensorReady ? `${s.humAvg.toFixed(1)} %` : '--.- %';
        stHumRange.textContent = s.sensorReady
          ? `${s.humMin.toFixed(1)} / ${s.humMax.toFixed(1)} %`
          : '-- / --';

        stEvents.textContent = `${s.motionEvents}`;
        stDuty.textContent = `${s.motionDutyCycle.toFixed(1)} %`;
        stMotionPeak.textContent = `${s.motionMax.toFixed(1)} %`;
        stLastMove.textContent = s.lastMotionUptimeMs > 0 ? fmtUptime(s.lastMotionUptimeMs) : 'N/A';
      } catch (_e) {
        stTempAvg.textContent = '--.- C';
      }
    }

    async function loadGps() {
      try {
        const r = await fetch('/api/gps', { cache: 'no-store' });
        const g = await r.json();
        gpsMode.textContent = g.simulated ? 'Simulado' : 'Deshabilitado';
        gpsLat.textContent = g.latitude.toFixed(6);
        gpsLon.textContent = g.longitude.toFixed(6);
        gpsSpeed.textContent = `${g.speedKmh.toFixed(1)} km/h`;
        gpsDist.textContent = `${g.distanceM.toFixed(1)} m`;
        gpsHdop.textContent = g.hdop.toFixed(1);
        gpsSats.textContent = `${g.satellites}`;
        gpsMap.href = `https://www.openstreetmap.org/?mlat=${g.latitude}&mlon=${g.longitude}#map=17/${g.latitude}/${g.longitude}`;
      } catch (_e) {
        gpsMode.textContent = 'No disponible';
      }
    }

    async function loadIpGeo() {
      try {
        const r = await fetch('/api/ip-location', { cache: 'no-store' });
        const g = await r.json();

        if (!g.available) {
          ipgState.textContent = 'Sin dato';
          return;
        }

        ipgState.textContent = 'Estimado';
        ipgIp.textContent = g.publicIp || '--';
        ipgPlace.textContent = `${g.city || '-'} / ${g.region || '-'}`;
        ipgCountry.textContent = g.country || '--';
        ipgLat.textContent = Number(g.latitude).toFixed(6);
        ipgLon.textContent = Number(g.longitude).toFixed(6);
        ipgMap.href = `https://www.openstreetmap.org/?mlat=${g.latitude}&mlon=${g.longitude}#map=12/${g.latitude}/${g.longitude}`;
      } catch (_e) {
        ipgState.textContent = 'Error';
      }
    }

    function refreshCam() {
      cam.src = `/camera.jpg?t=${Date.now()}`;
    }

    setInterval(loadTelemetry, 1000);
    setInterval(loadStats, 2000);
    setInterval(loadGps, 1200);
    setInterval(loadIpGeo, 10000);
    setInterval(refreshCam, 250);
    loadTelemetry();
    loadStats();
    loadGps();
    loadIpGeo();
    refreshCam();
  </script>
</body>
</html>
)HTML";

static void sendDashboard() {
  g_server.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
}

static void sendTelemetry() {
  if (!g_provider) {
    g_server.send(503, "application/json", "{\"error\":\"provider-unavailable\"}");
    return;
  }

  TelemetrySnapshot s = g_provider();
  String payload = "{";
  payload += "\"temperatureC\":" + String(s.temperatureC, 2) + ",";
  payload += "\"humidityRH\":" + String(s.humidityRH, 2) + ",";
  payload += "\"sensorReady\":" + String(s.sensorReady ? "true" : "false") + ",";
  payload += "\"cameraReady\":" + String(s.cameraReady ? "true" : "false") + ",";
  payload += "\"motionDetected\":" + String(s.motionDetected ? "true" : "false") + ",";
  payload += "\"motionLevel\":" + String(s.motionLevel, 2) + ",";
  payload += "\"uptimeMs\":" + String(s.uptimeMs);
  payload += "}";

  g_server.send(200, "application/json", payload);
}

static void sendStats() {
  if (!g_provider) {
    g_server.send(503, "application/json", "{\"error\":\"provider-unavailable\"}");
    return;
  }

  float tempAvg = 0.0f;
  float humAvg = 0.0f;
  if (g_stats.initialized && g_stats.sensorSampleCount > 0) {
    tempAvg = g_stats.tempSum / static_cast<float>(g_stats.sensorSampleCount);
    humAvg = g_stats.humSum / static_cast<float>(g_stats.sensorSampleCount);
  }

  float duty = 0.0f;
  if (g_stats.sampleCount > 0) {
    duty = 100.0f * static_cast<float>(g_stats.motionActiveSamples) /
           static_cast<float>(g_stats.sampleCount);
  }

  String payload = "{";
  payload += "\"sensorReady\":" + String(g_stats.initialized ? "true" : "false") + ",";
  payload += "\"sampleCount\":" + String(g_stats.sampleCount) + ",";
  payload += "\"sensorSampleCount\":" + String(g_stats.sensorSampleCount) + ",";
  payload += "\"tempAvg\":" + String(tempAvg, 2) + ",";
  payload += "\"tempMin\":" + String(g_stats.tempMin, 2) + ",";
  payload += "\"tempMax\":" + String(g_stats.tempMax, 2) + ",";
  payload += "\"humAvg\":" + String(humAvg, 2) + ",";
  payload += "\"humMin\":" + String(g_stats.humMin, 2) + ",";
  payload += "\"humMax\":" + String(g_stats.humMax, 2) + ",";
  payload += "\"motionEvents\":" + String(g_stats.motionEvents) + ",";
  payload += "\"motionDutyCycle\":" + String(duty, 2) + ",";
  payload += "\"motionMax\":" + String(g_stats.motionMax, 2) + ",";
  payload += "\"lastMotionUptimeMs\":" + String(g_stats.lastMotionUptimeMs);
  payload += "}";

  g_server.send(200, "application/json", payload);
}

static void sendGps() {
  String payload = "{";
  payload += "\"simulated\":" + String(g_gps.enabled ? "true" : "false") + ",";
  payload += "\"latitude\":" + String(g_gps.currentLat, 6) + ",";
  payload += "\"longitude\":" + String(g_gps.currentLon, 6) + ",";
  payload += "\"speedKmh\":" + String(g_gps.currentSpeedKmh, 2) + ",";
  payload += "\"maxSpeedKmh\":" + String(g_gps.maxSpeedKmh, 2) + ",";
  payload += "\"distanceM\":" + String(g_gps.totalDistanceM, 2) + ",";
  payload += "\"hdop\":" + String(g_gps.hdop, 1) + ",";
  payload += "\"satellites\":" + String(g_gps.satellites);
  payload += "}";
  g_server.send(200, "application/json", payload);
}

static void sendIpLocation() {
  String payload = "{";
  payload += "\"available\":" + String(g_ipGeo.available ? "true" : "false") + ",";
  payload += "\"publicIp\":\"" + g_ipGeo.publicIp + "\",";
  payload += "\"city\":\"" + g_ipGeo.city + "\",";
  payload += "\"region\":\"" + g_ipGeo.region + "\",";
  payload += "\"country\":\"" + g_ipGeo.country + "\",";
  payload += "\"latitude\":" + String(g_ipGeo.lat, 6) + ",";
  payload += "\"longitude\":" + String(g_ipGeo.lon, 6) + ",";
  payload += "\"lastUpdateMs\":" + String(g_ipGeo.lastUpdateMs);
  payload += "}";
  g_server.send(200, "application/json", payload);
}

static void sendCameraSnapshot() {
  uint8_t *jpg = nullptr;
  size_t len = 0;
  if (!captureJpegFrame(&jpg, &len) || !jpg || len == 0) {
    g_server.send(503, "text/plain", "camera-unavailable");
    return;
  }

  g_server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  g_server.setContentLength(len);
  g_server.send(200, "image/jpeg", "");
  WiFiClient client = g_server.client();
  client.write(jpg, len);
  free(jpg);
}

bool initWebDashboard(const char *ssid, const char *password, TelemetryProvider provider,
                      double gpsBaseLat, double gpsBaseLon,
                      bool gpsSimulationEnabled) {
  g_provider = provider;
  g_gps.enabled = gpsSimulationEnabled;
  g_gps.baseLat = gpsBaseLat;
  g_gps.baseLon = gpsBaseLon;
  g_gps.currentLat = gpsBaseLat;
  g_gps.currentLon = gpsBaseLon;
  g_gps.initialized = false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.printf("[WEB] Conectando a WiFi %s", ssid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000UL) {
    delay(400);
    Serial.print('.');
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WEB] No se pudo conectar a WiFi");
    g_wifiReady = false;
    return false;
  }

  Serial.printf("\n[WEB] WiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());

  if (!refreshIpGeo()) {
    Serial.println("[IP-GEO] No se pudo obtener ubicacion por IP en arranque");
  }

  g_server.on("/", HTTP_GET, sendDashboard);
  g_server.on("/api/telemetry", HTTP_GET, sendTelemetry);
  g_server.on("/api/stats", HTTP_GET, sendStats);
  g_server.on("/api/gps", HTTP_GET, sendGps);
  g_server.on("/api/ip-location", HTTP_GET, sendIpLocation);
  g_server.on("/camera.jpg", HTTP_GET, sendCameraSnapshot);
  g_server.begin();

  g_wifiReady = true;
  Serial.println("[WEB] Dashboard listo");
  return true;
}

void handleWebDashboard() {
  if (!g_wifiReady) {
    return;
  }

  if (g_ipGeo.available && (millis() - g_ipGeo.lastUpdateMs) > IP_GEO_REFRESH_MS) {
    refreshIpGeo();
  } else if (!g_ipGeo.requested) {
    refreshIpGeo();
  }

  if (g_provider) {
    TelemetrySnapshot s = g_provider();
    updateStats(s);
    updateVirtualGps(s);
  }
  g_server.handleClient();
}

String getDashboardUrl() {
  if (!g_wifiReady) {
    return String("No disponible");
  }
  return String("http://") + WiFi.localIP().toString();
}
