#include "bridge/bridge_mqtt.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "can_protocol.h" /* writeUint16BE, writeUint32BE */

namespace pcd {

static const char kPrefix[] = "pcd/";
static const char kSetSuffix[] = "/set";

/* Formatea un float a 2 decimales con precision limitada. */
static void formatValue(float value, char *out, size_t len) {
    if (len == 0) return;
    snprintf(out, len, "%.2f", static_cast<double>(value));
}

/* Extrae un segmento numerico decimal del topic y avanza el puntero. */
static bool parseSegment(const char *&cur, uint16_t &out) {
    /* Saltar separadores */
    while (*cur == '/' || *cur == '\0') ++cur;
    uint32_t value = 0;
    bool any = false;
    while (*cur >= '0' && *cur <= '9') {
        value = value * 10 + static_cast<uint32_t>(*cur - '0');
        ++cur;
        any = true;
    }
    if (!any) return false;
    out = static_cast<uint16_t>(value & 0xFFFF);
    return true;
}

MqttBridge::MqttBridge(IMqttTransport &transport)
    : transport_(transport), pending_count_(0) {}

void MqttBridge::onIncomingMessage(const char *topic, const uint8_t *data, size_t len) {
    CanonicalFrame incoming;
    if (!parseTopic(topic, incoming)) {
        return;
    }

    if (data != 0 && len > 0) {
        char payload[32];
        const size_t copy_len = (len < sizeof(payload) - 1) ? len : sizeof(payload) - 1;
        memcpy(payload, data, copy_len);
        payload[copy_len] = '\0';

        const char *value = payload;
        while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') {
            ++value;
        }

        const size_t value_len = strlen(value);
        if (value_len > 0) {
            if (strcmp(value, "on") == 0 || strcmp(value, "ON") == 0 || strcmp(value, "true") == 0 ||
                strcmp(value, "1") == 0) {
                incoming.action = ACT_ON;
                incoming.param = 0;
            } else if (strcmp(value, "off") == 0 || strcmp(value, "OFF") == 0 ||
                       strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
                incoming.action = ACT_OFF;
                incoming.param = 0;
            } else if (strcmp(value, "toggle") == 0 || strcmp(value, "TOGGLE") == 0) {
                incoming.action = ACT_TOGGLE;
                incoming.param = 0;
            } else {
                char *end = 0;
                const double numeric = strtod(value, &end);
                if (end != value) {
                    incoming.action = ACT_SET_VALUE;
                    incoming.param = static_cast<uint32_t>(numeric);
                } else {
                    incoming.action = ACT_TOGGLE;
                    incoming.param = 0;
                }
            }
        }
    } else {
        incoming.action = ACT_TOGGLE;
        incoming.param = 0;
    }

    if (pending_count_ < kMqttQueueDepth) {
        pending_[pending_count_++] = incoming;
    }
}

bool MqttBridge::process(const CanonicalFrame &frame) {
    if (transport_.publish == 0 || frame.is_command) {
        /* Solo publicamos estados/telemetria hacia afuera. */
        return false;
    }
    char topic[48];
    snprintf(topic, sizeof(topic), "%s%u/%02X/%u/state",
             kPrefix, frame.source_id, frame.resource, frame.channel);

    char value_str[16];
    formatValue(frame.value, value_str, sizeof(value_str));

    return transport_.publish(topic, reinterpret_cast<const uint8_t *>(value_str),
                              strlen(value_str), true /* retain */);
}

bool MqttBridge::buildCanonical(CanonicalFrame &out) {
    if (pending_count_ == 0) {
        return false;
    }
    out = pending_[0];
    for (uint8_t i = 0; i + 1 < pending_count_; ++i) {
        pending_[i] = pending_[i + 1];
    }
    --pending_count_;
    return true;
}

bool MqttBridge::available() const {
    return pending_count_ > 0;
}

bool MqttBridge::parseTopic(const char *topic, CanonicalFrame &out) const {
    if (strncmp(topic, kPrefix, sizeof(kPrefix) - 1) != 0) {
        return false;
    }
    const char *cur = topic + sizeof(kPrefix) - 1;

    /* <node_id>/<recurso>/<canal>/set */
    uint16_t node_id = 0;
    uint16_t resource = 0;
    uint16_t channel = 0;
    if (!parseSegment(cur, node_id)) return false;
    if (!parseSegment(cur, resource)) return false;
    if (!parseSegment(cur, channel)) return false;
    if (strcmp(cur, kSetSuffix) != 0) return false;

    out.protocol = PROTO_MQTT;
    out.source_id = node_id;
    out.resource = static_cast<uint8_t>(resource & 0xFF);
    out.channel = static_cast<uint8_t>(channel & 0xFF);
    out.action = ACT_TOGGLE;
    out.param = 0;
    out.is_command = 1;
    return true;
}

bool MqttBridge::publishDiscovery(uint16_t node_id, uint8_t resource, uint8_t channel,
                                  const char *name) {
    if (transport_.publish == 0) {
        return false;
    }
    const char *component = resourceComponentName(resource);
    char topic[80];
    char payload[128];
    snprintf(topic, sizeof(topic), "homeassistant/%s/pcd_%u_%u_%u/config",
             component, node_id, resource, channel);
    snprintf(payload, sizeof(payload),
             "{\"name\":\"%s\",\"uniq_id\":\"pcd_%u_%u_%u\","
             "\"state_topic\":\"pcd/%u/%02X/%u/state\","
             "\"command_topic\":\"pcd/%u/%02X/%u/set\"}",
             name ? name : "PCD", node_id, resource, channel,
             node_id, resource, channel, node_id, resource, channel);
    return transport_.publish(topic, reinterpret_cast<const uint8_t *>(payload),
                              strlen(payload), false);
}

bool MqttBridge::publishStatus(uint16_t node_id, bool online) {
    if (transport_.publish == 0) {
        return false;
    }
    char topic[32];
    snprintf(topic, sizeof(topic), "%s%u/status", kPrefix, node_id);
    const char *status = online ? "online" : "offline";
    return transport_.publish(topic, reinterpret_cast<const uint8_t *>(status),
                              strlen(status), true);
}

const char *resourceComponentName(uint8_t resource) {
    switch (resource) {
        case RES_RELAY: return "switch";
        case RES_DIMMER: return "light";
        case RES_COVER: return "cover";
        case RES_ENV_SENSOR:
        case RES_GAS_SENSOR:
        case RES_POWER_SENSOR:
            return "sensor";
        default:
            return "binary_sensor";
    }
}

}  // namespace pcd
