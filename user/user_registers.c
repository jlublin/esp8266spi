#include "user_registers.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"
#include <stdarg.h>

os_event_t uart_task_queue[2];

/* Register storage for wifi module */
uint32 esp_registers[256] = {0};

/* 2 FIFOs (one in, one out) of 4096 bytes each */
uint8 fifo[4096][2] = {{0}};

void user_uart_rx_handler(int c)
{
}

void ICACHE_FLASH_ATTR run_main_task(os_event_t *events)
{
	os_printf("Ping\n");
}
