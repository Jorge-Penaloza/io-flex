# Guion de video — Nodo IO Flex (Arduino Nano ESP32 + Modbus TCP)

**Jorge Peñaloza · 2026-11-10**
Duración objetivo: ~7 min. Formato: screencast con narración (OBS). Destino: **YouTube**.

**Título sugerido:** *Nodo de I/O Modbus TCP con Arduino Nano ESP32 — configúralo desde su propia web (IO Flex)*
Alternativas: "Expansión de I/O Modbus con ESP32, sin programar: todo por web" · "Arduino como esclavo Modbus TCP configurable: IO Flex paso a paso".

---

## Escena 0 — Gancho (primeros 10–15 s, no te lo saltes)

**EN PANTALLA:** el portal en la pestaña Pines + Modbus Poll leyendo valores en vivo, lado a lado.

**NARRACIÓN:**
"Esto es un Arduino de treinta dólares funcionando como una expansión de entradas y salidas industrial, hablando Modbus TCP, y lo mejor: no le programé nada; se configura entero desde esta página web. En siete minutos te muestro cómo funciona, qué se configura y cómo lo conectas a cualquier SCADA. Vamos."

---

## Antes de grabar (checklist)

- [ ] Portal del nodo abierto en el navegador: **http://ioflex.local/** (o `http://192.168.2.220/`).
- [ ] **Modbus Poll** abierto y conectado al nodo.
- [ ] Nodo encendido y en STA (barra del portal: "Modo: STA (conectado)").
- [ ] Escena de OBS con el navegador a la izquierda y Modbus Poll a la derecha (como en tu captura).
- [ ] Datos reales de referencia: IP `192.168.2.220` · nombre `ioflex.local` · puerto Modbus `502` · base de ejemplo `100`.

> En cada escena: **EN PANTALLA** = qué mostrar/hacer · **NARRACIÓN** = lo que dices.

---

## Escena 1 — Qué es el nodo IO Flex (~40 s)

**EN PANTALLA:** el portal abierto en la pestaña Pines, con los pines a la vista.

**NARRACIÓN:**
"Esto es un nodo IO Flex: un Arduino Nano ESP32 que funciona como una expansión de entradas y salidas remota, sobre Modbus TCP. La idea es simple: un controlador —hoy este PC, mañana una Raspberry— le habla por Modbus para leer entradas y mandar salidas, y el propio nodo se configura solo desde esta página web, sin instalar nada. Vamos a ver cómo se configura, qué significa cada cosa, y de dónde saco los datos para usarlo desde otra herramienta."

---

## Escena 2 — Cómo se accede y dónde ver la IP (~60 s)

**EN PANTALLA:** señala con el mouse la **barra de estado** arriba del portal: "Modo: STA (conectado) | IP: 192.168.2.220 | MiWiFi | ioflex.local". Mira la barra de direcciones (`ioflex.local`).

**NARRACIÓN:**
"Lo primero: cómo llego al nodo y de dónde saco sus datos. Fíjense en esta barra de arriba: me dice el modo, la IP, la red WiFi a la que está conectado y su nombre. Yo entré escribiendo `ioflex.local` —ese es el nombre del nodo por mDNS—, así no necesito recordar la IP: aunque el router le cambie la dirección, por nombre siempre lo encuentro. Y acá, en la barra, está la IP real: `192.168.2.220`. Esos dos datos —el nombre `ioflex.local` y la IP— son los que voy a usar en cualquier otra herramienta que le hable al nodo: un Modbus Poll, un SCADA, lo que sea. Este es el lugar donde siempre los puedo ver."

---

## Escena 3 — Pestaña Red: qué se configura y por qué (~90 s)

**EN PANTALLA:** clic en la pestaña **Red**. Recorre los campos de arriba a abajo con el mouse mientras hablas (nombre, SSID, clave, IP DHCP/estática). **No guardes** (para no reiniciar en vivo).

**NARRACIÓN:**
"En la pestaña Red configuro cómo se conecta el nodo. Primero el nombre: `ioflex` —eso es lo que después resuelve como `ioflex.local`—. Si tengo varios nodos, a cada uno le pongo un nombre distinto y los distingo fácil.
Después la red WiFi y la clave: el nodo se une a mi red como un equipo más.
Y acá lo importante: la IP. Tengo dos opciones. **DHCP**, que es automática: el router le asigna la dirección. Es cómodo, pero la dirección puede cambiar, y para un equipo de control eso molesta. Por eso en campo yo uso **IP estática**: le fijo una dirección que no cambia nunca. Eso sí —y esto es clave— la IP estática tiene que estar en el mismo segmento de mi red: acá mi red es 192.168.2, con gateway 192.168.2.1, así que la IP fija tiene que ser 192.168.2 punto algo libre. Si me equivoco de segmento, el nodo no conecta.
Cuando guardo, el nodo se reinicia y queda con esa configuración. Si alguna vez no logra conectarse, levanta solo una red WiFi propia llamada `IOFlex-Setup` para poder entrar a arreglarlo. O sea: nunca me quedo sin forma de llegar a él."

---

## Escena 4 — Pestaña Pines: la dirección Modbus y el mapa (~90 s)

**EN PANTALLA:** clic en la pestaña **Pines**. Señala el campo **"Dirección Modbus de inicio"** (muestra `100`). Luego recorre la tabla y detente en la columna **Reg**: D2→100, D3→101, … A7→119.

**NARRACIÓN:**
"En la pestaña Pines está el corazón del asunto. Arriba tengo la 'Dirección Modbus de inicio'. Esto es lo que yo decido: en qué registro Modbus empiezan los valores de los pines. Le puse 100. La regla es muy simple: **registro = inicio + índice del pin**. Entonces D2, que es el índice cero, queda en el registro 100; D3 en el 101; y así hasta A7, que es el índice 19, en el 119. Y no tengo que calcularlo de memoria: la tabla me muestra, en la columna 'Reg', la dirección exacta de cada puerto. Si mañana necesito que empiecen en otro número para que calce con mi SCADA, cambio este valor, guardo, el nodo reinicia, y todas las direcciones se recalculan solas.
Por cada pin elijo el modo: entrada, entrada con pull-up, salida, o analógica. Fíjense que el modo lo decido acá, en la web —no se toca por Modbus—; por Modbus solamente leo o escribo el valor. Eso deja el mapa Modbus limpio: un solo bloque de 20 registros con los valores, y punto."

---

## Escena 5 — Configurar un pin y verlo (~60 s)

**EN PANTALLA:** en un pin (por ej. **D2**), despliega el combo y elige **Salida**. Aparece el botón ON/OFF. Dale **ON** y muestra cómo cambia. Luego botón **"Guardar modos en NVS"**.

**NARRACIÓN:**
"Lo hago en vivo. Tomo D2 y lo pongo como Salida. Al instante aparece el botón de encender y apagar. Le doy ON: eso pone el pin en alto. Si tuviera un relé conectado acá, ya habría conmutado. Lo puedo apagar de nuevo. Cuando ya tengo los pines como los quiero, aprieto 'Guardar modos', y el nodo recuerda esta configuración aunque lo apague y lo prenda: queda grabada en su memoria."

---

## Escena 6 — Usar la IP en otro lado: Modbus Poll (~120 s)

**EN PANTALLA:** pasa a **Modbus Poll**. Muestra **Connection → Connect**: IP `192.168.2.220`, puerto `502`. Luego **Setup → Read/Write Definition**: Function `03`, Address `100`, Quantity `20`. Muestra los valores llegando. Después en el portal pon un pin como Salida y en Modbus Poll usa **06** (Write Single Register) para encenderlo, o al revés.

**NARRACIÓN:**
"Y acá está el porqué de todo: usar esos datos desde otra herramienta. Me paso a Modbus Poll, que es un cliente Modbus. En Conexión elijo Modbus TCP y pongo exactamente lo que vi en el portal: la IP `192.168.2.220` y el puerto `502`. Si prefiero, en vez de la IP puedo poner el nombre `ioflex.local`, da lo mismo.
Ahora le digo qué leer: función 03, dirección de inicio 100 —la misma que fijé en el nodo— y cantidad 20, que son mis veinte pines. Y ahí están: los valores de todos los puertos, en vivo. El registro 100 es D2, el 112 es A2, igual que en la tabla del portal.
Para escribir una salida, uso la función 06: escribo un 1 en el registro del pin que puse como salida, y el pin se enciende. Lo que quiero que quede claro es esto: el nodo no depende de esta página para trabajar. La página es para configurarlo; una vez configurado, cualquier maestro Modbus —este Modbus Poll hoy, un SCADA o la Raspberry mañana— le habla con la IP y la dirección de inicio, y listo."

---

## Escena 7 — Cierre (~30 s)

**EN PANTALLA:** vuelve al portal, pestaña Pines, vista general.

**NARRACIÓN:**
"En resumen: el nodo IO Flex se configura solo, desde su propia página, sin instalar nada. En la pestaña Red fijo cómo se conecta y le pongo IP fija; en la barra de arriba siempre veo su IP y su nombre para usarlos donde necesite. En la pestaña Pines decido en qué dirección Modbus empiezan los valores y qué hace cada puerto, y la tabla me muestra la dirección exacta de cada uno. Y el control real pasa por Modbus, que es lo que entiende cualquier sistema industrial. Eso es todo."

---

## Chuleta de datos para mostrar en el video

| Dato | Valor | Dónde se ve |
|------|-------|-------------|
| Nombre (mDNS) | `ioflex.local` | Barra de estado del portal / pestaña Red |
| IP | `192.168.2.220` | Barra de estado del portal |
| Puerto Modbus | `502` | (fijo del estándar Modbus TCP) |
| Dirección de inicio | `100` (configurable) | Pestaña Pines |
| Mapa | registro = inicio + índice → D2=100 … A7=119 | Columna **Reg** de la tabla |
| Modo de cada pin | se fija en la web, no por Modbus | Pestaña Pines |
| Respaldo si no conecta | AP WiFi `IOFlex-Setup` (clave `configurar`) | — |

---

## Descripción para YouTube (cópiala en la caja del video)

> Cómo convertir un Arduino Nano ESP32 en un nodo de entradas y salidas remoto sobre **Modbus TCP**, configurable **100% desde su propia página web** (IO Flex), sin programar nada: red e IP fija, direcciones Modbus que defines tú, modo de cada pin, y cómo conectarlo desde un cliente Modbus (Modbus Poll) o un SCADA.
>
> Hecho por Jorge Peñaloza.

**Capítulos** (ajusta los tiempos a tu grabación real):

```
0:00  Qué vas a ver
0:15  Qué es el nodo IO Flex
0:55  Cómo accedo y dónde veo la IP y el nombre
1:55  Pestaña Red: WiFi e IP fija (y por qué el segmento importa)
3:25  Pestaña Pines: la dirección Modbus de inicio y el mapa
4:55  Configurar un pin en vivo
5:55  Leerlo y controlarlo desde Modbus Poll
7:55  Resumen
```

> Pegar los capítulos con estos tiempos en la descripción hace que YouTube marque las secciones en la barra de progreso automáticamente (el primero debe ser `0:00`).

**Tags sugeridos:** Modbus TCP, Arduino, ESP32, Nano ESP32, IO remoto, SCADA, automatización industrial, OT, esclavo Modbus, IO Flex.
