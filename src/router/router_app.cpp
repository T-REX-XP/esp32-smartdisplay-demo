#include "router_app.h"

#include "router_pages.h"
#include "router_ui.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#define ROUTER_BOOT_TIMEOUT_MS 10000

static router_ui_t *g_ui;
static router_metrics_t g_metrics;
static unsigned g_req_id;
static unsigned long g_last_req_ms;
static unsigned long g_last_rx_ms;
static unsigned long g_boot_start_ms;
static bool g_host_linked;

static void send_line(const char *line)
{
	if (!line)
		return;
	Serial.println(line);
}

static void send_scope_request(const char *scope)
{
	char buf[96];

	g_req_id++;
	snprintf(buf, sizeof(buf),
		 "{\"v\":1,\"t\":\"req\",\"id\":%u,\"op\":\"metrics\",\"scope\":\"%s\"}",
		 g_req_id, scope);
	send_line(buf);
	g_last_req_ms = millis();
}

static void emit_screen_event(const char *screen_id)
{
	char buf[128];

	snprintf(buf, sizeof(buf),
		 "{\"v\":1,\"t\":\"evt\",\"op\":\"screen\",\"data\":{\"screen\":\"%s\",\"action\":\"loaded\"}}",
		 screen_id);
	send_line(buf);
}

static void emit_gesture_event(const char *dir)
{
	char buf[96];

	snprintf(buf, sizeof(buf),
		 "{\"v\":1,\"t\":\"evt\",\"op\":\"input\",\"data\":{\"type\":\"gesture\",\"dir\":\"%s\"}}",
		 dir);
	send_line(buf);
}

static lv_scr_load_anim_t anim_for_dir(const char *dir)
{
	if (dir && !strcmp(dir, "right"))
		return LV_SCR_LOAD_ANIM_MOVE_RIGHT;
	return LV_SCR_LOAD_ANIM_MOVE_LEFT;
}

static void show_router_page(router_page_t page, lv_scr_load_anim_t anim)
{
	router_ui_show_page(g_ui, page, anim);
	emit_screen_event(router_page_id(page));
	send_scope_request(router_page_scope(page));
}

static void leave_boot_to(router_page_t page, lv_scr_load_anim_t anim)
{
	show_router_page(page, anim);
	g_host_linked = true;
}

static void leave_boot_default(void)
{
	leave_boot_to(ROUTER_PAGE_SYSTEM, LV_SCR_LOAD_ANIM_FADE_ON);
}

static void apply_host_screen(const char *screen_id, lv_scr_load_anim_t anim)
{
	if (!screen_id || !screen_id[0])
		return;

	if (router_page_is_boot_id(screen_id)) {
		router_ui_show_boot(g_ui);
		emit_screen_event(screen_id);
		return;
	}

	router_page_t page = router_data_page_from_id(screen_id);
	leave_boot_to(page, anim);
}

static void apply_metrics_json(const char *json)
{
	if (!json)
		return;
	g_last_rx_ms = millis();
	g_host_linked = true;
	router_data_apply_json(&g_metrics, json);
	g_metrics.last_rx_ms = g_last_rx_ms;
	router_ui_refresh(g_ui, &g_metrics);
}

static void handle_push(JsonObject data, const char *op)
{
	if (!op)
		return;

	if (!strcmp(op, "boot")) {
		const char *text = data["text"] | "Booting...";
		unsigned pct = data["pct"] | 0;
		const char *screen = data["screen"];

		router_ui_set_boot_status(g_ui, text, pct);
		if (screen && screen[0])
			apply_host_screen(screen, LV_SCR_LOAD_ANIM_FADE_ON);
		return;
	}

	if (!strcmp(op, "alert")) {
		const char *text = data["text"] | "";
		const char *screen = data["screen"];

		if (text[0])
			router_ui_set_boot_status(g_ui, text, 100);
		if (screen && screen[0])
			apply_host_screen(screen, LV_SCR_LOAD_ANIM_FADE_ON);
	}
}

static void handle_cmd(JsonObject data, const char *op)
{
	if (!op)
		return;

	if (!strcmp(op, "screen")) {
		const char *screen = data["screen"];
		apply_host_screen(screen, LV_SCR_LOAD_ANIM_MOVE_LEFT);
		return;
	}

	if (!strcmp(op, "nav")) {
		const char *dir = data["dir"] | "next";
		router_page_t page = router_ui_current_page(g_ui);
		router_page_t next = page;
		lv_scr_load_anim_t anim = anim_for_dir(dir);

		if (!strcmp(dir, "prev") || !strcmp(dir, "right"))
			next = (router_page_t)((page + ROUTER_PAGE_COUNT - 1) % ROUTER_PAGE_COUNT);
		else
			next = (router_page_t)((page + 1) % ROUTER_PAGE_COUNT);

		if (!router_ui_on_boot(g_ui))
			leave_boot_to(next, anim);
	}
}

static void on_gesture(lv_event_t *e)
{
	lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

	if (lv_event_get_code(e) != LV_EVENT_GESTURE)
		return;
	lv_indev_wait_release(lv_indev_active());

	if (dir == LV_DIR_LEFT)
		emit_gesture_event("left");
	else if (dir == LV_DIR_RIGHT)
		emit_gesture_event("right");
}

static void attach_gestures(lv_obj_t *scr)
{
	if (scr)
		lv_obj_add_event_cb(scr, on_gesture, LV_EVENT_GESTURE, NULL);
}

void router_app_init(void)
{
	router_data_init(&g_metrics);
	g_ui = router_ui_create();
	g_boot_start_ms = millis();

	attach_gestures(router_ui_boot_screen(g_ui));
	for (int i = 0; i < ROUTER_PAGE_COUNT; i++)
		attach_gestures(router_ui_screen(g_ui, (router_page_t)i));

	emit_screen_event("router_boot");
}

void router_app_on_serial_line(const char *line)
{
	JsonDocument doc;

	if (!line || !line[0])
		return;

	if (deserializeJson(doc, line))
		return;

	const char *t = doc["t"];
	if (!t) {
		apply_metrics_json(line);
		return;
	}

	if (!strcmp(t, "res")) {
		char buf[2048];
		JsonObject data = doc["data"].as<JsonObject>();

		serializeJson(data, buf, sizeof(buf));
		apply_metrics_json(buf);
		return;
	}

	if (!strcmp(t, "push")) {
		handle_push(doc["data"].as<JsonObject>(), doc["op"]);
		return;
	}

	if (!strcmp(t, "cmd")) {
		handle_cmd(doc["data"].as<JsonObject>(), doc["op"]);
		return;
	}

	apply_metrics_json(line);
}

void router_app_loop(void)
{
	unsigned long now = millis();

	if (router_ui_on_boot(g_ui) && (now - g_boot_start_ms) >= ROUTER_BOOT_TIMEOUT_MS)
		leave_boot_default();

	if (router_ui_on_boot(g_ui) || !g_host_linked)
		return;

	router_page_t page = router_ui_current_page(g_ui);
	unsigned interval = (page == ROUTER_PAGE_SYSTEM) ? 1500u : 2000u;

	if (now - g_last_req_ms > interval)
		send_scope_request(router_page_scope(page));
}

router_metrics_t *router_app_metrics(void)
{
	return &g_metrics;
}

router_page_t router_app_current_page(void)
{
	return router_ui_current_page(g_ui);
}
