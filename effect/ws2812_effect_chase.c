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

/** @brief Moving pixel state of one independent chase. */
typedef struct
{
    rt_tick_t start_tick;  /**< Phase origin in ticks. */
    uint16_t last_pixel;   /**< Previous highlighted pixel. */
    rt_bool_t initialized; /**< First frame has been produced. */
} ws2812_chase_state_t;

/**
 * @brief Initialize chase animation and validate step period.
 * @param dev Target LED device.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @return RT_EOK or a configuration error.
 */
static rt_err_t ws2812_chase_start(ws2812_t dev, void *state, const ws2812_effect_config_t *config)
{
    ws2812_chase_state_t *chase = (ws2812_chase_state_t *)state;
    (void)dev;
    if (config->period_ticks == 0U)
    {
        return -RT_EINVAL;
    }
    chase->start_tick = rt_tick_get();
    chase->initialized = RT_FALSE;
    return RT_EOK;
}

/**
 * @brief Advance a single foreground LED along the selected strip.
 * @param dev LED device whose pixels are updated.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @param now Current RT-Thread tick.
 * @param force Whether the initial frame must be rendered.
 * @param changed Output flag indicating a newly rendered frame.
 * @return RT_EOK or the first pixel-update error.
 */
static rt_err_t ws2812_chase_update(ws2812_t dev, void *state, const ws2812_effect_config_t *config,
                                    rt_tick_t now, rt_bool_t force, rt_bool_t *changed)
{
    ws2812_chase_state_t *chase = (ws2812_chase_state_t *)state;
    uint16_t pixel = (uint16_t)(((rt_tick_t)(now - chase->start_tick) / config->period_ticks) % dev->node_len);
    rt_err_t result;
    *changed = force || !chase->initialized || pixel != chase->last_pixel;
    if (!*changed)
    {
        return RT_EOK;
    }
    chase->initialized = RT_TRUE;
    chase->last_pixel = pixel;
    result = ws2812_fill(dev, config->background_color);
    return (result == RT_EOK) ? ws2812_set_pixel(dev, pixel, config->color) : result;
}

/** @brief Built-in chase effect descriptor. */
const ws2812_effect_method_t ws2812_method_chase = {
    WS2812_EFFECT_CHASE, sizeof(ws2812_chase_state_t),
    ws2812_chase_start, ws2812_chase_update, RT_NULL
};
