uint32_t read_buf32()
{
	uint32_t val = 0;

	/* Read data from buf */
	CS.set(0);

	SPI_1.transfer(3 << 0); /* 3 = Read BUF */
	SPI_1.transfer(0x00); /* Addr */

	val |= SPI_1.transfer(0) << 0;
	val |= SPI_1.transfer(0) << 8;
	val |= SPI_1.transfer(0) << 16;
	val |= SPI_1.transfer(0) << 24;

	CS.set(1);

	return val;
};

uint32_t read_buf(uint8_t data[], uint8_t len)
{
	uint32_t val = 0;

	/* Read data from buf */
	CS.set(0);

	SPI_1.transfer(3 << 0); /* 3 = Read BUF */
	SPI_1.transfer(0x00); /* Addr */

	for(int i = 0; i < len; i++)
		data[i] = SPI_1.transfer(0);

	CS.set(1);

	return val;
};

void write_buf32(uint32_t data)
{
	/* Write data to buf */
	CS.set(0);

	SPI_1.transfer(2 << 0); /* 2 = Write BUF */
	SPI_1.transfer(0x00); /* Addr */

	SPI_1.transfer(data >> 0);
	SPI_1.transfer(data >> 8);
	SPI_1.transfer(data >> 16);
	SPI_1.transfer(data >> 24);

	CS.set(1);

};

void write_buf(const uint8_t data[], uint8_t len)
{
	/* Write data to buf */
	CS.set(0);
	delay(1);

	SPI_1.transfer(2 << 0); /* 2 = Write BUF */
	SPI_1.transfer(0x00); /* Addr */

	for(int i = 0; i < len; i++)
		SPI_1.transfer(data[i]);

	CS.set(1);

};

uint32_t read_sta()
{
	uint32_t val = 0;

	/* Read data from status */
	CS.set(0);

	SPI_1.transfer(4 << 0); /* 4 = Read status */

	val |= SPI_1.transfer(0) << 0;
	val |= SPI_1.transfer(0) << 8;
	val |= SPI_1.transfer(0) << 16;
	val |= SPI_1.transfer(0) << 24;

	CS.set(1);

	return val;
};

void write_sta(uint32_t data)
{
	/* Read data from status */
	CS.set(0);

	SPI_1.transfer(1 << 0); /* 1 = Write status */

	SPI_1.transfer(data >> 0);
	SPI_1.transfer(data >> 8);
	SPI_1.transfer(data >> 16);
	SPI_1.transfer(data >> 24);

	CS.set(1);
};

uint32_t read_reg(uint8_t addr)
{
	write_sta((0x1 << 28) |
	          (addr << 16) |
	          (0x01 << 8));

	while(read_sta()) { delay(100); }
	uint32_t val = read_buf32();

	return val;
}

void write_reg(uint8_t addr, uint32_t data)
{
	write_buf32(data);

	write_sta((0x2 << 28) |
	          (addr << 16) |
	          (0x01 << 8));

	while(read_sta()) { delay(100); }
}

int read_fifo(uint8_t fifo, uint8_t data[], uint8_t len)
{
	/* Only accepts max 32 bytes for now */
	if(len > 32)
		return -1;

	write_sta((0x3 << 28) |
	          ((fifo & 0xf) << 24) |
	          (len << 16) |
	          (0x01 << 8));

	uint32_t ret;

	while((ret = read_sta()) & 0xff00) { delay(100); }

	len = ret & 0xff;

	if(len > 32)
		return -1;

	read_buf(data, len);

	return len;
}

int write_fifo(uint8_t fifo, const uint8_t data[], uint8_t len)
{
	/* Only accepts max 32 bytes for now */
	if(len > 32)
		return -1;

	write_buf(data, len);

	write_sta((0x4 << 28) |
	          ((fifo & 0xf) << 24) |
	          (len << 16) |
	          (0x01 << 8));

	uint32_t ret;

	while((ret = read_sta()) & 0xff00) { delay(100); }

	return ret & 0xff;
}

void set_ssid(const char *ssid)
{
	union
	{
		char str[32];
		uint32_t v[8];
	} SSID = {0};

	strncpy(SSID.str, ssid, 32);

	for(int i = 0; i < 8; i++)
		write_reg(i, SSID.v[i]);
}

void set_key(const char *key)
{
	union
	{
		char str[64];
		uint32_t v[16];
	} outkey = {0};

	strncpy(outkey.str, key, 64);

	for(int i = 0; i < 16; i++)
		write_reg(8 + i, outkey.v[i]);
}

void enable_wifi(int sta)
{
	if(sta == 0)
		write_reg(30, 0x08);
	else
		write_reg(30, (1 << 23) | 0x08);

	while(!(read_reg(30) & 0x04)) { delay(100); } /* Wait for WiFi enabled */
}

void disable_wifi()
{
	uint32_t csr = read_reg(30);
	write_reg(30,  csr & ~(0x08));

	while((read_reg(30) & 0x04)) { delay(100); } /* Wait for WiFi enabled */
}

void enable_dhcp()
{
	write_reg(30, 0x08 | 0x02);

	while(!(read_reg(30) & 0x01)) { delay(100); } /* Wait for DHCP enabled */
}

void disable_dhcp()
{
	uint32_t csr = read_reg(30);
	write_reg(30,  csr & ~(0x01));

	while((read_reg(30) & 0x01)) { delay(100); } /* Wait for DHCP enabled */
}

void connect_tcp(uint32_t ip, uint16_t port)
{
#define IPv4_ADDR(x1,x2,x3,x4) ((x1) << 24 | (x2) << 16 | (x3) << 8 | (x4) << 0)
	/* Setup socket 0 */
	write_reg(32, ip);

	/* Port: 8000 */
	write_reg(33, port);

	/* TCP */
	write_reg(34, 0x02);

	/* Connect socket */
	write_reg(35, 0x80); /* TODO: wrong bits */
}

void disconnect_tcp()
{
	uint32_t csr = read_reg(35);
	write_reg(35,  csr & ~(0x80));

	while((read_reg(35) & 0x40)) { delay(100); } /* Wait for socket disabled */
}
