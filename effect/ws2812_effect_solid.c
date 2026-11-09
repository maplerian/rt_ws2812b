/*
 * Copyright (c) 2026, wdfk-prog
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         first version
 */
#include "ws2812_effect_internal.h"

/**
 * @brief Stateless fixed-color effect.
 * @param dev LED device whose pixels are updated.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @param now Current RT-Thread tick.
 * @param force Whether the initial frame must be rendered.
 * @param changed Output flag indicating a newly rendered frame.
 * @return RT_EOK or the first pixel-update error.
 */
static rt_err_t ws2812_solid_update(ws2812_t dev, void *state, const ws2812_effect_config_t *config,
                                    rt_tick_t now, rt_bool_t force, rt_bool_t *changed)
{
    (void)state;
    (void)now;
    *changed = force;
    return force ? ws2812_fill(dev, config->color) : RT_EOK;
}

/** @brief Built-in solid effect method descriptor. */
const ws2812_effect_method_t ws2812_method_solid = {
    WS2812_EFFECT_SOLID, 0U, RT_NULL, ws2812_solid_update, RT_NULL
};
