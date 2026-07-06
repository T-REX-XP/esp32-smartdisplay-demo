#include "router_app.h"

#include "router_pages.h"
#include "router_ui.h"

#include <Arduino.h>

static router_ui_t *g_ui;
static router_metrics_t g_metrics;
static unsigned g_req_id;
static unsigned long g_last_req_ms;
static unsigned long g_last_rx_ms;

static void send_scope_request(const char *scope)
{
	char buf[96];

	g_req_id++;
	snprintf(buf, sizeof(buf),
		 "{\"v\":1,\"t\":\"req\",\"id\":%u,\"op\":\"metrics\",\"scope\":\"%s\"}",
		 g_req_id, scope);
	Serial.println(buf);
	g_last_req_ms = millis();
}

static void emit_screen_event(router_page_t page)
{
	char buf[96];

	snprintf(buf, sizeof(buf),
		 "{\"v\":1,\"t\":\"evt\",\"op\":\"screen\",\"data\":{\"screen\":\"%s\",\"action\":\"loaded\"}}",
		 router_page_id(page));
	Serial.println(buf);
}

static void on_gesture(lv_event_t *e)
{
	lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
	router_page_t page = router_ui_current_page(g_ui);
	router_page_t next;

	if (lv_event_get_code(e) != LV_EVENT_GESTURE)
		return;
	lv_indev_wait_release(lv_indev_active());

	lv_scr_load_anim_t anim;

	if (dir == LV_DIR_LEFT) {
		next = (router_page_t)((page + 1) % ROUTER_PAGE_COUNT);
		anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
	} else if (dir == LV_DIR_RIGHT) {
		next = (router_page_t)((page + ROUTER_PAGE_COUNT - 1) % ROUTER_PAGE_COUNT);
		anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
	} else {
		return;
	}

	router_ui_show_page(g_ui, next, anim);
	emit_screen_event(next);
	send_scope_request(router_page_scope(next));
}

static void attach_gestures(void)
{
	for (int i = 0; i < ROUTER_PAGE_COUNT; i++) {
		lv_obj_t *scr = router_ui_screen(g_ui, (router_page_t)i);
		if (scr)
			lv_obj_add_event_cb(scr, on_gesture, LV_EVENT_GESTURE, NULL);
	}
}

void router_app_init(void)
{
	router_data_init(&g_metrics);
	g_ui = router_ui_create();
	attach_gestures();
	emit_screen_event(ROUTER_PAGE_SYSTEM);
	send_scope_request(router_page_scope(ROUTER_PAGE_SYSTEM));
}

void router_app_on_serial_line(const char *line)
{
	if (!line || !line[0])
		return;
	g_last_rx_ms = millis();
	router_data_apply_json(&g_metrics, line);
	g_metrics.last_rx_ms = g_last_rx_ms;
	router_ui_refresh(g_ui, &g_metrics);
}

void router_app_loop(void)
{
	unsigned long now = millis();
	router_page_t page = router_ui_current_page(g_ui);
	unsigned interval = 2000;

	if (page == ROUTER_PAGE_SYSTEM)
		interval = 1500;

	if (g_last_rx_ms == 0 && now > 30000) {
		/* waiting for host */
	} else if (now - g_last_req_ms > interval) {
		send_scope_request(router_page_scope(page));
	}
}

router_metrics_t *router_app_metrics(void)
{
	return &g_metrics;
}

router_page_t router_app_current_page(void)
{
	return router_ui_current_page(g_ui);
}
