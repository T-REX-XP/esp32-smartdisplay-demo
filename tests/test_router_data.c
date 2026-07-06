#include <stdio.h>
#include <string.h>

#include "router_data.h"

static int tests_failed;

static void expect(int cond, const char *msg)
{
	if (!cond) {
		tests_failed++;
		printf("FAIL: %s\n", msg);
	}
}

int main(void)
{
	router_metrics_t m;
	const char *json =
		"{\"v\":1,\"t\":\"res\",\"id\":3,\"data\":{"
		"\"hostname\":\"cm5\",\"cpu\":\"42\",\"ram_pct\":61,"
		"\"wan_ip\":\"10.0.0.2\",\"wifi_ssid\":\"OpenWrt\","
		"\"wifi_qr\":\"WIFI:S:OpenWrt;;\",\"firewall_state\":\"on\"}}";

	router_data_init(&m);
	router_data_apply_json(&m, json);

	expect(!strcmp(m.hostname, "cm5"), "hostname");
	expect(!strcmp(m.cpu, "42"), "cpu");
	expect(m.ram_pct == 61, "ram_pct");
	expect(!strcmp(m.wan_ip, "10.0.0.2"), "wan_ip");
	expect(!strcmp(m.wifi_ssid, "OpenWrt"), "wifi_ssid");
	expect(strstr(m.wifi_qr, "OpenWrt") != NULL, "wifi_qr");
	expect(router_data_page_from_id("router_wifi") == ROUTER_PAGE_WIFI, "page id");
	expect(router_data_page_from_id("router_network") == ROUTER_PAGE_NETWORK, "network page");

	printf(tests_failed ? "FAILED\n" : "OK\n");
	return tests_failed ? 1 : 0;
}
