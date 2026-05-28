# RZN2L PROFINET IRT xSPI Boot Loader + App 拆分说明

本文档总结将源参考工程 `rzn2l_xspi_boot` 拆分为目标 `Loader + App` 两个 IAR 工程时的主要改动、改动原因、联合工程管理方式、App 单独调试方式，以及本次工作所使用的软件和硬件环境。

## 1. 从源参考工程拆分为 Loader + App

源参考工程 `rzn2l_xspi_boot` 是一个单体 xSPI boot 工程：启动代码、系统初始化、PROFINET IRT App、外部 SDRAM 初始化、拷贝表、链接布局和调试配置都在一个工程中完成。目标工程将它拆为两个工程：

| 工程 | 角色 | 主要职责 |
| --- | --- | --- |
| `RZN2L_bsp_xspi0bootx1_loader` | Loader / SSBL | 上电后先运行；初始化必要硬件；把 App bin 固定打包到 xSPI Flash；读取 App manifest；把 App 各段复制到运行地址；跳转到 App 启动入口。 |
| `RZN2L_bsp_xspi0bootx1_app` | App | 保留 PROFINET IRT 应用主体；发布 App 段拷贝 manifest；按源工程布局运行在 ATCM/BTCM/System RAM/SDRAM/Non-cache 区域。 |

拆分的核心原则是：Loader 只做引导、硬件前置初始化、App 搬运和跳转；App 仍然按源工程的运行内存布局启动，尽量少改 PROFINET IRT 应用主体。

### 1.1 工程结构拆分

源工程中的启动和应用逻辑被拆成两个独立 IAR project：

- Loader 工程生成 `RZN2L_bsp_xspi0bootx1_loader.out/srec`，作为最终下载和调试入口。
- App 工程生成 `RZN2L_bsp_xspi0bootx1_app.bin/out`，其中 `.bin` 被 Loader 工程作为 image input 打包进 Loader 输出镜像。
- Loader 工程的 `.ewp` 中引用 App 产物：`RZN2L_bsp_xspi0bootx1_app/Debug_EK52_App1_STANDARD/Exe/RZN2L_bsp_xspi0bootx1_app.bin`。

这样做的原因是：App 可以单独构建，Loader 可以稳定打包指定 App bin；两边职责清楚，后续只改 App 时不必重新整理 Loader 逻辑。

### 1.2 xSPI Flash 中的 App 固定分区

源工程单体时，ROM 内容由一个链接脚本统一安排。拆分后，Loader 自己占用前面的 xSPI Flash 区域，App 被固定放在后面的 App 分区：

| 项目 | 地址 / 大小 | 说明 |
| --- | --- | --- |
| App ROM offset | `0x00100000` | App 相对 xSPI0 CS0 起点偏移 1 MB，避开 Loader 区域。 |
| App raw image 起点 | `0x60100050` | 保留 xSPI loader parameter `0x50` 后的 App bin 起点。 |
| Loader 打包 App 区域 | `0x60100050` 起 | Loader 把 App `.bin` 作为 `APPLICATION_PRG_SECTION` 放入该地址。 |
| App 普通 ROM 起点 | `0x60100150` | 预留 `.app_manifest` 后，普通 App ROM 内容从这里开始。 |

这样做的原因是：Loader 和 App 链接是独立的，必须约定一个稳定的 Flash 分区边界，否则 Loader 打包位置、App 链接地址和实际运行时读取地址会互相错位。

### 1.3 App manifest 机制

拆分后 Loader 不能直接解析 App 工程里的链接符号，例如 `USER_PRG_RBLOCK`、`SYSTEM_DATA_WBLOCK` 等，因为两个工程独立链接。为了解决这个问题，App 通过 `src/app_manifest.c` 发布一个固定格式的 manifest：

```c
typedef struct
{
    uint32_t src;
    uint32_t dst;
    uint32_t size;
    uint32_t flags;
} app_manifest_entry_t;

typedef struct
{
    uint32_t             magic;
    uint32_t             entry_count;
    uint32_t             entry_point;
    uint32_t             reserved;
    app_manifest_entry_t entries[9];
} app_manifest_t;
```

App 使用 IAR 的 `__section_begin()` 和 `__section_size()` 在链接时把各段的 `src/dst/size` 写入 manifest。Loader 只需要读取固定地址处的 manifest，然后按表执行拷贝。

当前 manifest 的固定布局为：

| 项目 | 值 |
| --- | --- |
| Manifest magic | `0x50415A52` (`RZAP`) |
| Manifest 固定地址 | `0x60100050` |
| Manifest 预留窗口 | `0x60100050 .. 0x6010014F`，大小 `0x100` |
| Manifest 实际大小 | `0xA0` |
| Loader 读取地址 | `APP_MANIFEST_ADDR = 0x60100050` |
| App entry point | `system_init`，当前链接地址 `0x0010A000` |

这样做的原因是：`.app_manifest` 如果仍放在普通 `readonly` 中，地址会随 App 代码大小变化而漂移，Loader 每次都要看 App map 并手工改地址。现在 `.app_manifest` 被固定在 App raw image 的最前面，Loader 地址不再依赖 map 文件。

### 1.4 App 拷贝段和运行地址

Loader 读取 manifest 后，按 9 个 entry 复制 App 段：

| Entry | 源段 | 目的段 | 运行区域 | 说明 |
| --- | --- | --- | --- | --- |
| 0 | `LDR_PRG_RBLOCK` | `LDR_PRG_WBLOCK` | BTCM | App 启动代码和 `system_init`。 |
| 1 | `LDR_DATA_RBLOCK` | `LDR_DATA_WBLOCK` | BTCM | App 启动阶段初始化数据。 |
| 2 | `VECTOR_RBLOCK` | `VECTOR_WBLOCK` | ATCM | App 向量表。 |
| 3 | `USER_PRG_RBLOCK` | `USER_PRG_WBLOCK` | System RAM | App 主体代码。 |
| 4 | `USER_DATA_RBLOCK` | `USER_DATA_WBLOCK` | System RAM | App 初始化数据。 |
| 5 | `SYSTEM_PRG_RBLOCK` | `SYSTEM_PRG_WBLOCK` | SDRAM CS2 | PROFINET/系统相关代码段。 |
| 6 | `SYSTEM_DATA_RBLOCK` | `SYSTEM_DATA_WBLOCK` | SDRAM CS2 | PROFINET/系统相关数据段。 |
| 7 | `NONCACHE_RBLOCK` | `NONCACHE_WBLOCK` | Non-cache mirror | 初始化 non-cache 数据，当前可为 0。 |
| 8 | `SHARED_NONCACHE_BUFFER_RBLOCK` | `SHARED_NONCACHE_BUFFER_WBLOCK` | Shared non-cache mirror | 共享 non-cache 数据，当前可为 0。 |

这样做的原因是：源工程启动代码原本由 IAR runtime 和链接脚本负责把各段复制到运行地址。拆分后，App 本身不能在运行前搬运自己，所以需要 Loader 按 App 提供的 manifest 提前完成这些拷贝。

### 1.5 App LDR BTCM 偏移

调试过程中发现，如果 App 的 LDR 代码仍运行在源工程默认的 `0x00102000` 一带，Loader 在执行拷贝时可能把自己正在运行的 BTCM 代码/数据覆盖掉，导致 `bsp_copy_multibyte()` 或后续跳转异常。

当前解决方式是在 App ICF 中给 App LDR 区增加偏移：

| 项目 | 当前值 |
| --- | --- |
| `APP_LDR_BTCM_OFFSET` | `0x00008000` |
| App `LDR_DATA_WBLOCK` | `0x00108000` 起 |
| App `LDR_PRG_WBLOCK` / `system_init` | `0x0010A000` 起 |

这样做的原因是：Loader 自身仍在 BTCM 中运行，App LDR 拷贝不能覆盖 Loader 当前代码。偏移 `0x8000` 后，App LDR 和 Loader BTCM 使用区错开，同时仍在 128 KB BTCM 范围内。

### 1.6 SDRAM 初始化职责

PROFINET IRT App 有大量代码/数据运行在外部 SDRAM CS2 区域。拆分后，Loader 在跳 App 前需要完成 SDRAM 初始化，使 App 段可以被复制到 `0x74000000` 等 SDRAM 地址。

App 在 Loader 启动模式下不再重复做完整 Loader 参数和 SDRAM 初始化，避免二次初始化影响已经准备好的运行环境。

## 2. 集成到一个 IAR `.eww` 工作区的优势

把 Loader 和 App 放到同一个 IAR workspace 中管理，主要有这些优势：

1. 构建关系清楚。App 先生成 `.bin`，Loader 再把该 `.bin` 打包进最终镜像，开发者在一个 workspace 中即可看到两个工程和构建产物。
2. 调试入口统一。实际板上启动和下载以 Loader 为入口，调试时可以直接用 Loader 工程下载包含 App 的最终镜像。
3. App 可独立维护。App 仍保留自己的 IAR project、链接脚本和 map 文件，便于单独查看 App 段布局、manifest 内容和 PROFINET 相关符号。
4. 产物路径固定。Loader `.ewp` 固定引用 App `.bin`，减少手工复制 App binary 的步骤，也降低打包错版本的风险。
5. 问题定位更直接。启动链路问题看 Loader，业务和 PROFINET 问题看 App；map 文件、list 文件和 debug 配置都可以按工程分别检查。
6. 便于版本提交。Loader/App 的源代码、ICF、工程配置可以在同一个 git 仓库里按功能分组提交，后续回退和比较更容易。

建议日常开发时遵循这个顺序：

```text
1. 修改 App 或 Loader 源码/配置。
2. 先编译 App，生成最新 RZN2L_bsp_xspi0bootx1_app.bin。
3. 再编译 Loader，让 Loader 打包最新 App bin。
4. 下载/调试 Loader 输出镜像。
5. 如修改了 App ICF 或 manifest，检查 App map 和 Loader map 是否仍对齐。
```

## 3. App 单独调试功能和注意事项

目标 App 增加了单独调试宏：

```c
#ifndef APP_STANDALONE_DEBUG
#define APP_STANDALONE_DEBUG (0)
#endif
```

该宏位于 App 工程的 `rzn_cfg/fsp_cfg/bsp/board_cfg.h`。它用于区分两种启动方式：

| 模式 | `APP_STANDALONE_DEBUG` | 用途 |
| --- | --- | --- |
| Loader 启动 App | `0` | 正常集成运行。Loader 负责硬件前置初始化、App 段拷贝和跳转。 |
| App 单独调试 | `1` | 不经过 Loader，直接用 App 工程下载/调试 App。 |

### 3.1 集成调试和单独调试使用不同 ICF

App 工程需要区分两类调试/运行场景，两类场景使用的 ICF 不同：

| 场景 | 推荐宏配置 | App ICF | 说明 |
| --- | --- | --- | --- |
| Loader + App 集成调试 | `APP_STANDALONE_DEBUG = 0` | `script/fsp_xspi0_boot_m.icf` | App 被链接到 Loader 约定的 App Flash 分区；`.app_manifest` 固定在 `0x60100050`；App LDR BTCM 增加 `0x8000` 偏移；由 Loader 打包 App bin、拷贝段并跳转到 App `system_init`。 |
| App 单独调试 | `APP_STANDALONE_DEBUG = 1` | `script/fsp_xspi0_boot.icf` | App 按接近源参考工程的方式独立下载和启动；用于不经过 Loader 的 App 侧定位问题。 |

选择 ICF 时要和 `APP_STANDALONE_DEBUG` 保持一致。集成调试时如果误用单独调试 ICF，App ROM 地址、manifest 固定地址和 LDR BTCM 偏移可能不符合 Loader 的约定；单独调试时如果误用集成调试 ICF，调试器下载和入口行为也可能不符合独立启动预期。

### 3.2 正常 Loader + App 模式

正常集成运行时应保持：

```c
#define APP_STANDALONE_DEBUG (0)
```

注意事项：

- Loader 会读取 `0x60100050` 的 manifest，并按 manifest 拷贝 App 段。
- Loader 跳转目标为 App `system_init`，而不是直接跳 `main`。
- App 工程应使用 `script/fsp_xspi0_boot_m.icf`，保证 App ROM 分区、manifest 固定窗口和 App LDR BTCM 偏移与 Loader 匹配。
- App 不应再次执行与 Loader 冲突的 Loader 参数初始化或 SDRAM 初始化。
- App `.bin` 需要先生成，Loader 再重新构建，确保打包的是最新 App。
- 如果 App ICF 的段布局变化，应检查 `.app_manifest`、`LDR_PRG_RBLOCK`、`APPLICATION_PRG_RBLOCK` 是否仍与预期一致。

### 3.3 App 单独调试模式

单独调试 App 时可以临时设置：

```c
#define APP_STANDALONE_DEBUG (1)
```

注意事项：

- 单独调试时 App 不经过 Loader，所以 App 需要使用 loader parameter，并执行必要的启动路径。
- App 工程应切换到 `script/fsp_xspi0_boot.icf`，不要使用集成调试用的 `script/fsp_xspi0_boot_m.icf`。
- 单独调试只用于 App 侧定位问题；验证最终启动链路仍应回到 Loader + App 模式。
- 调试完成后要把 `APP_STANDALONE_DEBUG` 改回 `0`，否则集成运行时可能重复初始化或走错启动路径。
- 单独调试使用 App 工程自己的 debug 配置，不代表最终下载镜像的 Loader 打包流程。
- 如果调试器下载行为、复位方式或入口点被改过，需要确认 `.dnx/.dbgdt` 等 IAR 调试配置是否符合当前调试目的。

### 3.4 App manifest 校验点

每次修改 App ICF、manifest 或大规模调整代码后，建议检查 App map：

```text
.app_manifest    0x6010'0050    size 0xA0
LDR_PRG_RBLOCK   0x6010'0150    ...
system_init      0x0010'A000
```

同时检查 Loader map：

```text
APPLICATION_PRG_RBLOCK    0x6010'0050    <App bin size>
```

如果这些地址不一致，Loader 可能读错 manifest 或打包错 App image。

## 4. 本次工作环境

| 项目 | 本次使用/识别到的版本或配置 |
| --- | --- |
| 源参考工程 | `rzn2l_xspi_boot` |
| 目标 Loader 工程 | `RZN2L_bsp_xspi0bootx1_loader` |
| 目标 App 工程 | `RZN2L_bsp_xspi0bootx1_app` |
| FSP 版本 | `3.0.0` |
| RASC / Smart Configurator 包 | `sc_v2025-04.1_fsp_v3.0.0` |
| IAR EWARM | `9.70.2.18199` |
| IAR build 工具 | `C:\iar\ewarm-9.70.2\common\bin\IarBuild.exe` |
| PN / PROFINET 包 | `Renesas_PROFINET_IRT_DEVKIT_V2.0.0` |
| CPU / Device | `RZN2L`, `R9A07G084M04` |
| FSP Board 配置 | `board.rzn2lrsk.xspi0_x1` |
| RTOS | `AWS FreeRTOS` |


## 5. 当前关键文件

| 文件 | 说明 |
| --- | --- |
| `RZN2L_bsp_xspi0bootx1_app/src/app_manifest.c` | App 发布 manifest，包含 App 各段的源地址、目的地址、大小和入口点。 |
| `RZN2L_bsp_xspi0bootx1_app/script/fsp_xspi0_boot_m.icf` | App 当前使用的 ICF；定义 App ROM offset、manifest 固定窗口和 LDR BTCM 偏移。 |
| `RZN2L_bsp_xspi0bootx1_loader/src/hal_entry.c` | Loader 读取 manifest、拷贝 App 段并跳转到 App `system_init`。 |
| `RZN2L_bsp_xspi0bootx1_loader/script/fsp_xspi0_boot_loader.icf` | Loader ICF；定义 Loader 自身布局和 App bin 打包区域。 |
| `RZN2L_bsp_xspi0bootx1_app/rzn_cfg/fsp_cfg/bsp/board_cfg.h` | App 单独调试宏 `APP_STANDALONE_DEBUG`。 |
| `RZN2L_bsp_xspi0bootx1_app/RZN2L_bsp_xspi0bootx1_app.ewp` | App IAR project，当前主配置使用 `script/fsp_xspi0_boot_m.icf`。 |
| `RZN2L_bsp_xspi0bootx1_loader/RZN2L_bsp_xspi0bootx1_loader.ewp` | Loader IAR project，引用 App `.bin` 作为 image input。 |

## 6. 推荐验证清单

修改 Loader/App 链接、manifest 或启动路径后，建议至少做以下检查：

1. App 编译后检查 App map：`.app_manifest` 是否仍在 `0x60100050`，大小是否不超过 `0x100`。
2. App 编译后检查 `system_init` 是否仍在 App LDR 运行区域，例如当前 `0x0010A000`。
3. Loader 编译后检查 Loader map：`APPLICATION_PRG_RBLOCK` 是否仍从 `0x60100050` 开始。
4. 解析 App `.bin` 前 `0xA0` 字节，确认 magic 为 `0x50415A52`，entry count 为 `9`，entry point 指向 App `system_init`。
5. 板上调试时确认 Loader copy entry 0 不覆盖 Loader 自身 BTCM 代码，最终跳转地址应为 App `system_init`，不是 Loader 的旧地址。
6. App 单独调试结束后，确认 `APP_STANDALONE_DEBUG` 已恢复为 `0` 再做集成验证。

## 7. 当前可能存在的问题和限制

本节记录的问题会在后续需求中逐步完善，并随工程更新推送到 GitHub。

### 7.1 App 其它构建配置尚未验证

目前目标 App 工程主要验证的是 `EK52_App1_STANDARD` 配置。IAR workspace 中 App 相关的其它构建选项和功能配置尚未逐项验证，因此不能默认认为所有 App configuration 都已经适配 Loader + App 拆分后的启动、链接、manifest 和调试流程。

如果后续需要使用其它 App 配置，应至少重新确认：

1. 该配置使用的 ICF 是否正确，集成调试应使用 `script/fsp_xspi0_boot_m.icf`，单独调试应使用 `script/fsp_xspi0_boot.icf`。
2. 该配置是否生成 Loader 期望路径下的 App `.bin`。
3. `.app_manifest` 是否仍固定在 `0x60100050`，且大小不超过 `0x100`。
4. App `system_init`、LDR WBLOCK、USER/SYSTEM/NONCACHE 段地址是否仍与 Loader manifest 拷贝逻辑匹配。
5. Loader map 中 `APPLICATION_PRG_RBLOCK` 是否仍从 `0x60100050` 开始，并打包了正确 App `.bin`。

### 7.2 Loader + App 集成调试的 SVC_Handler 现象

当前 Loader + App 集成调试时，Loader 可以成功跳转到 App。由于 App 使用 FreeRTOS，调试器可能会在任务上下文切换相关的 `SVC_Handler` 处表现为卡住或停住。根据目前观察，这并不是程序跑飞；单步执行可以继续正常运行，离线测试功能也正常。

该现象的具体原因仍需要进一步向 PG 或 IAR 确认，可能与 FreeRTOS 上下文切换、调试器异常处理显示方式、断点/单步策略或 IAR debug 配置有关。

当前建议流程是：

1. 先确认 App 可以单独调试，并且 App 侧功能正常。
2. 再做 Loader + App 集成测试。
3. 集成调试中如果停在 `SVC_Handler`，先通过单步、运行到下一断点、任务状态和离线功能结果判断是否为真实异常。
4. 如果功能离线运行正常，不应直接把该现象判断为 Loader 跳转失败或 App 跑飞。
5. 后续需要结合 PG/IAR 支持进一步确认该调试现象的根因。

## 8. 使用 RASC 生成代码的注意事项

当前工程已经不是纯 RASC 默认工程，而是在 RASC/FSP 工程基础上增加了 Loader + App 拆分、App manifest、ICF 固定地址、App LDR BTCM 偏移、Loader 打包 App bin、App 单独调试宏等定制内容。因此可以继续使用 RASC 添加或修改配置，但不能把 RASC 生成视为无风险操作。

### 8.1 主要风险

1. RASC 可能重生成 `rzn_gen/`、`rzn_cfg/`、`configuration.xml` 和部分 `.ewp` 配置，导致手工修改被覆盖或改回默认值。
2. RASC 可能改动 App 工程使用的 ICF。集成调试依赖 `script/fsp_xspi0_boot_m.icf`，单独调试依赖 `script/fsp_xspi0_boot.icf`，两者不能混用。
3. RASC 不知道当前工程额外增加的 split boot 约定，例如 `.app_manifest` 固定在 `0x60100050`、Loader `APP_MANIFEST_ADDR`、App LDR BTCM offset、Loader image input 打包 App bin 等。
4. `src/fsp_mod/` 中的手工适配文件可能和 RASC/FSP 重新生成的文件产生差异，出现旧 patch 丢失、引用回原始 FSP 文件或重复文件的问题。
5. `.dnx/.dbgdt/.ewp` 等 IAR 工程和调试配置可能被 RASC 或 IAR 自动更新，影响下载方式、入口点、ICF 选择和 App bin 打包路径。

### 8.2 RASC 前推荐处理

使用 RASC 前建议先保证当前代码有明确的保存点：

```powershell
git status --short
git add -A
git commit -m "backup before RASC changes"
```

如果只是临时试验，也可以使用 stash：

```powershell
git stash push -u -m "temp before RASC changes"
```

这样做的目的是让 RASC 生成后的差异可以被清楚比较，也方便在生成结果不符合预期时快速回退。

### 8.3 RASC 后推荐检查

RASC 生成后，不建议马上编译。先检查改动范围：

```powershell
git status --short
git diff --name-only
```

重点检查这些文件是否被 RASC 改动：

```text
RZN2L_bsp_xspi0bootx1_app/RZN2L_bsp_xspi0bootx1_app.ewp
RZN2L_bsp_xspi0bootx1_app/script/fsp_xspi0_boot_m.icf
RZN2L_bsp_xspi0bootx1_app/script/fsp_xspi0_boot.icf
RZN2L_bsp_xspi0bootx1_app/src/app_manifest.c
RZN2L_bsp_xspi0bootx1_app/rzn_cfg/fsp_cfg/bsp/board_cfg.h
RZN2L_bsp_xspi0bootx1_app/rzn_gen/*
RZN2L_bsp_xspi0bootx1_loader/RZN2L_bsp_xspi0bootx1_loader.ewp
RZN2L_bsp_xspi0bootx1_loader/src/hal_entry.c
RZN2L_bsp_xspi0bootx1_loader/script/fsp_xspi0_boot_loader.icf
RZN2L_bsp_xspi0bootx1_loader/rzn_gen/*
```

### 8.4 RASC 后必须确认的关键约定

RASC 后至少确认以下内容仍然成立：

```text
App 集成调试 ICF = script/fsp_xspi0_boot_m.icf
App 单独调试 ICF = script/fsp_xspi0_boot.icf
APP_STANDALONE_DEBUG 集成模式为 0
.app_manifest = 0x60100050
LDR_PRG_RBLOCK = 0x60100150
system_init = 0x0010A000
Loader APP_MANIFEST_ADDR = 0x60100050
Loader APPLICATION_PRG_RBLOCK = 0x60100050
Loader 引用的 App bin 路径指向最新 App 产物
```

确认这些约定后，再按正常顺序编译：

```text
1. 先编译 App，生成最新 App bin。
2. 再编译 Loader，让 Loader 打包最新 App bin。
3. 检查 App map 和 Loader map。
4. 下载 Loader 输出镜像做板上验证。
```

### 8.5 建议

可以使用 RASC 继续维护外设、pin、clock、thread、driver stack 等配置，但每次 RASC 后都要把生成结果当作一次工程变更来 review。尤其是 ICF、`.ewp`、`board_cfg.h`、`rzn_gen/` 和 Loader/App handoff 相关文件，必须人工确认后再编译和提交。

## 9. 参考文献

- Application Note “Example of separating loader program and application program projects”。
