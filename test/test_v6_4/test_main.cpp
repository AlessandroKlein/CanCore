/*
 * Pruebas de PCD v6.4: grupos, automatizacion (reglas/escenas/FSM) y scheduler.
 */

#include <unity.h>

#include "v6/pcd_v6.h"

using namespace pcd;

/* ---- captura de acciones ---- */
static PCD_Action g_last_action;
static uint8_t g_action_count = 0;

static void captureAction(const PCD_Action &action, void *) {
    g_last_action = action;
    ++g_action_count;
}

void test_group_addressing(void) {
    PCD_GroupRegistry groups;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, groups.addMember(10, 0x0001, 3));
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, groups.addMember(10, 0x0002, 3));
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, groups.addMember(11, 0x0001, 5));

    TEST_ASSERT_EQUAL_UINT8(2, groups.memberCount(10));
    TEST_ASSERT_TRUE(groups.inGroup(10, 0x0002, 3));
    TEST_ASSERT_FALSE(groups.inGroup(10, 0x0002, 4));
    TEST_ASSERT_FALSE(groups.inGroup(11, 0x0002, 3));
}

void test_automation_rule(void) {
    PCD_AutomationEngine engine;
    engine.setActionHandler(captureAction, 0);

    PCD_Rule rule;
    rule.condition.resourceId = 1; /* temperatura */
    rule.condition.op = PCD_OP_GREATER;
    rule.condition.value = 28.0f;
    rule.action.resourceId = 2; /* ventilador */
    rule.action.action = 1;      /* ON */
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, engine.addRule(rule));

    g_action_count = 0;
    TEST_ASSERT_EQUAL_UINT8(1, engine.evaluate(1, 30.0f));
    TEST_ASSERT_EQUAL_UINT8(2, g_last_action.resourceId);
    TEST_ASSERT_EQUAL_UINT8(0, engine.evaluate(1, 25.0f)); /* no dispara */
}

void test_scene_steps(void) {
    PCD_Scene scene;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, scene.addStep(0x0001, 3, 1, 0, 0, 500));
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, scene.addStep(0x0002, 4, 3, 60, 200, 0));
    TEST_ASSERT_EQUAL_UINT8(2, scene.stepCount());
    TEST_ASSERT_EQUAL_UINT16(0x0002, scene.step(1).nodeId);
    TEST_ASSERT_EQUAL_UINT32(60, scene.step(1).param);
}

void test_state_machine(void) {
    PCD_StateMachine sm;
    sm.setInitialState(0);
    sm.addTransition(0, 1, 1); /* NORMAL --evt1--> HOT */
    sm.addTransition(1, 2, 0); /* HOT --evt2--> NORMAL */
    sm.addTransition(1, 3, 2); /* HOT --evt3--> EMERGENCY */

    TEST_ASSERT_EQUAL_UINT8(0, sm.state());
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, sm.trigger(1));
    TEST_ASSERT_EQUAL_UINT8(1, sm.state());
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, sm.trigger(2));
    TEST_ASSERT_EQUAL_UINT8(0, sm.state());

    /* evento sin transicion desde el estado actual */
    TEST_ASSERT_EQUAL_UINT8(PCD_ERR_INVALID_ARGUMENT, sm.trigger(3));
}

void test_scheduler_interval(void) {
    PCD_Scheduler sched;
    sched.addInterval(1, 1000, 0);
    uint8_t id = 0;
    TEST_ASSERT_FALSE(sched.tick(999, id));
    TEST_ASSERT_TRUE(sched.tick(1000, id));
    TEST_ASSERT_EQUAL_UINT8(1, id);
    TEST_ASSERT_TRUE(sched.tick(2000, id)); /* se re-agenda */
}

void test_scheduler_after_delay(void) {
    PCD_Scheduler sched;
    sched.addAfterDelay(2, 500, 1000);
    uint8_t id = 0;
    TEST_ASSERT_FALSE(sched.tick(1499, id));
    TEST_ASSERT_TRUE(sched.tick(1500, id));
    TEST_ASSERT_EQUAL_UINT8(2, id);
    TEST_ASSERT_FALSE(sched.tick(2000, id)); /* se dispara una sola vez */
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_group_addressing);
    RUN_TEST(test_automation_rule);
    RUN_TEST(test_scene_steps);
    RUN_TEST(test_state_machine);
    RUN_TEST(test_scheduler_interval);
    RUN_TEST(test_scheduler_after_delay);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}
