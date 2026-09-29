/*
 * SPDX-FileCopyrightText: 2024-2026 AquaponicsEdgeSimulator Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AQUAPONICS_SIMULATOR_H
#define AQUAPONICS_SIMULATOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Aquaponics sensor telemetry readings.
 */
typedef struct {
    float ph;               /**< Water pH (nominal: 6.5 - 7.2) */
    float temperature;      /**< Water temperature in Celsius (nominal: 20.0 - 26.0 °C) */
    float ec;               /**< Electrical Conductivity in mS/cm (nominal: 1.2 - 2.0 mS/cm) */
    float dissolved_oxygen; /**< Dissolved Oxygen in mg/L (nominal: 6.0 - 8.5 mg/L) */
} aquaponics_readings_t;

/**
 * @brief Simulation scenario modes.
 */
typedef enum {
    SIM_MODE_NORMAL = 0,       /**< Stable operation with realistic small perturbations */
    SIM_MODE_PH_DRIFT,         /**< Nitrification imbalance causing continuous pH drift */
    SIM_MODE_HIGH_TEMP,        /**< Cooling failure / thermal spike scenario */
    SIM_MODE_LOW_DO            /**< Aeration pump failure causing hypoxia */
} sim_mode_t;

/**
 * @brief Physical and realistic operational safety limits for aquaponics.
 */
#define SIM_PH_MIN             0.0f
#define SIM_PH_MAX             14.0f
#define SIM_TEMP_MIN           0.0f
#define SIM_TEMP_MAX           50.0f
#define SIM_EC_MIN             0.0f
#define SIM_EC_MAX             5.0f
#define SIM_DO_MIN             0.0f
#define SIM_DO_MAX             20.0f

/**
 * @brief Initialize the simulator state.
 *
 * @param seed Pseudo-random generator seed (0 uses pseudo-entropy).
 * @param mode Initial operational scenario mode.
 */
void simulator_init(uint32_t seed, sim_mode_t mode);

/**
 * @brief Update the operational mode during runtime.
 *
 * @param mode Target operational mode.
 */
void simulator_set_mode(sim_mode_t mode);

/**
 * @brief Get the current simulation mode.
 *
 * @return Active simulation mode.
 */
sim_mode_t simulator_get_mode(void);

/**
 * @brief Convert simulation mode enum to human-readable string.
 *
 * @param mode Simulation mode.
 * @return String representation.
 */
const char *simulator_mode_to_string(sim_mode_t mode);

/**
 * @brief Parse string into simulation mode enum.
 *
 * @param str String identifier (e.g. "NORMAL", "PH_DRIFT", "HIGH_TEMPERATURE", "LOW_DISSOLVED_OXYGEN").
 * @return Matching mode or SIM_MODE_NORMAL if unrecognized.
 */
sim_mode_t simulator_mode_from_string(const char *str);

/**
 * @brief Compute the next simulation step and update current sensor values.
 *
 * Employs a bounded stochastic process with trend vectors depending on the
 * active operational scenario.
 *
 * @return New aquaponics sensor readings.
 */
aquaponics_readings_t simulator_step(void);

/**
 * @brief Get the latest calculated readings without advancing the simulation step.
 *
 * @return Current readings snapshot.
 */
aquaponics_readings_t simulator_get_current_readings(void);

/**
 * @brief Helper function to clamp sensor values within physical boundaries.
 */
float simulator_clamp(float value, float min_val, float max_val);

#ifdef __cplusplus
}
#endif

#endif // AQUAPONICS_SIMULATOR_H
