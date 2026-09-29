/*
 * SPDX-FileCopyrightText: 2024-2026 AquaponicsEdgeSimulator Contributors
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mqtt_client_service.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "telemetry.h"
#include "sdkconfig.h"

static const char *TAG = "mqtt_service";

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_mqtt_connected = false;
static char s_device_id[64] = "esp32-sim-01";
static char s_lwt_topic[TELEMETRY_TOPIC_MAX_LEN] = {0};
static char s_lwt_msg[128] = {0};

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to MQTT broker: %s", CONFIG_MQTT_BROKER_URI);
        s_mqtt_connected = true;
        /* Announce online status as retained message */
        mqtt_service_publish_status(s_device_id, "online", NULL, true);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from MQTT broker. Reconnecting...");
        s_mqtt_connected = false;
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT message published successfully, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT event error encountered");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Transport error code: 0x%x", event->error_handle->esp_transport_sock_errno);
        }
        break;

    default:
        ESP_LOGD(TAG, "MQTT event id:%" PRIi32, event_id);
        break;
    }
}

esp_err_t mqtt_service_init(const char *device_id)
{
    if (device_id) {
        strncpy(s_device_id, device_id, sizeof(s_device_id) - 1);
        s_device_id[sizeof(s_device_id) - 1] = '\0';
    }

    /* Build LWT topic and message */
    telemetry_build_topic(s_lwt_topic, sizeof(s_lwt_topic), s_device_id, "status");
    char *lwt_json = telemetry_format_status_json(s_device_id, "offline", "unexpected_disconnect");
    if (lwt_json) {
        strncpy(s_lwt_msg, lwt_json, sizeof(s_lwt_msg) - 1);
        s_lwt_msg[sizeof(s_lwt_msg) - 1] = '\0';
        free(lwt_json);
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = CONFIG_MQTT_BROKER_URI,
            },
        },
        .credentials = {
            .username = (strlen(CONFIG_MQTT_USERNAME) > 0) ? CONFIG_MQTT_USERNAME : NULL,
            .authentication = {
                .password = (strlen(CONFIG_MQTT_PASSWORD) > 0) ? CONFIG_MQTT_PASSWORD : NULL,
            },
        },
        .session = {
            .last_will = {
                .topic = s_lwt_topic,
                .msg = s_lwt_msg,
                .msg_len = strlen(s_lwt_msg),
                .qos = 1,
                .retain = 1,
            },
        },
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_client));

    ESP_LOGI(TAG, "MQTT client service started. Broker: %s", CONFIG_MQTT_BROKER_URI);
    return ESP_OK;
}

esp_err_t mqtt_service_publish_telemetry(const char *device_id, const char *payload)
{
    if (!s_client || !payload) {
        return ESP_ERR_INVALID_ARG;
    }

    char topic[TELEMETRY_TOPIC_MAX_LEN];
    if (telemetry_build_topic(topic, sizeof(topic), device_id, "telemetry") < 0) {
        ESP_LOGE(TAG, "Failed to build telemetry topic");
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Failed to publish telemetry to %s", topic);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t mqtt_service_publish_status(const char *device_id, const char *status, const char *reason, bool retain)
{
    if (!s_client || !status) {
        return ESP_ERR_INVALID_ARG;
    }

    char topic[TELEMETRY_TOPIC_MAX_LEN];
    if (telemetry_build_topic(topic, sizeof(topic), device_id, "status") < 0) {
        ESP_LOGE(TAG, "Failed to build status topic");
        return ESP_FAIL;
    }

    char *json_status = telemetry_format_status_json(device_id, status, reason);
    if (!json_status) {
        return ESP_ERR_NO_MEM;
    }

    int msg_id = esp_mqtt_client_publish(s_client, topic, json_status, 0, 1, retain ? 1 : 0);
    free(json_status);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Failed to publish status to %s", topic);
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool mqtt_service_is_connected(void)
{
    return s_mqtt_connected;
}
