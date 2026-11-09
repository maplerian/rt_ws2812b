/*
 * @Author: maplerian
 * @Date: 2020-07-11 16:03:03
 * @LastEditors: maplerian
 * @LastEditTime: 2020-07-11 16:53:52
 * @Description: file content
 */

/*
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         add configurable SPI timing and effects
 */
#ifndef __WS2812__H__
#define __WS2812__H__

#include <stdint.h>
#include <rtthread.h>
#include <rtdevice.h>
#include "ws2812_config.h"
#include "ws2812_color.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Legacy SPI zero symbol using 6 MHz and eight SPI clocks. */
#define WS2812_0 0xC0U
/** @brief Legacy SPI one symbol using 6 MHz and eight SPI clocks. */
#define WS2812_1 0xF0U
/** @brief Legacy bytes per logical protocol bit. */
#define OneBitOccupyByte 1U
/** @brief Legacy bytes for one RGB888 pixel. */
#define OneNodeBuffLength (8U * 3U * OneBitOccupyByte)

/** @brief Supported WS2812B timing revisions. */
typedef enum
{
    WS2812_VARIANT_LEGACY = 0, /**< Traditional WS2812B timing. */
    WS2812_VARIANT_V5 = 1      /**< WS2812B-V5 timing. */
} ws2812_variant_t;

/** @brief Board SPI attachment callback, invoked before the RT-Thread device lookup. */
typedef rt_err_t (*ws2812_spi_attach_fn_t)(const char *bus_name, const char *device_name, void *user_data);

/** @brief Attach callback and caller-owned context for a particular slot or device. */
typedef struct
{
    ws2812_spi_attach_fn_t callback; /**< SPI device registration callback. */
    void *user_data;                 /**< Opaque user context. */
} ws2812_attach_t;

/** @brief Logical LED bit width and high-pulse clock lengths at the actual SPI frequency. */
typedef struct
{
    uint8_t clocks_per_bit; /**< Total SPI clocks for one LED bit. */
    uint8_t high0;          /**< Leading SPI high clocks for logical zero. */
    uint8_t high1;          /**< Leading SPI high clocks for logical one. */
} ws2812_timing_t;

/** @brief SPI device creation configuration. */
typedef struct
{
    const char *spi_bus_name;    /**< SPI bus for attach callback. */
    const char *spi_device_name; /**< SPI device name in RT-Thread registry. */
    uint16_t leds_num;           /**< Physical LED count. */
    uint32_t spi_hz;             /**< Actual on-wire SCK frequency (Hz). */
    ws2812_variant_t variant;    /**< LED timing revision. */
    uint32_t reset_us;           /**< Reset-low microseconds; zero selects default. */
    uint16_t spi_mode;           /**< RT_SPI_MODE_0 or RT_SPI_MODE_3. */
    ws2812_attach_t attach;      /**< Optional per-device attach callback. */
    rt_bool_t timing_direct;     /**< Use timing below instead of table/search. */
    ws2812_timing_t timing;      /**< Explicit protocol waveform. */
} ws2812_config_t;

/** @brief Gamma selection; CUSTOM is a runtime-generated, per-device table. */
typedef enum
{
    WS2812_GAMMA_NONE = 0,  /**< Bypass gamma, keep brightness scaling. */
    WS2812_GAMMA_22 = 1,    /**< Fixed exponent 2.2. */
    WS2812_GAMMA_28 = 2,    /**< Fixed exponent 2.8. */
    WS2812_GAMMA_CUSTOM = 3 /**< Runtime exponent set with gamma_x100. */
} ws2812_gamma_t;

/** @brief WS2812 device; the first three fields preserve legacy source layout. */
struct ws2812
{
    struct rt_spi_device *spi;                                                    /**< Board-owned SPI device. */
    uint8_t *buff;                                                                /**< Packed pixel data after reset prefix. */
    uint16_t node_len;                                                            /**< Number of LEDs. */
    uint8_t *tx_buffer;                                                           /**< Whole SPI frame including reset bytes. */
    rt_size_t tx_length;                                                          /**< Whole encoded frame byte length. */
    rt_size_t pixel_offset;                                                       /**< Prefix reset bytes. */
    uint32_t *pixels;                                                             /**< Uncorrected RGB888 values. */
    uint32_t spi_hz;                                                              /**< Actual SCK frequency. */
    uint32_t reset_us;                                                            /**< Reset-low time. */
    ws2812_timing_t timing;                                                       /**< Selected waveform. */
    ws2812_variant_t variant;                                                     /**< Timing revision. */
    uint8_t brightness;                                                           /**< Global 0..255 scaling. */
    ws2812_gamma_t gamma;                                                         /**< Gamma lookup selection. */
    uint8_t fast_nibble_bytes;                                                    /**< Encoded bytes for four LED bits. */
    uint8_t storage_slot;                                                         /**< Kconfig slot or 0xff. */
    uint8_t registry_index;                                                       /**< Stable index in device array. */
    uint8_t fast_nibbles[WS2812_FAST_NIBBLE_COUNT][WS2812_FAST_NIBBLE_MAX_BYTES]; /**< Cached SPI symbols. */
#ifdef WS2812B_GAMMA_COMPUTE
    uint8_t gamma_custom_table[256]; /**< Computed lookup, isolated per device. */
    uint16_t gamma_x100;             /**< Current exponent times 100. */
#endif                               /* WS2812B_GAMMA_COMPUTE */
};

typedef struct ws2812 *ws2812_t;

/**
 * @brief Register a per-slot board SPI attach callback before initialization.
 * @param slot Zero-based Kconfig slot index.
 * @param callback Board registration method, or RT_NULL for BSP-precreated devices.
 * @param user_data Caller-owned context passed to callback.
 * @return RT_EOK or -RT_EINVAL for an invalid slot when parameter checks are enabled.
 */
rt_err_t ws2812_set_kconfig_attach(uint8_t slot, ws2812_spi_attach_fn_t callback, void *user_data);

#if defined(WS2812B_TIMING_TABLE) || defined(WS2812B_TIMING_SEARCH)
/**
 * @brief Select SPI waveform clock counts from the preset table or search fallback.
 * @param spi_hz Actual SPI SCK frequency in Hz.
 * @param variant Selected LED timing revision (classic or V5).
 * @param timing Receives the selected SPI clock counts on success.
 * @return RT_EOK if a supported waveform is found, otherwise -RT_EINVAL.
 */
rt_err_t ws2812_timing_select(uint32_t spi_hz, ws2812_variant_t variant, ws2812_timing_t *timing);
#endif /* WS2812B_TIMING_TABLE || WS2812B_TIMING_SEARCH */

#ifdef WS2812B_CHECK_EXTENDED
/**
 * @brief Validate SPI high/low pulses and total bit duration against the LED timing limits.
 * @param spi_hz Actual SPI SCK frequency in Hz.
 * @param variant Selected LED timing revision (classic or V5).
 * @param timing SPI clocks per bit and high-clock counts to validate.
 * @return RT_EOK if the waveform satisfies the limits, otherwise -RT_EINVAL.
 */
rt_err_t ws2812_timing_validate(uint32_t spi_hz, ws2812_variant_t variant, const ws2812_timing_t *timing);
#endif /* WS2812B_CHECK_EXTENDED */

/**
 * @brief Create an SPI-backed WS2812 device with the configured memory backend.
 * @param config SPI device, LED count, waveform and optional attach callback.
 * @return New device handle or RT_NULL on initialization failure.
 */
ws2812_t ws2812_create_with_config(const ws2812_config_t *config);

/**
 * @brief Create a legacy WS2812 device with SPI mode 3 and 6 MHz C0/F0 encoding.
 * @param spi_name Registered SPI device name.
 * @param led_node_length Number of WS2812 pixels in the strip.
 * @return Device handle or RT_NULL when creation fails.
 */
ws2812_t ws2812_create(char *spi_name, uint16_t led_node_length);

/**
 * @brief Create enabled Kconfig devices once at initialization time.
 * @return RT_EOK when all enabled slots initialize, otherwise -RT_ERROR.
 */
rt_err_t ws2812_init_all(void);

/**
 * @brief Number of created devices; this registry is not modified after initialization.
 * @return Current number of registered WS2812 devices.
 */
rt_size_t ws2812_get_device_count(void);

/**
 * @brief Retrieve a registered device by its stable creation-order index.
 * @param index Creation-order index, starting at zero.
 * @return Device handle or RT_NULL if the index does not exist.
 */
ws2812_t ws2812_get_device(rt_size_t index);

/**
 * @brief Retrieve a configured device by its Kconfig slot rather than creation order.
 * @param slot Zero-based Kconfig slot index.
 * @return Device handle or RT_NULL for a disabled/out-of-range slot.
 */
ws2812_t ws2812_get_kconfig_device(uint8_t slot);

/**
 * @brief Find a registered WS2812 instance attached to one RT SPI device.
 * @param spi_device_name Name previously registered with RT-Thread.
 * @return First matching WS2812 device handle or RT_NULL.
 */
ws2812_t ws2812_get_device_by_name(const char *spi_device_name);

/**
 * @brief Update a single RGB888 pixel and its encoded SPI waveform, without sending.
 * @param dev Target WS2812 device.
 * @param index Zero-based LED index.
 * @param rgb888 Color in 0x00RRGGBB format.
 * @return RT_EOK or -RT_EINVAL for invalid arguments when checking is enabled.
 */
rt_err_t ws2812_set_pixel(ws2812_t dev, uint16_t index, uint32_t rgb888);

/**
 * @brief Fill a strip with one RGB888 value, copying the first encoded waveform.
 * @param dev Target WS2812 device.
 * @param rgb888 Color in 0x00RRGGBB format.
 * @return RT_EOK or a parameter error when checking is enabled.
 */
rt_err_t ws2812_fill(ws2812_t dev, uint32_t rgb888);

/**
 * @brief Set global 0..255 brightness and re-encode all saved original RGB pixels.
 * @param dev Target WS2812 device.
 * @param brightness Brightness factor: 0 is off, 255 is unchanged.
 * @return RT_EOK or a parameter error when checking is enabled.
 * @note If an effect is active, the next effect poll renders and sends the changed frame.
 */
rt_err_t ws2812_set_brightness(ws2812_t dev, uint8_t brightness);

/**
 * @brief Select one compiled Gamma correction mode on a device.
 * @param dev Target WS2812 device.
 * @param mode NONE, 2.2, 2.8, or a previously configured CUSTOM mode.
 * @return RT_EOK if supported, otherwise -RT_EINVAL.
 * @note An active effect is marked for redraw on its next application poll.
 */
rt_err_t ws2812_set_gamma(ws2812_t dev, ws2812_gamma_t mode);

/**
 * @brief Compatibility alias of ws2812_set_gamma, using the same enum parameter.
 * @param dev Target WS2812 device.
 * @param mode Selected compiled Gamma mode.
 * @return Same result as ws2812_set_gamma().
 */
rt_err_t ws2812_set_gamma_mode(ws2812_t dev, ws2812_gamma_t mode);

#ifdef WS2812B_GAMMA_COMPUTE
/**
 * @brief Regenerate and select a per-device Gamma lookup table.
 * @param dev Target WS2812 device.
 * @param gamma_x100 Exponent multiplied by 100 (50..500 represents 0.50..5.00).
 * @return RT_EOK on success, or -RT_EINVAL for invalid input when checks are enabled.
 * @note Requires WS2812B_GAMMA_COMPUTE; caller must supply valid parameters when checks are disabled.
 */
rt_err_t ws2812_set_gamma_custom(ws2812_t dev, uint16_t gamma_x100);
#endif /* WS2812B_GAMMA_COMPUTE */

/**
 * @brief Send the complete reset-prefix, encoded-pixel and reset-suffix SPI frame.
 * @param dev Target WS2812 device.
 * @return RT_EOK or an RT-Thread SPI transfer error.
 */
rt_err_t ws2812_send_frame(ws2812_t dev);

/**
 * @brief Get the full SPI transmission frame length in bytes.
 * @param dev Target WS2812 device.
 * @return Frame length, or zero for RT_NULL.
 */
rt_size_t ws2812_get_frame_size(ws2812_t dev);

/* Original public API names and argument types are immutable. */

/**
 * @brief Clear all LED pixels in the buffer without sending.
 *
 * @param ws2812 Target WS2812 device.
 * @note Legacy void API; errors from ws2812_fill() are not propagated.
 */
void ws2812_clear_buff(ws2812_t ws2812);

/**
 * @brief Set the RGB888 color of a single LED without sending.
 *
 * @param ws2812 Target WS2812 device.
 * @param color RGB888 color in 0x00RRGGBB format.
 * @param index Zero-based LED index.
 * @note Legacy void API; errors from ws2812_set_pixel() are not propagated.
 */
void ws2812_write_rgb_to_node(ws2812_t ws2812, uint32_t color, uint16_t index);

/**
 * @brief Set the RGB888 color of all LEDs without sending.
 *
 * @param ws2812 Target WS2812 device.
 * @param color RGB888 color in 0x00RRGGBB format.
 * @note Legacy void API; errors from ws2812_fill() are not propagated.
 */
void ws2812_write_rgb_to_all(ws2812_t ws2812, uint32_t color);

/**
 * @brief Send the complete encoded SPI frame to the LED strip.
 *
 * @param ws2812 Target WS2812 device.
 * @note Legacy void API; SPI transfer errors are not propagated.
 */
void ws2812_send(ws2812_t ws2812);

#ifdef __cplusplus
}
#endif
#endif /* __WS2812__H__ */
