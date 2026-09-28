#include "v6/pcd_automation.h"

namespace pcd {

bool pcdEvaluateCondition(const PCD_Condition &condition, float value) {
    switch (condition.op) {
        case PCD_OP_EQUAL: return value == condition.value;
        case PCD_OP_NOT_EQUAL: return value != condition.value;
        case PCD_OP_GREATER: return value > condition.value;
        case PCD_OP_LESS: return value < condition.value;
        case PCD_OP_GREATER_EQUAL: return value >= condition.value;
        case PCD_OP_LESS_EQUAL: return value <= condition.value;
        default: return false;
    }
}

PCD_AutomationEngine::PCD_AutomationEngine() : count_(0), handler_(0), ctx_(0) {}

PCD_Error PCD_AutomationEngine::addRule(const PCD_Rule &rule) {
    if (count_ >= kMaxRules) {
        return PCD_ERR_NO_MEMORY;
    }
    rules_[count_++] = rule;
    return PCD_OK;
}

void PCD_AutomationEngine::setActionHandler(ActionHandler handler, void *ctx) {
    handler_ = handler;
    ctx_ = ctx;
}

uint8_t PCD_AutomationEngine::evaluate(uint8_t resourceId, float value) {
    uint8_t fired = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        const PCD_Rule &rule = rules_[i];
        if (rule.enabled && rule.condition.resourceId == resourceId &&
            pcdEvaluateCondition(rule.condition, value)) {
            ++fired;
            if (handler_ != 0) {
                handler_(rule.action, ctx_);
            }
        }
    }
    return fired;
}

void PCD_AutomationEngine::clear() {
    count_ = 0;
}

PCD_Scene::PCD_Scene() : count_(0) {}

PCD_Error PCD_Scene::addStep(uint16_t nodeId, uint8_t resourceId, uint8_t action, uint32_t param,
                             uint32_t delayMs, uint16_t transitionMs) {
    if (count_ >= kMaxSteps) {
        return PCD_ERR_NO_MEMORY;
    }
    PCD_SceneStep &s = steps_[count_++];
    s.nodeId = nodeId;
    s.resourceId = resourceId;
    s.action = action;
    s.param = param;
    s.delayMs = delayMs;
    s.transitionMs = transitionMs;
    s.valid = true;
    return PCD_OK;
}

void PCD_Scene::clear() {
    count_ = 0;
}

PCD_StateMachine::PCD_StateMachine() : count_(0), state_(0), lastTriggered_(false) {}

PCD_Error PCD_StateMachine::addTransition(uint8_t fromState, uint8_t trigger, uint8_t toState) {
    if (count_ >= kMaxTransitions) {
        return PCD_ERR_NO_MEMORY;
    }
    Transition &t = transitions_[count_++];
    t.from = fromState;
    t.trigger = trigger;
    t.to = toState;
    t.valid = true;
    return PCD_OK;
}

void PCD_StateMachine::setInitialState(uint8_t state) {
    state_ = state;
    lastTriggered_ = false;
}

PCD_Error PCD_StateMachine::trigger(uint8_t trigger) {
    lastTriggered_ = false;
    for (uint8_t i = 0; i < count_; ++i) {
        const Transition &t = transitions_[i];
        if (t.valid && t.from == state_ && t.trigger == trigger) {
            state_ = t.to;
            lastTriggered_ = true;
            return PCD_OK;
        }
    }
    return PCD_ERR_INVALID_ARGUMENT; /* sin transicion para este evento */
}

}  // namespace pcd
