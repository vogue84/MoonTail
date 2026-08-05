#ifndef K3_PROTOCOL_H
#define K3_PROTOCOL_H

#define K3_RPC_HOST        "127.0.0.1"
#define K3_RPC_PORT        50052
#define K3_LOCAL_PROXY_PORT 50053

#define K3_MAX_URL         512
#define K3_MAX_TOKEN       2048
#define K3_MAX_HOST        256
#define K3_MAX_EXPERTS     384
#define K3_DEFAULT_RPC_PORT 50052

typedef struct k3_session {
    char transport[32];
    char tunnel_host[K3_MAX_HOST];
    char rpc_host[K3_MAX_HOST];
    char access_token[K3_MAX_TOKEN];
    char access_client_id[K3_MAX_TOKEN];
    char access_client_secret[K3_MAX_TOKEN];
    int  rpc_port;
    int  local_port;
    char rpc_endpoint[320]; /* host:port for llama -rpc */
} k3_session;

typedef struct k3_register {
    char peer_id[64];
    char tunnel_host[K3_MAX_HOST];
    int  expert_start;
    int  expert_end;
    int  busy; /* 0 idle, 1 in session */
} k3_register;

#endif
