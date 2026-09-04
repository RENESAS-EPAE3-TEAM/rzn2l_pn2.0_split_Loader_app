# HyperRAM 与 SDRAM 版本差异和注意事项

本文档说明拆分后的 RZN2L Loader + App 工程中 SDRAM 与 HyperRAM 两种版本的差异、配置要求和调试注意事项。配置参考为原生`rzn2l_xspi_boot` 工程。请读者在理解了README_loader_app_split.md 这篇说明之后，再来阅读些文。

## 1. 版本选择

工程提供独立的 IAR target：

| 工程 | SDRAM target | HyperRAM target |
| --- | --- | --- |
| Loader | `SDRAM` | `HyperRAM` |
| App | `Debug_EK52_App1_STANDARD`、`Debug_EK52_App3_IsoApp`、`Debug_EK52_App5_FAILSAFE_PSD`、`Debug_EK52_App44_PROFIdrive` | 对应名称带 `_hram` 后缀的 target |

Loader 与 App 必须选择匹配的内存版本。不能将 SDRAM Loader 与 HyperRAM App 混合使用，也不能反向组合。

## 2. 硬件和运行时差异

| 项目 | SDRAM 版本 | HyperRAM 版本 |
| --- | --- | --- |
| 外部内存器件 | BSC CS2 上的 SDRAM | XSPI0 CS1 上的 HyperRAM |
| 外部内存初始化 | `bsp_sdram_init()` | `hram_init()` 和 `g_hyperbus0` |
| App 使用的外部内存映射 | SDRAM / CS2 及其 mirror 区 | XSPI0 CS1 及其 mirror 区 |
| 链接器使用的外部内存容量 | `RSK_SDRAM_SIZE` | `RSK_HRAM_SIZE = 0x00800000`，即 8 MiB |
| Heap 区域 | `0x74200000..0x743FFFFF` | `0x64200000..0x644FFFFF` |
| 外部 non-cache 区域 | 基于 SDRAM CS2 mirror | 基于 XSPI0 CS1 mirror |

生成的 HyperRAM 配置把 XSPI0 unit 0 的 CS1 映射为 `0x48000000..0x4FFFFFFF`。App 链接脚本会使用该器件对应的 cache 与 mirror 地址空间。

## 3. 必需的 IAR 宏定义

`USE_HRAM` 有两个彼此独立的职责。每个 App `_hram` target 都必须同时配置。

| IAR 配置项 | HyperRAM 必需值 | 作用 |
| --- | --- | --- |
| `ICCARM > CCDefines` | `USE_HRAM` | 选择 C/C++ 代码中的 HyperRAM 分支，包括外部内存初始化与 HyperRAM 专用 SDK 行为。 |
| `ILINK > IlinkConfigDefines` | `USE_HRAM=1` | 使 `.icf` 链接脚本选择 HyperRAM 内存布局。 |

`IlinkConfigDefines` 不会定义 C 预处理宏。只在 linker 配置中设置 `USE_HRAM=1` 是不完整的：ICF 会使用 HyperRAM 地址，但 C 源码仍会按 SDRAM 版本编译。

SDRAM target 不应在上述任一配置项中定义 `USE_HRAM`。

### HyperRAM 与 QSPI 共存宏

每个 App `_hram` target 还必须在编译器宏中定义：

```text
TEMPORARY_MEASURES_WHEN_USING_QSPI_AND_HYPERFLASH_AT_THE_SAME_TIME
```

该宏名称来自原参考工程。尽管名称中使用了 `HYPERFLASH`，它同样是 HyperRAM 配置所必需的。它会使 `src/fsp_mod/r_xspi_qspi.c` 使用原工程中的 XSPI 共存处理路径，包括适当的 open 延时与 write-combine 配置。

最关键的是，该宏避免把共享 XSPI 的 `COMSTT.MEMACC` 状态直接判定为 QSPI Flash 正在写入。XSPI0 CS1 上 HyperRAM 的 memory-mapped 访问可以正常地使该位保持有效。没有该宏时，PROFINET SDK 的 `flash_wait_ready_rsk()` 可能在访问 Flash 非易失数据时无限轮询 `R_XSPI_QSPI_StatusGet()`。

## 4. 链接脚本和调试模式

| 场景 | `APP_STANDALONE_DEBUG` | App 链接脚本 |
| --- | --- | --- |
| 正常 Loader + App 启动 | `0` | `script/fsp_xspi0_boot_m.icf` |
| App 单独调试 | `1` | `script/fsp_xspi0_boot.icf` |

两个 ICF 在 HyperRAM target 中都需要 `IlinkConfigDefines=USE_HRAM=1` 才会选择 HyperRAM 布局。

正常拆分启动时，由 Loader 初始化选定的外部内存、按 App manifest 拷贝 App 各段，再跳转到 App `system_init`。App 不应重新初始化会与 Loader 冲突的 QSPI 或外部内存。

App 单独调试时，App 自身负责 QSPI 协议设置、HyperRAM/SDRAM 初始化和外部内存段初始化。`APP_STANDALONE_DEBUG` 与所选 ICF 必须成对修改。

## 5. Loader 集成规则

Loader 必须在拷贝运行于外部内存、或数据位于外部内存的 App 段之前完成外部内存初始化。

- `SDRAM` target：在拷贝 App 段前初始化 SDRAM。
- `HyperRAM` target：在拷贝 App 段前打开并校验 `g_hyperbus0`。
- App manifest 对两种版本通用；具体拷贝目的地址由 App 链接脚本决定。
- 不要未经评审就把单体原工程的外部内存启动拷贝流程直接复制到拆分 Loader。拆分设计中，Loader 应执行 App manifest 所定义的拷贝计划。

HyperRAM driver 源码、API、配置头文件和生成实例必须保留在 Loader 的 `buildinfo.ipcf` 中。仅手工添加到 `.ewp` 的源文件，在 RASC 重新生成工程后可能丢失。

## 6. 构建和下载检查表

1. 在 App 与 Loader 中选择相匹配的 SDRAM 或 HyperRAM target。
2. 对 App `_hram` target，确认同时存在 `USE_HRAM` 编译器宏、`USE_HRAM=1` 链接器宏以及 QSPI 共存宏。
3. 先编译 App，确认该 target 输出目录生成预期 `.bin`。
4. 再编译匹配的 Loader target，使其打包最新且版本匹配的 App `.bin`。
5. 集成启动测试时，下载并调试 Loader 输出镜像。
6. 修改 ICF 或内存相关宏后，查看 App map 文件，确认外部内存、Heap 和 non-cache 区域均落在预期器件上。



## 7. 配置变更注意事项

IAR target 配置（`.ewp`）、C-SPY 配置（`.ewd`）、链接脚本（`.icf`）和 RASC 输入（`configuration.xml`、`buildinfo.ipcf`）分别影响构建流程的不同部分。修改时应保持各自职责清晰，并在 IAR 重新生成工程元数据后检查实际激活的 target 配置。
