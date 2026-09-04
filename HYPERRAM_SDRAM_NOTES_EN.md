# HyperRAM and SDRAM Build Variants

This document describes the SDRAM and HyperRAM variants of the split RZN2L Loader + App project. The reference configuration is the native `rzn2l_xspi_boot` project. Please read the README_loader_app_split_en.md first, and then look at this md file.

## 1. Variant Selection

The projects provide separate IAR targets:

| Project | SDRAM target(s) | HyperRAM target(s) |
| --- | --- | --- |
| Loader | `SDRAM` | `HyperRAM` |
| App | `Debug_EK52_App1_STANDARD`, `Debug_EK52_App3_IsoApp`, `Debug_EK52_App5_FAILSAFE_PSD`, `Debug_EK52_App44_PROFIdrive` | Corresponding target names ending in `_hram` |

Select a matching Loader and App variant. Do not combine an SDRAM Loader image with a HyperRAM App image, or the reverse.

## 2. Hardware and Runtime Differences

| Aspect | SDRAM variant | HyperRAM variant |
| --- | --- | --- |
| External memory device | SDRAM on BSC CS2 | HyperRAM on XSPI0 CS1 |
| External memory initialization | `bsp_sdram_init()` | `hram_init()` and `g_hyperbus0` |
| External memory mapping used by the App | SDRAM / CS2 and its mirror | XSPI0 CS1 and its mirror |
| External memory size used by the linker | `RSK_SDRAM_SIZE` | `RSK_HRAM_SIZE = 0x00800000` (8 MiB) |
| Heap region | `0x74200000..0x743FFFFF` | `0x64200000..0x644FFFFF` |
| Non-cache external region | Based on the SDRAM CS2 mirror | Based on the XSPI0 CS1 mirror |

The generated HyperRAM configuration maps XSPI0 unit 0, CS1 to `0x48000000..0x4FFFFFFF`. The application linker scripts use the corresponding cached and mirror address spaces.

## 3. Required IAR Definitions

`USE_HRAM` has two independent roles. Both must be configured for every App `_hram` target.

| IAR setting | Required HyperRAM value | Purpose |
| --- | --- | --- |
| `ICCARM > CCDefines` | `USE_HRAM` | Selects HyperRAM C/C++ source branches, including external-memory initialization and HyperRAM-specific SDK behavior. |
| `ILINK > IlinkConfigDefines` | `USE_HRAM=1` | Makes the `.icf` linker script select the HyperRAM memory layout. |

`IlinkConfigDefines` does not define a C preprocessor macro. Setting only `USE_HRAM=1` in the linker configuration is incomplete: the ICF selects HyperRAM addresses while C sources still compile as the SDRAM variant.

SDRAM targets must not define `USE_HRAM` in either setting.

### HyperRAM QSPI coexistence definition

Every App `_hram` target must also define the following compiler macro:

```text
TEMPORARY_MEASURES_WHEN_USING_QSPI_AND_HYPERFLASH_AT_THE_SAME_TIME
```

The name is inherited from the reference project, but it is required for the HyperRAM configuration as well. It enables the reference driver's XSPI coexistence path in `src/fsp_mod/r_xspi_qspi.c`, including the appropriate open delay and write-combine setting.

Most importantly, it avoids treating the shared XSPI `COMSTT.MEMACC` state as a QSPI Flash write-in-progress indication. HyperRAM memory-mapped accesses on XSPI0 CS1 can legitimately keep that bit active. Without this definition, the PROFINET SDK's `flash_wait_ready_rsk()` can poll `R_XSPI_QSPI_StatusGet()` forever during nonvolatile-data Flash access.

## 4. Linker Scripts and Debug Modes

| Scenario | `APP_STANDALONE_DEBUG` | App linker script |
| --- | --- | --- |
| Normal Loader + App boot | `0` | `script/fsp_xspi0_boot_m.icf` |
| App-only standalone debug | `1` | `script/fsp_xspi0_boot.icf` |

Both linker scripts require the `IlinkConfigDefines=USE_HRAM=1` setting for the HyperRAM layout.

For normal split boot, the Loader initializes the selected external memory, copies the App sections from the App manifest, and jumps to App `system_init`. The App must not reinitialize QSPI or external memory in a way that conflicts with the Loader.

For standalone App debug, the App is responsible for QSPI protocol setup, HyperRAM/SDRAM initialization, and external-memory section initialization. The selected linker script and `APP_STANDALONE_DEBUG` value must be changed together.

## 5. Loader Integration Rules

The Loader must initialize external memory before it copies App sections that execute from or hold data in external memory.

- `SDRAM` target: initialize SDRAM before App section copies.
- `HyperRAM` target: open and verify `g_hyperbus0` before App section copies.
- The App manifest is common to both variants; its copy destinations are resolved by the App linker script.
- Do not copy the monolithic reference project's external-memory startup copy sequence into the split Loader without review. In the split design, the Loader performs the App manifest copy plan.

The HyperRAM driver source, APIs, configuration header, and generated instance must remain registered in the Loader `buildinfo.ipcf`. Manual `.ewp` source entries alone may disappear after RASC regeneration.

## 6. Build and Deployment Checklist

1. Choose matching SDRAM or HyperRAM targets in both the App and Loader projects.
2. For an App `_hram` target, confirm both `USE_HRAM` compiler/linker definitions and the coexistence workaround compiler definition.
3. Build the App first and confirm the expected `.bin` is produced in that target's output directory.
4. Build the matching Loader target so it packages the latest matching App `.bin`.
5. Download and debug the Loader output for an integrated boot test.
6. After modifying an ICF or memory-related definition, inspect the App map file and verify the external-memory, heap, and non-cache regions use the intended device.

## 7. Debugging Checklist

If a HyperRAM App stops in `R_XSPI_QSPI_StatusGet()`:

1. Check the caller. The usual loop is `flash_wait_ready_rsk()`, not a blocking wait inside `StatusGet()` itself.
2. Verify `TEMPORARY_MEASURES_WHEN_USING_QSPI_AND_HYPERFLASH_AT_THE_SAME_TIME` is present in the active target's `ICCARM > CCDefines`.
3. Verify `USE_HRAM` is present in both `CCDefines` and `IlinkConfigDefines` with their respective syntax.
4. Confirm that the correct custom `src/fsp_mod/r_xspi_qspi.c` is selected, not the stock FSP QSPI driver.
5. For standalone App debug, verify `APP_STANDALONE_DEBUG = 1` and `fsp_xspi0_boot.icf` are selected together.
6. For integrated boot, verify `APP_STANDALONE_DEBUG = 0`, `fsp_xspi0_boot_m.icf`, a current App `.bin`, and the matching Loader target.

## 8. Change Control Notes

Changes to IAR project targets (`.ewp`), C-SPY configuration (`.ewd`), linker scripts (`.icf`), and RASC inputs (`configuration.xml`, `buildinfo.ipcf`) affect different parts of the build. Keep these responsibilities separate and review the active target after IAR regenerates project metadata.
