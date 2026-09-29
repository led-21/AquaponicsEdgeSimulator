/*
 * SPDX-FileCopyrightText: 2024-2026 AquaponicsEdgeSimulator Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "simulator.h"
#include "telemetry.h"
#include "wifi_manager.h"
#include "mqtt_client_service.h"

static const char *TAG = "aquaponics_main";

static sim_mode_t get_configured_simulation_mode(void)
{
#if defined(CONFIG_SIMULATOR_MODE_PH_DRIFT)
    return SIM_MODE_PH_DRIFT;
#elif defined(CONFIG_SIMULATOR_MODE_HIGH_TEMP)
    return SIM_MODE_HIGH_TEMP;
#elif defined(CONFIG_SIMULATOR_MODE_LOW_DO)
    return SIM_MODE_LOW_DO;
#else
    return SIM_MODE_NORMAL;
#endif
}

static void generate_iso_timestamp(char *buf, size_t buf_len)
{
    time_t now;
    struct tm timeinfo;
    time(&now);

    /* If SNTP is not synced yet, default to reproducible synthetic timestamp based on uptime */
    if (now < 1704067200) { /* Before 2024-01-01 */
        uint32_t uptime_sec = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
        uint32_t hours = (uptime_sec / 3600) % 24;
        uint32_t minutes = (uptime_sec / 60) % 60;
        uint32_t seconds = uptime_sec % 60;
        snprintf(buf, buf_len, "2026-01-01T%02u:%02u:%02uZ",
                 (unsigned)hours, (unsigned)minutes, (unsigned)seconds);
    } else {
        gmtime_r(&now, &timeinfo);
        strftime(buf, buf_len, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    }
}

static void telemetry_task(void *pvParameters)
{
    const char *device_id = CONFIG_SIMULATOR_DEVICE_ID;
    const TickType_t interval_ticks = pdMS_TO_TICKS(CONFIG_SIMULATOR_TELEMETRY_INTERVAL_SEC * 1000);
    char timestamp[32];

    ESP_LOGI(TAG, "Telemetry task started. Target interval: %d s, Device: %s",
             CONFIG_SIMULATOR_TELEMETRY_INTERVAL_SEC, device_id);

    while (1) {
        /* Run simulation step */
        aquaponics_readings_t readings = simulator_step();
        generate_iso_timestamp(timestamp, sizeof(timestamp));

        char *payload = telemetry_format_json(device_id, timestamp, &readings);
        if (payload) {
            ESP_LOGI(TAG, "Telemetry [%s]: pH=%.2f, Temp=%.2f C, EC=%.2f mS/cm, DO=%.2f mg/L",
                     simulator_mode_to_string(simulator_get_mode()),
                     readings.ph, readings.temperature, readings.ec, readings.dissolved_oxygen);

            if (mqtt_service_is_connected()) {
                esp_err_t err = mqtt_service_publish_telemetry(device_id, payload);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to publish telemetry to broker");
                }
            } else {
                ESP_LOGD(TAG, "MQTT not connected, skipping transmission");
            }

            free(payload);
        } else {
            ESP_LOGE(TAG, "Failed to format telemetry payload");
        }

        vTaskDelay(interval_ticks);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "  Aquaponics Edge Simulator (ESP32 / ESP-IDF)     ");
    ESP_LOGI(TAG, "==================================================");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "Silicon: %s (%d CPU cores, Wi-Fi%s%s), Flash: %" PRIu32 " MB",
                 CONFIG_IDF_TARGET,
                 chip_info.cores,
                 (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                 (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
                 flash_size / (1024 * 1024));
    }

    /* Initialize Non-Volatile Storage (NVS) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize physical simulation engine */
    sim_mode_t initial_mode = get_configured_simulation_mode();
    simulator_init(CONFIG_SIMULATOR_SEED, initial_mode);
    ESP_LOGI(TAG, "Simulator initialized (Mode: %s, Seed: %d)",
             simulator_mode_to_string(initial_mode), CONFIG_SIMULATOR_SEED);

    /* Initialize Wi-Fi subsystem */
    ESP_LOGI(TAG, "Starting Wi-Fi subsystem...");
    ESP_ERROR_CHECK(wifi_manager_init());

    /* Wait up to 10 seconds for Wi-Fi connection */
    ESP_LOGI(TAG, "Waiting for network connectivity...");
    if (wifi_manager_wait_connected(pdMS_TO_TICKS(10000)) == ESP_OK) {
        ESP_LOGI(TAG, "Network ready. Initializing MQTT client service...");
        ESP_ERROR_CHECK(mqtt_service_init(CONFIG_SIMULATOR_DEVICE_ID));
    } else {
        ESP_LOGW(TAG, "Wi-Fi not connected yet. Telemetry will begin and retry MQTT when online.");
        /* Initialize MQTT anyway so it auto-reconnects when network becomes available */
        mqtt_service_init(CONFIG_SIMULATOR_DEVICE_ID);
    }

    /* Launch telemetry background task */
    BaseType_t task_created = xTaskCreate(
        telemetry_task,
        "telemetry_task",
        4096,
        NULL,
        5,
        NULL
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create telemetry FreeRTOS task");
    }
}
