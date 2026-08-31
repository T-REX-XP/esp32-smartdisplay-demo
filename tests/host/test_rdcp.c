#include "../../src/proto/rdcp.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void expect(int cond, const char *msg)
{
	if (!cond) {
		fprintf(stderr, "FAIL: %s\n", msg);
		fails++;
	}
}

int main(void)
{
	rdcp_msg_t m;
	char buf[256];
	char tiny[8];

	expect(rdcp_parse(NULL, &m) != 0, "null line");
	expect(rdcp_parse("", &m) != 0, "empty");
	expect(rdcp_parse("   ", &m) != 0, "ws");
	expect(rdcp_parse("{}", NULL) != 0, "null out");
	expect(rdcp_parse("notjson", &m) != 0, "bad json");
	expect(rdcp_parse("{\"v\":2,\"t\":\"req\"}", &m) != 0, "v2");
	expect(rdcp_parse("{\"v\":1,\"t\":\"zzz\"}", &m) != 0, "bad t");

	expect(rdcp_parse("{\"request\":\"cpu\"}", &m) == 0, "legacy");
	expect(m.kind == RDCP_KIND_LEGACY && strcmp(m.scope, "cpu") == 0, "legacy scope");

	expect(rdcp_parse("{\"v\":1,\"t\":\"req\",\"id\":7,\"op\":\"ping\"}", &m) == 0, "ping");
	expect(m.kind == RDCP_KIND_REQ && m.id == 7 && strcmp(m.op, "ping") == 0, "ping fields");

	expect(rdcp_parse("{\"v\":1,\"t\":\"res\",\"id\":7,\"data\":{\"pong\":1,\"uptime_ms\":1234}}", &m) == 0, "pong");
	expect(m.kind == RDCP_KIND_RES && m.pong == 1 && m.uptime_ms == 1234, "pong fields");

	expect(rdcp_parse("{\"v\":1,\"t\":\"evt\",\"op\":\"echo\",\"data\":{\"text\":\"hi\"}}", &m) == 0, "echo");
	expect(strcmp(m.text, "hi") == 0, "echo text");

	expect(rdcp_parse("{\"v\":1,\"t\":\"cmd\",\"op\":\"screen\",\"data\":{\"screen\":\"router_system\",\"dir\":\"left\"}}", &m) == 0, "screen");
	expect(m.kind == RDCP_KIND_CMD && strcmp(m.screen, "router_system") == 0, "screen fields");

	expect(rdcp_parse("{\"v\":1,\"t\":\"push\",\"op\":\"config\",\"data\":{\"screen_timeout\":60,\"screen_timeout_mode\":\"off\"}}", &m) == 0, "config");
	expect(m.kind == RDCP_KIND_PUSH && m.screen_timeout == 60 && strcmp(m.timeout_mode, "off") == 0, "config fields");

	expect(rdcp_parse("{\"v\":1,\"t\":\"push\",\"op\":\"boot\",\"data\":{\"stage\":\"ready\",\"text\":\"System ready\",\"pct\":100}}", &m) == 0, "boot");
	expect(m.pct == 100 && strcmp(m.stage, "ready") == 0, "boot fields");

	expect(rdcp_parse("{\"v\":1,\"t\":\"req\",\"id\":\"3\",\"op\":\"metrics\",\"scope\":\"system\"}", &m) == 0, "quoted id");
	expect(m.id == 3, "quoted id val");

	expect(rdcp_parse("{\"v\":1,\"t\":\"cmd\",\"op\":\"echo\",\"data\":{\"text\":\"a\\\"b\"}}", &m) == 0, "escaped");
	expect(strcmp(m.text, "a\"b") == 0, "unescaped text");

	expect(rdcp_parse("{\"v\":1,\"t\":\"push\",\"op\":\"hello\"}", &m) == 0, "hello");
	expect(rdcp_parse("{\"v\":1,\"t\":\"evt\",\"op\":\"screen\",\"data\":{\"screen\":\"router_wifi\",\"action\":\"loaded\"}}", &m) == 0, "screen evt");
	expect(strcmp(m.screen, "router_wifi") == 0, "screen id");

	expect(rdcp_build_pong(NULL, 10, 1, 1) != 0, "pong null");
	expect(rdcp_build_pong(tiny, 8, 1, 1) != 0, "pong tiny");
	expect(rdcp_build_pong(buf, sizeof(buf), 7, 1234) == 0, "pong ok");
	expect(strstr(buf, "\"id\":7") && strstr(buf, "1234"), "pong body");

	expect(rdcp_build_echo(NULL, 10, "x") != 0, "echo null");
	expect(rdcp_build_echo(buf, sizeof(buf), NULL) == 0, "echo empty text");
	expect(rdcp_build_echo(buf, sizeof(buf), "a\"b\\c") == 0, "echo escape");
	expect(strstr(buf, "\\\"") != NULL, "escaped quote");
	expect(rdcp_build_echo(tiny, 8, "longtext") != 0, "echo tiny");
	{
		char q[130];
		char big[1024];
		memset(q, '"', 128);
		q[128] = '\0';
		expect(rdcp_build_echo(big, sizeof(big), q) == 0, "echo many quotes");
	}

	expect(rdcp_build_screen_evt(buf, sizeof(buf), "") != 0, "screen empty");
	expect(rdcp_build_screen_evt(NULL, 10, "x") != 0, "screen null");
	expect(rdcp_build_screen_evt(buf, sizeof(buf), "router_system") == 0, "screen ok");

	expect(rdcp_build_version_evt(NULL, 10, "1", 1, "c", 1) != 0, "ver null");
	expect(rdcp_build_version_evt(buf, sizeof(buf), NULL, 1, "c", 1) != 0, "ver stack");
	expect(rdcp_build_version_evt(buf, sizeof(buf), "1.0.0", 47, "esp32-router", 1) == 0, "ver ok");
	expect(rdcp_build_version_evt(tiny, 8, "1.0.0", 47, "esp32-router", 1) != 0, "ver tiny");

	expect(rdcp_build_metrics_req(buf, sizeof(buf), 0, "system") != 0, "metrics id0");
	expect(rdcp_build_metrics_req(buf, sizeof(buf), 1, "") != 0, "metrics scope");
	expect(rdcp_build_metrics_req(NULL, 10, 1, "system") != 0, "metrics null");
	expect(rdcp_build_metrics_req(buf, sizeof(buf), 3, "system") == 0, "metrics ok");
	expect(rdcp_build_metrics_req(tiny, 8, 3, "system") != 0, "metrics tiny");

	expect(rdcp_build_poweroff(NULL, 10) != 0, "pwr null");
	expect(rdcp_build_poweroff(tiny, 8) != 0, "pwr tiny");
	expect(rdcp_build_poweroff(buf, sizeof(buf)) == 0, "pwr ok");

	/* unclosed string / missing colon / missing t */
	expect(rdcp_parse("{\"v\":1,\"t\":\"req", &m) != 0, "unclosed t");
	expect(rdcp_parse("{\"v\" 1,\"t\":\"req\"}", &m) != 0, "no colon");
	expect(rdcp_parse("{\"v\":1,\"t\":\"req\",\"id\":x}", &m) == 0, "bad id still parses frame");
	expect(rdcp_parse("{\"v\":1,\"t\":\"res\"}", &m) == 0, "res no data");
	expect(rdcp_parse("{\"v\":1}", &m) != 0, "no t");
	expect(rdcp_parse("{\"request\":\"this-is-a-very-long-legacy-name-xyz\"}", &m) == 0, "long legacy");
	expect(rdcp_parse("{\"v\":1,\"t\":\"cmd\",\"op\":\"echo\",\"data\":{\"text\":123}}", &m) == 0, "text not string");
	expect(rdcp_parse("{\"v\":1,\"t\":\"req\",\"id\":\"\",\"op\":\"ping\"}", &m) == 0, "empty quoted id");
	expect(rdcp_parse("{\"v\":1,\"t\":\"cmd\",\"op\":\"echo\",\"data\":{\"text\":\"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789extra\"}}", &m) == 0, "long text");
	expect(rdcp_parse("{\"v\":1,\"t\":\"cmd\",\"op\":\"echo\",\"data\":{\"text\":\"abc\\", &m) != 0 || m.kind == RDCP_KIND_CMD, "backslash unclosed ok or fail");

	if (fails) {
		fprintf(stderr, "%d failures\n", fails);
		return 1;
	}
	puts("test_rdcp OK");
	return 0;
}
