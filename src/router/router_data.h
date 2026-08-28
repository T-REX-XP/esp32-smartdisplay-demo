/*
 * Live metrics from mcudd (JSON / RDCP res data).
 */

#ifndef ROUTER_DATA_H
#define ROUTER_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "router_pages.h"

#define ROUTER_STR_LEN 48
/* ~60s of history at 1.5s system poll interval */
#define ROUTER_HIST_LEN 40

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
	char wan_dev[16];
	char rx_rate[ROUTER_STR_LEN];
	char tx_rate[ROUTER_STR_LEN];
	int ping_ms;
	int ping_ok;
	char eth0_role[8];
	char eth0_speed[12];
	bool eth0_up;
	char eth1_role[8];
	char eth1_speed[12];
	bool eth1_up;
	char eth2_role[8];
	char eth2_speed[12];
	bool eth2_up;

	char wifi_24[ROUTER_STR_LEN];
	char wifi_5[ROUTER_STR_LEN];
	char lan_clients[ROUTER_STR_LEN];
	char clients_total[ROUTER_STR_LEN];
	char dhcp_leases[ROUTER_STR_LEN];
	char dhcp_summary[ROUTER_STR_LEN];
	unsigned dhcp_pool;
	unsigned dhcp_pct;

	char root_usage[ROUTER_STR_LEN];
	unsigned root_pct;
	char root_dev[16];
	char data_usage[ROUTER_STR_LEN];
	unsigned data_pct;
	char data_kind[16];
	char overlay_dev[24];
	char swap_usage[ROUTER_STR_LEN];
	unsigned swap_pct;

	char wifi_ssid[ROUTER_STR_LEN];
	char wifi_ap_state[ROUTER_STR_LEN];
	char wifi_enc[16];
	char wifi_qr[160];

	char firewall_state[ROUTER_STR_LEN];
	char blocked_24h[ROUTER_STR_LEN];
	char vpn_tunnels[ROUTER_STR_LEN];
	unsigned blocky_blocked;
	unsigned banip_blocked;

	bool link_ok;
	unsigned long last_rx_ms;

	uint8_t cpu_hist[ROUTER_HIST_LEN];
	uint8_t ram_hist[ROUTER_HIST_LEN];
	unsigned hist_len;
	unsigned hist_head;
} router_metrics_t;

void router_data_init(router_metrics_t *m);
void router_data_apply_json(router_metrics_t *m, const char *json);
void router_data_push_hist(router_metrics_t *m);
router_page_t router_data_page_from_id(const char *screen_id);

#ifdef __cplusplus
}
#endif

#endif /* ROUTER_DATA_H */
