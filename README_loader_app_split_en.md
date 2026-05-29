# RZN2L PROFINET IRT xSPI Boot Loader + App Split Notes

This document summarizes the main changes made when splitting the source reference project `rzn2l_xspi_boot` into the target `Loader + App` IAR projects. It covers the reasons for the changes, combined project management, standalone App debugging, and the software/hardware environment used for this work.

## Table of Contents

- [1. Work Environment](#1-work-environment)
- [2. Splitting the Source Reference Project into Loader + App](#2-splitting-the-source-reference-project-into-loader--app)
    - [2.1 Project Structure Split](#21-project-structure-split)
    - [2.2 Fixed App Partition in xSPI Flash](#22-fixed-app-partition-in-xspi-flash)
    - [2.3 App Manifest Mechanism](#23-app-manifest-mechanism)
    - [2.4 App Copy Sections and Runtime Addresses](#24-app-copy-sections-and-runtime-addresses)
    - [2.5 App LDR BTCM Offset](#25-app-ldr-btcm-offset)
    - [2.6 SDRAM Initialization Responsibility](#26-sdram-initialization-responsibility)
- [3. Benefits of Integrating Both Projects into One IAR `.eww` Workspace](#3-benefits-of-integrating-both-projects-into-one-iar-eww-workspace)
- [4. App Standalone Debug Feature and Notes](#4-app-standalone-debug-feature-and-notes)
    - [4.1 Integrated Debug and Standalone Debug Use Different ICF Files](#41-integrated-debug-and-standalone-debug-use-different-icf-files)
    - [4.2 Normal Loader + App Mode](#42-normal-loader--app-mode)
    - [4.3 App Standalone Debug Mode](#43-app-standalone-debug-mode)
    - [4.4 App Manifest Checkpoints](#44-app-manifest-checkpoints)
- [5. Current Key Files](#5-current-key-files)
- [6. Recommended Verification Checklist](#6-recommended-verification-checklist)
- [7. Current Possible Issues and Limitations](#7-current-possible-issues-and-limitations)
- [8. Notes for Code Generation with RASC](#8-notes-for-code-generation-with-rasc)
- [9. References](#9-references)

## 1. Work Environment

| Item | Version or configuration used/identified in this work |
| --- | --- |
| Source reference project | `rzn2l_xspi_boot` |
| Target Loader project | `RZN2L_bsp_xspi0bootx1_loader` |
| Target App project | `RZN2L_bsp_xspi0bootx1_app` |
| FSP version | `3.0.0` |
| RASC / Smart Configurator | `sc_v2025-04.1_fsp_v3.0.0` |
| IAR EWARM | `9.70.2.18199` |
| PN / PROFINET package | `Renesas_PROFINET_IRT_DEVKIT_V2.0.0` |
| CPU / Device | `RZN2L`, `R9A07G084M04` |
| FSP Board configuration | `board.rzn2lrsk.xspi0_x1` |
| RTOS | `AWS FreeRTOS` |

The PROFINET SDK directory must be placed next to `iar_project`. The recommended directory layout is:

```text
Renesas_PROFINET_IRT_DEVKIT_V2.0.0/
+-- iar_project/
+-- profinet_sdk/
`-- LICENSE.txt
```

The current git repository root is `iar_project`, so a normal push to GitHub only includes the contents under `iar_project`; it does not include the sibling `profinet_sdk` directory. To restore the project on another machine, prepare the same-version `profinet_sdk` separately and place it at the sibling location shown above.

## 2. Splitting the Source Reference Project into Loader + App

The source reference project `rzn2l_xspi_boot` is a monolithic xSPI boot project: startup code, system initialization, the PROFINET IRT App, external SDRAM initialization, copy tables, linker layout, and debug configuration are all handled in one project. The target project splits it into two projects:

| Project | Role | Main responsibility |
| --- | --- | --- |
| `RZN2L_bsp_xspi0bootx1_loader` | Loader / SSBL | Runs first after power-on; initializes required hardware; packages the App bin at a fixed location in xSPI Flash; reads the App manifest; copies each App section to its runtime address; jumps to the App startup entry. |
| `RZN2L_bsp_xspi0bootx1_app` | App | Keeps the PROFINET IRT application body; publishes the App section copy manifest; runs with the source-project-style memory layout in ATCM/BTCM/System RAM/SDRAM/Non-cache regions. |

The core split principle is: the Loader only performs boot, prerequisite hardware initialization, App relocation, and jump; the App still starts with the source project's runtime memory layout, with minimal changes to the PROFINET IRT application body.

### 2.1 Project Structure Split

The startup and application logic from the source project are split into two independent IAR projects:

- The Loader project generates `RZN2L_bsp_xspi0bootx1_loader.out/srec`, which is the final download and debug entry.
- The App project generates `RZN2L_bsp_xspi0bootx1_app.bin/out`; the `.bin` is packaged by the Loader project as an image input into the Loader output image.
- The Loader project's `.ewp` references the App artifact: `RZN2L_bsp_xspi0bootx1_app/Debug_EK52_App1_STANDARD/Exe/RZN2L_bsp_xspi0bootx1_app.bin`.

The reason for doing this is that the App can be built independently, and the Loader can package a specified App bin in a stable way. Responsibilities are clear on both sides, so later App-only changes do not require reorganizing the Loader logic.

### 2.2 Fixed App Partition in xSPI Flash

In the monolithic source project, ROM contents are arranged by one linker script. After the split, the Loader occupies the earlier xSPI Flash area, and the App is fixed in a later App partition:

| Item | Address / size | Description |
| --- | --- | --- |
| App ROM offset | `0x00100000` | App offset is 1 MB from the xSPI0 CS0 base, avoiding the Loader area. |
| App raw image start | `0x60100050` | App bin starts after the reserved xSPI loader parameter area of `0x50`. |
| Loader-packaged App area | Starts at `0x60100050` | The Loader places the App `.bin` at this address as `APPLICATION_PRG_SECTION`. |
| Normal App ROM start | `0x60100150` | After reserving `.app_manifest`, normal App ROM contents start here. |

The reason for doing this is that the Loader and App are linked independently, so a stable Flash partition boundary must be agreed on. Otherwise, the Loader packaging address, App link address, and actual runtime read address may become inconsistent.

### 2.3 App Manifest Mechanism

After the split, the Loader cannot directly parse linker symbols from the App project, such as `USER_PRG_RBLOCK` and `SYSTEM_DATA_WBLOCK`, because the two projects are linked independently. To solve this, the App publishes a fixed-format manifest through `src/app_manifest.c`:

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

The App uses IAR `__section_begin()` and `__section_size()` to write each section's `src/dst/size` into the manifest at link time. The Loader only needs to read the manifest at the fixed address and then copy according to the table.

The current fixed manifest layout is:

| Item | Value |
| --- | --- |
| Manifest magic | `0x50415A52` (`RZAP`) |
| Fixed manifest address | `0x60100050` |
| Manifest reserved window | `0x60100050 .. 0x6010014F`, size `0x100` |
| Actual manifest size | `0xA0` |
| Loader read address | `APP_MANIFEST_ADDR = 0x60100050` |
| App entry point | `system_init`, current link address `0x0010A000` |

The reason for doing this is that if `.app_manifest` remained in normal `readonly`, its address would drift as App code size changes, and the Loader address would need to be manually updated from the App map each time. Now `.app_manifest` is fixed at the beginning of the App raw image, so the Loader address no longer depends on the map file.

### 2.4 App Copy Sections and Runtime Addresses

After reading the manifest, the Loader copies 9 App entries:

| Entry | Source section | Destination section | Runtime region | Description |
| --- | --- | --- | --- | --- |
| 0 | `LDR_PRG_RBLOCK` | `LDR_PRG_WBLOCK` | BTCM | App startup code and `system_init`. |
| 1 | `LDR_DATA_RBLOCK` | `LDR_DATA_WBLOCK` | BTCM | App startup-stage initialized data. |
| 2 | `VECTOR_RBLOCK` | `VECTOR_WBLOCK` | ATCM | App vector table. |
| 3 | `USER_PRG_RBLOCK` | `USER_PRG_WBLOCK` | System RAM | App main code. |
| 4 | `USER_DATA_RBLOCK` | `USER_DATA_WBLOCK` | System RAM | App initialized data. |
| 5 | `SYSTEM_PRG_RBLOCK` | `SYSTEM_PRG_WBLOCK` | SDRAM CS2 | PROFINET/system-related code section. |
| 6 | `SYSTEM_DATA_RBLOCK` | `SYSTEM_DATA_WBLOCK` | SDRAM CS2 | PROFINET/system-related data section. |
| 7 | `NONCACHE_RBLOCK` | `NONCACHE_WBLOCK` | Non-cache mirror | Initialized non-cache data; currently may be 0. |
| 8 | `SHARED_NONCACHE_BUFFER_RBLOCK` | `SHARED_NONCACHE_BUFFER_WBLOCK` | Shared non-cache mirror | Shared non-cache data; currently may be 0. |

The reason for doing this is that in the source project, the startup code, IAR runtime, and linker script were originally responsible for copying each section to its runtime address. After the split, the App cannot relocate itself before it runs, so the Loader must perform these copies in advance according to the manifest provided by the App.

### 2.5 App LDR BTCM Offset

During debugging, it was found that if the App LDR code still runs around the source project's default `0x00102000` area, the Loader may overwrite its own currently running BTCM code/data while copying App sections, causing `bsp_copy_multibyte()` or the later jump to behave abnormally.

The current solution is to add an offset to the App LDR region in the App ICF:

| Item | Current value |
| --- | --- |
| `APP_LDR_BTCM_OFFSET` | `0x00008000` |
| App `LDR_DATA_WBLOCK` | Starts at `0x00108000` |
| App `LDR_PRG_WBLOCK` / `system_init` | Starts at `0x0010A000` |

The reason for doing this is that the Loader itself is still running in BTCM, and copying the App LDR must not overwrite the Loader's current code. With the `0x8000` offset, the App LDR and the Loader BTCM usage area are separated while still remaining within the 128 KB BTCM range.

It is important to note that the App LDR code still must run. The Loader cannot jump directly to App `main()` after completing section copies. The reason is that the App LDR is responsible for establishing the App's own startup context, including the App entry `system_init`, startup stack/runtime environment preparation, the App-side `SystemInit()`/warm start path, and the BSP/interrupt/RTOS initialization flow required before entering `main()`. The Loader can only place the App image at the correct runtime addresses and provide the required hardware prerequisites before the jump; it cannot replace the App project's own startup entry.

The functional difference between Loader LDR and App LDR can be understood as follows:

| Item | Loader LDR | App LDR |
| --- | --- | --- |
| Execution timing | Runs first after power-on or reset. | Runs after the Loader finishes copying App sections. |
| Main goal | Establishes the minimum boot environment, initializes required hardware, reads/packages/relocates the App, and jumps to the App. | Establishes the App's own runtime context, executes App startup entry `system_init`, and enters App runtime and `main()`. |
| Flash/App handling | Reads the App manifest and copies each App section to ATCM/BTCM/System RAM/SDRAM/Non-cache runtime addresses. | Does not package or relocate the whole App from Flash; in integrated mode, App sections have already been copied by the Loader according to the manifest. |
| Hardware initialization boundary | Handles the common hardware prerequisite initialization that must be completed before jumping to the App, such as SDRAM initialization. | Handles BSP/system initialization required by the App-side startup flow; in integrated mode, it should avoid repeating initialization that conflicts with the Loader. |
| Jump target | Finally jumps to the `entry_point` in the App manifest, currently App `system_init`. | Enters App `main()` and FreeRTOS/application tasks after completing the App startup flow. |

Therefore, the current solution does not delete or bypass the App LDR. Instead, it moves the App LDR runtime address away from the BTCM area used by the Loader, allowing the Loader to safely copy the App LDR into BTCM and then jump into App `system_init`.

### 2.6 SDRAM Initialization Responsibility

The PROFINET IRT App has a large amount of code/data running in the external SDRAM CS2 region. After the split, the Loader must complete SDRAM initialization before jumping to the App, so that App sections can be copied to SDRAM addresses such as `0x74000000`.

## 3. Benefits of Integrating Both Projects into One IAR `.eww` Workspace

Managing the Loader and App in the same IAR workspace provides these main benefits:

1. Clear build relationship. The App generates the `.bin` first, and the Loader then packages that `.bin` into the final image. Developers can see both projects and build artifacts in one workspace.
2. Unified debug entry. Actual board startup and download use the Loader as the entry, and debugging can directly download the final image containing the App through the Loader project.
3. App can be maintained independently. The App still keeps its own IAR project, linker script, and map file, making it easy to inspect App section layout, manifest content, and PROFINET-related symbols separately.
4. Fixed artifact path. The Loader `.ewp` references the App `.bin` through a fixed path, reducing manual App binary copy steps and lowering the risk of packaging the wrong version.
5. More direct issue localization. Startup-chain issues are checked in the Loader, while business logic and PROFINET issues are checked in the App. Map files, list files, and debug configuration can be inspected per project.
6. Easier version control. Loader/App source code, ICF files, and project configuration can be committed in the same git repository by feature, making later rollback and comparison easier.

The recommended daily development sequence is:

```text
1. Modify App or Loader source/configuration.
2. Build the App first to generate the latest RZN2L_bsp_xspi0bootx1_app.bin.
3. Build the Loader next, so the Loader packages the latest App bin.
4. Download/debug the Loader output image.
5. If the App ICF or manifest changed, check whether the App map and Loader map are still aligned.
```

## 4. App Standalone Debug Feature and Notes

The target App adds a standalone debug macro:

```c
#ifndef APP_STANDALONE_DEBUG
#define APP_STANDALONE_DEBUG (0)
#endif
```

This macro is located in the App project's `rzn_cfg/fsp_cfg/bsp/board_cfg.h`. It distinguishes two startup modes:

| Mode | `APP_STANDALONE_DEBUG` | Usage |
| --- | --- | --- |
| Loader starts App | `0` | Normal integrated operation. The Loader handles hardware prerequisite initialization, App section copies, and the jump. |
| App standalone debug | `1` | Debug/download the App directly through the App project, without going through the Loader. |

### 4.1 Integrated Debug and Standalone Debug Use Different ICF Files

The App project must distinguish two debug/runtime scenarios, and the two scenarios use different ICF files:

| Scenario | Recommended macro configuration | App ICF | Description |
| --- | --- | --- | --- |
| Loader + App integrated debug | `APP_STANDALONE_DEBUG = 0` | `script/fsp_xspi0_boot_m.icf` | The App is linked to the App Flash partition agreed with the Loader; `.app_manifest` is fixed at `0x60100050`; App LDR BTCM has a `0x8000` offset; the Loader packages the App bin, copies sections, and jumps to App `system_init`. |
| App standalone debug | `APP_STANDALONE_DEBUG = 1` | `script/fsp_xspi0_boot.icf` | The App is independently downloaded and started in a way close to the source reference project; used for App-side issue localization without the Loader. |

The selected ICF must match `APP_STANDALONE_DEBUG`. If the standalone-debug ICF is used during integrated debug, the App ROM address, fixed manifest address, and LDR BTCM offset may not match the Loader agreement. If the integrated-debug ICF is used during standalone debug, debugger download and entry behavior may not match standalone startup expectations.

### 4.2 Normal Loader + App Mode

For normal integrated operation, keep:

```c
#define APP_STANDALONE_DEBUG (0)
```

Notes:

- The Loader reads the manifest at `0x60100050` and copies App sections according to the manifest.
- The Loader jump target is App `system_init`, not `main` directly.
- The App project should use `script/fsp_xspi0_boot_m.icf` to ensure that the App ROM partition, fixed manifest window, and App LDR BTCM offset match the Loader.
- The App should not execute Loader parameter initialization or SDRAM initialization again if doing so conflicts with the Loader.
- The App `.bin` must be generated first, and then the Loader must be rebuilt to ensure the latest App is packaged.
- If the App ICF section layout changes, check whether `.app_manifest`, `LDR_PRG_RBLOCK`, and `APPLICATION_PRG_RBLOCK` are still as expected.

### 4.3 App Standalone Debug Mode

For standalone App debugging, temporarily set:

```c
#define APP_STANDALONE_DEBUG (1)
```

Notes:

- In standalone debug, the App does not go through the Loader, so the App needs to use loader parameters and execute the required startup path.
- The App project should switch to `script/fsp_xspi0_boot.icf`, and should not use the integrated-debug `script/fsp_xspi0_boot_m.icf`.
- Standalone debug is only for App-side issue localization; final startup-chain verification should return to Loader + App mode.
- After debugging, change `APP_STANDALONE_DEBUG` back to `0`; otherwise, integrated operation may repeat initialization or take the wrong startup path.
- Standalone debug uses the App project's own debug configuration and does not represent the Loader packaging flow of the final download image.
- If debugger download behavior, reset mode, or entry point has been changed, confirm whether `.dnx/.dbgdt` and other IAR debug configurations match the current debug purpose.

### 4.4 App Manifest Checkpoints

After each App ICF, manifest, or large-scale code adjustment, it is recommended to check the App map:

```text
.app_manifest    0x6010'0050    size 0xA0
LDR_PRG_RBLOCK   0x6010'0150    ...
system_init      0x0010'A000
```

Also check the Loader map:

```text
APPLICATION_PRG_RBLOCK    0x6010'0050    <App bin size>
```

If these addresses are inconsistent, the Loader may read the wrong manifest or package the wrong App image.

## 5. Current Key Files

| File | Description |
| --- | --- |
| `RZN2L_bsp_xspi0bootx1_app/src/app_manifest.c` | App publishes the manifest containing source address, destination address, size, and entry point for each App section. |
| `RZN2L_bsp_xspi0bootx1_app/script/fsp_xspi0_boot_m.icf` | Current App ICF; defines App ROM offset, fixed manifest window, and LDR BTCM offset. |
| `RZN2L_bsp_xspi0bootx1_loader/src/hal_entry.c` | Loader reads the manifest, copies App sections, and jumps to App `system_init`. |
| `RZN2L_bsp_xspi0bootx1_loader/script/fsp_xspi0_boot_loader.icf` | Loader ICF; defines the Loader's own layout and App bin packaging area. |
| `RZN2L_bsp_xspi0bootx1_app/rzn_cfg/fsp_cfg/bsp/board_cfg.h` | App standalone debug macro `APP_STANDALONE_DEBUG`. |
| `RZN2L_bsp_xspi0bootx1_app/RZN2L_bsp_xspi0bootx1_app.ewp` | App IAR project; the current main configuration uses `script/fsp_xspi0_boot_m.icf`. |
| `RZN2L_bsp_xspi0bootx1_loader/RZN2L_bsp_xspi0bootx1_loader.ewp` | Loader IAR project; references the App `.bin` as an image input. |

## 6. Recommended Verification Checklist

After modifying Loader/App linking, manifest, or startup path, at least perform these checks:

1. After building the App, check the App map: whether `.app_manifest` is still at `0x60100050` and whether its size does not exceed `0x100`.
2. After building the App, check whether `system_init` is still in the App LDR runtime area, for example the current `0x0010A000`.
3. After building the Loader, check the Loader map: whether `APPLICATION_PRG_RBLOCK` still starts from `0x60100050`.
4. Parse the first `0xA0` bytes of the App `.bin` and confirm that magic is `0x50415A52`, entry count is `9`, and entry point points to App `system_init`.
5. During board debugging, confirm that Loader copy entry 0 does not overwrite the Loader's own BTCM code. The final jump address should be App `system_init`, not an old Loader address.
6. After App standalone debugging, confirm that `APP_STANDALONE_DEBUG` has been restored to `0` before integrated verification.

## 7. Current Possible Issues and Limitations

The issues in this section will be gradually improved in later requirements and pushed to GitHub as the project is updated.

### 7.1 Other App Build Configurations Have Not Been Verified

Currently, the target App project has mainly been verified with the `EK52_App1_STANDARD` configuration. Other App-related build options and functional configurations in the IAR workspace have not been verified one by one, so they should not be assumed to already support the startup, linking, manifest, and debug flow after the Loader + App split.

If other App configurations need to be used later, at least reconfirm:

1. Whether the ICF used by that configuration is correct: integrated debug should use `script/fsp_xspi0_boot_m.icf`, and standalone debug should use `script/fsp_xspi0_boot.icf`.
2. Whether that configuration generates the App `.bin` at the path expected by the Loader.
3. Whether `.app_manifest` is still fixed at `0x60100050`, and whether its size does not exceed `0x100`.
4. Whether App `system_init`, LDR WBLOCK, USER/SYSTEM/NONCACHE section addresses still match the Loader manifest copy logic.
5. Whether `APPLICATION_PRG_RBLOCK` in the Loader map still starts from `0x60100050` and packages the correct App `.bin`.

### 7.2 `SVC_Handler` Behavior During Loader + App Integrated Debug

During current Loader + App integrated debugging, the Loader can successfully jump to the App. Because the App uses FreeRTOS, the debugger may appear to hang or stop at `SVC_Handler`, which is related to task context switching. Based on current observations, this is not a program runaway condition; single-step execution can continue normally, and offline test functionality also works normally.

The specific cause still needs further confirmation with PG or IAR. It may be related to FreeRTOS context switching, debugger exception display behavior, breakpoint/single-step strategy, or IAR debug configuration.

The currently recommended flow is:

1. First confirm that the App can be debugged standalone and that App-side functionality is normal.
2. Then perform Loader + App integrated testing.
3. If integrated debugging stops at `SVC_Handler`, first determine whether it is a real exception by using single-step, run-to-next-breakpoint, task state, and offline function results.
4. If offline operation works normally, do not directly treat this behavior as Loader jump failure or App runaway.
5. Further confirmation with PG/IAR support is needed later to determine the root cause of this debug behavior.

## 8. Notes for Code Generation with RASC

The current project is no longer a pure RASC default project. On top of the RASC/FSP project, it now includes customized content such as Loader + App split, App manifest, fixed ICF addresses, App LDR BTCM offset, Loader packaging App bin, and the App standalone debug macro. Therefore, RASC can still be used to add or modify configuration, but RASC generation must not be treated as risk-free.

### 8.1 Main Risks

1. RASC may regenerate `rzn_gen/`, `rzn_cfg/`, `configuration.xml`, and some `.ewp` settings, causing manual changes to be overwritten or reverted to defaults.
2. RASC may change the ICF used by the App project. Integrated debug depends on `script/fsp_xspi0_boot_m.icf`, standalone debug depends on `script/fsp_xspi0_boot.icf`, and the two must not be mixed.
3. RASC does not know the additional split-boot agreements in the current project, such as `.app_manifest` fixed at `0x60100050`, Loader `APP_MANIFEST_ADDR`, App LDR BTCM offset, and Loader image input packaging the App bin.
4. Manually adapted files in `src/fsp_mod/` may diverge from regenerated RASC/FSP files, causing old patches to be lost, references to return to original FSP files, or duplicate files to appear.
5. IAR project and debug configuration files such as `.dnx/.dbgdt/.ewp` may be automatically updated by RASC or IAR, affecting download method, entry point, ICF selection, and App bin packaging path.

### 8.2 Recommended Handling Before RASC

Before using RASC, it is recommended to make sure the current code has a clear save point:

```powershell
git status --short
git add -A
git commit -m "backup before RASC changes"
```

If it is only a temporary experiment, stash can also be used:

```powershell
git stash push -u -m "temp before RASC changes"
```

The purpose is to make differences after RASC generation easy to compare, and to make quick rollback possible if the generated result does not meet expectations.

### 8.3 Recommended Checks After RASC

After RASC generation, it is not recommended to build immediately. First check the change scope:

```powershell
git status --short
git diff --name-only
```

Pay special attention to whether RASC changed these files:

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

### 8.4 Key Agreements That Must Be Confirmed After RASC

After RASC, at least confirm that the following agreements still hold:

```text
App integrated debug ICF = script/fsp_xspi0_boot_m.icf
App standalone debug ICF = script/fsp_xspi0_boot.icf
APP_STANDALONE_DEBUG is 0 in integrated mode
.app_manifest = 0x60100050
LDR_PRG_RBLOCK = 0x60100150
system_init = 0x0010A000
Loader APP_MANIFEST_ADDR = 0x60100050
Loader APPLICATION_PRG_RBLOCK = 0x60100050
The App bin path referenced by Loader points to the latest App artifact
```

After confirming these agreements, build in the normal sequence:

```text
1. Build the App first to generate the latest App bin.
2. Build the Loader next, so the Loader packages the latest App bin.
3. Check the App map and Loader map.
4. Download the Loader output image for board verification.
```

### 8.5 Recommendation

RASC can continue to be used to maintain peripheral, pin, clock, thread, and driver stack configuration. However, after each RASC generation, treat the generated result as a project change that needs review. In particular, ICF files, `.ewp`, `board_cfg.h`, `rzn_gen/`, and Loader/App handoff-related files must be manually confirmed before building and committing.

## 9. References

- Application Note "Example of separating loader program and application program projects".
