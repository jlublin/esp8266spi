#ifndef __USER_REGISTERS_H__
#define __USER_REGISTERS_H__

#include <stdint.h>
#include "os_type.h"

extern uint32 esp_registers[256];
extern uint8 fifo[4096][2];

extern os_event_t uart_task_queue[2];
void run_main_task(os_event_t *events);

#endif
