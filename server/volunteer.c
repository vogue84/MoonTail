/*
 * volunteer.c — supervises localhost rpc-server + outbound transport (<520 LOC)
 * rpc-server MUST bind 127.0.0.1 only (security invariant).
 * Transport: cloudflared tunnel (default) or tailscale serve (MOONTAIL_TRANSPORT=tailscale).
 */
#include "../client/protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  define popen  _popen
#  define pclose _pclose
#else
#  include <unistd.h>
#  include <sys/wait.h>
#endif

static volatile int g_running = 1;
static char g_worker_url[K3_MAX_URL] = "http://127.0.0.1:8787";
static char g_rpc_bin[512] = "rpc-server";
static char g_tunnel_config[512] = "config/cloudflared-volunteer.yml";
static char g_peer_id[64] = "volunteer-1";
static char g_peer_token[128] = "";
static char g_reach_host[K3_MAX_HOST] = "";
static char g_model_path[512] = "";
static int  g_expert_start = 0;
static int  g_expert_end = 383;
static int  g_rpc_port = K3_RPC_PORT;
static int  g_use_tailscale = 0;

static void on_signal(int sig) { (void)sig; g_running = 0; }

static void usage(const char * argv0) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  --worker URL         Registry URL\n"
        "  --rpc PATH           rpc-server binary\n"
        "  --tunnel CFG         cloudflared config (cloudflare transport)\n"
        "  --transport MODE     cloudflare | tailscale\n"
        "  --peer-id ID         Volunteer id\n"
        "  --peer-token TOK     Auth token from moontail init\n"
        "  --tunnel-host HOST   Cloudflare tunnel hostname\n"
        "  --tailscale-host H   Tailscale IP or MagicDNS (tailscale transport)\n"
        "  --model PATH         GGUF for rpc-server -m\n"
        "  --experts START END  Expert shard range\n"
        "  --rpc-port PORT      Local rpc port (127.0.0.1 only)\n",
        argv0);
}

static void json_escape(const char * in, char * out, size_t cap) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 2 < cap; i++) {
        char c = in[i];
        if (c == '"' || c == '\\') { out[j++] = '\\'; if (j >= cap - 1) break; }
        out[j++] = c;
    }
    out[j] = 0;
}

static void detect_tailscale_host(void) {
    if (g_reach_host[0]) return;
    FILE * f = popen("tailscale ip -4 2>nul", "r");
#ifndef _WIN32
    if (!f) f = popen("tailscale ip -4 2>/dev/null", "r");
#endif
    if (!f) return;
    if (fgets(g_reach_host, sizeof(g_reach_host), f)) {
        char * nl = strchr(g_reach_host, '\n');
        if (nl) *nl = 0;
        nl = strchr(g_reach_host, '\r');
        if (nl) *nl = 0;
    }
    pclose(f);
}

static int register_peer(int busy) {
    char tpath[512], cmd[1600], body[1024];
    const char * home = getenv("HOME");
#ifdef _WIN32
    if (!home) home = getenv("USERPROFILE");
#endif
    if (home) snprintf(tpath, sizeof(tpath), "%s/.moontail/reg.json", home);
    else snprintf(tpath, sizeof(tpath), ".moontail/reg.json");

    if (g_use_tailscale) detect_tailscale_host();
    const char * host = g_reach_host[0] ? g_reach_host : g_peer_id;
    if (g_use_tailscale && !g_reach_host[0]) {
        fprintf(stderr, "tailscale: set MOONTAIL_TAILSCALE_HOST or install tailscale CLI\n");
        return -1;
    }

    char esc_id[128], esc_tok[256], esc_host[512];
    json_escape(g_peer_id, esc_id, sizeof(esc_id));
    json_escape(g_peer_token, esc_tok, sizeof(esc_tok));
    json_escape(host, esc_host, sizeof(esc_host));
    snprintf(body, sizeof(body),
        "{\"peer_id\":\"%s\",\"peer_token\":\"%s\",\"tunnel_host\":\"%s\","
        "\"rpc_port\":%d,\"expert_start\":%d,\"expert_end\":%d,\"busy\":%d}",
        esc_id, esc_tok, esc_host, g_rpc_port, g_expert_start, g_expert_end, busy);

    FILE * tf = fopen(tpath, "w");
    if (!tf) return -1;
    fputs(body, tf);
    fclose(tf);
    snprintf(cmd, sizeof(cmd),
        "curl -sf -X POST \"%s/register\" -H \"Content-Type: application/json\" -d @\"%s\" 2>/dev/null",
        g_worker_url, tpath);
    return system(cmd) == 0 ? 0 : -1;
}

#ifdef _WIN32
static PROCESS_INFORMATION g_rpc_pi, g_tunnel_pi;

static int spawn_process(const char * exe, char * const args[], PROCESS_INFORMATION * pi) {
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    char cmdline[2048] = "";
    for (int i = 0; args[i]; i++) {
        if (i) strncat(cmdline, " ", sizeof(cmdline) - strlen(cmdline) - 1);
        strncat(cmdline, args[i], sizeof(cmdline) - strlen(cmdline) - 1);
    }
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, pi))
        return -1;
    return 0;
}
#endif

static int spawn_tailscale_serve(void) {
    char local_target[64];
    char tcp_flag[32];
    snprintf(local_target, sizeof(local_target), "tcp://127.0.0.1:%d", g_rpc_port);
    snprintf(tcp_flag, sizeof(tcp_flag), "--tcp=%d", g_rpc_port);
#ifdef _WIN32
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tailscale serve --bg %s %s", tcp_flag, local_target);
    return system(cmd) == 0 ? 0 : -1;
#else
    pid_t pid = fork();
    if (pid == 0) {
        execlp("tailscale", "tailscale", "serve", "--bg", tcp_flag, local_target, (char *)NULL);
        _exit(127);
    }
    return pid > 0 ? 0 : -1;
#endif
}

int main(int argc, char ** argv) {
    const char * tr = getenv("MOONTAIL_TRANSPORT");
    if (tr && !strcmp(tr, "tailscale")) g_use_tailscale = 1;
    const char * ts = getenv("MOONTAIL_TAILSCALE_HOST");
    if (ts) strncpy(g_reach_host, ts, sizeof(g_reach_host) - 1);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--worker") == 0 && i + 1 < argc)
            { strncpy(g_worker_url, argv[++i], sizeof(g_worker_url) - 1); continue; }
        if (strcmp(argv[i], "--rpc") == 0 && i + 1 < argc)
            { strncpy(g_rpc_bin, argv[++i], sizeof(g_rpc_bin) - 1); continue; }
        if (strcmp(argv[i], "--tunnel") == 0 && i + 1 < argc)
            { strncpy(g_tunnel_config, argv[++i], sizeof(g_tunnel_config) - 1); continue; }
        if (strcmp(argv[i], "--transport") == 0 && i + 1 < argc) {
            g_use_tailscale = !strcmp(argv[++i], "tailscale");
            continue;
        }
        if (strcmp(argv[i], "--peer-id") == 0 && i + 1 < argc)
            { strncpy(g_peer_id, argv[++i], sizeof(g_peer_id) - 1); continue; }
        if (strcmp(argv[i], "--peer-token") == 0 && i + 1 < argc)
            { strncpy(g_peer_token, argv[++i], sizeof(g_peer_token) - 1); continue; }
        if (strcmp(argv[i], "--tunnel-host") == 0 && i + 1 < argc)
            { strncpy(g_reach_host, argv[++i], sizeof(g_reach_host) - 1); continue; }
        if (strcmp(argv[i], "--tailscale-host") == 0 && i + 1 < argc)
            { strncpy(g_reach_host, argv[++i], sizeof(g_reach_host) - 1); continue; }
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc)
            { strncpy(g_model_path, argv[++i], sizeof(g_model_path) - 1); continue; }
        if (strcmp(argv[i], "--experts") == 0 && i + 2 < argc) {
            g_expert_start = atoi(argv[++i]);
            g_expert_end = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--rpc-port") == 0 && i + 1 < argc) {
            g_rpc_port = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "-h") == 0) { usage(argv[0]); return 0; }
    }

    if (!g_peer_token[0]) {
        fprintf(stderr, "peer_token required — run moontail init --accept-terms\n");
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    char host_port[32];
    snprintf(host_port, sizeof(host_port), "%d", g_rpc_port);
    char host_bind[] = "127.0.0.1";

#ifndef _WIN32
    pid_t rpc_pid = fork();
    if (rpc_pid == 0) {
        if (g_model_path[0])
            execlp(g_rpc_bin, g_rpc_bin, "-H", host_bind, "-p", host_port, "-m", g_model_path, (char *)NULL);
        execlp(g_rpc_bin, g_rpc_bin, "-H", host_bind, "-p", host_port, (char *)NULL);
        fprintf(stderr, "exec rpc-server failed\n");
        _exit(127);
    }
    pid_t transport_pid = -1;
    if (g_use_tailscale) {
        if (spawn_tailscale_serve() != 0)
            fprintf(stderr, "tailscale serve failed — is tailscale installed and logged in?\n");
    } else if (g_tunnel_config[0]) {
        transport_pid = fork();
        if (transport_pid == 0) {
            execlp("cloudflared", "cloudflared", "tunnel", "--config", g_tunnel_config, "run", (char *)NULL);
            _exit(127);
        }
    }
#else
    char * rpc_args[16];
    int ra = 0;
    rpc_args[ra++] = (char *)g_rpc_bin;
    rpc_args[ra++] = (char *)"-H"; rpc_args[ra++] = host_bind;
    rpc_args[ra++] = (char *)"-p"; rpc_args[ra++] = host_port;
    if (g_model_path[0]) { rpc_args[ra++] = (char *)"-m"; rpc_args[ra++] = g_model_path; }
    rpc_args[ra] = NULL;
    if (spawn_process(g_rpc_bin, rpc_args, &g_rpc_pi) != 0) {
        fprintf(stderr, "rpc-server spawn failed\n");
        return 1;
    }
    if (g_use_tailscale) {
        if (spawn_tailscale_serve() != 0)
            fprintf(stderr, "tailscale serve failed\n");
    } else {
        char * tun_args[] = { (char *)"cloudflared", (char *)"tunnel", (char *)"--config", g_tunnel_config, (char *)"run", NULL };
        spawn_process("cloudflared", tun_args, &g_tunnel_pi);
    }
#endif

    if (register_peer(0) != 0)
        fprintf(stderr, "register failed — check MOONTAIL_WORKER and network\n");

    while (g_running) {
#ifndef _WIN32
        sleep(30);
#else
        Sleep(30000);
#endif
        register_peer(0);
    }

#ifndef _WIN32
    if (rpc_pid > 0) kill(rpc_pid, SIGTERM);
    if (transport_pid > 0) kill(transport_pid, SIGTERM);
    if (g_use_tailscale) system("tailscale serve reset 2>/dev/null");
#else
    TerminateProcess(g_rpc_pi.hProcess, 0);
    if (g_tunnel_pi.hProcess) TerminateProcess(g_tunnel_pi.hProcess, 0);
    if (g_use_tailscale) system("tailscale serve reset 2>nul");
#endif
    return 0;
}
