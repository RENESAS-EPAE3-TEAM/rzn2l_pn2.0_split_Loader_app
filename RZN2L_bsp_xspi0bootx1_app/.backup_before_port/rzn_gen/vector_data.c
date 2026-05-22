/* generated vector source file - do not edit */
        #include "bsp_api.h"
        /* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
        #if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_MAX_ENTRIES] =
        {
                        [0] = intcpu0_handler, /* INTCPU0 (Software interrupt 0) */
        };
        #if (1 == BSP_FEATURE_BSP_IRQ_CR52_SEL_SUPPORTED)
        /* If the product has an Interrupts Select Register and the selected interrupt is assigned to
         * the Interrupts Select Register, different from the number specified by Event number,
         * the numbers within the range assigned to the Interrupts Select Register will be assigned in order.
         * Products that do not have an Interrupts Select Register and interrupts that cannot be assigned to
         * the Interrupts Select Register are assigned the number specified by Event number. */
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_MAX_ENTRIES] =
        {
            [0] = BSP_PRV_CR52_SEL_ENUM(EVENT_INTCPU0), /* INTCPU0 (Software interrupt 0) */
        };
        #endif
        #endif