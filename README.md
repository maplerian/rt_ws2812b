# rt_ws2812b

适用于 **RT-Thread** 的 WS2812B 可寻址 RGB 灯带软件包：通过 SPI MOSI 合成单线时序，支持多路灯带、可配置编码、静态/动态内存、Gamma 校正与可选动态灯效。

## 实物效果

[![WS2812B 灯带颜色轮换演示](doc/media/preview-color-cycle.gif)](doc/effects-demo.md)

*颜色轮换实拍 GIF（由原始视频 `video(7).mp4` 制作，推测对应 Sequence / 颜色轮播）。*

**[查看全部 11 段视频和 3 张照片 →](doc/effects-demo.md)**（保留原始 MP4；包含 Blink、Chase、Gradient、Gradation、Sequence 等推测分类）

## 功能一览

| 功能 | 说明 |
| --- | --- |
| 多设备 | 通过 Kconfig 配置最多 3 个命名设备槽；设备注册容量可单独设置 |
| 波形编码 | SPI MOSI 输出 GRB 串行码；支持四位查表快速编码与可选逐位回退 |
| 时序 | 直接配置 SPI 时钟计数、使用预设表或启用搜索回退 |
| 内存 | 静态对象/像素/发送帧池，或 RT-Thread Heap 模式 |
| 亮度与 Gamma | 每路亮度控制、Gamma 2.2/2.8 查表及可选自定义指数 |
| 动态灯效 | Solid、Blink、Breathe、Gradient、Gradation、Chase、Sequence；可独立编译 |
| 调试 | 可选 FinSH/MSH 命令，用于切换效果、颜色、亮度及 Gamma |

**两类“渐变”不同**：`Gradient` 指整条灯带在两种颜色之间随时间过渡；`Gradation` 指不同位置出现不同颜色，并让空间渐变随时间变化。

## 快速开始

### 1. 启用软件包

在 RT-Thread `menuconfig` 中启用 `PKG_USING_WS2812B`（依赖 SPI 设备框架）。按实际 BSP 设置 SPI bus/device、灯珠数和物理时序，不要把配置频率直接当作已测量的 SCK。

| 常用配置项 | 作用 |
| --- | --- |
| `WS2812B_USING_KCONFIG_DEVICES` | 从 Kconfig 创建命名设备（默认开启） |
| `WS2812B_MEMORY_STATIC` / `WS2812B_MEMORY_DYNAMIC` | 静态内存或 Heap；默认静态 |
| `WS2812B_KCONFIG_SLOT_COUNT` | 设备槽菜单可见数量（1～3） |
| `WS2812B_MAX_DEVICES` | 总注册容量，需覆盖启用的设备槽和手工实例 |
| `WS2812B_USING_EFFECT` | 启用灯效框架（默认关闭） |
| `WS2812B_USING_MSH` | 启用调试命令（需 FinSH、灯效和 Kconfig 设备） |

其余波形、内存预算、默认效果及构建条件见 [Kconfig 配置与移植说明](doc/configuration.md)。

### 2. 准备 SPI 设备并初始化

先由 BSP 注册 SPI 设备；若设备未预先注册，应用需在初始化前为对应槽提供 `ws2812_set_kconfig_attach()` 回调。下面示例假设 **SPI 设备已经注册**、Kconfig 设备配置有效：

```c
#include "ws2812.h"
#ifdef WS2812B_USING_EFFECT
#include "ws2812_effect.h"
#endif

static rt_err_t app_ws2812_init(void)
{
    rt_err_t result;
#ifdef WS2812B_USING_EFFECT
    result = ws2812_effect_register_builtins();
    if (result != RT_EOK)
        return result;
#endif

    result = ws2812_init_all();
    if (result != RT_EOK)
        return result;

#ifdef WS2812B_USING_EFFECT
    result = ws2812_effect_init_kconfig();
#endif
    return result;
}
```

示例仅展示调用关系；由应用决定 `app_ws2812_init()` 的调用时机并处理错误。基础像素控制可调用 `ws2812_fill()` / `ws2812_set_pixel()` 后再调用 `ws2812_send_frame()`。

### 3. 驱动动态灯效（可选）

启用 `WS2812B_USING_EFFECT` 时，应用应从**自己管理的任务或事件循环**定期执行：

```c
/* 应用选择轮询周期；当前 now 取 RT-Thread Tick。 */
rt_err_t result = ws2812_effect_poll_all(rt_tick_get());
/* 由应用记录、处理非 RT_EOK 返回值。 */
```

软件包**不创建后台线程、不自动轮询**；首次 `start()` 选中效果后，实际渲染与 SPI 发送也需等待应用轮询。所有针对同一设备的效果修改与轮询操作应由应用串行化。

## MSH 调试（可选）

启用 `WS2812B_USING_MSH` 后，可在 FinSH/MSH 使用以下命令。执行命令不代表已经验证硬件波形。

```text
ws2812b list             # 查看各设备
ws2812b bright 64        # 调低亮度
ws2812b s red            # 红色常亮
ws2812b bl               # 红色闪烁
ws2812b gd               # 空间渐变
ws2812b seq              # 红绿蓝颜色序列
ws2812b reset            # 恢复 Kconfig 默认效果
ws2812b help             # 所有命令及参数
```

MSH 与应用轮询可能处于不同线程，应用必须保证并发安全；不能仅依赖 SPI 总线锁来保护像素/灯效状态。

## 文档

- [实物效果展示：全部 11 段视频与 3 张照片](doc/effects-demo.md)
- [配置与移植](doc/configuration.md)：设备槽、内存、时序、默认效果与 BSP 接入
- [架构与数据流](doc/architecture.md)：初始化、编码、发送、效果轮询和资源预算
- [WS2812B 单线时序与 SPI 编码原理](doc/ws2812b-spi-theory.md)：GRB、nibble LUT 与 RESET 波形

## 使用边界

- SPI 发送的是 WS2812B **MOSI 波形**，不是标准 SPI 从设备协议；需核对 MOSI 空闲电平、真实 SCK、RESET 低电平以及所用灯珠版本。
- 不包含板级 GPIO/CS/DMA 初始化，也不代替示波器、电源完整性和实际灯带验证。
- 静态模式需预留足够的像素与 SPI 帧内存；关闭参数检查后，调用方自行承担容量与输入合法性约束。
- 原有 `ws2812_create()`、`ws2812_write_rgb_to_node()` 等兼容入口仍保留；扩展对象不支持沿用旧示例直接 `rt_free` 的释放方式。

## License

本软件包遵循 [Apache License 2.0](LICENSE)。
