/*
 * SPDX-FileCopyrightText: 2024-2026 AquaponicsEdgeSimulator Contributors
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AQUAPONICS_MQTT_CLIENT_SERVICE_H
#define AQUAPONICS_MQTT_CLIENT_SERVICE_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the MQTT client service.
 *
 * Configures broker URI, credentials (from Kconfig), LWT (Last Will and Testament)
 * on "aquaponics/devices/{deviceId}/status", and registers event callbacks.
 *
 * @param device_id Unique identifier of this edge device.
 * @return ESP_OK on success, or appropriate error code.
 */
esp_err_t mqtt_service_init(const char *device_id);

/**
 * @brief Publish telemetry JSON to "aquaponics/devices/{deviceId}/telemetry".
 *
 * @param device_id Device identifier.
 * @param payload JSON formatted telemetry payload.
 * @return ESP_OK if message queued/sent successfully, ESP_FAIL otherwise.
 */
esp_err_t mqtt_service_publish_telemetry(const char *device_id, const char *payload);

/**
 * @brief Publish device status to "aquaponics/devices/{deviceId}/status".
 *
 * @param device_id Device identifier.
 * @param status Status string ("online", "offline").
 * @param reason Optional reason string or NULL.
 * @param retain Whether to retain this message on the broker.
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t mqtt_service_publish_status(const char *device_id, const char *status, const char *reason, bool retain);

/**
 * @brief Check if MQTT client is currently connected to the broker.
 *
 * @return true if connected, false otherwise.
 */
bool mqtt_service_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // AQUAPONICS_MQTT_CLIENT_SERVICE_H
