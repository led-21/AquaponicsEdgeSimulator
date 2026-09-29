/*
 * SPDX-FileCopyrightText: 2024-2026 AquaponicsEdgeSimulator Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AQUAPONICS_TELEMETRY_H
#define AQUAPONICS_TELEMETRY_H

#include <stddef.h>
#include <stdbool.h>
#include "simulator.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TELEMETRY_TOPIC_MAX_LEN 128

/**
 * @brief Format aquaponics sensor readings into standard JSON telemetry payload.
 *
 * Payload structure:
 * {
 *   "deviceId": "esp32-sim-01",
 *   "timestamp": "2026-01-01T12:00:00Z",
 *   "readings": {
 *     "ph": 6.82,
 *     "temperature": 24.10,
 *     "ec": 1.48,
 *     "dissolvedOxygen": 7.15
 *   }
 * }
 *
 * @param device_id Identifier of the edge device.
 * @param iso_timestamp ISO 8601 formatted timestamp string.
 * @param readings Pointer to aquaponics readings.
 * @return Dynamically allocated JSON string (caller must free with free()), or NULL on error.
 */
char *telemetry_format_json(const char *device_id, const char *iso_timestamp, const aquaponics_readings_t *readings);

/**
 * @brief Format device lifecycle / health status JSON.
 *
 * @param device_id Identifier of the edge device.
 * @param status Status string (e.g. "online", "offline").
 * @param reason Optional reason string (e.g. "unexpected_disconnect", "graceful_shutdown").
 * @return Dynamically allocated JSON string (caller must free with free()), or NULL on error.
 */
char *telemetry_format_status_json(const char *device_id, const char *status, const char *reason);

/**
 * @brief Generate standard MQTT topic path for a device subtopic.
 *
 * Pattern: "aquaponics/devices/{deviceId}/{subtopic}"
 *
 * @param buf Destination buffer.
 * @param buf_len Length of destination buffer.
 * @param device_id Device identifier.
 * @param subtopic Target subtopic name ("telemetry", "status", etc.).
 * @return Number of characters written, or -1 on overflow/error.
 */
int telemetry_build_topic(char *buf, size_t buf_len, const char *device_id, const char *subtopic);

#ifdef __cplusplus
}
#endif

#endif // AQUAPONICS_TELEMETRY_H
