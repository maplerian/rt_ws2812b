# rt_ws2812b

#### 1.简介

用于 RT-Thread 的 ws2812b 软件驱动包，使用 SPI + DMA 方式驱动。

##### 1.1 目录结构

|  名称   |       说明       |
| :-----: | :--------------: |
| example |   示例文件目录   |
| 根目录  | 源码及头文件目录 |

##### 1.2 许可证

rt_ws2812b package 遵循 Apachev2.0许可，详见 `LICENSE` 文件。

##### 1.3 依赖

-  RT-Thread V4.1.0

------

#### 2. 获取方式

###### 通过 git 克隆

```shell
git clone https://github.com/maplerian/rt_ws2812b.git
```

###### 在 ENV 中的开启

```
RT-Thread online packages  --->
      peripheral libraries and drivers --->
        -*- ws2812b: Ws2812b software driver package using SPI+DMA
```

#### 3. 注意事项

> 1.cubeMX配置时请将主时钟配置为48MHz,其他频率请自行计算SPI速率。
> 2.在board.h 中添加如下代码
> ```
> #define BSP_USING_SPIx
> #define BSP_SPIx_TX_USING_DMA
> ```
>x替换成你自己的SPI号
 

#### 4. 联系方式

- 维护：maplerian，godmial:<https://github.com/godmial>
- 主页：<https://github.com/maplerian/rt_ws2812b>

