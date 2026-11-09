/*
 * WS2812B SPI waveform selection for classic and V5 timing profiles.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         first version
 */
#include "ws2812.h"

/** @brief Datasheet timing limits in ns; all four pulse widths must be valid. */
typedef struct
{
    uint16_t t0h_min;   /**< Zero high lower bound (ns). */
    uint16_t t0h_max;   /**< Zero high upper bound (ns). */
    uint16_t t0l_min;   /**< Zero low lower bound (ns). */
    uint16_t t0l_max;   /**< Zero low upper bound (ns). */
    uint16_t t1h_min;   /**< One high lower bound (ns). */
    uint16_t t1h_max;   /**< One high upper bound (ns). */
    uint16_t t1l_min;   /**< One low lower bound (ns). */
    uint16_t t1l_max;   /**< One low upper bound (ns). */
    uint16_t total_min; /**< Common bit period lower bound (ns). */
    uint16_t total_max; /**< Common bit period upper bound (ns). */
    uint16_t target0;   /**< Preferred T0H for ranking valid solutions (ns). */
    uint16_t target1;   /**< Preferred T1H for ranking valid solutions (ns). */
} ws2812_timing_limits_t;

/** @brief Classic Worldsemi WS2812B nominal values with +/-150 ns tolerance. */
#if defined(WS2812B_TIMING_SEARCH) || defined(WS2812B_CHECK_EXTENDED)
static const ws2812_timing_limits_t legacy_limits = {
    250, 550, 700, 1000, 650, 950, 300, 600, 950, 1550, 400, 800
};

/** @brief Worldsemi WS2812B V5 timing constraints and 1.25 us target. */
static const ws2812_timing_limits_t v5_limits = {
    220, 380, 580, 1000, 580, 1000, 580, 1000, 1160, 1380, 300, 650
};

#endif /* WS2812B_TIMING_SEARCH || WS2812B_CHECK_EXTENDED */

#if defined(WS2812B_TIMING_TABLE) || defined(WS2812B_TIMING_SEARCH)
/** @brief One precomputed timing option, selected by actual SCK and LED variant. */
typedef struct
{
    uint32_t hz;            /**< Actual SPI clock frequency in hertz. */
    uint8_t variant;        /**< WS2812_VARIANT_LEGACY or WS2812_VARIANT_V5. */
    ws2812_timing_t timing; /**< WS bit length and high-clock counts. */
} ws2812_timing_entry_t;

/** @brief Preset waveforms selected by frequency and LED revision. */
static const ws2812_timing_entry_t common_timings[] = {
    /* SPI_Hz        WS2812_variant             SPI_clocks_per_WS_bit, T0H, T1H
     * Units: Hz; revision; SPI clock counts for a WS bit and the leading high clocks for logical 0/1.
     */
    {   2250000U, WS2812_VARIANT_LEGACY,         {  3U,                 1U,  2U } },
    {   2400000U, WS2812_VARIANT_LEGACY,         {  3U,                 1U,  2U } },
    {   2500000U, WS2812_VARIANT_LEGACY,         {  3U,                 1U,  2U } },
    {   3000000U, WS2812_VARIANT_V5,             {  4U,                 1U,  2U } },
    {   3200000U, WS2812_VARIANT_LEGACY,         {  4U,                 1U,  3U } },
    {   3200000U, WS2812_VARIANT_V5,             {  4U,                 1U,  2U } },
    {   3333333U, WS2812_VARIANT_LEGACY,         {  4U,                 1U,  3U } },
    {   3333333U, WS2812_VARIANT_V5,             {  4U,                 1U,  2U } },
    {   4000000U, WS2812_VARIANT_LEGACY,         {  5U,                 2U,  3U } },
    {   4500000U, WS2812_VARIANT_LEGACY,         {  6U,                 2U,  4U } },
    {   5000000U, WS2812_VARIANT_LEGACY,         {  6U,                 2U,  4U } },
    {   6000000U, WS2812_VARIANT_LEGACY,         {  7U,                 2U,  5U } },
    {   6000000U, WS2812_VARIANT_V5,             {  8U,                 2U,  4U } },
    {   6400000U, WS2812_VARIANT_LEGACY,         {  8U,                 3U,  5U } },
    {   6400000U, WS2812_VARIANT_V5,             {  8U,                 2U,  4U } },
    {   6666667U, WS2812_VARIANT_LEGACY,         {  8U,                 3U,  5U } },
    {   6666667U, WS2812_VARIANT_V5,             {  8U,                 2U,  4U } },
    {   7200000U, WS2812_VARIANT_LEGACY,         {  9U,                 3U,  6U } },
    {   8000000U, WS2812_VARIANT_LEGACY,         { 10U,                 3U,  6U } },
    {   8000000U, WS2812_VARIANT_V5,             { 10U,                 2U,  5U } },
    {   9000000U, WS2812_VARIANT_LEGACY,         { 11U,                 4U,  7U } },
    {   9000000U, WS2812_VARIANT_V5,             { 12U,                 3U,  6U } },
    {  10000000U, WS2812_VARIANT_LEGACY,         { 12U,                 4U,  8U } },
    {  10000000U, WS2812_VARIANT_V5,             { 12U,                 3U,  6U } },
    {  12000000U, WS2812_VARIANT_LEGACY,         { 15U,                 5U, 10U } },
    {  12000000U, WS2812_VARIANT_V5,             { 15U,                 4U,  8U } },
    {  13333333U, WS2812_VARIANT_LEGACY,         { 17U,                 5U, 11U } },
    {  13333333U, WS2812_VARIANT_V5,             { 17U,                 4U,  9U } },
    {  14400000U, WS2812_VARIANT_LEGACY,         { 18U,                 6U, 12U } },
    {  14400000U, WS2812_VARIANT_V5,             { 18U,                 4U,  9U } },
    {  16000000U, WS2812_VARIANT_LEGACY,         { 20U,                 6U, 13U } },
    {  16000000U, WS2812_VARIANT_V5,             { 20U,                 5U, 10U } },
    {  18000000U, WS2812_VARIANT_LEGACY,         { 22U,                 7U, 14U } },
    {  18000000U, WS2812_VARIANT_V5,             { 23U,                 5U, 12U } },
    {  20000000U, WS2812_VARIANT_LEGACY,         { 25U,                 8U, 16U } },
    {  20000000U, WS2812_VARIANT_V5,             { 25U,                 6U, 13U } },
    {  24000000U, WS2812_VARIANT_LEGACY,         { 30U,                10U, 19U } },
    {  24000000U, WS2812_VARIANT_V5,             { 30U,                 7U, 16U } },
    {  25000000U, WS2812_VARIANT_LEGACY,         { 31U,                10U, 20U } },
    {  25000000U, WS2812_VARIANT_V5,             { 31U,                 7U, 16U } },
    {  26666667U, WS2812_VARIANT_LEGACY,         { 33U,                11U, 21U } },
    {  26666667U, WS2812_VARIANT_V5,             { 33U,                 8U, 17U } },
    {  27000000U, WS2812_VARIANT_LEGACY,         { 34U,                11U, 22U } },
    {  27000000U, WS2812_VARIANT_V5,             { 34U,                 8U, 18U } },
    {  30000000U, WS2812_VARIANT_LEGACY,         { 37U,                12U, 24U } },
    {  30000000U, WS2812_VARIANT_V5,             { 37U,                 9U, 19U } },
    {  32000000U, WS2812_VARIANT_LEGACY,         { 40U,                13U, 26U } },
    {  32000000U, WS2812_VARIANT_V5,             { 40U,                10U, 21U } },
    {  36000000U, WS2812_VARIANT_LEGACY,         { 45U,                14U, 29U } },
    {  36000000U, WS2812_VARIANT_V5,             { 45U,                11U, 23U } },
    {  40000000U, WS2812_VARIANT_LEGACY,         { 50U,                16U, 32U } },
    {  40000000U, WS2812_VARIANT_V5,             { 50U,                12U, 26U } },
    {  48000000U, WS2812_VARIANT_LEGACY,         { 60U,                19U, 38U } },
    {  48000000U, WS2812_VARIANT_V5,             { 60U,                14U, 31U } },
};

#endif /* WS2812B_TIMING_TABLE || WS2812B_TIMING_SEARCH */

#if defined(WS2812B_TIMING_SEARCH) || defined(WS2812B_CHECK_EXTENDED)
/**
 * @brief Check an exact pulse count against the inclusive data-sheet bounds.
 * @param clocks Number of SPI clock ticks occupied by this pulse.
 * @param hz Actual SPI SCK frequency.
 * @param minimum Minimum pulse duration in ns.
 * @param maximum Maximum pulse duration in ns.
 * @return RT_TRUE if this pulse is within bounds.
 */
static rt_bool_t ws2812_pulse_valid(uint32_t clocks, uint32_t hz, uint32_t minimum, uint32_t maximum)
{
    const uint64_t scaled = (uint64_t)clocks * 1000000000ULL;
    return (scaled >= (uint64_t)minimum * hz) && (scaled <= (uint64_t)maximum * hz);
}

/**
 * @brief Check all four data pulse widths and the total bit period.
 * @param hz Actual SCK frequency.
 * @param limits Physical LED timing ranges.
 * @param timing Candidate SPI counts.
 * @return RT_TRUE only for a valid profile.
 */
static rt_bool_t ws2812_timing_valid(uint32_t hz, const ws2812_timing_limits_t *limits,
                                     const ws2812_timing_t *timing)
{
    uint32_t n = timing->clocks_per_bit;
    if (n < WS2812_MIN_CLOCKS_PER_BIT || n > WS2812_MAX_CLOCKS_PER_BIT || timing->high0 == 0U || timing->high1 == 0U ||
        timing->high0 >= n || timing->high1 >= n)
    {
        return RT_FALSE;
    }
    return ws2812_pulse_valid(n, hz, limits->total_min, limits->total_max) &&
           ws2812_pulse_valid(timing->high0, hz, limits->t0h_min, limits->t0h_max) &&
           ws2812_pulse_valid(n - timing->high0, hz, limits->t0l_min, limits->t0l_max) &&
           ws2812_pulse_valid(timing->high1, hz, limits->t1h_min, limits->t1h_max) &&
           ws2812_pulse_valid(n - timing->high1, hz, limits->t1l_min, limits->t1l_max);
}
#endif /* WS2812B_TIMING_SEARCH || WS2812B_CHECK_EXTENDED */

#ifdef WS2812B_TIMING_SEARCH
/**
 * @brief Round a clock count to whole nanoseconds for scoring candidate timings.
 * @param clocks SPI clock count.
 * @param hz Actual frequency.
 * @return Pulse duration rounded to nearest nanosecond.
 */
static uint32_t ws2812_ns(uint32_t clocks, uint32_t hz)
{
    return (uint32_t)(((uint64_t)clocks * 1000000000ULL + hz / 2U) / hz);
}

/** @brief Absolute difference between two nonnegative time values. */
static uint32_t ws2812_abs_delta(uint32_t a, uint32_t b)
{
    return (a >= b) ? (a - b) : (b - a);
}

/**
 * @brief Find a data-sheet-compliant waveform for a frequency missing from the table.
 * @param hz Actual SPI frequency.
 * @param limits Physical WS2812 timing constraints.
 * @param timing Best candidate returned on success.
 * @return RT_EOK or -RT_EINVAL if no candidate fits.
 */
static rt_err_t ws2812_search_timing(uint32_t hz, const ws2812_timing_limits_t *limits,
                                     ws2812_timing_t *timing)
{
    uint32_t best_score = 0xFFFFFFFFU;
    rt_bool_t found = RT_FALSE;
    uint16_t period;
    for (period = WS2812_MIN_CLOCKS_PER_BIT; period <= WS2812_MAX_CLOCKS_PER_BIT; ++period)
    {
        uint16_t high;
        uint8_t high0 = 0U;
        uint8_t high1 = 0U;
        uint32_t best0 = 0xFFFFFFFFU;
        uint32_t best1 = 0xFFFFFFFFU;
        uint32_t score;
#ifdef WS2812B_ENCODER_FAST_ONLY
        if ((period & 1U) != 0U || period > WS2812B_FAST_MAX_CLOCKS_PER_BIT)
        {
            continue;
        }
#endif /* WS2812B_ENCODER_FAST_ONLY */
        if (!ws2812_pulse_valid(period, hz, limits->total_min, limits->total_max))
        {
            continue;
        }
        for (high = 1U; high < period; ++high)
        {
            uint32_t delta;
            if (ws2812_pulse_valid(high, hz, limits->t0h_min, limits->t0h_max) &&
                ws2812_pulse_valid(period - high, hz, limits->t0l_min, limits->t0l_max))
            {
                delta = ws2812_abs_delta(ws2812_ns(high, hz), limits->target0);
                if (delta < best0)
                {
                    best0 = delta;
                    high0 = (uint8_t)high;
                }
            }
            if (ws2812_pulse_valid(high, hz, limits->t1h_min, limits->t1h_max) &&
                ws2812_pulse_valid(period - high, hz, limits->t1l_min, limits->t1l_max))
            {
                delta = ws2812_abs_delta(ws2812_ns(high, hz), limits->target1);
                if (delta < best1)
                {
                    best1 = delta;
                    high1 = (uint8_t)high;
                }
            }
        }
        if (high0 == 0U || high1 == 0U)
        {
            continue;
        }
        score = 2U * ws2812_abs_delta(ws2812_ns(period, hz), 1250U) + best0 + best1;
        if (!found || score < best_score)
        {
            timing->clocks_per_bit = (uint8_t)period;
            timing->high0 = high0;
            timing->high1 = high1;
            found = RT_TRUE;
            best_score = score;
        }
    }
    return (found && ws2812_timing_valid(hz, limits, timing)) ? RT_EOK : -RT_EINVAL;
}
#endif /* WS2812B_TIMING_SEARCH */

#if defined(WS2812B_TIMING_TABLE) || defined(WS2812B_TIMING_SEARCH)
rt_err_t ws2812_timing_select(uint32_t spi_hz, ws2812_variant_t variant, ws2812_timing_t *timing)
{
    rt_size_t i;
#if defined(WS2812B_TIMING_SEARCH) || defined(WS2812B_CHECK_EXTENDED)
    const ws2812_timing_limits_t *limits;
#endif /* WS2812B_TIMING_SEARCH || WS2812B_CHECK_EXTENDED */
    if (timing == RT_NULL || spi_hz == 0U ||
        (variant != WS2812_VARIANT_LEGACY && variant != WS2812_VARIANT_V5))
    {
        return -RT_EINVAL;
    }
#if defined(WS2812B_TIMING_SEARCH) || defined(WS2812B_CHECK_EXTENDED)
    limits = (variant == WS2812_VARIANT_LEGACY) ? &legacy_limits : &v5_limits;
#endif /* WS2812B_TIMING_SEARCH || WS2812B_CHECK_EXTENDED */
    for (i = 0U; i < sizeof(common_timings) / sizeof(common_timings[0]); ++i)
    {
        if (common_timings[i].hz != spi_hz || common_timings[i].variant != (uint8_t)variant)
        {
            continue;
        }
#ifdef WS2812B_CHECK_EXTENDED
        if (!ws2812_timing_valid(spi_hz, limits, &common_timings[i].timing))
        {
            return -RT_EINVAL;
        }
#endif /* WS2812B_CHECK_EXTENDED */
        *timing = common_timings[i].timing;
        return RT_EOK;
    }
#ifdef WS2812B_TIMING_SEARCH
    return ws2812_search_timing(spi_hz, limits, timing);
#else
    return -RT_EINVAL;
#endif /* WS2812B_TIMING_SEARCH */
}

#endif /* WS2812B_TIMING_TABLE || WS2812B_TIMING_SEARCH */

#ifdef WS2812B_CHECK_EXTENDED
/**
 * @brief Validate explicit SPI symbol clocks against the selected physical LED revision.
 * @param spi_hz Actual SPI clock frequency in Hz.
 * @param variant WS2812B timing revision.
 * @param timing Explicit user-provided symbol clock counts.
 * @return RT_EOK for a compliant encoding, -RT_EINVAL otherwise.
 */
rt_err_t ws2812_timing_validate(uint32_t spi_hz, ws2812_variant_t variant, const ws2812_timing_t *timing)
{
    const ws2812_timing_limits_t *limits;
    if (spi_hz == 0U || timing == RT_NULL ||
        (variant != WS2812_VARIANT_LEGACY && variant != WS2812_VARIANT_V5))
    {
        return -RT_EINVAL;
    }
    limits = (variant == WS2812_VARIANT_LEGACY) ? &legacy_limits : &v5_limits;
    return ws2812_timing_valid(spi_hz, limits, timing) ? RT_EOK : -RT_EINVAL;
}
#endif /* WS2812B_CHECK_EXTENDED */
