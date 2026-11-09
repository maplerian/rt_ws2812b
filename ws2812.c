/*
 * @Author: maplerian
 * @Date: 2020-07-11 16:02:23
 * @LastEditors: maplerian
 * @LastEditTime: 2020-07-11 22:08:53
 * @Description: file content
 */

/*
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         add multiple configurable WS2812B devices and SPI encoding
 */
#include "ws2812.h"
#ifdef WS2812B_USING_EFFECT
#include "effect/ws2812_effect_internal.h"
#endif /* WS2812B_USING_EFFECT */

#define DBG_TAG "ws2812b"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef WS2812B_USING_KCONFIG_DEVICES
/* Guard manually supplied or stale config headers too; runtime checks are optional. */
#if (defined(WS2812B_KCONFIG_DEVICE0) + defined(WS2812B_KCONFIG_DEVICE1) + \
     defined(WS2812B_KCONFIG_DEVICE2)) > WS2812B_MAX_DEVICES
#error "WS2812B_MAX_DEVICES must cover all enabled Kconfig device slots"
#endif /* enabled Kconfig slots > WS2812B_MAX_DEVICES */
#endif /* WS2812B_USING_KCONFIG_DEVICES */

#ifdef WS2812B_GAMMA_22
#include "gamma/ws2812_gamma22.h"
#endif /* WS2812B_GAMMA_22 */
#ifdef WS2812B_GAMMA_28
#include "gamma/ws2812_gamma28.h"
#endif /* WS2812B_GAMMA_28 */
#ifdef WS2812B_GAMMA_COMPUTE
#include "gamma/ws2812_gamma_compute.h"
#endif /* WS2812B_GAMMA_COMPUTE */

/** @brief Per-slot attachment and resolved device, indexed by Kconfig device number. */
typedef struct
{
    ws2812_t dev;           /**< Kconfig-created instance, or RT_NULL. */
    ws2812_attach_t attach; /**< Slot-specific callback supplied by the application. */
} ws2812_static_storage_t;

/** @brief Package-wide registry; device IDs do not change after startup. */
typedef struct
{
    ws2812_t devices[WS2812B_MAX_DEVICES];                       /**< Creation-ordered handles. */
    rt_size_t count;                                             /**< Registered device count. */
    ws2812_static_storage_t storage[WS2812B_KCONFIG_SLOT_COUNT]; /**< Kconfig slots and attachments. */
#ifdef WS2812B_MEMORY_STATIC
    rt_size_t pixels_used; /**< Allocated words in static RGB pool. */
    rt_size_t frames_used; /**< Allocated bytes in static SPI pool. */
#endif                     /* WS2812B_MEMORY_STATIC */
} ws2812_manager_t;

/** @brief Device objects and Kconfig slot callback registry. */
static ws2812_manager_t ws2812_manager;

#ifdef WS2812B_MEMORY_STATIC
/* A configured slot reserves its exact override size, or its auto-computed worst case.
 * RGB stores raw RGB888. Frames require N*3*clocks bytes plus reset-low bytes at both ends.
 */
#define WS2812_AUTO_FRAME(LEDS, HZ, RESET)               \
    ((LEDS) * 3ULL * WS2812B_STATIC_MAX_CLOCKS_PER_BIT + \
     2ULL * (((HZ) * 1ULL * (RESET) + WS2812_SPI_RESET_BYTES_DIVISOR - 1ULL) / WS2812_SPI_RESET_BYTES_DIVISOR))
#define WS2812_FRAME_CAPACITY(LEDS, HZ, RESET, OVERRIDE) \
    ((OVERRIDE) != 0 ? (OVERRIDE) : WS2812_AUTO_FRAME(LEDS, HZ, RESET))

#ifdef WS2812B_KCONFIG_DEVICE0
#define WS2812_SLOT0_PIXELS WS2812B_DEV0_LEDS
#define WS2812_SLOT0_FRAME  WS2812_FRAME_CAPACITY(WS2812B_DEV0_LEDS, WS2812B_DEV0_HZ, \
                                                  WS2812B_DEV0_RESET_US, WS2812B_DEV0_FRAME_BYTES)
#else
#define WS2812_SLOT0_PIXELS 0
#define WS2812_SLOT0_FRAME  0
#endif /* WS2812B_KCONFIG_DEVICE0 */
#ifdef WS2812B_KCONFIG_DEVICE1
#define WS2812_SLOT1_PIXELS WS2812B_DEV1_LEDS
#define WS2812_SLOT1_FRAME  WS2812_FRAME_CAPACITY(WS2812B_DEV1_LEDS, WS2812B_DEV1_HZ, \
                                                  WS2812B_DEV1_RESET_US, WS2812B_DEV1_FRAME_BYTES)
#else
#define WS2812_SLOT1_PIXELS 0
#define WS2812_SLOT1_FRAME  0
#endif /* WS2812B_KCONFIG_DEVICE1 */
#ifdef WS2812B_KCONFIG_DEVICE2
#define WS2812_SLOT2_PIXELS WS2812B_DEV2_LEDS
#define WS2812_SLOT2_FRAME  WS2812_FRAME_CAPACITY(WS2812B_DEV2_LEDS, WS2812B_DEV2_HZ, \
                                                  WS2812B_DEV2_RESET_US, WS2812B_DEV2_FRAME_BYTES)
#else
#define WS2812_SLOT2_PIXELS 0
#define WS2812_SLOT2_FRAME  0
#endif /* WS2812B_KCONFIG_DEVICE2 */

#define WS2812_STATIC_TOTAL_PIXELS \
    (WS2812_SLOT0_PIXELS + WS2812_SLOT1_PIXELS + WS2812_SLOT2_PIXELS + WS2812B_STATIC_EXTRA_PIXELS)
#define WS2812_STATIC_TOTAL_FRAME_BYTES \
    (WS2812_SLOT0_FRAME + WS2812_SLOT1_FRAME + WS2812_SLOT2_FRAME + WS2812B_STATIC_EXTRA_FRAME_BYTES)

/** @brief Compile-time device objects; one per possible creation slot. */
static struct ws2812 ws2812_static_devices[WS2812B_MAX_DEVICES];
/** @brief Shared but non-overlapping RGB888 slices, allocated only during initialization. */
static uint32_t ws2812_static_pixels[WS2812_STATIC_TOTAL_PIXELS ? WS2812_STATIC_TOTAL_PIXELS : 1];
/** @brief Shared non-overlapping SPI frame slices, allocated only during initialization. */
static uint8_t ws2812_static_frames[WS2812_STATIC_TOTAL_FRAME_BYTES ? WS2812_STATIC_TOTAL_FRAME_BYTES : 1];
#endif /* WS2812B_MEMORY_STATIC */

/** @brief One compiled Kconfig device definition; missing slots stay zero-initialized. */
typedef struct
{
    const char *bus;          /**< RT SPI bus used to attach the device. */
    const char *name;         /**< Registered RT SPI device name. */
    uint16_t leds;            /**< Logical pixel count. */
    uint32_t hz;              /**< Actual SCK in Hz. */
    uint32_t reset_us;        /**< Required reset-low time. */
    ws2812_variant_t variant; /**< LED protocol timing revision. */
    ws2812_timing_t timing;   /**< Explicit waveform in direct mode. */
    rt_bool_t enabled;        /**< True if enabled by Kconfig. */
} ws2812_kconfig_entry_t;

#if defined(WS2812B_USING_KCONFIG_DEVICES)
/** @brief Configured slot descriptors, addressed by Kconfig slot number. */
static const ws2812_kconfig_entry_t ws2812_kconfig_devices[WS2812B_KCONFIG_SLOT_COUNT] = {
#if WS2812B_KCONFIG_SLOT_COUNT > 0
#ifdef WS2812B_KCONFIG_DEVICE0
    [0] = { WS2812B_DEV0_BUS, WS2812B_DEV0_NAME, WS2812B_DEV0_LEDS, WS2812B_DEV0_HZ, WS2812B_DEV0_RESET_US, (ws2812_variant_t)WS2812B_DEV0_VARIANT, { WS2812B_DEV0_CLOCKS_PER_BIT, WS2812B_DEV0_T0H, WS2812B_DEV0_T1H }, RT_TRUE },
#endif /* WS2812B_KCONFIG_DEVICE0 */
#endif /* WS2812B_KCONFIG_SLOT_COUNT > 0 */
#if WS2812B_KCONFIG_SLOT_COUNT > 1
#ifdef WS2812B_KCONFIG_DEVICE1
    [1] = { WS2812B_DEV1_BUS, WS2812B_DEV1_NAME, WS2812B_DEV1_LEDS, WS2812B_DEV1_HZ, WS2812B_DEV1_RESET_US, (ws2812_variant_t)WS2812B_DEV1_VARIANT, { WS2812B_DEV1_CLOCKS_PER_BIT, WS2812B_DEV1_T0H, WS2812B_DEV1_T1H }, RT_TRUE },
#endif /* WS2812B_KCONFIG_DEVICE1 */
#endif /* WS2812B_KCONFIG_SLOT_COUNT > 1 */
#if WS2812B_KCONFIG_SLOT_COUNT > 2
#ifdef WS2812B_KCONFIG_DEVICE2
    [2] = { WS2812B_DEV2_BUS, WS2812B_DEV2_NAME, WS2812B_DEV2_LEDS, WS2812B_DEV2_HZ, WS2812B_DEV2_RESET_US, (ws2812_variant_t)WS2812B_DEV2_VARIANT, { WS2812B_DEV2_CLOCKS_PER_BIT, WS2812B_DEV2_T0H, WS2812B_DEV2_T1H }, RT_TRUE },
#endif /* WS2812B_KCONFIG_DEVICE2 */
#endif /* WS2812B_KCONFIG_SLOT_COUNT > 2 */
};
#endif /* WS2812B_USING_KCONFIG_DEVICES */

rt_err_t ws2812_set_kconfig_attach(uint8_t slot, ws2812_spi_attach_fn_t callback, void *user_data)
{
#ifdef WS2812B_CHECK_PARAMETERS
    if (slot >= WS2812B_KCONFIG_SLOT_COUNT)
    {
        return -RT_EINVAL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    ws2812_manager.storage[slot].attach.callback = callback;
    ws2812_manager.storage[slot].attach.user_data = user_data;
    return RT_EOK;
}

/**
 * @brief Correct one RGB component after exact rounded global brightness scaling.
 * @param dev Device-local brightness and Gamma selection.
 * @param channel Original 8-bit RGB channel.
 * @return Adjusted channel value.
 */
static uint8_t ws2812_adjust_component(ws2812_t dev, uint8_t channel)
{
    /* Exact identity for 0 <= p = channel*brightness <= 65025:
     * round(p/255) == ((p+128)+((p+128)>>8))>>8. Plain p>>8 is NOT equivalent. */
    uint32_t value = (uint32_t)channel * dev->brightness + 128U;
    value = (value + (value >> 8U)) >> 8U;
    switch (dev->gamma)
    {
#ifdef WS2812B_GAMMA_22
    case WS2812_GAMMA_22:
        return ws2812_gamma22[value];
#endif /* WS2812B_GAMMA_22 */
#ifdef WS2812B_GAMMA_28
    case WS2812_GAMMA_28:
        return ws2812_gamma28[value];
#endif /* WS2812B_GAMMA_28 */
#ifdef WS2812B_GAMMA_COMPUTE
    case WS2812_GAMMA_CUSTOM:
        return dev->gamma_custom_table[value];
#endif /* WS2812B_GAMMA_COMPUTE */
    case WS2812_GAMMA_NONE:
    default:
        return (uint8_t)value;
    }
}

/**
 * @brief Set one MSB-first SPI output bit using unsigned shifts and masks.
 * @param dst Target encoded SPI frame bytes.
 * @param position Zero-based bit offset within the encoded pixel.
 */
static void ws2812_set_wire_bit(uint8_t *dst, uint16_t position)
{
    /* position/8 and position%8 are exactly >>3 and &7 for unsigned positions. */
    dst[position >> 3U] |= (uint8_t)(0x80U >> (position & 7U));
}

/**
 * @brief Build all 16 four-bit GRB waveform templates for the selected clock width.
 * @param dev Device whose timing and local LUT are initialized.
 * @note A nibble represents four LED data bits, each expanded to its own SPI high/low pulse.
 */
static void ws2812_prepare_fast_nibbles(ws2812_t dev)
{
    uint8_t nibble;
    const uint8_t width = dev->timing.clocks_per_bit;
    /* Four WS bits take 4*width SPI bits, or width/2 bytes when width is even. */
    dev->fast_nibble_bytes = (uint8_t)(width >> 1U);
    for (nibble = 0U; nibble < WS2812_FAST_NIBBLE_COUNT; ++nibble)
    {
        uint8_t bit;
        uint8_t *dst = dev->fast_nibbles[nibble];
        rt_memset(dst, 0, dev->fast_nibble_bytes);
        for (bit = 0U; bit < 4U; ++bit)
        {
            uint8_t high = (nibble & (0x08U >> bit)) ? dev->timing.high1 : dev->timing.high0;
            uint8_t clk;
            for (clk = 0U; clk < high; ++clk)
            {
                ws2812_set_wire_bit(dst, (uint16_t)bit * width + clk);
            }
        }
    }
}

#if defined(WS2812B_ENCODER_AUTO) || defined(WS2812B_ENCODER_SELF_CHECK)
/**
 * @brief Encode a GRB pixel bit by bit for the generic SPI waveform backend.
 * @param dst Output buffer holding three channels times clocks_per_bit bytes.
 * @param grb Original 8-bit channel values, ordered G, R, B.
 * @param timing Chosen SPI pulse width parameters for logical zero and one.
 */
static void ws2812_encode_generic(uint8_t *dst, const uint8_t grb[WS2812_CHANNEL_COUNT],
                                  const ws2812_timing_t *timing)
{
    rt_size_t position = 0U;
    uint8_t channel;
    rt_memset(dst, 0, (rt_size_t)WS2812_CHANNEL_COUNT * timing->clocks_per_bit);
    for (channel = 0U; channel < WS2812_CHANNEL_COUNT; ++channel)
    {
        int bit;
        for (bit = 7; bit >= 0; --bit)
        {
            uint8_t high = (grb[channel] & (1U << bit)) ? timing->high1 : timing->high0;
            uint8_t clock;
            for (clock = 0U; clock < timing->clocks_per_bit; ++clock, ++position)
            {
                if (clock < high)
                {
                    ws2812_set_wire_bit(dst, (uint16_t)position);
                }
            }
        }
    }
}
#endif /* WS2812B_ENCODER_AUTO || WS2812B_ENCODER_SELF_CHECK */

/**
 * @brief Encode G, R and B using six cached nibble lookups (two per channel).
 * @param dev Device with sixteen precomputed SPI nibble waveforms.
 * @param dst Destination spanning three clocks_per_bit bytes.
 * @param grb Eight-bit channels ordered Green, Red, Blue.
 */
static void ws2812_encode_fast(ws2812_t dev, uint8_t *dst, const uint8_t grb[WS2812_CHANNEL_COUNT])
{
    uint8_t channel;
    const rt_size_t n = dev->fast_nibble_bytes;
    for (channel = 0U; channel < WS2812_CHANNEL_COUNT; ++channel)
    {
        /* High nibble [7:4] precedes low nibble [3:0] on the SPI wire. */
        rt_memcpy(dst, dev->fast_nibbles[grb[channel] >> 4U], n);
        rt_memcpy(dst + n, dev->fast_nibbles[grb[channel] & 0x0FU], n);
        dst += n << 1U;
    }
}

#ifdef WS2812B_ENCODER_SELF_CHECK
/**
 * @brief Compare nibble LUT output against reference encoding on fixed RGB samples.
 * @param dev Device containing waveform timing and initialized nibble LUT.
 * @return RT_TRUE when every sample matches, RT_FALSE on a mismatch.
 */
static rt_bool_t ws2812_check_encoder(ws2812_t dev)
{
    static const uint8_t examples[][3] = { { 0U, 0U, 0U }, { 255U, 255U, 255U }, { 0x12U, 0x34U, 0x56U }, { 0xAAU, 0x00U, 0x55U } };
    uint8_t reference[WS2812_CHANNEL_COUNT * WS2812_MAX_CLOCKS_PER_BIT];
    uint8_t actual[WS2812_CHANNEL_COUNT * WS2812_MAX_CLOCKS_PER_BIT];
    rt_size_t i;
    if (dev->fast_nibble_bytes == 0U)
    {
        return RT_TRUE;
    }
    for (i = 0U; i < sizeof(examples) / sizeof(examples[0]); ++i)
    {
        ws2812_encode_generic(reference, examples[i], &dev->timing);
        ws2812_encode_fast(dev, actual, examples[i]);
        if (rt_memcmp(reference, actual, (rt_size_t)WS2812_CHANNEL_COUNT * dev->timing.clocks_per_bit) != 0)
        {
            return RT_FALSE;
        }
    }
    return RT_TRUE;
}
#endif /* WS2812B_ENCODER_SELF_CHECK */

/**
 * @brief Encode one original RGB888 pixel into its SPI waveform, without transmitting.
 * @param dev Device with selected SPI timing, brightness and Gamma correction.
 * @param index Zero-based destination LED index in the encoded frame.
 * @param rgb888 Original RGB888 color in 0x00RRGGBB byte order.
 * @note The data is converted to the WS2812 wire order G, R, B.
 */
static void ws2812_encode_pixel(ws2812_t dev, uint16_t index, uint32_t rgb888)
{
    /* RGB888 uses 0x00RRGGBB, but the LED wire protocol sends G7..G0,
     * then R7..R0, then B7..B0. Brightness/Gamma are applied per channel.
     *   G = bits 15:8, R = bits 23:16, B = bits 7:0.
     */
    const uint8_t grb[WS2812_CHANNEL_COUNT] = {
        ws2812_adjust_component(dev, (uint8_t)(rgb888 >> 8U)),
        ws2812_adjust_component(dev, (uint8_t)(rgb888 >> 16U)),
        ws2812_adjust_component(dev, (uint8_t)rgb888)
    };
    uint8_t *dst = dev->buff + (rt_size_t)index * WS2812_CHANNEL_COUNT * dev->timing.clocks_per_bit;
    if (dev->fast_nibble_bytes != 0U)
    {
        ws2812_encode_fast(dev, dst, grb);
    }
#ifdef WS2812B_ENCODER_AUTO
    else
    {
        ws2812_encode_generic(dst, grb, &dev->timing);
    }
#endif /* WS2812B_ENCODER_AUTO */
}

/**
 * @brief Re-encode saved RGB888 pixels after brightness or Gamma changes.
 * @param dev Target device owning the pixel and frame buffers.
 */
static void ws2812_encode_all(ws2812_t dev)
{
    uint16_t i;
    for (i = 0U; i < dev->node_len; ++i)
    {
        ws2812_encode_pixel(dev, i, dev->pixels[i]);
    }
}

/**
 * @brief Create an LED device using only the allocation backend selected at build time.
 * @param config SPI, LED count and symbol timing.
 * @param storage_slot Kconfig slot for this creation or dynamic-slot sentinel.
 * @param frame_budget Reserved Kconfig static frame capacity; zero for a manual instance.
 * @return Created instance or RT_NULL on error. No public deletion is provided.
 */
static ws2812_t ws2812_create_internal(const ws2812_config_t *config, uint8_t storage_slot, rt_size_t frame_budget)
{
    ws2812_t dev;
    struct rt_spi_device *spi;
    struct rt_spi_configuration spi_config = { 0 };
    ws2812_timing_t timing;
    uint32_t reset_us;
    uint64_t reset_bytes;
    uint64_t frame_bytes;
    uint64_t pixels_bytes;
#ifdef WS2812B_MEMORY_STATIC
    rt_size_t reserved_frame;
#endif /* WS2812B_MEMORY_STATIC */

#ifdef WS2812B_CHECK_PARAMETERS
    /* Validate optional public arguments before using them; disable only for trusted callers. */
    if (config == RT_NULL || config->spi_device_name == RT_NULL || config->spi_device_name[0] == '\0' ||
        config->leds_num == 0U || config->spi_hz == 0U || ws2812_manager.count >= WS2812B_MAX_DEVICES ||
        (config->spi_mode != RT_SPI_MODE_0 && config->spi_mode != RT_SPI_MODE_3))
    {
        return RT_NULL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */

    if (config->timing_direct)
    {
        timing = config->timing;
    }
    else
    {
#if defined(WS2812B_TIMING_TABLE) || defined(WS2812B_TIMING_SEARCH)
        if (ws2812_timing_select(config->spi_hz, config->variant, &timing) != RT_EOK)
        {
            return RT_NULL;
        }
#else
        /* DIRECT builds exclude both the lookup table and waveform search implementation. */
        LOG_E("Direct timing configuration is required");
        return RT_NULL;
#endif /* WS2812B_TIMING_TABLE || WS2812B_TIMING_SEARCH */
    }

#ifdef WS2812B_CHECK_PARAMETERS
    /* At least three clocks distinguish a zero's shorter high pulse from a one's high pulse.
     * Width <=96 is the implementation's packing/search limit, not a datasheet bound.
     * Each high duration must be >0 and <width to produce both high and low portions.
     * high1 > high0 is needed for distinct on-wire logical symbols.
     */
    if (timing.clocks_per_bit < WS2812_MIN_CLOCKS_PER_BIT ||
        timing.clocks_per_bit > WS2812_MAX_CLOCKS_PER_BIT || timing.high0 == 0U || timing.high1 == 0U ||
        timing.high0 >= timing.clocks_per_bit || timing.high1 >= timing.clocks_per_bit ||
        timing.high1 <= timing.high0)
    {
        return RT_NULL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
#ifdef WS2812B_CHECK_EXTENDED
    if (ws2812_timing_validate(config->spi_hz, config->variant, &timing) != RT_EOK)
    {
        return RT_NULL;
    }
#endif /* WS2812B_CHECK_EXTENDED */

#ifdef WS2812B_ENCODER_FAST_ONLY
    if ((timing.clocks_per_bit & 1U) != 0U || timing.clocks_per_bit > WS2812B_FAST_MAX_CLOCKS_PER_BIT)
    {
        LOG_W("FAST_ONLY requires an even SPI width <= %u", WS2812B_FAST_MAX_CLOCKS_PER_BIT);
        return RT_NULL;
    }
#endif /* WS2812B_ENCODER_FAST_ONLY */
    reset_us = config->reset_us ? config->reset_us : WS2812_RESET_DEFAULT_US;
#ifdef WS2812B_CHECK_PARAMETERS
    if (reset_us < ((config->variant == WS2812_VARIANT_V5) ? WS2812_RESET_V5_MIN_US : WS2812_RESET_LEGACY_MIN_US))
    {
        return RT_NULL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    /* Each GRB pixel has 24 protocol bits; each protocol bit is timing.clocks_per_bit SPI clocks.
     * 24*clocks/8 = 3*clocks encoded bytes per pixel, plus reset-low prefix and suffix.
     */
    /* Reset-low bytes = ceil(reset_us * SPI_Hz / (8 bits/byte * 10^6 us/s)).
     * The divisor is 8,000,000, not a power of two; replacing / with >>
     * would shorten the reset pulse. This executes only during creation.
     */
    reset_bytes = ((uint64_t)reset_us * config->spi_hz + WS2812_SPI_RESET_BYTES_DIVISOR - 1ULL) /
                  WS2812_SPI_RESET_BYTES_DIVISOR;
    frame_bytes = (uint64_t)config->leds_num * WS2812_CHANNEL_COUNT * timing.clocks_per_bit + 2ULL * reset_bytes;
    pixels_bytes = (uint64_t)config->leds_num * sizeof(uint32_t);
#ifdef WS2812B_CHECK_PARAMETERS
    /* Size overflow and static capacity checks are part of the optional trusted-caller contract. */
    if (!reset_bytes || frame_bytes > (uint64_t)(rt_size_t)-1 || pixels_bytes > (uint64_t)(rt_size_t)-1)
    {
        return RT_NULL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */

#ifdef WS2812B_MEMORY_STATIC
    reserved_frame = frame_budget ? frame_budget : (rt_size_t)frame_bytes;
#ifdef WS2812B_CHECK_PARAMETERS
    if (reserved_frame < frame_bytes ||
        ws2812_manager.pixels_used + config->leds_num > WS2812_STATIC_TOTAL_PIXELS ||
        ws2812_manager.frames_used + reserved_frame > WS2812_STATIC_TOTAL_FRAME_BYTES)
    {
        LOG_E("WS2812B static pool exhausted; increase Kconfig capacity");
        return RT_NULL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
#endif /* WS2812B_MEMORY_STATIC */

    if (config->attach.callback != RT_NULL &&
        config->attach.callback(config->spi_bus_name, config->spi_device_name, config->attach.user_data) != RT_EOK)
    {
        LOG_E("SPI attach failed: %s", config->spi_device_name);
        return RT_NULL;
    }
    spi = (struct rt_spi_device *)rt_device_find(config->spi_device_name);
    if (spi == RT_NULL || spi->parent.type != RT_Device_Class_SPIDevice)
    {
        LOG_E("SPI device unavailable: %s", config->spi_device_name);
        return RT_NULL;
    }

#ifdef WS2812B_MEMORY_STATIC
    dev = &ws2812_static_devices[ws2812_manager.count];
    rt_memset(dev, 0, sizeof(*dev));
    dev->tx_buffer = &ws2812_static_frames[ws2812_manager.frames_used];
    dev->pixels = &ws2812_static_pixels[ws2812_manager.pixels_used];
#else
    dev = (ws2812_t)rt_malloc(sizeof(*dev));
    if (dev == RT_NULL)
    {
        return RT_NULL;
    }
    rt_memset(dev, 0, sizeof(*dev));
    dev->tx_buffer = (uint8_t *)rt_malloc((rt_size_t)frame_bytes);
    if (dev->tx_buffer == RT_NULL)
    {
        goto failed;
    }
    dev->pixels = (uint32_t *)rt_malloc((rt_size_t)pixels_bytes);
    if (dev->pixels == RT_NULL)
    {
        goto failed;
    }
#endif /* WS2812B_MEMORY_STATIC */

    spi_config.mode = RT_SPI_MASTER | config->spi_mode | RT_SPI_MSB;
    spi_config.data_width = 8U;
    spi_config.max_hz = config->spi_hz;
    if (rt_spi_configure(spi, &spi_config) != RT_EOK)
    {
        LOG_E("SPI configuration failed: %s", config->spi_device_name);
        goto failed;
    }
    dev->spi = spi;
    dev->tx_length = (rt_size_t)frame_bytes;
    dev->pixel_offset = (rt_size_t)reset_bytes;
    dev->buff = dev->tx_buffer + dev->pixel_offset;
    dev->node_len = config->leds_num;
    dev->spi_hz = config->spi_hz;
    dev->reset_us = reset_us;
    dev->variant = config->variant;
    dev->timing = timing;
    dev->brightness = 255U;
    dev->gamma = WS2812_GAMMA_NONE;
    dev->storage_slot = storage_slot;
    dev->registry_index = (uint8_t)ws2812_manager.count;
    if ((timing.clocks_per_bit & 1U) == 0U && timing.clocks_per_bit <= WS2812B_FAST_MAX_CLOCKS_PER_BIT)
    {
        ws2812_prepare_fast_nibbles(dev);
    }
#ifdef WS2812B_ENCODER_SELF_CHECK
    if (!ws2812_check_encoder(dev))
    {
        LOG_E("SPI nibble encoder self-check failed");
        goto failed;
    }
#endif /* WS2812B_ENCODER_SELF_CHECK */
    rt_memset(dev->tx_buffer, 0, dev->tx_length);
    rt_memset(dev->pixels, 0, (rt_size_t)pixels_bytes);
    ws2812_encode_all(dev);

#ifdef WS2812B_MEMORY_STATIC
    ws2812_manager.pixels_used += config->leds_num;
    ws2812_manager.frames_used += reserved_frame;
#endif /* WS2812B_MEMORY_STATIC */
    ws2812_manager.devices[ws2812_manager.count++] = dev;
    if (storage_slot < WS2812B_KCONFIG_SLOT_COUNT)
    {
        ws2812_manager.storage[storage_slot].dev = dev;
    }
    LOG_I("Created %s (%u LEDs, %u Hz, %u SPI bytes)", config->spi_device_name, dev->node_len,
          (unsigned)dev->spi_hz, (unsigned)dev->tx_length);
    return dev;

failed:
#ifndef WS2812B_MEMORY_STATIC
    if (dev->pixels != RT_NULL)
    {
        rt_free(dev->pixels);
    }
    if (dev->tx_buffer != RT_NULL)
    {
        rt_free(dev->tx_buffer);
    }
    rt_free(dev);
#else
    rt_memset(dev, 0, sizeof(*dev));
#endif /* WS2812B_MEMORY_STATIC */
    return RT_NULL;
}

ws2812_t ws2812_create_with_config(const ws2812_config_t *config)
{
    return ws2812_create_internal(config, WS2812_DYNAMIC_STORAGE_SLOT, 0U);
}

ws2812_t ws2812_create(char *spi_name, uint16_t led_node_length)
{
    ws2812_config_t config;
    rt_memset(&config, 0, sizeof(config));
    config.spi_device_name = spi_name;
    config.leds_num = led_node_length;
    config.spi_hz = 6000000U;
    config.variant = WS2812_VARIANT_V5;
    config.spi_mode = RT_SPI_MODE_3;
    config.timing_direct = RT_TRUE;
    config.timing.clocks_per_bit = 8U;
    config.timing.high0 = 2U; /* Produces the original 0xC0 SPI byte. */
    config.timing.high1 = 4U; /* Produces the original 0xF0 SPI byte. */
    return ws2812_create_with_config(&config);
}

rt_size_t ws2812_get_device_count(void)
{
    return ws2812_manager.count;
}

ws2812_t ws2812_get_device(rt_size_t index)
{
    return (index < ws2812_manager.count) ? ws2812_manager.devices[index] : RT_NULL;
}

ws2812_t ws2812_get_kconfig_device(uint8_t slot)
{
    return (slot < WS2812B_KCONFIG_SLOT_COUNT) ? ws2812_manager.storage[slot].dev : RT_NULL;
}

ws2812_t ws2812_get_device_by_name(const char *spi_device_name)
{
    rt_device_t spi;
    rt_size_t index;
#ifdef WS2812B_CHECK_PARAMETERS
    if (spi_device_name == RT_NULL)
    {
        return RT_NULL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    spi = rt_device_find(spi_device_name);
    for (index = 0U; index < ws2812_manager.count; ++index)
    {
        if (ws2812_manager.devices[index]->spi == (struct rt_spi_device *)spi)
        {
            return ws2812_manager.devices[index];
        }
    }
    return RT_NULL;
}

rt_err_t ws2812_set_pixel(ws2812_t dev, uint16_t index, uint32_t rgb888)
{
#ifdef WS2812B_CHECK_PARAMETERS
    if (dev == RT_NULL || index >= dev->node_len)
    {
        return -RT_EINVAL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    dev->pixels[index] = rgb888 & 0xFFFFFFU;
    ws2812_encode_pixel(dev, index, dev->pixels[index]);
    return RT_EOK;
}

rt_err_t ws2812_fill(ws2812_t dev, uint32_t rgb888)
{
    uint16_t i;
    rt_size_t pixel_bytes;
#ifdef WS2812B_CHECK_PARAMETERS
    if (dev == RT_NULL)
    {
        return -RT_EINVAL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    rgb888 &= 0xFFFFFFU;
    pixel_bytes = (rt_size_t)WS2812_CHANNEL_COUNT * dev->timing.clocks_per_bit;
    /* One color: encode once, then copy the complete on-wire pixel symbol N-1 times. */
    ws2812_encode_pixel(dev, 0U, rgb888);
    for (i = 0U; i < dev->node_len; ++i)
    {
        dev->pixels[i] = rgb888;
        if (i != 0U)
        {
            rt_memcpy(dev->buff + (rt_size_t)i * pixel_bytes, dev->buff, pixel_bytes);
        }
    }
    return RT_EOK;
}

rt_err_t ws2812_set_brightness(ws2812_t dev, uint8_t brightness)
{
#ifdef WS2812B_CHECK_PARAMETERS
    if (dev == RT_NULL)
    {
        return -RT_EINVAL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    dev->brightness = brightness;
    ws2812_encode_all(dev);
#ifdef WS2812B_USING_EFFECT
    ws2812_effect_mark_dirty(dev);
#endif /* WS2812B_USING_EFFECT */
    return RT_EOK;
}

rt_err_t ws2812_set_gamma(ws2812_t dev, ws2812_gamma_t mode)
{
    rt_bool_t supported = (mode == WS2812_GAMMA_NONE);
#ifdef WS2812B_CHECK_PARAMETERS
    if (dev == RT_NULL || mode > WS2812_GAMMA_CUSTOM)
    {
        return -RT_EINVAL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    switch (mode)
    {
#ifdef WS2812B_GAMMA_22
    case WS2812_GAMMA_22:
        supported = RT_TRUE;
        break;
#endif /* WS2812B_GAMMA_22 */
#ifdef WS2812B_GAMMA_28
    case WS2812_GAMMA_28:
        supported = RT_TRUE;
        break;
#endif /* WS2812B_GAMMA_28 */
#ifdef WS2812B_GAMMA_COMPUTE
    case WS2812_GAMMA_CUSTOM:
        supported = dev->gamma_x100 != 0U;
        break;
#endif /* WS2812B_GAMMA_COMPUTE */
    default:
        break;
    }
    if (!supported)
    {
        return -RT_EINVAL;
    }
    dev->gamma = mode;
    ws2812_encode_all(dev);
#ifdef WS2812B_USING_EFFECT
    ws2812_effect_mark_dirty(dev);
#endif /* WS2812B_USING_EFFECT */
    return RT_EOK;
}

rt_err_t ws2812_set_gamma_mode(ws2812_t dev, ws2812_gamma_t mode)
{
    return ws2812_set_gamma(dev, mode);
}

rt_err_t ws2812_send_frame(ws2812_t dev)
{
    rt_size_t sent;
#ifdef WS2812B_CHECK_PARAMETERS
    if (dev == RT_NULL || dev->spi == RT_NULL || dev->tx_buffer == RT_NULL)
    {
        return -RT_EINVAL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    sent = rt_spi_send(dev->spi, dev->tx_buffer, dev->tx_length);
    if (sent != dev->tx_length)
    {
        LOG_E("SPI short write: %u of %u", (unsigned)sent, (unsigned)dev->tx_length);
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_size_t ws2812_get_frame_size(ws2812_t dev)
{
    return (dev != RT_NULL) ? dev->tx_length : 0U;
}

void ws2812_clear_buff(ws2812_t ws2812)
{
    (void)ws2812_fill(ws2812, WS2812_COLOR_OFF);
}

void ws2812_write_rgb_to_node(ws2812_t ws2812, uint32_t color, uint16_t index)
{
    (void)ws2812_set_pixel(ws2812, index, color);
}

void ws2812_write_rgb_to_all(ws2812_t ws2812, uint32_t color)
{
    (void)ws2812_fill(ws2812, color);
}

void ws2812_send(ws2812_t ws2812)
{
    (void)ws2812_send_frame(ws2812);
}

rt_err_t ws2812_init_all(void)
{
#ifdef WS2812B_USING_KCONFIG_DEVICES
    uint8_t slot;
    for (slot = 0U; slot < WS2812B_KCONFIG_SLOT_COUNT; ++slot)
    {
        ws2812_config_t config;
        const ws2812_kconfig_entry_t *entry = &ws2812_kconfig_devices[slot];
        if (!entry->enabled)
        {
            continue;
        }
        rt_memset(&config, 0, sizeof(config));
        config.spi_bus_name = entry->bus;
        config.spi_device_name = entry->name;
        config.leds_num = entry->leds;
        config.spi_hz = entry->hz;
        config.reset_us = entry->reset_us;
        config.variant = entry->variant;
        config.spi_mode = RT_SPI_MODE_0;
        config.attach = ws2812_manager.storage[slot].attach;
#ifdef WS2812B_TIMING_DIRECT
        config.timing_direct = RT_TRUE;
        config.timing = entry->timing;
#endif /* WS2812B_TIMING_DIRECT */
#ifdef WS2812B_MEMORY_STATIC
        {
            rt_size_t frame_budget = 0U;
            switch (slot)
            {
#if WS2812B_KCONFIG_SLOT_COUNT > 0
            case 0U:
                frame_budget = (rt_size_t)WS2812_SLOT0_FRAME;
                break;
#endif /* WS2812B_KCONFIG_SLOT_COUNT > 0 */
#if WS2812B_KCONFIG_SLOT_COUNT > 1
            case 1U:
                frame_budget = (rt_size_t)WS2812_SLOT1_FRAME;
                break;
#endif /* WS2812B_KCONFIG_SLOT_COUNT > 1 */
#if WS2812B_KCONFIG_SLOT_COUNT > 2
            case 2U:
                frame_budget = (rt_size_t)WS2812_SLOT2_FRAME;
                break;
#endif /* WS2812B_KCONFIG_SLOT_COUNT > 2 */
            default:
                break;
            }
            if (ws2812_create_internal(&config, slot, frame_budget) == RT_NULL)
            {
                return -RT_ERROR;
            }
        }
#else
        if (ws2812_create_internal(&config, slot, 0U) == RT_NULL)
        {
            return -RT_ERROR;
        }
#endif /* WS2812B_MEMORY_STATIC */
    }
#endif /* WS2812B_USING_KCONFIG_DEVICES */
    return RT_EOK;
}
