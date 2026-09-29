/*
 * SPDX-FileCopyrightText: 2024-2026 AquaponicsEdgeSimulator Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../main/include/simulator.h"
#include "../main/include/telemetry.h"
#include "cJSON.h"

/* Simple test framework macros */
static int tests_run = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s:%d: %s (%s)\n", __FILE__, __LINE__, msg, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    printf("Running %s...\n", #fn); \
    int prev_failed = tests_failed; \
    fn(); \
    tests_run++; \
    if (tests_failed == prev_failed) { \
        printf("  [PASS] %s\n", #fn); \
    } \
} while(0)

/* Test 1: Deterministic initial state */
static void test_simulator_initialization(void)
{
    simulator_init(42, SIM_MODE_NORMAL);
    aquaponics_readings_t r = simulator_get_current_readings();

    TEST_ASSERT(fabsf(r.ph - 6.80f) < 0.001f, "Initial pH should be 6.80");
    TEST_ASSERT(fabsf(r.temperature - 23.50f) < 0.001f, "Initial temperature should be 23.50");
    TEST_ASSERT(fabsf(r.ec - 1.50f) < 0.001f, "Initial EC should be 1.50");
    TEST_ASSERT(fabsf(r.dissolved_oxygen - 7.20f) < 0.001f, "Initial DO should be 7.20");
    TEST_ASSERT(simulator_get_mode() == SIM_MODE_NORMAL, "Initial mode should be NORMAL");
}

/* Test 2: Clamp function strictly enforces physical bounds */
static void test_simulator_clamping(void)
{
    TEST_ASSERT(simulator_clamp(-5.0f, 0.0f, 14.0f) == 0.0f, "Clamp below min");
    TEST_ASSERT(simulator_clamp(18.5f, 0.0f, 14.0f) == 14.0f, "Clamp above max");
    TEST_ASSERT(simulator_clamp(7.1f, 0.0f, 14.0f) == 7.1f, "Clamp within range");
}

/* Test 3: Simulation steps remain within physical limits across 1000 iterations */
static void test_simulator_range_enforcement(void)
{
    sim_mode_t modes[] = {SIM_MODE_NORMAL, SIM_MODE_PH_DRIFT, SIM_MODE_HIGH_TEMP, SIM_MODE_LOW_DO};

    for (int m = 0; m < 4; m++) {
        simulator_init(100 + m, modes[m]);
        for (int i = 0; i < 500; i++) {
            aquaponics_readings_t r = simulator_step();
            TEST_ASSERT(r.ph >= SIM_PH_MIN && r.ph <= SIM_PH_MAX, "pH within physical bounds");
            TEST_ASSERT(r.temperature >= SIM_TEMP_MIN && r.temperature <= SIM_TEMP_MAX, "Temp within bounds");
            TEST_ASSERT(r.ec >= SIM_EC_MIN && r.ec <= SIM_EC_MAX, "EC within bounds");
            TEST_ASSERT(r.dissolved_oxygen >= SIM_DO_MIN && r.dissolved_oxygen <= SIM_DO_MAX, "DO within bounds");
        }
    }
}

/* Test 4: Directional trend in PH_DRIFT mode */
static void test_simulator_ph_drift_trend(void)
{
    simulator_init(42, SIM_MODE_PH_DRIFT);
    aquaponics_readings_t initial = simulator_get_current_readings();

    for (int i = 0; i < 50; i++) {
        simulator_step();
    }
    aquaponics_readings_t after = simulator_get_current_readings();

    TEST_ASSERT(after.ph < initial.ph, "pH must trend downwards during nitrification drift");
}

/* Test 5: Directional trend in HIGH_TEMP mode */
static void test_simulator_high_temp_trend(void)
{
    simulator_init(42, SIM_MODE_HIGH_TEMP);
    aquaponics_readings_t initial = simulator_get_current_readings();

    for (int i = 0; i < 30; i++) {
        simulator_step();
    }
    aquaponics_readings_t after = simulator_get_current_readings();

    TEST_ASSERT(after.temperature > initial.temperature, "Temperature must rise during thermal spike");
    TEST_ASSERT(after.dissolved_oxygen < initial.dissolved_oxygen, "DO must decrease as water warms");
}

/* Test 6: Directional trend in LOW_DO mode */
static void test_simulator_low_do_trend(void)
{
    simulator_init(42, SIM_MODE_LOW_DO);
    aquaponics_readings_t initial = simulator_get_current_readings();

    for (int i = 0; i < 30; i++) {
        simulator_step();
    }
    aquaponics_readings_t after = simulator_get_current_readings();

    TEST_ASSERT(after.dissolved_oxygen < initial.dissolved_oxygen, "DO must drop during aeration failure");
}

/* Test 7: String to Enum and Enum to String conversions */
static void test_simulator_mode_string_conversions(void)
{
    TEST_ASSERT(strcmp(simulator_mode_to_string(SIM_MODE_NORMAL), "NORMAL") == 0, "Enum to string NORMAL");
    TEST_ASSERT(strcmp(simulator_mode_to_string(SIM_MODE_PH_DRIFT), "PH_DRIFT") == 0, "Enum to string PH_DRIFT");
    TEST_ASSERT(strcmp(simulator_mode_to_string(SIM_MODE_HIGH_TEMP), "HIGH_TEMPERATURE") == 0, "Enum to string HIGH_TEMP");
    TEST_ASSERT(strcmp(simulator_mode_to_string(SIM_MODE_LOW_DO), "LOW_DISSOLVED_OXYGEN") == 0, "Enum to string LOW_DO");

    TEST_ASSERT(simulator_mode_from_string("NORMAL") == SIM_MODE_NORMAL, "String to enum NORMAL");
    TEST_ASSERT(simulator_mode_from_string("PH_DRIFT") == SIM_MODE_PH_DRIFT, "String to enum PH_DRIFT");
    TEST_ASSERT(simulator_mode_from_string("HIGH_TEMPERATURE") == SIM_MODE_HIGH_TEMP, "String to enum HIGH_TEMPERATURE");
    TEST_ASSERT(simulator_mode_from_string("LOW_DISSOLVED_OXYGEN") == SIM_MODE_LOW_DO, "String to enum LOW_DISSOLVED_OXYGEN");
    TEST_ASSERT(simulator_mode_from_string("UNKNOWN_STRING") == SIM_MODE_NORMAL, "Fallback to NORMAL on unknown");
}

/* Test 8: MQTT Topic Generation */
static void test_telemetry_topics(void)
{
    char buf[128];
    int res = telemetry_build_topic(buf, sizeof(buf), "esp32-sim-01", "telemetry");
    TEST_ASSERT(res > 0, "Build telemetry topic success");
    TEST_ASSERT(strcmp(buf, "aquaponics/devices/esp32-sim-01/telemetry") == 0, "Telemetry topic format");

    res = telemetry_build_topic(buf, sizeof(buf), "esp32-sim-01", "status");
    TEST_ASSERT(res > 0, "Build status topic success");
    TEST_ASSERT(strcmp(buf, "aquaponics/devices/esp32-sim-01/status") == 0, "Status topic format");

    /* Buffer overflow safety */
    char small_buf[10];
    res = telemetry_build_topic(small_buf, sizeof(small_buf), "esp32-sim-01", "telemetry");
    TEST_ASSERT(res == -1, "Buffer overflow should return -1");

    /* Null pointer safety */
    TEST_ASSERT(telemetry_build_topic(NULL, 10, "id", "telemetry") == -1, "Null buffer handling");
    TEST_ASSERT(telemetry_build_topic(buf, sizeof(buf), NULL, "telemetry") == -1, "Null device id handling");
}

/* Test 9: Telemetry JSON formatting */
static void test_telemetry_json_serialization(void)
{
    aquaponics_readings_t readings = {
        .ph = 6.82f,
        .temperature = 24.15f,
        .ec = 1.48f,
        .dissolved_oxygen = 7.15f
    };

    char *json = telemetry_format_json("esp32-sim-01", "2026-01-01T12:00:00Z", &readings);
    TEST_ASSERT(json != NULL, "JSON formatting returned non-null");

    /* Parse back to validate structural conformity */
    cJSON *root = cJSON_Parse(json);
    TEST_ASSERT(root != NULL, "Generated string must be valid JSON");

    cJSON *dev_id = cJSON_GetObjectItem(root, "deviceId");
    TEST_ASSERT(dev_id != NULL && cJSON_IsString(dev_id), "deviceId exists and is string");
    TEST_ASSERT(strcmp(dev_id->valuestring, "esp32-sim-01") == 0, "deviceId value match");

    cJSON *ts = cJSON_GetObjectItem(root, "timestamp");
    TEST_ASSERT(ts != NULL && cJSON_IsString(ts), "timestamp exists and is string");
    TEST_ASSERT(strcmp(ts->valuestring, "2026-01-01T12:00:00Z") == 0, "timestamp value match");

    cJSON *readings_obj = cJSON_GetObjectItem(root, "readings");
    TEST_ASSERT(readings_obj != NULL && cJSON_IsObject(readings_obj), "readings exists and is object");

    cJSON *ph = cJSON_GetObjectItem(readings_obj, "ph");
    TEST_ASSERT(ph != NULL && cJSON_IsNumber(ph), "ph is number");
    TEST_ASSERT(fabs(ph->valuedouble - 6.82) < 0.01, "ph value match");

    cJSON *temp = cJSON_GetObjectItem(readings_obj, "temperature");
    TEST_ASSERT(temp != NULL && cJSON_IsNumber(temp), "temperature is number");
    TEST_ASSERT(fabs(temp->valuedouble - 24.15) < 0.01, "temperature value match");

    cJSON *ec = cJSON_GetObjectItem(readings_obj, "ec");
    TEST_ASSERT(ec != NULL && cJSON_IsNumber(ec), "ec is number");
    TEST_ASSERT(fabs(ec->valuedouble - 1.48) < 0.01, "ec value match");

    cJSON *d_o = cJSON_GetObjectItem(readings_obj, "dissolvedOxygen");
    TEST_ASSERT(d_o != NULL && cJSON_IsNumber(d_o), "dissolvedOxygen is number");
    TEST_ASSERT(fabs(d_o->valuedouble - 7.15) < 0.01, "dissolvedOxygen value match");

    /* Ensure no legacy proprietary fields are present */
    TEST_ASSERT(cJSON_GetObjectItem(root, "OwnerId") == NULL, "JSON must NOT contain OwnerId");
    TEST_ASSERT(cJSON_GetObjectItem(root, "SensorId") == NULL, "JSON must NOT contain SensorId");
    TEST_ASSERT(cJSON_GetObjectItem(root, "SystemId") == NULL, "JSON must NOT contain SystemId");

    cJSON_Delete(root);
    free(json);
}

/* Test 10: Status JSON formatting */
static void test_telemetry_status_json_serialization(void)
{
    char *online_json = telemetry_format_status_json("esp32-sim-01", "online", NULL);
    TEST_ASSERT(online_json != NULL, "Online status non-null");
    TEST_ASSERT(strstr(online_json, "\"status\":\"online\"") != NULL, "Status online");
    free(online_json);

    char *offline_json = telemetry_format_status_json("esp32-sim-01", "offline", "unexpected_disconnect");
    TEST_ASSERT(offline_json != NULL, "Offline status non-null");
    TEST_ASSERT(strstr(offline_json, "\"status\":\"offline\"") != NULL, "Status offline");
    TEST_ASSERT(strstr(offline_json, "\"reason\":\"unexpected_disconnect\"") != NULL, "Reason included");
    free(offline_json);
}

int main(void)
{
    printf("========================================\n");
    printf("  AquaponicsEdgeSimulator Test Suite   \n");
    printf("========================================\n");

    RUN_TEST(test_simulator_initialization);
    RUN_TEST(test_simulator_clamping);
    RUN_TEST(test_simulator_range_enforcement);
    RUN_TEST(test_simulator_ph_drift_trend);
    RUN_TEST(test_simulator_high_temp_trend);
    RUN_TEST(test_simulator_low_do_trend);
    RUN_TEST(test_simulator_mode_string_conversions);
    RUN_TEST(test_telemetry_topics);
    RUN_TEST(test_telemetry_json_serialization);
    RUN_TEST(test_telemetry_status_json_serialization);

    printf("========================================\n");
    printf("Tests Run: %d | Passed: %d | Failed: %d\n",
           tests_run, tests_run - tests_failed, tests_failed);
    printf("========================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
