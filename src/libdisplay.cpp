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

#include <libdisplay.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // Pantalla OLED vinculada al dispositivo

/**
 * Vincula la pantalla al dispositivo y asigna el color de texto blanco como predeterminado.
 * Si no es exitosa la vinculación, se muestra un mensaje en consola.
 */
void startDisplay() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306: No encontrada en 0x3C, intentando 0x3D..."));
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(F("SSD1306: No encontrada. Continuando sin pantalla."));
      return; // Continúa sin pantalla en lugar de congelarse
    }
  }
  Serial.println(F("SSD1306: Iniciada correctamente"));
  display.setTextColor(SSD1306_WHITE);
}

void displayMonitoring(float temp, float humi, bool motion, float motionLevel,
                       bool sensorReady, bool cameraReady) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Monitoreo IoT");
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);
  display.setCursor(0, 14);

  if (sensorReady) {
    display.printf("Temp: %.1f C\n", temp);
    display.printf("Hum : %.1f %%\n", humi);
  } else {
    display.println("Temp: --.- C");
    display.println("Hum : --.- %");
  }

  display.printf("Mov : %s\n", motion ? "ACTIVO" : "QUIETO");
  display.printf("Nivel: %4.1f%%", motionLevel);

  if (!cameraReady) {
    display.setCursor(90, 0);
    display.print("CAM!");
  }

  if (!sensorReady) {
    display.setCursor(90, 54);
    display.print("SHT!");
  }

  display.display();
}