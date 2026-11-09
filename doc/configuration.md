# WS2812B 配置与移植说明

软件包配置分为三个层次：RT-Thread 软件包索引 Kconfig、软件包源码的 `SConscript` 条件编译、板级应用实际生成的 `.config/rtconfig.h`。`RT-Thread/packages` 仓库的 Kconfig 索引与 `packages/ws2812b-latest/` 源码仓库是不同交付面；两份 Kconfig 同步维护，项目的配置结果由使用者生成。

## 构建选项

| Kconfig 选项 | 含义 / 编译行为 |
|---|---|
| `PKG_USING_WS2812B` | 启用软件包，依赖 RT-Thread SPI 设备框架 |
| `WS2812B_MEMORY_STATIC/DYNAMIC` | 静态全局对象和连续池，或使用 Heap；二选一 |
| `WS2812B_USING_KCONFIG_DEVICES` | 从编译配置创建设备槽 |
| `WS2812B_KCONFIG_SLOT_COUNT` | 可见的槽数量，上限 3；不等于全部设备注册表容量 |
| `WS2812B_MAX_DEVICES` | 注册表总容量；Kconfig 动态下限为实际启用的命名设备槽数（1～3），手动创建设备需要额外容量 |
| `WS2812B_TIMING_DIRECT/TABLE/SEARCH` | 直接使用编码参数、查表、或查表失败再整数搜索 |
| `WS2812B_ENCODER_FAST_ONLY/AUTO` | 强制使用 nibble 快速编码，或允许通用逐位回退 |
| `WS2812B_CHECK_PARAMETERS` | 部分运行期参数和容量边界检查；关闭由调用方保证输入和预留空间 |
| `WS2812B_CHECK_EXTENDED` | 更完整的脉宽验证，需在初始化阶段承担计算开销 |
| `WS2812B_USING_GAMMA`、`WS2812B_GAMMA_*` | 静态 2.2/2.8 查表，或运行时指数生成（需 libm） |
| `WS2812B_USING_EFFECT`、`WS2812B_EFFECT_*` | 方法注册表以及按方法选择的源文件；已启用的内置方法 ID 编译期连续编号 |
| `WS2812B_EFFECT_CUSTOM_SLOTS` | 额外自定义方法槽位数；方法总容量由已编译内置数自动加上额外槽数 |
| `WS2812B_USING_MSH` | 调试命令入口，依赖 FinSH、效果和 Kconfig 设备 |

`WS2812B_MAX_DEVICES` 的有效范围按启用的命名槽位数自动收紧：启用 3 路时为 3～32、启用任意 2 路时为 2～32、启用 0～1 路时为 1～32。此限制与 `WS2812B_CHECK_PARAMETERS` 无关；如果在启用额外槽位后出现无效的旧容量值，需要在 `menuconfig` 中重新选择并保存。容量不应只按 `WS2812B_KCONFIG_SLOT_COUNT`（可见槽数）计算。额外的 C API 实例仍需自行预留注册表容量。如果手工宏或陈旧的 `rtconfig.h` 仍导致槽位数量超出容量，`ws2812.c` 的独立编译期 `#error` 会拒绝该组合。

每个启用的设备槽内配置 `WS2812B_DEVn_INITIAL_*` 单选菜单，只列出当前已编译且无需运行时步骤的内置方法，并始终提供 `No initial effect`（不启动默认灯效）。SEQUENCE 需要应用提供步骤，因此不作为初始方法；仅启用 SEQUENCE 或未启用其他内置方法时，自动选择 `No initial effect`，应用仍可调用 `ws2812_effect_start_sequence()` 启动灯效。初始方法保留 Kconfig 的 `WS2812B_DEVn_INITIAL_*` 菜单选择，同时自动派生隐藏的 `WS2812B_DEVn_INITIAL_KIND` 整数；C99 预处理宏在编译时映射到压缩后的枚举 ID，不再生成 `WS2812B_DEVn_INITIAL_EFFECT` 数值宏。无默认灯效时使用内部哨兵 65535，不是可注册方法 ID。此时 `ws2812_effect_init_kconfig()` 跳过相应设备，`ws2812_effect_start_kconfig()` 返回 `RT_EOK` 而不改变现有灯效。旧版整数配置升级后须重新运行 `menuconfig` 并保存 `.config/rtconfig.h`；初始周期范围统一为 2～65535 Tick。**BLINK 必须选偶数 Tick**：其前景/背景各占 `period_ticks / 2` Tick。选中 BLINK 且配置奇数初始周期时，C99 编译期数组长度断言错误；其他方法不受偶数限制。运行时 `ws2812_effect_start()` 对 BLINK 奇数周期在 `WS2812B_CHECK_PARAMETERS=y` 时返回 `-RT_EINVAL`；MSH 命令打印错误信息并返回 `-1`（成功为 `0`），不直接返回底层 API 错误码。关闭参数检查时由调用者确保偶数，否则实际周期缩短 1 Tick。

**软件包没有 worker/thread 选项，也不自动调用 poll**。`ws2812_effect_poll_all(rt_tick_get())` 必须由应用自己的线程或主循环定期执行。禁止在 SPI 中断处理程序里使用可能等待总线互斥的 `rt_spi_send()`；RT-Thread SPI API 的同步与调用约束可参考 [官方 SPI 设备文档](https://www.rt-thread.io/document/site/programming-manual/device/spi/spi/)。

## 时钟与 RESET 配置

在 `WS2812B_TIMING_DIRECT` 下，编译阶段不编入时序查找/搜索实现。设备配置的 `WS2812B_DEVn_HZ` 是用于计算时序的**假定实际 SPI SCK**，必须与 BSP 最终时钟分频值一致。`CLOCKS_PER_BIT/T0H/T1H` 不是毫秒数，而是 SPI 时钟计数。`WS2812B_DEVn_RESET_US` 对经典版本（`VARIANT=0`）允许 60～2000 μs，对 V5（`VARIANT=1`）允许 300～2000 μs，与创建时的最小复位时间检查一致。建议在硬件上核实 T0H、T0L、T1H、T1L，以及两帧之间的 RESET-low。

手动设 `FRAME_BYTES` 时，至少需 `3*LEDS*CLOCKS_PER_BIT + 2*ceil(HZ*RESET_US/8000000)` 字节。自动配置的分配宽度按 `STATIC_MAX_CLOCKS_PER_BIT` 而非当前选中宽度计算，通常保守但可能多占 RAM；可自行按实际需求缩减，前提是初始化时容量足够。静态设备对象、RGB 池、发送帧和灯效状态是不同内存项，估算时不要重复计算。

## 一条灯带的使用顺序

以下示例适用于 `WS2812B_USING_KCONFIG_DEVICES=y` 且 `WS2812B_USING_EFFECT=y` 的配置。灯效开关默认 `n`；关闭时 `SConscript` 不编译灯效模块，不能调用 `ws2812_effect_*` API。若只使用基础像素控制，保留 attach 和 `ws2812_init_all()`，省略灯效注册、初始化及轮询调用，再通过像素写入和发送 API 控制灯带。

```c
/* If BSP has already registered the SPI device, attach registration is optional. */
ws2812_set_kconfig_attach(0U, board_attach, RT_NULL);
ws2812_effect_register_builtins();
ws2812_init_all();
ws2812_effect_init_kconfig();

/* Application task body (no package worker): */
for (;;) {
    ws2812_effect_poll_all(rt_tick_get());
    rt_thread_mdelay(20);
}
```

示例为调用顺序，不是可直接编译的完整应用：`board_attach` 由具体 BSP 实现，且每一步都应检查返回值。方法注册与设备初始化仅执行一次；`rt_thread_mdelay(20)` 是示例的应用选择，不是协议要求。可选 MSH 调试可能在另一线程中更换灯效，产品设计必须保证其与 poll 的线程安全或限制为调试条件使用。

## API 与迁移边界

原版 `ws2812_create()`、`ws2812_clear_buff()`、`ws2812_write_rgb_to_node()`、`ws2812_write_rgb_to_all()` 和 `ws2812_send()` 保留函数名与参数签名。扩展 API 允许指定 SPI 时序、亮度、Gamma 及效果，但不会替应用创建或管理线程。没有公开销毁接口，设备和方法注册表在启动后保持静态。

遇到初始化失败时先检查：SPI 设备是否注册、实际 SPI 时钟是否与配置一致、快速编码是否支持所选时序、静态池是否足够，以及默认方法是否已编译。灯珠颜色错位时先确认 GRB 顺序；颜色漂移或重启时区分数字时序异常和供电/电平问题，不能单凭软件配置判断是哪一类。
