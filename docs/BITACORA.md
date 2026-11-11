# Bitácora — IO Flex

Registro del desarrollo y decisiones de diseño. Jorge Peñaloza.

## Contexto

El proyecto nació de la idea de una distribución Linux propia (diferida a "etapa 2") y derivó en algo práctico y acotado: un sistema OT de dos niveles con un **controlador** (hoy un PC, a futuro una Raspberry Pi) y **nodos de I/O** económicos que hablan **Modbus TCP**. Se eligió el **Arduino Nano ESP32** que había disponible como primer nodo de pruebas.

## Decisiones de diseño

- **Protocolo: Modbus TCP.** Estándar en OT, soportado por cualquier SCADA/PLC. El nodo actúa como servidor/esclavo en el puerto 502.
- **Configuración en el propio nodo (no en el PC).** Un andamiaje inicial configuraba los pines desde una web en el PC; se descartó: el nodo debe ser autónomo y configurarse desde su propio portal (puerto 80).
- **Modbus "solo valores".** Por Modbus se exponen únicamente los valores (un bloque de 20 holding registers desde una dirección de inicio configurable). El modo de cada pin se fija por la web. Mapa simple para el maestro.
- **Dirección de inicio configurable.** Regla `registro = inicio + índice`, mostrada por pin en la tabla del portal, para que calce con el mapa del SCADA sin ambigüedad.
- **mDNS.** El nodo responde como `ioflex.local`, para no depender de la IP (útil con DHCP o IP fija).
- **Guardado de red a prueba de fallos.** Problema detectado: guardar una clave mal escrita dejaba el nodo inaccesible. Solución: validar la conexión antes de persistir.
  - Desde el AP: prueba en modo AP+STA sin perder el AP; guarda solo si conecta, con feedback en la misma página.
  - Desde la red (STA): guarda la nueva config como *candidato*, reinicia, la prueba y, si falla, **revierte** sola a la última configuración que funcionaba.
- **Página de resultado con auto-retorno.** Tras guardar, la página espera a que el nodo vuelva (polling al **mismo origen**, para evitar bloqueo CORS al entrar por IP), muestra el estado y ofrece botón "Volver al portal".

## Hitos

1. Demo Modbus TCP básico (`nodo_modbus_tcp`): contador, ADC y un coil para el LED.
2. Solución de flasheo en Windows sin admin: driver **WinUSB** con **Zadig** sobre "Arduino DFU".
3. **IO Flex**: 20 pines configurables por registros Modbus + portal de pines.
4. Portal de red en el nodo (WiFi/IP) con AP de rescate `IOFlex-Setup`.
5. Portal unificado **Red + Pines** servido por el Arduino; se retira la web del PC.
6. **mDNS** (`ioflex.local`) + Modbus **solo valores** con **dirección de inicio configurable**.
7. **Guardado a prueba de fallos** (prueba + reversión) y página de resultado con auto-retorno (polling same-origin).

## Estado actual

- Firmware operativo y probado: portal (Red+Pines), Modbus TCP, mDNS, failsafe con reversión verificada.
- Config del nodo de pruebas: IP por DHCP (se encuentra por `ioflex.local`); base Modbus configurable desde el portal.

## Pendiente / próximos pasos

- Migrar el rol de controlador/orquestador a una Raspberry Pi.
- Acceso remoto por Tailscale mediante *subnet router* en la Pi.
- Acondicionamiento de señal para 5 V / 24 V y pruebas con sensores/relés reales.
- Conseguir un segundo nodo ESP32 para uso práctico.
