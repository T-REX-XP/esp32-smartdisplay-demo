/*
 * Live metrics from mcudd (JSON / RDCP res data).
 */

#ifndef ROUTER_DATA_H
#define ROUTER_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "router_pages.h"

#define ROUTER_STR_LEN 48

typedef struct {
	char hostname[ROUTER_STR_LEN];
	char time[ROUTER_STR_LEN];
	char cpu[ROUTER_STR_LEN];
	char cpu_temp[ROUTER_STR_LEN];
	char ram_used[ROUTER_STR_LEN];
	unsigned ram_pct;
	char uptime_short[ROUTER_STR_LEN];
	char load_short[ROUTER_STR_LEN];

	char wan_ip[ROUTER_STR_LEN];
	char rx_rate[ROUTER_STR_LEN];
	char tx_rate[ROUTER_STR_LEN];
	int ping_ms;

	char wifi_24[ROUTER_STR_LEN];
	char wifi_5[ROUTER_STR_LEN];
	char lan_clients[ROUTER_STR_LEN];
	char clients_total[ROUTER_STR_LEN];
	char dhcp_leases[ROUTER_STR_LEN];
	unsigned dhcp_pct;

	char root_usage[ROUTER_STR_LEN];
	unsigned root_pct;
	char data_usage[ROUTER_STR_LEN];
	unsigned data_pct;
	char swap_usage[ROUTER_STR_LEN];

	char wifi_ssid[ROUTER_STR_LEN];
	char wifi_ap_state[ROUTER_STR_LEN];
	char wifi_qr[128];

	char firewall_state[ROUTER_STR_LEN];
	char blocked_24h[ROUTER_STR_LEN];
	char vpn_tunnels[ROUTER_STR_LEN];

	bool link_ok;
	unsigned long last_rx_ms;
} router_metrics_t;

void router_data_init(router_metrics_t *m);
void router_data_apply_json(router_metrics_t *m, const char *json);
router_page_t router_data_page_from_id(const char *screen_id);

#ifdef __cplusplus
}
#endif

#endif /* ROUTER_DATA_H */
