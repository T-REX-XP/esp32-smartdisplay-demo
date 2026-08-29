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

#ifndef RDCP_RX_BUFFER_SIZE
#define RDCP_RX_BUFFER_SIZE 8192
#endif
#ifndef RDCP_TX_BUFFER_SIZE
#define RDCP_TX_BUFFER_SIZE 2048
#endif
#ifndef RDCP_LINE_MAX
#define RDCP_LINE_MAX 4096
#endif

static HardwareSerial RdcpSerial(2);
static char g_line_buf[RDCP_LINE_MAX];
static size_t g_line_len;

static Stream &rdcp_stream(void)
{
	return RdcpSerial;
}

void rdcp_transport_begin(void)
{
	/*
	 * Buffer sizes must be set BEFORE begin() or ESP32 Arduino ignores them
	 * (default RX stays 256 B and host startup floods drop screen/nav cmds).
	 */
	RdcpSerial.setRxBufferSize(RDCP_RX_BUFFER_SIZE);
	RdcpSerial.setTxBufferSize(RDCP_TX_BUFFER_SIZE);
	RdcpSerial.begin(RDCP_UART_BAUD, SERIAL_8N1, RDCP_UART_RX, RDCP_UART_TX);
	RdcpSerial.setTimeout(20);
	g_line_len = 0;

	unsigned long clearStart = millis();
	while (RdcpSerial.available() && (millis() - clearStart) < 200) {
		RdcpSerial.read();
		delay(1);
	}
}
#else
#ifndef RDCP_LINE_MAX
#define RDCP_LINE_MAX 4096
#endif

static char g_line_buf[RDCP_LINE_MAX];
static size_t g_line_len;

static Stream &rdcp_stream(void)
{
	return Serial;
}

void rdcp_transport_begin(void)
{
	/* USB Serial is initialized in setup() before router_app_init(). */
	g_line_len = 0;
}
#endif

int rdcp_transport_available(void)
{
	return rdcp_stream().available();
}

String rdcp_transport_read_line(void)
{
	Stream &s = rdcp_stream();

	while (s.available()) {
		int c = s.read();
		if (c < 0)
			break;
		if (c == '\r')
			continue;
		if (c == '\n') {
			g_line_buf[g_line_len] = '\0';
			String line(g_line_buf);
			g_line_len = 0;
			line.trim();
			return line;
		}
		if (g_line_len + 1 < sizeof(g_line_buf))
			g_line_buf[g_line_len++] = (char)c;
		else
			g_line_len = 0; /* overflow — resync on next newline */
	}
	return String();
}

void rdcp_transport_send_line(const char *line)
{
	if (!line)
		return;
	rdcp_stream().println(line);
}
