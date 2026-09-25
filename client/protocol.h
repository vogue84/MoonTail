#ifndef K3_PROTOCOL_H
#define K3_PROTOCOL_H

#define K3_RPC_HOST        "127.0.0.1"
#define K3_RPC_PORT        50052

#define K3_MAX_URL         512
#define K3_MAX_HOST        256
#define K3_MAX_EXPERTS        896
#define K3_DEFAULT_EXPERT_END 895
#define K3_DEFAULT_RPC_PORT   50052

typedef struct k3_session {
    char rpc_host[K3_MAX_HOST];
    int  rpc_port;
    char rpc_endpoint[320];
} k3_session;

#endif
