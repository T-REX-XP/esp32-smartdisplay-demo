#include "router_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/qrcode/lv_qrcode.h"

LV_IMG_DECLARE(ui_img_pattern_png);
LV_IMG_DECLARE(ui_img_sls_logo_png);

#define COL_TEXT lv_color_hex(0x000746)
#define COL_MUTED lv_color_hex(0x9C9CD9)
#define COL_ACCENT lv_color_hex(0x293062)
#define COL_PANEL lv_color_hex(0xE8E8F8)
#define COL_WHITE lv_color_hex(0xFFFFFF)
#define COL_OK lv_color_hex(0x2E7D32)
#define COL_WARN lv_color_hex(0xC62828)

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

	lv_obj_t *net_wan_lbl;
	lv_obj_t *net_rx_lbl;
	lv_obj_t *net_tx_lbl;
	lv_obj_t *net_ping_lbl;

	lv_obj_t *cli_total_lbl;
	lv_obj_t *cli_24_lbl;
	lv_obj_t *cli_5_lbl;
	lv_obj_t *cli_lan_lbl;
	lv_obj_t *cli_dhcp_bar;

	lv_obj_t *sto_root_bar;
	lv_obj_t *sto_root_lbl;
	lv_obj_t *sto_data_bar;
	lv_obj_t *sto_data_lbl;

	lv_obj_t *wifi_ssid_lbl;
	lv_obj_t *wifi_state_lbl;
	lv_obj_t *wifi_qr;

	lv_obj_t *sec_fw_lbl;
	lv_obj_t *sec_blocked_lbl;
	lv_obj_t *sec_vpn_lbl;
};

static lv_obj_t *make_screen_bg(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);

	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(scr, COL_WHITE, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_image_src(scr, &ui_img_pattern_png, LV_PART_MAIN);
	lv_obj_set_style_bg_image_tiled(scr, true, LV_PART_MAIN);
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

static lv_obj_t *add_metric_card(lv_obj_t *parent, const char *title, int y)
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
	return card;
}

static void build_system(router_ui_t *ui, lv_obj_t *scr)
{
	lv_obj_t *card;

	ui->sys_cpu_arc = lv_arc_create(scr);
	lv_obj_set_size(ui->sys_cpu_arc, 140, 140);
	lv_obj_align(ui->sys_cpu_arc, LV_ALIGN_CENTER, 0, -10);
	lv_arc_set_range(ui->sys_cpu_arc, 0, 100);
	lv_arc_set_value(ui->sys_cpu_arc, 0);
	lv_arc_set_bg_angles(ui->sys_cpu_arc, 135, 45);
	lv_arc_set_angles(ui->sys_cpu_arc, 135, 135);
	lv_obj_set_style_arc_color(ui->sys_cpu_arc, lv_color_hex(0xE63431), LV_PART_INDICATOR);
	lv_obj_set_style_arc_width(ui->sys_cpu_arc, 10, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(ui->sys_cpu_arc, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_arc_width(ui->sys_cpu_arc, 6, LV_PART_MAIN);

	ui->sys_cpu_lbl = add_body_label(scr, "CPU 0%", LV_ALIGN_CENTER, 0, -10);
	lv_obj_set_style_text_font(ui->sys_cpu_lbl, &lv_font_montserrat_18, LV_PART_MAIN);

	card = add_metric_card(scr, "RAM", 200);
	ui->sys_ram_bar = lv_bar_create(card);
	lv_obj_set_size(ui->sys_ram_bar, lv_pct(100), 10);
	lv_obj_align(ui->sys_ram_bar, LV_ALIGN_BOTTOM_MID, 0, -4);
	lv_bar_set_range(ui->sys_ram_bar, 0, 100);
	lv_obj_set_style_bg_color(ui->sys_ram_bar, COL_MUTED, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ui->sys_ram_bar, COL_ACCENT, LV_PART_INDICATOR);

	ui->sys_ram_lbl = lv_label_create(card);
	lv_label_set_text(ui->sys_ram_lbl, "--");
	lv_obj_set_style_text_color(ui->sys_ram_lbl, COL_TEXT, LV_PART_MAIN);
	lv_obj_align(ui->sys_ram_lbl, LV_ALIGN_BOTTOM_LEFT, 0, -18);

	ui->sys_host_lbl = add_body_label(scr, "Router", LV_ALIGN_BOTTOM_LEFT, 12, -36);
	ui->sys_uptime_lbl = add_body_label(scr, "up --", LV_ALIGN_BOTTOM_RIGHT, -12, -36);
	lv_obj_set_style_text_font(ui->sys_host_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->sys_uptime_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
}

static void build_network(router_ui_t *ui, lv_obj_t *scr)
{
	lv_obj_t *card = add_metric_card(scr, "WAN", 52);

	ui->net_wan_lbl = lv_label_create(card);
	lv_label_set_text(ui->net_wan_lbl, "--");
	lv_obj_set_style_text_color(ui->net_wan_lbl, COL_ACCENT, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->net_wan_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_align(ui->net_wan_lbl, LV_ALIGN_CENTER, 0, 6);

	ui->net_rx_lbl = add_body_label(scr, "RX --", LV_ALIGN_LEFT_MID, 16, 20);
	ui->net_tx_lbl = add_body_label(scr, "TX --", LV_ALIGN_LEFT_MID, 16, 50);
	ui->net_ping_lbl = add_body_label(scr, "PING -- ms", LV_ALIGN_RIGHT_MID, -16, 20);
	lv_obj_set_style_text_font(ui->net_rx_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
	lv_obj_set_style_text_font(ui->net_tx_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
}

static void build_clients(router_ui_t *ui, lv_obj_t *scr)
{
	lv_obj_t *c;

	c = add_metric_card(scr, "2.4 GHz", 52);
	ui->cli_24_lbl = lv_label_create(c);
	lv_label_set_text(ui->cli_24_lbl, "0");
	lv_obj_set_style_text_font(ui->cli_24_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->cli_24_lbl, COL_ACCENT, LV_PART_MAIN);
	lv_obj_align(ui->cli_24_lbl, LV_ALIGN_CENTER, 0, 8);

	c = add_metric_card(scr, "5 GHz", 118);
	ui->cli_5_lbl = lv_label_create(c);
	lv_label_set_text(ui->cli_5_lbl, "0");
	lv_obj_set_style_text_font(ui->cli_5_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->cli_5_lbl, COL_ACCENT, LV_PART_MAIN);
	lv_obj_align(ui->cli_5_lbl, LV_ALIGN_CENTER, 0, 8);

	c = add_metric_card(scr, "LAN / DHCP", 184);
	ui->cli_lan_lbl = lv_label_create(c);
	lv_label_set_text(ui->cli_lan_lbl, "LAN 0");
	lv_obj_set_style_text_color(ui->cli_lan_lbl, COL_TEXT, LV_PART_MAIN);
	lv_obj_align(ui->cli_lan_lbl, LV_ALIGN_TOP_LEFT, 0, 16);

	ui->cli_total_lbl = lv_label_create(c);
	lv_label_set_text(ui->cli_total_lbl, "0 clients");
	lv_obj_set_style_text_color(ui->cli_total_lbl, COL_MUTED, LV_PART_MAIN);
	lv_obj_align(ui->cli_total_lbl, LV_ALIGN_TOP_RIGHT, 0, 16);

	ui->cli_dhcp_bar = lv_bar_create(c);
	lv_obj_set_size(ui->cli_dhcp_bar, lv_pct(100), 8);
	lv_obj_align(ui->cli_dhcp_bar, LV_ALIGN_BOTTOM_MID, 0, -4);
	lv_bar_set_range(ui->cli_dhcp_bar, 0, 100);
}

static void build_storage(router_ui_t *ui, lv_obj_t *scr)
{
	lv_obj_t *c;

	c = add_metric_card(scr, "ROOT", 52);
	ui->sto_root_lbl = lv_label_create(c);
	lv_label_set_text(ui->sto_root_lbl, "--");
	lv_obj_align(ui->sto_root_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);
	ui->sto_root_bar = lv_bar_create(c);
	lv_obj_set_size(ui->sto_root_bar, lv_pct(100), 10);
	lv_obj_align(ui->sto_root_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
	lv_bar_set_range(ui->sto_root_bar, 0, 100);

	c = add_metric_card(scr, "DATA / SWAP", 118);
	ui->sto_data_lbl = lv_label_create(c);
	lv_label_set_text(ui->sto_data_lbl, "--");
	lv_obj_align(ui->sto_data_lbl, LV_ALIGN_TOP_LEFT, 0, 16);
	ui->sto_data_bar = lv_bar_create(c);
	lv_obj_set_size(ui->sto_data_bar, lv_pct(100), 10);
	lv_obj_align(ui->sto_data_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
	lv_bar_set_range(ui->sto_data_bar, 0, 100);
}

static void build_wifi(router_ui_t *ui, lv_obj_t *scr)
{
	ui->wifi_ssid_lbl = add_body_label(scr, "SSID", LV_ALIGN_TOP_LEFT, 14, 48);
	lv_obj_set_style_text_font(ui->wifi_ssid_lbl, &lv_font_montserrat_18, LV_PART_MAIN);

	ui->wifi_state_lbl = add_body_label(scr, "AP --", LV_ALIGN_TOP_LEFT, 14, 78);
	lv_obj_set_style_text_color(ui->wifi_state_lbl, COL_MUTED, LV_PART_MAIN);

	ui->wifi_qr = lv_qrcode_create(scr);
	lv_qrcode_set_size(ui->wifi_qr, 120);
	lv_obj_align(ui->wifi_qr, LV_ALIGN_RIGHT_MID, -10, 20);
	lv_qrcode_set_dark_color(ui->wifi_qr, COL_TEXT);
	lv_qrcode_set_light_color(ui->wifi_qr, COL_WHITE);

	add_body_label(scr, "Scan to join", LV_ALIGN_BOTTOM_MID, 0, -16);
}

static void build_security(router_ui_t *ui, lv_obj_t *scr)
{
	lv_obj_t *c = add_metric_card(scr, "Firewall", 52);

	ui->sec_fw_lbl = lv_label_create(c);
	lv_label_set_text(ui->sec_fw_lbl, "unknown");
	lv_obj_set_style_text_font(ui->sec_fw_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->sec_fw_lbl, COL_ACCENT, LV_PART_MAIN);
	lv_obj_align(ui->sec_fw_lbl, LV_ALIGN_CENTER, 0, 8);

	c = add_metric_card(scr, "Threats / VPN", 130);
	ui->sec_blocked_lbl = lv_label_create(c);
	lv_label_set_text(ui->sec_blocked_lbl, "Blocked 24h: --");
	lv_obj_align(ui->sec_blocked_lbl, LV_ALIGN_TOP_LEFT, 0, 14);
	ui->sec_vpn_lbl = lv_label_create(c);
	lv_label_set_text(ui->sec_vpn_lbl, "VPN: --");
	lv_obj_align(ui->sec_vpn_lbl, LV_ALIGN_BOTTOM_LEFT, 0, -4);
}

static void build_boot(router_ui_t *ui)
{
	lv_obj_t *scr = lv_obj_create(NULL);

	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(scr, COL_WHITE, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_image_src(scr, &ui_img_pattern_png, LV_PART_MAIN);
	lv_obj_set_style_bg_image_tiled(scr, true, LV_PART_MAIN);
	lv_obj_set_style_bg_image_opa(scr, LV_OPA_40, LV_PART_MAIN);

	ui->boot_logo = lv_image_create(scr);
	lv_image_set_src(ui->boot_logo, &ui_img_sls_logo_png);
	lv_obj_align(ui->boot_logo, LV_ALIGN_CENTER, 0, -48);

	ui->boot_title_lbl = lv_label_create(scr);
	lv_label_set_text(ui->boot_title_lbl, "Router Display");
	lv_obj_set_style_text_font(ui->boot_title_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->boot_title_lbl, COL_TEXT, LV_PART_MAIN);
	lv_obj_align(ui->boot_title_lbl, LV_ALIGN_CENTER, 0, 36);

	ui->boot_msg_lbl = lv_label_create(scr);
	lv_label_set_text(ui->boot_msg_lbl, "Waiting for router...");
	lv_obj_set_style_text_font(ui->boot_msg_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(ui->boot_msg_lbl, COL_MUTED, LV_PART_MAIN);
	lv_obj_align(ui->boot_msg_lbl, LV_ALIGN_CENTER, 0, 64);
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
	for (i = 0; i < ROUTER_PAGE_COUNT; i++)
		build_page(ui, (router_page_t)i);
	ui->current = ROUTER_PAGE_SYSTEM;
	ui->on_boot = true;
	lv_screen_load(ui->boot_scr);
	return ui;
}

void router_ui_destroy(router_ui_t *ui)
{
	int i;

	if (!ui)
		return;
	if (ui->boot_scr)
		lv_obj_delete(ui->boot_scr);
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

void router_ui_refresh(router_ui_t *ui, const router_metrics_t *m)
{
	char buf[ROUTER_STR_LEN * 2 + 16];
	int cpu;

	if (!ui || !m)
		return;

	cpu = cpu_value(m->cpu);
	if (ui->sys_cpu_arc) {
		lv_arc_set_value(ui->sys_cpu_arc, cpu);
		lv_arc_set_angles(ui->sys_cpu_arc, 135, 135 + (cpu * 270 / 100));
	}
	if (ui->sys_cpu_lbl) {
		snprintf(buf, sizeof(buf), "CPU %s%%", m->cpu);
		lv_label_set_text(ui->sys_cpu_lbl, buf);
	}
	if (ui->sys_ram_bar)
		lv_bar_set_value(ui->sys_ram_bar, m->ram_pct, LV_ANIM_OFF);
	if (ui->sys_ram_lbl) {
		snprintf(buf, sizeof(buf), "RAM %u%%", m->ram_pct);
		lv_label_set_text(ui->sys_ram_lbl, buf);
	}
	if (ui->sys_host_lbl)
		lv_label_set_text(ui->sys_host_lbl, m->hostname);
	if (ui->sys_uptime_lbl) {
		snprintf(buf, sizeof(buf), "up %s", m->uptime_short);
		lv_label_set_text(ui->sys_uptime_lbl, buf);
	}

	if (ui->net_wan_lbl)
		lv_label_set_text(ui->net_wan_lbl, m->wan_ip);
	if (ui->net_rx_lbl) {
		snprintf(buf, sizeof(buf), "RX %s", m->rx_rate);
		lv_label_set_text(ui->net_rx_lbl, buf);
	}
	if (ui->net_tx_lbl) {
		snprintf(buf, sizeof(buf), "TX %s", m->tx_rate);
		lv_label_set_text(ui->net_tx_lbl, buf);
	}
	if (ui->net_ping_lbl) {
		snprintf(buf, sizeof(buf), "PING %d ms", m->ping_ms);
		lv_label_set_text(ui->net_ping_lbl, buf);
	}

	if (ui->cli_24_lbl)
		lv_label_set_text(ui->cli_24_lbl, m->wifi_24);
	if (ui->cli_5_lbl)
		lv_label_set_text(ui->cli_5_lbl, m->wifi_5);
	if (ui->cli_lan_lbl) {
		snprintf(buf, sizeof(buf), "LAN %s", m->lan_clients);
		lv_label_set_text(ui->cli_lan_lbl, buf);
	}
	if (ui->cli_total_lbl)
		lv_label_set_text(ui->cli_total_lbl, m->clients_total);
	if (ui->cli_dhcp_bar)
		lv_bar_set_value(ui->cli_dhcp_bar, m->dhcp_pct, LV_ANIM_OFF);

	if (ui->sto_root_lbl)
		lv_label_set_text(ui->sto_root_lbl, m->root_usage);
	if (ui->sto_root_bar)
		lv_bar_set_value(ui->sto_root_bar, m->root_pct, LV_ANIM_OFF);
	if (ui->sto_data_lbl) {
		snprintf(buf, sizeof(buf), "%s  swap %s", m->data_usage, m->swap_usage);
		lv_label_set_text(ui->sto_data_lbl, buf);
	}
	if (ui->sto_data_bar)
		lv_bar_set_value(ui->sto_data_bar, m->data_pct, LV_ANIM_OFF);

	if (ui->wifi_ssid_lbl)
		lv_label_set_text(ui->wifi_ssid_lbl, m->wifi_ssid);
	if (ui->wifi_state_lbl)
		lv_label_set_text(ui->wifi_state_lbl, m->wifi_ap_state);
	if (ui->wifi_qr && m->wifi_qr[0] && m->wifi_qr[0] != '-')
		lv_qrcode_update(ui->wifi_qr, m->wifi_qr, strlen(m->wifi_qr));

	if (ui->sec_fw_lbl)
		lv_label_set_text(ui->sec_fw_lbl, m->firewall_state);
	if (ui->sec_blocked_lbl) {
		snprintf(buf, sizeof(buf), "Blocked 24h: %s", m->blocked_24h);
		lv_label_set_text(ui->sec_blocked_lbl, buf);
	}
	if (ui->sec_vpn_lbl) {
		snprintf(buf, sizeof(buf), "VPN: %s", m->vpn_tunnels);
		lv_label_set_text(ui->sec_vpn_lbl, buf);
	}
}
