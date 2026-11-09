/*
 * Copyright (c) 2026, wdfk-prog
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         first version
 */
#ifndef WS2812_EFFECT_INTERNAL_H
#define WS2812_EFFECT_INTERNAL_H
#include "ws2812_effect.h"

/**
 * @brief Interpolate two RGB888 colors with a normalized 0..255 coefficient.
 * @param start First RGB888 color in 0x00RRGGBB format.
 * @param end Last RGB888 color in 0x00RRGGBB format.
 * @param position Blend fraction: zero selects start, 255 selects end.
 * @return Interpolated RGB888 color.
 */
uint32_t ws2812_effect_blend(uint32_t start, uint32_t end, uint32_t position);

/**
 * @brief Request a fresh effect frame after changing the device's RGB encoding.
 * @param dev Device whose currently active effect will be redrawn by its next poll.
 * @note Internal, single-owner polling contract; never sends SPI here.
 */
void ws2812_effect_mark_dirty(ws2812_t dev);

#ifdef WS2812B_EFFECT_SOLID
/** @brief Solid-color builtin effect descriptor. */
extern const ws2812_effect_method_t ws2812_method_solid;
#endif /* WS2812B_EFFECT_SOLID */
#ifdef WS2812B_EFFECT_BLINK
/** @brief Blink builtin effect descriptor. */
extern const ws2812_effect_method_t ws2812_method_blink;
#endif /* WS2812B_EFFECT_BLINK */
#ifdef WS2812B_EFFECT_BREATHE
/** @brief Breathe builtin effect descriptor. */
extern const ws2812_effect_method_t ws2812_method_breathe;
#endif /* WS2812B_EFFECT_BREATHE */
#ifdef WS2812B_EFFECT_GRADIENT
/** @brief Temporal gradient builtin effect descriptor. */
extern const ws2812_effect_method_t ws2812_method_gradient;
#endif /* WS2812B_EFFECT_GRADIENT */
#ifdef WS2812B_EFFECT_GRADATION
/** @brief Spatial RGB gradation builtin effect descriptor. */
extern const ws2812_effect_method_t ws2812_method_gradation;
#endif /* WS2812B_EFFECT_GRADATION */
#ifdef WS2812B_EFFECT_CHASE
/** @brief Chase builtin effect descriptor. */
extern const ws2812_effect_method_t ws2812_method_chase;
#endif /* WS2812B_EFFECT_CHASE */
#ifdef WS2812B_EFFECT_SEQUENCE
/** @brief Sequence builtin effect descriptor. */
extern const ws2812_effect_method_t ws2812_method_sequence;
#endif /* WS2812B_EFFECT_SEQUENCE */

#ifdef WS2812B_USING_KCONFIG_DEVICES
/* A slot is selected only when it is within the configured count and enabled. */
#if (WS2812B_KCONFIG_SLOT_COUNT > 0) && defined(WS2812B_KCONFIG_DEVICE0)
#define WS2812_FOR_SLOT_0(M) M(0)
#else
#define WS2812_FOR_SLOT_0(M)
#endif /* WS2812B_KCONFIG_SLOT_COUNT > 0 && WS2812B_KCONFIG_DEVICE0 */
#if (WS2812B_KCONFIG_SLOT_COUNT > 1) && defined(WS2812B_KCONFIG_DEVICE1)
#define WS2812_FOR_SLOT_1(M) M(1)
#else
#define WS2812_FOR_SLOT_1(M)
#endif /* WS2812B_KCONFIG_SLOT_COUNT > 1 && WS2812B_KCONFIG_DEVICE1 */
#if (WS2812B_KCONFIG_SLOT_COUNT > 2) && defined(WS2812B_KCONFIG_DEVICE2)
#define WS2812_FOR_SLOT_2(M) M(2)
#else
#define WS2812_FOR_SLOT_2(M)
#endif /* WS2812B_KCONFIG_SLOT_COUNT > 2 && WS2812B_KCONFIG_DEVICE2 */

/** @brief Expand a macro once for each enabled named device slot. */
#define WS2812_FOR_EACH_ENABLED_SLOT(M) \
    WS2812_FOR_SLOT_0(M)                \
    WS2812_FOR_SLOT_1(M)                \
    WS2812_FOR_SLOT_2(M)

/** @brief Read the hidden stable Kconfig effect kind for slot n. */
#define WS2812_DEV_KIND_(n) WS2812B_DEV##n##_INITIAL_KIND
#define WS2812_DEV_KIND(n)  WS2812_DEV_KIND_(n)

/** @brief Read the configured initial period for slot n. */
#define WS2812_DEV_PERIOD_(n) WS2812B_DEV##n##_INITIAL_PERIOD_TICKS
#define WS2812_DEV_PERIOD(n)  WS2812_DEV_PERIOD_(n)

/** @brief Stable Kconfig kind for BLINK (not the compiled effect ID). */
#define WS2812_INITIAL_KIND_BLINK 2U
#endif /* WS2812B_USING_KCONFIG_DEVICES */

#endif /* WS2812_EFFECT_INTERNAL_H */
