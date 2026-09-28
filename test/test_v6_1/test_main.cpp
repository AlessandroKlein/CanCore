/*
 * Pruebas de PCD v6.1 (robustez y confiabilidad):
 * configuracion remota con commit/rollback, device shadow y alarm manager.
 */

#include <unity.h>

#include "v6/pcd_v6.h"

using namespace pcd;

/* ------------------------------------------------------------------ */
/* Configuracion remota                                                */
/* ------------------------------------------------------------------ */

void test_config_stage_commit(void) {
    PCD_ConfigManager cfg;
    cfg.setDefault(PCD_CFG_NODE_ID, 0x0012);
    cfg.setDefault(PCD_CFG_BITRATE, 500000);

    TEST_ASSERT_EQUAL_UINT8(PCD_OK, cfg.stageSet(PCD_CFG_NODE_ID, 0x0042));
    TEST_ASSERT_EQUAL_UINT8(PCD_CFG_STATE_STAGING, cfg.state());
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, cfg.apply());
    TEST_ASSERT_EQUAL_UINT8(PCD_CFG_STATE_APPLIED, cfg.state());
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, cfg.commit());
    TEST_ASSERT_EQUAL_UINT8(PCD_CFG_STATE_COMMITTED, cfg.state());

    uint32_t value = 0;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, cfg.get(PCD_CFG_NODE_ID, value));
    TEST_ASSERT_EQUAL_UINT32(0x0042, value);
    TEST_ASSERT_FALSE(cfg.hasPending());
}

void test_config_rollback_keeps_active(void) {
    PCD_ConfigManager cfg;
    cfg.setDefault(PCD_CFG_NODE_ID, 0x0012);
    cfg.stageSet(PCD_CFG_NODE_ID, 0x0042);
    cfg.apply();
    cfg.commit();

    /* Nueva configuracion que falla en la fase TEST. */
    cfg.stageSet(PCD_CFG_NODE_ID, 0x0099);
    cfg.apply();
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, cfg.rollback());
    TEST_ASSERT_EQUAL_UINT8(PCD_CFG_STATE_ROLLED_BACK, cfg.state());

    uint32_t value = 0;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, cfg.get(PCD_CFG_NODE_ID, value));
    TEST_ASSERT_EQUAL_UINT32(0x0042, value); /* activa intacta */
    TEST_ASSERT_FALSE(cfg.hasPending());
}

void test_config_factory_reset(void) {
    PCD_ConfigManager cfg;
    cfg.setDefault(PCD_CFG_NODE_ID, 0x0012);
    cfg.stageSet(PCD_CFG_NODE_ID, 0x0042);
    cfg.apply();
    cfg.commit();

    TEST_ASSERT_EQUAL_UINT8(PCD_OK, cfg.factoryReset());
    uint32_t value = 0;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, cfg.get(PCD_CFG_NODE_ID, value));
    TEST_ASSERT_EQUAL_UINT32(0x0012, value); /* vuelve al default */
    TEST_ASSERT_EQUAL_UINT8(PCD_CFG_STATE_IDLE, cfg.state());
}

void test_config_invalid_key(void) {
    PCD_ConfigManager cfg;
    TEST_ASSERT_EQUAL_UINT8(PCD_ERR_INVALID_ARGUMENT,
                            cfg.stageSet(static_cast<PCD_ConfigKey>(0xFF), 1));
}

/* ------------------------------------------------------------------ */
/* Device Shadow                                                       */
/* ------------------------------------------------------------------ */

void test_shadow_pending_and_sync(void) {
    PCD_DeviceShadow shadow;

    /* Deseo ON, nodo no reporta -> pendiente. */
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, shadow.setDesired(0x0C, 4, true));
    TEST_ASSERT_TRUE(shadow.isPending(0x0C, 4));
    TEST_ASSERT_FALSE(shadow.isSynced(0x0C, 4));

    /* Nodo reporta ON -> sincronizado. */
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, shadow.setReported(0x0C, 4, true));
    TEST_ASSERT_FALSE(shadow.isPending(0x0C, 4));
    TEST_ASSERT_TRUE(shadow.isSynced(0x0C, 4));
}

void test_shadow_offline_sync(void) {
    PCD_DeviceShadow shadow;

    /* Nodo offline: deseo queda pendiente. */
    shadow.setDesired(0x0C, 1, true);
    shadow.setDesired(0x0C, 2, false);
    TEST_ASSERT_EQUAL_UINT(2, shadow.pendingCount());

    /* El nodo vuelve y reporta su estado real. */
    shadow.setReported(0x0C, 1, true);
    TEST_ASSERT_EQUAL_UINT(1, shadow.pendingCount()); /* queda el canal 2 */
    shadow.setReported(0x0C, 2, false);
    TEST_ASSERT_EQUAL_UINT(0, shadow.pendingCount());
}
/* ------------------------------------------------------------------ */
/* Alarm Manager                                                       */
/* ------------------------------------------------------------------ */

void test_alarm_escalation(void) {
    PCD_AlarmManager mgr;
    TEST_ASSERT_EQUAL_UINT8(PCD_OK, mgr.addAlarm(1, 35.0f, 40.0f, 45.0f, 1.0f, 0));

    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_NORMAL, mgr.evaluate(1, 30.0f, 0));
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_WARNING, mgr.evaluate(1, 36.0f, 0));
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_ALARM, mgr.evaluate(1, 41.0f, 0));
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_CRITICAL, mgr.evaluate(1, 46.0f, 0));
}

void test_alarm_hysteresis(void) {
    PCD_AlarmManager mgr;
    mgr.addAlarm(1, 35.0f, 40.0f, 45.0f, 1.0f, 0);

    mgr.evaluate(1, 46.0f, 0); /* CRITICAL */
    mgr.evaluate(1, 41.0f, 0); /* ALARM */
    /* 34.5 sigue dentro de la banda de histeresis (35 - 1 = 34). */
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_ALARM, mgr.evaluate(1, 34.5f, 0));
    /* Por debajo de la banda -> CLEARED. */
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_CLEARED, mgr.evaluate(1, 33.0f, 0));
}

void test_alarm_debounce(void) {
    PCD_AlarmManager mgr;
    mgr.addAlarm(1, 35.0f, 40.0f, 45.0f, 1.0f, 1000);

    /* Pico instantaneo no dispara hasta cumplir el debounce. */
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_NORMAL, mgr.evaluate(1, 50.0f, 0));
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_NORMAL, mgr.evaluate(1, 50.0f, 999));
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_CRITICAL, mgr.evaluate(1, 50.0f, 1000));
}

void test_alarm_acknowledge_and_clear(void) {
    PCD_AlarmManager mgr;
    mgr.addAlarm(1, 35.0f, 40.0f, 45.0f, 1.0f, 0);

    mgr.evaluate(1, 46.0f, 0);
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_CRITICAL, mgr.state(1));
    TEST_ASSERT_EQUAL_UINT(1, mgr.activeAlarmCount());

    TEST_ASSERT_EQUAL_UINT8(PCD_OK, mgr.acknowledge(1));
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_ACKNOWLEDGED, mgr.state(1));

    TEST_ASSERT_EQUAL_UINT8(PCD_OK, mgr.clear(1));
    TEST_ASSERT_EQUAL_UINT8(PCD_ALARM_NORMAL, mgr.state(1));
    TEST_ASSERT_EQUAL_UINT(0, mgr.activeAlarmCount());
}

void test_alarm_state_names(void) {
    TEST_ASSERT_EQUAL_STRING("CRITICAL", pcdAlarmStateName(PCD_ALARM_CRITICAL));
    TEST_ASSERT_EQUAL_STRING("NORMAL", pcdAlarmStateName(PCD_ALARM_NORMAL));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_config_stage_commit);
    RUN_TEST(test_config_rollback_keeps_active);
    RUN_TEST(test_config_factory_reset);
    RUN_TEST(test_config_invalid_key);
    RUN_TEST(test_shadow_pending_and_sync);
    RUN_TEST(test_shadow_offline_sync);
    RUN_TEST(test_alarm_escalation);
    RUN_TEST(test_alarm_hysteresis);
    RUN_TEST(test_alarm_debounce);
    RUN_TEST(test_alarm_acknowledge_and_clear);
    RUN_TEST(test_alarm_state_names);
    return UNITY_END();
}

void setUp() {}
void tearDown() {}

