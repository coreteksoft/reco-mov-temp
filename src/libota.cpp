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

#include <libota.h>
#include <libiot.h>
#include <libstorage.h>
#include <cstring>
#include <cstdlib>

// Versión del firmware (debe coincidir con main.cpp)
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v1.1.1"
#endif


/**
 * Configuración inicial de OTA
 */
void setupOTA(PubSubClient& client) {
    Serial.println("--- Configurando OTA ---");
    Serial.print("OTA_TOPIC definido: ");
    Serial.println(OTA_TOPIC);
    
    // Inicializa la biblioteca Update
    Update.onProgress([](unsigned int progress, unsigned int total) {
        // Muestra el progreso de la actualización
        Serial.printf("Progreso: %u%%\r", (progress * 100) / total);
    });
    
    // Suscribe al tópico de OTA
    subscribeToOTATopic(client);
    
    Serial.println("--- OTA configurado ---");
}

/**
 * Suscribe al tópico de OTA
 */
void subscribeToOTATopic(PubSubClient & client) {
    // Verifica si el cliente MQTT está conectado
    if (!client.connected()) {
        Serial.println("Cliente MQTT no conectado. No se puede suscribir al tópico OTA.");
        return;
    }
    
    Serial.print("Intentando suscribirse a: '");
    Serial.print(OTA_TOPIC);
    Serial.println("'");
    
    // Suscribe con QoS 1 para asegurar la entrega
    bool result = client.subscribe(OTA_TOPIC, 1);
    if (result) {
        Serial.println("✓ Suscrito exitosamente al tópico OTA: " + String(OTA_TOPIC));
        // Procesar mensajes inmediatamente para confirmar suscripción
        client.loop();
        delay(50);
        Serial.println("✓ Procesamiento de mensajes después de suscripción OTA completado");
    } else {
        Serial.println("✗ Error al suscribirse al tópico OTA: " + String(OTA_TOPIC));
        Serial.print("Estado del cliente: ");
        Serial.println(client.state());
    }
}

/**
 * Verifica si hay actualizaciones disponibles
 * Procesa el mensaje JSON recibido en el tópico OTA
 */
void checkOTAUpdate(const char* payload) {
    Serial.println("Mensaje OTA recibido: " + String(payload));
    String currentVersion = getFirmwareVersion();
    Serial.print("Versión actual del firmware: ");
    Serial.println(currentVersion);
    
    // Parsea el JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.print("Error al parsear JSON: ");
        Serial.println(error.c_str());
        return;
    }
    
    // Verifica si el JSON contiene la URL
    if (doc.containsKey("url")) {
        const char* url = doc["url"];
        const char* version = doc["version"] | "desconocida";
        
        String currentVersion = getFirmwareVersion();
        Serial.print("Versión actual: ");
        Serial.println(currentVersion);
        Serial.println("Nueva versión disponible: " + String(version));
        Serial.println("URL de actualización: " + String(url));
        
        // Lanza tarea OTA con URL y versión
        startOTATask(url, version);
    } else {
        Serial.println("Mensaje OTA inválido: No contiene URL");
    }
}


/**
 * Lanza la tarea OTA en otro núcleo
 */
void startOTATask(const char* url, const char* version) {
    // Crear estructura con datos para la tarea
    OTAData* otaData = (OTAData*)malloc(sizeof(OTAData));
    if (otaData == NULL) {
        Serial.println("Error: No se pudo asignar memoria para OTA");
        return;
    }
    
    // Hacer copias en heap para que sobrevivan en la tarea
    // Usar malloc + strcpy en lugar de strdup para mayor compatibilidad
    size_t urlLen = strlen(url) + 1;
    size_t versionLen = strlen(version) + 1;
    
    otaData->url = (char*)malloc(urlLen);
    otaData->version = (char*)malloc(versionLen);
    
    if (otaData->url == NULL || otaData->version == NULL) {
        Serial.println("Error: No se pudo asignar memoria para strings OTA");
        free(otaData->url);
        free(otaData->version);
        free(otaData);
        return;
    }
    
    strcpy(otaData->url, url);
    strcpy(otaData->version, version);

    xTaskCreatePinnedToCore(
        performOTAUpdateTask, // función
        "OTA_Task",           // nombre de la tarea
        8192,                 // tamaño del stack
        otaData,              // parámetro (puntero a estructura OTAData)
        1,                    // prioridad
        NULL,                 // handle
        1                     // núcleo (Core 1)
    );
}


/**
 * Función que ejecuta la OTA (en otro hilo)
 */
void performOTAUpdateTask(void* parameter) {
    OTAData* otaData = (OTAData*)parameter;
    const char* url = otaData->url;
    const char* version = otaData->version;

    Serial.println("Iniciando actualización OTA desde: " + String(url));
    Serial.println("Nueva versión: " + String(version));

    HTTPClient http;
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("Error HTTP: %d\n", httpCode);
        http.end();
        free(otaData->url);
        free(otaData->version);
        free(otaData);
        vTaskDelete(NULL);
        return;
    }

    int contentLength = http.getSize();
    Serial.printf("Tamaño del firmware: %d bytes\n", contentLength);

    if (!Update.begin(contentLength)) {
        Serial.println("No hay espacio suficiente para la actualización");
        http.end();
        free(otaData->url);
        free(otaData->version);
        free(otaData);
        vTaskDelete(NULL);
        return;
    }

    uint8_t buff[OTA_BUFFER_SIZE] = { 0 };
    WiFiClient* stream = http.getStreamPtr();

    size_t written = 0;
    while (written < contentLength) {
        size_t available = stream->available();
        if (available) {
            size_t bytesToRead = min(available, sizeof(buff));
            size_t bytesRead = stream->readBytes(buff, bytesToRead);
            
            if (Update.write(buff, bytesRead) != bytesRead) {
                Serial.println("Error al escribir en la memoria flash");
                http.end();
                free(otaData->url);
                free(otaData->version);
                free(otaData);
                vTaskDelete(NULL);
                return;
            }
            written += bytesRead;
        }
        delay(1);  // respirito para el watchdog
    }

    if (Update.end()) {
        Serial.println("Actualización completada correctamente");
        
        // Guardar la nueva versión en memoria no volátil antes de reiniciar
        Serial.print("Guardando nueva versión en memoria no volátil: ");
        Serial.println(version);
        if (saveFirmwareVersion(String(version))) {
            Serial.println("✓ Versión guardada correctamente");
        } else {
            Serial.println("⚠ Error al guardar la versión (continuando igualmente)");
        }
        
        http.end();
        free(otaData->url);
        free(otaData->version);
        free(otaData);
        delay(1000);
        ESP.restart();
    } else {
        Serial.println("Error al finalizar la actualización: " + String(Update.errorString()));
        http.end();
        free(otaData->url);
        free(otaData->version);
        free(otaData);
        vTaskDelete(NULL);
    }
}
