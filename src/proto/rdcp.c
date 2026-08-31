#include "rdcp.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *skip_ws(const char *p)
{
	while (*p && isspace((unsigned char)*p))
		p++;
	return p;
}

static void copy_str(char *dst, size_t dstn, const char *src)
{
	snprintf(dst, dstn, "%s", src);
}

static const char *find_key_colon(const char *json, const char *key)
{
	char pat[40];
	const char *p;
	const char *colon;

	snprintf(pat, sizeof(pat), "\"%s\"", key);
	p = json;
	while ((p = strstr(p, pat)) != NULL) {
		colon = skip_ws(p + strlen(pat));
		if (*colon == ':')
			return skip_ws(colon + 1);
		p++;
	}
	return NULL;
}

static int extract_string(const char *json, const char *key, char *dst, size_t dstn)
{
	const char *p;
	const char *end;
	size_t i;

	p = find_key_colon(json, key);
	if (!p)
		return -1;
	if (*p != '"')
		return -1;
	p++;
	end = p;
	while (*end && *end != '"') {
		if (*end == '\\' && end[1])
			end += 2;
		else
			end++;
	}
	if (*end != '"')
		return -1;
	/* copy without escapes for the simple cases used on the wire */
	i = 0;
	while (p < end && i + 1 < dstn) {
		if (*p == '\\' && p + 1 < end) {
			p++;
			dst[i++] = *p++;
			continue;
		}
		dst[i++] = *p++;
	}
	dst[i] = '\0';
	return 0;
}

static int extract_uint(const char *json, const char *key, unsigned *out)
{
	const char *p;
	unsigned v = 0;
	int any = 0;

	p = find_key_colon(json, key);
	if (!p)
		return -1;
	if (*p == '"') {
		p++;
		while (*p && isdigit((unsigned char)*p)) {
			v = v * 10u + (unsigned)(*p - '0');
			any = 1;
			p++;
		}
	} else {
		while (*p && isdigit((unsigned char)*p)) {
			v = v * 10u + (unsigned)(*p - '0');
			any = 1;
			p++;
		}
	}
	if (!any)
		return -1;
	*out = v;
	return 0;
}

static int has_key(const char *json, const char *key)
{
	return find_key_colon(json, key) != NULL;
}

int rdcp_parse(const char *line, rdcp_msg_t *out)
{
	char t[8];
	unsigned v = 0;

	if (!line || !out)
		return -1;
	memset(out, 0, sizeof(*out));
	while (*line && isspace((unsigned char)*line))
		line++;
	if (*line == '\0')
		return -1;

	if (extract_string(line, "request", out->request, sizeof(out->request)) == 0 &&
	    out->request[0]) {
		out->kind = RDCP_KIND_LEGACY;
		copy_str(out->scope, sizeof(out->scope), out->request);
		return 0;
	}

	if (extract_uint(line, "v", &v) != 0 || v != 1)
		return -1;
	if (extract_string(line, "t", t, sizeof(t)) != 0)
		return -1;

	(void)extract_string(line, "op", out->op, sizeof(out->op));
	(void)extract_uint(line, "id", &out->id);
	(void)extract_string(line, "scope", out->scope, sizeof(out->scope));
	(void)extract_string(line, "screen", out->screen, sizeof(out->screen));
	(void)extract_string(line, "dir", out->dir, sizeof(out->dir));
	(void)extract_string(line, "text", out->text, sizeof(out->text));
	(void)extract_string(line, "stage", out->stage, sizeof(out->stage));
	(void)extract_string(line, "screen_timeout_mode", out->timeout_mode, sizeof(out->timeout_mode));
	(void)extract_uint(line, "pct", &out->pct);
	(void)extract_uint(line, "screen_timeout", &out->screen_timeout);
	(void)extract_uint(line, "uptime_ms", &out->uptime_ms);
	if (has_key(line, "pong"))
		out->pong = 1;

	if (!strcmp(t, "req"))
		out->kind = RDCP_KIND_REQ;
	else if (!strcmp(t, "res"))
		out->kind = RDCP_KIND_RES;
	else if (!strcmp(t, "evt"))
		out->kind = RDCP_KIND_EVT;
	else if (!strcmp(t, "cmd"))
		out->kind = RDCP_KIND_CMD;
	else if (!strcmp(t, "push"))
		out->kind = RDCP_KIND_PUSH;
	else
		return -1;
	return 0;
}

int rdcp_build_pong(char *buf, size_t n, unsigned id, unsigned long uptime_ms)
{
	if (!buf || n == 0)
		return -1;
	return snprintf(buf, n,
			"{\"v\":1,\"t\":\"res\",\"id\":%u,\"data\":{\"pong\":1,\"uptime_ms\":%lu}}",
			id, uptime_ms) < (int)n ? 0 : -1;
}

int rdcp_build_echo(char *buf, size_t n, const char *text)
{
	char esc[RDCP_TEXT_MAX * 2];
	size_t i = 0;

	if (!buf || n == 0)
		return -1;
	if (!text)
		text = "";
	while (*text && i + 1 < sizeof(esc)) {
		if (*text == '"' || *text == '\\') {
			if (i + 2 >= sizeof(esc))
				break;
			esc[i++] = '\\';
		}
		esc[i++] = *text++;
	}
	esc[i] = '\0';
	return snprintf(buf, n,
			"{\"v\":1,\"t\":\"evt\",\"op\":\"echo\",\"data\":{\"text\":\"%s\"}}",
			esc) < (int)n ? 0 : -1;
}

int rdcp_build_screen_evt(char *buf, size_t n, const char *screen)
{
	if (!buf || n == 0 || !screen || !screen[0])
		return -1;
	return snprintf(buf, n,
			"{\"v\":1,\"t\":\"evt\",\"op\":\"screen\",\"data\":{\"screen\":\"%s\",\"action\":\"loaded\"}}",
			screen) < (int)n ? 0 : -1;
}

int rdcp_build_version_evt(char *buf, size_t n, const char *stack, unsigned release,
			   const char *component, unsigned rdcp)
{
	if (!buf || n == 0 || !stack || !component)
		return -1;
	return snprintf(buf, n,
			"{\"v\":1,\"t\":\"evt\",\"op\":\"version\",\"data\":{\"stack\":\"%s\",\"release\":%u,\"component\":\"%s\",\"rdcp\":%u}}",
			stack, release, component, rdcp) < (int)n ? 0 : -1;
}

int rdcp_build_metrics_req(char *buf, size_t n, unsigned id, const char *scope)
{
	if (!buf || n == 0 || !scope || !scope[0] || id == 0)
		return -1;
	return snprintf(buf, n,
			"{\"v\":1,\"t\":\"req\",\"id\":%u,\"op\":\"metrics\",\"scope\":\"%s\"}",
			id, scope) < (int)n ? 0 : -1;
}

int rdcp_build_poweroff(char *buf, size_t n)
{
	if (!buf || n == 0)
		return -1;
	return snprintf(buf, n,
			"{\"v\":1,\"t\":\"req\",\"op\":\"poweroff\",\"data\":{\"source\":\"boot\"}}") < (int)n ? 0 : -1;
}
