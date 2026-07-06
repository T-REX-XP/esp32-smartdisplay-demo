#pragma once

#include "router_data.h"

#ifdef __cplusplus

#include <ArduinoJson.h>

void router_app_init(void);
void router_app_loop(void);
void router_app_on_serial_line(const char *line);
router_metrics_t *router_app_metrics(void);
router_page_t router_app_current_page(void);

#endif
