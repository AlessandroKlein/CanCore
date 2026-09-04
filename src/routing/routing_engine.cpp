#include "routing/routing_engine.h"

#include "system/system_logger.h"

namespace pcd {

RoutingEngine::RoutingEngine(RouteTable &table)
    : table_(table),
      node_(0),
      gateway_node_id_(0),
      sink_(0),
      sink_ctx_(0) {}

void RoutingEngine::attach(CanNode &node, uint16_t gateway_node_id) {
    node_ = &node;
    gateway_node_id_ = gateway_node_id;
}

void RoutingEngine::setSink(CanonicalSink sink, void *ctx) {
    sink_ = sink;
    sink_ctx_ = ctx;
}

void RoutingEngine::onCanFrame(const CanFrame &frame) {
    const CanId id = frame.fields();
    if (id.source == gateway_node_id_) {
        return;  /* no procesar las propias tramas del gateway */
    }
    /* Solo interesan eventos, estados, escenas y heartbeat de otros nodos. */
    if (id.msg_type != MSG_EVENT && id.msg_type != MSG_STATE &&
        id.msg_type != MSG_GROUP && id.msg_type != MSG_HEARTBEAT) {
        return;
    }
    process(CanonicalFrame::fromCanFrame(frame));
}

bool RoutingEngine::process(const CanonicalFrame &frame) {
    MappingRule rule;
    if (!table_.match(frame, rule)) {
        return false;
    }
    execute(rule);
    return true;
}

void RoutingEngine::execute(const MappingRule &rule) {
    /* Construye el frame canonico de destino a partir de la regla. */
    CanonicalFrame dst;
    dst.protocol = rule.dst_protocol;
    dst.source_id = rule.dst_id;
    dst.resource = rule.dst_resource;
    dst.channel = rule.dst_channel;
    dst.action = rule.dst_action;
    dst.param = rule.dst_param;
    dst.is_command = 1;

    switch (rule.dst_protocol) {
        case PROTO_CAN: {
            if (node_ == 0) {
                LOG_ERROR("RoutingEngine: destino CAN pero sin CanNode adjunto");
                return;
            }
            if (rule.dst_id == gateway_node_id_) {
                /* Aplicar localmente en el propio gateway. */
                node_->applyLocal(rule.dst_resource, rule.dst_channel,
                                  rule.dst_action, rule.dst_param);
            } else {
                node_->sendCommand(static_cast<uint8_t>(rule.dst_id & kTargetMask),
                                   rule.dst_resource, rule.dst_channel,
                                   rule.dst_action, rule.dst_param);
            }
            break;
        }

        default:
            /* Entregar a un puente externo (MQTT, Modbus, KNX, ESP-NOW, IP...). */
            if (sink_ != 0) {
                sink_(dst, sink_ctx_);
            } else {
                LOG_ERROR("RoutingEngine: destino no CAN sin sink configurado");
            }
            break;
    }
}

}  // namespace pcd
