/*=============================================================================
 * app_manifest.c
 *
 * Application image manifest consumed by the SSBL (Loader) project
 * RZN2L_bsp_xspi0bootx1_loader.
 *
 * The Loader cannot resolve App-side link-time symbols (the two projects are
 * built and linked independently). To bridge this, the App publishes a small
 * manifest struct at a fixed physical address in xSPI flash. The App ICF keeps
 * .app_manifest in a reserved 0x100-byte window at the beginning of the App raw
 * image, and the Loader's hal_entry() reads it directly via that pointer before
 * performing the listed memcpy()s and jumping to entry_point.
 *
 * The src/dst/size values are filled in by the IAR linker using the
 * __section_begin / __section_size intrinsics on the blocks defined in
 * script/fsp_xspi0_boot_m.icf:
 *
 *    LDR_PRG_RBLOCK                 -> LDR_PRG_WBLOCK                 (BTCM, offset)
 *    LDR_DATA_RBLOCK                -> LDR_DATA_WBLOCK                (BTCM, offset)
 *    VECTOR_RBLOCK                  -> VECTOR_WBLOCK                  (ATCM)
 *    USER_PRG_RBLOCK                -> USER_PRG_WBLOCK                (SystemRAM)
 *    USER_DATA_RBLOCK               -> USER_DATA_WBLOCK               (SystemRAM)
 *    SYSTEM_PRG_RBLOCK              -> SYSTEM_PRG_WBLOCK              (SDRAM CS2)
 *    SYSTEM_DATA_RBLOCK             -> SYSTEM_DATA_WBLOCK             (SDRAM CS2)
 *    NONCACHE_RBLOCK                -> NONCACHE_WBLOCK                (SystemRAM noncache mirror)
 *    SHARED_NONCACHE_BUFFER_RBLOCK  -> SHARED_NONCACHE_BUFFER_WBLOCK  (SystemRAM noncache mirror)
 *
 * After all enabled entries are copied, the Loader jumps to entry_point.
 * App LDR WBLOCKs are offset in BTCM so the Loader can copy them without
 * overwriting its own BTCM code. entry_point remains App system_init so App
 * startup still performs stack, bss, MPU/GIC and runtime initialization before
 * entering main().
 *
 * Keep this struct ABI in lockstep with the matching definition in the Loader's
 * src/hal_entry.c (app_manifest_t).
 *===========================================================================*/

#include <stdint.h>

#pragma section = "LDR_PRG_RBLOCK"
#pragma section = "LDR_PRG_WBLOCK"
#pragma section = "LDR_DATA_RBLOCK"
#pragma section = "LDR_DATA_WBLOCK"
#pragma section = "VECTOR_RBLOCK"
#pragma section = "VECTOR_WBLOCK"
#pragma section = "USER_PRG_RBLOCK"
#pragma section = "USER_PRG_WBLOCK"
#pragma section = "USER_DATA_RBLOCK"
#pragma section = "USER_DATA_WBLOCK"
#pragma section = "SYSTEM_PRG_RBLOCK"
#pragma section = "SYSTEM_PRG_WBLOCK"
#pragma section = "SYSTEM_DATA_RBLOCK"
#pragma section = "SYSTEM_DATA_WBLOCK"
#pragma section = "NONCACHE_RBLOCK"
#pragma section = "NONCACHE_WBLOCK"
#pragma section = "SHARED_NONCACHE_BUFFER_RBLOCK"
#pragma section = "SHARED_NONCACHE_BUFFER_WBLOCK"

#define APP_MANIFEST_MAGIC   0x50415A52u   /* 'RZAP' little-endian */
#define APP_MANIFEST_ENTRIES 9

extern void system_init(void);

typedef struct
{
    uint32_t src;
    uint32_t dst;
    uint32_t size;
    uint32_t flags;          /* bit0 = enable                                 */
} app_manifest_entry_t;

typedef struct
{
    uint32_t              magic;
    uint32_t              entry_count;
    uint32_t              entry_point;   /* address to jump to after copying  */
    uint32_t              reserved;
    app_manifest_entry_t  entries[APP_MANIFEST_ENTRIES];
} app_manifest_t;                        /* 16 + 9*16 = 160 bytes (0xA0)      */

/* The IAR linker resolves __section_begin/__section_size at link time and
 * writes the resulting addresses into this constant initialiser image. */
__root const app_manifest_t g_app_manifest @ ".app_manifest" =
{
    .magic       = APP_MANIFEST_MAGIC,
    .entry_count = APP_MANIFEST_ENTRIES,
    .entry_point = (uint32_t)system_init,
    .reserved    = 0u,
    .entries =
    {
        { /* BTCM copy: App startup/system_init code */
            .src   = (uint32_t)__section_begin("LDR_PRG_RBLOCK"),
            .dst   = (uint32_t)__section_begin("LDR_PRG_WBLOCK"),
            .size  = (uint32_t)__section_size ("LDR_PRG_WBLOCK"),
            .flags = 1u,
        },
        { /* BTCM copy: App startup initialized data */
            .src   = (uint32_t)__section_begin("LDR_DATA_RBLOCK"),
            .dst   = (uint32_t)__section_begin("LDR_DATA_WBLOCK"),
            .size  = (uint32_t)__section_size ("LDR_DATA_WBLOCK"),
            .flags = 1u,
        },
        { /* ATCM copy: vector table */
            .src   = (uint32_t)__section_begin("VECTOR_RBLOCK"),
            .dst   = (uint32_t)__section_begin("VECTOR_WBLOCK"),
            .size  = (uint32_t)__section_size ("VECTOR_WBLOCK"),
            .flags = 1u,
        },
        { /* SystemRAM copy: user/app code */
            .src   = (uint32_t)__section_begin("USER_PRG_RBLOCK"),
            .dst   = (uint32_t)__section_begin("USER_PRG_WBLOCK"),
            .size  = (uint32_t)__section_size ("USER_PRG_WBLOCK"),
            .flags = 1u,
        },
        { /* SystemRAM copy: user/app initialized data */
            .src   = (uint32_t)__section_begin("USER_DATA_RBLOCK"),
            .dst   = (uint32_t)__section_begin("USER_DATA_WBLOCK"),
            .size  = (uint32_t)__section_size ("USER_DATA_WBLOCK"),
            .flags = 1u,
        },
        { /* SDRAM copy: PROFINET (obsd*.o) code */
            .src   = (uint32_t)__section_begin("SYSTEM_PRG_RBLOCK"),
            .dst   = (uint32_t)__section_begin("SYSTEM_PRG_WBLOCK"),
            .size  = (uint32_t)__section_size ("SYSTEM_PRG_WBLOCK"),
            .flags = 1u,
        },
        { /* SDRAM copy: PROFINET (obsd*.o) initialized data */
            .src   = (uint32_t)__section_begin("SYSTEM_DATA_RBLOCK"),
            .dst   = (uint32_t)__section_begin("SYSTEM_DATA_WBLOCK"),
            .size  = (uint32_t)__section_size ("SYSTEM_DATA_WBLOCK"),
            .flags = 1u,
        },
        { /* Noncache initialized data */
            .src   = (uint32_t)__section_begin("NONCACHE_RBLOCK"),
            .dst   = (uint32_t)__section_begin("NONCACHE_WBLOCK"),
            .size  = (uint32_t)__section_size ("NONCACHE_WBLOCK"),
            .flags = 1u,
        },
        { /* Shared noncache initialized data */
            .src   = (uint32_t)__section_begin("SHARED_NONCACHE_BUFFER_RBLOCK"),
            .dst   = (uint32_t)__section_begin("SHARED_NONCACHE_BUFFER_WBLOCK"),
            .size  = (uint32_t)__section_size ("SHARED_NONCACHE_BUFFER_WBLOCK"),
            .flags = 1u,
        },
    },
};
