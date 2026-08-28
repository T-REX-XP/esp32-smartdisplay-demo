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

static void test_wifi_payload(void)
{
	router_metrics_t m;
	const char *json =
		"{\"wifi_ssid\":\"ImmortalCM5\",\"wifi_enc\":\"WPA2\","
		"\"wifi_ap_state\":\"up\","
		"\"wifi_qr\":\"WIFI:T:WPA;S:ImmortalCM5;P:secret;;\"}";

	router_data_init(&m);
	router_data_apply_json(&m, json);
	expect(!strcmp(m.wifi_ssid, "ImmortalCM5"), "wifi ssid");
	expect(!strcmp(m.wifi_enc, "WPA2"), "wifi enc");
	expect(!strcmp(m.wifi_ap_state, "up"), "wifi state");
	expect(!strcmp(m.wifi_qr, "WIFI:T:WPA;S:ImmortalCM5;P:secret;;"), "wifi qr");

	router_data_apply_json(&m,
			       "{\"wifi_qr\":\"WIFI:T:WPA;S:Foo\\\\;Bar;P:x;;\"}");
	expect(!strcmp(m.wifi_qr, "WIFI:T:WPA;S:Foo\\;Bar;P:x;;"),
	       "qr json unescape");
}

static void test_init_defaults(void)
{
	router_metrics_t m;

	router_data_init(&m);
	expect(!strcmp(m.hostname, "Router"), "default hostname");
	expect(!strcmp(m.cpu, "0"), "default cpu");
	expect(!strcmp(m.firewall_state, "unknown"), "default firewall");
	expect(!strcmp(m.swap_usage, "off"), "default swap off");
	expect(m.dhcp_pool == 150, "default pool 150");
	expect(m.link_ok == false, "link starts down");
	expect(m.ping_ms == -1, "ping unset");
	expect(m.hist_len == 0, "empty hist");
}

static void test_clients_payload(void)
{
	router_metrics_t m;
	const char *json =
		"{\"wifi_24\":\"1\",\"wifi_5\":\"2\",\"lan_clients\":\"3\","
		"\"clients_total\":\"5 clients\",\"dhcp_leases\":\"5\","
		"\"dhcp_pool\":150,\"dhcp_pct\":3,\"dhcp_summary\":\"phone, laptop, +3\"}";

	router_data_init(&m);
	router_data_apply_json(&m, json);
	expect(!strcmp(m.wifi_24, "1"), "wifi_24");
	expect(!strcmp(m.wifi_5, "2"), "wifi_5");
	expect(!strcmp(m.lan_clients, "3"), "lan_clients");
	expect(!strcmp(m.clients_total, "5 clients"), "clients_total");
	expect(!strcmp(m.dhcp_leases, "5"), "dhcp_leases");
	expect(!strcmp(m.dhcp_summary, "phone, laptop, +3"), "dhcp_summary");
	expect(m.dhcp_pool == 150, "dhcp_pool");
	expect(m.dhcp_pct == 3, "dhcp_pct");
	expect(m.hist_len == 0, "clients does not push hist");
}

static void test_storage_payload(void)
{
	router_metrics_t m;
	const char *json =
		"{\"root_usage\":\"118M/496M\",\"root_pct\":24,\"root_dev\":\"root\","
		"\"data_usage\":\"25.4G/28.5G\",\"data_pct\":93,\"data_kind\":\"emmc\","
		"\"overlay_dev\":\"mmcblk0p1\",\"swap_usage\":\"256M/512M\",\"swap_pct\":50}";

	router_data_init(&m);
	router_data_apply_json(&m, json);
	expect(!strcmp(m.root_usage, "118M/496M"), "root_usage");
	expect(m.root_pct == 24, "root_pct");
	expect(!strcmp(m.root_dev, "root"), "root_dev");
	expect(!strcmp(m.data_usage, "25.4G/28.5G"), "data_usage");
	expect(m.data_pct == 93, "data_pct");
	expect(!strcmp(m.data_kind, "emmc"), "data_kind");
	expect(!strcmp(m.overlay_dev, "mmcblk0p1"), "overlay_dev");
	expect(!strcmp(m.swap_usage, "256M/512M"), "swap_usage");
	expect(m.swap_pct == 50, "swap_pct");
}

static void test_storage_legacy_array_fallback(void)
{
	router_metrics_t m;

	router_data_init(&m);
	router_data_apply_json(&m,
			       "{\"storage\":[{\"mountpoint\":\"/\",\"used_percent\":88}]}");
	expect(m.root_pct == 88, "legacy storage[] used_percent");
}

static void test_security_payload(void)
{
	router_metrics_t m;
	const char *json =
		"{\"firewall_state\":\"lan ok · wan Rj/drop\",\"blocked_24h\":\"42+138\","
		"\"vpn_tunnels\":\"2 (wg+ts)\",\"blocky_blocked\":42,\"banip_blocked\":138}";

	router_data_init(&m);
	router_data_apply_json(&m, json);
	expect(!strcmp(m.firewall_state, "lan ok · wan Rj/drop"), "firewall_state");
	expect(!strcmp(m.blocked_24h, "42+138"), "blocked_24h");
	expect(!strcmp(m.vpn_tunnels, "2 (wg+ts)"), "vpn_tunnels");
	expect(m.blocky_blocked == 42, "blocky_blocked");
	expect(m.banip_blocked == 138, "banip_blocked");
}

static void test_uptime_temp_alias_and_rdcp_wrapper(void)
{
	router_metrics_t m;
	const char *wrapped =
		"{\"v\":1,\"t\":\"res\",\"id\":2,\"data\":{"
		"\"uptime_short\":\"1h 02m\",\"time\":\"19:42\",\"temp_c\":\"48\"}}";

	router_data_init(&m);
	router_data_apply_json(&m, wrapped);
	expect(!strcmp(m.uptime_short, "1h 02m"), "uptime_short");
	expect(!strcmp(m.time, "19:42"), "time");
	expect(!strcmp(m.cpu_temp, "48"), "temp_c alias");
}

static void test_page_from_id_all(void)
{
	expect(router_data_page_from_id("router_system") == ROUTER_PAGE_SYSTEM,
	       "system page");
	expect(router_data_page_from_id("router_network") == ROUTER_PAGE_NETWORK,
	       "network page");
	expect(router_data_page_from_id("router_clients") == ROUTER_PAGE_CLIENTS,
	       "clients page");
	expect(router_data_page_from_id("router_storage") == ROUTER_PAGE_STORAGE,
	       "storage page");
	expect(router_data_page_from_id("router_wifi") == ROUTER_PAGE_WIFI,
	       "wifi page");
	expect(router_data_page_from_id("router_security") == ROUTER_PAGE_SECURITY,
	       "security page");
	expect(router_data_page_from_id("router_boot") == ROUTER_PAGE_SYSTEM,
	       "unknown boot falls back to system");
	expect(router_data_page_from_id(NULL) == ROUTER_PAGE_SYSTEM, "null page id");
}

static void test_null_apply_and_empty_strings(void)
{
	router_metrics_t m;

	router_data_init(&m);
	router_data_apply_json(NULL, "{}");
	router_data_apply_json(&m, NULL);
	expect(!strcmp(m.hostname, "Router"), "null json no-op");

	router_data_apply_json(&m, "{\"wan_ip\":\"\"}");
	expect(!strcmp(m.wan_ip, "-"), "empty string becomes dash");
}

static void test_push_hist_direct(void)
{
	router_metrics_t m;

	router_data_init(&m);
	strncpy(m.cpu, "75", sizeof(m.cpu) - 1);
	m.ram_pct = 33;
	router_data_push_hist(&m);
	expect(m.hist_len == 1, "direct push hist len");
	expect(m.cpu_hist[0] == 75, "direct push cpu");
	expect(m.ram_hist[0] == 33, "direct push ram");
}

static void test_cpu_hist_clamp(void)
{
	router_metrics_t m;

	router_data_init(&m);
	router_data_apply_json(&m, "{\"cpu\":\"150\",\"ram_pct\":200}");
	expect(m.cpu_hist[0] == 100, "cpu hist clamped");
	expect(m.ram_hist[0] == 100, "ram hist clamped");
}

int main(void)
{
	test_init_defaults();
	test_parse_system();
	test_cpu_temp_key_order();
	test_hist_ring();
	test_network_does_not_touch_uart_link_or_hist();
	test_network_ports_and_ping();
	test_wifi_payload();
	test_clients_payload();
	test_storage_payload();
	test_storage_legacy_array_fallback();
	test_security_payload();
	test_uptime_temp_alias_and_rdcp_wrapper();
	test_page_from_id_all();
	test_null_apply_and_empty_strings();
	test_push_hist_direct();
	test_cpu_hist_clamp();

	printf(tests_failed ? "FAILED\n" : "OK\n");
	return tests_failed ? 1 : 0;
}
