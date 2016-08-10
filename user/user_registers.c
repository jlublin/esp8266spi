#include "user_registers.h"
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

/* 2 FIFOs (one in, one out) of 4096 bytes each */
uint8 fifo[4096][2] = {{0}};

/* Socket connections */
struct espconn conn[4];

esp_udp conn_udp[4];
esp_tcp conn_tcp[4];

void user_uart_rx_handler(int c)
{
}

/* This is run everytime a CSR register is written from SPI */
void ICACHE_FLASH_ATTR run_main_task(os_event_t *events)
{
	DEBUG_PRINTF("CSR update\n");

	/* Always store old CSR */
	static uint32_t wifi_csr = 0;
	uint32_t new_wifi_csr = esp_registers.r.wifi.csr;

	uint32_t wcsr_update = wifi_csr ^ new_wifi_csr;

	/*** WiFi CSR updates ***/

	/* WiFi enable/disable */
	if(wcsr_update & 0x08)
	{
		if(new_wifi_csr & 0x08)
		{
			DEBUG_PRINTF("Wifi enable\n");
			wifi_enable();
		}
		else
		{
			DEBUG_PRINTF("Wifi disable\n");
			wifi_disable();
		}

		esp_registers.r.wifi.csr ^= 0x08; /* TODO: Is this atomic? */
		wifi_csr ^= 0x08;
	}

	/* DHCP enable/disable */
	if(wcsr_update & 0x02)
	{
		if(new_wifi_csr & 0x02)
		{
			DEBUG_PRINTF("DHCP enable\n");
			dhcp_enable();
		}
		else
		{
			DEBUG_PRINTF("DHCP disable\n");
			dhcp_disable();
		}

		esp_registers.r.wifi.csr ^= 0x02; /* TODO: Is this atomic? */
		wifi_csr ^= 0x02;
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
			{
				DEBUG_PRINTF("Socket enable\n");
				socket_enable(i);
			}
			else
			{
				DEBUG_PRINTF("Socket disable\n");
				socket_disable(i);
			}

			esp_registers.r.sockets[i].csr ^= 0x80; /* TODO: Is this atomic? */
			socket_csr[i] ^= 0x80;
		}
	}
}

void wifi_enable()
{
/*	if(registers.wifi_softap.csr.mode == WIFI_MODE_STA)
	{
	}
	else if(registers.wifi_softap.csr.mode == WIFI_MODE_SOFTAP)
	{
*/		struct softap_config station_conf =
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
		wifi_softap_dhcps_stop();
		esp_registers.r.wifi.csr &= ~(0x01);

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
/*	}
	else if(registers.wifi_softap.csr.mode == WIFI_MODE_STA_AP)
		DEBUG_PRINTF("Wifi STA+SoftAP not supported!\n");
	else
		DEBUG_PRINTF("Wifi mode not set!\n");
*/
}

void wifi_disable()
{
	wifi_set_opmode_current(WIFI_MODE_DISABLE);
	esp_registers.r.wifi.csr &= ~(0x04); /* TODO: Or should this wait for some callback? Atomic? */
}

void dhcp_enable()
{
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
	wifi_softap_dhcps_stop();

	esp_registers.r.wifi.csr &= ~(0x01); /* TODO: Or should this wait for some callback? Atomic? */
}

void tcp_connect_callback(void *arg)
{
	DEBUG_PRINTF("tcp_connect_callback\n");

	/* TODO atomic set ready flag */
}

void tcp_disconnect_callback(void *arg)
{
	DEBUG_PRINTF("tcp_disconnect_callback\n");

	/* TODO atomic clear ready flag */
}

void tcp_error_callback(void *arg, int8_t err)
{
	DEBUG_PRINTF("tcp_error_callback, err:%d\n", err);

	/* TODO atomic set error */
}

void recv_callback(void *arg, char *pdata, unsigned short len)
{
	DEBUG_PRINTF("recv_callback: %s\n", pdata);

	/* TODO add to fifo and atomic set data ready */
}

void socket_enable(int i)
{
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
//		espconn_regist_write_finish(?
		err = espconn_regist_connectcb(&conn[i], &tcp_connect_callback);
		DEBUG_PRINTF("Err1: %x\n", err);
		err = espconn_regist_disconcb(&conn[i], &tcp_disconnect_callback);
		DEBUG_PRINTF("Err2: %x\n", err);
		err = espconn_regist_reconcb(&conn[i], &tcp_error_callback);
		DEBUG_PRINTF("Err3: %x\n", err);
		err = espconn_regist_recvcb(&conn[i], &recv_callback);
		DEBUG_PRINTF("Err4: %x\n", err);
		err = espconn_connect(&conn[i]);
		DEBUG_PRINTF("Err5: %x\n", err);
	}
	/* UDP */
	/* Unknown exactly how these work, maybe needs seperate conn for send and receive? */
}

void socket_disable(int i)
{
}
