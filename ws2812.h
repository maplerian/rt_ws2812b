/*
 * @Author: maplerian
 * @Date: 2020-07-11 16:03:03
 * @LastEditors: maplerian
 * @LastEditTime: 2020-07-11 16:53:52
 * @Description: file content
 */

#ifndef __WS2812__H__
#define __WS2812__H__

#include <stdint.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#include "stm32l431xx.h"
#include "libraries\STM32L4xx_HAL_Driver\Inc\stm32l4xx_hal_gpio.h"

#define WS2812_0 0xC0
#define WS2812_1 0xF0

// 以下编码准寻· 光学色相环(RGB模型)-12色
#define red_0 0xFF0000             // 红
#define brown_30 0xFF7F00          // 棕色
#define yellow_60 0xFFFF00         // 黄色
#define Dark_green_90 0x7FFF00     // 深绿色
#define Medium_green_120 0x00FF00  // 中绿色
#define Light_green_150 0x00FF7F   // 浅绿色
#define Baby_blue_180 0x00FFFF     // 浅蓝色
#define Medium_blue_210 0x007FFF   // 中蓝色
#define Dark_blue_240 0x0000FF     // 深蓝色
#define modena_270 0x7F00FF        // 深紫色
#define Medium_purple_300 0xFF00FF // 中紫色
#define lilac_330 0xFF007F         // 浅紫色

#define white_360 0xFFFFFF // 白色
#define off_390 0x000000   // 熄灯

//  一位占用多少个字节
#define OneBitOccupyByte 1
#define OneNodeBuffLength 8 * 3 * OneBitOccupyByte

struct ws2812
{
    struct rt_spi_device *spi;
    uint8_t *buff;
    uint16_t node_len;
};
typedef struct ws2812 *ws2812_t;

ws2812_t ws2812_create(char *spi_name, uint16_t led_node_length);
void ws2812_clear_buff(ws2812_t ws2812);
void ws2812_write_rgb_to_node(ws2812_t ws2812, uint32_t color, uint16_t index);
void ws2812_write_rgb_to_all(ws2812_t ws2812, uint32_t color);
void ws2812_send(ws2812_t ws2812);

#endif //!__WS2812__H__
