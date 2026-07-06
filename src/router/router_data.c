#include "router_data.h"

#include "router_pages.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_str(char *dst, size_t len, const char *val)
{
	if (!dst || !len)
		return;
	if (!val || !val[0]) {
		dst[0] = '-';
		dst[1] = '\0';
		return;
	}
	strncpy(dst, val, len - 1);
	dst[len - 1] = '\0';
}

static const char *json_str(const char *json, const char *key, char *buf, size_t len)
{
	char pattern[40];
	const char *p, *start, *end;

	if (!json || !key || !buf || !len)
		return NULL;
	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	p = strstr(json, pattern);
	if (!p)
		return NULL;
	p = strchr(p + strlen(pattern), '"');
	if (!p)
		return NULL;
	start = p + 1;
	end = strchr(start, '"');
	if (!end || (size_t)(end - start) >= len)
		return NULL;
	memcpy(buf, start, (size_t)(end - start));
	buf[end - start] = '\0';
	return buf;
}

static unsigned json_uint(const char *json, const char *key)
{
	char tmp[24];

	if (!json_str(json, key, tmp, sizeof(tmp)))
		return 0;
	return (unsigned)strtoul(tmp, NULL, 10);
}

void router_data_init(router_metrics_t *m)
{
	if (!m)
		return;
	memset(m, 0, sizeof(*m));
	set_str(m->hostname, ROUTER_STR_LEN, "Router");
	set_str(m->cpu, ROUTER_STR_LEN, "0");
	set_str(m->wan_ip, ROUTER_STR_LEN, NULL);
	set_str(m->wifi_ssid, ROUTER_STR_LEN, NULL);
	set_str(m->firewall_state, ROUTER_STR_LEN, "unknown");
}

static void merge_object(router_metrics_t *m, const char *obj)
{
	char tmp[ROUTER_STR_LEN];

	if (json_str(obj, "hostname", tmp, sizeof(tmp)))
		set_str(m->hostname, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "time", tmp, sizeof(tmp)))
		set_str(m->time, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "cpu", tmp, sizeof(tmp)))
		set_str(m->cpu, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "cpu_temp", tmp, sizeof(tmp)) || json_str(obj, "temp_c", tmp, sizeof(tmp)))
		set_str(m->cpu_temp, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "ram_used", tmp, sizeof(tmp)))
		set_str(m->ram_used, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "uptime_short", tmp, sizeof(tmp)))
		set_str(m->uptime_short, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "load_short", tmp, sizeof(tmp)))
		set_str(m->load_short, ROUTER_STR_LEN, tmp);
	if (strstr(obj, "\"ram_pct\""))
		m->ram_pct = json_uint(obj, "ram_pct");

	if (json_str(obj, "wan_ip", tmp, sizeof(tmp)))
		set_str(m->wan_ip, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "rx_rate", tmp, sizeof(tmp)))
		set_str(m->rx_rate, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "tx_rate", tmp, sizeof(tmp)))
		set_str(m->tx_rate, ROUTER_STR_LEN, tmp);
	if (strstr(obj, "\"ping_ms\""))
		m->ping_ms = (int)json_uint(obj, "ping_ms");

	if (json_str(obj, "wifi_24", tmp, sizeof(tmp)))
		set_str(m->wifi_24, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "wifi_5", tmp, sizeof(tmp)))
		set_str(m->wifi_5, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "lan_clients", tmp, sizeof(tmp)))
		set_str(m->lan_clients, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "clients_total", tmp, sizeof(tmp)))
		set_str(m->clients_total, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "dhcp_leases", tmp, sizeof(tmp)))
		set_str(m->dhcp_leases, ROUTER_STR_LEN, tmp);
	if (strstr(obj, "\"dhcp_pct\""))
		m->dhcp_pct = json_uint(obj, "dhcp_pct");

	if (json_str(obj, "root_usage", tmp, sizeof(tmp)))
		set_str(m->root_usage, ROUTER_STR_LEN, tmp);
	if (strstr(obj, "\"root_pct\""))
		m->root_pct = json_uint(obj, "root_pct");
	if (json_str(obj, "data_usage", tmp, sizeof(tmp)))
		set_str(m->data_usage, ROUTER_STR_LEN, tmp);
	if (strstr(obj, "\"data_pct\""))
		m->data_pct = json_uint(obj, "data_pct");
	if (json_str(obj, "swap_usage", tmp, sizeof(tmp)))
		set_str(m->swap_usage, ROUTER_STR_LEN, tmp);

	if (json_str(obj, "wifi_ssid", tmp, sizeof(tmp)))
		set_str(m->wifi_ssid, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "wifi_ap_state", tmp, sizeof(tmp)))
		set_str(m->wifi_ap_state, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "wifi_qr", tmp, sizeof(tmp)))
		set_str(m->wifi_qr, sizeof(m->wifi_qr), tmp);

	if (json_str(obj, "firewall_state", tmp, sizeof(tmp)))
		set_str(m->firewall_state, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "blocked_24h", tmp, sizeof(tmp)))
		set_str(m->blocked_24h, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "vpn_tunnels", tmp, sizeof(tmp)))
		set_str(m->vpn_tunnels, ROUTER_STR_LEN, tmp);

	if (strstr(obj, "\"link_ok\"")) {
		const char *lk = strstr(obj, "\"link_ok\":true");
		m->link_ok = lk != NULL;
	}

	/* storage[] first entry used for root bar when dedicated keys absent */
	if (!m->root_pct && strstr(obj, "\"storage\"")) {
		const char *entry = strstr(obj, "\"used_percent\"");
		if (entry) {
			const char *q = strchr(entry, ':');
			if (q) {
				unsigned u = (unsigned)strtoul(q + 1, NULL, 10);
				m->root_pct = u > 100 ? 100 : u;
			}
		}
	}
}

void router_data_apply_json(router_metrics_t *m, const char *json)
{
	const char *data;

	if (!m || !json)
		return;

	m->last_rx_ms = 0; /* caller sets millis */

	data = strstr(json, "\"data\":");
	if (data)
		merge_object(m, data);
	else
		merge_object(m, json);
}

router_page_t router_data_page_from_id(const char *screen_id)
{
	int i;

	if (!screen_id)
		return ROUTER_PAGE_SYSTEM;
	for (i = 0; i < ROUTER_PAGE_COUNT; i++) {
		if (!strcmp(screen_id, router_page_id((router_page_t)i)))
			return (router_page_t)i;
	}
	return ROUTER_PAGE_SYSTEM;
}
