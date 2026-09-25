#include "protocol.h"
#include "moontail-banner.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  define popen  _popen
#  define pclose _pclose
static void mt_sleep(int sec) { Sleep(sec * 1000); }
#else
#  include <unistd.h>
#  include <sys/stat.h>
static void mt_sleep(int sec) { sleep(sec); }
#endif

char g_worker[K3_MAX_URL] = "http://127.0.0.1:8787";
static char g_config[512] = "";
char g_peer_id[64] = "";
static char g_peer_token[128] = "";
static char g_model[512] = "";
static char g_llama[512] = "llama-cli";
static char g_volunteer[512] = "build/moontail-volunteer";
static char g_tensor[512] = "config/tensor-overrides.kimi-k3";
static char g_chat_template[64] = "kimi-k3";
char g_tunnel_host[K3_MAX_HOST] = "";
static char g_session_peer[64] = "";
static char g_job_id[64] = "";
static int  g_quiet = 0;
static int  g_skip_tunnel = 0;

static const char * mt_home(void) {
    const char * h = getenv("HOME");
#ifdef _WIN32
    if (!h) h = getenv("USERPROFILE");
#endif
    return h;
}

static void config_path(void) {
    if (g_config[0]) return;
    const char * home = mt_home();
    if (home) snprintf(g_config, sizeof(g_config), "%s/.moontail/config", home);
    else snprintf(g_config, sizeof(g_config), ".moontail/config");
}

static void ensure_peer_token(void) {
    if (g_peer_token[0]) return;
    FILE * ur = fopen("/dev/urandom", "rb");
    unsigned char buf[16];
    if (ur && fread(buf, 1, sizeof(buf), ur) == sizeof(buf)) {
        fclose(ur);
        for (size_t i = 0; i < sizeof(buf); i++)
            snprintf(g_peer_token + i * 2, 3, "%02x", buf[i]);
    } else {
        if (ur) fclose(ur);
        snprintf(g_peer_token, sizeof(g_peer_token), "mt%ld%ld", (long)time(NULL), (long)rand());
    }
}

void mt_config_load(void) {
    config_path();
    const char * w = getenv("MOONTAIL_WORKER");
    if (w) strncpy(g_worker, w, sizeof(g_worker) - 1);
    const char * th = getenv("MOONTAIL_TUNNEL_HOST");
    if (th) strncpy(g_tunnel_host, th, sizeof(g_tunnel_host) - 1);
    const char * ct = getenv("MOONTAIL_CHAT_TEMPLATE");
    if (ct) strncpy(g_chat_template, ct, sizeof(g_chat_template) - 1);
    FILE * f = fopen(g_config, "r");
    if (!f) return;
    char line[768];
    while (fgets(line, sizeof(line), f)) {
        char * nl = strchr(line, '\n'); if (nl) *nl = 0;
        char * eq = strchr(line, '='); if (!eq) continue;
        *eq = 0;
        if (!strcmp(line, "worker")) strncpy(g_worker, eq + 1, sizeof(g_worker) - 1);
        else if (!strcmp(line, "peer_id")) strncpy(g_peer_id, eq + 1, sizeof(g_peer_id) - 1);
        else if (!strcmp(line, "peer_token")) strncpy(g_peer_token, eq + 1, sizeof(g_peer_token) - 1);
        else if (!strcmp(line, "model")) strncpy(g_model, eq + 1, sizeof(g_model) - 1);
        else if (!strcmp(line, "tunnel_host")) strncpy(g_tunnel_host, eq + 1, sizeof(g_tunnel_host) - 1);
    }
    fclose(f);
}

void mt_config_save(void) {
    config_path();
#ifndef _WIN32
    const char * home = mt_home();
    if (home) { char dir[512]; snprintf(dir, sizeof(dir), "%s/.moontail", home); mkdir(dir, 0755); }
#endif
    FILE * f = fopen(g_config, "w");
    if (!f) return;
    fprintf(f, "worker=%s\npeer_id=%s\npeer_token=%s\nmodel=%s\n",
            g_worker, g_peer_id, g_peer_token, g_model);
    if (g_tunnel_host[0]) fprintf(f, "tunnel_host=%s\n", g_tunnel_host);
    fclose(f);
}

static int worker_is_localhost(void) {
    return !strncmp(g_worker, "http://127.0.0.1", 16) ||
           !strncmp(g_worker, "http://localhost", 16);
}

static void json_escape(const char * in, char * out, size_t cap) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 2 < cap; i++) {
        char c = in[i];
        if (c == '"' || c == '\\') { out[j++] = '\\'; if (j >= cap - 1) break; }
        if (c == '\n' || c == '\r') { out[j++] = ' '; continue; }
        out[j++] = c;
    }
    out[j] = 0;
}

static char * body_tmp_path(char * buf, size_t cap) {
    const char * home = mt_home();
    if (home) snprintf(buf, cap, "%s/.moontail/post.json", home);
    else snprintf(buf, cap, ".moontail/post.json");
    return buf;
}

static int http_get(const char * path, char * out, size_t cap) {
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "curl.exe -sf \"%s%s\" 2>nul", g_worker, path);
#else
    snprintf(cmd, sizeof(cmd), "curl -sf \"%s%s\" 2>/dev/null", g_worker, path);
#endif
    FILE * fp = popen(cmd, "r");
    if (!fp) return -1;
    out[0] = 0;
    while (fgets(out + strlen(out), (int)(cap - strlen(out)), fp) && strlen(out) < cap - 1) {}
    pclose(fp);
    return out[0] ? 0 : -1;
}

static int http_post_json(const char * path, const char * body, char * out, size_t cap) {
    char tpath[512], cmd[1200];
    body_tmp_path(tpath, sizeof(tpath));
#ifndef _WIN32
    const char * home = mt_home();
    if (home) { char d[512]; snprintf(d, sizeof(d), "%s/.moontail", home); mkdir(d, 0755); }
#endif
    FILE * tf = fopen(tpath, "w");
    if (!tf) return -1;
    fputs(body, tf);
    fclose(tf);
    snprintf(cmd, sizeof(cmd),
#ifdef _WIN32
        "curl.exe -sf -X POST \"%s%s\" -H \"Content-Type: application/json\" -d @\"%s\" 2>nul",
#else
        "curl -sf -X POST \"%s%s\" -H \"Content-Type: application/json\" -d @\"%s\" 2>/dev/null",
#endif
             g_worker, path, tpath);
    FILE * fp = popen(cmd, "r");
    if (!fp) return -1;
    if (out) out[0] = 0;
    if (out) while (fgets(out + strlen(out), (int)(cap - strlen(out)), fp) && strlen(out) < cap - 1) {}
    pclose(fp);
    return (out && out[0]) ? 0 : -1;
}

static int json_int(const char * j, const char * key) {
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char * p = strstr(j, pat);
    return p ? atoi(p + strlen(pat)) : -1;
}

static void json_str(const char * j, const char * key, char * dst, size_t cap) {
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char * p = strstr(j, pat);
    dst[0] = 0;
    if (!p) return;
    p += strlen(pat);
    char * d = dst;
    while (*p && *p != '"' && (size_t)(d - dst) < cap - 1) *d++ = *p++;
    *d = 0;
}

static int parse_session(const char * line, k3_session * s) {
    char tunnel[K3_MAX_HOST];
    json_str(line, "rpc_host", s->rpc_host, sizeof(s->rpc_host));
    json_str(line, "tunnel_host", tunnel, sizeof(tunnel));
    json_str(line, "peer_id", g_session_peer, sizeof(g_session_peer));
    json_str(line, "job_id", g_job_id, sizeof(g_job_id));
    s->rpc_port = K3_DEFAULT_RPC_PORT;
    const char * p = strstr(line, "\"rpc_port\":");
    if (p) s->rpc_port = atoi(p + 11);
    if (!s->rpc_host[0] && tunnel[0]) strncpy(s->rpc_host, tunnel, sizeof(s->rpc_host) - 1);
    if (!s->rpc_host[0]) return -1;
    snprintf(s->rpc_endpoint, sizeof(s->rpc_endpoint), "%s:%d", s->rpc_host, s->rpc_port);
    return 0;
}

static void release_session(void) {
    if (!g_peer_id[0] || !g_peer_token[0]) return;
    char body[512], esc[128], esc_id[128];
    json_escape(g_peer_token, esc, sizeof(esc));
    json_escape(g_peer_id, esc_id, sizeof(esc_id));
    if (g_job_id[0])
        snprintf(body, sizeof(body), "{\"peer_id\":\"%s\",\"peer_token\":\"%s\",\"job_id\":\"%s\"}",
                 esc_id, esc, g_job_id);
    else snprintf(body, sizeof(body), "{\"peer_id\":\"%s\",\"peer_token\":\"%s\"}", esc_id, esc);
    char dummy[8] = "";
    http_post_json("/session/release", body, dummy, sizeof(dummy));
}

static int run_llama_prompt(const k3_session * s, const char * prompt) {
    char ot_lines[8][256];
    int ot_n = 0;
    char esc[8192];
    json_escape(prompt, esc, sizeof(esc));

    FILE * f = fopen(g_tensor, "r");
    if (!f) f = fopen("config/tensor-overrides.kimi-k3", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f) && ot_n < 8) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char * nl = strchr(line, '\n'); if (nl) *nl = 0;
            strncpy(ot_lines[ot_n++], line, sizeof(ot_lines[0]) - 1);
        }
        fclose(f);
    }
    char ot[1024] = "";
    for (int i = 0; i < ot_n; i++) {
        strncat(ot, ot_lines[i], sizeof(ot) - strlen(ot) - 1);
        strncat(ot, " ", sizeof(ot) - strlen(ot) - 1);
    }
    char cmd[16384];
    snprintf(cmd, sizeof(cmd),
        "%s -m \"%s\" -rpc %s %s --chat-template %s -p \"%s\" -n 128 --no-display-prompt",
        g_llama, g_model, s->rpc_endpoint, ot, g_chat_template, esc);
    if (!g_quiet) fprintf(stderr, "moontail → llama-cli\n");
    return system(cmd);
}

int mt_cmd_status(void) {
    char buf[2048];
    if (http_get("/status", buf, sizeof(buf)) != 0) {
        fprintf(stderr, "cannot reach %s/status\n", g_worker);
        return 1;
    }
    int pool = json_int(buf, "volunteer_pool_size");
    int wait = json_int(buf, "waiting_for");
    int ready = json_int(buf, "swarm_ready");
    int q = json_int(buf, "queue_depth");
    int active = json_int(buf, "active_prompts");
    int maxc = json_int(buf, "max_concurrent_prompts");
    if (ready)
        printf("Swarm ready: %d volunteers, %d/%d prompt slots, %d queued\n", pool, active, maxc, q);
    else
        printf("Waiting for %d more user(s) to run moontail init (%d joined)\n", wait, pool);
    return 0;
}

#ifdef _WIN32
#  include <io.h>
#  define mt_access _access
#else
#  include <unistd.h>
#  define mt_access access
#endif

static void resolve_bin_paths(void) {
    const char * home = mt_home();
    if (!home) return;
    char vol[512], llama[512];
#ifdef _WIN32
    snprintf(vol, sizeof(vol), "%s\\.moontail\\bin\\moontail-volunteer.exe", home);
    snprintf(llama, sizeof(llama), "%s\\.moontail\\bin\\llama-cli.exe", home);
#else
    snprintf(vol, sizeof(vol), "%s/.moontail/bin/moontail-volunteer", home);
    snprintf(llama, sizeof(llama), "%s/.moontail/bin/llama-cli", home);
#endif
    if (mt_access(vol, 0) == 0) strncpy(g_volunteer, vol, sizeof(g_volunteer) - 1);
    if (mt_access(llama, 0) == 0) strncpy(g_llama, llama, sizeof(g_llama) - 1);
}

int mt_cmd_init(int accept_terms) {
    if (!accept_terms) {
        fprintf(stderr, "run: moontail setup --accept-terms  (see docs/VOLUNTEER_TERMS.md)\n");
        return 1;
    }
    mt_config_load();
    resolve_bin_paths();
    ensure_peer_token();
    if (!g_peer_id[0]) snprintf(g_peer_id, sizeof(g_peer_id), "mt-%ld", (long)time(NULL));
    if (!g_model[0]) {
        const char * env = getenv("MOONTAIL_MODEL");
        if (env && env[0]) strncpy(g_model, env, sizeof(g_model) - 1);
        else fprintf(stderr, "Set MOONTAIL_MODEL to your Kimi K3 GGUF path (see docs/KIMI_K3_LAUNCH.md)\n");
    }
    mt_config_save();
    char cmd[4096];
    char exp_end[16];
    snprintf(exp_end, sizeof(exp_end), "%d", K3_DEFAULT_EXPERT_END);
    snprintf(cmd, sizeof(cmd),
        "%s --worker \"%s\" --peer-id \"%s\" --peer-token \"%s\" --model \"%s\" --experts 0 %s",
        g_volunteer, g_worker, g_peer_id, g_peer_token, g_model, exp_end);
    {
        char tun_cfg[512] = "config/cloudflared-volunteer.yml";
        const char * home = mt_home();
        if (home) {
            char user_tun[512];
#ifdef _WIN32
            snprintf(user_tun, sizeof(user_tun), "%s\\.moontail\\cloudflared-volunteer.yml", home);
#else
            snprintf(user_tun, sizeof(user_tun), "%s/.moontail/cloudflared-volunteer.yml", home);
#endif
            if (mt_access(user_tun, 0) == 0) strncpy(tun_cfg, user_tun, sizeof(tun_cfg) - 1);
        }
        strncat(cmd, " --tunnel \"", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, tun_cfg, sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, "\"", sizeof(cmd) - strlen(cmd) - 1);
        if (g_tunnel_host[0]) {
            strncat(cmd, " --tunnel-host \"", sizeof(cmd) - strlen(cmd) - 1);
            strncat(cmd, g_tunnel_host, sizeof(cmd) - strlen(cmd) - 1);
            strncat(cmd, "\"", sizeof(cmd) - strlen(cmd) - 1);
        }
    }
    strncat(cmd, " &", sizeof(cmd) - strlen(cmd) - 1);
    fprintf(stderr, "Volunteer %s starting (GPU lending — see docs/VOLUNTEER_TERMS.md)...\n", g_peer_id);
    (void)system(cmd);
    char buf[2048];
    for (;;) {
        if (http_get("/status", buf, sizeof(buf)) != 0) { mt_sleep(2); continue; }
        int wait = json_int(buf, "waiting_for");
        int pool = json_int(buf, "volunteer_pool_size");
        if (wait <= 0) {
            printf("Volunteer confirmed. Swarm ready (%d GPUs).\nType 'prompt hello' in the shell, or: moontail prompt \"...\"\n", pool);
            return 0;
        }
        printf("Waiting for %d more user(s) to run moontail init (%d joined).\n", wait, pool);
        fflush(stdout);
        mt_sleep(15);
    }
}

int mt_cmd_prompt(const char * prompt) {
    mt_config_load();
    if (!g_peer_id[0] || !g_peer_token[0] || !g_model[0]) {
        fprintf(stderr, "run moontail setup first (or type 'join' in the shell)\n");
        return 1;
    }
    char esc_id[128], esc_tok[256], esc_prompt[8192], body[9216], buf[4096];
    json_escape(g_peer_id, esc_id, sizeof(esc_id));
    json_escape(g_peer_token, esc_tok, sizeof(esc_tok));
    json_escape(prompt, esc_prompt, sizeof(esc_prompt));
    snprintf(body, sizeof(body), "{\"peer_id\":\"%s\",\"peer_token\":\"%s\",\"prompt\":\"%s\"}",
             esc_id, esc_tok, esc_prompt);
    if (http_post_json("/queue", body, buf, sizeof(buf))) { fprintf(stderr, "queue failed\n"); return 1; }
    char job[64]; json_str(buf, "job_id", job, sizeof(job));
    for (;;) {
        char path[128]; snprintf(path, sizeof(path), "/queue/%s", job);
        if (http_get(path, buf, sizeof(buf))) return 1;
        const char * st = strstr(buf, "\"status\":\"");
        if (!st) { mt_sleep(5); continue; }
        st += 10;
        if (!strncmp(st, "ready", 5) || !strncmp(st, "running", 7)) break;
        if (!strncmp(st, "expired", 7)) { fprintf(stderr, "job expired\n"); return 1; }
        int pos = json_int(buf, "position");
        int wait = json_int(buf, "waiting_for");
        if (wait > 0) printf("Waiting for %d more volunteer(s)...\n", wait);
        else if (pos > 0) printf("Queue position: %d\n", pos);
        else printf("Queued...\n");
        fflush(stdout);
        mt_sleep(5);
    }
    snprintf(body, sizeof(body), "{\"peer_id\":\"%s\",\"peer_token\":\"%s\",\"job_id\":\"%s\"}",
             esc_id, esc_tok, job);
    if (http_post_json("/session", body, buf, sizeof(buf))) { fprintf(stderr, "session failed\n"); return 1; }
    k3_session s; memset(&s, 0, sizeof(s));
    if (parse_session(buf, &s)) { fprintf(stderr, "bad session\n"); return 1; }
    if (!g_quiet) fprintf(stderr, "moontail → %s -rpc %s\n", g_llama, s.rpc_endpoint);
    int rc = run_llama_prompt(&s, prompt);
    release_session();
    return rc != 0;
}

int main(int argc, char ** argv) {
    mt_config_load();
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--skip-tunnel")) {
            if (!worker_is_localhost()) {
                fprintf(stderr, "refused: --skip-tunnel (worker is not localhost)\n");
                return 1;
            }
            g_skip_tunnel = 1;
        } else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) g_quiet = 1;
    }
    if (argc < 2) return mt_cmd_repl();
    if (!strcmp(argv[1], "setup")) {
        int ok = 0;
        for (int i = 2; i < argc; i++) if (!strcmp(argv[i], "--accept-terms")) ok = 1;
        return mt_cmd_setup(ok);
    }
    if (!strcmp(argv[1], "help") || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        mt_print_help(argv[0]);
        return 0;
    }
    if (!strcmp(argv[1], "init")) {
        int ok = 0;
        for (int i = 2; i < argc; i++) if (!strcmp(argv[i], "--accept-terms")) ok = 1;
        return mt_cmd_init(ok);
    }
    if (!strcmp(argv[1], "status")) return mt_cmd_status();
    if (!strcmp(argv[1], "prompt") && argc > 2) return mt_cmd_prompt(argv[2]);
    if (!strcmp(argv[1], "shell") || !strcmp(argv[1], "repl")) return mt_cmd_repl();
    mt_print_help(argv[0]);
    return 1;
}
