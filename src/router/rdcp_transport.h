#pragma once

#include <Arduino.h>

void rdcp_transport_begin(void);
int rdcp_transport_available(void);
String rdcp_transport_read_line(void);
void rdcp_transport_send_line(const char *line);
