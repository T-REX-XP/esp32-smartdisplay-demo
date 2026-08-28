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

static const char *json_find_key(const char *json, const char *key)
{
	char pattern[48];
	const char *p, *colon;

	if (!json || !key)
		return NULL;
	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	p = json;
	while ((p = strstr(p, pattern)) != NULL) {
		colon = p + strlen(pattern);
		while (*colon == ' ' || *colon == '\t')
			colon++;
		if (*colon == ':')
			return colon + 1;
		p++;
	}
	return NULL;
}

static const char *json_str(const char *json, const char *key, char *buf, size_t len)
{
	const char *p, *src;
	size_t o = 0;

	if (!json || !key || !buf || !len)
		return NULL;
	p = json_find_key(json, key);
	if (!p)
		return NULL;
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p != '"')
		return NULL;
	src = p + 1;
	while (*src && *src != '"' && o + 1 < len) {
		if (*src == '\\' && src[1]) {
			src++;
			buf[o++] = *src++;
			continue;
		}
		buf[o++] = *src++;
	}
	while (*src && *src != '"') {
		if (*src == '\\' && src[1])
			src += 2;
		else
			src++;
	}
	if (*src != '"')
		return NULL;
	buf[o] = '\0';
	return buf;
}

static int json_int(const char *json, const char *key, int *out)
{
	const char *p;

	p = json_find_key(json, key);
	if (!p || !out)
		return 0;
	*out = (int)strtol(p, NULL, 10);
	return 1;
}

static int json_bool(const char *json, const char *key, int *out)
{
	const char *p;

	p = json_find_key(json, key);
	if (!p || !out)
		return 0;
	while (*p == ' ' || *p == '\t')
		p++;
	*out = (p[0] == 't' || p[0] == 'T' || p[0] == '1');
	return 1;
}

static unsigned json_uint(const char *json, const char *key)
{
	int v = 0;

	if (!json_int(json, key, &v) || v < 0)
		return 0;
	return (unsigned)v;
}

void router_data_init(router_metrics_t *m)
{
	if (!m)
		return;
	memset(m, 0, sizeof(*m));
	set_str(m->hostname, ROUTER_STR_LEN, "Router");
	set_str(m->cpu, ROUTER_STR_LEN, "0");
	set_str(m->cpu_temp, ROUTER_STR_LEN, NULL);
	set_str(m->wan_ip, ROUTER_STR_LEN, NULL);
	set_str(m->wan_dev, sizeof(m->wan_dev), "eth0");
	set_str(m->rx_rate, ROUTER_STR_LEN, NULL);
	set_str(m->tx_rate, ROUTER_STR_LEN, NULL);
	set_str(m->wifi_ssid, ROUTER_STR_LEN, NULL);
	set_str(m->wifi_enc, sizeof(m->wifi_enc), NULL);
	set_str(m->wifi_ap_state, ROUTER_STR_LEN, NULL);
	set_str(m->firewall_state, ROUTER_STR_LEN, "unknown");
	set_str(m->root_usage, ROUTER_STR_LEN, NULL);
	set_str(m->data_usage, ROUTER_STR_LEN, "none");
	set_str(m->data_kind, sizeof(m->data_kind), "none");
	set_str(m->swap_usage, ROUTER_STR_LEN, "off");
	set_str(m->wifi_24, ROUTER_STR_LEN, "0");
	set_str(m->wifi_5, ROUTER_STR_LEN, "0");
	set_str(m->lan_clients, ROUTER_STR_LEN, "0");
	set_str(m->clients_total, ROUTER_STR_LEN, "0 clients");
	set_str(m->dhcp_leases, ROUTER_STR_LEN, "0");
	set_str(m->dhcp_summary, ROUTER_STR_LEN, "no leases");
	m->dhcp_pool = 150;
	m->link_ok = false;
	m->ping_ms = -1;
	m->ping_ok = false;
	set_str(m->eth0_role, sizeof(m->eth0_role), "WAN");
	set_str(m->eth1_role, sizeof(m->eth1_role), "LAN");
	set_str(m->eth2_role, sizeof(m->eth2_role), "LAN");
	set_str(m->eth0_speed, sizeof(m->eth0_speed), NULL);
	set_str(m->eth1_speed, sizeof(m->eth1_speed), NULL);
	set_str(m->eth2_speed, sizeof(m->eth2_speed), NULL);
}

void router_data_push_hist(router_metrics_t *m)
{
	unsigned idx;
	int cpu;

	if (!m)
		return;
	cpu = atoi(m->cpu);
	if (cpu < 0)
		cpu = 0;
	if (cpu > 100)
		cpu = 100;

	idx = m->hist_head % ROUTER_HIST_LEN;
	m->cpu_hist[idx] = (uint8_t)cpu;
	m->ram_hist[idx] = (uint8_t)(m->ram_pct > 100 ? 100 : m->ram_pct);
	m->hist_head = (m->hist_head + 1) % ROUTER_HIST_LEN;
	if (m->hist_len < ROUTER_HIST_LEN)
		m->hist_len++;
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
	if (json_find_key(obj, "ram_pct"))
		m->ram_pct = json_uint(obj, "ram_pct");

	/* System scope only — do not append history from other pages. */
	if (json_find_key(obj, "cpu") && json_find_key(obj, "ram_pct"))
		router_data_push_hist(m);

	if (json_str(obj, "wan_ip", tmp, sizeof(tmp)))
		set_str(m->wan_ip, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "wan_dev", tmp, sizeof(tmp)))
		set_str(m->wan_dev, sizeof(m->wan_dev), tmp);
	if (json_str(obj, "rx_rate", tmp, sizeof(tmp)))
		set_str(m->rx_rate, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "tx_rate", tmp, sizeof(tmp)))
		set_str(m->tx_rate, ROUTER_STR_LEN, tmp);
	if (json_int(obj, "ping_ms", &m->ping_ms))
		;
	{
		int b = 0;
		if (json_bool(obj, "ping_ok", &b))
			m->ping_ok = b != 0;
		if (json_str(obj, "eth0_role", tmp, sizeof(tmp)))
			set_str(m->eth0_role, sizeof(m->eth0_role), tmp);
		if (json_str(obj, "eth0_speed", tmp, sizeof(tmp)))
			set_str(m->eth0_speed, sizeof(m->eth0_speed), tmp);
		if (json_bool(obj, "eth0_up", &b))
			m->eth0_up = b != 0;
		if (json_str(obj, "eth1_role", tmp, sizeof(tmp)))
			set_str(m->eth1_role, sizeof(m->eth1_role), tmp);
		if (json_str(obj, "eth1_speed", tmp, sizeof(tmp)))
			set_str(m->eth1_speed, sizeof(m->eth1_speed), tmp);
		if (json_bool(obj, "eth1_up", &b))
			m->eth1_up = b != 0;
		if (json_str(obj, "eth2_role", tmp, sizeof(tmp)))
			set_str(m->eth2_role, sizeof(m->eth2_role), tmp);
		if (json_str(obj, "eth2_speed", tmp, sizeof(tmp)))
			set_str(m->eth2_speed, sizeof(m->eth2_speed), tmp);
		if (json_bool(obj, "eth2_up", &b))
			m->eth2_up = b != 0;
	}

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
	if (json_str(obj, "dhcp_summary", tmp, sizeof(tmp)))
		set_str(m->dhcp_summary, ROUTER_STR_LEN, tmp);
	if (json_find_key(obj, "dhcp_pool"))
		m->dhcp_pool = json_uint(obj, "dhcp_pool");
	if (json_find_key(obj, "dhcp_pct"))
		m->dhcp_pct = json_uint(obj, "dhcp_pct");

	if (json_str(obj, "root_usage", tmp, sizeof(tmp)))
		set_str(m->root_usage, ROUTER_STR_LEN, tmp);
	if (json_find_key(obj, "root_pct"))
		m->root_pct = json_uint(obj, "root_pct");
	if (json_str(obj, "root_dev", tmp, sizeof(tmp)))
		set_str(m->root_dev, sizeof(m->root_dev), tmp);
	if (json_str(obj, "data_usage", tmp, sizeof(tmp)))
		set_str(m->data_usage, ROUTER_STR_LEN, tmp);
	if (json_find_key(obj, "data_pct"))
		m->data_pct = json_uint(obj, "data_pct");
	if (json_str(obj, "data_kind", tmp, sizeof(tmp)))
		set_str(m->data_kind, sizeof(m->data_kind), tmp);
	if (json_str(obj, "overlay_dev", tmp, sizeof(tmp)))
		set_str(m->overlay_dev, sizeof(m->overlay_dev), tmp);
	if (json_str(obj, "swap_usage", tmp, sizeof(tmp)))
		set_str(m->swap_usage, ROUTER_STR_LEN, tmp);
	if (json_find_key(obj, "swap_pct"))
		m->swap_pct = json_uint(obj, "swap_pct");

	if (json_str(obj, "wifi_ssid", tmp, sizeof(tmp)))
		set_str(m->wifi_ssid, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "wifi_ap_state", tmp, sizeof(tmp)))
		set_str(m->wifi_ap_state, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "wifi_enc", tmp, sizeof(tmp)))
		set_str(m->wifi_enc, sizeof(m->wifi_enc), tmp);
	if (json_find_key(obj, "wifi_qr"))
		json_str(obj, "wifi_qr", m->wifi_qr, sizeof(m->wifi_qr));

	if (json_str(obj, "firewall_state", tmp, sizeof(tmp)))
		set_str(m->firewall_state, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "blocked_24h", tmp, sizeof(tmp)))
		set_str(m->blocked_24h, ROUTER_STR_LEN, tmp);
	if (json_str(obj, "vpn_tunnels", tmp, sizeof(tmp)))
		set_str(m->vpn_tunnels, ROUTER_STR_LEN, tmp);
	if (json_find_key(obj, "blocky_blocked"))
		m->blocky_blocked = json_uint(obj, "blocky_blocked");
	if (json_find_key(obj, "banip_blocked"))
		m->banip_blocked = json_uint(obj, "banip_blocked");

	/* UART link_ok is owned by firmware (RX silence), not host JSON. */

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
