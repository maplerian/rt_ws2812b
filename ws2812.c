/*
 * @Author: maplerian
 * @Date: 2020-07-11 16:02:23
 * @LastEditors: maplerian
 * @LastEditTime: 2020-07-11 22:08:53
 * @Description: file content
 */

#include "ws2812.h"

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
    ws2812_spi_config.mode = RT_SPI_MASTER | RT_SPI_MODE_0 | RT_SPI_MSB; //  高位在前
    ws2812_spi_config.max_hz = 13 * 1000 * 1000 + 333 * 1000;            //  200ns / bit => 8bit 1.6us => 1.25us < 1.6us < 1.85us 可行     6M 3b = 500ns 5b = 833ns
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

//  写颜色节点颜色到缓冲区  //  2byte == 1bit
void ws2812_write_buff(uint8_t *buff, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t rgb = g << 24 | r << 16 | b << 8;
    uint8_t i = 24;
    while (i--)
    {
        //  bit 1: 1111 1111 1110 0000
        if (rgb & 0x80000000)
        {
            *buff++ = 0xff;
            *buff++ = 0xe0;
        }
        //  bit 0: 1111 1000 0000 0000
        else
        {
            *buff++ = 0xf8;
            *buff++ = 0x00;
        }
        //  👆  bit 1: 11bit h, bit 0: 5bit h
        rgb <<= 1;
    }
}

void ws2812_write_rgb_to_node(ws2812_t ws2812, uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    ws2812_write_buff(&ws2812->buff[OneNodeBuffLength * index], r, g, b);
}

void ws2812_write_rgb_to_all(ws2812_t ws2812, uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t i;
    for (i = 0; i < ws2812->node_len; i++)
        ws2812_write_rgb_to_node(ws2812, i, r, g, b);
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
