#include "rdcp_transport.h"

#ifdef RDCP_TRANSPORT_UART2
#include "driver/gpio.h"
#include "driver/uart.h"

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
#ifndef RDCP_USB_MIRROR_RX
#define RDCP_USB_MIRROR_RX 0
#endif

#ifndef RDCP_RX_BUFFER_SIZE
#define RDCP_RX_BUFFER_SIZE 8192
#endif
#ifndef RDCP_TX_BUFFER_SIZE
#if RDCP_USB_MIRROR_RX
#define RDCP_TX_BUFFER_SIZE 8192
#else
#define RDCP_TX_BUFFER_SIZE 2048
#endif
#endif
#ifndef RDCP_LINE_MAX
#define RDCP_LINE_MAX 4096
#endif

static char g_line_buf[RDCP_LINE_MAX];
static size_t g_line_len;

static Stream &rdcp_stream(void)
{
	/* UART2 (Serial2) remapped onto P1 JST (GPIO3 RX / GPIO1 TX).
	 * UART0 (Serial) is not begun — same PCB nets as USB-C CH340.
	 * RDCP_USB_MIRROR_RX copies host→MCU lines onto GPIO1 as `#rx …`. */
	return Serial2;
}

#if RDCP_USB_MIRROR_RX
static void rdcp_mirror_rx_to_usb(const String &line)
{
	if (line.length() == 0 || line.startsWith("#"))
		return;
	Stream &s = rdcp_stream();
	/* "#rx " + line + CRLF; skip rather than stall LVGL if TX is full. */
	const size_t need = 4 + (size_t)line.length() + 2;
	if ((size_t)s.availableForWrite() < need + 64)
		return;
	s.print("#rx ");
	s.println(line);
}
#endif

void rdcp_transport_begin(void)
{
	/* Temporary: leave UART0 unused. Detach ROM console from GPIO1/3
	 * without Serial.begin()/end(), then own the pins with UART2. */
	(void)uart_driver_delete(UART_NUM_0);
	gpio_reset_pin((gpio_num_t)RDCP_UART_RX);
	gpio_reset_pin((gpio_num_t)RDCP_UART_TX);

	Serial2.setRxBufferSize(RDCP_RX_BUFFER_SIZE);
	Serial2.setTxBufferSize(RDCP_TX_BUFFER_SIZE);
	Serial2.begin(RDCP_UART_BAUD, SERIAL_8N1, RDCP_UART_RX, RDCP_UART_TX);
	Serial2.setTimeout(20);
	g_line_len = 0;
	delay(50);

	unsigned long clearStart = millis();
	while (Serial2.available() && (millis() - clearStart) < 200) {
		Serial2.read();
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
#if defined(RDCP_TRANSPORT_UART2) && RDCP_USB_MIRROR_RX
			rdcp_mirror_rx_to_usb(line);
#endif
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
	rdcp_stream().flush();
}

void rdcp_transport_flush(void)
{
	rdcp_stream().flush();
}
