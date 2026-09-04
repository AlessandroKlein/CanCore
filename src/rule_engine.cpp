#include "rule_engine.h"

#include <string.h>

namespace pcd {

static const uint8_t kSegmentBytes = 5;
static const uint8_t kSegmentCount = kBindingRuleBytes / kSegmentBytes;
static const uint8_t kAllSegments = (1 << kSegmentCount) - 1;
static const uint8_t kAppendIndex = 0xFF;

RuleEngine::RuleEngine(CanNode &node, ConfigStore &config)
    : node_(node), config_(config), staged_mask_(0), staged_index_(kAppendIndex) {
    memset(staged_, 0, sizeof(staged_));
}

void RuleEngine::frameTrampoline(const CanFrame &frame, void *ctx) {
    static_cast<RuleEngine *>(ctx)->evaluate(frame);
}

void RuleEngine::configTrampoline(const CanFrame &frame, void *ctx) {
    static_cast<RuleEngine *>(ctx)->handleConfig(frame);
}

void RuleEngine::begin() {
    node_.setNodeId(config_.nodeId());
    node_.setNodeIdMode(config_.nodeIdMode());
    node_.onAnyFrame(&RuleEngine::frameTrampoline, this);
    node_.onConfig(&RuleEngine::configTrampoline, this);
}

bool RuleEngine::matches(const BindingRule &rule, const CanFrame &frame, const CanId &id) const {
    if (rule.source_node != kAnySource && rule.source_node != id.source) {
        return false;
    }
    if (rule.source_resource != frame.resource()) {
        return false;
    }
    if (rule.source_channel != kAnyChannel && rule.source_channel != frame.channel()) {
        return false;
    }

    switch (rule.trigger) {
        case TRIG_ANY:
            return true;
        case TRIG_EVENT:
            return id.msg_type == MSG_EVENT && frame.data[2] == rule.event;
        case TRIG_VALUE_GREATER:
            return id.msg_type == MSG_STATE && frameValue(frame) > rule.threshold;
        case TRIG_VALUE_LESS:
            return id.msg_type == MSG_STATE && frameValue(frame) < rule.threshold;
        case TRIG_VALUE_EQUAL:
            return id.msg_type == MSG_STATE && frameValue(frame) == rule.threshold;
        default:
            return false;
    }
}

void RuleEngine::execute(const BindingRule &rule) {
    node_.applyLocal(rule.target_resource, rule.target_channel, rule.action, rule.param);
}

void RuleEngine::evaluate(const CanFrame &frame) {
    const CanId id = frame.fields();
    if (id.msg_type != MSG_EVENT && id.msg_type != MSG_STATE) {
        return;
    }
    for (uint8_t i = 0; i < config_.ruleCount(); ++i) {
        const BindingRule &rule = config_.rule(i);
        if (matches(rule, frame, id)) {
            execute(rule);
        }
    }
}

void RuleEngine::sendAck(uint8_t sub_command, uint8_t status) {
    uint8_t payload[2];
    payload[0] = sub_command;
    payload[1] = status;
    node_.sendConfigAck(payload, sizeof(payload));
}

bool RuleEngine::handleConfig(const CanFrame &frame) {
    if (frame.resource() != RES_CONFIG) {
        return false;
    }
    const uint8_t sub_command = frame.channel();
    const uint8_t *payload = &frame.data[2];

    switch (sub_command) {
        case CFG_SET_NODE_ID: {
            const uint16_t new_id = readUint16BE(payload);
            if (!isValidSource(new_id)) {
                sendAck(sub_command, CFG_STATUS_BAD_REQUEST);
                return true;
            }
            config_.setNodeId(new_id);
            config_.setNodeIdMode(NODE_ID_MANUAL);
            node_.setNodeId(new_id);
            node_.setNodeIdMode(NODE_ID_MANUAL);
            sendAck(sub_command, config_.save() ? CFG_STATUS_OK : CFG_STATUS_STORAGE_ERROR);
            return true;
        }

        case CFG_SET_NODE_MODE: {
            if (payload[0] > NODE_ID_AUTOMATIC) {
                sendAck(sub_command, CFG_STATUS_BAD_REQUEST);
                return true;
            }
            const NodeIdMode mode = static_cast<NodeIdMode>(payload[0]);
            config_.setNodeIdMode(mode);
            node_.setNodeIdMode(mode);
            sendAck(sub_command, config_.save() ? CFG_STATUS_OK : CFG_STATUS_STORAGE_ERROR);
            return true;
        }

        case CFG_SUBSCRIBE: {
            const uint16_t source = readUint16BE(payload);
            if (!node_.addListenFilter(source, payload[2], payload[3])) {
                sendAck(sub_command, CFG_STATUS_FULL);
            } else {
                sendAck(sub_command, CFG_STATUS_OK);
            }
            return true;
        }

        case CFG_UNSUBSCRIBE:
            node_.clearListenFilters();
            sendAck(sub_command, CFG_STATUS_OK);
            return true;

        case CFG_CLEAR_SUBSCRIPTIONS:
            node_.clearListenFilters();
            sendAck(sub_command, CFG_STATUS_OK);
            return true;

        case CFG_RULE_BEGIN: {
            memset(staged_, 0, sizeof(staged_));
            staged_mask_ = 0;
            staged_index_ = payload[0];
            sendAck(sub_command, CFG_STATUS_OK);
            return true;
        }

        case CFG_RULE_CHUNK: {
            const uint8_t segment = payload[0];
            if (segment >= kSegmentCount) {
                sendAck(sub_command, CFG_STATUS_BAD_REQUEST);
                return true;
            }
            memcpy(&staged_[segment * kSegmentBytes], &payload[1], kSegmentBytes);
            staged_mask_ |= static_cast<uint8_t>(1 << segment);
            return true;
        }

        case CFG_RULE_COMMIT: {
            if (staged_mask_ != kAllSegments) {
                sendAck(sub_command, CFG_STATUS_BAD_REQUEST);
                return true;
            }
            if (readUint16BE(payload) != crc16(staged_, kBindingRuleBytes)) {
                staged_mask_ = 0;
                sendAck(sub_command, CFG_STATUS_CRC_ERROR);
                return true;
            }

            BindingRule rule;
            ConfigStore::deserializeRule(staged_, rule);
            staged_mask_ = 0;

            bool stored;
            if (staged_index_ < config_.ruleCount()) {
                stored = config_.replaceRule(staged_index_, rule);
            } else {
                stored = config_.addRule(rule);
            }
            if (!stored) {
                sendAck(sub_command, CFG_STATUS_FULL);
                return true;
            }
            sendAck(sub_command, config_.save() ? CFG_STATUS_OK : CFG_STATUS_STORAGE_ERROR);
            return true;
        }

        case CFG_RULE_DELETE: {
            if (!config_.removeRule(payload[0])) {
                sendAck(sub_command, CFG_STATUS_BAD_REQUEST);
                return true;
            }
            sendAck(sub_command, config_.save() ? CFG_STATUS_OK : CFG_STATUS_STORAGE_ERROR);
            return true;
        }

        case CFG_RULE_CLEAR: {
            config_.clearRules();
            sendAck(sub_command, config_.save() ? CFG_STATUS_OK : CFG_STATUS_STORAGE_ERROR);
            return true;
        }

        case CFG_SAVE: {
            sendAck(sub_command, config_.save() ? CFG_STATUS_OK : CFG_STATUS_STORAGE_ERROR);
            return true;
        }

        case CFG_RULE_COUNT: {
            uint8_t response[3];
            response[0] = CFG_RULE_COUNT;
            response[1] = CFG_STATUS_OK;
            response[2] = config_.ruleCount();
            node_.sendConfigAck(response, sizeof(response));
            return true;
        }

        default:
            return false;
    }
}

}  // namespace pcd
