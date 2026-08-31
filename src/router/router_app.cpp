#include "router_app.h"

#include "rdcp_transport.h"
#include "router_pages.h"
#include "router_ui.h"
#include "mcud_version.h"
#include "../app/router_engine.h"

#include <Arduino.h>
#include <string.h>

#define ROUTER_SWIPE_MIN_PX 40

#ifndef ROUTER_BTN_BOOT_GPIO
#ifdef ROUTER_BTN_SW1_GPIO
#define ROUTER_BTN_BOOT_GPIO ROUTER_BTN_SW1_GPIO
#else
#define ROUTER_BTN_BOOT_GPIO 0
#endif
#endif
#ifndef ROUTER_BTN_DEBOUNCE_MS
#define ROUTER_BTN_DEBOUNCE_MS 50
#endif
#ifndef ROUTER_BTN_NAV_MAX_MS
#define ROUTER_BTN_NAV_MAX_MS 900
#endif
#ifndef ROUTER_BTN_POWEROFF_START_MS
#define ROUTER_BTN_POWEROFF_START_MS 900
#endif
#ifndef ROUTER_BTN_POWEROFF_MS
#define ROUTER_BTN_POWEROFF_MS 5000
#endif
#ifndef ROUTER_LINK_TIMEOUT_MS
#define ROUTER_LINK_TIMEOUT_MS 5000
#endif

static router_ui_t *g_ui;
static router_metrics_t g_metrics;
static router_engine_t g_eng;
static lv_point_t g_swipe_start;
static bool g_swipe_active;

static bool g_btn_ready;
static bool g_btn_down;
static bool g_btn_poweroff_sent;
static unsigned long g_btn_down_ms;
static unsigned g_btn_last_sec;

static void hw_tx(const char *line, void *ctx)
{
	(void)ctx;
	rdcp_transport_send_line(line);
}

static void hw_show_boot(void *ctx)
{
	(void)ctx;
	router_ui_show_boot(g_ui);
}

static void hw_show_page(int page, void *ctx)
{
	(void)ctx;
	if (page < 0 || page >= ROUTER_PAGE_COUNT)
		return;
	router_ui_show_page(g_ui, (router_page_t)page, LV_SCR_LOAD_ANIM_FADE_ON);
}

static void hw_boot_status(const char *text, unsigned pct, void *ctx)
{
	(void)ctx;
	router_ui_set_boot_status(g_ui, text, pct);
}

static void hw_apply_metrics(const char *json, void *ctx)
{
	(void)ctx;
	if (!json)
		return;
	router_data_apply_json(&g_metrics, json);
	g_metrics.last_rx_ms = millis();
	g_metrics.link_ok = true;
	if (g_ui && !router_ui_on_boot(g_ui) && !router_ui_poweroff_active(g_ui))
		router_ui_refresh(g_ui, &g_metrics);
}

static unsigned hw_now_ms(void *ctx)
{
	(void)ctx;
	return (unsigned)millis();
}

static bool boot_btn_pressed(void)
{
	return digitalRead(ROUTER_BTN_BOOT_GPIO) == LOW;
}

static void router_app_init_button(void)
{
	pinMode(ROUTER_BTN_BOOT_GPIO, INPUT_PULLUP);
	g_btn_ready = true;
	g_btn_down = boot_btn_pressed();
	g_btn_down_ms = millis();
	g_btn_poweroff_sent = false;
	g_btn_last_sec = 0;
}

void router_app_poll_button(void)
{
	unsigned long now;
	unsigned long held;
	unsigned sec_left;
	bool pressed;
	static unsigned long last_check;

	if (!g_btn_ready)
		router_app_init_button();

	now = millis();
	if (now - last_check < (unsigned long)ROUTER_BTN_DEBOUNCE_MS)
		return;
	last_check = now;

	pressed = boot_btn_pressed();

	if (pressed && !g_btn_down) {
		g_btn_down = true;
		g_btn_down_ms = now;
		g_btn_poweroff_sent = false;
		g_btn_last_sec = 0;
	}

	if (!pressed && g_btn_down) {
		held = now - g_btn_down_ms;
		if (router_ui_poweroff_active(g_ui))
			router_ui_poweroff_hide(g_ui);
		else if (held < (unsigned long)ROUTER_BTN_NAV_MAX_MS)
			router_engine_on_input(&g_eng, "left");
		g_btn_down = false;
		g_btn_last_sec = 0;
		return;
	}

	if (!pressed || g_btn_poweroff_sent)
		return;

	held = now - g_btn_down_ms;
	if (held < (unsigned long)ROUTER_BTN_POWEROFF_START_MS)
		return;

	sec_left = ((unsigned long)ROUTER_BTN_POWEROFF_MS - held + 999UL) / 1000UL;
	if (sec_left < 1)
		sec_left = 1;
	if (sec_left > (unsigned)ROUTER_BTN_POWEROFF_MS / 1000U)
		sec_left = (unsigned)ROUTER_BTN_POWEROFF_MS / 1000U;

	if (sec_left != g_btn_last_sec) {
		g_btn_last_sec = sec_left;
		router_ui_poweroff_show(g_ui, sec_left);
	}

	if (held >= (unsigned long)ROUTER_BTN_POWEROFF_MS) {
		router_ui_poweroff_shutting_down(g_ui);
		router_engine_emit_poweroff(&g_eng);
		g_btn_poweroff_sent = true;
	}
}

static void on_swipe_touch(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	if (code == LV_EVENT_PRESSED) {
		lv_indev_get_point(lv_indev_active(), &g_swipe_start);
		g_swipe_active = true;
		return;
	}

	if (code != LV_EVENT_RELEASED || !g_swipe_active)
		return;

	g_swipe_active = false;

	lv_point_t end;
	lv_indev_get_point(lv_indev_active(), &end);

	lv_coord_t dx = end.x - g_swipe_start.x;
	lv_coord_t dy = end.y - g_swipe_start.y;

	if (abs((int)dx) < ROUTER_SWIPE_MIN_PX)
		return;
	if (abs((int)dx) < abs((int)dy))
		return;

	router_engine_on_input(&g_eng, dx < 0 ? "left" : "right");
}

static void attach_swipe(lv_obj_t *scr)
{
	router_ui_install_swipe(scr, on_swipe_touch);
}

void router_app_init(void)
{
	router_hooks_t hooks;

	router_data_init(&g_metrics);
	g_ui = router_ui_create();

	memset(&hooks, 0, sizeof(hooks));
	hooks.tx = hw_tx;
	hooks.show_boot = hw_show_boot;
	hooks.show_page = hw_show_page;
	hooks.set_boot_status = hw_boot_status;
	hooks.apply_metrics = hw_apply_metrics;
	hooks.now_ms = hw_now_ms;
	hooks.stack = MCUD_STACK_VERSION;
	hooks.release = (unsigned)MCUD_STACK_RELEASE;
	hooks.component = MCUD_COMPONENT_FIRMWARE;
	hooks.rdcp = (unsigned)MCUD_RDCP_VERSION;

	router_engine_init(&g_eng, &hooks);

	attach_swipe(router_ui_boot_screen(g_ui));
	for (int i = 0; i < ROUTER_PAGE_COUNT; i++)
		attach_swipe(router_ui_screen(g_ui, (router_page_t)i));

	router_app_init_button();
}

void router_app_on_serial_line(const char *line)
{
	router_engine_on_line(&g_eng, line);
}

void router_app_loop(void)
{
	bool ok;
	static unsigned long last_uart_rebegin;

	router_engine_tick(&g_eng);
	ok = router_engine_link_ok(&g_eng, ROUTER_LINK_TIMEOUT_MS);
	if (!ok) {
		unsigned long now = millis();

		if (last_uart_rebegin == 0)
			last_uart_rebegin = now;
		else if (now - last_uart_rebegin > 5000UL) {
			rdcp_transport_begin();
			last_uart_rebegin = now;
		}
	} else {
		last_uart_rebegin = millis();
	}
	if (g_metrics.link_ok != ok) {
		g_metrics.link_ok = ok;
		if (g_ui && !router_ui_on_boot(g_ui) && !router_ui_poweroff_active(g_ui))
			router_ui_refresh(g_ui, &g_metrics);
	}
}

router_metrics_t *router_app_metrics(void)
{
	return &g_metrics;
}

router_page_t router_app_current_page(void)
{
	if (g_eng.page < 0)
		return ROUTER_PAGE_SYSTEM;
	return (router_page_t)g_eng.page;
}
