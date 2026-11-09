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

/** @brief Per-device brightness triangle state for the breathe renderer. */
typedef struct
{
    rt_tick_t start_tick;  /**< Animation phase origin. */
    uint16_t last_ratio;   /**< Previous interpolation coefficient. */
    rt_bool_t initialized; /**< True after first frame is rendered. */
} ws2812_breathe_state_t;

/**
 * @brief Initialize breathing animation phase.
 * @param dev Target LED device.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @return RT_EOK or a configuration error.
 */
static rt_err_t ws2812_breathe_start(ws2812_t dev, void *state, const ws2812_effect_config_t *config)
{
    ws2812_breathe_state_t *breathe = (ws2812_breathe_state_t *)state;
    (void)dev;
    if (config->period_ticks < 2U)
    {
        return -RT_EINVAL;
    }
    breathe->start_tick = rt_tick_get();
    breathe->initialized = RT_FALSE;
    return RT_EOK;
}

/**
 * @brief Render a 0..255..0 RGB brightness triangle once per changed ratio.
 * @param dev LED device whose pixels are updated.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @param now Current RT-Thread tick.
 * @param force Whether the initial frame must be rendered.
 * @param changed Output flag indicating a newly rendered frame.
 * @return RT_EOK or the first pixel-update error.
 */
static rt_err_t ws2812_breathe_update(ws2812_t dev, void *state, const ws2812_effect_config_t *config,
                                      rt_tick_t now, rt_bool_t force, rt_bool_t *changed)
{
    ws2812_breathe_state_t *breathe = (ws2812_breathe_state_t *)state;
    uint64_t position = ((uint64_t)((rt_tick_t)(now - breathe->start_tick) % config->period_ticks) * 510ULL) /
                        config->period_ticks;
    uint16_t ratio = (uint16_t)(position <= 255ULL ? position : 510ULL - position);
    *changed = force || !breathe->initialized || ratio != breathe->last_ratio;
    if (!*changed)
    {
        return RT_EOK;
    }
    breathe->initialized = RT_TRUE;
    breathe->last_ratio = ratio;
    return ws2812_fill(dev, ws2812_effect_blend(WS2812_COLOR_OFF, config->color, ratio));
}

/** @brief Built-in breathing effect descriptor. */
const ws2812_effect_method_t ws2812_method_breathe = {
    WS2812_EFFECT_BREATHE, sizeof(ws2812_breathe_state_t),
    ws2812_breathe_start, ws2812_breathe_update, RT_NULL
};
