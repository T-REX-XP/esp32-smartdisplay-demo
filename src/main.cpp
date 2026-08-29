/**
 * UART debug stub for mcudd link testing (keep it simple).
 *
 * Shows each newline-terminated UART line on the LCD, ACKs with echo JSON,
 * and heartbeats every 2s so the host log proves ESP TX is alive.
 *
 * IMPORTANT hardware:
 * - CM5 J3 is GND/RX/TX only — no power. Panel needs USB or battery.
 * - USB-C serial shares GPIO1/3 with the JST. For JST↔mcudd, unplug USB-C
 *   and run on battery (or external 5V), otherwise CH340 fights CM5 TX.
 *
 * Uses UART0 (Serial) on GPIO3 RX / GPIO1 TX — same pins as the USB adapter.
 * Full UI backup: src/main.cpp.bak-full-ui
 */

#include <Arduino.h>
#include <esp32_smartdisplay.h>

static lv_obj_t *title_lbl;
static lv_obj_t *rx_lbl;
static uint32_t rx_count;
static String line_buf;
static auto lv_last_tick = millis();
static auto last_hb = millis();

static void show_rx(const String &line)
{
	rx_count++;
	String text = "#" + String(rx_count) + "\n" + line;
	if (text.length() > 280)
		text = text.substring(0, 280) + "...";
	lv_label_set_text(rx_lbl, text.c_str());

	Serial.printf("{\"v\":1,\"t\":\"evt\",\"op\":\"echo\",\"data\":{\"text\":\"rx#%lu\"}}\n",
		      (unsigned long)rx_count);
}

void setup()
{
	Serial.begin(115200);
	Serial.setRxBufferSize(4096);
	Serial.setTimeout(20);
	delay(200);
	while (Serial.available())
		(void)Serial.read();

	smartdisplay_init();
	lv_obj_t *scr = lv_scr_act();
	lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

	title_lbl = lv_label_create(scr);
	lv_label_set_text(title_lbl, "mcudd UART debug\nUART0 GPIO3/1 @115200\nunplug USB; use battery");
	lv_obj_set_style_text_color(title_lbl, lv_color_hex(0x7ec8e3), 0);
	lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 4);

	rx_lbl = lv_label_create(scr);
	lv_label_set_long_mode(rx_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_width(rx_lbl, lv_pct(92));
	lv_obj_set_style_text_color(rx_lbl, lv_color_hex(0xe8e8e8), 0);
	lv_obj_align(rx_lbl, LV_ALIGN_TOP_LEFT, 8, 56);
	lv_label_set_text(rx_lbl, "waiting for lines...");

	Serial.println("{\"v\":1,\"t\":\"evt\",\"op\":\"echo\",\"data\":{\"text\":\"debug-ready\"}}");
}

void loop()
{
	while (Serial.available()) {
		char c = (char)Serial.read();
		if (c == '\n' || c == '\r') {
			if (line_buf.length() > 0) {
				show_rx(line_buf);
				line_buf = "";
			}
		} else {
			line_buf += c;
			if (line_buf.length() > 1024)
				line_buf = "";
		}
	}

	auto const now = millis();
	if (now - last_hb >= 2000) {
		last_hb = now;
		Serial.printf("{\"v\":1,\"t\":\"evt\",\"op\":\"echo\",\"data\":{\"text\":\"hb\"}}\n");
	}

	lv_tick_inc(now - lv_last_tick);
	lv_last_tick = now;
	lv_timer_handler();
	delay(2);
}

extern "C" void action_on_rotate(lv_event_t *e)
{
	(void)e;
}
