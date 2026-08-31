# DOCUMENTO DE REQUISITOS DEL PRODUCTO (PRD) v5.0 — "MASTER"

## Ecosistema Domótico e Industrial Descentralizado Multi-Función sobre CAN Bus (29 bits) y Enrutamiento Puente Multi-Protocolo

**Fecha de Emisión:** 31 de Agosto de 2026

**Estado:** Especificación Técnica Aprobada para Desarrollo Avanzado

**Arquitecturas Objetivo:** ESP32 (Gateways, Pantallas, Orquestador OTA), ATmega328P, ATmega2560 (Nodos de Campo), STM32 (Opcional Nodos de Alta Densidad).

---

### 1. Resumen Ejecutivo y Filosofía de Diseño

El sistema se define como un ecosistema de automatización **100% descentralizado (Peer-to-Peer)** sin punto único de falla (Single Point of Failure - SPOF). No existe un orquestador o "cerebro central" obligatorio para la ejecución del control en tiempo real.

Cada nodo en el bus es multi-función y almacena de forma autónoma su propia "fuente de la verdad" (Source of Truth) en memoria no volátil (EEPROM interna para microcontroladores AVR o NVS/Preferences/LittleFS para ESP32). Cada nodo registra localmente a qué eventos locales o remotos debe reaccionar, qué condiciones aplicar y qué informes de telemetría debe emitir.

El **ESP32 Gateway** funciona como orquestador de interfaz, motor de renderizado web, gestor de actualizaciones de firmware masivas (OTA) y **puente de traducción transparente multiprotocolo** (Ethernet UDP/TCP, MQTT, Modbus RTU/TCP). Ante una desconexión o fallo crítico del Gateway, los nodos de campo continúan ejecutando de forma ininterrumpida sus vinculaciones, lazos de control y automatismos locales.

```
       +-----------------------------------------------------------------+
       |                  Red de Redes / Infraestructura                 |
       |  +--------------------+  +--------------------+  +-----------+  |
       |  | Home Assistant /   |  | Red Ethernet Local |  | Servidor  |  |
       |  | Servidor MQTT      |  | (UDP/TCP Tunnel)   |  | Git (OTA) |  |
       |  +---------+----------+  +---------+----------+  +-----+-----+  |
       +------------|-----------------------|-------------------|--------+
                    |                       |                   |
          =============================================================
          |           ESP32 GATEWAY / PUENTE MULTI-PROTOCOLO           |
          =============================================================
             | (CAN_H / CAN_L)                             | (RS-485)
   +---------+-----------------------+           +---------+------------------+
   | BUS CAN (PCD v1 - 29 Bits)      |           | BUS MODBUS RTU             |
   +---------+-----------------------+           +---------+------------------+
             |                       |                     |
     +-------+-------+       +-------+-------+     +-------+-------+
     | Nodo Campo    |       | Pantalla      |     | Dispositivo   |
     | Multi-Función |       | Táctil CAN    |     | Modbus        |
     | (AVR/ESP32)   |       | (ESP32)       |     | Industrial    |
     +---------------+       +---------------+     +---------------+

```

---

### 2. Arquitectura de Nodos Multi-Función

El modelo clásico de automatización asigna un solo rol por dirección de red ("Un Dispositivo = Un Interruptor"). Este PRD establece un esquema de **Recursos Heterogéneos Agrupados por Node-ID**:

* **Estructura de Nodo Heterogéneo (Ejemplo Node-ID `0x15` - Living):**
* **Canal `0x10` (Relés / Cargas AC):** Sub-canales 1, 2 y 3 (Iluminación principal, extractor, valona).
* **Canal `0x20` (Dimmers corte de fase / PWM):** Sub-canales 1 y 2 (Luz difusa, velador).
* **Canal `0x30` (Entradas Digitales / Pulsadores):** Sub-canales 1 al 4 (Teclas de pared mecánicas con detección de clic simple, doble, largo).
* **Canal `0x40` (Sensores de Temperatura/Humedad):** Sub-canal 1 (Sensor AHT21/SHT30 I2C).
* **Canal `0x50` (Sensores de Calidad de Aire / Gases):** Sub-canal 1 (Sensor analógico MQ7 con acondicionamiento).


* **Indexación y Decodificación Interna:** El firmware base de cada nodo integra un administrador de dispositivos (Device Manager). El protocolo no direcciona al "Nodo `0x15`", sino al par **(Node-ID, Tipo de Recurso, Índice de Canal)**.

---

### 3. Protocolo de Comunicación CAN (PCD v1 - 29 Bits)

Se utiliza la trama extendida CAN 2.0B (29 bits de identificador) con una asignación fija de campos por hardware para resolución estricta de arbitraje sin colisiones y priorización nativa.

#### 3.1. Estructura Exacta del Identificador de 29 Bits

| Rango de Bits | Ancho | Campo | Descripción Técnica y Priorización |
| --- | --- | --- | --- |
| **28 - 26** | 3 bits | **Prioridad** | Rango `0` a `7`. `0` = Mensaje Crítico (Paro de emergencia, alarmas térmicas/fuego). `2` = Control de tiempo real. `4` = Transacciones SDO/Configuración. `6` = Mensajes de estado/telemetría periódica. `7` = Tráfico de fondo (Bloques de datos OTA). El bus CAN resuelve arbitraje dominantemente por los bits más significativos. |
| **25 - 21** | 5 bits | **Tipo de Mensaje** | Categoría funcional del paquete: `0x01` (Heartbeat/Vida), `0x02` (Evento/Acción PDO), `0x03` (Configuración SDO), `0x04` (OTA Firmware), `0x05` (Broadcast de Grupo/Escenas), `0x06` (Respuesta de Estado / ACK). |
| **20 - 14** | 7 bits | **Destino (Target)** | Dirección del nodo receptor (`0x01` a `0x7E`). La dirección `0x00` está reservada para Broadcast general o direccionamiento de grupo lógico. |
| **13 - 0** | 14 bits | **Origen (Source)** | Dirección del nodo emisor (`0x0001` a `0x3FFF`). Garantiza unicidad del ID en el bus para evitar pérdida de arbitraje entre dos nodos que transmitan el mismo tipo de mensaje en el mismo milisegundo. |

#### 3.2. Estructura Estándar del Payload (8 Bytes)

* **Byte 0 — Tipo de Recurso (Resource Type):**
* `0x10`: Relé / Salida Digital ON-OFF.
* `0x20`: Dimmer / Salida Analógica PWM (0-100%).
* `0x30`: Entrada Digital / Pulsador / Detector de presencia.
* `0x40`: Sensor Ambiental (Temperatura, Humedad, Presión).
* `0x50`: Sensor Gas / Calidad de Aire (PPM, CO2, AQI).
* `0x60`: Sensor Eléctrico (Voltaje, Corriente, Potencia, Factor de Potencia).
* `0x70`: Actuador de Cortina / Persianas (Posición 0-100%, Ángulo de lamas).
* `0xE0`: Configuración / Vinculación Lógica (SDO Payload).
* `0xF0`: Sistema / Control OTA / Gestión de Red.


* **Byte 1 — Índice del Canal (Channel Index):** Identificador numérico del recurso dentro del nodo (`0x01` a `0xFF`).
* **Bytes 2..7 — Datos / Comando / Carga Útil:**
* *Para Control/Comando:* Byte 2 = Acción (`0x00` OFF, `0x01` ON, `0x02` TOGGLE, `0x03` SET_VALUE), Byte 3-7 = Parámetros adicionales (tiempo de fade, temporizador de apagado).
* *Para Telemetría/Estado:* Bytes 2..5 = Valor en punto flotante IEEE 754 de 32 bits (float) o enteros codificados en Big-Endian, Bytes 6..7 = Flags de diagnóstico (error de sensor, sobrecalentamiento, bajo voltaje).



---

### 4. Ciclo de Confirmación, Bucle de Realimentación y Sincronización

Para erradicar incoherencias entre el estado real del actuador y la representación visual en interfaces (pantallas/web), se impone el protocolo determinista **Comando → Ejecución → Difusión de Estado (Feedback Loop)**:

1. **Iniciación:** Un emisor (Pantalla Táctil `0x05`, Gateway `0x01` o Botón Remoto) emite una trama dirigida (Tipo `0x02`, Destino `0x16`): *"Establecer Relé Canal 0x01 a ON"*.
2. **Procesamiento Local:** El Nodo `0x16` valida la trama, aplica los retardos de seguridad (anti-rebote, cruce por cero) y conmuta el componente físico.
3. **Confirmación Broadcast (ACK Operativo):** Inmediatamente después de la conmutación efectiva, el Nodo `0x16` genera una nueva trama de alta prioridad (Tipo `0x06`, Destino `0x00` Broadcast): *"Atención a todos: El Nodo 0x16, Recurso 0x10, Canal 0x01 cambió a estado ON"*.
4. **Sincronización Total:** Todos los agentes del bus (pantallas táctiles, pasarelas MQTT, registradores de eventos) interceptan el broadcast `0x06` y actualizan sus dashboards en tiempo real.
5. **Acción Local Física:** Si un usuario pulsa la tecla conectada directamente al pin del Nodo `0x16`, este conmuta el relé internamente y ejecuta el paso 3 (Broadcast `0x06`). Toda la red se actualiza de forma transparente e instantánea.

---

### 5. Nodos de Visualización: Pantallas Táctiles y Displays

Las pantallas de control (HMI) integradas en el bus son tratadas como nodos CAN de campo estándar (arquitectura Peer-to-Peer):

* **Operativa Descentralizada:** No dependen de la rendición de páginas HTML procesadas por un servidor externo. Poseen almacenamiento interno de pantallas, gráficos y tipografías.
* **Tabla de Suscripción (Input Mapping):** Matriz almacenada en memoria no volátil donde el nodo visual registra los pares `(Nodo_Origen, Recurso, Canal)` que deben refrescar indicadores gráficos, medidores o etiquetas de texto en pantalla.
* **Tabla de Acción (Output Mapping):** Matriz que vincula eventos de toque (`TouchEvent` en coordenadas X/Y o Botón ID) con la transmisión de tramas CAN Tipo `0x02` hacia los nodos destino correspondientes.
* **Aprovisionamiento Visual Remoto:** Mediante el servidor Web residente en el ESP32 Gateway, el instalador define las relaciones de la pantalla en un entorno gráfico. El Gateway compila estas reglas y las transfiere a la pantalla usando tramas de configuración SDO (Tipo `0x03`).

---

### 6. Ecosistema de Gateways y Puentes de Enrutamiento Multi-Protocolo

El Gateway basado en ESP32 actúa como un puente multifunción y conmutador de datos entre la red CAN local de baja nivel y las redes de datos de área local (LAN/WAN). El firmware modular permite compilar e integrar los siguientes perfiles de puente:

#### 6.1. Puente CAN ↔ MQTT / Home Assistant (Perfil IoT)

* **Monitoreo Pasivo:** Escucha continuamente las tramas de estado Broadcast (Tipo `0x06`) y las traduce a tópicos JSON estructurados en la red IP (`home/can/node_15/resource_10/channel_1/state`).
* **Auto-Descubrimiento (MQTT Discovery):** Genera payloads de descubrimiento en sintaxis compatible con Home Assistant (`homeassistant/light/can_15_10_1/config`), permitiendo que las entidades aparezcan automáticamente sin configuración manual en el servidor domótico.
* **Traducción de Ingesta:** Escucha en los tópicos de comando (`home/can/node_15/resource_10/channel_1/set`) y emite en el bus CAN las tramas Tipo `0x02` requeridas.

#### 6.2. Puente CAN ↔ Ethernet IP TCP/UDP (Perfil Túnel de Red)

* **Encapsulamiento Transparente:** Permite interconectar dos o más segmentos de buses CAN físicamente distantes (ej. Edificio Principal y Galpón distante) utilizando la red LAN Ethernet/Wi-Fi mediante túneles UDP/TCP.
* **Formato de Paquete del Túnel:** Encapsula la trama CAN nativa (ID de 29 bits, DLC, 8 bytes de payload, marcas de tiempo) dentro de un datagrama UDP compacto de baja latencia con verificación de integridad CRC16.
* **Filtros de Enrutamiento:** Permite configurar tablas de paso/bloqueo de tramas en el gateway para evitar congestionar el túnel IP con mensajes de telemetría de fondo no esenciales.

#### 6.3. Puente CAN ↔ RS-485 / Modbus RTU-TCP (Perfil Industrial)

* **Mapeo de Registros:** Convierte peticiones Modbus RTU/TCP (Funciones `0x03` Read Holding Registers, `0x06` Write Single Register, `0x10` Write Multiple Registers) en transacciones de la red CAN.
* **Integración Industrial:** Permite a PLCs o controladores industriales leer el estado de los sensores y botones CAN como si fueran registros Modbus estándar, o permitir que un pulsador CAN active un variador de frecuencia o bomba industrial conectada vía RS-485 Modbus RTU.

```
 +-----------------------------------------------------------------------------------+
 |                        ESP32 GATEWAY - ARQUITECTURA PUENTE                        |
 |                                                                                   |
 |  +-----------------------------------------------------------------------------+  |
 |  |                         MATRIZ DE ENRUTAMIENTO IP                           |  |
 |  +-------+------------------------------+------------------------------+-------+  |
 |          |                              |                              |          |
 |          v                              v                              v          |
 |  +---------------+              +---------------+              +---------------+  |
 |  |  Cliente MQTT |              |  Túnel UDP    |              | Servidor      |  |
 |  |  (Discovery)  |              |  (CAN-over-IP)|              | Modbus TCP    |  |
 |  +-------+-------+              +-------+-------+              +-------+-------+  |
 |          |                              |                              |          |
 |          +------------------------------+------------------------------+          |
 |                                         |                                         |
 |                                         v                                         |
 |                        +---------------------------------+                        |
 |                        |   MOTOR DE TRADUCCIÓN DE TRAMAS |                        |
 |                        |   Y TABLAS DE MAPEO INTERNO     |                        |
 |                        +----------------+----------------+                        |
 |                                         |                                         |
 |          +------------------------------+------------------------------+          |
 |          |                                                             |          |
 |          v                                                             v          |
 |  +---------------+                                             +---------------+  |
 |  | Controlador   |                                             | Controlador   |  |
 |  | CAN (TWAI)    |                                             | RS-485        |  |
 |  +-------+-------+                                             +-------+-------+  |
 +----------|-------------------------------------------------------------|----------+
            | (CAN_H / CAN_L)                                             | (A / B)
            v                                                             v
       Bus CAN 29-bit                                            Bus Modbus RTU

```

---

### 7. Sistema de Actualizaciones de Firmware Masivas (OTA) Orquestado por Git

El mantenimiento de firmware de los nodos AVR (ATmega328P/ATmega2560) y ESP32 se realiza de manera centralizada a través del bus CAN sin requerir conexiones físicas programadoras (ICSP/FTDI) en cada nodo.

#### 7.1. Flujo de Descubrimiento e Ingesta de Firmware

1. **Publicación:** El desarrollador sube la compilación del proyecto (binarios `.bin`) junto con un manifiesto `ota_manifest.json` a un repositorio Git (GitHub/GitLab/Gitea local).
2. **Consulta Registrada:** El ESP32 Gateway consulta el manifiesto JSON de forma programada o bajo demanda visual desde la interfaz Web.
* *Consideración de Red local:* Dado que las consultas NTP UDP (Puerto 123) suelen estar bloqueadas en determinados proveedores de internet (ISP), la validación de tiempo para certificados TLS/SSL se obtiene mediante NTP sobre TCP o consultando un servidor NTP/RTC interno en la infraestructura de red local (ej. MikroTik router).


3. **Estructura del Manifiesto OTA (`ota_manifest.json`):**
* Especifica el tipo de hardware objetivo (`target_hw`: `atmega2560_dimmer`, `esp32_hmi`), la versión del firmware (`version`: `2.1.0`), la firma hash SHA-256 de verificación y la URL directa de descarga del archivo ejecutable.



#### 7.2. Protocolo de Fragmentación y Transferencia CAN (Tipo `0x04`)

1. **Inicio de Sesión OTA:** El Gateway emite una trama CAN Tipo `0x04` (Sub-comando `START_OTA`) dirigida al Node-ID objetivo. Contiene el tamaño total del archivo binario y el checksum global SHA256/CRC32.
2. **Modo Bootloader:** El nodo receptor valida la solicitud, detiene las tareas de control no críticas, conmuta su ejecución al espacio de memoria del **Bootloader CAN** y responde con un ACK de preparación.
3. **Fragmentación de Carga Útil:** El Gateway descarga el archivo `.bin`, lo divide en fragmentos exactos de 7 bytes y los transmite secuencialmente utilizando tramas CAN Tipo `0x04`:
* **Byte 0:** Índice correlativo de bloque / Secuencia (`0x00` a `0xFF`).
* **Bytes 1..7:** 7 bytes de datos binarios del nuevo firmware.


4. **Escritura y Verificación de Flujo:** El Bootloader CAN del nodo recibe los bloques, los almacena temporalmente en el búfer de pÁgina Flash, calcula el CRC local y emite un ACK de ventana cada 32 bloques recibidos para evitar el desbordamiento del bus.
5. **Finalización:** Una vez transferido el 100% de los datos, el nodo verifica la firma digital del nuevo firmware almacenado en la Flash. Si la validación es exitosa, reescribe el puntero de vector de interrupciones y reinicia el sistema ejecutando la nueva versión. Si falla, revierte al firmware anterior (Dual-Bank en ESP32 o recuperación por respaldo en AVR).

---

### 8. Arquitectura de Firmware Modular y Gestión de Compilación

Para permitir que una misma base de código fuente compile en múltiples plataformas de hardware (ESP32, ATmega328P, ATmega2560, STM32) optimizando el uso de recursos, se define una estructura rígida orientada al preprocesador C/C++:

* **Inclusión Única Garantizada:** Todo archivo de cabecera debe incluir la directiva `#pragma once` en la primera línea.
* **Aislamiento por Arquitectura:** Se emplean macros condicionales nativas del compilador para segregar dependencias de plataforma:
```cpp
#if defined(ARDUINO_ARCH_ESP32)
    // Carga de librerías y controladores específicos de ESP32 (TWAI, WiFi, LittleFS)
#elif defined(__AVR__)
    // Carga de librerías ligeras para micros AVR (SPI, EEPROM, MCP2515)
#endif

```


* **Activación de Características por Configuración (`system_config.h`):** Las funcionalidades complejas se gestionan mediante banderas numéricas (`0` o `1`). Si una función está desactivada, el compilador elimina completamente el código del binario final:
* `FEATURE_OTA_MANAGER`
* `FEATURE_WEB_SERVER`
* `FEATURE_MQTT_BRIDGE`
* `FEATURE_MODBUS_BRIDGE`


* **Optimización de Producción y Debugging:** Las rutinas de trazado e impresión en puerto serie se envuelven en macros de preprocesador (`LOG_DEBUG()`, `LOG_ERROR()`). En compilaciones para producción, la definición de debug se establece en `0`, erradicando todas las cadenas de texto (strings) de la memoria Flash y liberando memoria RAM crítica en arquitecturas AVR.

---

### 9. Flujo de Configuración Descentralizada y Persistencia de Reglas

La lógica de automatización se ejecuta en el punto más cercano al actuador. La sincronización y configuración de reglas sigue un ciclo estricto de persitencia:

```
 [1. Usuario configura regla en Web GUI]
                    |
                    v
 [2. ESP32 guarda regla en JSON local (Backup/LittleFS)]
                    |
                    v
 [3. ESP32 envía trama SDO Tipo 0x03 vía CAN al Nodo Destino]
                    |
                    v
 [4. Nodo Destino valida, graba en EEPROM interna y responde ACK]
                    |
                    v
 [5. Conexión Gateway Perdida] ---> [6. Nodo ejecuta regla desde EEPROM sin interrupción]

```

1. **Definición de Regla:** El usuario crea una vinculación lógica desde la interfaz Web hospedada en el ESP32 Gateway (ej. *"Si Entrada 1 del Nodo 0x05 envía evento de pulsación corta -> Conmutar Relé Canal 2 del Nodo 0x16"*).
2. **Respaldo Local en Gateway:** El Gateway escribe la regla en un archivo estructurado JSON dentro del sistema de archivos interno `LittleFS`. Este archivo sirve como punto de restauración y exportación de la instalación completa.
3. **Transmisión de Configuración Lógica:** El Gateway emite una secuencia de tramas de configuración SDO (Tipo `0x03`) dirigidas a la memoria del **Nodo `0x16**` (nodo que ejecuta la acción) y del **Nodo `0x05**` (nodo que genera el evento si aplica).
4. **Almacenamiento Local en el Nodo:** El nodo receptor parsea el comando de configuración, valida la integridad de los parámetros y los graba de forma permanente en su matriz de vinculaciones en la memoria **EEPROM interna**. Emite un ACK de confirmación de almacenamiento al bus.
5. **Inmunidad Operativa:** Ante un corte de energía, falla de red o desconexión del ESP32 Gateway, los nodos inician leyendo su EEPROM local en milisegundos y ejecutan el control de forma totalmente autónoma.

---

### 10. Estructura de Archivos del Proyecto (PlatformIO)

La organización del repositorio de código fuente se estructura en módulos aislados por responsabilidad:

```text
canbus_ecosistema_v5/
├── platformio.ini               # Configuración global de entornos ([env:esp32_gateway], [env:atmega2560_node], etc.)
├── data/                        # Archivos estáticos para LittleFS (UI Web, CSS/JS comprimidos, plantillas)
├── include/
│   ├── system_config.h          # Macros globales de selección de hardware, flags FEATURE_* y modos DEBUG
│   ├── can_protocol.h           # Máscaras de bits, desplazamientos (<<, >>) para ID 29-bit y enums de payload
│   ├── hal_can.h                # Abstracción de capa física (Driver TWAI para ESP32, Driver MCP2515 para AVR)
│   ├── device_manager.h         # Gestor interno de canales, recursos y mapeo de I/O en nodos multi-función
│   ├── routing_engine.h         # Motor de enrutamiento transparente (CAN ↔ MQTT, CAN ↔ UDP, CAN ↔ Modbus)
│   ├── ota_manager.h            # Orquestador OTA: descarga desde repositorios Git y streaming de bloques por CAN
│   └── config_storage.h         # Capa de abstracción de persistencia: EEPROM (AVR) y Preferences/LittleFS (ESP32)
└── src/
    ├── main.cpp                 # Punto de entrada universal. Inicializa HAL, recupera config y ejecuta el Scheduler
    ├── can_bootloader.cpp       # Código del Bootloader residente en sección protegida Flash para AVR/ESP32
    ├── bridge_mqtt.cpp          # Implementación del cliente MQTT y motor de auto-descubrimiento para Home Assistant
    ├── bridge_tunnel.cpp        # Encapsulador/Desencapsulador de túnel de red UDP/TCP CAN-over-IP
    ├── bridge_modbus.cpp        # Driver maestro/esclavo Modbus RTU/TCP y tabla de traducción de registros
    └── web_server.cpp           # Servidor Web REST API, WebSockets y gestor visual de vinculaciones (Solo ESP32)

```

---

### 11. Consideraciones de Infraestructura de Red y Robustez Física

#### 11.1. Seguridad y Aislamiento Lógico

* **Segmentación de Red:** Los ESP32 Gateways deben residir en una VLAN o subred aislada (ej. `192.168.10.x/24`), separada del tráfico de usuarios finales.
* **Políticas de Firewall:** En configuraciones con routers avanzados (ej. MikroTik), el tráfico entre la red de automatización y el resto de la LAN se restringe mediante reglas estrictas de firewall, permitiendo únicamente el puerto MQTT (`1883`), HTTP/HTTPS de gestión (`80`/`443`) y el puerto UDP configurable del túnel CAN. Queda estrictamente prohibida la exposición directa de los puertos de gestión a redes WAN/Internet.

#### 11.2. Protección de la Capa Física del Bus

* **Aislamiento Galvanico:** Las etapas de potencia (control de relés, dimmers de 220VAC) y las entradas digitales de campo deben contar con optoacoplamiento (ej. PC817, MOC3041) para proteger las líneas del microcontrolador ante sobretensiones o transitorios.
* **Inmunidad al Bus CAN y RS-485:** Cada nodo debe incorporar su correspondiente transceptor de bus de grado industrial (ej. SN65HVD230 para CAN 3.3V, MCP2551/TJ1050 para CAN 5V, MAX485/ST485 para RS-485).
* **Protección ESD/Transitorios:** Incorporar diodos supresores de tensiones transitorias (TVS) bidireccionales (ej. PESD1CAN) conectados entre las líneas `CAN_H` / `CAN_L` (o `RS485_A` / `RS485_B`) y la tierra de protección (`GND`).
* **Terminación de Bus Conmutable:** Incorporar resistencias de terminación de bus de `120 Ω` (1/4W) en el diseño impreso de cada placa, activables mediante un conmutador DIP o Jumper físico. Estas resistencias **solo deben habilitarse en los dos nodos extremos** del cableado físico del bus.