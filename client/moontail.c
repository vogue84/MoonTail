/*
 * MoonTail CLI — init (volunteer) | status | prompt
 */
#include "protocol.h"
#include "moon.h"
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
#  include <sys/wait.h>
#  include <sys/stat.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
static void mt_sleep(int sec) { sleep(sec); }
#endif

static char g_worker[K3_MAX_URL] = "http://127.0.0.1:8787";
static char g_config[512] = "";
static char g_peer_id[64] = "";
static char g_peer_token[128] = "";
static char g_model[512] = "";
static char g_llama[512] = "llama-cli";
static char g_volunteer[512] = "build/moontail-volunteer";
static char g_tensor[512] = "config/tensor-overrides.kimi-k2";
static char g_tunnel_host[K3_MAX_HOST] = "";
static char g_tailscale_host[K3_MAX_HOST] = "";
static char g_transport[32] = "";
static char g_session_peer[64] = "";
static char g_job_id[64] = "";
static int  g_quiet = 0;
static int  g_skip_tunnel = 0;
static int  g_local_port = K3_LOCAL_PROXY_PORT;

static void config_path(void) {
    if (g_config[0]) return;
    const char * home = getenv("HOME");
#ifdef _WIN32
    if (!home) home = getenv("USERPROFILE");
#endif
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

static void config_load(void) {
    config_path();
    const char * w = getenv("MOONTAIL_WORKER");
    if (w) strncpy(g_worker, w, sizeof(g_worker) - 1);
    const char * th = getenv("MOONTAIL_TUNNEL_HOST");
    if (th) strncpy(g_tunnel_host, th, sizeof(g_tunnel_host) - 1);
    const char * tsh = getenv("MOONTAIL_TAILSCALE_HOST");
    if (tsh) strncpy(g_tailscale_host, tsh, sizeof(g_tailscale_host) - 1);
    const char * tr = getenv("MOONTAIL_TRANSPORT");
    if (tr) strncpy(g_transport, tr, sizeof(g_transport) - 1);
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
        else if (!strcmp(line, "tailscale_host")) strncpy(g_tailscale_host, eq + 1, sizeof(g_tailscale_host) - 1);
        else if (!strcmp(line, "transport")) strncpy(g_transport, eq + 1, sizeof(g_transport) - 1);
    }
    fclose(f);
}

static void config_save(void) {
    config_path();
#ifndef _WIN32
    const char * home = getenv("HOME");
    if (home) {
        char dir[512]; snprintf(dir, sizeof(dir), "%s/.moontail", home);
        mkdir(dir, 0755);
    }
#endif
    FILE * f = fopen(g_config, "w");
    if (!f) return;
    fprintf(f, "worker=%s\npeer_id=%s\npeer_token=%s\nmodel=%s\n",
            g_worker, g_peer_id, g_peer_token, g_model);
    if (g_tunnel_host[0]) fprintf(f, "tunnel_host=%s\n", g_tunnel_host);
    if (g_tailscale_host[0]) fprintf(f, "tailscale_host=%s\n", g_tailscale_host);
    if (g_transport[0]) fprintf(f, "transport=%s\n", g_transport);
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
    const char * home = getenv("HOME");
#ifdef _WIN32
    if (!home) home = getenv("USERPROFILE");
#endif
    if (home) snprintf(buf, cap, "%s/.moontail/post.json", home);
    else snprintf(buf, cap, ".moontail/post.json");
    return buf;
}

static int http_get(const char * path, char * out, size_t cap) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -sf \"%s%s\" 2>/dev/null", g_worker, path);
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
    const char * home = getenv("HOME");
    if (home) { char d[512]; snprintf(d, sizeof(d), "%s/.moontail", home); mkdir(d, 0755); }
#endif
    FILE * tf = fopen(tpath, "w");
    if (!tf) return -1;
    fputs(body, tf);
    fclose(tf);
    snprintf(cmd, sizeof(cmd), "curl -sf -X POST \"%s%s\" -H \"Content-Type: application/json\" -d @\"%s\" 2>/dev/null",
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

static int wait_tcp(int port, int ms) {
#ifdef _WIN32
    WSADATA wsa; if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
#endif
    for (int t = 0; t < ms; t += 200) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) break;
        struct sockaddr_in a; memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET; a.sin_port = htons((unsigned short)port);
        inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        int ok = connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0;
#ifdef _WIN32
        closesocket(fd); Sleep(200);
#else
        close(fd); usleep(200000);
#endif
        if (ok) return 0;
    }
    return -1;
}

static int parse_session(const char * line, k3_session * s) {
    json_str(line, "transport", s->transport, sizeof(s->transport));
    json_str(line, "tunnel_host", s->tunnel_host, sizeof(s->tunnel_host));
    json_str(line, "rpc_host", s->rpc_host, sizeof(s->rpc_host));
    json_str(line, "access_client_id", s->access_client_id, sizeof(s->access_client_id));
    json_str(line, "access_client_secret", s->access_client_secret, sizeof(s->access_client_secret));
    json_str(line, "access_token", s->access_token, sizeof(s->access_token));
    json_str(line, "peer_id", g_session_peer, sizeof(g_session_peer));
    json_str(line, "job_id", g_job_id, sizeof(g_job_id));
    s->local_port = g_local_port;
    s->rpc_port = K3_DEFAULT_RPC_PORT;
    const char * p = strstr(line, "\"local_port\":");
    if (p) s->local_port = atoi(p + 13);
    p = strstr(line, "\"rpc_port\":");
    if (p) s->rpc_port = atoi(p + 11);
    if (!s->rpc_host[0] && s->tunnel_host[0])
        strncpy(s->rpc_host, s->tunnel_host, sizeof(s->rpc_host) - 1);
    if (!strcmp(s->transport, "tailscale") && s->rpc_host[0]) {
        snprintf(s->rpc_endpoint, sizeof(s->rpc_endpoint), "%s:%d", s->rpc_host, s->rpc_port);
        return 0;
    }
    snprintf(s->rpc_endpoint, sizeof(s->rpc_endpoint), "127.0.0.1:%d", s->local_port);
    return s->tunnel_host[0] ? 0 : -1;
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

static int spawn_tunnel(const k3_session * s) {
    if (!strcmp(s->transport, "tailscale")) return 0;
    if (g_skip_tunnel && worker_is_localhost()) return 0;
    char url[32]; snprintf(url, sizeof(url), "127.0.0.1:%d", s->local_port);
#ifdef _WIN32
    char cmd[768];
    if (s->access_client_id[0]) {
        SetEnvironmentVariableA("CF_ACCESS_CLIENT_ID", s->access_client_id);
        SetEnvironmentVariableA("CF_ACCESS_CLIENT_SECRET", s->access_client_secret);
    }
    snprintf(cmd, sizeof(cmd), "start /B cloudflared access tcp --url %s --hostname %s", url, s->tunnel_host);
    return system(cmd) == 0 ? 0 : -1;
#else
    pid_t pid = fork();
    if (pid == 0) {
        if (s->access_client_id[0]) {
            setenv("CF_ACCESS_CLIENT_ID", s->access_client_id, 1);
            setenv("CF_ACCESS_CLIENT_SECRET", s->access_client_secret, 1);
        }
        execlp("cloudflared", "cloudflared", "access", "tcp", "--url", url, "--hostname", s->tunnel_host, NULL);
        _exit(127);
    }
    return 0;
#endif
}

static int run_llama_prompt(const k3_session * s, const char * prompt) {
    char ot_lines[8][256];
    int ot_n = 0;
    char esc[8192];
    json_escape(prompt, esc, sizeof(esc));

    FILE * f = fopen(g_tensor, "r");
    if (!f) f = fopen("config/tensor-overrides.kimi-k2", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f) && ot_n < 8) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char * nl = strchr(line, '\n'); if (nl) *nl = 0;
            strncpy(ot_lines[ot_n++], line, sizeof(ot_lines[0]) - 1);
        }
        fclose(f);
    }
#ifndef _WIN32
    char * args[32];
    int na = 0;
    args[na++] = g_llama;
    args[na++] = "-m"; args[na++] = g_model;
    args[na++] = "-rpc"; args[na++] = (char *)s->rpc_endpoint;
    for (int i = 0; i < ot_n; i++) args[na++] = ot_lines[i];
    args[na++] = "--chat-template"; args[na++] = "kimi-k2";
    args[na++] = "-p"; args[na++] = esc;
    args[na++] = "-n"; args[na++] = "128";
    args[na++] = "--no-display-prompt";
    args[na] = NULL;
    pid_t pid = fork();
    if (pid == 0) {
        execvp(g_llama, args);
        _exit(127);
    }
    if (pid < 0) return 1;
    int st = 0;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 1;
#else
    char ot[1024] = "";
    for (int i = 0; i < ot_n; i++) {
        strncat(ot, ot_lines[i], sizeof(ot) - strlen(ot) - 1);
        strncat(ot, " ", sizeof(ot) - strlen(ot) - 1);
    }
    char cmd[16384];
    snprintf(cmd, sizeof(cmd),
        "%s -m \"%s\" -rpc %s %s --chat-template kimi-k2 -p \"%s\" -n 128 --no-display-prompt",
        g_llama, g_model, s->rpc_endpoint, ot, esc);
    if (!g_quiet) fprintf(stderr, "moontail → llama-cli\n");
    return system(cmd);
#endif
}

static int cmd_status(void) {
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

static void pull_model_path(void) {
    FILE * f = popen("bash scripts/pull-k2.sh 2>/dev/null | tail -1", "r");
    if (!f) return;
    char line[512];
    if (fgets(line, sizeof(line), f) && !strncmp(line, "model=", 6)) {
        strncpy(g_model, line + 6, sizeof(g_model) - 1);
        char * nl = strchr(g_model, '\n'); if (nl) *nl = 0;
    }
    pclose(f);
}

static int cmd_init(int accept_terms) {
    if (!accept_terms) {
        fprintf(stderr, "run: moontail init --accept-terms  (see docs/VOLUNTEER_TERMS.md)\n");
        return 1;
    }
    config_load();
    ensure_peer_token();
    if (!g_peer_id[0]) snprintf(g_peer_id, sizeof(g_peer_id), "mt-%ld", (long)time(NULL));
    if (!g_model[0]) {
        fprintf(stderr, "Pulling Kimi K2-Instruct from Hugging Face (needs HF_TOKEN)...\n");
        if (system("bash scripts/pull-k2.sh") != 0)
            fprintf(stderr, "pull failed — set HF_TOKEN or place GGUF in ~/.moontail/\n");
        pull_model_path();
    }
    if (!g_transport[0]) {
        const char * tr = getenv("MOONTAIL_TRANSPORT");
        if (tr) strncpy(g_transport, tr, sizeof(g_transport) - 1);
    }
    config_save();
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "%s --worker \"%s\" --peer-id \"%s\" --peer-token \"%s\" --model \"%s\" --experts 0 383",
        g_volunteer, g_worker, g_peer_id, g_peer_token, g_model);
    if (!strcmp(g_transport, "tailscale")) {
        strncat(cmd, " --transport tailscale", sizeof(cmd) - strlen(cmd) - 1);
        if (g_tailscale_host[0]) {
            strncat(cmd, " --tailscale-host \"", sizeof(cmd) - strlen(cmd) - 1);
            strncat(cmd, g_tailscale_host, sizeof(cmd) - strlen(cmd) - 1);
            strncat(cmd, "\"", sizeof(cmd) - strlen(cmd) - 1);
        }
    } else if (g_tunnel_host[0]) {
        strncat(cmd, " --tunnel-host \"", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, g_tunnel_host, sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, "\"", sizeof(cmd) - strlen(cmd) - 1);
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
            printf("Volunteer confirmed. Swarm ready (%d GPUs).\nRun: moontail prompt \"...\"\n", pool);
            return 0;
        }
        printf("Waiting for %d more user(s) to run moontail init (%d joined).\n", wait, pool);
        fflush(stdout);
        mt_sleep(15);
    }
}

static int cmd_prompt(const char * prompt) {
    config_load();
    if (!g_peer_id[0] || !g_peer_token[0] || !g_model[0]) {
        fprintf(stderr, "run moontail init --accept-terms first\n");
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
    if (spawn_tunnel(&s)) { release_session(); return 1; }
    if (strcmp(s.transport, "tailscale"))
        if (wait_tcp(s.local_port, 15000)) { release_session(); return 1; }
    if (!g_quiet && s.transport[0]) fprintf(stderr, "moontail → %s via %s (%s)\n", g_llama, s.transport, s.rpc_endpoint);
    int rc = run_llama_prompt(&s, prompt);
    release_session();
    return rc != 0;
}

static void usage(const char * p) {
    moontail_print_moon(1);
    fprintf(stderr,
        "  %s init --accept-terms   Volunteer GPU + join swarm\n"
        "  %s status                Pool / queue\n"
        "  %s prompt \"text\"        Run queued Kimi K2 prompt\n"
        "  --skip-tunnel            Dev only (localhost worker)\n",
        p, p, p);
}

int main(int argc, char ** argv) {
    config_load();
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--skip-tunnel")) {
            if (!worker_is_localhost()) {
                fprintf(stderr, "refused: --skip-tunnel (worker is not localhost)\n");
                return 1;
            }
            g_skip_tunnel = 1;
        } else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) g_quiet = 1;
    }
    if (argc < 2) { usage(argv[0]); return 0; }
    if (!strcmp(argv[1], "init")) {
        int ok = 0;
        for (int i = 2; i < argc; i++) if (!strcmp(argv[i], "--accept-terms")) ok = 1;
        return cmd_init(ok);
    }
    if (!strcmp(argv[1], "status")) return cmd_status();
    if (!strcmp(argv[1], "prompt") && argc > 2) return cmd_prompt(argv[2]);
    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) { usage(argv[0]); return 0; }
    usage(argv[0]);
    return 1;
}
