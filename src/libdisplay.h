/*
 * The MIT License
 *
 * Copyright 2024 Alvaro Salazar <alvaro@denkitronik.com>.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef LIBDISPLAY_H
#define LIBDISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <time.h>

#define SCREEN_WIDTH 128    ///< Ancho de la pantalla (en pixeles)
#define SCREEN_HEIGHT 64    ///< Alto de la pantalla (en pixeles)

// Pines I2C para la OLED — GPIO libres que no usa la cámara OV2640
#define OLED_SDA 21         ///< SDA de la OLED → GPIO21 del ESP32-S3-CAM
#define OLED_SCL 20         ///< SCL de la OLED → GPIO20 del ESP32-S3-CAM

extern Adafruit_SSD1306 display; ///< Pantalla OLED vinculada al dispositivo

void startDisplay();
void displayMonitoring(float temp, float humi, bool motion, float motionLevel,
					   bool sensorReady, bool cameraReady);

#endif /* LIBDISPLAY_H */