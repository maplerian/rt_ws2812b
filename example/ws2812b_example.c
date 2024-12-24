/**!
 *
 * @file           : /rt_ws2812b/example/ws2812b_example.c
 *
 * @date           : 2020-07-16 21:24:54
 *
 * @author         : maplerian
 *
 * @brief          : file content
 *
 */
#include <ws2812.h>

#define WS2812B_EXAMPLE_SPI_DEVICE_NAME "spi1"
#define WS2812B_EXAMPLE_SPI_CS_PORT_NAME GPIOB
#define WS2812B_EXAMPLE_SPI_CS_PIN_NAME GPIO_PIN_12

#ifndef WS2812B_EXAMPLE_SPI_NAME
#define WS2812B_EXAMPLE_SPI_NAME "spi10"
#endif //! WS2812B_EXAMPLE_SPI_NAME

#ifndef WS2812B_EXAMPLE_NODE_LENGTH
#define WS2812B_EXAMPLE_NODE_LENGTH 1
#endif // !WS2812B_EXAMPLE_NODE_LENGTH

#ifdef BSP_SPI1_TX_USING_DMA || BSP_SPI2_TX_USING_DMA || BSP_SPI3_TX_USING_DMA || BSP_SPI4_TX_USING_DMA || BSP_SPI5_TX_USING_DMA || BSP_SPI6_TX_USING_DMA
#define ENABLE 1
#else
#define ENABLE 0
#endif // BSP_SPI1_TX_USING_DMA || BSP_SPI2_TX_USING_DMA || BSP_SPI3_TX_USING_DMA || BSP_SPI4_TX_USING_DMA || BSP_SPI5_TX_USING_DMA || BSP_SPI6_TX_USING_DMA

#if ENABLE
static int rt_hw_spi_ws2812_init(void)
{
    rt_hw_spi_device_attach(WS2812B_EXAMPLE_SPI_DEVICE_NAME, WS2812B_EXAMPLE_SPI_NAME, WS2812B_EXAMPLE_SPI_CS_PORT_NAME, WS2812B_EXAMPLE_SPI_CS_PIN_NAME);

    return RT_EOK;
}
/* 导出到自动初始化 */
INIT_COMPONENT_EXPORT(rt_hw_spi_ws2812_init);

int ws2812b_test(int argc, char *argv[])
{

    rt_device_t ws2812b_spi = rt_device_find(WS2812B_EXAMPLE_SPI_NAME);
    if (ws2812b_spi == RT_NULL)
    {
        rt_kprintf("Not Find Spi Device: %s.\r\n", WS2812B_EXAMPLE_SPI_NAME);
        return RT_ERROR;
    }
    ws2812_t ws2812 = ws2812_create(WS2812B_EXAMPLE_SPI_NAME, WS2812B_EXAMPLE_NODE_LENGTH);
    if (!ws2812)
    {
        rt_kprintf("create ws2812 object faild.\r\n");
        return RT_ERROR;
    }

    ws2812_write_rgb_to_all(ws2812, red_0);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, brown_30);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, yellow_60);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, Dark_green_90);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, Medium_green_120);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, Light_green_150);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, Baby_blue_180);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, Medium_blue_210);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    rt_free((void *)ws2812);
    return 1;
}
MSH_CMD_EXPORT(ws2812b_test, test ws2812b function);
#else
#error "SPI TX does not turn on DMA transfer."
#endif
