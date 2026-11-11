# Instalación, compilación y flasheo

Herramientas y versiones usadas (fijadas para reproducibilidad; en sitios OT aislados conviene archivarlas).

## Versiones

| Herramienta | Versión | Uso |
|-------------|---------|-----|
| arduino-cli | 1.6.0 | Compilar y flashear (headless) |
| Core `arduino:esp32` | 2.0.18-arduino.5 | Soporte Nano ESP32 |
| Lib `modbus-esp8266` (emelianov) | 4.1.0 | Servidor Modbus TCP (clase `ModbusIP`) |
| FQBN | `arduino:esp32:nano_nora` | Placa |
| Zadig | — | Instalar driver WinUSB (solo Windows, una vez) |
| ffmpeg | 9.0.2 | (Opcional) material de video del proyecto |
| edge-tts | — | (Opcional) narración TTS del video |

## Flasheo del Nano ESP32 (DFU)

El Nano ESP32 se programa por **DFU**. En **Windows** la interfaz "Arduino DFU" necesita el driver **WinUSB** una sola vez:

1. Conectar la placa por USB.
2. Abrir **Zadig** como administrador.
3. Options → *List All Devices*. Seleccionar **Arduino DFU** (aparece en "Otros dispositivos").
4. Elegir driver **WinUSB** → *Install Driver*. (Paso único; luego no se repite.)

## Procedimiento online

```bash
arduino-cli core update-index
arduino-cli core install arduino:esp32@2.0.18-arduino.5
arduino-cli lib install "modbus-esp8266@4.1.0"

# Compilar y subir (ajustar el puerto COM/serial)
arduino-cli compile --fqbn arduino:esp32:nano_nora --upload -p COM4 firmware/io_flex
```

Para recuperar un nodo con config de red inválida (sin tener que entrar por el AP):

```bash
# 1) Borra la config de red guardada en NVS
arduino-cli compile --fqbn arduino:esp32:nano_nora --upload -p COM4 firmware/io_flex_reset
# 2) Vuelve a cargar el firmware normal (arranca con el respaldo DEF_SSID por DHCP)
arduino-cli compile --fqbn arduino:esp32:nano_nora --upload -p COM4 firmware/io_flex
```

## Procedimiento air-gapped (sitio sin internet)

En un equipo con internet, archivar y llevar al sitio:

- El binario de `arduino-cli` (1.6.0).
- El core `arduino:esp32@2.0.18-arduino.5` (carpeta de `~/.arduino15/packages` o `%LOCALAPPDATA%\Arduino15\packages`).
- La librería `modbus-esp8266@4.1.0` (carpeta `libraries`).
- El instalador de Zadig (Windows).

En el sitio, copiar esas carpetas a la instalación local de arduino-cli y compilar con el mismo comando. Verificar con `arduino-cli core list` y `arduino-cli lib list`.

## Notas de entorno (Windows)

- Sin permisos de administrador en la sesión: el único paso que pide admin es instalar el driver con Zadig (una vez).
- Si usas **FortiClient VPN**, desconéctala para descargar (corta la salida a internet en WSL2 y puede bloquear descargas).
