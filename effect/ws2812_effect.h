/*
 * Copyright (c) 2026, wdfk-prog
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         first version
 */
#ifndef WS2812_EFFECT_H
#define WS2812_EFFECT_H

#include "ws2812.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Enabled built-in IDs are contiguous build-local array indexes, not persistent values. */
typedef enum
{
#ifdef WS2812B_EFFECT_SOLID
    WS2812_EFFECT_SOLID,            /**< Fixed color. */
#endif /* WS2812B_EFFECT_SOLID */
#ifdef WS2812B_EFFECT_BLINK
    WS2812_EFFECT_BLINK,            /**< Alternate foreground and background. */
#endif /* WS2812B_EFFECT_BLINK */
#ifdef WS2812B_EFFECT_BREATHE
    WS2812_EFFECT_BREATHE,          /**< Repeating fade up and down. */
#endif /* WS2812B_EFFECT_BREATHE */
#ifdef WS2812B_EFFECT_GRADIENT
    WS2812_EFFECT_GRADIENT,         /**< Whole-strip interpolation across time. */
#endif /* WS2812B_EFFECT_GRADIENT */
#ifdef WS2812B_EFFECT_CHASE
    WS2812_EFFECT_CHASE,            /**< Moving foreground pixel. */
#endif /* WS2812B_EFFECT_CHASE */
#ifdef WS2812B_EFFECT_SEQUENCE
    WS2812_EFFECT_SEQUENCE,         /**< Repeating per-step colors. */
#endif /* WS2812B_EFFECT_SEQUENCE */
#ifdef WS2812B_EFFECT_GRADATION
    WS2812_EFFECT_GRADATION,        /**< Spatial gradient with moving RGB endpoints. */
#endif /* WS2812B_EFFECT_GRADATION */
    WS2812_EFFECT_BUILTIN_COUNT     /**< Built-in count; not an effect method ID. */
} ws2812_effect_type_t;

/** @brief Registered method capacity: compiled built-ins followed by optional custom slots. */
#define WS2812B_EFFECT_METHOD_CAPACITY (WS2812_EFFECT_BUILTIN_COUNT + WS2812B_EFFECT_CUSTOM_SLOTS)

/** @brief Immutable color and duration for a color-sequence effect. */
typedef struct
{
    uint32_t color;           /**< RGB888 step color. */
    rt_tick_t duration_ticks; /**< RT-Thread ticks for this step, greater than zero. */
} ws2812_effect_step_t;

/** @brief Spatial gradation parameters; each custom step must be at most its channel maximum. */
typedef struct
{
    uint32_t start_color; /**< Initial 0xRRGGBB; zero with other fields zero uses defaults. */
    uint32_t end_color;   /**< End 0xRRGGBB of the spatial gradient. */
    uint8_t step_r;       /**< Red change per period. */
    uint8_t step_g;       /**< Green change per period. */
    uint8_t step_b;       /**< Blue change per period. */
    uint8_t max_r;        /**< Red upper bound. */
    uint8_t max_g;        /**< Green upper bound. */
    uint8_t max_b;        /**< Blue upper bound. */
} ws2812_gradation_config_t;

/** @brief Generic parameters copied when a registered effect method starts. */
typedef struct
{
    uint16_t type;                       /**< Registered effect ID. */
    uint32_t color;                      /**< Primary 0xRRGGBB color. */
    uint32_t background_color;           /**< Secondary 0xRRGGBB color. */
    rt_tick_t period_ticks;              /**< RT-Thread ticks; BLINK requires an even period >= 2. */
    const ws2812_effect_step_t *steps;   /**< Sequence steps, copied by package. */
    uint16_t step_count;                 /**< Number of sequence entries. */
    ws2812_gradation_config_t gradation; /**< Spatial/temporal gradation parameters. */
} ws2812_effect_config_t;

/**
 * @brief Initialize candidate effect state without sending SPI or changing an active effect.
 * @note The candidate state is copied to the device slot on success; do not retain its pointer.
 */
typedef rt_err_t (*ws2812_effect_start_fn_t)(ws2812_t dev, void *state,
                                             const ws2812_effect_config_t *config);
/** @brief Render callback, run from the selected LED polling thread. */
typedef rt_err_t (*ws2812_effect_update_fn_t)(ws2812_t dev, void *state,
                                              const ws2812_effect_config_t *config,
                                              rt_tick_t now, rt_bool_t force,
                                              rt_bool_t *changed);
/** @brief Stop callback for a given effect instance. */
typedef void (*ws2812_effect_stop_fn_t)(ws2812_t dev, void *state);

/** @brief One reusable effect method registered before polling starts. */
typedef struct
{
    uint16_t id;                      /**< Method ID. */
    rt_size_t state_size;             /**< Bytes needed for independent device state. */
    ws2812_effect_start_fn_t start;   /**< Initialize state for this device. */
    ws2812_effect_update_fn_t update; /**< Generate changed pixels; never sends SPI. */
    ws2812_effect_stop_fn_t stop;     /**< Optional cleanup before replacing method. */
} ws2812_effect_method_t;

/**
 * @brief Register an effect at its own numeric method->id index during initialization.
 * @param method Static descriptor kept alive by the caller.
 * @return RT_EOK or -RT_EINVAL when the indexed slot is invalid or occupied.
 */
rt_err_t ws2812_effect_register(const ws2812_effect_method_t *method);

/**
 * @brief Register a method at the array index equal to its ID.
 * @param slot Requested array index, below WS2812B_EFFECT_METHOD_CAPACITY.
 * @param method Static descriptor kept alive for device lifetime.
 * @return RT_EOK or -RT_EINVAL if slot != method->id or the slot is occupied.
 */
rt_err_t ws2812_effect_register_at(uint8_t slot, const ws2812_effect_method_t *method);

/**
 * @brief Register the enabled built-in effects during initialization, once per boot.
 * @return RT_EOK if all method descriptors fit the configured method slots.
 */
rt_err_t ws2812_effect_register_builtins(void);

/**
 * @brief Replace one device's current effect without sending SPI immediately.
 * @param dev Initialized WS2812 device with a stable registry index.
 * @param config Selected registered method ID, color, period and any sequence steps.
 * @return RT_EOK on success, or an RT-Thread error code on invalid configuration.
 * @note A failed start leaves the previously selected effect active and unchanged.
 * @note BLINK requires an even period of at least 2 ticks. With parameter checks enabled,
 *       odd periods return -RT_EINVAL; otherwise callers must provide valid even periods.
 * @note Sequence steps are copied into device-owned storage before this call returns.
 * @note Serialize effect changes with the application-owned polling context.
 */
rt_err_t ws2812_effect_start(ws2812_t dev, const ws2812_effect_config_t *config);

/**
 * @brief Select a copied color/duration sequence without constructing a generic effect configuration.
 * @param dev Initialized WS2812 device whose current effect is replaced.
 * @param steps Non-overlapping sequence steps; copied before this function returns.
 * @param step_count Number of steps; must be in the range 1..WS2812B_EFFECT_MAX_STEPS.
 * @return RT_EOK on success, or the error returned by ws2812_effect_start().
 * @note All durations use RT-Thread ticks. The application must poll to render the first frame.
 * @note The caller serializes effect changes with the application polling context.
 * @note Returns -RT_EINVAL when the SEQUENCE built-in is disabled at build time.
 */
rt_err_t ws2812_effect_start_sequence(ws2812_t dev, const ws2812_effect_step_t *steps, uint16_t step_count);

/**
 * @brief Stop the current effect while keeping its last rendered frame.
 * @param dev Device whose active method is stopped.
 * @return RT_EOK or -RT_EINVAL for an invalid device handle.
 */
rt_err_t ws2812_effect_stop(ws2812_t dev);

#ifdef WS2812B_USING_KCONFIG_DEVICES
/**
 * @brief Select the configured default effect, or leave a slot without a default unchanged.
 * @param slot Named Kconfig device slot, from zero to the configured slot limit.
 * @return RT_EOK for a configured, disabled, or no-initial-effect slot; an error for an invalid method.
 * @note A no-initial-effect selection does not stop a currently active effect.
 * @note Used at boot and when business logic restores the normal effect after an alert.
 */
rt_err_t ws2812_effect_start_kconfig(uint8_t slot);

/**
 * @brief Apply enabled Kconfig slots' default effects without SPI; skip slots selecting none.
 * @return RT_EOK if each enabled slot accepts its default or explicitly selects no initial effect.
 */
rt_err_t ws2812_effect_init_kconfig(void);
#endif /* WS2812B_USING_KCONFIG_DEVICES */

/**
 * @brief Update one strip and send only when an effect changes pixels.
 * @param dev Created device.
 * @param now Current RT-Thread tick count.
 * @return RT_EOK or rendering/SPI error.
 */
rt_err_t ws2812_effect_poll(ws2812_t dev, rt_tick_t now);

/**
 * @brief Poll all registered devices from the one application-selected polling context.
 * @param now Current RT-Thread tick passed to each active effect.
 * @return RT_EOK if all updates succeed, otherwise the last encountered RT-Thread error.
 */
rt_err_t ws2812_effect_poll_all(rt_tick_t now);

#ifdef __cplusplus
}
#endif
#endif /* WS2812_EFFECT_H */
