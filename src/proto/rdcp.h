#ifndef RDCP_H
#define RDCP_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RDCP_OP_MAX 16
#define RDCP_SCOPE_MAX 16
#define RDCP_SCREEN_MAX 32
#define RDCP_DIR_MAX 8
#define RDCP_TEXT_MAX 128
#define RDCP_STAGE_MAX 16
#define RDCP_MODE_MAX 8
#define RDCP_LINE_MAX 4096

typedef enum {
	RDCP_KIND_NONE = 0,
	RDCP_KIND_REQ,
	RDCP_KIND_RES,
	RDCP_KIND_EVT,
	RDCP_KIND_CMD,
	RDCP_KIND_PUSH,
	RDCP_KIND_LEGACY
} rdcp_kind_t;

typedef struct {
	rdcp_kind_t kind;
	char op[RDCP_OP_MAX];
	unsigned id;
	char scope[RDCP_SCOPE_MAX];
	char screen[RDCP_SCREEN_MAX];
	char dir[RDCP_DIR_MAX];
	char text[RDCP_TEXT_MAX];
	char stage[RDCP_STAGE_MAX];
	char timeout_mode[RDCP_MODE_MAX];
	unsigned pct;
	unsigned screen_timeout;
	unsigned pong;
	unsigned uptime_ms;
	char request[RDCP_SCOPE_MAX];
} rdcp_msg_t;

int rdcp_parse(const char *line, rdcp_msg_t *out);

int rdcp_build_pong(char *buf, size_t n, unsigned id, unsigned long uptime_ms);
int rdcp_build_echo(char *buf, size_t n, const char *text);
int rdcp_build_screen_evt(char *buf, size_t n, const char *screen);
int rdcp_build_version_evt(char *buf, size_t n, const char *stack, unsigned release,
			   const char *component, unsigned rdcp);
int rdcp_build_input_evt(char *buf, size_t n, const char *dir);
int rdcp_build_metrics_req(char *buf, size_t n, unsigned id, const char *scope);
int rdcp_build_poweroff(char *buf, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* RDCP_H */
