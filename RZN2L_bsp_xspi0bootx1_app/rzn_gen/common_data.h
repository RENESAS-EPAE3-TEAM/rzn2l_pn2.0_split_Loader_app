/* generated common header file - do not edit */
#ifndef COMMON_DATA_H_
#define COMMON_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "FreeRTOS.h"
                #include "queue.h"
#include "FreeRTOS.h"
                #include "timers.h"
#include "r_ioport.h"
#include "bsp_pin_cfg.h"
FSP_HEADER
#define IOPORT_CFG_NAME g_bsp_pin_cfg
#define IOPORT_CFG_OPEN R_IOPORT_Open
#define IOPORT_CFG_CTRL g_ioport_ctrl

/* IOPORT Instance */
extern const ioport_instance_t g_ioport;

/* IOPORT control structure. */
extern ioport_instance_ctrl_t g_ioport_ctrl;
extern QueueHandle_t psd_queue;
extern TimerHandle_t plsw_urx_timer;
                void plsw_urx_timer_callback( TimerHandle_t xTimer );
extern TimerHandle_t plsw_cmd_timer;
                void plsw_cmd_timer_callback( TimerHandle_t xTimer );
extern TimerHandle_t g_resource_monitor_timer;
                void g_resource_monitor_timer_callback( TimerHandle_t xTimer );
void g_common_init(void);
FSP_FOOTER
#endif /* COMMON_DATA_H_ */
