# Ejemplos PCD_CAN 0.6.0

Todos los ejemplos son sketches de aplicacion. Las librerias de terceros aparecen solo en `examples/` y nunca en `src/` ni en los manifiestos de PCD_CAN.

## Nodos y UI


## Puentes bidireccionales

Cada puente debe:

1. descubrir recursos mediante `MSG_DISCOVERY`,
2. configurar filtros con `addListenFilter()` o `CFG_SUBSCRIBE`,
3. traducir eventos externos a comandos CAN,
4. traducir `MSG_STATE` a estados externos,
5. evitar inventar estado cuando el nodo CAN no lo confirmó.

Ver `docs/puentes-bidireccionales.md` para el contrato y la elección de dependencias.

# Ejemplos PCD_CAN 0.6.0
- `BridgeESPNowBidireccional`: paquetes ESP-NOW <-> CAN.
