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

static void test_parse_system(void)
{
	router_metrics_t m;
	const char *json =
		"{\"v\":1,\"t\":\"res\",\"id\":3,\"data\":{"
		"\"hostname\":\"cm5\",\"cpu\":\"42\",\"ram_pct\":61,\"ram_used\":\"512M\","
		"\"load_short\":\"0.42\",\"cpu_temp\":\"52\","
		"\"wan_ip\":\"10.0.0.2\",\"wifi_ssid\":\"OpenWrt\","
		"\"wifi_qr\":\"WIFI:S:OpenWrt;;\",\"firewall_state\":\"on\"}}";

	router_data_init(&m);
	router_data_apply_json(&m, json);

	expect(!strcmp(m.hostname, "cm5"), "hostname");
	expect(!strcmp(m.cpu, "42"), "cpu not stolen from cpu_temp");
	expect(!strcmp(m.cpu_temp, "52"), "cpu_temp");
	expect(m.ram_pct == 61, "ram_pct");
	expect(!strcmp(m.ram_used, "512M"), "ram_used");
	expect(!strcmp(m.load_short, "0.42"), "load_short");
	expect(!strcmp(m.wan_ip, "10.0.0.2"), "wan_ip");
	expect(!strcmp(m.wifi_ssid, "OpenWrt"), "wifi_ssid");
	expect(strstr(m.wifi_qr, "OpenWrt") != NULL, "wifi_qr");
	expect(router_data_page_from_id("router_wifi") == ROUTER_PAGE_WIFI, "page id");
	expect(router_data_page_from_id("router_network") == ROUTER_PAGE_NETWORK,
	       "network page");
}

static void test_cpu_temp_key_order(void)
{
	router_metrics_t m;

	router_data_init(&m);
	router_data_apply_json(&m,
			       "{\"cpu_temp\":\"47\",\"cpu\":\"12\",\"ram_pct\":10}");
	expect(!strcmp(m.cpu, "12"), "cpu after cpu_temp in json");
	expect(!strcmp(m.cpu_temp, "47"), "cpu_temp before cpu in json");
}

static void test_hist_ring(void)
{
	router_metrics_t m;
	unsigned i;

	router_data_init(&m);
	for (i = 0; i < 5; i++) {
		char buf[80];

		snprintf(buf, sizeof(buf), "{\"cpu\":\"%u\",\"ram_pct\":%u}", i * 10,
			 20 + i);
		router_data_apply_json(&m, buf);
	}
	expect(m.hist_len == 5, "hist_len 5");
	expect(m.cpu_hist[0] == 0, "hist[0] cpu 0");
	expect(m.cpu_hist[4] == 40, "hist[4] cpu 40");
	expect(m.ram_hist[4] == 24, "hist[4] ram 24");

	for (i = 0; i < ROUTER_HIST_LEN + 3; i++)
		router_data_apply_json(&m, "{\"cpu\":\"99\",\"ram_pct\":50}");
	expect(m.hist_len == ROUTER_HIST_LEN, "hist capped at ROUTER_HIST_LEN");
}

static void test_network_does_not_touch_uart_link_or_hist(void)
{
	router_metrics_t m;

	router_data_init(&m);
	m.link_ok = true;
	m.last_rx_ms = 1234;
	router_data_apply_json(&m, "{\"cpu\":\"8\",\"ram_pct\":30}");
	expect(m.hist_len == 1, "system payload pushes hist");

	router_data_apply_json(&m,
			       "{\"wan_ip\":\"10.0.0.1\",\"link_ok\":false,\"ping_ms\":4}");
	expect(m.link_ok == true, "host link_ok does not clear UART link");
	expect(m.hist_len == 1, "network payload does not push hist");
	expect(!strcmp(m.wan_ip, "10.0.0.1"), "wan_ip still applied");
}

static void test_network_ports_and_ping(void)
{
	router_metrics_t m;
	const char *json =
		"{\"wan_ip\":\"10.0.0.2\",\"wan_dev\":\"eth0\","
		"\"rx_rate\":\"1.2M/s\",\"tx_rate\":\"80.0K/s\","
		"\"ping_ms\":12,\"ping_ok\":true,"
		"\"eth0_role\":\"WAN\",\"eth0_up\":true,\"eth0_speed\":\"2.5G\","
		"\"eth1_role\":\"LAN\",\"eth1_up\":true,\"eth1_speed\":\"2.5G\","
		"\"eth2_role\":\"LAN\",\"eth2_up\":false,\"eth2_speed\":\"--\"}";

	router_data_init(&m);
	router_data_apply_json(&m, json);
	expect(!strcmp(m.rx_rate, "1.2M/s"), "rx_rate");
	expect(!strcmp(m.tx_rate, "80.0K/s"), "tx_rate");
	expect(m.ping_ms == 12, "ping_ms");
	expect(m.ping_ok == 1, "ping_ok");
	expect(m.eth0_up == true, "eth0 up");
	expect(m.eth1_up == true, "eth1 up");
	expect(m.eth2_up == false, "eth2 down");
	expect(!strcmp(m.eth0_speed, "2.5G"), "eth0 speed");
	expect(!strcmp(m.eth0_role, "WAN"), "eth0 role");
	expect(!strcmp(m.eth2_role, "LAN"), "eth2 role");
}

int main(void)
{
	test_parse_system();
	test_cpu_temp_key_order();
	test_hist_ring();
	test_network_does_not_touch_uart_link_or_hist();
	test_network_ports_and_ping();

	printf(tests_failed ? "FAILED\n" : "OK\n");
	return tests_failed ? 1 : 0;
}
