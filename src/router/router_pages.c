#include "router_pages.h"

#include <stddef.h>
#include <string.h>

#include "router_icon_chars.h"

#define ROUTER_SCREEN_BOOT "router_boot"

static const char *const PAGE_IDS[ROUTER_PAGE_COUNT] = {
	"router_system",
	"router_network",
	"router_clients",
	"router_storage",
	"router_wifi",
	"router_security",
};

static const char *const PAGE_SCOPES[ROUTER_PAGE_COUNT] = {
	"system",
	"network",
	"clients",
	"storage",
	"wifi",
	"security",
};

static const char *const PAGE_ICONS[ROUTER_PAGE_COUNT] = {
	ROUTER_ICON_MICROCHIP,
	ROUTER_ICON_NETWORK,
	ROUTER_ICON_USERS,
	ROUTER_ICON_HDD,
	ROUTER_ICON_WIFI,
	ROUTER_ICON_SHIELD,
};

const char *router_page_id(router_page_t page)
{
	if (page < 0 || page >= ROUTER_PAGE_COUNT)
		return "";
	return PAGE_IDS[page];
}

const char *router_page_scope(router_page_t page)
{
	if (page < 0 || page >= ROUTER_PAGE_COUNT)
		return "";
	return PAGE_SCOPES[page];
}

const char *router_page_icon(router_page_t page)
{
	if (page < 0 || page >= ROUTER_PAGE_COUNT)
		return "";
	return PAGE_ICONS[page];
}

bool router_page_is_boot_id(const char *screen_id)
{
	return screen_id && !strcmp(screen_id, ROUTER_SCREEN_BOOT);
}
