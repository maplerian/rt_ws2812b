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

/** @brief One strip's spatial gradient evolving RGB start endpoint. */
typedef struct
{
    uint8_t start_r;       /**< Current red start endpoint. */
    uint8_t start_g;       /**< Current green start endpoint. */
    uint8_t start_b;       /**< Current blue start endpoint. */
    uint8_t dir_r;         /**< Red direction: 1 decrement, 0 increment. */
    uint8_t dir_g;         /**< Green direction: 1 decrement, 0 increment. */
    uint8_t dir_b;         /**< Blue direction: 1 decrement, 0 increment. */
    rt_tick_t last_tick;   /**< Last color-update tick. */
    rt_bool_t initialized; /**< Spatial frame has been rendered. */
} ws2812_gradation_state_t;

/**
 * @brief Initialize default start color and direction from the original gradation.
 * @param dev Unused strip handle.
 * @param state Per-device writable RGB endpoint state.
 * @param config Optional caller-specific spatial RGB constraints.
 * @return RT_EOK when initialized.
 */
static rt_err_t ws2812_gradation_start(ws2812_t dev, void *state, const ws2812_effect_config_t *config)
{
    ws2812_gradation_state_t *gradation = (ws2812_gradation_state_t *)state;
    uint32_t initial = config->gradation.start_color;
    (void)dev;
#ifdef WS2812B_CHECK_PARAMETERS
    /* Zero maxima for every channel select the original built-in defaults. */
    if ((config->gradation.max_r != 0U || config->gradation.max_g != 0U ||
         config->gradation.max_b != 0U) &&
        (config->gradation.step_r > config->gradation.max_r ||
         config->gradation.step_g > config->gradation.max_g ||
         config->gradation.step_b > config->gradation.max_b ||
         ((initial >> 16U) & 0xFFU) > config->gradation.max_r ||
         ((initial >> 8U) & 0xFFU) > config->gradation.max_g ||
         (initial & 0xFFU) > config->gradation.max_b))
    {
        return -RT_EINVAL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    if (config->gradation.max_r == 0U && config->gradation.max_g == 0U &&
        config->gradation.max_b == 0U)
    {
        initial = 0x004600U; /* Original defaults: start (0,70,0). */
    }
    gradation->start_r = (uint8_t)(initial >> 16);
    gradation->start_g = (uint8_t)(initial >> 8);
    gradation->start_b = (uint8_t)initial;
    gradation->dir_r = 1U;
    gradation->dir_g = 0U;
    gradation->dir_b = 1U;
    gradation->last_tick = rt_tick_get();
    gradation->initialized = RT_FALSE;
    return RT_EOK;
}

/**
 * @brief Update one channel with the original back-and-forth LED animation rule.
 * @param value Current RGB channel intensity.
 * @param direction Mutable 1-decrement/0-increment state.
 * @param step Change per update.
 * @param maximum Configured upper limit.
 * @return New eight-bit channel value.
 */
static uint8_t ws2812_gradation_step(uint8_t value, uint8_t *direction, uint8_t step, uint8_t maximum)
{
    if (value > (uint8_t)(maximum - step))
    {
        *direction = 1U;
    }
    else if (value < step)
    {
        *direction = 0U;
    }
    /* Retain the original uint8 wrap behavior; do not alter the visual waveform here. */
    return (uint8_t)(*direction ? (value - step) : (value + step));
}

/**
 * @brief Spatially interpolate the current RGB start endpoint to the fixed endpoint.
 * @param dev Target strip and its LED count.
 * @param state Per-device moving endpoint.
 * @param config End RGB888 color, default 0x000078.
 * @param now Current RT Tick, wrap-safe.
 * @param force Force spatial rerender even when timer not due.
 * @param changed Receives RT_TRUE only after pixels were generated.
 * @return RT_EOK or the first pixel API error.
 */
static rt_err_t ws2812_gradation_update(ws2812_t dev, void *state, const ws2812_effect_config_t *config,
                                        rt_tick_t now, rt_bool_t force, rt_bool_t *changed)
{
    ws2812_gradation_state_t *gradation = (ws2812_gradation_state_t *)state;
    const ws2812_gradation_config_t *settings = &config->gradation;
    rt_bool_t defaults = (settings->max_r == 0U && settings->max_g == 0U && settings->max_b == 0U);
    rt_tick_t period = (config->period_ticks != 0U) ? config->period_ticks : 20U;
    uint8_t step_r = defaults ? 2U : settings->step_r;
    uint8_t step_g = defaults ? 1U : settings->step_g;
    uint8_t step_b = defaults ? 1U : settings->step_b;
    uint8_t max_r = defaults ? 130U : settings->max_r;
    uint8_t max_g = defaults ? 117U : settings->max_g;
    uint8_t max_b = defaults ? 10U : settings->max_b;
    uint32_t end = defaults ? 0x000078U : settings->end_color;
    uint16_t i;
    if ((rt_tick_t)(now - gradation->last_tick) >= period)
    {
        gradation->last_tick = now;
        gradation->start_r = ws2812_gradation_step(gradation->start_r, &gradation->dir_r, step_r, max_r);
        gradation->start_g = ws2812_gradation_step(gradation->start_g, &gradation->dir_g, step_g, max_g);
        gradation->start_b = ws2812_gradation_step(gradation->start_b, &gradation->dir_b, step_b, max_b);
        force = RT_TRUE;
    }
    *changed = force || !gradation->initialized;
    if (!*changed)
    {
        return RT_EOK;
    }
    gradation->initialized = RT_TRUE;
    for (i = 0U; i < dev->node_len; ++i)
    {
        /* Preserve original pixel-index/LED-count endpoint interpolation. */
        int32_t red = gradation->start_r + (((int32_t)((end >> 16) & 255U) - gradation->start_r) * i) / dev->node_len;
        int32_t green = gradation->start_g + (((int32_t)((end >> 8) & 255U) - gradation->start_g) * i) / dev->node_len;
        int32_t blue = gradation->start_b + (((int32_t)(end & 255U) - gradation->start_b) * i) / dev->node_len;
        rt_err_t result = ws2812_set_pixel(dev, i, ((uint32_t)red << 16) | ((uint32_t)green << 8) | (uint32_t)blue);
        if (result != RT_EOK)
        {
            return result;
        }
    }
    return RT_EOK;
}

/** @brief Spatial and temporal gradation builtin descriptor. */
const ws2812_effect_method_t ws2812_method_gradation = {
    WS2812_EFFECT_GRADATION, sizeof(ws2812_gradation_state_t),
    ws2812_gradation_start, ws2812_gradation_update, RT_NULL
};
