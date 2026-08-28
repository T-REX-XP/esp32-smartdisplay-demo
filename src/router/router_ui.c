#include "router_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/qrcode/lv_qrcode.h"

#ifndef ROUTER_UI_DARK
#define ROUTER_UI_DARK 0
#endif

LV_IMG_DECLARE(ui_img_sls_logo_png);
#if !ROUTER_UI_DARK
LV_IMG_DECLARE(ui_img_pattern_png);
#endif

#if ROUTER_UI_DARK
/* LuCI luci-theme-bootstrap BootstrapDark-ish */
#define COL_BG lv_color_hex(0x1d1d1d)
#define COL_TEXT lv_color_hex(0xf8f9fa)
#define COL_MUTED lv_color_hex(0x9ca3af)
#define COL_ACCENT lv_color_hex(0x1c7ed6)
#define COL_PANEL lv_color_hex(0x2b3035)
#define COL_WHITE lv_color_hex(0xffffff)
#define COL_OK lv_color_hex(0x51cf66)
#define COL_WARN lv_color_hex(0xff6b6b)
#define COL_QR_LIGHT COL_PANEL
#else
#define COL_BG lv_color_hex(0xffffff)
#define COL_TEXT lv_color_hex(0x000746)
#define COL_MUTED lv_color_hex(0x9c9cd9)
#define COL_ACCENT lv_color_hex(0x293062)
#define COL_PANEL lv_color_hex(0xe8e8f8)
#define COL_WHITE lv_color_hex(0xffffff)
#define COL_OK lv_color_hex(0x2e7d32)
#define COL_WARN lv_color_hex(0xc62828)
#define COL_QR_LIGHT COL_WHITE
#endif

struct router_ui {
	lv_obj_t *boot_scr;
	lv_obj_t *boot_logo;
	lv_obj_t *boot_title_lbl;
	lv_obj_t *boot_msg_lbl;
	lv_obj_t *boot_bar;
	bool on_boot;

	lv_obj_t *screens[ROUTER_PAGE_COUNT];
	router_page_t current;

	lv_obj_t *sys_cpu_arc;
	lv_obj_t *sys_cpu_lbl;
	lv_obj_t *sys_ram_bar;
	lv_obj_t *sys_ram_lbl;
	lv_obj_t *sys_host_lbl;
	lv_obj_t *sys_uptime_lbl;
	lv_obj_t *sys_load_lbl;
	lv_obj_t *sys_temp_lbl;
	lv_obj_t *sys_link_lbl;
	lv_obj_t *sys_chart;
	lv_chart_series_t *sys_ser_cpu;
	lv_chart_series_t *sys_ser_ram;

	lv_obj_t *net_wan_lbl;
	lv_obj_t *net_rx_lbl;
	lv_obj_t *net_tx_lbl;
	lv_obj_t *net_ping_lbl;
	lv_obj_t *net_eth0_lbl;
	lv_obj_t *net_eth0_speed;
	lv_obj_t *net_eth0_badge;
	lv_obj_t *net_eth1_lbl;
	lv_obj_t *net_eth1_speed;
	lv_obj_t *net_eth1_badge;
	lv_obj_t *net_eth2_lbl;
	lv_obj_t *net_eth2_speed;
	lv_obj_t *net_eth2_badge;

	lv_obj_t *cli_total_lbl;
	lv_obj_t *cli_24_lbl;
	lv_obj_t *cli_5_lbl;
	lv_obj_t *cli_lan_lbl;
	lv_obj_t *cli_dhcp_bar;

	lv_obj_t *sto_root_bar;
	lv_obj_t *sto_root_lbl;
	lv_obj_t *sto_data_title;
	lv_obj_t *sto_data_bar;
	lv_obj_t *sto_data_lbl;
	lv_obj_t *sto_swap_bar;
	lv_obj_t *sto_swap_lbl;

	lv_obj_t *wifi_ssid_lbl;
	lv_obj_t *wifi_state_lbl;
	lv_obj_t *wifi_enc_lbl;
	lv_obj_t *wifi_qr;

	lv_obj_t *sec_fw_lbl;
	lv_obj_t *sec_blocked_lbl;
	lv_obj_t *sec_vpn_lbl;

	lv_obj_t *poweroff_panel;
	lv_obj_t *poweroff_title_lbl;
	lv_obj_t *poweroff_count_lbl;
	lv_obj_t *poweroff_hint_lbl;
	bool poweroff_visible;

	const router_metrics_t *last_m;
};

static lv_obj_t *make_screen_bg(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);

	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
#if !ROUTER_UI_DARK
	lv_obj_set_style_bg_image_src(scr, &ui_img_pattern_png, LV_PART_MAIN);
	lv_obj_set_style_bg_image_tiled(scr, true, LV_PART_MAIN);
#endif
	return scr;
}

static lv_obj_t *add_header(lv_obj_t *parent, const char *title, const char *right)
{
	lv_obj_t *bar = lv_obj_create(parent);

	lv_obj_set_size(bar, lv_pct(100), 36);
	lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(bar, COL_ACCENT, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(bar, 6, LV_PART_MAIN);

	lv_obj_t *t = lv_label_create(bar);
	lv_label_set_text(t, title);
	lv_obj_set_style_text_color(t, COL_WHITE, LV_PART_MAIN);
	lv_obj_set_style_text_font(t, &lv_font_montserrat_18, LV_PART_MAIN);
	lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, 0);

	if (right && right[0]) {
		lv_obj_t *r = lv_label_create(bar);
		lv_label_set_text(r, right);
		lv_obj_set_style_text_color(r, COL_WHITE, LV_PART_MAIN);
		lv_obj_set_style_text_font(r, &lv_font_montserrat_14, LV_PART_MAIN);
		lv_obj_align(r, LV_ALIGN_RIGHT_MID, -4, 0);
	}
	return bar;
}

static lv_obj_t *add_body_label(lv_obj_t *parent, const char *text, lv_align_t align, int x, int y)
{
	lv_obj_t *l = lv_label_create(parent);

	lv_label_set_text(l, text);
	lv_obj_set_style_text_color(l, COL_TEXT, LV_PART_MAIN);
	lv_obj_set_style_text_font(l, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_align(l, align, x, y);
	return l;
}

static lv_obj_t *add_metric_card(lv_obj_t *parent, const char *title, int y,
				 lv_obj_t **title_lbl)
{
	lv_obj_t *card = lv_obj_create(parent);

	lv_obj_set_size(card, lv_pct(92), 56);
	lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y);
	lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(card, COL_PANEL, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
	lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(card, 8, LV_PART_MAIN);

	lv_obj_t *t = lv_label_create(card);
	lv_label_set_text(t, title);
	lv_obj_set_style_text_color(t, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_text_font(t, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);
	if (title_lbl)
		*title_lbl = t;
	return card;
}

static lv_color_t cpu_arc_color(int pct)
{
	if (pct >= 85)
		return COL_WARN;
	if (pct >= 60)
		return lv_color_hex(0xF9A825);
	return COL_OK;
}

static void build_system(router_ui_t *ui, lv_obj_t *scr)
{
	lv_obj_t *card;

	ui->sys_link_lbl = add_body_label(scr, "LINK --", LV_ALIGN_TOP_RIGHT, -10, 40);
	lv_obj_set_style_text_color(ui->sys_link_lbl, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->sys_link_lbl, &lv_font_montserrat_14, LV_PART_MAIN);

	ui->sys_cpu_arc = lv_arc_create(scr);
	lv_obj_set_size(ui->sys_cpu_arc, 96, 96);
	lv_obj_align(ui->sys_cpu_arc, LV_ALIGN_TOP_MID, 0, 44);
	lv_arc_set_range(ui->sys_cpu_arc, 0, 100);
	lv_arc_set_value(ui->sys_cpu_arc, 0);
	lv_arc_set_bg_angles(ui->sys_cpu_arc, 135, 45);
	lv_arc_set_angles(ui->sys_cpu_arc, 135, 135);
	lv_obj_remove_flag(ui->sys_cpu_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_color(ui->sys_cpu_arc, COL_OK, LV_PART_INDICATOR);
	lv_obj_set_style_arc_width(ui->sys_cpu_arc, 9, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(ui->sys_cpu_arc, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_arc_width(ui->sys_cpu_arc, 5, LV_PART_MAIN);

	ui->sys_cpu_lbl = add_body_label(scr, "CPU 0%", LV_ALIGN_TOP_MID, 0, 86);
	lv_obj_set_style_text_font(ui->sys_cpu_lbl, &lv_font_montserrat_14, LV_PART_MAIN);

	/* 1-minute CPU (accent) + RAM (muted) sparkline */
	ui->sys_chart = lv_chart_create(scr);
	lv_obj_set_size(ui->sys_chart, lv_pct(92), 36);
	lv_obj_align(ui->sys_chart, LV_ALIGN_TOP_MID, 0, 148);
	lv_obj_set_style_bg_color(ui->sys_chart, COL_PANEL, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(ui->sys_chart, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_radius(ui->sys_chart, 8, LV_PART_MAIN);
	lv_obj_set_style_border_width(ui->sys_chart, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(ui->sys_chart, 4, LV_PART_MAIN);
	lv_chart_set_type(ui->sys_chart, LV_CHART_TYPE_LINE);
	lv_chart_set_point_count(ui->sys_chart, ROUTER_HIST_LEN);
	lv_chart_set_axis_range(ui->sys_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
	lv_chart_set_div_line_count(ui->sys_chart, 0, 0);
	lv_obj_set_style_size(ui->sys_chart, 0, 0, LV_PART_INDICATOR);
	ui->sys_ser_cpu = lv_chart_add_series(ui->sys_chart, COL_ACCENT,
					      LV_CHART_AXIS_PRIMARY_Y);
	ui->sys_ser_ram = lv_chart_add_series(ui->sys_chart, COL_MUTED,
					      LV_CHART_AXIS_PRIMARY_Y);
	lv_obj_add_flag(ui->sys_chart, LV_OBJ_FLAG_HIDDEN);

	card = add_metric_card(scr, "MEMORY", 192, NULL);
	ui->sys_ram_bar = lv_bar_create(card);
	lv_obj_set_size(ui->sys_ram_bar, lv_pct(100), 10);
	lv_obj_align(ui->sys_ram_bar, LV_ALIGN_BOTTOM_MID, 0, -4);
	lv_bar_set_range(ui->sys_ram_bar, 0, 100);
	lv_obj_set_style_bg_color(ui->sys_ram_bar, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ui->sys_ram_bar, COL_ACCENT, LV_PART_INDICATOR);

	ui->sys_ram_lbl = lv_label_create(card);
	lv_label_set_text(ui->sys_ram_lbl, "RAM --");
	lv_obj_set_style_text_color(ui->sys_ram_lbl, COL_TEXT, LV_PART_MAIN);
	lv_obj_align(ui->sys_ram_lbl, LV_ALIGN_BOTTOM_LEFT, 0, -18);

	ui->sys_load_lbl = add_body_label(scr, "load --", LV_ALIGN_BOTTOM_LEFT, 12, -52);
	ui->sys_temp_lbl = add_body_label(scr, "temp --", LV_ALIGN_BOTTOM_RIGHT, -12, -52);
	lv_obj_set_style_text_color(ui->sys_load_lbl, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->sys_temp_lbl, COL_MUTED, LV_PART_MAIN);

	ui->sys_host_lbl = add_body_label(scr, "Router", LV_ALIGN_BOTTOM_LEFT, 12, -28);
	ui->sys_uptime_lbl = add_body_label(scr, "up --", LV_ALIGN_BOTTOM_RIGHT, -12, -28);
	lv_obj_set_style_text_font(ui->sys_host_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->sys_uptime_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
}

static void add_port_row(lv_obj_t *parent, int y, const char *title,
			 lv_obj_t **name_lbl, lv_obj_t **speed_lbl, lv_obj_t **badge)
{
	lv_obj_t *row = lv_obj_create(parent);

	lv_obj_set_size(row, lv_pct(92), 36);
	lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
	lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(row, COL_PANEL, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
	lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(row, 4, LV_PART_MAIN);

	*name_lbl = lv_label_create(row);
	lv_label_set_text(*name_lbl, title);
	lv_obj_set_style_text_color(*name_lbl, COL_TEXT, LV_PART_MAIN);
	lv_obj_set_style_text_font(*name_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_align(*name_lbl, LV_ALIGN_LEFT_MID, 4, 0);

	*speed_lbl = lv_label_create(row);
	lv_label_set_text(*speed_lbl, "--");
	lv_obj_set_style_text_color(*speed_lbl, COL_MUTED, LV_PART_MAIN);
	lv_obj_align(*speed_lbl, LV_ALIGN_CENTER, 20, 0);

	*badge = lv_label_create(row);
	lv_label_set_text(*badge, "DOWN");
	lv_obj_set_style_text_color(*badge, COL_WARN, LV_PART_MAIN);
	lv_obj_set_style_text_font(*badge, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_align(*badge, LV_ALIGN_RIGHT_MID, -4, 0);
}

static void build_network(router_ui_t *ui, lv_obj_t *scr)
{
	lv_obj_t *card = add_metric_card(scr, "WAN", 44, NULL);

	ui->net_wan_lbl = lv_label_create(card);
	lv_label_set_text(ui->net_wan_lbl, "--");
	lv_obj_set_style_text_color(ui->net_wan_lbl, COL_ACCENT, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->net_wan_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
	lv_obj_align(ui->net_wan_lbl, LV_ALIGN_CENTER, 0, 6);

	ui->net_rx_lbl = add_body_label(scr, "RX --", LV_ALIGN_TOP_LEFT, 14, 108);
	ui->net_tx_lbl = add_body_label(scr, "TX --", LV_ALIGN_TOP_LEFT, 14, 128);
	ui->net_ping_lbl = add_body_label(scr, "PING --", LV_ALIGN_TOP_RIGHT, -14, 108);
	lv_obj_set_style_text_font(ui->net_rx_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->net_tx_lbl, &lv_font_montserrat_14, LV_PART_MAIN);

	add_port_row(scr, 160, "eth0 WAN", &ui->net_eth0_lbl, &ui->net_eth0_speed,
		     &ui->net_eth0_badge);
	add_port_row(scr, 200, "eth1 LAN", &ui->net_eth1_lbl, &ui->net_eth1_speed,
		     &ui->net_eth1_badge);
	add_port_row(scr, 240, "eth2 LAN", &ui->net_eth2_lbl, &ui->net_eth2_speed,
		     &ui->net_eth2_badge);
}

static void build_clients(router_ui_t *ui, lv_obj_t *scr)
{
	lv_obj_t *c;

	c = add_metric_card(scr, "2.4 GHz", 52, NULL);
	ui->cli_24_lbl = lv_label_create(c);
	lv_label_set_text(ui->cli_24_lbl, "0");
	lv_obj_set_style_text_font(ui->cli_24_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->cli_24_lbl, COL_ACCENT, LV_PART_MAIN);
	lv_obj_align(ui->cli_24_lbl, LV_ALIGN_CENTER, 0, 8);

	c = add_metric_card(scr, "5 GHz", 118, NULL);
	ui->cli_5_lbl = lv_label_create(c);
	lv_label_set_text(ui->cli_5_lbl, "0");
	lv_obj_set_style_text_font(ui->cli_5_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->cli_5_lbl, COL_ACCENT, LV_PART_MAIN);
	lv_obj_align(ui->cli_5_lbl, LV_ALIGN_CENTER, 0, 8);

	c = add_metric_card(scr, "LAN / DHCP", 184, NULL);
	ui->cli_lan_lbl = lv_label_create(c);
	lv_label_set_text(ui->cli_lan_lbl, "DHCP 0/150");
	lv_obj_set_style_text_color(ui->cli_lan_lbl, COL_TEXT, LV_PART_MAIN);
	lv_obj_align(ui->cli_lan_lbl, LV_ALIGN_TOP_LEFT, 0, 16);

	ui->cli_total_lbl = lv_label_create(c);
	lv_label_set_text(ui->cli_total_lbl, "no leases");
	lv_obj_set_style_text_color(ui->cli_total_lbl, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->cli_total_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_label_set_long_mode(ui->cli_total_lbl, LV_LABEL_LONG_DOT);
	lv_obj_set_width(ui->cli_total_lbl, 110);
	lv_obj_align(ui->cli_total_lbl, LV_ALIGN_TOP_RIGHT, 0, 16);

	ui->cli_dhcp_bar = lv_bar_create(c);
	lv_obj_set_size(ui->cli_dhcp_bar, lv_pct(100), 8);
	lv_obj_align(ui->cli_dhcp_bar, LV_ALIGN_BOTTOM_MID, 0, -4);
	lv_bar_set_range(ui->cli_dhcp_bar, 0, 100);
	lv_obj_set_style_bg_color(ui->cli_dhcp_bar, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ui->cli_dhcp_bar, COL_ACCENT, LV_PART_INDICATOR);
}

static void build_storage(router_ui_t *ui, lv_obj_t *scr)
{
	lv_obj_t *c;

	c = add_metric_card(scr, "ROOT", 44, NULL);
	ui->sto_root_lbl = lv_label_create(c);
	lv_label_set_text(ui->sto_root_lbl, "--");
	lv_obj_set_style_text_color(ui->sto_root_lbl, COL_TEXT, LV_PART_MAIN);
	lv_obj_align(ui->sto_root_lbl, LV_ALIGN_TOP_LEFT, 0, 16);
	ui->sto_root_bar = lv_bar_create(c);
	lv_obj_set_size(ui->sto_root_bar, lv_pct(100), 10);
	lv_obj_align(ui->sto_root_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
	lv_bar_set_range(ui->sto_root_bar, 0, 100);
	lv_obj_set_style_bg_color(ui->sto_root_bar, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ui->sto_root_bar, COL_ACCENT, LV_PART_INDICATOR);

	c = add_metric_card(scr, "DATA", 110, &ui->sto_data_title);
	ui->sto_data_lbl = lv_label_create(c);
	lv_label_set_text(ui->sto_data_lbl, "--");
	lv_obj_set_style_text_color(ui->sto_data_lbl, COL_TEXT, LV_PART_MAIN);
	lv_obj_align(ui->sto_data_lbl, LV_ALIGN_TOP_LEFT, 0, 16);
	ui->sto_data_bar = lv_bar_create(c);
	lv_obj_set_size(ui->sto_data_bar, lv_pct(100), 10);
	lv_obj_align(ui->sto_data_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
	lv_bar_set_range(ui->sto_data_bar, 0, 100);
	lv_obj_set_style_bg_color(ui->sto_data_bar, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ui->sto_data_bar, COL_ACCENT, LV_PART_INDICATOR);

	c = add_metric_card(scr, "SWAP", 176, NULL);
	ui->sto_swap_lbl = lv_label_create(c);
	lv_label_set_text(ui->sto_swap_lbl, "off");
	lv_obj_set_style_text_color(ui->sto_swap_lbl, COL_TEXT, LV_PART_MAIN);
	lv_obj_align(ui->sto_swap_lbl, LV_ALIGN_TOP_LEFT, 0, 16);
	ui->sto_swap_bar = lv_bar_create(c);
	lv_obj_set_size(ui->sto_swap_bar, lv_pct(100), 10);
	lv_obj_align(ui->sto_swap_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
	lv_bar_set_range(ui->sto_swap_bar, 0, 100);
	lv_obj_set_style_bg_color(ui->sto_swap_bar, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ui->sto_swap_bar, COL_ACCENT, LV_PART_INDICATOR);
}

static void build_wifi(router_ui_t *ui, lv_obj_t *scr)
{
	ui->wifi_ssid_lbl = add_body_label(scr, "SSID", LV_ALIGN_TOP_LEFT, 14, 48);
	lv_obj_set_style_text_font(ui->wifi_ssid_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
	lv_label_set_long_mode(ui->wifi_ssid_lbl, LV_LABEL_LONG_DOT);
	lv_obj_set_width(ui->wifi_ssid_lbl, 110);

	ui->wifi_enc_lbl = add_body_label(scr, "--", LV_ALIGN_TOP_LEFT, 14, 74);
	lv_obj_set_style_text_color(ui->wifi_enc_lbl, COL_MUTED, LV_PART_MAIN);

	ui->wifi_state_lbl = add_body_label(scr, "AP --", LV_ALIGN_TOP_LEFT, 14, 98);
	lv_obj_set_style_text_color(ui->wifi_state_lbl, COL_MUTED, LV_PART_MAIN);

	ui->wifi_qr = lv_qrcode_create(scr);
	lv_qrcode_set_size(ui->wifi_qr, 128);
	lv_obj_align(ui->wifi_qr, LV_ALIGN_BOTTOM_RIGHT, -12, -36);
	lv_qrcode_set_dark_color(ui->wifi_qr, COL_TEXT);
	lv_qrcode_set_light_color(ui->wifi_qr, COL_QR_LIGHT);

	add_body_label(scr, "Scan to join", LV_ALIGN_BOTTOM_LEFT, 14, -20);
}

static void build_security(router_ui_t *ui, lv_obj_t *scr)
{
	lv_obj_t *c = add_metric_card(scr, "Firewall", 52, NULL);

	ui->sec_fw_lbl = lv_label_create(c);
	lv_label_set_text(ui->sec_fw_lbl, "unknown");
	lv_obj_set_style_text_font(ui->sec_fw_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->sec_fw_lbl, COL_ACCENT, LV_PART_MAIN);
	lv_label_set_long_mode(ui->sec_fw_lbl, LV_LABEL_LONG_DOT);
	lv_obj_set_width(ui->sec_fw_lbl, lv_pct(100));
	lv_obj_align(ui->sec_fw_lbl, LV_ALIGN_CENTER, 0, 8);

	c = add_metric_card(scr, "Threats / VPN", 130, NULL);
	ui->sec_blocked_lbl = lv_label_create(c);
	lv_label_set_text(ui->sec_blocked_lbl, "Blocked 24h: --");
	lv_obj_align(ui->sec_blocked_lbl, LV_ALIGN_TOP_LEFT, 0, 14);
	ui->sec_vpn_lbl = lv_label_create(c);
	lv_label_set_text(ui->sec_vpn_lbl, "VPN: --");
	lv_obj_align(ui->sec_vpn_lbl, LV_ALIGN_BOTTOM_LEFT, 0, -4);
}

static void build_boot(router_ui_t *ui)
{
	lv_obj_t *scr = make_screen_bg();

	add_header(scr, "BOOTING", NULL);

	ui->boot_logo = lv_image_create(scr);
	lv_image_set_src(ui->boot_logo, &ui_img_sls_logo_png);
	lv_obj_align(ui->boot_logo, LV_ALIGN_CENTER, 0, -36);

	ui->boot_title_lbl = lv_label_create(scr);
	lv_label_set_text(ui->boot_title_lbl, "Router Display");
	lv_obj_set_style_text_font(ui->boot_title_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->boot_title_lbl, COL_TEXT, LV_PART_MAIN);
	lv_obj_align(ui->boot_title_lbl, LV_ALIGN_CENTER, 0, 48);

	ui->boot_msg_lbl = lv_label_create(scr);
	lv_label_set_text(ui->boot_msg_lbl, "Starting...");
	lv_obj_set_style_text_font(ui->boot_msg_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->boot_msg_lbl, COL_MUTED, LV_PART_MAIN);
	lv_obj_align(ui->boot_msg_lbl, LV_ALIGN_CENTER, 0, 76);
	lv_label_set_long_mode(ui->boot_msg_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(ui->boot_msg_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
	lv_obj_set_width(ui->boot_msg_lbl, lv_pct(88));

	ui->boot_bar = lv_bar_create(scr);
	lv_obj_set_size(ui->boot_bar, lv_pct(80), 12);
	lv_obj_align(ui->boot_bar, LV_ALIGN_BOTTOM_MID, 0, -24);
	lv_bar_set_range(ui->boot_bar, 0, 100);
	lv_bar_set_value(ui->boot_bar, 0, LV_ANIM_OFF);
	lv_obj_set_style_bg_color(ui->boot_bar, COL_PANEL, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ui->boot_bar, COL_ACCENT, LV_PART_INDICATOR);

	ui->boot_scr = scr;
}

void router_ui_install_swipe(lv_obj_t *scr, lv_event_cb_t cb)
{
	lv_obj_t *layer;

	if (!scr || !cb)
		return;

	layer = lv_obj_create(scr);
	lv_obj_set_size(layer, lv_pct(100), lv_pct(100));
	lv_obj_align(layer, LV_ALIGN_TOP_LEFT, 0, 0);
	lv_obj_remove_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(layer, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_bg_opa(layer, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(layer, 0, LV_PART_MAIN);
	lv_obj_add_event_cb(layer, cb, LV_EVENT_PRESSED, NULL);
	lv_obj_add_event_cb(layer, cb, LV_EVENT_RELEASED, NULL);
	lv_obj_move_foreground(layer);
}

static void build_page(router_ui_t *ui, router_page_t page)
{
	const char *titles[] = { "SYSTEM", "NETWORK", "CLIENTS", "STORAGE", "WIFI AP", "SECURITY" };
	lv_obj_t *scr;

	scr = make_screen_bg();
	add_header(scr, titles[page], NULL);
	ui->screens[page] = scr;

	switch (page) {
	case ROUTER_PAGE_SYSTEM:
		build_system(ui, scr);
		break;
	case ROUTER_PAGE_NETWORK:
		build_network(ui, scr);
		break;
	case ROUTER_PAGE_CLIENTS:
		build_clients(ui, scr);
		break;
	case ROUTER_PAGE_STORAGE:
		build_storage(ui, scr);
		break;
	case ROUTER_PAGE_WIFI:
		build_wifi(ui, scr);
		break;
	case ROUTER_PAGE_SECURITY:
		build_security(ui, scr);
		break;
	default:
		break;
	}
}

router_ui_t *router_ui_create(void)
{
	router_ui_t *ui = (router_ui_t *)calloc(1, sizeof(*ui));
	int i;

	if (!ui)
		return NULL;
	build_boot(ui);
	ui->on_boot = true;
	lv_screen_load(ui->boot_scr);
	lv_refr_now(lv_disp_get_default());
	for (i = 0; i < ROUTER_PAGE_COUNT; i++)
		build_page(ui, (router_page_t)i);
	ui->current = ROUTER_PAGE_SYSTEM;
	return ui;
}

void router_ui_destroy(router_ui_t *ui)
{
	int i;

	if (!ui)
		return;
	if (ui->boot_scr)
		lv_obj_delete(ui->boot_scr);
	if (ui->poweroff_panel)
		lv_obj_delete(ui->poweroff_panel);
	for (i = 0; i < ROUTER_PAGE_COUNT; i++) {
		if (ui->screens[i])
			lv_obj_delete(ui->screens[i]);
	}
	free(ui);
}

void router_ui_show_boot(router_ui_t *ui)
{
	if (!ui || !ui->boot_scr)
		return;
	ui->on_boot = true;
	lv_screen_load(ui->boot_scr);
}

void router_ui_set_boot_status(router_ui_t *ui, const char *text, unsigned pct)
{
	if (!ui)
		return;
	if (text && ui->boot_msg_lbl)
		lv_label_set_text(ui->boot_msg_lbl, text);
	if (ui->boot_bar) {
		if (pct > 100)
			pct = 100;
		lv_bar_set_value(ui->boot_bar, pct, LV_ANIM_ON);
	}
}

bool router_ui_on_boot(const router_ui_t *ui)
{
	return ui && ui->on_boot;
}

void router_ui_show_page(router_ui_t *ui, router_page_t page, lv_scr_load_anim_t anim)
{
	if (!ui || page < 0 || page >= ROUTER_PAGE_COUNT)
		return;
	ui->on_boot = false;
	ui->current = page;
	lv_screen_load_anim(ui->screens[page], anim, 200, 0, false);
	if (ui->last_m)
		router_ui_refresh(ui, ui->last_m);
}

router_page_t router_ui_current_page(const router_ui_t *ui)
{
	return ui ? ui->current : ROUTER_PAGE_SYSTEM;
}

lv_obj_t *router_ui_screen(const router_ui_t *ui, router_page_t page)
{
	if (!ui || page < 0 || page >= ROUTER_PAGE_COUNT)
		return NULL;
	return ui->screens[page];
}

lv_obj_t *router_ui_boot_screen(router_ui_t *ui)
{
	return ui ? ui->boot_scr : NULL;
}

static int cpu_value(const char *cpu)
{
	int v = cpu ? atoi(cpu) : 0;

	if (v < 0)
		v = 0;
	if (v > 100)
		v = 100;
	return v;
}

static void apply_page_stale(lv_obj_t *scr, bool stale)
{
	uint32_t i, n;
	lv_opa_t opa = stale ? LV_OPA_50 : LV_OPA_COVER;

	if (!scr)
		return;
	n = lv_obj_get_child_count(scr);
	/* child 0 = header; last = swipe overlay */
	if (n < 3)
		return;
	for (i = 1; i + 1 < n; i++)
		lv_obj_set_style_opa(lv_obj_get_child(scr, i), opa, LV_PART_MAIN);
}

static void refresh_system(router_ui_t *ui, const router_metrics_t *m, char *buf,
			   size_t buf_len)
{
	int cpu = cpu_value(m->cpu);

	if (ui->sys_cpu_arc) {
		lv_obj_set_style_arc_color(ui->sys_cpu_arc, cpu_arc_color(cpu),
					   LV_PART_INDICATOR);
		lv_arc_set_value(ui->sys_cpu_arc, cpu);
		lv_arc_set_angles(ui->sys_cpu_arc, 135, 135 + (cpu * 270 / 100));
	}
	if (ui->sys_cpu_lbl) {
		snprintf(buf, buf_len, "CPU %s%%", m->cpu);
		lv_label_set_text(ui->sys_cpu_lbl, buf);
	}
	if (ui->sys_link_lbl) {
		if (m->link_ok) {
			lv_label_set_text(ui->sys_link_lbl, "LINK OK");
			lv_obj_set_style_text_color(ui->sys_link_lbl, COL_OK, LV_PART_MAIN);
		} else if (m->last_rx_ms) {
			lv_label_set_text(ui->sys_link_lbl, "LINK LOST");
			lv_obj_set_style_text_color(ui->sys_link_lbl, COL_WARN, LV_PART_MAIN);
		} else {
			lv_label_set_text(ui->sys_link_lbl, "LINK --");
			lv_obj_set_style_text_color(ui->sys_link_lbl, COL_MUTED, LV_PART_MAIN);
		}
		lv_obj_set_style_opa(ui->sys_link_lbl, LV_OPA_COVER, LV_PART_MAIN);
	}
	if (ui->sys_chart && ui->sys_ser_cpu && ui->sys_ser_ram) {
		unsigned i;

		if (m->hist_len == 0) {
			lv_obj_add_flag(ui->sys_chart, LV_OBJ_FLAG_HIDDEN);
		} else {
			unsigned start = (m->hist_head + ROUTER_HIST_LEN - m->hist_len) %
					 ROUTER_HIST_LEN;

			lv_obj_remove_flag(ui->sys_chart, LV_OBJ_FLAG_HIDDEN);
			for (i = 0; i < ROUTER_HIST_LEN; i++) {
				if (i < m->hist_len) {
					unsigned idx = (start + i) % ROUTER_HIST_LEN;
					lv_chart_set_series_value_by_id(ui->sys_chart,
									 ui->sys_ser_cpu, i,
									 m->cpu_hist[idx]);
					lv_chart_set_series_value_by_id(ui->sys_chart,
									 ui->sys_ser_ram, i,
									 m->ram_hist[idx]);
				} else {
					lv_chart_set_series_value_by_id(ui->sys_chart,
									 ui->sys_ser_cpu, i,
									 LV_CHART_POINT_NONE);
					lv_chart_set_series_value_by_id(ui->sys_chart,
									 ui->sys_ser_ram, i,
									 LV_CHART_POINT_NONE);
				}
			}
			lv_chart_refresh(ui->sys_chart);
		}
	}
	if (ui->sys_ram_bar)
		lv_bar_set_value(ui->sys_ram_bar, m->ram_pct, LV_ANIM_OFF);
	if (ui->sys_ram_lbl) {
		if (m->ram_used[0] && m->ram_used[0] != '-')
			snprintf(buf, buf_len, "RAM %u%% (%s)", m->ram_pct, m->ram_used);
		else
			snprintf(buf, buf_len, "RAM %u%%", m->ram_pct);
		lv_label_set_text(ui->sys_ram_lbl, buf);
	}
	if (ui->sys_load_lbl) {
		if (m->load_short[0] && m->load_short[0] != '-')
			snprintf(buf, buf_len, "load %s", m->load_short);
		else
			snprintf(buf, buf_len, "load --");
		lv_label_set_text(ui->sys_load_lbl, buf);
	}
	if (ui->sys_temp_lbl) {
		if (m->cpu_temp[0] && m->cpu_temp[0] != '-')
			snprintf(buf, buf_len, "temp %sC", m->cpu_temp);
		else
			snprintf(buf, buf_len, "temp --");
		lv_label_set_text(ui->sys_temp_lbl, buf);
	}
	if (ui->sys_host_lbl)
		lv_label_set_text(ui->sys_host_lbl, m->hostname);
	if (ui->sys_uptime_lbl) {
		snprintf(buf, buf_len, "up %s", m->uptime_short);
		lv_label_set_text(ui->sys_uptime_lbl, buf);
	}
}

static void refresh_network(router_ui_t *ui, const router_metrics_t *m, char *buf,
			    size_t buf_len)
{
	if (ui->net_wan_lbl)
		lv_label_set_text(ui->net_wan_lbl, m->wan_ip);
	if (ui->net_rx_lbl) {
		snprintf(buf, buf_len, "RX %s", m->rx_rate);
		lv_label_set_text(ui->net_rx_lbl, buf);
	}
	if (ui->net_tx_lbl) {
		snprintf(buf, buf_len, "TX %s", m->tx_rate);
		lv_label_set_text(ui->net_tx_lbl, buf);
	}
	if (ui->net_ping_lbl) {
		if (m->ping_ms < 0)
			snprintf(buf, buf_len, "PING --");
		else if (!m->ping_ok)
			snprintf(buf, buf_len, "PING fail");
		else
			snprintf(buf, buf_len, "PING %d ms", m->ping_ms);
		lv_label_set_text(ui->net_ping_lbl, buf);
		lv_obj_set_style_text_color(ui->net_ping_lbl,
					    m->ping_ok ? COL_OK : COL_MUTED, LV_PART_MAIN);
	}

	if (ui->net_eth0_speed)
		lv_label_set_text(ui->net_eth0_speed, m->eth0_speed);
	if (ui->net_eth0_badge) {
		lv_label_set_text(ui->net_eth0_badge, m->eth0_up ? "UP" : "DOWN");
		lv_obj_set_style_text_color(ui->net_eth0_badge,
					    m->eth0_up ? COL_OK : COL_WARN, LV_PART_MAIN);
	}
	if (ui->net_eth1_speed)
		lv_label_set_text(ui->net_eth1_speed, m->eth1_speed);
	if (ui->net_eth1_badge) {
		lv_label_set_text(ui->net_eth1_badge, m->eth1_up ? "UP" : "DOWN");
		lv_obj_set_style_text_color(ui->net_eth1_badge,
					    m->eth1_up ? COL_OK : COL_WARN, LV_PART_MAIN);
	}
	if (ui->net_eth2_speed)
		lv_label_set_text(ui->net_eth2_speed, m->eth2_speed);
	if (ui->net_eth2_badge) {
		lv_label_set_text(ui->net_eth2_badge, m->eth2_up ? "UP" : "DOWN");
		lv_obj_set_style_text_color(ui->net_eth2_badge,
					    m->eth2_up ? COL_OK : COL_WARN, LV_PART_MAIN);
	}
}

static void refresh_clients(router_ui_t *ui, const router_metrics_t *m, char *buf,
			    size_t buf_len)
{
	if (ui->cli_24_lbl)
		lv_label_set_text(ui->cli_24_lbl, m->wifi_24);
	if (ui->cli_5_lbl)
		lv_label_set_text(ui->cli_5_lbl, m->wifi_5);
	if (ui->cli_lan_lbl) {
		unsigned pool = m->dhcp_pool ? m->dhcp_pool : 150;

		snprintf(buf, buf_len, "DHCP %s/%u",
			 m->dhcp_leases[0] ? m->dhcp_leases : "0", pool);
		lv_label_set_text(ui->cli_lan_lbl, buf);
	}
	if (ui->cli_total_lbl) {
		if (m->dhcp_summary[0])
			lv_label_set_text(ui->cli_total_lbl, m->dhcp_summary);
		else
			lv_label_set_text(ui->cli_total_lbl, m->clients_total);
	}
	if (ui->cli_dhcp_bar) {
		lv_bar_set_value(ui->cli_dhcp_bar, m->dhcp_pct, LV_ANIM_OFF);
		lv_obj_set_style_bg_color(ui->cli_dhcp_bar,
					  m->dhcp_pct >= 85 ? COL_WARN : COL_ACCENT,
					  LV_PART_INDICATOR);
	}
}

static void refresh_storage(router_ui_t *ui, const router_metrics_t *m, char *buf,
			    size_t buf_len)
{
	if (ui->sto_root_lbl) {
		if (m->root_dev[0] && m->root_dev[0] != '-')
			snprintf(buf, buf_len, "%s  %s", m->root_usage, m->root_dev);
		else
			snprintf(buf, buf_len, "%s", m->root_usage);
		lv_label_set_text(ui->sto_root_lbl, buf);
	}
	if (ui->sto_root_bar) {
		lv_bar_set_value(ui->sto_root_bar, m->root_pct, LV_ANIM_OFF);
		lv_obj_set_style_bg_color(ui->sto_root_bar,
					  m->root_pct >= 85 ? COL_WARN : COL_ACCENT,
					  LV_PART_INDICATOR);
	}
	if (ui->sto_data_title) {
		const char *title = "DATA";

		if (!strcmp(m->data_kind, "overlay"))
			title = "OVERLAY";
		else if (!strcmp(m->data_kind, "extroot"))
			title = "EXTROOT";
		else if (!strcmp(m->data_kind, "emmc"))
			title = "eMMC";
		else if (!strcmp(m->data_kind, "sd"))
			title = "SD";
		else if (!strcmp(m->data_kind, "disk"))
			title = "DISK";
		else if (!strcmp(m->data_kind, "none"))
			title = "DATA";
		lv_label_set_text(ui->sto_data_title, title);
	}
	if (ui->sto_data_lbl) {
		if (!strcmp(m->data_kind, "none") || !m->data_usage[0] ||
		    m->data_usage[0] == '-')
			snprintf(buf, buf_len, "none");
		else if (m->overlay_dev[0] && m->overlay_dev[0] != '-')
			snprintf(buf, buf_len, "%s  %s", m->data_usage, m->overlay_dev);
		else
			snprintf(buf, buf_len, "%s", m->data_usage);
		lv_label_set_text(ui->sto_data_lbl, buf);
	}
	if (ui->sto_data_bar) {
		lv_bar_set_value(ui->sto_data_bar, m->data_pct, LV_ANIM_OFF);
		lv_obj_set_style_bg_color(ui->sto_data_bar,
					  m->data_pct >= 85 ? COL_WARN : COL_ACCENT,
					  LV_PART_INDICATOR);
	}
	if (ui->sto_swap_lbl)
		lv_label_set_text(ui->sto_swap_lbl,
				  m->swap_usage[0] ? m->swap_usage : "off");
	if (ui->sto_swap_bar) {
		lv_bar_set_value(ui->sto_swap_bar, m->swap_pct, LV_ANIM_OFF);
		lv_obj_set_style_bg_color(ui->sto_swap_bar,
					  m->swap_pct >= 85 ? COL_WARN : COL_ACCENT,
					  LV_PART_INDICATOR);
	}
}

static void refresh_wifi(router_ui_t *ui, const router_metrics_t *m, char *buf,
			 size_t buf_len)
{
	if (ui->wifi_ssid_lbl)
		lv_label_set_text(ui->wifi_ssid_lbl, m->wifi_ssid);
	if (ui->wifi_enc_lbl)
		lv_label_set_text(ui->wifi_enc_lbl, m->wifi_enc);
	if (ui->wifi_state_lbl) {
		snprintf(buf, buf_len, "AP %s", m->wifi_ap_state);
		lv_label_set_text(ui->wifi_state_lbl, buf);
		if (!strcmp(m->wifi_ap_state, "up"))
			lv_obj_set_style_text_color(ui->wifi_state_lbl, COL_OK, LV_PART_MAIN);
		else if (!strcmp(m->wifi_ap_state, "disabled"))
			lv_obj_set_style_text_color(ui->wifi_state_lbl, COL_WARN, LV_PART_MAIN);
		else
			lv_obj_set_style_text_color(ui->wifi_state_lbl, COL_MUTED, LV_PART_MAIN);
	}
	if (ui->wifi_qr && m->wifi_qr[0] && m->wifi_qr[0] != '-') {
		lv_obj_remove_flag(ui->wifi_qr, LV_OBJ_FLAG_HIDDEN);
		lv_qrcode_update(ui->wifi_qr, m->wifi_qr, strlen(m->wifi_qr));
	} else if (ui->wifi_qr) {
		lv_obj_add_flag(ui->wifi_qr, LV_OBJ_FLAG_HIDDEN);
	}
}

static void refresh_security(router_ui_t *ui, const router_metrics_t *m, char *buf,
			     size_t buf_len)
{
	if (ui->sec_fw_lbl)
		lv_label_set_text(ui->sec_fw_lbl, m->firewall_state);
	if (ui->sec_blocked_lbl) {
		if (strchr(m->blocked_24h, '+'))
			snprintf(buf, buf_len, "24h %s", m->blocked_24h);
		else
			snprintf(buf, buf_len, "Blocked 24h: %s", m->blocked_24h);
		lv_label_set_text(ui->sec_blocked_lbl, buf);
	}
	if (ui->sec_vpn_lbl) {
		snprintf(buf, buf_len, "VPN %s", m->vpn_tunnels);
		lv_label_set_text(ui->sec_vpn_lbl, buf);
	}
}

void router_ui_refresh(router_ui_t *ui, const router_metrics_t *m)
{
	char buf[ROUTER_STR_LEN * 2 + 16];
	bool stale;

	if (!ui || !m)
		return;

	ui->last_m = m;
	if (ui->on_boot)
		return;

	switch (ui->current) {
	case ROUTER_PAGE_SYSTEM:
		refresh_system(ui, m, buf, sizeof(buf));
		break;
	case ROUTER_PAGE_NETWORK:
		refresh_network(ui, m, buf, sizeof(buf));
		break;
	case ROUTER_PAGE_CLIENTS:
		refresh_clients(ui, m, buf, sizeof(buf));
		break;
	case ROUTER_PAGE_STORAGE:
		refresh_storage(ui, m, buf, sizeof(buf));
		break;
	case ROUTER_PAGE_WIFI:
		refresh_wifi(ui, m, buf, sizeof(buf));
		break;
	case ROUTER_PAGE_SECURITY:
		refresh_security(ui, m, buf, sizeof(buf));
		break;
	default:
		break;
	}

	/* Host gone: keep last numbers, dim them (demo / last-known). */
	stale = !m->link_ok && m->last_rx_ms != 0;
	apply_page_stale(ui->screens[ui->current], stale);
	if (ui->sys_link_lbl)
		lv_obj_set_style_opa(ui->sys_link_lbl, LV_OPA_COVER, LV_PART_MAIN);
}

static void router_ui_build_poweroff_panel(router_ui_t *ui)
{
	char count_buf[8];

	if (!ui || ui->poweroff_panel)
		return;

	ui->poweroff_panel = lv_obj_create(lv_layer_top());
	lv_obj_set_size(ui->poweroff_panel, lv_pct(100), lv_pct(100));
	lv_obj_set_style_bg_color(ui->poweroff_panel, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(ui->poweroff_panel, LV_OPA_90, LV_PART_MAIN);
	lv_obj_set_style_border_width(ui->poweroff_panel, 0, LV_PART_MAIN);
	lv_obj_remove_flag(ui->poweroff_panel, LV_OBJ_FLAG_SCROLLABLE);

	ui->poweroff_title_lbl = lv_label_create(ui->poweroff_panel);
	lv_label_set_text(ui->poweroff_title_lbl, "Turn off device");
	lv_obj_set_style_text_color(ui->poweroff_title_lbl, COL_WHITE, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->poweroff_title_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_align(ui->poweroff_title_lbl, LV_ALIGN_TOP_MID, 0, 48);

	ui->poweroff_count_lbl = lv_label_create(ui->poweroff_panel);
	snprintf(count_buf, sizeof(count_buf), "%u", 5u);
	lv_label_set_text(ui->poweroff_count_lbl, count_buf);
	lv_obj_set_style_text_color(ui->poweroff_count_lbl, COL_WARN, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->poweroff_count_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_align(ui->poweroff_count_lbl, LV_ALIGN_CENTER, 0, -8);

	ui->poweroff_hint_lbl = lv_label_create(ui->poweroff_panel);
	lv_label_set_text(ui->poweroff_hint_lbl, "Hold BOOT to shut down\nRelease to cancel");
	lv_obj_set_style_text_color(ui->poweroff_hint_lbl, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_text_align(ui->poweroff_hint_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->poweroff_hint_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_align(ui->poweroff_hint_lbl, LV_ALIGN_BOTTOM_MID, 0, -36);

	lv_obj_add_flag(ui->poweroff_panel, LV_OBJ_FLAG_HIDDEN);
	ui->poweroff_visible = false;
}

void router_ui_poweroff_show(router_ui_t *ui, unsigned seconds_left)
{
	char buf[8];

	if (!ui)
		return;
	if (!ui->poweroff_panel)
		router_ui_build_poweroff_panel(ui);
	if (!ui->poweroff_panel)
		return;
	if (seconds_left < 1)
		seconds_left = 1;
	snprintf(buf, sizeof(buf), "%u", seconds_left);
	lv_label_set_text(ui->poweroff_count_lbl, buf);
	lv_obj_remove_flag(ui->poweroff_panel, LV_OBJ_FLAG_HIDDEN);
	ui->poweroff_visible = true;
}

void router_ui_poweroff_shutting_down(router_ui_t *ui)
{
	if (!ui)
		return;
	if (!ui->poweroff_panel)
		router_ui_build_poweroff_panel(ui);
	if (!ui->poweroff_panel)
		return;
	lv_label_set_text(ui->poweroff_title_lbl, "Turn off device");
	lv_label_set_text(ui->poweroff_count_lbl, "—");
	lv_label_set_text(ui->poweroff_hint_lbl, "Shutting down…");
	lv_obj_remove_flag(ui->poweroff_panel, LV_OBJ_FLAG_HIDDEN);
	ui->poweroff_visible = true;
}

void router_ui_poweroff_hide(router_ui_t *ui)
{
	if (!ui || !ui->poweroff_panel)
		return;
	lv_label_set_text(ui->poweroff_hint_lbl, "Hold BOOT to shut down\nRelease to cancel");
	lv_obj_add_flag(ui->poweroff_panel, LV_OBJ_FLAG_HIDDEN);
	ui->poweroff_visible = false;
}

bool router_ui_poweroff_active(const router_ui_t *ui)
{
	return ui && ui->poweroff_visible;
}
