/*
 * Copyright (c) 2026, wdfk-prog
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         first version
 */
#ifndef WS2812_GAMMA_COMPUTE_H
#define WS2812_GAMMA_COMPUTE_H

#include "ws2812.h"

/**
 * @brief Regenerate and select a per-device Gamma lookup table.
 * @param dev Target WS2812 device.
 * @param gamma_x100 Exponent multiplied by 100 (50..500 represents 0.50..5.00).
 * @return RT_EOK on success, or -RT_EINVAL for invalid input when checks are enabled.
 * @note Requires WS2812B_GAMMA_COMPUTE; caller must supply valid parameters when checks are disabled.
 */
rt_err_t ws2812_set_gamma_custom(ws2812_t dev, uint16_t gamma_x100);

#endif /* WS2812_GAMMA_COMPUTE_H */
