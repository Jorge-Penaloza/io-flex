/*
  io_flex_reset  -  Sketch DESECHABLE: borra la config de red guardada (NVS "net")
  del nodo IO Flex. Se usa cuando quedo una config que impide conectar (p.ej. IP
  estatica en un segmento equivocado) y el nodo cae al AP.

  Procedimiento:
    1) Flashear este sketch -> borra NVS "net".
    2) Volver a flashear io_flex.ino (firmware normal). Al no haber config,
       usa el respaldo DEF_SSID por DHCP y vuelve a la LAN.

  Jorge Peñaloza - 2026-11-10
*/
#include <Preferences.h>

Preferences prefs;

void setup() {
  Serial.begin(115200);
  delay(400);
  prefs.begin("net", false);
  bool ok = prefs.clear();
  prefs.end();
  Serial.println();
  Serial.print("NVS 'net' borrado: ");
  Serial.println(ok ? "OK" : "FALLO");
  Serial.println("Ahora reflashear io_flex.ino (firmware normal).");
}

void loop() {
  delay(2000);
  Serial.println("[reset] NVS net limpio - reflashear io_flex.ino");
}
