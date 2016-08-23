#ifndef __USER_REGISTERS_H__
#define __USER_REGISTERS_H__

#include <stdint.h>
#include "os_type.h"

extern os_event_t spi_task_queue[2];
void run_main_task(os_event_t *events);

static const uint32_t SIG_CSR = 0;
static const uint32_t SIG_READ_BUF = 1;
static const uint32_t SIG_WRITE_BUF = 2;

void wifi_enable();
void wifi_disable();

void dhcp_enable();
void dhcp_disable();

void socket_enable(int i);
void socket_disable(int i);
void socket_send_data(int socket);

#define IPv4_ADDR(a,b,c,d) \
				((uint32)((d) & 0xff) << 24) | \
				((uint32)((c) & 0xff) << 16) | \
				((uint32)((b) & 0xff) << 8)  | \
				(uint32)((a) & 0xff)

#define WIFI_MODE_DISABLE	0
#define WIFI_MODE_STA		1
#define WIFI_MODE_SOFTAP	2
#define WIFI_MODE_STA_AP	3

struct wifi
{
	uint32_t ssid[8];
	uint32_t key[16];
	uint32_t ip;
	uint32_t netmask;
	uint32_t gw;
	uint32_t dhcp_least_start;
	uint32_t dhcp_least_end;
	uint32_t cfg;
	uint32_t csr;
	uint32_t reserved;
};

static const int WIFI_CFG_AUTH	= 30;
static const int WIFI_CFG_MODE = 26;
static const int WIFI_CFG_CHANNEL = 24;
static const int WIFI_CFG_APSTA = 23;
static const int WIFI_CFG_SSID_HIDDEN = 22;
static const int WIFI_CFG_MAX_CONNECTIONS = 20;
static const int WIFI_CFG_BEACON_INTERVAL = 0;

struct socket
{
	uint32_t remote_ip;
	uint32_t ports;
	uint32_t cfg;
	uint32_t csr;
};

static const int SOCKET_PORTS_LOCAL = 16;
static const int SOCKET_PORTS_REMOTE = 0;

struct interrupts
{
	uint32_t interrupts;
	uint32_t interrupt_mask;
};

typedef union
{
	struct
	{
		struct wifi wifi;
		struct socket sockets[4];
	} r;

	uint32_t v[32+4*4];
} esp_registers_t;

extern esp_registers_t esp_registers;

/* Simple FIFO structure, start == end => empty, start == end + 1 -> full (mod len) */
struct fifo /* TODO: fifos with 32-bit ints (performance) */
{
	uint8_t data[256];
	uint16_t start;
	uint16_t end;
};

/* Socket FIFOs */
extern struct fifo socket_send_fifo[4];
extern struct fifo socket_recv_fifo[4];

#endif
