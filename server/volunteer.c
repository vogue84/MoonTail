/*
 * volunteer.c — supervises localhost rpc-server + outbound cloudflared (<480 LOC)
 * rpc-server MUST bind 127.0.0.1 only (security invariant).
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
static int  g_expert_start = 0;
static int  g_expert_end = 255;
static int  g_rpc_port = K3_RPC_PORT;

static void on_signal(int sig) { (void)sig; g_running = 0; }

static void usage(const char * argv0) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  --worker URL       Registry URL\n"
        "  --rpc PATH         rpc-server binary\n"
        "  --tunnel CFG       cloudflared config\n"
        "  --peer-id ID       Volunteer id\n"
        "  --experts START END  Expert shard range\n"
        "  --rpc-port PORT    Local rpc port (127.0.0.1 only)\n",
        argv0);
}

static int register_peer(int busy) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "curl -sf -X POST \"%s/register\" -H \"Content-Type: application/json\" "
        "-d \"{\\\"peer_id\\\":\\\"%s\\\",\\\"tunnel_host\\\":\\\"%s\\\","
        "\\\"expert_start\\\":%d,\\\"expert_end\\\":%d,\\\"busy\\\":%d}\" 2>/dev/null",
        g_worker_url, g_peer_id, g_peer_id, g_expert_start, g_expert_end, busy);
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

int main(int argc, char ** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--worker") == 0 && i + 1 < argc)
            { strncpy(g_worker_url, argv[++i], sizeof(g_worker_url) - 1); continue; }
        if (strcmp(argv[i], "--rpc") == 0 && i + 1 < argc)
            { strncpy(g_rpc_bin, argv[++i], sizeof(g_rpc_bin) - 1); continue; }
        if (strcmp(argv[i], "--tunnel") == 0 && i + 1 < argc)
            { strncpy(g_tunnel_config, argv[++i], sizeof(g_tunnel_config) - 1); continue; }
        if (strcmp(argv[i], "--peer-id") == 0 && i + 1 < argc)
            { strncpy(g_peer_id, argv[++i], sizeof(g_peer_id) - 1); continue; }
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

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    char host_port[32];
    snprintf(host_port, sizeof(host_port), "%d", g_rpc_port);
    char host_bind[] = "127.0.0.1";

#ifndef _WIN32
    pid_t rpc_pid = fork();
    if (rpc_pid == 0) {
        execlp(g_rpc_bin, g_rpc_bin, "-H", host_bind, "-p", host_port, (char *)NULL);
        fprintf(stderr, "exec rpc-server failed\n");
        _exit(127);
    }
    pid_t tunnel_pid = -1;
    if (g_tunnel_config[0]) {
        tunnel_pid = fork();
        if (tunnel_pid == 0) {
            execlp("cloudflared", "cloudflared", "tunnel", "--config", g_tunnel_config, "run", (char *)NULL);
            _exit(127);
        }
    }
#else
    char * rpc_args[] = { (char *)g_rpc_bin, (char *)"-H", host_bind, (char *)"-p", host_port, NULL };
    if (spawn_process(g_rpc_bin, rpc_args, &g_rpc_pi) != 0) {
        fprintf(stderr, "rpc-server spawn failed\n");
        return 1;
    }
    char * tun_args[] = { (char *)"cloudflared", (char *)"tunnel", (char *)"--config", g_tunnel_config, (char *)"run", NULL };
    spawn_process("cloudflared", tun_args, &g_tunnel_pi);
#endif

    register_peer(0);
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
    if (tunnel_pid > 0) kill(tunnel_pid, SIGTERM);
#else
    TerminateProcess(g_rpc_pi.hProcess, 0);
    if (g_tunnel_pi.hProcess) TerminateProcess(g_tunnel_pi.hProcess, 0);
#endif
    return 0;
}
