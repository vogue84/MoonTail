/*
 * MoonTail interactive CLI — setup wizard, REPL, help.
 */
#include "cli.h"
#include "moon.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern char g_worker[K3_MAX_URL];
extern char g_peer_id[64];
extern char g_tailscale_host[K3_MAX_HOST];
extern char g_transport[32];

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

static int detect_tailscale(void) {
    const char * env = getenv("MOONTAIL_TAILSCALE_HOST");
    if (env && env[0]) {
        strncpy(g_tailscale_host, env, K3_MAX_HOST - 1);
        return 1;
    }
    FILE * f = popen("tailscale ip -4 2>nul", "r");
#ifndef _WIN32
    if (!f) f = popen("tailscale ip -4 2>/dev/null", "r");
#endif
    if (!f) return 0;
    if (fgets(g_tailscale_host, K3_MAX_HOST, f)) {
        trim_line(g_tailscale_host);
        pclose(f);
        return g_tailscale_host[0] != 0;
    }
    pclose(f);
    return 0;
}

static void print_config_summary(void) {
    printf("\n  worker     %s\n", g_worker[0] ? g_worker : "(not set)");
    printf("  transport  %s\n", g_transport[0] ? g_transport : "tailscale");
    if (g_tailscale_host[0]) printf("  tailscale  %s\n", g_tailscale_host);
    if (g_peer_id[0]) printf("  peer_id    %s\n", g_peer_id);
    printf("\n");
}

void mt_print_help(const char * prog) {
    moontail_print_moon(1);
    printf(
        "moontail — volunteer GPU, queue for Kimi K2\n\n"
        "Quick start (one command after install):\n"
        "  %s setup              Join swarm (Tailscale + volunteer GPU)\n"
        "  %s                    Open interactive shell (same as above if new)\n\n"
        "Interactive shell commands:\n"
        "  help                  Show this help\n"
        "  status                Swarm pool / queue\n"
        "  join                  Volunteer GPU (accept terms)\n"
        "  prompt <message>      Queue a Kimi K2 prompt\n"
        "  config                Show saved settings\n"
        "  quit                  Exit\n\n"
        "Direct commands:\n"
        "  %s setup [--accept-terms]\n"
        "  %s status\n"
        "  %s prompt \"your message\"\n"
        "  %s init --accept-terms   (same as join)\n\n"
        "Needs: curl, Tailscale on same tailnet, GPU + K2 weights.\n"
        "Docs: docs/TAILSCALE.md · docs/VOLUNTEER_TERMS.md\n\n",
        prog, prog, prog, prog, prog, prog);
}

int mt_detect_tailscale(void) {
    return detect_tailscale();
}

int mt_cmd_setup(int accept_terms) {
    mt_config_load();
    if (!load_official_worker()) {
        fprintf(stderr, "setup: set MOONTAIL_WORKER or run install.sh (writes ~/.moontail/worker.url)\n");
        return 1;
    }
    if (!g_transport[0]) strncpy(g_transport, "tailscale", sizeof(g_transport) - 1);
    if (!strcmp(g_transport, "tailscale") && !detect_tailscale()) {
        fprintf(stderr, "setup: install Tailscale, log in, then retry (or set MOONTAIL_TAILSCALE_HOST)\n");
        return 1;
    }
    mt_config_save();
    moontail_print_moon(0);
    printf("MoonTail setup — Kimi K2 reciprocal swarm\n");
    print_config_summary();
    if (!accept_terms) {
        printf("You will lend GPU cycles to other swarm members.\n");
        printf("Accept volunteer terms? (docs/VOLUNTEER_TERMS.md) [y/N]: ");
        fflush(stdout);
        char ans[32] = "";
        if (!fgets(ans, sizeof(ans), stdin)) return 1;
        trim_line(ans);
        if (ans[0] != 'y' && ans[0] != 'Y') {
            printf("Cancelled. Run: %s setup --accept-terms\n", "moontail");
            return 1;
        }
        accept_terms = 1;
    }
    printf("Joining swarm…\n");
    return mt_cmd_init(accept_terms);
}

int mt_cmd_repl(void) {
    mt_config_load();
    if (!g_worker[0] || !strncmp(g_worker, "http://127.0.0.1", 16))
        load_official_worker();
    if (!g_transport[0]) strncpy(g_transport, "tailscale", sizeof(g_transport) - 1);

    moontail_print_moon(0);
    printf("moontail shell — type 'help' for commands, 'quit' to exit\n");
    if (!g_peer_id[0])
        printf("Tip: run 'join' first to volunteer your GPU\n");
    print_config_summary();

    char line[8192];
    for (;;) {
        printf("moontail> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        trim_line(line);
        if (!line[0]) continue;
        if (!strcmp(line, "quit") || !strcmp(line, "exit") || !strcmp(line, "q")) break;
        if (!strcmp(line, "help") || !strcmp(line, "?")) {
            mt_print_help("moontail");
            continue;
        }
        if (!strcmp(line, "status")) { mt_cmd_status(); continue; }
        if (!strcmp(line, "config")) { print_config_summary(); continue; }
        if (!strcmp(line, "join") || !strcmp(line, "setup")) {
            mt_cmd_setup(1);
            continue;
        }
        if (!strncmp(line, "prompt ", 7)) {
            mt_cmd_prompt(line + 7);
            continue;
        }
        if (line[0] == '"') {
            mt_cmd_prompt(line);
            continue;
        }
        /* bare text → treat as prompt */
        mt_cmd_prompt(line);
    }
    printf("bye\n");
    return 0;
}
