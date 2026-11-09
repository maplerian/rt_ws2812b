# CI: STM32F407 编译矩阵与 GitHub Pages

## 验证级别

本仓库是 RT-Thread 在线软件包，没有独立 BSP `SConstruct`。GitHub Actions 使用
`RT-Thread/rt-thread` 的 **`bsp/stm32/stm32f407-atk-explorer`** 作为编译载体，
安装 `arm-none-eabi-gcc`、SCons 和 STM32 HAL/CMSIS 软件包，随后将本仓库源码临时
接入 BSP 的 SCons 编译图，显式使用 `variant_dir=build/packages/ws2812b`
使各源文件目标对象位于可验证的路径。

`.github/workflows/ci.yml` 中的 **10 组矩阵是实编译门禁**：
每组独立运行 `scons -j`，要求成功链接 `rt-thread.elf`，再使用 `arm-none-eabi-size`
核查 ARM ELF 文件类型，并检查 `build/packages/ws2812b/` 下选中与未选中的源文件对象。
每个应参与的 `.o` 必须恰好出现一次，防止漏编译或重复编译。
任何一步失败，该 profile 的 GitHub Actions job 都失败。

这些 profile 使用 CI 专用的 `rtconfig.h` 宏覆盖层调用真实 BSP 编译图。
它们检查 **C 条件编译、SConscript 选源与最终链接**，但不验证从真实
`menuconfig` / `.config` 产生这些宏时 Kconfig 的选项约束。CI 无独立 Python-only
静态配置矩阵，也不把 SCons 解析结果当成编译成功。

**软件包索引冲突：** RT-Thread 官方 `packages` 仓库已有另一个
`peripherals/ws2812b`，共用 `PKG_USING_WS2812B` 选项但实现不同。
本 CI 故意不在 `.config` 中启用该包；若检测到 BSP 已启用该官方索引项便失败。
真正参与编译的源码仅来自 GitHub Actions `checkout` 的当前仓库，
并在 `SConstruct` 中显式添加本地 `packages/ws2812b/SConscript`。
因此这些 profile 是**通过 `rtconfig.h` 配置宏驱动的真实构建矩阵**，
不是对在线 package index 的 Kconfig 菜单解析验证。

## STM32F407 矩阵

| Profile | 编译组合 |
| --- | --- |
| `static-minimal` | 静态池、直接时序、快速 nibble 编码、一个设备 |
| `dynamic-auto` | Heap、直接时序、AUTO 通用编码回退，关闭参数检查 |
| `timing-table-extended` | 预设时序表及扩展物理时序检查 |
| `timing-search` | 搜索时序、AUTO 编码与编码自检 |
| `gamma-lut` | 静态 Gamma 2.2/2.8 LUT |
| `gamma-compute` | 动态内存、自定义 Gamma 计算、`libm` 链接 |
| `effect-minimal` | 仅编译 SOLID/BLINK，并验证初始 Blink 选择 |
| `effect-all-msh` | 七种内置灯效、三个设备槽、MSH/FinSH |
| `sparse-slots` | 仅启用第 2 槽，注册容量为 1，Gradation |
| `effect-sequence-only` | 只启用 Sequence，初始效果选择为 None |

这些设备槽中的 `spi1` / `spi2` / `spi3` 和配置的频率是**编译期数据**，
不是声称 BSP 已注册这些设备，也不是实际波形已经通过测量。
CI 不会调用 `ws2812_init_all()`，因此不要求有真实灯带或外接 MOSI 线。

可在 Linux 构建环境中执行单组构建：

```sh
sh .github/ci/ws2812b/build-stm32f407.sh static-minimal
```

或者查看合法 profile：

```sh
sh .github/ci/ws2812b/build-stm32f407.sh --list-profiles
```

每个 GitHub Actions job 上传日志、生效的编译宏、RT-Thread/Env SHA 与 GCC 版本记录，
便于定位失败配置和上游版本漂移。
`RTTHREAD_REF` 和 `RTTHREAD_ENV_REF` 当前跟随各自上游 `master`，
日志会记录实际解析到的 commit。需要严格可复现构建时可将它们改为经过验证的固定 tag/SHA。

## GitHub Pages

`.github/workflows/pages.yml` 与编译 CI 独立：

- `pull_request`（目标 `master`）：执行 `mkdocs build --strict`，不发布。
- `push` 到 `master`：严格构建成功后上传 Pages 产物并发布。
- `workflow_dispatch`：非 `master` 分支只构建、不发布。

在仓库 **Settings → Pages → Build and deployment → Source** 中选择 **GitHub Actions**。
`mkdocs.yml` 包含 [灯效实拍演示](effects-demo.md) 导航，素材来自 `doc/media/`。

## 验证边界

编译成功证明对应 profile 的源文件可在所选 BSP 工具链下编译并链接，
**不证明** STM32F103/其他芯片兼容、SPI 实际分频、电平/RESET 时序、DMA 行为、
RAM 实际资源裕量、应用轮询并发或实物灯带效果。
也没有自动修改单独维护的 `RT-Thread/packages` 在线软件包索引。

参考：

- [项目自身 STM32F407 CI 参考（rtt_isotp-c）](https://github.com/wdfk-prog/rtt_isotp-c/blob/master/.github/workflows/rt-thread-stm32-ci.yml)
- [STM32F4 编译矩阵参考（canopennode-rtt）](https://github.com/wdfk-prog/canopennode-rtt/blob/master/.github/workflows/ci.yml)
- [RT-Thread 官方源](https://github.com/RT-Thread/rt-thread)
- [GitHub Pages 自定义工作流](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
