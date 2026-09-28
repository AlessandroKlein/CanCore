#pragma once

/*
 * PCD v6.4 - Automatizacion distribuida: reglas, escenas y maquina de estados.
 *
 * Las automatizaciones NO dependen del gateway: un nodo puede ejecutar reglas
 * localmente (IF resource == value THEN write resource). Se complementan con
 * escenas (secuencia de acciones) y una maquina de estados por eventos.
 */

#include <stdint.h>

#include "v6/pcd_errors.h"

namespace pcd {

/* ------------------------------------------------------------------ */
/* Reglas (IF ... THEN ...)                                            */
/* ------------------------------------------------------------------ */

enum PCD_ConditionOp : uint8_t {
    PCD_OP_EQUAL = 0,
    PCD_OP_NOT_EQUAL,
    PCD_OP_GREATER,
    PCD_OP_LESS,
    PCD_OP_GREATER_EQUAL,
    PCD_OP_LESS_EQUAL
};

struct PCD_Condition {
    uint8_t resourceId;
    PCD_ConditionOp op;
    float value;

    PCD_Condition() : resourceId(0), op(PCD_OP_EQUAL), value(0.0f) {}
};

struct PCD_Action {
    uint8_t resourceId;
    uint8_t action;   /* ACT_ON / ACT_OFF / ACT_SET_VALUE */
    uint32_t param;

    PCD_Action() : resourceId(0), action(0), param(0) {}
};

struct PCD_Rule {
    PCD_Condition condition;
    PCD_Action action;
    bool enabled;

    PCD_Rule() : enabled(true) {}
};

/* Evalua una condicion contra un valor. */
bool pcdEvaluateCondition(const PCD_Condition &condition, float value);

class PCD_AutomationEngine {
  public:
    typedef void (*ActionHandler)(const PCD_Action &action, void *ctx);

    static const uint8_t kMaxRules = 16;

    PCD_AutomationEngine();

    PCD_Error addRule(const PCD_Rule &rule);
    void setActionHandler(ActionHandler handler, void *ctx);

    /* Evalua todas las reglas cuyo recurso coincida; devuelve cuantas dispararon. */
    uint8_t evaluate(uint8_t resourceId, float value);

    void clear();

  private:
    PCD_Rule rules_[kMaxRules];
    uint8_t count_;
    ActionHandler handler_;
    void *ctx_;
};

/* ------------------------------------------------------------------ */
/* Escenas (secuencia ordenada de acciones)                            */
/* ------------------------------------------------------------------ */

struct PCD_SceneStep {
    uint16_t nodeId;
    uint8_t resourceId;
    uint8_t action;
    uint32_t param;
    uint32_t delayMs;       /* espera antes de este paso */
    uint16_t transitionMs;  /* transicion/fade             */
    bool valid;
};

class PCD_Scene {
  public:
    static const uint8_t kMaxSteps = 16;

    PCD_Scene();

    PCD_Error addStep(uint16_t nodeId, uint8_t resourceId, uint8_t action, uint32_t param,
                      uint32_t delayMs = 0, uint16_t transitionMs = 0);

    uint8_t stepCount() const { return count_; }
    const PCD_SceneStep &step(uint8_t index) const { return steps_[index]; }
    void clear();

  private:
    PCD_SceneStep steps_[kMaxSteps];
    uint8_t count_;
};

/* ------------------------------------------------------------------ */
/* Maquina de estados por eventos                                      */
/* ------------------------------------------------------------------ */

class PCD_StateMachine {
  public:
    static const uint8_t kMaxTransitions = 16;

    PCD_StateMachine();

    PCD_Error addTransition(uint8_t fromState, uint8_t trigger, uint8_t toState);
    void setInitialState(uint8_t state);

    /* Dispara un evento; devuelve PCD_OK si hubo transicion. */
    PCD_Error trigger(uint8_t trigger);

    uint8_t state() const { return state_; }
    bool hasTriggered() const { return lastTriggered_; }

  private:
    struct Transition {
        uint8_t from;
        uint8_t trigger;
        uint8_t to;
        bool valid;
    };

    Transition transitions_[kMaxTransitions];
    uint8_t count_;
    uint8_t state_;
    bool lastTriggered_;
};

}  // namespace pcd
