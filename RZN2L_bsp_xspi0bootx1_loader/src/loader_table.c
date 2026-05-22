#include "loader_table.h"

#if defined(__ICCARM__)
 #pragma section="APPLICATION_PRG_RBLOCK"
 #pragma section="APPLICATION_PRG_WBLOCK"

 #define application_prg_flash_addr   (__section_begin("APPLICATION_PRG_RBLOCK"))
 #define application_prg_start_addr   (__section_begin("APPLICATION_PRG_WBLOCK"))
 #define application_prg_size         (__section_size ("APPLICATION_PRG_RBLOCK"))

#elif defined(__GNUC__)
extern uint32_t IMAGE_APP_RAM_start;
extern uint32_t IMAGE_APP_FLASH_section_start;
extern uint32_t IMAGE_APP_FLASH_section_size;

 #define application_prg_flash_addr   (&IMAGE_APP_FLASH_section_start)
 #define application_prg_start_addr   (&IMAGE_APP_RAM_start)
 #define application_prg_size         (&IMAGE_APP_FLASH_section_size)

#endif
/*
 * Static fallback table - used ONLY when the App's .app_manifest at
 * 0x60100000 is missing or has an invalid magic. Real copy parameters are
 * carried by the manifest at runtime (see src/hal_entry.c).
 */
const loader_table table[TABLE_ENTRY_NUM] BSP_PLACE_IN_SECTION("CPU0_LOADER_TABLE") =
{
  { (uintptr_t *)application_prg_flash_addr, (uintptr_t *)application_prg_start_addr, (uintptr_t)application_prg_size, (uint32_t)TABLE_ENABLE  },
  { (uintptr_t *)TABLE_INVALID_VALUE,        (uintptr_t *)TABLE_INVALID_VALUE,        (uintptr_t)TABLE_INVALID_VALUE,  (uint32_t)TABLE_DISABLE },
  { (uintptr_t *)TABLE_INVALID_VALUE,        (uintptr_t *)TABLE_INVALID_VALUE,        (uintptr_t)TABLE_INVALID_VALUE,  (uint32_t)TABLE_DISABLE },
  { (uintptr_t *)TABLE_INVALID_VALUE,        (uintptr_t *)TABLE_INVALID_VALUE,        (uintptr_t)TABLE_INVALID_VALUE,  (uint32_t)TABLE_DISABLE }
};
