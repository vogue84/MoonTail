/*
 * MoonTail — reciprocal MoE swarm CLI (client)
 * POST /session → cloudflared access tcp → llama.cpp with -rpc 127.0.0.1:PORT
 */
#include "protocol.h"
#include "moon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <process.h>
#  define popen  _popen
#  define pclose _pclose
#else
#  include <unistd.h>
#  include <sys/wait.h>
#  include <signal.h>
#endif

static char g_worker_url[K3_MAX_URL] = "http://127.0.0.1:8787";
static char g_llama_bin[512] = "llama-cli";
static char g_model[512] = "";
static int  g_skip_tunnel = 0;
static int  g_quiet = 0;
static int  g_local_rpc_port = K3_LOCAL_PROXY_PORT;

static int worker_is_localhost(void) {
    const char * url = g_worker_url;
    const char * host = url;
    if (strncmp(url, "http://", 7) == 0) host = url + 7;
    else if (strncmp(url, "https://", 8) == 0) host = url + 8;
    else return 0;
    char hostbuf[256];
    size_t i = 0;
    while (host[i] && host[i] != ':' && host[i] != '/' && i < sizeof(hostbuf) - 1) {
        hostbuf[i] = host[i];
        i++;
    }
    hostbuf[i] = '\0';
    return (strcmp(hostbuf, "127.0.0.1") == 0 || strcmp(hostbuf, "localhost") == 0
            || strcmp(hostbuf, "::1") == 0);
}

static int skip_tunnel_allowed(void) {
    return g_skip_tunnel && worker_is_localhost();
}

static void usage(const char * argv0) {
    if (!g_quiet) moontail_print_moon(1);
    fprintf(stderr,
        "Usage: %s [options] -- [llama-cli args...]\n"
        "  --worker URL       Control plane (default %s)\n"
        "  --llama PATH       llama-cli binary\n"
        "  --model PATH       GGUF model (-m)\n"
        "  --local-rpc PORT   Dev only: 127.0.0.1:PORT when --worker is localhost\n"
        "  --skip-tunnel      Dev only: same; refused if --worker is not localhost\n"
        "  --quiet, -q        Skip moon banner\n",
        argv0, g_worker_url);
}

static int http_post_session(k3_session * out) {
    char cmd[4096];
    char line[8192];
    snprintf(cmd, sizeof(cmd),
        "curl -sf -X POST \"%s/session\" -H \"Content-Type: application/json\" "
        "-d \"{\\\"expert_start\\\":0,\\\"expert_end\\\":255}\" 2>/dev/null",
        g_worker_url);
    FILE * fp = popen(cmd, "r");
    if (!fp) return -1;
    line[0] = '\0';
    while (fgets(line + strlen(line), (int)(sizeof(line) - strlen(line)), fp)) {
        if (strlen(line) >= sizeof(line) - 1) break;
    }
    pclose(fp);
    if (line[0] == '\0') return -1;

    const char * h = strstr(line, "\"tunnel_host\"");
    const char * t = strstr(line, "\"access_token\"");
    const char * p = strstr(line, "\"local_port\"");
    if (!h || !t) return -1;
    h = strchr(h, ':'); t = strchr(t, ':');
    if (!h || !t) return -1;
    h = strchr(h, '"'); if (!h) return -1; h++;
    {
        char * d = out->tunnel_host;
        while (*h && *h != '"' && d < out->tunnel_host + K3_MAX_HOST - 1) *d++ = *h++;
        *d = '\0';
    }
    t = strchr(t, '"'); if (!t) return -1; t++;
    {
        char * d = out->access_token;
        while (*t && *t != '"' && d < out->access_token + K3_MAX_TOKEN - 1) *d++ = *t++;
        *d = '\0';
    }
    out->local_port = g_local_rpc_port;
    if (p) { p = strchr(p, ':'); if (p) out->local_port = atoi(p + 1); }
    snprintf(out->rpc_endpoint, sizeof(out->rpc_endpoint), "127.0.0.1:%d", out->local_port);
    return 0;
}

static int spawn_cloudflared(const k3_session * s) {
    char port_str[16];
    char url[32];
    snprintf(port_str, sizeof(port_str), "%d", s->local_port);
    snprintf(url, sizeof(url), "127.0.0.1:%s", port_str);
#ifdef _WIN32
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "start /B cloudflared access tcp --url %s --hostname %s",
        url, s->tunnel_host);
    return system(cmd) == 0 ? 0 : -1;
#else
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        if (s->access_token[0]) setenv("CF_ACCESS_TOKEN", s->access_token, 1);
        execlp("cloudflared", "cloudflared", "access", "tcp",
               "--url", url, "--hostname", s->tunnel_host, (char *)NULL);
        _exit(127);
    }
    return 0;
#endif
}

static void load_tensor_overrides(char * storage, size_t cap) {
    FILE * f = fopen("config/tensor-overrides.k3", "r");
    if (!f) f = fopen("../config/tensor-overrides.k3", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char * nl = strchr(line, '\n'); if (nl) *nl = '\0';
        char * sp = line;
        while (*sp && isspace((unsigned char)*sp)) sp++;
        if (*sp == '\0') continue;
        strncat(storage, sp, cap - strlen(storage) - 1);
        strncat(storage, "\n", cap - strlen(storage) - 1);
    }
    fclose(f);
}

static int run_llama(const k3_session * s, int argc, char ** argv, int base) {
    char rpc_arg[128];
    snprintf(rpc_arg, sizeof(rpc_arg), "-rpc %s", s->rpc_endpoint);
    char overrides[1024] = "";
    load_tensor_overrides(overrides, sizeof(overrides));
    char cmd[8192];
    int n = snprintf(cmd, sizeof(cmd), "%s", g_llama_bin);
    if (g_model[0]) n += snprintf(cmd + n, sizeof(cmd) - n, " -m \"%s\"", g_model);
    n += snprintf(cmd + n, sizeof(cmd) - n, " %s", rpc_arg);
    char * o = overrides;
    while (o && *o) {
        char * e = strchr(o, '\n');
        if (e) *e = '\0';
        if (*o) n += snprintf(cmd + n, sizeof(cmd) - n, " %s", o);
        o = e ? e + 1 : NULL;
    }
    for (int i = base; i < argc; i++)
        n += snprintf(cmd + n, sizeof(cmd) - n, " %s", argv[i]);
    if (!g_quiet) fprintf(stderr, "moontail → %s\n", cmd);
    return system(cmd);
}

int main(int argc, char ** argv) {
    k3_session session;
    memset(&session, 0, sizeof(session));
    int base = 1;

    for (; base < argc; base++) {
        if (strcmp(argv[base], "--") == 0) { base++; break; }
        if (strcmp(argv[base], "--worker") == 0 && base + 1 < argc)
            { strncpy(g_worker_url, argv[++base], sizeof(g_worker_url) - 1); continue; }
        if (strcmp(argv[base], "--llama") == 0 && base + 1 < argc)
            { strncpy(g_llama_bin, argv[++base], sizeof(g_llama_bin) - 1); continue; }
        if (strcmp(argv[base], "--model") == 0 && base + 1 < argc)
            { strncpy(g_model, argv[++base], sizeof(g_model) - 1); continue; }
        if (strcmp(argv[base], "--local-rpc") == 0 && base + 1 < argc) {
            g_skip_tunnel = 1; g_local_rpc_port = atoi(argv[++base]); continue;
        }
        if (strcmp(argv[base], "--skip-tunnel") == 0) { g_skip_tunnel = 1; continue; }
        if (strcmp(argv[base], "--quiet") == 0 || strcmp(argv[base], "-q") == 0) {
            g_quiet = 1; continue;
        }
        if (strcmp(argv[base], "-h") == 0 || strcmp(argv[base], "--help") == 0) {
            usage(argv[0]); return 0;
        }
        break;
    }

    if (!g_quiet) moontail_print_moon(1);

    if (g_skip_tunnel && !worker_is_localhost()) {
        fprintf(stderr,
            "refused: --skip-tunnel/--local-rpc requires --worker on localhost (127.0.0.1)\n");
        return 1;
    }

    if (skip_tunnel_allowed()) {
        snprintf(session.rpc_endpoint, sizeof(session.rpc_endpoint),
                 "127.0.0.1:%d", g_local_rpc_port);
    } else {
        if (http_post_session(&session) != 0) {
            fprintf(stderr, "session failed; use --skip-tunnel for local gate4/dev\n");
            return 1;
        }
        if (spawn_cloudflared(&session) != 0) {
            fprintf(stderr, "cloudflared spawn failed\n");
            return 1;
        }
#ifndef _WIN32
        sleep(2);
#endif
    }

    return run_llama(&session, argc, argv, base) != 0;
}
