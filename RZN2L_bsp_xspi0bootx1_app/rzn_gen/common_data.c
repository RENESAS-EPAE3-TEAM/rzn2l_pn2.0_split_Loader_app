/* generated common source file - do not edit */
#include "common_data.h"
/** IOPORT interface configuration for event link **/
            const ioport_extend_cfg_t  g_ioport_cfg_extend =
            {
                .port_group_output_cfg[IOPORT_PORT_GROUP_1] =
                {
                    .pin_select   = (uint8_t)( IOPORT_EVENT_PIN_SELECTION_NONE),
                    .operation    = IOPORT_EVENT_OUTPUT_OPERATION_LOW
                },
                .port_group_output_cfg[IOPORT_PORT_GROUP_2] =
                {
                    .pin_select   = (uint8_t)( IOPORT_EVENT_PIN_SELECTION_NONE),
                    .operation    = IOPORT_EVENT_OUTPUT_OPERATION_LOW
                },
                .port_group_input_cfg[IOPORT_PORT_GROUP_1] =
                {
                    .event_control = IOPORT_EVENT_CONTROL_DISABLE,
                    .pin_select    = (uint8_t)( IOPORT_EVENT_PIN_SELECTION_NONE),
                    .edge_detection    = IOPORT_EVENT_DETECTION_RISING_EDGE,
                    .overwrite_control = IOPORT_EVENT_CONTROL_DISABLE,
                    .buffer_init_value = IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 7U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 6U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 5U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 4U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 3U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 2U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 1U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW
                },
                .port_group_input_cfg[IOPORT_PORT_GROUP_2] =
                {
                    .event_control = IOPORT_EVENT_CONTROL_DISABLE,
                    .pin_select    = (uint8_t)( IOPORT_EVENT_PIN_SELECTION_NONE),
                    .edge_detection    = IOPORT_EVENT_DETECTION_RISING_EDGE,
                    .overwrite_control = IOPORT_EVENT_CONTROL_DISABLE,
                    .buffer_init_value = IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 7U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 6U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 5U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 4U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 3U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 2U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW << 1U | IOPORT_EVENT_INITIAL_BUFFER_VALUE_LOW
                },
                .single_port_cfg[IOPORT_SINGLE_PORT_0] =
                {
                    .event_control = IOPORT_EVENT_CONTROL_DISABLE,
                    .direction = IOPORT_EVENT_DIRECTION_OUTPUT,
                    .port_num  = (uint16_t)BSP_IO_PORT_16_PIN_0,
                    .operation = IOPORT_EVENT_OUTPUT_OPERATION_LOW,
                    .edge_detection = IOPORT_EVENT_DETECTION_RISING_EDGE
                },
                .single_port_cfg[IOPORT_SINGLE_PORT_1] =
                {
                    .event_control = IOPORT_EVENT_CONTROL_DISABLE,
                    .direction = IOPORT_EVENT_DIRECTION_OUTPUT,
                    .port_num  = (uint16_t)BSP_IO_PORT_16_PIN_0,
                    .operation = IOPORT_EVENT_OUTPUT_OPERATION_LOW,
                    .edge_detection = IOPORT_EVENT_DETECTION_RISING_EDGE
                },
                .single_port_cfg[IOPORT_SINGLE_PORT_2] =
                {
                    .event_control = IOPORT_EVENT_CONTROL_DISABLE,
                    .direction = IOPORT_EVENT_DIRECTION_OUTPUT,
                    .port_num  = (uint16_t)BSP_IO_PORT_16_PIN_0,
                    .operation = IOPORT_EVENT_OUTPUT_OPERATION_LOW,
                    .edge_detection = IOPORT_EVENT_DETECTION_RISING_EDGE
                },
                .single_port_cfg[IOPORT_SINGLE_PORT_3] =
                {
                    .event_control = IOPORT_EVENT_CONTROL_DISABLE,
                    .direction = IOPORT_EVENT_DIRECTION_OUTPUT,
                    .port_num  = (uint16_t)BSP_IO_PORT_16_PIN_0,
                    .operation = IOPORT_EVENT_OUTPUT_OPERATION_LOW,
                    .edge_detection = IOPORT_EVENT_DETECTION_RISING_EDGE
                }
            };

ioport_instance_ctrl_t g_ioport_ctrl;

const ioport_instance_t g_ioport =
        {
            .p_api = &g_ioport_on_ioport,
            .p_ctrl = &g_ioport_ctrl,
            .p_cfg = &g_bsp_pin_cfg,

        };
QueueHandle_t psd_queue;
                #if 1
                StaticQueue_t psd_queue_memory;
                uint8_t psd_queue_queue_memory[1 * 20];
                #endif
                void rtos_startup_err_callback(void * p_instance, void * p_data);
TimerHandle_t plsw_urx_timer;
                #if 1
                StaticTimer_t  plsw_urx_timer_memory;
                #endif
                void rtos_startup_err_callback(void * p_instance, void * p_data);
TimerHandle_t plsw_cmd_timer;
                #if 1
                StaticTimer_t  plsw_cmd_timer_memory;
                #endif
                void rtos_startup_err_callback(void * p_instance, void * p_data);
TimerHandle_t g_resource_monitor_timer;
                #if 0
                StaticTimer_t  g_resource_monitor_timer_memory;
                #endif
                void rtos_startup_err_callback(void * p_instance, void * p_data);
void g_common_init(void) {
psd_queue =
                #if 1
                xQueueCreateStatic(
                #else
                xQueueCreate(
                #endif
                20,
                1
                #if 1
                , &psd_queue_queue_memory[0],
                &psd_queue_memory
                #endif
                );
                if (NULL == psd_queue) {
                rtos_startup_err_callback(psd_queue, 0);
                }
plsw_urx_timer =
                #if 1
                xTimerCreateStatic(
                #else
                xTimerCreate(
                #endif
                "Plsw Urx Timer",
                3,
                pdFALSE,
                NULL,
                plsw_urx_timer_callback
                #if 1
                , &plsw_urx_timer_memory
                #endif
                );
                if (NULL == plsw_urx_timer) {
                rtos_startup_err_callback(plsw_urx_timer, 0);
                }
plsw_cmd_timer =
                #if 1
                xTimerCreateStatic(
                #else
                xTimerCreate(
                #endif
                "Plsw Cmd Timer",
                3,
                pdFALSE,
                NULL,
                plsw_cmd_timer_callback
                #if 1
                , &plsw_cmd_timer_memory
                #endif
                );
                if (NULL == plsw_cmd_timer) {
                rtos_startup_err_callback(plsw_cmd_timer, 0);
                }
g_resource_monitor_timer =
                #if 0
                xTimerCreateStatic(
                #else
                xTimerCreate(
                #endif
                "ResourceMonitor",
                1000,
                pdTRUE,
                NULL,
                g_resource_monitor_timer_callback
                #if 0
                , &g_resource_monitor_timer_memory
                #endif
                );
                if (NULL == g_resource_monitor_timer) {
                rtos_startup_err_callback(g_resource_monitor_timer, 0);
                }
}
