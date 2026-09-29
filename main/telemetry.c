/*
 * SPDX-FileCopyrightText: 2024-2026 AquaponicsEdgeSimulator Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "cJSON.h"

static double round_two_decimals(float val)
{
    return round((double)val * 100.0) / 100.0;
}

char *telemetry_format_json(const char *device_id, const char *iso_timestamp, const aquaponics_readings_t *readings)
{
    if (!device_id || !iso_timestamp || !readings) {
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "deviceId", device_id);
    cJSON_AddStringToObject(root, "timestamp", iso_timestamp);

    cJSON *readings_obj = cJSON_CreateObject();
    if (!readings_obj) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON_AddNumberToObject(readings_obj, "ph", round_two_decimals(readings->ph));
    cJSON_AddNumberToObject(readings_obj, "temperature", round_two_decimals(readings->temperature));
    cJSON_AddNumberToObject(readings_obj, "ec", round_two_decimals(readings->ec));
    cJSON_AddNumberToObject(readings_obj, "dissolvedOxygen", round_two_decimals(readings->dissolved_oxygen));

    cJSON_AddItemToObject(root, "readings", readings_obj);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

char *telemetry_format_status_json(const char *device_id, const char *status, const char *reason)
{
    if (!device_id || !status) {
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "deviceId", device_id);
    cJSON_AddStringToObject(root, "status", status);
    if (reason && strlen(reason) > 0) {
        cJSON_AddStringToObject(root, "reason", reason);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

int telemetry_build_topic(char *buf, size_t buf_len, const char *device_id, const char *subtopic)
{
    if (!buf || buf_len == 0 || !device_id || !subtopic) {
        return -1;
    }

    int written = snprintf(buf, buf_len, "aquaponics/devices/%s/%s", device_id, subtopic);
    if (written < 0 || (size_t)written >= buf_len) {
        return -1;
    }

    return written;
}
