#include "bsp_api.h"

/* Value for table */
#define TABLE_ENTRY_NUM                  (4)
#define TABLE_ENABLE                     (1)
#define TABLE_DISABLE                    (0)
#define TABLE_INVALID_VALUE              (0xffffffff)

/* loader_table definition */
typedef struct {
    uintptr_t * src;
    uintptr_t * dst;
    uintptr_t size;
    uint32_t enable_flag;
} loader_table;
