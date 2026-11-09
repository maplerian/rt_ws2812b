/*
 * Copyright (c) 2026, wdfk-prog
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         first version
 */
#ifndef WS2812_CONFIG_H
#define WS2812_CONFIG_H

/* Numeric defaults for integrations that do not generate RT-Thread Kconfig values.
 * Boolean Kconfig switches are intentionally not defined to zero: #ifdef is used.
 */
#ifndef WS2812B_MAX_DEVICES
/** @brief Maximum number of LED devices in the package registry. */
#define WS2812B_MAX_DEVICES 3
#endif /* !defined(WS2812B_MAX_DEVICES) */
#ifndef WS2812B_KCONFIG_SLOT_COUNT
/** @brief Number of named devices addressable through Kconfig slots (up to three). */
#define WS2812B_KCONFIG_SLOT_COUNT 3
#endif /* !defined(WS2812B_KCONFIG_SLOT_COUNT) */
#ifndef WS2812B_STATIC_MAX_CLOCKS_PER_BIT
/** @brief SPI clocks per WS2812 bit assumed for automatic static frame budgets. */
#define WS2812B_STATIC_MAX_CLOCKS_PER_BIT 12
#endif /* !defined(WS2812B_STATIC_MAX_CLOCKS_PER_BIT) */
#ifndef WS2812B_FAST_MAX_CLOCKS_PER_BIT
/** @brief Maximum SPI clock width accepted by the fast nibble encoder. */
#define WS2812B_FAST_MAX_CLOCKS_PER_BIT 12
#endif /* !defined(WS2812B_FAST_MAX_CLOCKS_PER_BIT) */
#ifndef WS2812B_STATIC_EXTRA_PIXELS
/** @brief Number of extra RGB888 pixels reserved for manually created static devices. */
#define WS2812B_STATIC_EXTRA_PIXELS 0
#endif /* !defined(WS2812B_STATIC_EXTRA_PIXELS) */
#ifndef WS2812B_STATIC_EXTRA_FRAME_BYTES
/** @brief Extra SPI frame bytes reserved for manually created static devices. */
#define WS2812B_STATIC_EXTRA_FRAME_BYTES 0
#endif /* !defined(WS2812B_STATIC_EXTRA_FRAME_BYTES) */
#ifndef WS2812B_EFFECT_CUSTOM_SLOTS
/** @brief Extra effect slots reserved for application-registered methods. */
#define WS2812B_EFFECT_CUSTOM_SLOTS 0
#endif /* !defined(WS2812B_EFFECT_CUSTOM_SLOTS) */
#ifndef WS2812B_EFFECT_STATE_BYTES
/** @brief Per-device byte capacity for private effect method runtime state. */
#define WS2812B_EFFECT_STATE_BYTES 64
#endif /* !defined(WS2812B_EFFECT_STATE_BYTES) */
#ifndef WS2812B_EFFECT_MAX_STEPS
/** @brief Maximum copied steps in each device's SEQUENCE effect. */
#define WS2812B_EFFECT_MAX_STEPS 16
#endif /* !defined(WS2812B_EFFECT_MAX_STEPS) */

/** @brief RGB channel count of one GRB-formatted WS2812 pixel. */
#define WS2812_CHANNEL_COUNT 3U
/** @brief Number of protocol bits per RGB channel. */
#define WS2812_CHANNEL_BITS 8U
/** @brief Number of nibble combinations, from 0x0 to 0xf. */
#define WS2812_FAST_NIBBLE_COUNT 16U
/** @brief Maximum SPI bytes representing four LED bits; capacity is set by Kconfig. */
#define WS2812_FAST_NIBBLE_MAX_BYTES ((WS2812B_FAST_MAX_CLOCKS_PER_BIT + 1U) / 2U)
/** @brief Minimum SPI clocks needed to distinguish 0 and 1 pulse widths. */
#define WS2812_MIN_CLOCKS_PER_BIT 3U
/** @brief Maximum search/packing width in clocks per logical LED bit. */
#define WS2812_MAX_CLOCKS_PER_BIT 96U
/** @brief Default reset-low interval in microseconds. */
#define WS2812_RESET_DEFAULT_US 300U
/** @brief Conservatively allowed minimum reset-low time for classic devices. */
#define WS2812_RESET_LEGACY_MIN_US 60U
/** @brief Conservatively allowed minimum reset-low time for V5 devices. */
#define WS2812_RESET_V5_MIN_US 300U
/** @brief Number of SPI bit-times in one second, for integer time conversion. */
#define WS2812_SPI_BITS_PER_SECOND 1000000ULL
/** @brief Numerator to convert us at SPI Hz into bytes. */
#define WS2812_SPI_RESET_BYTES_DIVISOR 8000000ULL
/** @brief Reserved value outside the range of valid static Kconfig slots. */
#define WS2812_DYNAMIC_STORAGE_SLOT 0xFFU

#endif /* WS2812_CONFIG_H */
