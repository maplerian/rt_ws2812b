# WS2812B SPI Driver for RT-Thread

This site publishes the package's existing technical documentation. The driver
uses RT-Thread SPI to generate the WS2812B GRB waveform and supports multiple
independent strips, selectable memory backends, optional Gamma correction and
application-polled LED effects.

- [Architecture and data flow](architecture.md): module boundaries, initialization,
  SPI encoding and memory budgeting.
- [Configuration and porting](configuration.md): Kconfig options, memory and timing
  constraints, feature dependencies, and board integration.
- [WS2812B SPI waveform theory](ws2812b-spi-theory.md): GRB, pulse timing, the
  nibble lookup encoder and reset-low frames.
- [LED effect demonstrations](effects-demo.md): original videos, photos and previews.
- [STM32F407 compile CI and Pages](ci.md): real SCons build profiles and deployment setup.

The repository has no local `SConstruct`. CI integrates the package into the
STM32F407 RT-Thread BSP, compiles selected feature combinations with SCons,
links an ELF and checks emitted package objects. This is not a hardware test.
