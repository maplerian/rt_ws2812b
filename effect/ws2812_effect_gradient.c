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

/** @brief Time-only whole-strip two-color gradient state. */
typedef struct
{
    rt_tick_t start_tick;  /**< Start Tick for interpolation. */
    uint16_t last_ratio;   /**< Last rendered interpolation ratio. */
    rt_bool_t initialized; /**< First frame was generated. */
} ws2812_gradient_state_t;

/**
 * @brief Initialize two-color gradient phase for one device.
 * @param dev Target LED device.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @return RT_EOK or a configuration error.
 */
static rt_err_t ws2812_gradient_start(ws2812_t dev, void *state, const ws2812_effect_config_t *config)
{
    ws2812_gradient_state_t *gradient = (ws2812_gradient_state_t *)state;
    (void)dev;
    if (config->period_ticks < 2U)
    {
        return -RT_EINVAL;
    }
    gradient->start_tick = rt_tick_get();
    gradient->initialized = RT_FALSE;
    return RT_EOK;
}

/**
 * @brief Interpolate a uniform whole-strip color between two RGB endpoints.
 * @param dev LED device whose pixels are updated.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @param now Current RT-Thread tick.
 * @param force Whether the initial frame must be rendered.
 * @param changed Output flag indicating a newly rendered frame.
 * @return RT_EOK or the first pixel-update error.
 */
static rt_err_t ws2812_gradient_update(ws2812_t dev, void *state, const ws2812_effect_config_t *config,
                                       rt_tick_t now, rt_bool_t force, rt_bool_t *changed)
{
    ws2812_gradient_state_t *gradient = (ws2812_gradient_state_t *)state;
    uint64_t position = ((uint64_t)((rt_tick_t)(now - gradient->start_tick) % config->period_ticks) * 510ULL) /
                        config->period_ticks;
    uint16_t ratio = (uint16_t)(position <= 255ULL ? position : 510ULL - position);
    *changed = force || !gradient->initialized || gradient->last_ratio != ratio;
    if (!*changed)
    {
        return RT_EOK;
    }
    gradient->initialized = RT_TRUE;
    gradient->last_ratio = ratio;
    return ws2812_fill(dev, ws2812_effect_blend(config->color, config->background_color, ratio));
}

/** @brief Built-in time-only whole-strip gradient descriptor. */
const ws2812_effect_method_t ws2812_method_gradient = {
    WS2812_EFFECT_GRADIENT, sizeof(ws2812_gradient_state_t),
    ws2812_gradient_start, ws2812_gradient_update, RT_NULL
};
