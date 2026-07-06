/*
 * Router display screen IDs — keep in sync with /etc/mcud/pages.json on the host.
 */

#ifndef ROUTER_PAGES_H
#define ROUTER_PAGES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	ROUTER_PAGE_SYSTEM = 0,
	ROUTER_PAGE_NETWORK,
	ROUTER_PAGE_CLIENTS,
	ROUTER_PAGE_STORAGE,
	ROUTER_PAGE_WIFI,
	ROUTER_PAGE_SECURITY,
	ROUTER_PAGE_COUNT
} router_page_t;

const char *router_page_id(router_page_t page);
const char *router_page_scope(router_page_t page);
bool router_page_is_boot_id(const char *screen_id);

#ifdef __cplusplus
}
#endif

#endif /* ROUTER_PAGES_H */
