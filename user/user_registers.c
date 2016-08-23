#include "user_registers.h"
#include "spi.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"
#include "espconn.h"
#include "user_config.h"
#include <stdarg.h>
#include <string.h>

os_event_t spi_task_queue[2];

/* Register storage for wifi module */
esp_registers_t esp_registers;

/* Socket FIFOs */
struct fifo socket_send_fifo[4];
struct fifo socket_recv_fifo[4];

int fifo_get(struct fifo *fifo);
int fifo_put(struct fifo *fifo, uint8_t val);
int fifo_len(struct fifo *fifo);

/* Socket connections */
struct espconn conn[4];
uint8_t conn_sending[4] = {0};

esp_udp conn_udp[4];
esp_tcp conn_tcp[4];

void user_uart_rx_handler(int c)
{
}

/*
 * This is run everytime a CSR register is written from SPI,
 * Should also handle all request regarding changing status on
 * WiFi, DHCP, sockets, etc...
 */
void ICACHE_FLASH_ATTR run_main_task(os_event_t *events)
{
	if(events->sig == SIG_CSR)
	{
		DEBUG_PRINTF("CSR update\n");

		/*** WiFi CSR updates ***/

		/* Always store old CSR */
		static uint32_t wifi_csr = 0;
		uint32_t new_wifi_csr = esp_registers.r.wifi.csr;

		uint32_t wcsr_update = wifi_csr ^ new_wifi_csr;

		/* WiFi enable/disable */
		if(wcsr_update & 0x08)
		{
			if(new_wifi_csr & 0x08)
				wifi_enable();
					/* TODO: if error, reset esp_registers.r.wifi.csr ? Or just error flag? */
			else
				wifi_disable();

			wifi_csr ^= 0x08; /* Set shadow register according to visible register */
		}

		/* DHCP enable/disable */
		if(wcsr_update & 0x02)
		{
			if(new_wifi_csr & 0x02)
				dhcp_enable();
					/* TODO: if error, reset esp_registers.r.wifi.csr ? Or just error flag? */
			else
				dhcp_disable();

			wifi_csr ^= 0x02; /* Set shadow register according to visible register */
		}

		/*** Sockets CSR updates ***/

		/* Always store old CSR */
		static uint32_t socket_csr[4] = {0};

		for(int i = 0; i < 4; i++)
		{
			uint32_t new_socket_csr = esp_registers.r.sockets[i].csr;

			uint32_t scsr_update = socket_csr[i] ^ new_socket_csr;

			/* Socket connect/disconnect */
			if(scsr_update & 0x80)
			{
				if(new_socket_csr & 0x80)
					socket_enable(i);
					/* TODO: if error, reset esp_registers.r.sockets[i].csr ? Or just error flag? */
				else
					socket_disable(i);

				socket_csr[i] ^= 0x80;
			}
		}
	}
	else if(events->sig == SIG_READ_BUF)
	{
		DEBUG_PRINTF("SIG_READ_BUF\n");

		uint32_t wrsta = events->par; /* WRSTA */

		int fifo_i = (wrsta >> 24) & 0xf;
		struct fifo *fifo = &socket_recv_fifo[fifo_i];
		int len = (wrsta >> 16) & 0xff;

		DEBUG_PRINTF("Fifo(R) %d len: %d\n", fifo_i, fifo_len(fifo));
#if 0
		int i, j; /* This implementation has problem with calculating length */
		for(i = 0; i < len/4 && fifo_len(fifo); i++)
		{
			union
			{
				uint32_t v;
				uint8_t b[4];
			} val;

			for(j = 0, val.v = 0; j < 4 && fifo_len(fifo); j++)
				val.b[j] = fifo_get(fifo);

			DEBUG_PRINTF("Value: %x\n", val.v);
			WRITE_PERI_REG(SPI_W8(HSPI) + 4*i, val.v);
		}
#endif
		int i, j;
		union
		{
			uint32_t v;
			uint8_t b[4];
		} val;

		for(i = 0; i < len && fifo_len(fifo); i++)
		{
			j = i & 0x3; /* Byte counter */

			val.b[j] = fifo_get(fifo);

			if(j == 3) /* Last byte */
			{
				DEBUG_PRINTF("Value: %x\n", val.v);
				WRITE_PERI_REG(SPI_W8(HSPI) + i & 0xfffffffc, val.v);
			}

		}

		if(j != 3) /* Last byte not aligned, write now */
		{
			DEBUG_PRINTF("Value: %x\n", val.v);
			WRITE_PERI_REG(SPI_W8(HSPI) + i & 0xfffffffc, val.v);
		}

		WRITE_PERI_REG(SPI_WR_STATUS(HSPI), i); /* TODO: Clear to tell all done and length */
	}
	else if(events->sig == SIG_WRITE_BUF)
	{
		DEBUG_PRINTF("SIG_WRITE_BUF\n");

		uint32_t wrsta = events->par; /* WRSTA */

		int fifo_i = (wrsta >> 24) & 0xf;
		struct fifo *fifo = &socket_send_fifo[fifo_i];
		int len = (wrsta >> 16) & 0xff;

		/* TODO: verify available memory in fifo */

		int i, j;
		for(i = 0; i < len; i++) /* TODO This implementation has problem with calculating length */
		{
			union
			{
				uint32_t v;
				uint8_t b[4];
			} val;

			val.v = READ_PERI_REG(SPI_W0(HSPI) + 4*i);
			DEBUG_PRINTF("Value: %x\n", val.v);

			for(j = 0; j < 4 && (4*i + j) < len; j++)
				fifo_put(fifo, val.b[j]);
		}

		DEBUG_PRINTF("Fifo(W) %d len: %d\n", fifo_i, fifo_len(fifo));

		WRITE_PERI_REG(SPI_WR_STATUS(HSPI), i*4 | (j & 0x3)); /* TODO error in length calculation - Clear to tell all done and length */

		/* Send data */
		if(fifo_i < 4)
		{
			/* Do not send if already sending */
			if(conn_sending[fifo_i] == 0)
				socket_send_data(fifo_i);
		}
	}
}

void wifi_enable()
{
	DEBUG_PRINTF("Wifi enable\n");

	int wifi_mode = (esp_registers.r.wifi.csr >> 23 & 0x1);

	if(wifi_mode == 0)
	{
		struct station_config station_conf =
		{
			.bssid_set = 0
		};

		strncpy(station_conf.ssid, (char*)esp_registers.r.wifi.ssid, 32);
		strncpy(station_conf.password, (char*)esp_registers.r.wifi.key, 64);

		DEBUG_PRINTF("Connecting to SSID: %s with key: %s\n", station_conf.ssid, station_conf.password);

		wifi_set_opmode_current(WIFI_MODE_STA);
		wifi_station_set_config_current(&station_conf);
		wifi_station_connect();

		esp_registers.r.wifi.csr |= 0x04; /* TODO: Or should this wait for some callback? Atomic? */
	}
	else if(wifi_mode == 1)
	{
		struct softap_config station_conf =
		{
			.authmode = AUTH_WPA2_PSK,
			.ssid_hidden = 0,
			.max_connection = 1,
			.beacon_interval = 100
		};

		strncpy(station_conf.ssid, (char*)esp_registers.r.wifi.ssid, 32);
		station_conf.ssid_len = strnlen(station_conf.ssid, 32);
		strncpy(station_conf.password, (char*)esp_registers.r.wifi.key, 64);
		station_conf.channel = 1; /* TODO make configurable */

		DEBUG_PRINTF("Setting up SoftAP with name: %s and key: %s\n", station_conf.ssid, station_conf.password);

		wifi_set_opmode_current(WIFI_MODE_SOFTAP);
		wifi_softap_set_config_current(&station_conf);
		wifi_station_connect();

		/* Disable dhcp server, can't be done in user_init and is required
		 * for setting ip address */
		dhcp_disable();

		/* Set IP address */
		struct ip_info ip =
		{
			.ip = IPv4_ADDR(192, 168, 0, 1),
			.netmask = IPv4_ADDR(255, 255, 255, 0),
			.gw = 0
		};

		if(!wifi_set_ip_info(SOFTAP_IF, &ip))
			DEBUG_PRINTF("wifi_set_ip_info failed\n"); /* TODO handle error */

		esp_registers.r.wifi.csr |= 0x04; /* TODO: Or should this wait for some callback? Atomic? */
	}
}

void wifi_disable()
{
	DEBUG_PRINTF("Wifi disable\n");

	/* TODO: What happens here if WiFi isn't enabled or it was starting up? */
	wifi_set_opmode_current(WIFI_MODE_DISABLE);
	esp_registers.r.wifi.csr &= ~(0x04); /* TODO: Or should this wait for some callback? Atomic? */

	/* Also disable DHCP */
	if(esp_registers.r.wifi.csr & 0x01)
		dhcp_disable();
}

void wifi_event_handler(System_Event_t *event)
{
	if(event->event == EVENT_STAMODE_CONNECTED)
	{
		DEBUG_PRINTF("EVENT_STAMODE_CONNECTED: %s %d\n",
		             event->event_info.connected.ssid,
		             event->event_info.connected.channel);
	}
	else if(event->event == EVENT_STAMODE_DISCONNECTED)
	{
		DEBUG_PRINTF("EVENT_STAMODE_DISCONNECTED: %s %d\n",
		             event->event_info.disconnected.ssid,
		             event->event_info.disconnected.reason);
	}
	else if(event->event == EVENT_STAMODE_AUTHMODE_CHANGE)
	{
		DEBUG_PRINTF("EVENT_STAMODE_AUTHMODE_CHANGE: %d -> %d\n",
		             event->event_info.auth_change.old_mode,
		             event->event_info.auth_change.new_mode);
	}
	else if(event->event == EVENT_STAMODE_GOT_IP)
	{
		DEBUG_PRINTF("EVENT_STAMODE_GOT_IP: " IPSTR ", " IPSTR ", " IPSTR "\n",
		             IP2STR(&event->event_info.got_ip.ip),
		             IP2STR(&event->event_info.got_ip.mask),
		             IP2STR(&event->event_info.got_ip.gw));
	}
	else if(event->event == EVENT_SOFTAPMODE_STACONNECTED)
	{
		DEBUG_PRINTF("EVENT_SOFTAPMODE_STACONNECTED: \n");
	}
	else if(event->event == EVENT_SOFTAPMODE_STADISCONNECTED)
	{
		DEBUG_PRINTF("EVENT_SOFTAPMODE_STADISCONNECTED: \n");
	}
	else
	{
		DEBUG_PRINTF("Unknown WiFi event\n");
	}
}

void dhcp_enable()
{
	DEBUG_PRINTF("DHCP enable\n");

	struct dhcps_lease lease =
	{
		.start_ip = IPv4_ADDR(192, 168, 0, 10),
		.end_ip = IPv4_ADDR(192, 168, 0, 100)
	};

	if(!wifi_softap_set_dhcps_lease(&lease))
		DEBUG_PRINTF("wifi_softap_set_dhcps_lease failed\n"); /* TODO Error handling */

	wifi_softap_dhcps_start();

	esp_registers.r.wifi.csr |= 0x01; /* TODO: Or should this wait for some callback? Atomic? */
}

void dhcp_disable()
{
	/* TODO: What happens if dhcp isn't enabled or it was starting up? */
	DEBUG_PRINTF("DHCP disable\n");

	wifi_softap_dhcps_stop();

	esp_registers.r.wifi.csr &= ~(0x01); /* TODO: Or should this wait for some callback? Atomic? */
}

int tcp_get_conn_index(void *arg)
{
	struct espconn *conn = (struct espconn*) arg;

	for(int i = 0; i < 4; i++)
		if(memcmp(conn_tcp[i].remote_ip, conn->proto.tcp->remote_ip, 4) == 0 &&
		   conn_tcp[i].remote_port == conn->proto.tcp->remote_port)
			return i;

	DEBUG_PRINTF("ERROR: Couldn't find connection\n");
	DEBUG_PRINTF("Conn: %d.%d.%d.%d:%d\n",
	             conn->proto.tcp->remote_ip[0],
	             conn->proto.tcp->remote_ip[1],
	             conn->proto.tcp->remote_ip[2],
	             conn->proto.tcp->remote_ip[3],
	             conn->proto.tcp->remote_port);

	return -1;
}

void tcp_connect_callback(void *arg)
{
	DEBUG_PRINTF("tcp_connect_callback\n");

	int i = tcp_get_conn_index(arg);

	if(i < 0)
		return; /* Error */

	esp_registers.r.sockets[i].csr |= 0x80 | 0x40; /* TODO: Atomic? */
}

void tcp_disconnect_callback(void *arg)
{
	/* TODO: What happens if socket isn't enabled or it was connecting? */
	DEBUG_PRINTF("tcp_disconnect_callback\n");

	int i = tcp_get_conn_index(arg);

	if(i < 0)
		return; /* Error */

	esp_registers.r.sockets[i].csr &= ~(0x80 | 0x40); /* TODO: Atomic? */
}

void tcp_error_callback(void *arg, int8_t err)
{
	DEBUG_PRINTF("tcp_error_callback, err:%d\n", err);

	int i = tcp_get_conn_index(arg);

	if(i < 0)
		return; /* Error */

	/* Set error 1 for general error TODO: spec different errors */
	/* TODO: needs to call esp_disconnect also? from user thread then too... */

	esp_registers.r.sockets[i].csr &= ~(0x80 | 0x40 | (1 << 2)); /* TODO: Atomic? */
}

void recv_callback(void *arg, char *pdata, unsigned short len)
{
	DEBUG_PRINTF("recv_callback: %s\n", pdata);

	int i = tcp_get_conn_index(arg);

	if(i < 0)
		return; /* Error */

	for(int j = 0; j < len; j++)
		fifo_put(&socket_recv_fifo[i], pdata[j]);

	/* TODO add to fifo and atomic set data ready */
}

void sent_callback(void *arg)
{
	DEBUG_PRINTF("sent_callback\n");

	int i = tcp_get_conn_index(arg);

	if(i < 0)
		return;

	if(fifo_len(&socket_send_fifo[i]) != 0)
		socket_send_data(i);
	else
		conn_sending[i] = 0;
}

void socket_enable(int i)
{
	DEBUG_PRINTF("Socket %d enable\n", i);

	memset(&conn[i], 0, sizeof(struct espconn));

	/* TCP */
	if(esp_registers.r.sockets[i].cfg & 0x02)
	{
		memset(&conn_tcp[i], 0, sizeof(esp_tcp));

		conn[i].type = ESPCONN_TCP;
		conn[i].state = ESPCONN_NONE;
		conn[i].proto.tcp = &conn_tcp[i];

		conn_tcp[i].remote_ip[0] = esp_registers.r.sockets[i].remote_ip >> 24;
		conn_tcp[i].remote_ip[1] = esp_registers.r.sockets[i].remote_ip >> 16;
		conn_tcp[i].remote_ip[2] = esp_registers.r.sockets[i].remote_ip >> 8;
		conn_tcp[i].remote_ip[3] = esp_registers.r.sockets[i].remote_ip >> 0;

		conn_tcp[i].remote_port = esp_registers.r.sockets[i].ports & 0xffff;

		uint16_t local_port = esp_registers.r.sockets[i].ports >> 16;

		if(local_port == 0)
			conn_tcp[i].local_port = espconn_port();
		else
			conn_tcp[i].local_port = local_port;

		DEBUG_PRINTF("Connecting TCP (0.0.0.0:%d -> %d.%d.%d.%d:%d)\n",
		             conn_tcp[i].local_port,
		             conn_tcp[i].remote_ip[0],
		             conn_tcp[i].remote_ip[1],
		             conn_tcp[i].remote_ip[2],
		             conn_tcp[i].remote_ip[3],
		             conn_tcp[i].remote_port);

		int err;
		err = espconn_regist_connectcb(&conn[i], &tcp_connect_callback);
		DEBUG_PRINTF("Err1: %x\n", err);
		err = espconn_regist_disconcb(&conn[i], &tcp_disconnect_callback);
		DEBUG_PRINTF("Err2: %x\n", err);
		err = espconn_regist_reconcb(&conn[i], &tcp_error_callback);
		DEBUG_PRINTF("Err3: %x\n", err);
		err = espconn_regist_recvcb(&conn[i], &recv_callback);
		DEBUG_PRINTF("Err4: %x\n", err);
		espconn_regist_sentcb(&conn[i], &sent_callback); /* TODO: should probably use espconn_regist_write_finish */
		DEBUG_PRINTF("Err5: %x\n", err);
		err = espconn_connect(&conn[i]);
		DEBUG_PRINTF("Err6: %x\n", err);
	}
	/* UDP */
	/* Unknown exactly how these work, maybe needs seperate conn for send and receive? */
}

void socket_disable(int i)
{
	DEBUG_PRINTF("Socket %d disable\n", i);

	espconn_disconnect(&conn[i]);
}

void socket_send_data(int socket_i)
{
	uint8_t buf[256];

	conn_sending[socket_i] = 1;
	uint16_t len = fifo_len(&socket_send_fifo[socket_i]);

	len = (len > 256)? 256 : len;

	for(int i = 0; i < len; i++)
		buf[i] = fifo_get(&socket_send_fifo[socket_i]);

	espconn_send(&conn[socket_i], buf, len);
}

void fifo_reset(struct fifo *fifo)
{
	fifo->start = 0;
	fifo->end = 0;
}

int fifo_get(struct fifo *fifo)
{
	if(fifo->start == fifo->end)
		return -1; /* FIFO empty */

	int val = fifo->data[fifo->start];

	fifo->start += 1;
	if(fifo->start >= 256)
		fifo->start = 0;

	return val;
}

int fifo_put(struct fifo *fifo, uint8_t val)
{
	if(fifo->end + 1 == fifo->start || fifo->end + 1 == fifo->start + 256)
		return -1; /* FIFO full */

	fifo->data[fifo->end] = val;

	fifo->end += 1;
	if(fifo->end >= 256)
		fifo->end = 0;

	return 0;
}

int fifo_len(struct fifo *fifo)
{
	int len = fifo->end - fifo->start;

	if(len < 0)
		len += 256;

	return len;
}
