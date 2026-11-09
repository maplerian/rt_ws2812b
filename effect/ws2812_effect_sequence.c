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

/** @brief Per-device current step and phase of a color sequence. */
typedef struct
{
    rt_tick_t start_tick;  /**< Sequence phase origin. */
    uint16_t last_index;   /**< Previously displayed step index. */
    rt_bool_t initialized; /**< First sequence frame rendered. */
} ws2812_sequence_state_t;

/**
 * @brief Validate sequence step durations and initialize phase.
 * @param dev Target LED device.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @return RT_EOK or a configuration error.
 */
static rt_err_t ws2812_sequence_start(ws2812_t dev, void *state, const ws2812_effect_config_t *config)
{
    ws2812_sequence_state_t *sequence = (ws2812_sequence_state_t *)state;
    uint16_t i;
    (void)dev;
    if (config->steps == RT_NULL || config->step_count == 0U)
    {
        return -RT_EINVAL;
    }
    for (i = 0U; i < config->step_count; ++i)
    {
        if (config->steps[i].duration_ticks == 0U)
        {
            return -RT_EINVAL;
        }
    }
    sequence->start_tick = rt_tick_get();
    sequence->initialized = RT_FALSE;
    return RT_EOK;
}

/**
 * @brief Render the current step of a repeating color/duration sequence.
 * @param dev LED device whose pixels are updated.
 * @param state Per-device private effect state.
 * @param config Immutable effect configuration.
 * @param now Current RT-Thread tick.
 * @param force Whether the initial frame must be rendered.
 * @param changed Output flag indicating a newly rendered frame.
 * @return RT_EOK or the first pixel-update error.
 */
static rt_err_t ws2812_sequence_update(ws2812_t dev, void *state, const ws2812_effect_config_t *config,
                                       rt_tick_t now, rt_bool_t force, rt_bool_t *changed)
{
    ws2812_sequence_state_t *sequence = (ws2812_sequence_state_t *)state;
    uint64_t total = 0ULL;
    uint64_t position;
    uint16_t i;
    uint16_t selected = 0U;
    for (i = 0U; i < config->step_count; ++i)
    {
        total += (uint64_t)config->steps[i].duration_ticks;
    }
    position = ((uint64_t)(rt_tick_t)(now - sequence->start_tick)) % total;
    for (i = 0U; i < config->step_count; ++i)
    {
        if (position < config->steps[i].duration_ticks)
        {
            selected = i;
            break;
        }
        position -= config->steps[i].duration_ticks;
    }
    *changed = force || !sequence->initialized || selected != sequence->last_index;
    if (!*changed)
    {
        return RT_EOK;
    }
    sequence->initialized = RT_TRUE;
    sequence->last_index = selected;
    return ws2812_fill(dev, config->steps[selected].color);
}

/** @brief Built-in sequence effect descriptor. */
const ws2812_effect_method_t ws2812_method_sequence = {
    WS2812_EFFECT_SEQUENCE, sizeof(ws2812_sequence_state_t),
    ws2812_sequence_start, ws2812_sequence_update, RT_NULL
};
