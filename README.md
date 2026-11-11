# IO Flex — Nodo de I/O remoto Modbus TCP (Arduino Nano ESP32)

Nodo de entradas y salidas remoto, **configurable 100% desde su propia página web**, que habla **Modbus TCP** con cualquier maestro (PLC, SCADA, una Raspberry Pi o un cliente como Modbus Poll). No requiere programar nada para usarlo: la red, la dirección Modbus y el modo de cada pin se configuran desde el navegador.

Pensado como bloque de I/O económico para una arquitectura OT de dos niveles: un **controlador/orquestador** (hoy un PC, a futuro una Raspberry Pi) + **nodos IO Flex** sobre la red.

> Proyecto personal de Jorge Peñaloza. Firmware para **Arduino Nano ESP32** (módulo u-blox NORA-W106, ESP32-S3).

---

## Características

- **Portal web propio** (puerto 80): todo se configura desde el nodo, sin apps ni IDE.
  - Pestaña **Red**: nombre (mDNS), WiFi e IP (DHCP o estática).
  - Pestaña **Pines**: dirección Modbus de inicio + modo y valor de cada pin.
- **Modbus TCP** (puerto 502), esclavo/servidor, **solo valores**: un bloque de 20 holding registers desde la dirección de inicio que tú eliges.
- **Direcciones que defines tú**: `registro = inicio + índice del pin`. La tabla del portal muestra el registro exacto de cada pin.
- **Descubrimiento por nombre (mDNS)**: se accede como `http://<nombre>.local` (por defecto `ioflex.local`), sin depender de la IP.
- **Guardado de red a prueba de fallos**: antes de quedarse con una configuración, el nodo **prueba la conexión**. Si falla (p. ej. clave mal escrita), **no la guarda** y vuelve solo a la última que funcionaba. Nunca se queda inaccesible por un error de tipeo.
- **AP de rescate**: si no logra conectarse a ninguna red, levanta un punto de acceso `IOFlex-Setup` con el portal para reconfigurarlo.
- **Persistencia en NVS**: red y configuración de pines sobreviven a cortes de energía.

---

## Hardware

- **Arduino Nano ESP32** (ESP32-S3, 3.3 V, WiFi/BLE).
- 20 pines de I/O expuestos: `D2…D13` (índices 0–11) y `A0…A7` (índices 12–19).
- Las entradas analógicas (`A0…A7`) usan el ADC de 12 bits (0–4095).

> ⚠️ Lógica de **3.3 V**. Para señales/relés de 5 V o 24 V usar acondicionamiento (optoacopladores, módulos de relé, divisores).

---

## Estructura del repositorio

```
firmware/
  io_flex/            Firmware principal (portal web + Modbus + failsafe + mDNS)
  io_flex_reset/      Sketch desechable: borra la config de red guardada (NVS)
  nodo_modbus_tcp/    Demo mínimo inicial (Modbus TCP básico)
web/                  Scripts de prueba desde PC (opcionales, ver nota)
docs/
  INSTALL.md          Herramientas, versiones y cómo compilar/flashear
  BITACORA.md         Registro del desarrollo y decisiones de diseño
```

> `tools/`, `wifi.txt`, los videos y los binarios de compilación están excluidos por `.gitignore`.

---

## Mapa Modbus

Función soportada: **Holding Registers** (FC03 leer, FC06 escribir uno, FC16 escribir varios). Direccionamiento **base 0** (protocolo).

| Pin | Índice | Registro |
|-----|--------|----------|
| D2 … D13 | 0 … 11 | `inicio + 0` … `inicio + 11` |
| A0 … A7 | 12 … 19 | `inicio + 12` … `inicio + 19` |

- La **dirección de inicio** se fija en la pestaña Pines (se guarda en NVS; cambiarla reinicia el nodo). Ejemplo con inicio `100`: `D2 = 100`, `A0 = 112`, `A7 = 119`.
- **Valor** de cada pin:
  - Entrada / Entrada+pullup → el maestro **lee** `0`/`1`.
  - Analógica → el maestro **lee** `0…4095`.
  - Salida → el maestro **escribe** `0`/`1`.
- El **modo** de cada pin se fija por la web (no se expone por Modbus), para dejar el mapa limpio.

Modos (en el portal): `0` Off · `1` Entrada · `2` Entrada+pullup · `3` Salida · `4` Analógica.

---

## Puesta en marcha (resumen)

1. **Compilar y flashear** `firmware/io_flex` en el Nano ESP32 — ver [docs/INSTALL.md](docs/INSTALL.md) (arduino-cli + DFU; en Windows, driver WinUSB con Zadig).
2. **Primer arranque**: el nodo intenta conectarse por WiFi en **DHCP** (usando el respaldo `DEF_SSID`/`DEF_PASS` del firmware; por defecto la IP es DHCP, no estática). Si no editaste ese respaldo o no logra conectar, levanta el AP `IOFlex-Setup` (clave `configurar`) para configurarlo: conéctate y abre `http://192.168.4.1/`.
3. En **Red**: elige tu WiFi, clave e IP (DHCP o fija en el segmento de tu red). *Probar y guardar* valida la conexión antes de aplicarla.
4. Entra por **`http://ioflex.local/`** (o la IP). En **Pines**: define la dirección Modbus de inicio y el modo de cada pin.
5. Desde tu maestro Modbus, conéctate a la **IP del nodo : 502** y lee/escribe desde la dirección de inicio.

### Ejemplo con Modbus Poll
- Connection → Connect: **Modbus TCP/IP**, IP del nodo, puerto **502**.
- Setup → Read/Write Definition: función **03**, Address = **tu dirección de inicio**, Quantity = **20**.
- Escribir una salida: función **06** sobre el registro del pin.

---

## Scripts de prueba (`web/`)

`configurador.py` y `nodo_live.py` fueron herramientas de prueba desde el PC (vía Modbus) usadas durante el desarrollo. **Ya no son necesarias**: el nodo se configura solo desde su portal. Se conservan como referencia.

---

## Roadmap

- Raspberry Pi como controlador/orquestador y, opcionalmente, SCADA web.
- Acceso remoto por **Tailscale** vía un *subnet router* en la Pi (el ESP32 no corre Tailscale).
- Acondicionamiento de señal para 5 V / 24 V.
- Soporte de varios nodos con nombres mDNS distintos.

---

## Licencia

MIT — ver [LICENSE](LICENSE). Jorge Peñaloza, 2026.
