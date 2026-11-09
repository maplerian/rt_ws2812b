/*
 * Copyright (c) 2026, wdfk-prog
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         first version
 */
#ifndef WS2812_COLOR_H
#define WS2812_COLOR_H

#include <stdint.h>

/** @brief RGB888 channels before the WS2812 wire-order conversion. */
typedef struct
{
    uint8_t r; /**< Red channel, 0..255. */
    uint8_t g; /**< Green channel, 0..255. */
    uint8_t b; /**< Blue channel, 0..255. */
} ws2812_rgb_t;

/** @brief Common RGB888 palette; arbitrary 0xRRGGBB inputs are also supported. */
typedef enum
{
    WS2812_COLOR_OFF = 0x000000U,     /**< Black/off. */
    WS2812_COLOR_RED = 0xFF0000U,     /**< Red. */
    WS2812_COLOR_ORANGE = 0xFF7F00U,  /**< Orange. */
    WS2812_COLOR_YELLOW = 0xFFFF00U,  /**< Yellow. */
    WS2812_COLOR_GREEN = 0x00FF00U,   /**< Green. */
    WS2812_COLOR_CYAN = 0x00FFFFU,    /**< Cyan. */
    WS2812_COLOR_BLUE = 0x0000FFU,    /**< Blue. */
    WS2812_COLOR_PURPLE = 0x7F00FFU,  /**< Purple. */
    WS2812_COLOR_MAGENTA = 0xFF00FFU, /**< Magenta. */
    WS2812_COLOR_WHITE = 0xFFFFFFU,   /**< White. */
    WS2812_COLOR_GOLD = 0xFFD700U     /**< Gold. */
} ws2812_color_t;

/* Original palette aliases retained for source compatibility. */
#define red_0             WS2812_COLOR_RED
#define brown_30          WS2812_COLOR_ORANGE
#define yellow_60         WS2812_COLOR_YELLOW
#define Dark_green_90     0x7FFF00U
#define Medium_green_120  WS2812_COLOR_GREEN
#define Light_green_150   0x00FF7FU
#define Baby_blue_180     WS2812_COLOR_CYAN
#define Medium_blue_210   0x007FFFU
#define Dark_blue_240     WS2812_COLOR_BLUE
#define modena_270        WS2812_COLOR_PURPLE
#define Medium_purple_300 WS2812_COLOR_MAGENTA
#define lilac_330         0xFF007FU
#define white_360         WS2812_COLOR_WHITE
#define off_390           WS2812_COLOR_OFF

#endif /* WS2812_COLOR_H */
