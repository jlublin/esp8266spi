#include "spi.h"

/******************************************************************************
 * FunctionName : spi_slave_init
 * Description  : SPI slave mode initial funtion, including mode setting,
 * 			IO setting, transmission interrupt opening, interrupt function registration
 * Parameters   : 	uint8 spi_no - SPI module number, Only "SPI" and "HSPI" are valid
*******************************************************************************/
void spi_slave_init(uint8 spi_no)
{
	uint32 regvalue; 

	if(spi_no > 1)
        return; // handle invalid input number

    // clear bit9, bit8 of reg PERIPHS_IO_MUX
    // bit9 should be cleared when HSPI clock doesn't equal CPU clock
    // bit8 should be cleared when SPI clock doesn't equal CPU clock
    //// WRITE_PERI_REG(PERIPHS_IO_MUX, 0x105); // clear bit9//TEST

    if(spi_no == SPI)
	{
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_CLK_U, 1);	// configure io to spi mode
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_CMD_U, 1);	// configure io to spi mode	
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA0_U, 1);	// configure io to spi mode	
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA1_U, 1);	// configure io to spi mode	
    }
	else if(spi_no == HSPI)
	{
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 2);	// configure io to spi mode
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2);	// configure io to spi mode	
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2);	// configure io to spi mode	
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 2);	// configure io to spi mode	
    }

    // regvalue = READ_PERI_REG(SPI_FLASH_SLAVE(spi_no));
    // slave mode, slave use buffers which are register "SPI_FLASH_C0~C15", enable trans done isr
    // set bit 30 bit 29 bit9, bit9 is trans done isr mask
    SET_PERI_REG_MASK(
		SPI_SLAVE(spi_no),
		SPI_SLAVE_MODE |
#ifdef DEBUG
		SPI_SLV_RD_STA_DONE_EN |
		SPI_SLV_WR_BUF_DONE_EN |
		SPI_SLV_RD_BUF_DONE_EN |
		SPI_SLV_WR_RD_BUF_EN |
		SPI_TRANS_DONE_EN |
#endif
		SPI_SLV_WR_STA_DONE_EN);

    // disable general trans intr 
    // CLEAR_PERI_REG_MASK(SPI_SLAVE(spi_no),SPI_TRANS_DONE_EN);

    CLEAR_PERI_REG_MASK(SPI_USER(spi_no), SPI_FLASH_MODE);		// disable flash operation mode
    SET_PERI_REG_MASK(SPI_USER(spi_no), SPI_USR_MISO_HIGHPART);	// SLAVE SEND DATA BUFFER IN C8-C15 


//////**************RUN WHEN SLAVE RECIEVE*******************///////
   // tow lines below is to configure spi timing.
//    SET_PERI_REG_MASK(SPI_CTRL2(spi_no), (0x2 & SPI_MOSI_DELAY_NUM) << SPI_MOSI_DELAY_NUM_S); // delay num

    WRITE_PERI_REG(SPI_CLOCK(spi_no), 0);
    
/////***************************************************//////	

    // set 8 bit slave command length, because slave must have at least one bit addr,
    // 8 bit slave+8bit addr, so master device first 2 bytes can be regarded as a command 
    // and the  following bytes are datas, 
    // 32 bytes input wil be stored in SPI_FLASH_C0-C7
    // 32 bytes output data should be set to SPI_FLASH_C8-C15
    WRITE_PERI_REG(SPI_USER2(spi_no), (0x7 & SPI_USR_COMMAND_BITLEN) << SPI_USR_COMMAND_BITLEN_S); // 0x70000000

    // set 8 bit slave recieve buffer length, the buffer is SPI_FLASH_C0-C7
    // set 32 bit slave status register
    SET_PERI_REG_MASK(SPI_SLAVE1(spi_no), ((0xff & SPI_SLV_BUF_BITLEN) << SPI_SLV_BUF_BITLEN_S) |
                                          ((0x1f & SPI_SLV_STATUS_BITLEN) << SPI_SLV_STATUS_BITLEN_S) |
                                          ((0x7 & SPI_SLV_WR_ADDR_BITLEN) << SPI_SLV_WR_ADDR_BITLEN_S) |
                                          ((0x7 & SPI_SLV_RD_ADDR_BITLEN) << SPI_SLV_RD_ADDR_BITLEN_S));

	os_printf("SPI_CLOCK is %08x\n", READ_PERI_REG(SPI_CLOCK(spi_no)));
	os_printf("SPI_CTRL is %08x\n", READ_PERI_REG(SPI_CTRL(spi_no)));
	os_printf("SPI_CTRL2 is %08x\n", READ_PERI_REG(SPI_CTRL2(spi_no)));
    os_printf("SPI_USER is 0x%08x\n", READ_PERI_REG(SPI_USER(spi_no)));
    os_printf("SPI_USER1 is 0x%08x\n", READ_PERI_REG(SPI_USER1(spi_no)));
    os_printf("SPI_USER2 is 0x%08x\n", READ_PERI_REG(SPI_USER2(spi_no)));
    os_printf("SPI_SLAVE is 0x%08x\n", READ_PERI_REG(SPI_SLAVE(spi_no)));
    os_printf("SPI_SLAVE1 is 0x%08x\n", READ_PERI_REG(SPI_SLAVE1(spi_no)));
    os_printf("SPI_SLAVE2 is 0x%08x\n", READ_PERI_REG(SPI_SLAVE2(spi_no)));
    os_printf("SPI_SLAVE3 is 0x%08x\n", READ_PERI_REG(SPI_SLAVE3(spi_no)));

    SET_PERI_REG_MASK(SPI_PIN(spi_no), BIT19); //BIT19   

    // maybe enable slave transmission liston 
    SET_PERI_REG_MASK(SPI_CMD(spi_no), SPI_USR);

    // register level2 isr function, which contains spi, hspi and i2s events
    ETS_SPI_INTR_ATTACH(spi_slave_isr_handler, NULL);

    // enable level2 isr, which contains spi, hspi and i2s events
    ETS_SPI_INTR_ENABLE();
}

/******************************************************************************
 * FunctionName : spi_slave_isr_handler
 * Description  : SPI interrupt function, SPI HSPI and I2S interrupt can trig this function
 			   some basic operation like clear isr flag has been done, 
 			   and it is availible	for adding user coder in the funtion
 * Parameters  : void *param- function parameter address, which has been registered in function spi_slave_init
*******************************************************************************/
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "user_registers.h"

static uint8 spi_data[32] = {0};

/*
 * SPI protocol (Only WRSTA available, TODO: why?)
 * 1. (Write argument data into WRBUF)
 * 2. Write command into WRSTA
 * 3. Read status from WRSTA until command complete
 * 4. (Read return data from RDBUF)
 *
 * WRBUF: 32 bytes data
 * RDBUF: 32 bytes data
 *
 * WRSTA: (cmd(4) arg(4), addr/len(8)), (ret, len)
 *
 * WRSTA has a shadow register so that SPI master can't overwrite important
 * data without slave restoring it. ?? Might need to set a busy bit when writing...
 *
 * Commands read/write_fifo can R/W max 64 bytes (6 bits addressing)
 * Registers are accessed as 32 bits values addressed as 0-255.
 *
 * ret is 1 when work in progress, 0 on successfully done, 255 on error (or reset cmd when done?)
 *
 * RESET     | 0x0
 * READ_REG  | 0x1 | - | 0-255 (addr) <-- 0 | 4 | <4 data>
 * WRITE_REG | 0x2 | - | 0-255 (addr) --> <4 data>
 * READ_BUF  | 0x3 | 0-15 (fifo#) | 1-64 <-- 0 | 1-64 | <1-64 data>
 * WRITE_BUF | 0x4 | 0-15 (fifo#) | 1-64 --> <1-64 data>
 * BIT_MODIFY| 0x5 | - | 0-255 (addr) --> <4 set mask> <4 clear mask>
 * STATUS? (Some general status, most importantly interrupt status)
 *
 */

void spi_slave_isr_handler(void *param)
{
	if(READ_PERI_REG(0x3ff00020) & BIT4)
	{
		// following 3 lines is to clear isr signal
		CLEAR_PERI_REG_MASK(SPI_SLAVE(SPI), 0x3ff);
	}
	else if(READ_PERI_REG(0x3ff00020) & BIT7)
	{ //bit7 is for hspi isr,
		uint32 intflags = READ_PERI_REG(SPI_SLAVE(HSPI));

		/*
		 * Clear interrupt enable flags, is this needed?
		 * Works very fine without it
		 */
#if 0
		CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI),
							SPI_TRANS_DONE_EN |
							SPI_SLV_WR_STA_DONE_EN |
							SPI_SLV_RD_STA_DONE_EN |
							SPI_SLV_WR_BUF_DONE_EN |
							SPI_SLV_RD_BUF_DONE_EN);

		/* Synchronous reset of peripheral, is this needed? */
		SET_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SYNC_RESET);
#endif
#if 1
		/*
		 * Clear interrupt flags and then set interrupt enable flags.
		 * TODO: Is set interrupt enable really needed?
		 */
		CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI),
							SPI_TRANS_DONE |
							SPI_SLV_WR_STA_DONE |
							SPI_SLV_RD_STA_DONE |
							SPI_SLV_WR_BUF_DONE |
							SPI_SLV_RD_BUF_DONE);

		SET_PERI_REG_MASK(
			SPI_SLAVE(HSPI),
			SPI_SLV_WR_STA_DONE_EN |
#ifdef DEBUG
			SPI_TRANS_DONE_EN |
			SPI_SLV_RD_STA_DONE_EN |
			SPI_SLV_WR_BUF_DONE_EN |
			SPI_SLV_RD_BUF_DONE_EN |
#endif
			0);
#endif

#ifdef DEBUG
		/* Interrupt due to write to buffer done */
		if(intflags & SPI_SLV_WR_BUF_DONE)
		{
			uint8 idx = 0;
			while(idx < 8)
			{
				uint32 recv_data = READ_PERI_REG(SPI_W0(HSPI) + (idx << 2));

				spi_data[idx << 2] = recv_data & 0xff;
				spi_data[(idx << 2) + 1] = (recv_data >> 8) & 0xff;
				spi_data[(idx << 2) + 2] = (recv_data >> 16) & 0xff;
				spi_data[(idx << 2) + 3] = (recv_data >> 24) & 0xff;
				idx++;
			}

			DEBUG_PRINTF("WR Done\n");
			disp_spi_data();
		}

		/* Interrupt due to read from buffer done */
		if(intflags & SPI_SLV_RD_BUF_DONE)
		{
			DEBUG_PRINTF("RD Done\n");
			DEBUG_PRINTF("Status RD: %p\n", READ_PERI_REG(SPI_RD_STATUS(HSPI)));
			DEBUG_PRINTF("Status WR: %p\n", READ_PERI_REG(SPI_WR_STATUS(HSPI)));
		}

		/* Interrupt due to read from status done */
		if(intflags & SPI_SLV_RD_STA_DONE)
		{
			DEBUG_PRINTF("RD STA Done\n");
			DEBUG_PRINTF("Status RD: %p\n", READ_PERI_REG(SPI_RD_STATUS(HSPI)));
			DEBUG_PRINTF("Status WR: %p\n", READ_PERI_REG(SPI_WR_STATUS(HSPI)));
		}
#endif

		/* Interrupt due to write to status done */
		if(intflags & SPI_SLV_WR_STA_DONE)
		{
			DEBUG_PRINTF("WR STA Done\n");
			DEBUG_PRINTF("Status RD: %p\n", READ_PERI_REG(SPI_RD_STATUS(HSPI)));
			DEBUG_PRINTF("Status WR: %p\n", READ_PERI_REG(SPI_WR_STATUS(HSPI)));

			uint32 val = READ_PERI_REG(SPI_WR_STATUS(HSPI));

			DEBUG_PRINTF("Cmd: %d Arg1: %d Arg2: %d\n",
			          (val >> 28),
			          (val >> 24) & 0xf,
			          (val >> 16) & 0xff);


			if((val >> 28) == 0x01) /* READ_REG */
			{
				uint8 addr = (val >> 16) & 0xff;
				DEBUG_PRINTF("READ_REG(%x): %x\n", addr, esp_registers.v[addr]);

				WRITE_PERI_REG(SPI_W8(HSPI), esp_registers.v[addr]);

				WRITE_PERI_REG(SPI_WR_STATUS(HSPI), 0); /* Clear to tell all done and length */
			}
			else if((val >> 28) == 0x02) /* WRITE_REG */
			{
				uint8 addr = (val >> 16) & 0xff;
				uint32 val = esp_registers.v[addr] = READ_PERI_REG(SPI_W0(HSPI));

				/* TODO, should we stash all csr at register address 0-csr? */
				if(addr == 30) /* WiFi CSR */
				{
					system_os_post(2, 0, SIG_CSR);
				}

				else if(addr == 35) /* Socket[0] CSR */
				{
					system_os_post(2, 0, SIG_CSR);
				}

				DEBUG_PRINTF("WRITE_REG(%x): %x\n", addr, val);

				WRITE_PERI_REG(SPI_WR_STATUS(HSPI), 0); /* Clear to tell all done and length */
			}
			else if((val >> 28) == 0x03) /* READ_BUF */
			{
				system_os_post(2, SIG_READ_BUF, val); /* TODO Separate thread? */
				/* Semaphore, only allow one request at a time? Or what is the effect of multiple requests? */
			}
			else if((val >> 28) == 0x04)
			{
				system_os_post(2, SIG_WRITE_BUF, val); /* TODO Separate thread? */
			}
			else if((val >> 28) == 0x05)
			{
				/* TODO */
			}
		}
	}
	else if(READ_PERI_REG(0x3ff00020) & BIT9)
	{ /* I2S interrupt, should not happen */
	}
}

void ICACHE_FLASH_ATTR set_miso_test_data()
{
	WRITE_PERI_REG(SPI_W0(HSPI), 0xa5a4a3a2);
	WRITE_PERI_REG(SPI_W1(HSPI), 0xa9a8a7a6);
	WRITE_PERI_REG(SPI_W2(HSPI), 0xadacabaa);
	WRITE_PERI_REG(SPI_W3(HSPI), 0xb1b0afae);

	WRITE_PERI_REG(SPI_W4(HSPI), 0xb5b4b3b2);
	WRITE_PERI_REG(SPI_W5(HSPI), 0xb9b8b7b6);
	WRITE_PERI_REG(SPI_W6(HSPI), 0xbdbcbbba);
	WRITE_PERI_REG(SPI_W7(HSPI), 0xc1c0bfbe);

	WRITE_PERI_REG(SPI_W8(HSPI), 0x05040302);
	WRITE_PERI_REG(SPI_W9(HSPI), 0x09080706);
	WRITE_PERI_REG(SPI_W10(HSPI), 0x0d0c0b0a);
	WRITE_PERI_REG(SPI_W11(HSPI), 0x11100f0e);

	WRITE_PERI_REG(SPI_W12(HSPI), 0x15141312);
	WRITE_PERI_REG(SPI_W13(HSPI), 0x19181716);
	WRITE_PERI_REG(SPI_W14(HSPI), 0x1d1c1b1a);
	WRITE_PERI_REG(SPI_W15(HSPI), 0x21201f1e);
}

void ICACHE_FLASH_ATTR disp_spi_data()
{
	uint8 i = 0;
	for(i = 0; i < 8; i++)
	{
		DEBUG_PRINTF("data %d : 0x%02x 0x%02x 0x%02x 0x%02x\n\r", i,
		                                  spi_data[4*i + 0],
		                                  spi_data[4*i + 1],
		                                  spi_data[4*i + 2],
		                                  spi_data[4*i + 3]);
	}
}

void ICACHE_FLASH_ATTR spi_init()
{
	DEBUG_PRINTF("spi init\n\r");
	spi_slave_init(HSPI);

	DEBUG_PRINTF("spi miso init\n\r");
	set_miso_test_data();
	DEBUG_PRINTF("spi init done\n\r");
}
