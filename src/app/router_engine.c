#include "router_engine.h"
#include "../proto/rdcp.h"

#include <stdio.h>
#include <string.h>

#ifndef ROUTER_LINK_TIMEOUT_MS
#define ROUTER_LINK_TIMEOUT_MS 5000u
#endif

static const char *const PAGE_IDS[ROUTER_ENGINE_PAGE_COUNT] = {
	"router_system",
	"router_network",
	"router_clients",
	"router_storage",
	"router_wifi",
	"router_security",
};

static const char *const PAGE_SCOPES[ROUTER_ENGINE_PAGE_COUNT] = {
	"system", "network", "clients", "storage", "wifi", "security",
};

static unsigned now_ms(router_engine_t *e)
{
	if (e && e->hooks.now_ms)
		return e->hooks.now_ms(e->hooks.ctx);
	return 0;
}

static void tx(router_engine_t *e, const char *line)
{
	if (e && e->hooks.tx && line)
		e->hooks.tx(line, e->hooks.ctx);
}

static void mark_linked(router_engine_t *e)
{
	e->linked = true;
	e->last_rx_ms = now_ms(e);
}

static void emit_version(router_engine_t *e)
{
	char buf[192];
	const char *stack = e->hooks.stack ? e->hooks.stack : "1.0.0";
	const char *comp = e->hooks.component ? e->hooks.component : "esp32-router";
	unsigned rel = e->hooks.release ? e->hooks.release : 47;
	unsigned rdcp = e->hooks.rdcp ? e->hooks.rdcp : 1;

	if (rdcp_build_version_evt(buf, sizeof(buf), stack, rel, comp, rdcp) == 0)
		tx(e, buf);
}

static void emit_screen(router_engine_t *e, const char *id)
{
	char buf[160];

	if (rdcp_build_screen_evt(buf, sizeof(buf), id) == 0)
		tx(e, buf);
}

static void apply_page(router_engine_t *e, int page)
{
	if (page < 0 || page >= ROUTER_ENGINE_PAGE_COUNT) {
		e->page = ROUTER_ENGINE_BOOT;
		if (e->hooks.show_boot)
			e->hooks.show_boot(e->hooks.ctx);
		emit_screen(e, "router_boot");
		return;
	}
	e->page = page;
	if (e->hooks.show_page)
		e->hooks.show_page(page, e->hooks.ctx);
	emit_screen(e, PAGE_IDS[page]);
}

static void request_metrics(router_engine_t *e)
{
	char buf[96];
	const char *scope;

	if (e->page < 0 || e->page >= ROUTER_ENGINE_PAGE_COUNT)
		return;
	e->req_id++;
	scope = PAGE_SCOPES[e->page];
	if (rdcp_build_metrics_req(buf, sizeof(buf), e->req_id, scope) == 0)
		tx(e, buf);
	e->last_req_ms = now_ms(e);
}

static int neighbor(int page, const char *dir)
{
	if (page < 0)
		return 0;
	if (dir && (!strcmp(dir, "right") || !strcmp(dir, "prev")))
		return (page + ROUTER_ENGINE_PAGE_COUNT - 1) % ROUTER_ENGINE_PAGE_COUNT;
	return (page + 1) % ROUTER_ENGINE_PAGE_COUNT;
}

int router_engine_page_from_id(const char *screen_id)
{
	int i;

	if (!screen_id)
		return ROUTER_ENGINE_BOOT;
	if (router_engine_is_boot_id(screen_id))
		return ROUTER_ENGINE_BOOT;
	for (i = 0; i < ROUTER_ENGINE_PAGE_COUNT; i++) {
		if (!strcmp(screen_id, PAGE_IDS[i]))
			return i;
	}
	return 0;
}

const char *router_engine_page_id(int page)
{
	if (page < 0 || page >= ROUTER_ENGINE_PAGE_COUNT)
		return "router_boot";
	return PAGE_IDS[page];
}

const char *router_engine_page_scope(int page)
{
	if (page < 0 || page >= ROUTER_ENGINE_PAGE_COUNT)
		return "";
	return PAGE_SCOPES[page];
}

bool router_engine_is_boot_id(const char *screen_id)
{
	return screen_id && !strcmp(screen_id, "router_boot");
}

void router_engine_init(router_engine_t *e, const router_hooks_t *hooks)
{
	if (!e)
		return;
	memset(e, 0, sizeof(*e));
	e->page = ROUTER_ENGINE_BOOT;
	if (hooks)
		e->hooks = *hooks;
	if (e->hooks.show_boot)
		e->hooks.show_boot(e->hooks.ctx);
	emit_screen(e, "router_boot");
	emit_version(e);
}

int router_engine_on_line(router_engine_t *e, const char *line)
{
	rdcp_msg_t msg;
	char buf[256];

	if (!e || !line || !line[0])
		return -1;
	if (rdcp_parse(line, &msg) != 0)
		return -1;

	if (msg.kind == RDCP_KIND_LEGACY) {
		mark_linked(e);
		if (e->hooks.apply_metrics)
			e->hooks.apply_metrics(line, e->hooks.ctx);
		return 0;
	}

	switch (msg.kind) {
	case RDCP_KIND_REQ:
		mark_linked(e);
		if (!strcmp(msg.op, "version")) {
			emit_version(e);
			return 0;
		}
		if (!strcmp(msg.op, "ping")) {
			if (rdcp_build_pong(buf, sizeof(buf), msg.id, now_ms(e)) == 0)
				tx(e, buf);
			return 0;
		}
		return 0;
	case RDCP_KIND_PUSH:
		mark_linked(e);
		if (!strcmp(msg.op, "hello"))
			emit_version(e);
		if (!strcmp(msg.op, "boot")) {
			if (e->hooks.set_boot_status)
				e->hooks.set_boot_status(msg.text[0] ? msg.text : "Booting...",
							 msg.pct, e->hooks.ctx);
			if (msg.screen[0])
				apply_page(e, router_engine_page_from_id(msg.screen));
			return 0;
		}
		if (!strcmp(msg.op, "config")) {
			e->screen_timeout = msg.screen_timeout;
			if (msg.timeout_mode[0])
				snprintf(e->timeout_mode, sizeof(e->timeout_mode), "%s", msg.timeout_mode);
			return 0;
		}
		if (!strcmp(msg.op, "alert")) {
			if (e->hooks.set_boot_status && msg.text[0])
				e->hooks.set_boot_status(msg.text, 100, e->hooks.ctx);
			if (msg.screen[0])
				apply_page(e, router_engine_page_from_id(msg.screen));
			return 0;
		}
		return 0;
	case RDCP_KIND_CMD:
		mark_linked(e);
		if (!strcmp(msg.op, "screen") && msg.screen[0]) {
			if (msg.dir[0])
				snprintf(e->last_gesture_dir, sizeof(e->last_gesture_dir), "%s", msg.dir);
			apply_page(e, router_engine_page_from_id(msg.screen));
			request_metrics(e);
			return 0;
		}
		if (!strcmp(msg.op, "nav")) {
			apply_page(e, neighbor(e->page, msg.dir[0] ? msg.dir : "next"));
			request_metrics(e);
			return 0;
		}
		if (!strcmp(msg.op, "echo")) {
			if (rdcp_build_echo(buf, sizeof(buf), msg.text) == 0)
				tx(e, buf);
			return 0;
		}
		return 0;
	case RDCP_KIND_RES:
		mark_linked(e);
		if (e->hooks.apply_metrics)
			e->hooks.apply_metrics(line, e->hooks.ctx);
		return 0;
	default:
		return 0;
	}
}

int router_engine_on_input(router_engine_t *e, const char *dir)
{
	char buf[160];

	if (!e)
		return -1;
	if (!dir || !dir[0])
		dir = "left";
	snprintf(e->last_gesture_dir, sizeof(e->last_gesture_dir), "%s", dir);
	/* Notify the host before the local page change so mcudd can write
	 * /tmp/mcud_active_screen (LuCI) even when cmd screen is rate-limited.
	 * Then apply locally — waiting for host cmd bricks swipe when CM5 TX
	 * never reaches GPIO3 (USB/CH340 share). apply_page emits evt screen. */
	if (rdcp_build_input_evt(buf, sizeof(buf), dir) == 0)
		tx(e, buf);
	apply_page(e, neighbor(e->page, dir));
	if (e->linked)
		request_metrics(e);
	return 0;
}

int router_engine_tick(router_engine_t *e)
{
	unsigned now;
	unsigned interval;

	if (!e)
		return 0;
	now = now_ms(e);
	/* Keep announcing until the host is known-reachable. Init events are
	 * easy to miss (USB flash / mcudd restart); without this LuCI stays
	 * on router_boot and swipe evt never has a listener. */
	if (!e->linked) {
		if (e->last_req_ms == 0 || now - e->last_req_ms > 2000u) {
			emit_version(e);
			emit_screen(e, router_engine_page_id(e->page));
			e->last_req_ms = now ? now : 1;
		}
		return 0;
	}
	if (e->page < 0)
		return 0;
	interval = (e->page == 0) ? 1500u : 2000u;
	if (now - e->last_req_ms > interval)
		request_metrics(e);
	return 0;
}

bool router_engine_link_ok(router_engine_t *e, unsigned timeout_ms)
{
	if (!e || !e->linked || e->last_rx_ms == 0)
		return false;
	if (timeout_ms == 0)
		timeout_ms = ROUTER_LINK_TIMEOUT_MS;
	return (now_ms(e) - e->last_rx_ms) <= timeout_ms;
}

int router_engine_emit_poweroff(router_engine_t *e)
{
	char buf[96];

	if (!e)
		return -1;
	if (rdcp_build_poweroff(buf, sizeof(buf)) == 0)
		tx(e, buf);
	return 0;
}
