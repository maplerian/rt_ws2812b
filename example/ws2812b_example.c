#include <ws2812.h>

#ifndef WS2812B_EXAMPLE_SPI_NAME
#define WS2812B_EXAMPLE_SPI_NAME    "spi1"
#endif  //!WS2812B_EXAMPLE_SPI_NAME

#ifndef WS2812B_EXAMPLE_NODE_LENGTH
#define WS2812B_EXAMPLE_NODE_LENGTH 1
#endif // !WS2812B_EXAMPLE_NODE_LENGTH

#ifdef BSP_SPI1_TX_USING_DMA || BSP_SPI2_TX_USING_DMA || BSP_SPI3_TX_USING_DMA || BSP_SPI4_TX_USING_DMA || BSP_SPI5_TX_USING_DMA || BSP_SPI6_TX_USING_DMA
#define ENABLE  1
#else
#define ENABLE  0
#endif // BSP_SPI1_TX_USING_DMA || BSP_SPI2_TX_USING_DMA || BSP_SPI3_TX_USING_DMA || BSP_SPI4_TX_USING_DMA || BSP_SPI5_TX_USING_DMA || BSP_SPI6_TX_USING_DMA

#if ENABLE
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
    //  低于这个值可能会造成某节点无法显示这个值
    #define COLOR_MIN_VALUE   10
    ws2812_write_rgb_to_all(ws2812, COLOR_MIN_VALUE, 0, 0);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, 0, COLOR_MIN_VALUE, 0);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, 0, 0, COLOR_MIN_VALUE);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, COLOR_MIN_VALUE, COLOR_MIN_VALUE, 0);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, COLOR_MIN_VALUE, 0, COLOR_MIN_VALUE);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, 0, COLOR_MIN_VALUE, COLOR_MIN_VALUE);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, COLOR_MIN_VALUE, COLOR_MIN_VALUE, COLOR_MIN_VALUE);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    ws2812_write_rgb_to_all(ws2812, 0, 0, 0);
    ws2812_send(ws2812);
    rt_thread_mdelay(500);

    rt_free((void *p)ws2812);
    return 1;
}
MSH_CMD_EXPORT(ws2812b_test, test ws2812b function);
#else
#error "SPI TX does not turn on DMA transfer."
#endif
