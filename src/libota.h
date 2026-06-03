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

#ifndef LIBOTA_H
#define LIBOTA_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// Constantes para OTA
#define OTA_TOPIC "dispositivo/device1/ota"  // Tópico para recibir actualizaciones OTA
#define OTA_BUFFER_SIZE 4096                 // Tamaño del buffer para descargar firmware

// Estructura para pasar datos a la tarea OTA
struct OTAData {
    char* url;
    char* version;
};

// Funciones para OTA
void setupOTA(PubSubClient & client);                            // Configuración inicial de OTA
void checkOTAUpdate(const char* payload);   // Verifica si hay actualizaciones disponibles
void performOTAUpdateTask(void* parameter); // Función que ejecuta la OTA (en otro hilo)
void subscribeToOTATopic(PubSubClient & client);                 // Suscribe al tópico de OTA
void startOTATask(const char* url, const char* version); // Lanza la tarea OTA en otro núcleo
#endif /* LIBOTA_H */
