#include "rdcp_transport.h"

#ifdef RDCP_TRANSPORT_UART2
#ifndef RDCP_UART_RX
#if defined(ESP32_2432S022C) || defined(ESP32_2432S022N)
/* 2432S022: GPIO16=LCD DC, GPIO17=LCD CS — RDCP uses P1 JST (GPIO3 RX / GPIO1 TX). */
#define RDCP_UART_RX 3
#define RDCP_UART_TX 1
#else
#define RDCP_UART_RX 16
#define RDCP_UART_TX 17
#endif
#endif
#ifndef RDCP_UART_TX
#define RDCP_UART_TX 1
#endif
#ifndef RDCP_UART_BAUD
#define RDCP_UART_BAUD 115200
#endif

static HardwareSerial RdcpSerial(2);

static Stream &rdcp_stream(void)
{
	return RdcpSerial;
}

void rdcp_transport_begin(void)
{
	RdcpSerial.begin(RDCP_UART_BAUD, SERIAL_8N1, RDCP_UART_RX, RDCP_UART_TX);
	RdcpSerial.setTimeout(1000);

	unsigned long clearStart = millis();
	while (RdcpSerial.available() && (millis() - clearStart) < 200) {
		RdcpSerial.read();
		delay(1);
	}
}
#else
static Stream &rdcp_stream(void)
{
	return Serial;
}

void rdcp_transport_begin(void)
{
	/* USB Serial is initialized in setup() before router_app_init(). */
}
#endif

int rdcp_transport_available(void)
{
	return rdcp_stream().available();
}

String rdcp_transport_read_line(void)
{
	return rdcp_stream().readStringUntil('\n');
}

void rdcp_transport_send_line(const char *line)
{
	if (!line)
		return;
	rdcp_stream().println(line);
}
