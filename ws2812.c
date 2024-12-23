/*
 * @Author: maplerian
 * @Date: 2020-07-11 16:02:23
 * @LastEditors: maplerian
 * @LastEditTime: 2020-07-11 22:08:53
 * @Description: file content
 */

#include "ws2812.h"

volatile uint8_t RGB_BIT_Buffer[OneNodeBuffLength];
ws2812_t ws2812_create(char *spi_name, uint16_t led_node_length)
{
    if (!spi_name || !led_node_length)
        return RT_NULL;

    struct rt_spi_device *spi = (struct rt_spi_device *)rt_device_find(spi_name);
    if (spi == RT_NULL)
        return RT_NULL;
    ws2812_t ws2812 = (ws2812_t)rt_malloc(sizeof(struct ws2812));
    ws2812->buff = (uint8_t *)rt_malloc(OneNodeBuffLength * led_node_length);
    if (!ws2812->buff)
    {
        rt_kprintf("There is not enough memory to create a buffer.\r\n");
        rt_free(ws2812);
        return RT_NULL;
    }
    //  配置 spi
    struct rt_spi_configuration ws2812_spi_config;
    ws2812_spi_config.mode = RT_SPI_MASTER | RT_SPI_MODE_3 | RT_SPI_MSB; //  高位在前
    ws2812_spi_config.max_hz = 6 * 1000 * 1000;                          // Ensure that the chip's clock frequency is 42 MHz.
    ws2812_spi_config.data_width = 8;
    rt_spi_configure((struct rt_spi_device *)spi, &ws2812_spi_config);

    //  赋值
    ws2812->spi = spi;
    ws2812->node_len = led_node_length;
    return ws2812;
}

void ws2812_clear_buff(ws2812_t ws2812)
{
    if (!ws2812 || !ws2812->buff || !ws2812->node_len)
        return;
    rt_memset(ws2812->buff, 0x00, OneNodeBuffLength * ws2812->node_len);
}

static void ws2812_creat_data(uint8_t R, uint8_t G, uint8_t B)
{
    uint8_t temp[OneNodeBuffLength] = {0};
    for (uint8_t i = 0; i < 8; i++)
    {
        temp[7 - i] = (G & 0x01) ? WS2812_1 : WS2812_0;
        G = G >> 1;
    }
    for (uint8_t i = 0; i < 8; i++)
    {
        temp[15 - i] = (R & 0x01) ? WS2812_1 : WS2812_0;
        R = R >> 1;
    }
    for (uint8_t i = 0; i < 8; i++)
    {
        temp[23 - i] = (B & 0x01) ? WS2812_1 : WS2812_0;
        B = B >> 1;
    }
    memcpy((void *)RGB_BIT_Buffer, temp, OneNodeBuffLength);
}

void ws2812_write_rgb_to_node(ws2812_t ws2812, uint32_t color, uint16_t index)
{
    uint8_t R, G, B;
    uint16_t i;

    R = (color >> 16) & 0x00FF;
    G = (color >> 8) & 0x0000FF;
    B = (color) & 0x0000FF;

    ws2812_creat_data(R, G, B);
    if (index < ws2812->node_len && index >= 0)
    {
        memcpy((void *)(ws2812->buff + OneNodeBuffLength * index), (void *)RGB_BIT_Buffer, OneNodeBuffLength);
    }
    else
    {
        rt_kprintf("Error: index is out of range.\n");
    }
}

void ws2812_write_rgb_to_all(ws2812_t ws2812, uint32_t color)
{
    uint16_t i;
    for (i = 0; i < ws2812->node_len; i++)
        ws2812_write_rgb_to_node(ws2812, color, i);
}

void ws2812_send(ws2812_t ws2812)
{
    if (!ws2812 || !ws2812->spi || !ws2812->buff)
    {
        rt_kprintf("Parame is null\r\n");
        return;
    }
    rt_spi_send(ws2812->spi, ws2812->buff, OneNodeBuffLength * ws2812->node_len);
}
