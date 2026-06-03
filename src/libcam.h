/*
 * libcam.h — Biblioteca de camara OV2640 para deteccion de movimiento.
 */

#ifndef LIBCAM_H
#define LIBCAM_H

#include <Arduino.h>

/// Intervalo entre capturas de cámara (en milisegundos)
#define CAM_CAPTURE_INTERVAL 700

// Sensibilidad de deteccion de movimiento
#define MOTION_PIXEL_DIFF_THRESHOLD 28
#define MOTION_MIN_CHANGED_SAMPLES 320
#define MOTION_SAMPLE_STRIDE 4

// ─────────────────────────────────────────────────────────────
//  PINES OV2640 — GOOUUU ESP32-S3-CAM (fijos en PCB)
//  No los cambies a menos que tu placa sea diferente.
// ─────────────────────────────────────────────────────────────
#define CAM_PIN_PWDN    -1   ///< No usado en esta placa
#define CAM_PIN_RESET   -1   ///< No usado en esta placa
#define CAM_PIN_XCLK    15   ///< Clock

#define CAM_PIN_SIOD     4   ///< I2C SDA
#define CAM_PIN_SIOC     5   ///< I2C SCL

#define CAM_PIN_D7      16   ///< Bus de datos bit 7
#define CAM_PIN_D6      17   ///< Bus de datos bit 6
#define CAM_PIN_D5      18   ///< Bus de datos bit 5
#define CAM_PIN_D4      12   ///< Bus de datos bit 4
#define CAM_PIN_D3      10   ///< Bus de datos bit 3
#define CAM_PIN_D2       8   ///< Bus de datos bit 2
#define CAM_PIN_D1       9   ///< Bus de datos bit 1
#define CAM_PIN_D0      11   ///< Bus de datos bit 0

#define CAM_PIN_VSYNC    6   ///< Sincronización vertical
#define CAM_PIN_HREF     7   ///< Referencia horizontal
#define CAM_PIN_PCLK    13   ///< Pixel clock

// ─────────────────────────────────────────────────────────────
//  ESTADO GLOBAL DEL MOVIMIENTO DETECTADO
// ─────────────────────────────────────────────────────────────
extern bool motionDetected;       ///< true cuando se detecta movimiento en el ultimo frame
extern float motionLevel;         ///< Porcentaje de cambio entre frame actual y anterior

// ─────────────────────────────────────────────────────────────
//  FUNCIONES PÚBLICAS
// ─────────────────────────────────────────────────────────────

/**
 * Inicializa la cámara OV2640 con los pines y resolución definidos.
 * Debe llamarse una vez en setup().
 * @return true si la cámara se inicializó correctamente, false en error.
 */
bool initCamera();

/**
 * Captura frame en escala de grises y actualiza el estado global
 * de deteccion de movimiento.
 */
void updateMotionDetection();

/**
 * Captura un frame y lo devuelve en JPEG para endpoint web.
 * El buffer se reserva dinamicamente y el llamador debe liberarlo con free().
 */
bool captureJpegFrame(uint8_t **jpgBuf, size_t *jpgLen);

#endif /* LIBCAM_H */
