/*
 * SPDX-FileCopyrightText: 2024-2026 AquaponicsEdgeSimulator Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "simulator.h"
#include <string.h>
#include <math.h>

/* Nominal aquaponics setpoints */
static const float NOMINAL_PH   = 6.80f;
static const float NOMINAL_TEMP = 23.50f;
static const float NOMINAL_EC   = 1.50f;
static const float NOMINAL_DO   = 7.20f;

/* Internal PRNG state for deterministic simulation */
static uint32_t s_prng_state = 42;
static sim_mode_t s_current_mode = SIM_MODE_NORMAL;
static aquaponics_readings_t s_current_readings = {
    .ph = 6.80f,
    .temperature = 23.50f,
    .ec = 1.50f,
    .dissolved_oxygen = 7.20f,
};

float simulator_clamp(float value, float min_val, float max_val)
{
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/**
 * @brief Simple deterministic 32-bit LCG returning float in [-1.0, 1.0].
 */
static float sim_random_unit(void)
{
    s_prng_state = s_prng_state * 1664525u + 1013904223u;
    return ((float)(s_prng_state >> 16) / 32767.5f) - 1.0f;
}

void simulator_init(uint32_t seed, sim_mode_t mode)
{
    s_prng_state = (seed != 0) ? seed : 1337;
    s_current_mode = mode;
    s_current_readings.ph = NOMINAL_PH;
    s_current_readings.temperature = NOMINAL_TEMP;
    s_current_readings.ec = NOMINAL_EC;
    s_current_readings.dissolved_oxygen = NOMINAL_DO;
}

void simulator_set_mode(sim_mode_t mode)
{
    s_current_mode = mode;
}

sim_mode_t simulator_get_mode(void)
{
    return s_current_mode;
}

const char *simulator_mode_to_string(sim_mode_t mode)
{
    switch (mode) {
    case SIM_MODE_PH_DRIFT:
        return "PH_DRIFT";
    case SIM_MODE_HIGH_TEMP:
        return "HIGH_TEMPERATURE";
    case SIM_MODE_LOW_DO:
        return "LOW_DISSOLVED_OXYGEN";
    case SIM_MODE_NORMAL:
    default:
        return "NORMAL";
    }
}

sim_mode_t simulator_mode_from_string(const char *str)
{
    if (!str) return SIM_MODE_NORMAL;
    if (strcmp(str, "PH_DRIFT") == 0) return SIM_MODE_PH_DRIFT;
    if (strcmp(str, "HIGH_TEMPERATURE") == 0 || strcmp(str, "HIGH_TEMP") == 0) return SIM_MODE_HIGH_TEMP;
    if (strcmp(str, "LOW_DISSOLVED_OXYGEN") == 0 || strcmp(str, "LOW_DO") == 0) return SIM_MODE_LOW_DO;
    return SIM_MODE_NORMAL;
}

aquaponics_readings_t simulator_step(void)
{
    /* Natural minor stochastic noise */
    float ph_noise   = sim_random_unit() * 0.02f;
    float temp_noise = sim_random_unit() * 0.08f;
    float ec_noise   = sim_random_unit() * 0.015f;
    float do_noise   = sim_random_unit() * 0.04f;

    switch (s_current_mode) {
    case SIM_MODE_PH_DRIFT:
        /* Progressive acidification scenario (nitrification acid accumulation) */
        s_current_readings.ph += (-0.05f + ph_noise);
        s_current_readings.temperature += (NOMINAL_TEMP - s_current_readings.temperature) * 0.05f + temp_noise;
        s_current_readings.ec += (NOMINAL_EC - s_current_readings.ec) * 0.05f + ec_noise;
        s_current_readings.dissolved_oxygen += (NOMINAL_DO - s_current_readings.dissolved_oxygen) * 0.05f + do_noise;
        break;

    case SIM_MODE_HIGH_TEMP:
        /* Thermal spike scenario: temperature climbs, DO solubility falls */
        s_current_readings.ph += (NOMINAL_PH - s_current_readings.ph) * 0.05f + ph_noise;
        s_current_readings.temperature += (0.25f + temp_noise);
        s_current_readings.ec += 0.01f + ec_noise; /* Water evaporation slight EC increase */
        s_current_readings.dissolved_oxygen += (-0.08f + do_noise); /* Warmer water holds less DO */
        break;

    case SIM_MODE_LOW_DO:
        /* Aerator failure: rapid DO drop */
        s_current_readings.ph += (NOMINAL_PH - s_current_readings.ph) * 0.05f + ph_noise;
        s_current_readings.temperature += (NOMINAL_TEMP - s_current_readings.temperature) * 0.05f + temp_noise;
        s_current_readings.ec += (NOMINAL_EC - s_current_readings.ec) * 0.05f + ec_noise;
        s_current_readings.dissolved_oxygen += (-0.20f + do_noise);
        break;

    case SIM_MODE_NORMAL:
    default:
        /* Mean-reverting random walk around optimal target values */
        s_current_readings.ph += (NOMINAL_PH - s_current_readings.ph) * 0.05f + ph_noise;
        s_current_readings.temperature += (NOMINAL_TEMP - s_current_readings.temperature) * 0.05f + temp_noise;
        s_current_readings.ec += (NOMINAL_EC - s_current_readings.ec) * 0.05f + ec_noise;
        s_current_readings.dissolved_oxygen += (NOMINAL_DO - s_current_readings.dissolved_oxygen) * 0.05f + do_noise;
        break;
    }

    /* Enforce physical limit clamping */
    s_current_readings.ph = simulator_clamp(s_current_readings.ph, SIM_PH_MIN, SIM_PH_MAX);
    s_current_readings.temperature = simulator_clamp(s_current_readings.temperature, SIM_TEMP_MIN, SIM_TEMP_MAX);
    s_current_readings.ec = simulator_clamp(s_current_readings.ec, SIM_EC_MIN, SIM_EC_MAX);
    s_current_readings.dissolved_oxygen = simulator_clamp(s_current_readings.dissolved_oxygen, SIM_DO_MIN, SIM_DO_MAX);

    return s_current_readings;
}

aquaponics_readings_t simulator_get_current_readings(void)
{
    return s_current_readings;
}
