/*
 * Copyright (c) 2026, wdfk-prog
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2026-10-27     wdfk-prog         first version
 */
#include "ws2812_effect.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <finsh.h>

#define DBG_TAG "ws2812_msh"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/** @brief Name and RGB value of one distinct entry in the public color palette. */
typedef struct
{
    const char *name; /**< Short color token accepted by MSH. */
    uint32_t rgb;     /**< Public RGB888 palette value. */
} ws2812b_msh_color_t;

/** @brief Fifteen unique RGB values from ws2812_color.h, in test order. */
static const ws2812b_msh_color_t ws2812b_colors[] = {
    { "red", WS2812_COLOR_RED },
    { "orange", WS2812_COLOR_ORANGE },
    { "yellow", WS2812_COLOR_YELLOW },
    { "green", WS2812_COLOR_GREEN },
    { "cyan", WS2812_COLOR_CYAN },
    { "blue", WS2812_COLOR_BLUE },
    { "purple", WS2812_COLOR_PURPLE },
    { "magenta", WS2812_COLOR_MAGENTA },
    { "white", WS2812_COLOR_WHITE },
    { "gold", WS2812_COLOR_GOLD },
    { "dark_green", Dark_green_90 },
    { "light_green", Light_green_150 },
    { "medium_blue", Medium_blue_210 },
    { "lilac", lilac_330 },
    { "off", WS2812_COLOR_OFF },
};

/**
 * @brief Parse a nonnegative integer from a shell token without trailing data.
 * @param text Input token, including optional 0x prefix when base is 16.
 * @param base Numeric base requested by the caller (10 or 16).
 * @param result Parsed unsigned 32-bit value on success.
 * @return RT_TRUE if a valid number was parsed.
 */
static rt_bool_t ws2812b_parse_number(const char *text, int base, uint32_t *result)
{
    char *end;
    unsigned long value;
    if (text == RT_NULL || text[0] == '\0' || text[0] == '-' || result == RT_NULL)
    {
        return RT_FALSE;
    }
    errno = 0;
    value = strtoul(text, &end, base);
    if (errno == ERANGE || *end != '\0' || value > UINT32_MAX)
    {
        return RT_FALSE;
    }
    *result = (uint32_t)value;
    return RT_TRUE;
}

/**
 * @brief Parse a named palette color, short RGB alias, or hexadecimal RGB888 value.
 * @param text Shell color token.
 * @param result Parsed 24-bit RGB value.
 * @return RT_TRUE if the token specifies a valid color.
 */
static rt_bool_t ws2812b_parse_color(const char *text, uint32_t *result)
{
    rt_size_t i;
    if (text == RT_NULL || result == RT_NULL)
    {
        return RT_FALSE;
    }
    if (rt_strcmp(text, "r") == 0 || rt_strcmp(text, "g") == 0 || rt_strcmp(text, "b") == 0)
    {
        *result = (rt_strcmp(text, "r") == 0) ? WS2812_COLOR_RED :
                  (rt_strcmp(text, "g") == 0) ? WS2812_COLOR_GREEN : WS2812_COLOR_BLUE;
        return RT_TRUE;
    }
    for (i = 0U; i < sizeof(ws2812b_colors) / sizeof(ws2812b_colors[0]); ++i)
    {
        if (rt_strcmp(text, ws2812b_colors[i].name) == 0)
        {
            *result = ws2812b_colors[i].rgb;
            return RT_TRUE;
        }
    }
    return ws2812b_parse_number(text, 16, result) && *result <= 0xFFFFFFU;
}

/**
 * @brief Parse a sequence step formatted as color or color:ticks.
 * @param text One shell token, optionally ending with a decimal tick duration.
 * @param step Parsed step with a default duration of 500 ticks.
 * @return RT_TRUE when both color and duration are valid.
 */
static rt_bool_t ws2812b_parse_step(const char *text, ws2812_effect_step_t *step)
{
    const char *separator;
    if (text == RT_NULL || step == RT_NULL)
    {
        return RT_FALSE;
    }
    separator = strchr(text, ':');
    uint32_t ticks = 500U;
    if (separator != RT_NULL)
    {
        char color[24];
        rt_size_t length = (rt_size_t)(separator - text);
        if (length == 0U || length >= sizeof(color))
        {
            return RT_FALSE;
        }
        rt_memcpy(color, text, length);
        color[length] = '\0';
        if (!ws2812b_parse_color(color, &step->color) ||
            !ws2812b_parse_number(separator + 1, 10, &ticks))
        {
            return RT_FALSE;
        }
    }
    else if (!ws2812b_parse_color(text, &step->color))
    {
        return RT_FALSE;
    }
    step->duration_ticks = (rt_tick_t)ticks;
    return ticks > 0U && (uint32_t)step->duration_ticks == ticks;
}

/** @brief Print the compact WS2812B MSH command forms and their default values. */
static void ws2812b_print_usage(void)
{
    rt_kprintf("Usage: ws2812b [slot=0] <command> [args]\n");
    rt_kprintf("  help | list | info | off | reset\n");
    rt_kprintf("  bright [0..255]            gamma [off|22|28|50..500]\n");
    rt_kprintf("  solid|s [color]            blink|bl [color] [bg] [ticks]\n");
    rt_kprintf("  breathe|br [color] [ticks] gradient|gr [color] [bg] [ticks]\n");
    rt_kprintf("  gradation|gd [ticks]       chase|ch [color] [bg] [ticks]\n");
    rt_kprintf("  seq [color[:ticks] ...]    colors [ticks]\n");
    rt_kprintf("  Colors: red green blue off ... or hex RRGGBB (r/g/b aliases).\n");
    rt_kprintf("  Defaults: red/off, 600 ticks blink, 1000 breathe/gradient,\n");
    rt_kprintf("            20 gradation, 100 chase, 500 per seq step.\n");
    rt_kprintf("  Gamma 22/28/custom require their respective build options.\n");
}

/**
 * @brief Print physical strip, brightness and gamma state for one configured slot.
 * @param slot Zero-based Kconfig slot index.
 * @param dev Initialized device, or RT_NULL for a disabled/unavailable slot.
 */
static void ws2812b_print_info(uint32_t slot, ws2812_t dev)
{
    const char *gamma;
    if (dev == RT_NULL)
    {
        rt_kprintf("WS2812B slot %u: unavailable\n", (unsigned)slot);
        return;
    }
    gamma = (dev->gamma == WS2812_GAMMA_NONE) ? "off" :
            (dev->gamma == WS2812_GAMMA_22) ? "2.2" :
            (dev->gamma == WS2812_GAMMA_28) ? "2.8" : "custom";
    rt_kprintf("WS2812B slot %u: %u LEDs, SPI cfg %u Hz, bright %u/255, gamma %s",
               (unsigned)slot, (unsigned)dev->node_len, (unsigned)dev->spi_hz,
               (unsigned)dev->brightness, gamma);
#ifdef WS2812B_GAMMA_COMPUTE
    if (dev->gamma == WS2812_GAMMA_CUSTOM)
    {
        rt_kprintf(" (%u/100)", (unsigned)dev->gamma_x100);
    }
#endif /* WS2812B_GAMMA_COMPUTE */
    rt_kprintf("\n");
}

/**
 * @brief Start the chosen effect, printing the RT-Thread result for MSH feedback.
 * @param dev Target strip.
 * @param slot Configured slot index for logging.
 * @param command User-supplied effect name.
 * @param config Parsed effect parameters.
 * @return 0 when accepted, -1 when unsupported or invalid.
 */
static int ws2812b_start(ws2812_t dev, uint32_t slot, const char *command, const ws2812_effect_config_t *config)
{
    rt_err_t result = ws2812_effect_start(dev, config);
    if (result != RT_EOK)
    {
        rt_kprintf("WS2812B slot %u: %s failed (%d); check compiled effect and parameters.\n",
                   (unsigned)slot, command, (int)result);
        return -1;
    }
    rt_kprintf("WS2812B slot %u: %s accepted (rendered on next application poll).\n",
               (unsigned)slot, command);
    return 0;
}

/**
 * @brief Run short-form effect, palette, gamma, brightness and diagnostic commands.
 * @param argc Number of MSH tokens.
 * @param argv Command, optional slot, subcommand and its arguments.
 * @return 0 on success, -1 for unavailable slots, unsupported options or invalid input.
 * @note Like the former MSH entry, effect changes must be serialized with application poll().
 */
static int ws2812b(int argc, char **argv)
{
    ws2812_effect_config_t config = { 0 };
    ws2812_effect_step_t steps[WS2812B_EFFECT_MAX_STEPS];
    const char *command;
    uint32_t slot = 0U;
    uint32_t value;
    uint32_t ticks;
    rt_size_t i;
    int arg = 1;
    int count;
    ws2812_t dev;
    rt_err_t result;

    if (argc == 1 || (argc == 2 && (rt_strcmp(argv[1], "help") == 0 || rt_strcmp(argv[1], "?") == 0)))
    {
        ws2812b_print_usage();
        return 0;
    }
    if (argc == 2 && rt_strcmp(argv[1], "list") == 0)
    {
        for (i = 0U; i < WS2812B_KCONFIG_SLOT_COUNT; ++i)
        {
            ws2812b_print_info((uint32_t)i, ws2812_get_kconfig_device((uint8_t)i));
        }
        return 0;
    }
    if (ws2812b_parse_number(argv[arg], 10, &value))
    {
        slot = value;
        ++arg;
    }
    if (slot >= WS2812B_KCONFIG_SLOT_COUNT || arg >= argc)
    {
        ws2812b_print_usage();
        return -1;
    }
    command = argv[arg++];
    count = argc - arg;
    dev = ws2812_get_kconfig_device((uint8_t)slot);
    if (dev == RT_NULL)
    {
        rt_kprintf("WS2812B slot %u is not initialized.\n", (unsigned)slot);
        return -1;
    }
    if ((rt_strcmp(command, "help") == 0 || rt_strcmp(command, "?") == 0) && count == 0)
    {
        ws2812b_print_usage();
        return 0;
    }
    if (rt_strcmp(command, "info") == 0 && count == 0)
    {
        ws2812b_print_info(slot, dev);
        return 0;
    }
    if (rt_strcmp(command, "bright") == 0)
    {
        if (count == 0)
        {
            rt_kprintf("WS2812B slot %u brightness: %u/255\n", (unsigned)slot, (unsigned)dev->brightness);
            return 0;
        }
        if (count != 1 || !ws2812b_parse_number(argv[arg], 10, &value) || value > 255U)
        {
            rt_kprintf("Usage: ws2812b [slot] bright [0..255]\n");
            return -1;
        }
        result = ws2812_set_brightness(dev, (uint8_t)value);
        if (result != RT_EOK)
        {
            rt_kprintf("WS2812B slot %u: brightness failed (%d).\n", (unsigned)slot, (int)result);
            return -1;
        }
        rt_kprintf("WS2812B slot %u brightness: %u/255 (next poll redraws).\n", (unsigned)slot, (unsigned)value);
        return 0;
    }
    if (rt_strcmp(command, "gamma") == 0)
    {
        if (count == 0)
        {
            ws2812b_print_info(slot, dev);
            return 0;
        }
        if (count != 1)
        {
            ws2812b_print_usage();
            return -1;
        }
        if (rt_strcmp(argv[arg], "off") == 0 || rt_strcmp(argv[arg], "none") == 0 || rt_strcmp(argv[arg], "0") == 0)
        {
            result = ws2812_set_gamma(dev, WS2812_GAMMA_NONE);
        }
        else if (rt_strcmp(argv[arg], "22") == 0)
        {
            result = ws2812_set_gamma(dev, WS2812_GAMMA_22);
        }
        else if (rt_strcmp(argv[arg], "28") == 0)
        {
            result = ws2812_set_gamma(dev, WS2812_GAMMA_28);
        }
#ifdef WS2812B_GAMMA_COMPUTE
        else if (ws2812b_parse_number(argv[arg], 10, &value) && value >= 50U && value <= 500U)
        {
            result = ws2812_set_gamma_custom(dev, (uint16_t)value);
        }
#endif /* WS2812B_GAMMA_COMPUTE */
        else
        {
            ws2812b_print_usage();
            return -1;
        }
        if (result != RT_EOK)
        {
            rt_kprintf("WS2812B slot %u: gamma mode unavailable (%d); check build options.\n", (unsigned)slot, (int)result);
            return -1;
        }
        ws2812b_print_info(slot, dev);
        return 0;
    }
    if (rt_strcmp(command, "reset") == 0 && count == 0)
    {
        result = ws2812_effect_start_kconfig((uint8_t)slot);
        if (result != RT_EOK)
        {
            rt_kprintf("WS2812B slot %u: Kconfig reset failed (%d).\n", (unsigned)slot, (int)result);
            return -1;
        }
        rt_kprintf("WS2812B slot %u: Kconfig default selected (or no default configured).\n", (unsigned)slot);
        return 0;
    }
    if (rt_strcmp(command, "off") == 0 && count == 0)
    {
#ifdef WS2812B_EFFECT_SOLID
        config.type = WS2812_EFFECT_SOLID;
#else
        config.type = UINT16_MAX; /* Unavailable effect must never select a custom slot. */
#endif /* WS2812B_EFFECT_SOLID */
        config.color = WS2812_COLOR_OFF;
        return ws2812b_start(dev, slot, command, &config);
    }
    if ((rt_strcmp(command, "solid") == 0 || rt_strcmp(command, "s") == 0) && count <= 1)
    {
#ifdef WS2812B_EFFECT_SOLID
        config.type = WS2812_EFFECT_SOLID;
#else
        config.type = UINT16_MAX; /* Unavailable effect must never select a custom slot. */
#endif /* WS2812B_EFFECT_SOLID */
        config.color = WS2812_COLOR_RED;
        if (count == 1 && !ws2812b_parse_color(argv[arg], &config.color))
        {
            goto invalid;
        }
    }
    else if ((rt_strcmp(command, "gradation") == 0 || rt_strcmp(command, "gd") == 0) && count <= 1)
    {
#ifdef WS2812B_EFFECT_GRADATION
        config.type = WS2812_EFFECT_GRADATION;
#else
        config.type = UINT16_MAX; /* Unavailable effect must never select a custom slot. */
#endif /* WS2812B_EFFECT_GRADATION */
        config.period_ticks = 20U;
        if (count == 1 && (!ws2812b_parse_number(argv[arg], 10, &ticks) || ticks == 0U))
        {
            goto invalid;
        }
        if (count == 1)
        {
            config.period_ticks = (rt_tick_t)ticks;
        }
    }
    else if ((rt_strcmp(command, "breathe") == 0 || rt_strcmp(command, "br") == 0) && count <= 2)
    {
#ifdef WS2812B_EFFECT_BREATHE
        config.type = WS2812_EFFECT_BREATHE;
#else
        config.type = UINT16_MAX; /* Unavailable effect must never select a custom slot. */
#endif /* WS2812B_EFFECT_BREATHE */
        config.color = WS2812_COLOR_GREEN;
        config.period_ticks = 1000U;
        if ((count >= 1 && !ws2812b_parse_color(argv[arg], &config.color)) ||
            (count == 2 && (!ws2812b_parse_number(argv[arg + 1], 10, &ticks) || ticks < 2U)))
        {
            goto invalid;
        }
        if (count == 2)
        {
            config.period_ticks = (rt_tick_t)ticks;
        }
    }
    else if ((rt_strcmp(command, "blink") == 0 || rt_strcmp(command, "bl") == 0 ||
              rt_strcmp(command, "gradient") == 0 || rt_strcmp(command, "gr") == 0 ||
              rt_strcmp(command, "chase") == 0 || rt_strcmp(command, "ch") == 0) && count <= 3)
    {
        rt_bool_t blink = rt_strcmp(command, "blink") == 0 || rt_strcmp(command, "bl") == 0;
        rt_bool_t chase = rt_strcmp(command, "chase") == 0 || rt_strcmp(command, "ch") == 0;
        if (blink)
        {
#ifdef WS2812B_EFFECT_BLINK
            config.type = WS2812_EFFECT_BLINK;
#else
            config.type = UINT16_MAX;
#endif /* WS2812B_EFFECT_BLINK */
        }
        else if (chase)
        {
#ifdef WS2812B_EFFECT_CHASE
            config.type = WS2812_EFFECT_CHASE;
#else
            config.type = UINT16_MAX;
#endif /* WS2812B_EFFECT_CHASE */
        }
        else
        {
#ifdef WS2812B_EFFECT_GRADIENT
            config.type = WS2812_EFFECT_GRADIENT;
#else
            config.type = UINT16_MAX;
#endif /* WS2812B_EFFECT_GRADIENT */
        }
        config.color = chase ? WS2812_COLOR_GREEN : WS2812_COLOR_RED;
        config.background_color = (blink || chase) ? WS2812_COLOR_OFF : WS2812_COLOR_BLUE;
        config.period_ticks = chase ? 100U : blink ? 600U : 1000U;
        if ((count >= 1 && !ws2812b_parse_color(argv[arg], &config.color)) ||
            (count >= 2 && !ws2812b_parse_color(argv[arg + 1], &config.background_color)) ||
            (count == 3 && (!ws2812b_parse_number(argv[arg + 2], 10, &ticks) || ticks == 0U)))
        {
            goto invalid;
        }
        if (count == 3)
        {
            config.period_ticks = (rt_tick_t)ticks;
        }
        if (blink && (config.period_ticks < 2U || (config.period_ticks & 1U) != 0U))
        {
            rt_kprintf("BLINK period must be even and >= 2 ticks.\n");
            return -1;
        }
    }
    else if ((rt_strcmp(command, "seq") == 0) && count <= WS2812B_EFFECT_MAX_STEPS)
    {
#ifdef WS2812B_EFFECT_SEQUENCE
        config.type = WS2812_EFFECT_SEQUENCE;
#else
        config.type = UINT16_MAX; /* Unavailable effect must never select a custom slot. */
#endif /* WS2812B_EFFECT_SEQUENCE */
        if (count == 0)
        {
            if (WS2812B_EFFECT_MAX_STEPS < 3U)
            {
                goto invalid;
            }
            steps[0].color = WS2812_COLOR_RED;
            steps[1].color = WS2812_COLOR_GREEN;
            steps[2].color = WS2812_COLOR_BLUE;
            for (i = 0U; i < 3U; ++i)
            {
                steps[i].duration_ticks = 500U;
            }
            config.step_count = 3U;
        }
        else
        {
            config.step_count = (uint16_t)count;
            for (i = 0U; i < (rt_size_t)count; ++i)
            {
                if (!ws2812b_parse_step(argv[arg + (int)i], &steps[i]))
                {
                    goto invalid;
                }
            }
        }
        config.steps = steps; /* Effect start copies all temporary steps. */
    }
    else if (rt_strcmp(command, "colors") == 0 && count <= 1)
    {
        rt_size_t size = sizeof(ws2812b_colors) / sizeof(ws2812b_colors[0]);
        ticks = 1000U;
        if (size > WS2812B_EFFECT_MAX_STEPS)
        {
            rt_kprintf("Palette needs %u steps (configured maximum: %u).\n",
                       (unsigned)size, (unsigned)WS2812B_EFFECT_MAX_STEPS);
            return -1;
        }
        if (count == 1 && (!ws2812b_parse_number(argv[arg], 10, &ticks) || ticks == 0U))
        {
            goto invalid;
        }
        for (i = 0U; i < size; ++i)
        {
            steps[i].color = ws2812b_colors[i].rgb;
            steps[i].duration_ticks = (rt_tick_t)ticks;
        }
#ifdef WS2812B_EFFECT_SEQUENCE
        config.type = WS2812_EFFECT_SEQUENCE;
#else
        config.type = UINT16_MAX; /* Unavailable effect must never select a custom slot. */
#endif /* WS2812B_EFFECT_SEQUENCE */
        config.step_count = (uint16_t)size;
        config.steps = steps;
        result = ws2812_effect_start(dev, &config);
        if (result != RT_EOK)
        {
            rt_kprintf("WS2812B slot %u: color cycle failed (%d).\n", (unsigned)slot, (int)result);
            return -1;
        }
        rt_kprintf("WS2812B slot %u: %u colors, %u ticks each, in order:\n",
                   (unsigned)slot, (unsigned)size, (unsigned)ticks);
        for (i = 0U; i < size; ++i)
        {
            rt_kprintf("%s=%06X%s", ws2812b_colors[i].name, (unsigned)ws2812b_colors[i].rgb,
                       ((i + 1U) % 4U == 0U || i + 1U == size) ? "\n" : "  ");
        }
        return 0;
    }
    else
    {
        goto invalid;
    }
    return ws2812b_start(dev, slot, command, &config);

invalid:
    rt_kprintf("Invalid ws2812b %s parameters.\n", command);
    ws2812b_print_usage();
    return -1;
}
MSH_CMD_EXPORT(ws2812b, control WS2812B effects brightness gamma and diagnostics);
