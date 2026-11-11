/*
  Nodo Modbus TCP  -  Arduino Nano ESP32 (ESP32-S3)
  Proyecto: Distribucion Linux / banco de pruebas OT  -  Jorge Peñaloza
  Fecha: 2026-11-10

  Modo:  Station (STA) -> se une a la WiFi de la red (misma LAN que el PC).
  Libreria: modbus-esp8266 (emelianov) v4.x  ->  ModbusIP (servidor TCP puerto 502)

  Mapa Modbus:
    Hreg0 : contador de prueba (+1 cada segundo)
    Hreg1 : lectura analogica de A0 (0..4095)
    Coil0 : controla el LED de la placa (ON/OFF)
*/

#include <WiFi.h>
#include <ModbusIP_ESP8266.h>

// ===== Credenciales WiFi (editar antes de compilar) =====
const char* WIFI_SSID = "TU_WIFI";
const char* WIFI_PASS = "TU_CLAVE_WIFI";
// ========================================================

ModbusIP mb;

const uint16_t HREG_CONTADOR = 0;
const uint16_t HREG_ADC      = 1;
const uint16_t COIL_LED      = 0;

const int LED_PIN = LED_BUILTIN;
const int ADC_PIN = A0;

unsigned long lastUpdate = 0;
unsigned long lastBeat   = 0;
uint16_t contador = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_PIN, OUTPUT);

  Serial.print("Conectando a WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi OK. IP del nodo: ");
  Serial.println(WiFi.localIP());
  Serial.println("Servidor Modbus TCP escuchando en puerto 502");

  mb.server();
  mb.addHreg(HREG_CONTADOR, 0);
  mb.addHreg(HREG_ADC, 0);
  mb.addCoil(COIL_LED, false);
}

void loop() {
  mb.task();

  unsigned long now = millis();
  if (now - lastUpdate > 1000) {
    lastUpdate = now;
    contador++;
    mb.Hreg(HREG_CONTADOR, contador);
    mb.Hreg(HREG_ADC, analogRead(ADC_PIN));
  }
  if (now - lastBeat > 3000) {
    lastBeat = now;
    Serial.print("[beat] IP="); Serial.print(WiFi.localIP());
    Serial.print(" contador="); Serial.println(contador);
  }

  digitalWrite(LED_PIN, mb.Coil(COIL_LED) ? HIGH : LOW);
  yield();
}
