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

#define DBG_TAG "ws2812_fx"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/** @brief Aligned fixed-size state passed to registered effect callbacks. */
typedef union
{
    uint64_t align;                            /**< At least eight-byte alignment. */
    uint8_t bytes[WS2812B_EFFECT_STATE_BYTES]; /**< Implementation-private bytes. */
} ws2812_method_storage_t;

/** @brief One device's currently selected effect and copied sequence parameters. */
typedef struct
{
    const ws2812_effect_method_t *method;                 /**< Selected renderer. */
    ws2812_effect_config_t config;                        /**< Current copied settings. */
    ws2812_effect_step_t steps[WS2812B_EFFECT_MAX_STEPS]; /**< Owned sequence. */
    ws2812_method_storage_t state;                        /**< Private method working state. */
    rt_bool_t active;                                     /**< True if an effect is active. */
    rt_bool_t dirty;                                      /**< First frame must be rendered. */
} ws2812_effect_slot_t;

/** @brief Single registry and all independent per-device effect states. */
typedef struct
{
    /* Zero enabled methods still need one physical C array element; logical capacity stays zero. */
    const ws2812_effect_method_t *methods[WS2812B_EFFECT_METHOD_CAPACITY ? WS2812B_EFFECT_METHOD_CAPACITY : 1]; /**< Direct index is effect ID. */
    ws2812_effect_slot_t current[WS2812B_MAX_DEVICES];                                                          /**< Stable device index. */
} ws2812_effect_manager_t;

/** @brief Package-wide descriptors and device effect contexts; no Heap allocation. */
static ws2812_effect_manager_t ws2812_effect_manager;

void ws2812_effect_mark_dirty(ws2812_t dev)
{
    if (dev != RT_NULL && dev->registry_index < ws2812_get_device_count())
    {
        ws2812_effect_manager.current[dev->registry_index].dirty = RT_TRUE;
    }
}

/**
 * @brief Register one effect method at its numeric ID before application polling starts.
 * @param slot Direct array index, exactly equal to method->id.
 * @param method Static callback descriptor with lifetime equal to the application.
 * @return RT_EOK or -RT_EINVAL when the index or descriptor is invalid/occupied.
 */
rt_err_t ws2812_effect_register_at(uint8_t slot, const ws2812_effect_method_t *method)
{
    if (slot >= WS2812B_EFFECT_METHOD_CAPACITY || method == RT_NULL || method->id != slot ||
        method->update == RT_NULL || method->state_size > WS2812B_EFFECT_STATE_BYTES ||
        ws2812_effect_manager.methods[slot] != RT_NULL)
    {
        return -RT_EINVAL;
    }
    ws2812_effect_manager.methods[slot] = method;
    return RT_EOK;
}

rt_err_t ws2812_effect_register(const ws2812_effect_method_t *method)
{
    return (method != RT_NULL && method->id < WS2812B_EFFECT_METHOD_CAPACITY) ? ws2812_effect_register_at((uint8_t)method->id, method) : -RT_EINVAL;
}

rt_err_t ws2812_effect_register_builtins(void)
{
    rt_err_t status = RT_EOK;
#ifdef WS2812B_EFFECT_SOLID
    if (ws2812_effect_register(&ws2812_method_solid) != RT_EOK)
    {
        status = -RT_ERROR;
    }
#endif /* WS2812B_EFFECT_SOLID */
#ifdef WS2812B_EFFECT_BLINK
    if (ws2812_effect_register(&ws2812_method_blink) != RT_EOK)
    {
        status = -RT_ERROR;
    }
#endif /* WS2812B_EFFECT_BLINK */
#ifdef WS2812B_EFFECT_BREATHE
    if (ws2812_effect_register(&ws2812_method_breathe) != RT_EOK)
    {
        status = -RT_ERROR;
    }
#endif /* WS2812B_EFFECT_BREATHE */
#ifdef WS2812B_EFFECT_GRADIENT
    if (ws2812_effect_register(&ws2812_method_gradient) != RT_EOK)
    {
        status = -RT_ERROR;
    }
#endif /* WS2812B_EFFECT_GRADIENT */
#ifdef WS2812B_EFFECT_GRADATION
    if (ws2812_effect_register(&ws2812_method_gradation) != RT_EOK)
    {
        status = -RT_ERROR;
    }
#endif /* WS2812B_EFFECT_GRADATION */
#ifdef WS2812B_EFFECT_CHASE
    if (ws2812_effect_register(&ws2812_method_chase) != RT_EOK)
    {
        status = -RT_ERROR;
    }
#endif /* WS2812B_EFFECT_CHASE */
#ifdef WS2812B_EFFECT_SEQUENCE
    if (ws2812_effect_register(&ws2812_method_sequence) != RT_EOK)
    {
        status = -RT_ERROR;
    }
#endif /* WS2812B_EFFECT_SEQUENCE */
    return status;
}

/**
 * @brief Replace the current effect in one fixed device slot, owning copied sequence data.
 * @param dev Device with a stable creation-order index.
 * @param slot Destination running effect slot.
 * @param config Caller settings; sequence entries will be copied into the slot.
 * @return RT_EOK or invalid/unregistered method error; active method stays intact on failure.
 * @note Method start callbacks must only initialize their candidate state and must not send SPI.
 */
static rt_err_t ws2812_effect_configure_slot(ws2812_t dev, ws2812_effect_slot_t *slot,
                                             const ws2812_effect_config_t *config)
{
    const ws2812_effect_method_t *method;
    ws2812_method_storage_t next_state;
    rt_err_t status;
    if (config == RT_NULL || config->type >= WS2812B_EFFECT_METHOD_CAPACITY ||
        config->step_count > WS2812B_EFFECT_MAX_STEPS ||
        (config->step_count != 0U && config->steps == RT_NULL))
    {
        return -RT_EINVAL;
    }
    method = ws2812_effect_manager.methods[config->type];
    if (method == RT_NULL)
    {
        return -RT_EINVAL;
    }
    /* Validate and initialize the replacement without disturbing the running effect. */
    rt_memset(&next_state, 0, sizeof(next_state));
    if (method->start != RT_NULL)
    {
        status = method->start(dev, next_state.bytes, config);
        if (status != RT_EOK)
        {
            return status;
        }
    }
    if (slot->active && slot->method != RT_NULL && slot->method->stop != RT_NULL)
    {
        slot->method->stop(dev, slot->state.bytes);
    }
    slot->active = RT_FALSE;
    slot->method = method;
    /* A caller may restart the same owned sequence: never memcpy onto itself. */
    if (config->step_count != 0U && config->steps != slot->steps)
    {
        rt_memcpy(slot->steps, config->steps, (rt_size_t)config->step_count * sizeof(slot->steps[0]));
    }
    slot->config = *config;
    if (config->step_count != 0U)
    {
        slot->config.steps = slot->steps;
    }
    slot->state = next_state;
    slot->active = RT_TRUE;
    slot->dirty = RT_TRUE;
    LOG_D("Selected effect method %u on device %u", config->type, dev->registry_index);
    return RT_EOK;
}

#ifdef WS2812B_USING_KCONFIG_DEVICES
/** @brief Reserved Kconfig no-effect ID, outside every registrable method index. */
#define WS2812_INITIAL_EFFECT_NONE 65535U

/* Kconfig kinds are stable; enabled enum values are assigned densely by the compiler. */
#define WS2812_KIND_0 WS2812_INITIAL_EFFECT_NONE
#define WS2812_KIND_1 WS2812_EFFECT_SOLID
#define WS2812_KIND_2 WS2812_EFFECT_BLINK
#define WS2812_KIND_3 WS2812_EFFECT_BREATHE
#define WS2812_KIND_4 WS2812_EFFECT_GRADIENT
#define WS2812_KIND_5 WS2812_EFFECT_CHASE
#define WS2812_KIND_6 WS2812_EFFECT_GRADATION

/* Expand the numeric kind before joining it to WS2812_KIND_. */
#define WS2812_CAT_(a, b) a##b
#define WS2812_CAT(a, b)  WS2812_CAT_(a, b)
/** @brief Resolve Kconfig kind to the compact enum index at preprocess time. */
#define WS2812_INITIAL_ID(n) WS2812_CAT(WS2812_KIND_, WS2812_DEV_KIND(n))

/** @brief Initial effect settings of the configured device slots. */
typedef struct
{
    uint16_t type;          /**< Initial effect ID or no-effect sentinel. */
    uint32_t color;         /**< Primary RGB888 color. */
    uint32_t background;    /**< Background RGB888 color. */
    rt_tick_t period_ticks; /**< Tick duration or update interval for the method. */
} ws2812_initial_effect_t;

/** @brief Expand one Kconfig device's initial effect into a positional table entry. */
#define WS2812_INITIAL_ENTRY(n)                                   \
    [n] = { WS2812_INITIAL_ID(n), WS2812B_DEV##n##_INITIAL_COLOR, \
            WS2812B_DEV##n##_INITIAL_BACKGROUND,                  \
            WS2812B_DEV##n##_INITIAL_PERIOD_TICKS },

/** @brief Kconfig initial selections indexed by physical device slot. */
static const ws2812_initial_effect_t ws2812_initial_effects[WS2812B_KCONFIG_SLOT_COUNT] = {
#if !defined(WS2812B_KCONFIG_DEVICE0) && !defined(WS2812B_KCONFIG_DEVICE1) && !defined(WS2812B_KCONFIG_DEVICE2)
    { WS2812_INITIAL_EFFECT_NONE, 0U, 0U, 0U }
#else
    WS2812_FOR_EACH_ENABLED_SLOT(WS2812_INITIAL_ENTRY)
#endif /* No Kconfig devices enabled */
};

rt_err_t ws2812_effect_start_kconfig(uint8_t slot)
{
    ws2812_effect_config_t effect = { 0 };
    const ws2812_initial_effect_t *initial;
    ws2812_t dev;
#ifdef WS2812B_CHECK_PARAMETERS
    if (slot >= WS2812B_KCONFIG_SLOT_COUNT)
    {
        return -RT_EINVAL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    dev = ws2812_get_kconfig_device(slot);
    if (dev == RT_NULL)
    {
        return RT_EOK; /* Disabled Kconfig slots do not select an effect. */
    }
    initial = &ws2812_initial_effects[slot];
    if (initial->type == WS2812_INITIAL_EFFECT_NONE)
    {
        return RT_EOK;
    }
    effect.type = initial->type;
    effect.color = initial->color;
    effect.background_color = initial->background;
    effect.period_ticks = initial->period_ticks;
    /* Kconfig's initial-effect choice excludes SEQUENCE, which requires runtime steps. */
    return ws2812_effect_start(dev, &effect);
}

rt_err_t ws2812_effect_init_kconfig(void)
{
    uint8_t slot;
    for (slot = 0U; slot < WS2812B_KCONFIG_SLOT_COUNT; ++slot)
    {
        rt_err_t result = ws2812_effect_start_kconfig(slot);
        if (result != RT_EOK)
        {
            return result;
        }
    }
    return RT_EOK;
}
#endif /* WS2812B_USING_KCONFIG_DEVICES */

rt_err_t ws2812_effect_start(ws2812_t dev, const ws2812_effect_config_t *config)
{
    if (dev == RT_NULL || dev->registry_index >= ws2812_get_device_count())
    {
        return -RT_EINVAL;
    }
    /* Does not perform SPI IO. The application calls poll() to render the first frame. */
    return ws2812_effect_configure_slot(dev, &ws2812_effect_manager.current[dev->registry_index], config);
}

rt_err_t ws2812_effect_start_sequence(ws2812_t dev, const ws2812_effect_step_t *steps, uint16_t step_count)
{
#ifdef WS2812B_EFFECT_SEQUENCE
    /* This temporary descriptor is passed to the existing copy-owning effect API. */
    ws2812_effect_config_t config = { 0 };

    config.type = WS2812_EFFECT_SEQUENCE;
    config.steps = steps;
    config.step_count = step_count;
    return ws2812_effect_start(dev, &config);
#else
    (void)dev;
    (void)steps;
    (void)step_count;
    return -RT_EINVAL;
#endif /* WS2812B_EFFECT_SEQUENCE */
}

rt_err_t ws2812_effect_stop(ws2812_t dev)
{
    ws2812_effect_slot_t *slot;
    if (dev == RT_NULL || dev->registry_index >= ws2812_get_device_count())
    {
        return -RT_EINVAL;
    }
    slot = &ws2812_effect_manager.current[dev->registry_index];
    if (slot->active && slot->method != RT_NULL && slot->method->stop != RT_NULL)
    {
        slot->method->stop(dev, slot->state.bytes);
    }
    slot->active = RT_FALSE;
    return RT_EOK;
}

rt_err_t ws2812_effect_poll(ws2812_t dev, rt_tick_t now)
{
    ws2812_effect_slot_t *slot;
    rt_bool_t changed = RT_FALSE;
    rt_err_t status;
    if (dev == RT_NULL || dev->registry_index >= ws2812_get_device_count())
    {
        return -RT_EINVAL;
    }
    slot = &ws2812_effect_manager.current[dev->registry_index];
    if (!slot->active)
    {
        return RT_EOK;
    }
    status = slot->method->update(dev, slot->state.bytes, &slot->config, now, slot->dirty, &changed);
    if (status != RT_EOK)
    {
        slot->dirty = RT_TRUE;
        return status;
    }
    if (changed || slot->dirty)
    {
        status = ws2812_send_frame(dev);
        if (status != RT_EOK)
        {
            slot->dirty = RT_TRUE; /* Retry the unsent frame on the next poll. */
            return status;
        }
    }
    slot->dirty = RT_FALSE;
    return RT_EOK;
}

rt_err_t ws2812_effect_poll_all(rt_tick_t now)
{
    rt_size_t index;
    rt_err_t status = RT_EOK;
    for (index = 0U; index < ws2812_get_device_count(); ++index)
    {
        rt_err_t current = ws2812_effect_poll(ws2812_get_device(index), now);
        if (current != RT_EOK)
        {
            status = current;
        }
    }
    return status;
}

/**
 * @brief Interpolate two 8-bit channels with exact rounded division by 255.
 * @param a Start channel value.
 * @param b End channel value.
 * @param position Interpolation factor in the inclusive range 0..255.
 * @return Blended 8-bit channel value.
 */
static uint8_t ws2812_effect_blend_channel(uint8_t a, uint8_t b, uint32_t position)
{
    /* Original: (a*(255-position) + b*position + 127)/255. The weighted sum <=65025;
     * ((p+128)+((p+128)>>8))>>8 is exactly the rounded quotient for this range.
     */
    uint32_t value = (uint32_t)a * (255U - position) + (uint32_t)b * position + 128U;
    return (uint8_t)((value + (value >> 8U)) >> 8U);
}

uint32_t ws2812_effect_blend(uint32_t start, uint32_t end, uint32_t position)
{
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    if (position > 255U)
    {
        position = 255U;
    }
    red = ws2812_effect_blend_channel((uint8_t)(start >> 16U), (uint8_t)(end >> 16U), position);
    green = ws2812_effect_blend_channel((uint8_t)(start >> 8U), (uint8_t)(end >> 8U), position);
    blue = ws2812_effect_blend_channel((uint8_t)start, (uint8_t)end, position);
    return (red << 16U) | (green << 8U) | blue;
}
