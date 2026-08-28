#include <stdio.h>
#include <string.h>

#include "router_icon_chars.h"
#include "router_pages.h"

static int tests_failed;

static void expect(int cond, const char *msg)
{
	if (!cond) {
		tests_failed++;
		printf("FAIL: %s\n", msg);
	}
}

static void test_page_ids_and_scopes(void)
{
	static const char *const ids[] = {
		"router_system",  "router_network", "router_clients",
		"router_storage", "router_wifi",    "router_security",
	};
	static const char *const scopes[] = {
		"system", "network", "clients", "storage", "wifi", "security",
	};
	int i;

	expect(ROUTER_PAGE_COUNT == 6, "six router pages");
	for (i = 0; i < ROUTER_PAGE_COUNT; i++) {
		expect(!strcmp(router_page_id((router_page_t)i), ids[i]),
		       "page id order");
		expect(!strcmp(router_page_scope((router_page_t)i), scopes[i]),
		       "page scope order");
		expect(router_page_icon((router_page_t)i)[0] != '\0',
		       "page icon defined");
	}
}

static void test_page_bounds(void)
{
	expect(router_page_id((router_page_t)-1)[0] == '\0', "negative page id");
	expect(router_page_scope((router_page_t)99)[0] == '\0',
	       "out of range scope");
	expect(router_page_icon((router_page_t)99)[0] == '\0',
	       "out of range icon");
	expect(!strcmp(router_page_icon(ROUTER_PAGE_WIFI), ROUTER_ICON_WIFI),
	       "wifi page icon");
}

static void test_boot_id(void)
{
	expect(router_page_is_boot_id("router_boot"), "boot id match");
	expect(!router_page_is_boot_id("router_system"), "system not boot");
	expect(!router_page_is_boot_id(NULL), "null not boot");
}

int main(void)
{
	test_page_ids_and_scopes();
	test_page_bounds();
	test_boot_id();

	printf(tests_failed ? "FAILED\n" : "OK\n");
	return tests_failed ? 1 : 0;
}
