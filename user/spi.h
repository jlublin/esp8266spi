#ifndef SPI_APP_H
#define SPI_APP_H

#include "spi_register.h"
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"

/* SPI number define */
#define SPI 			0
#define HSPI			1

/* SPI slave mode init */
void spi_slave_init(uint8 spi_no);

/* SPI slave interrupt handler */
void spi_slave_isr_handler(void *param);

void ICACHE_FLASH_ATTR spi_init(void);
void ICACHE_FLASH_ATTR set_miso_test_data();
void ICACHE_FLASH_ATTR disp_spi_data();

#endif

