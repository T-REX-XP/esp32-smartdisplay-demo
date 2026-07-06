/*
 * Router LVGL screens — maps to luci-app-mcu-display pages.json scopes.
 */

#ifndef ROUTER_UI_H
#define ROUTER_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "router_data.h"
#include "router_pages.h"

#include <lvgl.h>

typedef struct router_ui router_ui_t;

router_ui_t *router_ui_create(void);
void router_ui_destroy(router_ui_t *ui);
void router_ui_show_page(router_ui_t *ui, router_page_t page, lv_scr_load_anim_t anim);
void router_ui_show_boot(router_ui_t *ui);
void router_ui_set_boot_status(router_ui_t *ui, const char *text, unsigned pct);
bool router_ui_on_boot(const router_ui_t *ui);
router_page_t router_ui_current_page(const router_ui_t *ui);
void router_ui_refresh(router_ui_t *ui, const router_metrics_t *metrics);
lv_obj_t *router_ui_screen(const router_ui_t *ui, router_page_t page);
lv_obj_t *router_ui_boot_screen(router_ui_t *ui);
void router_ui_install_swipe(lv_obj_t *scr, lv_event_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* ROUTER_UI_H */
