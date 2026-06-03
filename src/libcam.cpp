/*
 * libcam.cpp — Implementacion de captura OV2640 y deteccion de movimiento.
 */

#include <libcam.h>
#include <esp_camera.h>
#include <img_converters.h>
#include <stdlib.h>
#include <string.h>

bool motionDetected = false;
float motionLevel = 0.0f;

static unsigned long lastCaptureTime = 0;
static bool cameraReady = false;
static uint8_t *previousFrame = nullptr;
static size_t previousFrameLen = 0;

// ─────────────────────────────────────────────────────────────
//  initCamera()
// ─────────────────────────────────────────────────────────────
bool initCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0    = CAM_PIN_D0;
  config.pin_d1    = CAM_PIN_D1;
  config.pin_d2    = CAM_PIN_D2;
  config.pin_d3    = CAM_PIN_D3;
  config.pin_d4    = CAM_PIN_D4;
  config.pin_d5    = CAM_PIN_D5;
  config.pin_d6    = CAM_PIN_D6;
  config.pin_d7    = CAM_PIN_D7;
  config.pin_xclk  = CAM_PIN_XCLK;
  config.pin_pclk  = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href  = CAM_PIN_HREF;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn  = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;

  config.xclk_freq_hz = 20000000;   // 20 MHz
  config.pixel_format = PIXFORMAT_GRAYSCALE;

  // QQVGA (160x120) en DRAM para procesar movimiento localmente
  config.frame_size   = FRAMESIZE_QQVGA;
  config.jpeg_quality = 12;
  config.fb_count     = 1;
  config.fb_location  = CAMERA_FB_IN_DRAM;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Error al iniciar cámara: 0x%x\n", err);
    Serial.println("[CAM] Revisa el cableado de los pines GPIO en libcam.h");
    cameraReady = false;
    return false;
  }

  // Ajustes para un analisis mas estable en interiores
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);
  }

  Serial.println("[CAM] Camara OV2640 iniciada (QQVGA, grayscale)");
  cameraReady = true;
  return true;
}

void updateMotionDetection() {
  if (!cameraReady) return;

  if ((millis() - lastCaptureTime) < CAM_CAPTURE_INTERVAL) return;
  lastCaptureTime = millis();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAM] Error al capturar frame");
    return;
  }

  if (previousFrame && previousFrameLen == fb->len) {
    size_t changedSamples = 0;
    size_t sampled = 0;

    for (size_t i = 0; i < fb->len; i += MOTION_SAMPLE_STRIDE) {
      int diff = abs(static_cast<int>(fb->buf[i]) - static_cast<int>(previousFrame[i]));
      sampled++;
      if (diff >= MOTION_PIXEL_DIFF_THRESHOLD) {
        changedSamples++;
      }
    }

    if (sampled > 0) {
      motionLevel = (100.0f * static_cast<float>(changedSamples)) / static_cast<float>(sampled);
    } else {
      motionLevel = 0.0f;
    }

    motionDetected = (changedSamples >= MOTION_MIN_CHANGED_SAMPLES);
    Serial.printf("[CAM] Motion: %s | nivel=%.2f%% | muestras=%u\n",
                  motionDetected ? "ACTIVO" : "QUIETO",
                  motionLevel,
                  static_cast<unsigned int>(changedSamples));
  }

  if (!previousFrame || previousFrameLen != fb->len) {
    if (previousFrame) {
      free(previousFrame);
      previousFrame = nullptr;
      previousFrameLen = 0;
    }

    previousFrame = static_cast<uint8_t *>(malloc(fb->len));
    if (!previousFrame) {
      Serial.println("[CAM] Sin memoria para frame de referencia");
      esp_camera_fb_return(fb);
      return;
    }
    previousFrameLen = fb->len;
  }

  memcpy(previousFrame, fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

bool captureJpegFrame(uint8_t **jpgBuf, size_t *jpgLen) {
  if (!jpgBuf || !jpgLen) {
    return false;
  }

  *jpgBuf = nullptr;
  *jpgLen = 0;

  if (!cameraReady) {
    return false;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    return false;
  }

  bool ok = false;
  if (fb->format == PIXFORMAT_JPEG) {
    *jpgBuf = static_cast<uint8_t *>(malloc(fb->len));
    if (*jpgBuf) {
      memcpy(*jpgBuf, fb->buf, fb->len);
      *jpgLen = fb->len;
      ok = true;
    }
  } else {
    ok = frame2jpg(fb, 80, jpgBuf, jpgLen);
  }

  esp_camera_fb_return(fb);
  return ok;
}
