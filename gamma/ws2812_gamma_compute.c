/*
 * Copyright (c) 2026, wdfk-prog
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         first version
 */
#include "ws2812_gamma_compute.h"
#include <math.h>

#define DBG_TAG "ws2812_gamma"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/**
 * @brief Recompute one device-local lookup table for a caller-selected exponent.
 * @param dev WS2812B strip whose gamma should change.
 * @param gamma_x100 Exponent times 100; 50..500 corresponds to 0.50..5.00.
 * @return RT_EOK after the new table is selected, or -RT_EINVAL on invalid input.
 * @note LUT[i] = round(255 * powf(i/255, gamma_x100/100)); used only when changed.
 */
rt_err_t ws2812_set_gamma_custom(ws2812_t dev, uint16_t gamma_x100)
{
    uint16_t i;
    float exponent;
#ifdef WS2812B_CHECK_PARAMETERS
    if (dev == RT_NULL || gamma_x100 < 50U || gamma_x100 > 500U)
    {
        return -RT_EINVAL;
    }
#endif /* WS2812B_CHECK_PARAMETERS */
    exponent = (float)gamma_x100 / 100.0f;
    for (i = 0U; i < 256U; ++i)
    {
        float adjusted = 255.0f * powf((float)i / 255.0f, exponent) + 0.5f;
        if (adjusted > 255.0f)
        {
            adjusted = 255.0f;
        }
        dev->gamma_custom_table[i] = (uint8_t)adjusted;
    }
    dev->gamma_x100 = gamma_x100;
    LOG_I("Updated device Gamma to %u/100", gamma_x100);
    return ws2812_set_gamma(dev, WS2812_GAMMA_CUSTOM);
}
