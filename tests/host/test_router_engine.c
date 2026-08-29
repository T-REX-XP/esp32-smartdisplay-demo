#include "../../src/app/router_engine.h"

#include <stdio.h>
#include <string.h>

static int fails;
static char last_tx[16][256];
static int tx_n;
static int shown_page = -99;
static int shown_boot;
static char boot_text[64];
static unsigned boot_pct;
static char metrics[256];
static unsigned fake_now;

static void expect(int cond, const char *msg)
{
	if (!cond) {
		fprintf(stderr, "FAIL: %s\n", msg);
		fails++;
	}
}

static void on_tx(const char *line, void *ctx)
{
	(void)ctx;
	if (tx_n < 16)
		snprintf(last_tx[tx_n++], sizeof(last_tx[0]), "%s", line ? line : "");
}

static void on_boot(void *ctx)
{
	(void)ctx;
	shown_boot++;
	shown_page = ROUTER_ENGINE_BOOT;
}

static void on_page(int page, void *ctx)
{
	(void)ctx;
	shown_page = page;
}

static void on_boot_status(const char *text, unsigned pct, void *ctx)
{
	(void)ctx;
	snprintf(boot_text, sizeof(boot_text), "%s", text ? text : "");
	boot_pct = pct;
}

static void on_metrics(const char *json, void *ctx)
{
	(void)ctx;
	snprintf(metrics, sizeof(metrics), "%s", json ? json : "");
}

static unsigned on_now(void *ctx)
{
	(void)ctx;
	return fake_now;
}

static router_hooks_t hooks(void)
{
	router_hooks_t h;

	memset(&h, 0, sizeof(h));
	h.tx = on_tx;
	h.show_boot = on_boot;
	h.show_page = on_page;
	h.set_boot_status = on_boot_status;
	h.apply_metrics = on_metrics;
	h.now_ms = on_now;
	h.stack = "1.0.0";
	h.release = 47;
	h.component = "esp32-router";
	h.rdcp = 1;
	return h;
}

static int tx_has(const char *needle)
{
	int i;

	for (i = 0; i < tx_n; i++) {
		if (strstr(last_tx[i], needle))
			return 1;
	}
	return 0;
}

int main(void)
{
	router_engine_t e;
	router_hooks_t h = hooks();

	router_engine_init(NULL, &h);
	expect(router_engine_on_line(NULL, "{}") != 0, "null eng");
	expect(router_engine_on_input(NULL, "left") != 0, "null input");
	expect(router_engine_emit_poweroff(NULL) != 0, "null pwr");
	expect(router_engine_tick(NULL) == 0, "null tick");
	expect(router_engine_page_from_id(NULL) == ROUTER_ENGINE_BOOT, "null id");
	expect(router_engine_is_boot_id("router_boot"), "boot id");
	expect(!router_engine_is_boot_id(NULL), "null boot");
	expect(strcmp(router_engine_page_id(-1), "router_boot") == 0, "id boot");
	expect(strcmp(router_engine_page_id(0), "router_system") == 0, "id sys");
	expect(router_engine_page_scope(-1)[0] == '\0', "scope boot");
	expect(strcmp(router_engine_page_scope(1), "network") == 0, "scope net");
	expect(router_engine_page_from_id("router_wifi") == 4, "wifi idx");
	expect(router_engine_page_from_id("nope") == 0, "unknown page");

	tx_n = 0;
	shown_boot = 0;
	router_engine_init(&e, &h);
	expect(shown_boot == 1, "init boot");
	expect(tx_has("router_boot"), "init screen evt");
	expect(tx_has("\"op\":\"version\""), "init version");
	expect(!e.linked, "standalone");

	tx_n = 0;
	expect(router_engine_on_input(&e, "left") == 0, "standalone swipe");
	expect(shown_page == 0, "local nav to system");
	expect(tx_has("\"op\":\"screen\""), "local screen evt");
	expect(!tx_has("\"op\":\"input\""), "no input when standalone");

	tx_n = 0;
	expect(router_engine_on_input(&e, "right") == 0, "standalone right");
	expect(shown_page == ROUTER_ENGINE_PAGE_COUNT - 1, "wrap prev");

	expect(router_engine_on_line(&e, "") != 0, "empty line");
	expect(router_engine_on_line(&e, "not-json") != 0, "bad line");

	tx_n = 0;
	fake_now = 100;
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"push\",\"op\":\"hello\",\"data\":{\"stack\":\"1.0.0\",\"release\":47,\"component\":\"mcudd\",\"rdcp\":1}}") == 0, "hello");
	expect(e.linked, "linked after hello");
	expect(tx_has("\"op\":\"version\""), "hello version");

	tx_n = 0;
	expect(router_engine_on_input(&e, NULL) == 0, "linked swipe");
	expect(tx_has("\"op\":\"input\""), "evt input");
	expect(tx_has("left"), "default dir");

	tx_n = 0;
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"req\",\"id\":7,\"op\":\"ping\"}") == 0, "ping");
	expect(tx_has("\"id\":7"), "pong id");
	expect(tx_has("\"pong\":1"), "pong");

	tx_n = 0;
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"req\",\"id\":1,\"op\":\"version\"}") == 0, "req version");
	expect(tx_has("\"op\":\"version\""), "evt version");

	tx_n = 0;
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"cmd\",\"op\":\"echo\",\"data\":{\"text\":\"mcud-link-test\"}}") == 0, "echo");
	expect(tx_has("mcud-link-test"), "echo text");

	tx_n = 0;
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"cmd\",\"op\":\"screen\",\"data\":{\"screen\":\"router_network\",\"dir\":\"left\"}}") == 0, "cmd screen");
	expect(shown_page == 1, "network page");
	expect(tx_has("router_network"), "screen ack");
	expect(tx_has("\"op\":\"metrics\""), "metrics poll");

	tx_n = 0;
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"cmd\",\"op\":\"nav\",\"data\":{\"dir\":\"prev\"}}") == 0, "nav prev");
	expect(shown_page == 0, "nav to system");

	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"cmd\",\"op\":\"nav\",\"data\":{\"dir\":\"next\"}}") == 0, "nav next");

	tx_n = 0;
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"cmd\",\"op\":\"screen\",\"data\":{\"screen\":\"router_boot\"}}") == 0, "cmd boot screen");
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"push\",\"op\":\"boot\",\"data\":{\"text\":\"Network\",\"pct\":60,\"screen\":\"router_boot\"}}") == 0, "push boot");
	expect(shown_page == ROUTER_ENGINE_BOOT, "back to boot");
	expect(boot_pct == 60, "boot pct");

	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"push\",\"op\":\"boot\",\"data\":{\"pct\":1}}") == 0, "boot default text");

	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"push\",\"op\":\"config\",\"data\":{\"screen_timeout\":60,\"screen_timeout_mode\":\"off\"}}") == 0, "config");
	expect(e.screen_timeout == 60 && strcmp(e.timeout_mode, "off") == 0, "config stored");

	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"push\",\"op\":\"alert\",\"data\":{\"text\":\"WAN down\",\"screen\":\"router_system\"}}") == 0, "alert");
	expect(strcmp(boot_text, "WAN down") == 0, "alert text");
	expect(shown_page == 0, "alert screen");

	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"push\",\"op\":\"other\"}") == 0, "unknown push");
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"cmd\",\"op\":\"other\"}") == 0, "unknown cmd");
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"req\",\"op\":\"other\"}") == 0, "unknown req");
	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"evt\",\"op\":\"screen\"}") == 0, "inbound evt ignored");

	expect(router_engine_on_line(&e, "{\"v\":1,\"t\":\"res\",\"id\":3,\"data\":{\"hostname\":\"Router\"}}") == 0, "res");
	expect(strstr(metrics, "hostname") != NULL, "metrics applied");
	expect(router_engine_on_line(&e, "{\"request\":\"cpu\"}") == 0, "legacy");

	tx_n = 0;
	expect(router_engine_emit_poweroff(&e) == 0, "poweroff");
	expect(tx_has("poweroff"), "pwr tx");

	fake_now = 2000;
	e.last_req_ms = 0;
	e.page = 0;
	e.linked = true;
	tx_n = 0;
	expect(router_engine_tick(&e) == 0, "tick poll");
	expect(tx_has("metrics"), "tick metrics");

	e.page = 1;
	e.last_req_ms = fake_now;
	tx_n = 0;
	expect(router_engine_tick(&e) == 0, "tick interval hold");

	e.last_rx_ms = 100;
	fake_now = 100;
	expect(router_engine_link_ok(&e, 5000), "link ok");
	fake_now = 6000;
	expect(!router_engine_link_ok(&e, 5000), "link stale");
	expect(!router_engine_link_ok(NULL, 5000), "link null");
	e.linked = false;
	expect(!router_engine_link_ok(&e, 0), "link unlinked");
	e.linked = true;
	e.last_rx_ms = 0;
	expect(!router_engine_link_ok(&e, 0), "link no rx");
	e.last_rx_ms = 100;
	fake_now = 200;
	expect(router_engine_link_ok(&e, 0), "link default timeout");

	e.linked = false;
	e.page = 0;
	expect(router_engine_tick(&e) == 0, "tick unlinked");
	e.linked = true;
	e.page = ROUTER_ENGINE_BOOT;
	expect(router_engine_tick(&e) == 0, "tick boot");

	/* no hooks */
	{
		router_engine_t bare;
		router_hooks_t empty;

		memset(&empty, 0, sizeof(empty));
		router_engine_init(&bare, &empty);
		expect(router_engine_on_line(&bare, "{\"v\":1,\"t\":\"push\",\"op\":\"hello\"}") == 0, "bare hello");
		expect(router_engine_on_line(&bare, "{\"v\":1,\"t\":\"push\",\"op\":\"boot\",\"data\":{\"text\":\"x\",\"pct\":1}}") == 0, "bare boot");
		expect(router_engine_on_line(&bare, "{\"v\":1,\"t\":\"push\",\"op\":\"alert\",\"data\":{\"text\":\"x\"}}") == 0, "bare alert");
		expect(router_engine_on_line(&bare, "{\"v\":1,\"t\":\"cmd\",\"op\":\"screen\",\"data\":{\"screen\":\"router_system\"}}") == 0, "bare screen");
		expect(router_engine_on_line(&bare, "{\"v\":1,\"t\":\"res\",\"id\":1}") == 0, "bare res");
		expect(router_engine_on_line(&bare, "{\"request\":\"cpu\"}") == 0, "bare legacy");
		expect(router_engine_on_input(&bare, "left") == 0, "bare input");
		expect(router_engine_emit_poweroff(&bare) == 0, "bare pwr");
		expect(router_engine_on_line(&bare, "{\"v\":1,\"t\":\"cmd\",\"op\":\"echo\",\"data\":{\"text\":\"z\"}}") == 0, "bare echo");
		expect(router_engine_on_line(&bare, "{\"v\":1,\"t\":\"req\",\"id\":1,\"op\":\"ping\"}") == 0, "bare ping");
	}

	if (fails) {
		fprintf(stderr, "%d failures\n", fails);
		return 1;
	}
	puts("test_router_engine OK");
	return 0;
}
