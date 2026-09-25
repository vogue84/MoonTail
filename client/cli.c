#include "cli.h"
#include "moontail-banner.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern char g_worker[K3_MAX_URL];
extern char g_peer_id[64];
extern char g_tunnel_host[K3_MAX_HOST];

static void trim_line(char * s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || isspace((unsigned char)s[n - 1]))) s[--n] = 0;
    char * p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static int read_url_file(const char * path, char * out, size_t cap) {
    FILE * f = fopen(path, "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        trim_line(line);
        if (!line[0] || line[0] == '#') continue;
        if (!strncmp(line, "http://", 7) || !strncmp(line, "https://", 8)) {
            strncpy(out, line, cap - 1);
            out[cap - 1] = 0;
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int load_official_worker(void) {
    const char * env = getenv("MOONTAIL_WORKER");
    if (env && env[0]) {
        strncpy(g_worker, env, K3_MAX_URL - 1);
        return 1;
    }
    const char * home = getenv("HOME");
#ifdef _WIN32
    if (!home) home = getenv("USERPROFILE");
#endif
    char path[512];
    if (home) {
        snprintf(path, sizeof(path), "%s/.moontail/worker.url", home);
        if (read_url_file(path, g_worker, K3_MAX_URL)) return 1;
    }
    if (read_url_file("config/official-worker.url", g_worker, K3_MAX_URL)) return 1;
    return 0;
}

static int detect_cloudflared(void) {
    FILE * f = popen("cloudflared --version 2>nul", "r");
#ifndef _WIN32
    if (!f) f = popen("cloudflared --version 2>/dev/null", "r");
#endif
    if (!f) return 0;
    char buf[64];
    int ok = fgets(buf, sizeof(buf), f) != NULL;
    pclose(f);
    return ok;
}

static void print_config_summary(void) {
    printf("\n  worker     %s\n", g_worker[0] ? g_worker : "(not set)");
    if (g_tunnel_host[0]) printf("  tunnel     %s\n", g_tunnel_host);
    if (g_peer_id[0]) printf("  peer_id    %s\n", g_peer_id);
    printf("\n");
}

void mt_print_help(const char * prog) {
    moontail_print_moon(1);
    printf(
        "moontail — volunteer GPU slice, queue for Kimi K3 swarm\n\n"
        "Quick start:\n"
        "  bash scripts/volunteer-tunnel.sh   One-time Cloudflare tunnel (volunteers)\n"
        "  %s setup --accept-terms            Join swarm + lend expert VRAM\n"
        "  %s prompt \"your message\"           Queue a Kimi K3 prompt\n\n"
        "Interactive: %s  (help · join · prompt · status · quit)\n\n"
        "Docs: docs/FAQ.md · docs/HN_LAUNCH.md · docs/VOLUNTEER_TERMS.md\n\n",
        prog, prog, prog);
}

int mt_cmd_setup(int accept_terms) {
    mt_config_load();
    if (!load_official_worker()) {
        fprintf(stderr, "setup: set MOONTAIL_WORKER or run install.sh\n");
        return 1;
    }
    if (!detect_cloudflared()) {
        fprintf(stderr, "setup: install cloudflared — https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/downloads/\n");
        return 1;
    }
    const char * env = getenv("MOONTAIL_TUNNEL_HOST");
    if (env && env[0]) strncpy(g_tunnel_host, env, K3_MAX_HOST - 1);
    if (!g_tunnel_host[0]) {
        fprintf(stderr,
            "setup: run bash scripts/volunteer-tunnel.sh\n"
            "Or set MOONTAIL_TUNNEL_HOST to your tunnel hostname.\n");
        return 1;
    }
    mt_config_save();
    moontail_print_moon(0);
    printf("MoonTail setup — Kimi K3 reciprocal swarm\n");
    print_config_summary();
    if (!accept_terms) {
        printf("You will lend GPU cycles to other swarm members.\n");
        printf("Accept volunteer terms? (docs/VOLUNTEER_TERMS.md) [y/N]: ");
        fflush(stdout);
        char ans[32] = "";
        if (!fgets(ans, sizeof(ans), stdin)) return 1;
        trim_line(ans);
        if (ans[0] != 'y' && ans[0] != 'Y') return 1;
        accept_terms = 1;
    }
    printf("Joining swarm…\n");
    return mt_cmd_init(accept_terms);
}

int mt_cmd_repl(void) {
    mt_config_load();
    if (!g_worker[0] || !strncmp(g_worker, "http://127.0.0.1", 16))
        load_official_worker();

    moontail_print_moon(0);
    printf("moontail shell — type 'help' for commands\n");
    if (!g_peer_id[0]) printf("Tip: run 'join' first to volunteer your GPU slice\n");
    print_config_summary();

    char line[8192];
    for (;;) {
        printf("moontail> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        trim_line(line);
        if (!line[0]) continue;
        if (!strcmp(line, "quit") || !strcmp(line, "exit") || !strcmp(line, "q")) break;
        if (!strcmp(line, "help") || !strcmp(line, "?")) { mt_print_help("moontail"); continue; }
        if (!strcmp(line, "status")) { mt_cmd_status(); continue; }
        if (!strcmp(line, "config")) { print_config_summary(); continue; }
        if (!strcmp(line, "join") || !strcmp(line, "setup")) { mt_cmd_setup(1); continue; }
        if (!strncmp(line, "prompt ", 7)) { mt_cmd_prompt(line + 7); continue; }
        if (line[0] == '"') { mt_cmd_prompt(line); continue; }
        mt_cmd_prompt(line);
    }
    printf("bye\n");
    return 0;
}
