# 6. Actualizaciones OTA por CAN

> **Estado: emisor implementado.** El `OtaManager` del gateway valida la imagen,
> calcula el CRC global y genera bloques `makeOtaData()` de 7 bytes. El bootloader
> CAN receptor, las ventanas ACK y la escritura de flash siguen pendientes.
> Esta seccion fija el contrato para que ambos lados se desarrollen sin
> ambiguedades.

## 6.1 Por que OTA sobre CAN

Los nodos AVR de campo no tienen WiFi y suelen quedar dentro de cajas de pared o
tableros. Actualizarlos por el mismo par trenzado que ya los une evita
desmontarlos. El gateway ESP32 recibe el binario por WiFi y lo reparte por CAN.

## 6.2 Prioridad y coexistencia

Las tramas OTA usan prioridad 7 (`PRIO_BACKGROUND`), la mas baja: el arbitraje
del bus garantiza que una alarma critica (prioridad 0) o un comando de control
(prioridad 2) siempre ganan frente a un bloque de firmware. La transferencia se
ralentiza, la domotica no se detiene.

## 6.3 Manifiesto

Antes de transferir nada, el gateway anuncia la campana con `OTA_START`:

| Campo | Bytes | Contenido |
|-------|------:|-----------|
| Version | 2 | mayor.menor del firmware |
| Tamano | 4 | bytes totales de la imagen |
| CRC | 2 | CRC-16 de la imagen completa |
| Hardware | 1 | identificador de familia (328P, 2560, ESP32) |
| Bloques | 2 | cantidad de bloques de 7 bytes |

El nodo valida familia de hardware y espacio disponible y responde
`OTA_READY_ACK` o `OTA_ABORT`.

## 6.4 Transferencia con ventana

- `OTA_DATA`: byte 0 = numero de secuencia, bytes 1..7 = datos. 7 bytes utiles por
  trama.
- Ventana de N bloques (16 por defecto): el emisor envia la ventana completa y
  espera.
- `OTA_WINDOW_ACK`: el receptor devuelve el numero de ventana y un mapa de bits
  con los bloques faltantes; el emisor retransmite solo esos.
- Timeout por ventana con **retroceso exponencial** (por ejemplo 100 ms, 200 ms,
  400 ms) y un maximo de reintentos antes de abortar.
- `OTA_FINISH`: el nodo verifica el CRC de la imagen completa antes de marcarla
  como valida.
- `OTA_ABORT`: cancela en cualquier momento y deja el firmware anterior intacto.

## 6.5 Maquina de estados del nodo

```
IDLE --OTA_START--> VALIDANDO --ok--> RECIBIENDO --ventana ok--> RECIBIENDO
                        |                 |                          |
                     rechazo           timeout/CRC              OTA_FINISH
                        v                 v                          v
                      IDLE            ABORTANDO -----------------> VERIFICANDO
                                                                     |
                                                          CRC ok --> APLICANDO --> reinicio
                                                          CRC mal -> IDLE (firmware anterior)
```

## 6.6 Almacenamiento de la imagen

| Plataforma | Estrategia |
|------------|-----------|
| ESP32 | Particion OTA de respaldo y `esp_ota_*`: rollback nativo |
| ATmega2560 | Bootloader propio en la seccion NRWW + buffer en EEPROM externa o escritura por paginas |
| ATmega328P | 32 KB de flash: la imagen no cabe duplicada, hay que escribir por paginas desde el bootloader, con el riesgo asumido de un corte de energia a mitad |

Para AVR el bootloader debe implementar un subconjunto minimo del protocolo
(recibir `MSG_OTA` dirigido a su Node-ID) y un temporizador de guarda que salte
a la aplicacion si no llega ninguna campana.

## 6.7 Seguridad

- CRC-16 obligatorio por imagen; opcionalmente CRC por bloque.
- Verificacion de familia de hardware antes de escribir un solo byte.
- El nodo nunca borra el firmware en ejecucion antes de haber validado el nuevo,
  salvo en 328P donde no hay espacio para las dos imagenes (documentar el riesgo
  en la instalacion).
- Nunca hacer OTA masivo simultaneo sobre el bus: una campana por nodo, o por
  grupos pequenos.
