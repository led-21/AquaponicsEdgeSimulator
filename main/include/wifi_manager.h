/*
 * SPDX-FileCopyrightText: 2024-2026 AquaponicsEdgeSimulator Contributors
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AQUAPONICS_WIFI_MANAGER_H
#define AQUAPONICS_WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the Wi-Fi station interface.
 *
 * Configures netif, sets up event handlers for connection/reconnection,
 * and starts the Wi-Fi subsystem using credentials from Kconfig.
 *
 * @return ESP_OK on success, or appropriate error code.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Block until Wi-Fi reaches connected state or timeout expires.
 *
 * @param ticks_to_wait Maximum ticks to wait.
 * @return ESP_OK if connected, ESP_ERR_TIMEOUT if timed out, ESP_FAIL on failure.
 */
esp_err_t wifi_manager_wait_connected(TickType_t ticks_to_wait);

/**
 * @brief Query current Wi-Fi connection status.
 *
 * @return true if station has acquired an IP, false otherwise.
 */
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // AQUAPONICS_WIFI_MANAGER_H
