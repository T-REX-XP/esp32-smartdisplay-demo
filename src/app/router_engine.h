#ifndef ROUTER_ENGINE_H
#define ROUTER_ENGINE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROUTER_ENGINE_PAGE_COUNT 6
#define ROUTER_ENGINE_BOOT (-1)

typedef struct {
	void (*tx)(const char *line, void *ctx);
	void (*show_boot)(void *ctx);
	void (*show_page)(int page, void *ctx);
	void (*set_boot_status)(const char *text, unsigned pct, void *ctx);
	void (*apply_metrics)(const char *json, void *ctx);
	unsigned (*now_ms)(void *ctx);
	void *ctx;
	const char *stack;
	unsigned release;
	const char *component;
	unsigned rdcp;
} router_hooks_t;

typedef struct {
	bool linked;
	int page; /* ROUTER_ENGINE_BOOT or 0..5 */
	unsigned req_id;
	unsigned last_req_ms;
	unsigned last_rx_ms;
	unsigned screen_timeout;
	char timeout_mode[8];
	char last_gesture_dir[8];
	router_hooks_t hooks;
} router_engine_t;

void router_engine_init(router_engine_t *e, const router_hooks_t *hooks);
int router_engine_on_line(router_engine_t *e, const char *line);
int router_engine_on_input(router_engine_t *e, const char *dir);
int router_engine_tick(router_engine_t *e);
bool router_engine_link_ok(router_engine_t *e, unsigned timeout_ms);
int router_engine_emit_poweroff(router_engine_t *e);
int router_engine_page_from_id(const char *screen_id);
const char *router_engine_page_id(int page);
const char *router_engine_page_scope(int page);
bool router_engine_is_boot_id(const char *screen_id);

#ifdef __cplusplus
}
#endif

#endif /* ROUTER_ENGINE_H */
