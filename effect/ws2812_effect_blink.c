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

/* C99 compile-time assertion: BLINK uses two integer half-periods. */
#ifdef WS2812B_USING_KCONFIG_DEVICES
/** @brief Reject odd initial BLINK periods for each enabled Kconfig device. */
#define WS2812_ASSERT_BLINK_PERIOD(n)                                                             \
    typedef char ws2812_blink_period_even_##n[(WS2812_DEV_KIND(n) != WS2812_INITIAL_KIND_BLINK || \
                                               (WS2812_DEV_PERIOD(n) & 1U) == 0U)                 \
                                                  ? 1                                             \
                                                  : -1];
WS2812_FOR_EACH_ENABLED_SLOT(WS2812_ASSERT_BLINK_PERIOD)
#endif /* WS2812B_USING_KCONFIG_DEVICES */

/** @brief Independent blink phase tracked per device and active slot. */
typedef struct
{
    rt_tick_t start_tick; /**< Phase origin in RT-Thread ticks. */
    uint8_t last_phase;   /**< Last displayed 0/1 phase, or 2 before first draw. */
} ws2812_blink_state_t;

/**
 * @brief Initialize a two-color blink phase for one device.
 * @param dev Target LED device.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @return RT_EOK or a configuration error.
 */
static rt_err_t ws2812_blink_start(ws2812_t dev, void *state, const ws2812_effect_config_t *config)
{
    ws2812_blink_state_t *blink = (ws2812_blink_state_t *)state;
    (void)dev;
    if (config->period_ticks < 2U)
    {
        return -RT_EINVAL;
    }
#ifdef WS2812B_CHECK_PARAMETERS
    /* Two equal integer half-periods require an even number of ticks. */
    if ((config->period_ticks & 1U) != 0U)
    {
        return -RT_EINVAL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    blink->start_tick = rt_tick_get();
    blink->last_phase = 2U;
    return RT_EOK;
}

/**
 * @brief Render two alternating colors with one full period in RT ticks.
 * @param dev LED device whose pixels are updated.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @param now Current RT-Thread tick.
 * @param force Whether the initial frame must be rendered.
 * @param changed Output flag indicating a newly rendered frame.
 * @return RT_EOK or the first pixel-update error.
 */
static rt_err_t ws2812_blink_update(ws2812_t dev, void *state, const ws2812_effect_config_t *config,
                                    rt_tick_t now, rt_bool_t force, rt_bool_t *changed)
{
    ws2812_blink_state_t *blink = (ws2812_blink_state_t *)state;
    rt_tick_t half = config->period_ticks / 2U;
    uint8_t phase = (uint8_t)(((rt_tick_t)(now - blink->start_tick) / half) & 1U);
    *changed = force || phase != blink->last_phase;
    if (!*changed)
    {
        return RT_EOK;
    }
    blink->last_phase = phase;
    return ws2812_fill(dev, phase ? config->background_color : config->color);
}

/** @brief Built-in blink method descriptor. */
const ws2812_effect_method_t ws2812_method_blink = {
    WS2812_EFFECT_BLINK, sizeof(ws2812_blink_state_t),
    ws2812_blink_start, ws2812_blink_update, RT_NULL
};
