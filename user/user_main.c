#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "user_registers.h"
#include "spi.h"

extern void (*__init_array_start)(void);
extern void (*__init_array_end)(void);

LOCAL void ICACHE_FLASH_ATTR
uart1_write_char(char c)
{
	uart_tx_one_char(UART1, c);
}

/* Main init run at boot */
void ICACHE_FLASH_ATTR user_init()
{
	/* Call all C++ constructors */
    void (**p)(void) = &__init_array_end;
    while (p != &__init_array_start)
        (*--p)();

    /* Initialize UARTs */
	uart_rx_intr_enable(UART0);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD);
	uart_init(74880, 115200);
	os_install_putc1((void *)uart1_write_char);

	/* Disable logging to uart */
//	system_set_os_print(0);

	/* Print start message */
	ets_printf("\nStarting SPI test ESP8266++++\n");
	os_printf("\nDebug: Starting SPI test ESP8266++++\n");

    /* Start uart task */
	system_os_task(run_main_task, 1, uart_task_queue, 2);

	/* Start SPI Slave */
	spi_test_init();

	/* Start main timer */

	static volatile os_timer_t epc_main_timer;
	os_timer_setfn(&epc_main_timer, (os_timer_func_t *)run_main_task, NULL);
	os_timer_arm(&epc_main_timer, 1000, 1);

	system_os_post(2, 0, 0);
}
