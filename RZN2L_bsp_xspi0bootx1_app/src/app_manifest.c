/*=============================================================================
 * app_manifest.c
 *
 * Application image manifest consumed by the SSBL (Loader) project
 * RZN2L_bsp_xspi0bootx1_loader.
 *
 * The Loader cannot resolve App-side link-time symbols (the two projects are
 * built and linked independently). To bridge this, the App publishes a small
 * manifest struct at a known physical address in xSPI flash
 * (APP_MANIFEST_ADDR = 0x60100000, 0x50 bytes). The Loader's hal_entry()
 * reads it directly via that pointer, then performs the listed memcpy()s and
 * jumps to entry_point.
 *
 * The src/dst/size values are filled in by the IAR linker using the
 * __section_begin / __section_size intrinsics on the super-blocks defined in
 * script/fsp_xspi0_boot_app.icf.
 *
 *    APPLICATION_PRG_RBLOCK  ->  APPLICATION_PRG_WBLOCK   (SRAM @ 0x10000000)
 *    APPLICATION_SDRAM_RBLOCK -> APPLICATION_SDRAM_WBLOCK (SDRAM @ 0x74000000)
 *
 * Keep this struct ABI in lockstep with the matching definition in the Loader's
 * src/hal_entry.c (app_manifest_t).
 *===========================================================================*/

#include <stdint.h>

#pragma section = "APPLICATION_PRG_RBLOCK"
#pragma section = "APPLICATION_PRG_WBLOCK"
#pragma section = "APPLICATION_SDRAM_RBLOCK"
#pragma section = "APPLICATION_SDRAM_WBLOCK"

#define APP_MANIFEST_MAGIC   0x50415A52u   /* 'RZAP' little-endian */
#define APP_MANIFEST_ENTRIES 4

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
} app_manifest_t;                        /* 16 + 4*16 = 80 bytes (0x50)       */

/* The IAR linker resolves __section_begin/__section_size at link time and
 * writes the resulting addresses into this constant initialiser image. */
__root const app_manifest_t g_app_manifest @ ".app_manifest" =
{
    .magic       = APP_MANIFEST_MAGIC,
    .entry_count = 2u,
    .entry_point = 0x10000000u,          /* APPLICATION_PRG_WBLOCK start      */
    .reserved    = 0u,
    .entries =
    {
        { /* SRAM copy: FSP, glue code, vectors, .data, .rodata */
            .src   = (uint32_t)__section_begin("APPLICATION_PRG_RBLOCK"),
            .dst   = (uint32_t)__section_begin("APPLICATION_PRG_WBLOCK"),
            .size  = (uint32_t)__section_size ("APPLICATION_PRG_RBLOCK"),
            .flags = 1u,
        },
        { /* SDRAM copy: PROFINET (obsd*.o) stack code & data */
            .src   = (uint32_t)__section_begin("APPLICATION_SDRAM_RBLOCK"),
            .dst   = (uint32_t)__section_begin("APPLICATION_SDRAM_WBLOCK"),
            .size  = (uint32_t)__section_size ("APPLICATION_SDRAM_RBLOCK"),
            .flags = 1u,
        },
        { 0u, 0u, 0u, 0u },
        { 0u, 0u, 0u, 0u },
    },
};
