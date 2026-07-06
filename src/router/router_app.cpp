#include "router_app.h"

#include "router_pages.h"
#include "router_ui.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdlib.h>
#include <string.h>

#define ROUTER_BOOT_TIMEOUT_MS 10000
#define ROUTER_SWIPE_MIN_PX 40

static router_ui_t *g_ui;
static router_metrics_t g_metrics;
static unsigned g_req_id;
static unsigned long g_last_req_ms;
static unsigned long g_last_rx_ms;
static unsigned long g_boot_start_ms;
static bool g_host_linked;
static lv_point_t g_swipe_start;
static bool g_swipe_active;
static char g_last_gesture_dir[8];

static void send_line(const char *line)
{
	if (!line)
		return;
	Serial.println(line);
}

static void host_frame_received(void)
{
	g_host_linked = true;
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

static router_page_t page_for_dir(router_page_t page, const char *dir)
{
	if (!dir || !strcmp(dir, "left"))
		return (router_page_t)((page + 1) % ROUTER_PAGE_COUNT);
	return (router_page_t)((page + ROUTER_PAGE_COUNT - 1) % ROUTER_PAGE_COUNT);
}

static void show_router_page(router_page_t page, lv_scr_load_anim_t anim)
{
	router_ui_show_page(g_ui, page, anim);
	emit_screen_event(router_page_id(page));
	if (g_host_linked)
		send_scope_request(router_page_scope(page));
}

static void apply_local_nav(const char *dir)
{
	router_page_t page = router_ui_current_page(g_ui);
	router_page_t next;
	lv_scr_load_anim_t anim = anim_for_dir(dir);

	if (router_ui_on_boot(g_ui)) {
		next = ROUTER_PAGE_SYSTEM;
		anim = LV_SCR_LOAD_ANIM_FADE_ON;
	} else {
		next = page_for_dir(page, dir);
	}

	show_router_page(next, anim);
}

static void leave_boot_default(void)
{
	router_ui_show_page(g_ui, ROUTER_PAGE_SYSTEM, LV_SCR_LOAD_ANIM_FADE_ON);
	emit_screen_event(router_page_id(ROUTER_PAGE_SYSTEM));
	if (g_host_linked)
		send_scope_request(router_page_scope(ROUTER_PAGE_SYSTEM));
}

static void apply_host_screen(const char *screen_id, lv_scr_load_anim_t anim)
{
	if (!screen_id || !screen_id[0])
		return;

	host_frame_received();

	if (router_page_is_boot_id(screen_id)) {
		router_ui_show_boot(g_ui);
		emit_screen_event(screen_id);
		return;
	}

	router_page_t page = router_data_page_from_id(screen_id);
	router_ui_show_page(g_ui, page, anim);
	emit_screen_event(screen_id);
	if (g_host_linked)
		send_scope_request(router_page_scope(page));
}

static void apply_metrics_json(const char *json)
{
	if (!json)
		return;
	host_frame_received();
	g_last_rx_ms = millis();
	router_data_apply_json(&g_metrics, json);
	g_metrics.last_rx_ms = g_last_rx_ms;
	router_ui_refresh(g_ui, &g_metrics);
}

static void handle_push(JsonObject data, const char *op)
{
	if (!op)
		return;

	host_frame_received();

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

	host_frame_received();

	if (!strcmp(op, "screen")) {
		const char *screen = data["screen"];
		const char *dir = g_last_gesture_dir[0] ? g_last_gesture_dir : "left";

		apply_host_screen(screen, anim_for_dir(dir));
		return;
	}

	if (!strcmp(op, "nav")) {
		const char *dir = data["dir"] | "next";
		router_page_t page = router_ui_current_page(g_ui);
		router_page_t next = page;
		lv_scr_load_anim_t anim = anim_for_dir(dir);
		const char *gesture_dir = dir;

		if (!strcmp(dir, "prev") || !strcmp(dir, "right")) {
			next = (router_page_t)((page + ROUTER_PAGE_COUNT - 1) % ROUTER_PAGE_COUNT);
			gesture_dir = "right";
		} else {
			next = (router_page_t)((page + 1) % ROUTER_PAGE_COUNT);
			gesture_dir = "left";
		}

		if (!router_ui_on_boot(g_ui))
			apply_host_screen(router_page_id(next), anim_for_dir(gesture_dir));
	}
}

static void on_nav_request(const char *dir)
{
	strncpy(g_last_gesture_dir, dir, sizeof(g_last_gesture_dir) - 1);
	g_last_gesture_dir[sizeof(g_last_gesture_dir) - 1] = '\0';
	emit_gesture_event(dir);

	/* Host-linked: mcudd/webui replies with cmd. Standalone: navigate locally. */
	if (!g_host_linked)
		apply_local_nav(dir);
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

	on_nav_request(dx < 0 ? "left" : "right");
}

static void attach_swipe(lv_obj_t *scr)
{
	router_ui_install_swipe(scr, on_swipe_touch);
}

void router_app_init(void)
{
	router_data_init(&g_metrics);
	g_ui = router_ui_create();
	g_boot_start_ms = millis();
	g_last_gesture_dir[0] = '\0';

	attach_swipe(router_ui_boot_screen(g_ui));
	for (int i = 0; i < ROUTER_PAGE_COUNT; i++)
		attach_swipe(router_ui_screen(g_ui, (router_page_t)i));

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

	if (router_ui_on_boot(g_ui))
		return;

	router_page_t page = router_ui_current_page(g_ui);
	unsigned interval = (page == ROUTER_PAGE_SYSTEM) ? 1500u : 2000u;

	if (!g_host_linked)
		return;

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
