#include "router_pages.h"

#include <stddef.h>

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
