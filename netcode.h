// =============================================================================
// netcode.h — JJK Fighter: UDP Networking Layer (ENet)
// =============================================================================

#ifndef NETCODE_H
#define NETCODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#include <enet/enet.h>

// This is the tiny packet we send over the Wi-Fi 60 times a second
typedef struct {
    bool left;
    bool right;
    bool jump;
    bool crouch;
    bool attack;
    bool rct;
    bool domain;
    bool dodge;
    bool ult;
} NetInput;

typedef enum {
    NET_MSG_NONE = 0,
    NET_MSG_INPUT,
    NET_MSG_SELECT,
    NET_MSG_MATCH_STATE
} NetMessageType;

typedef enum {
    NET_ROLE_NONE = 0,
    NET_ROLE_HOST,   // Player 1 (Server)
    NET_ROLE_CLIENT  // Player 2 (Joins the Server)
} NetRole;

extern NetRole gNetRole;
extern bool gNetConnected;

// Core API
bool NetInit(void);
void NetCleanup(void);

// Connection Handling
bool NetHostCreate(int port);
bool NetClientConnect(const char* ipAddress, int port);

// Gameplay Sync
bool NetSendInput(NetInput input);
bool NetSendMessage(NetMessageType type, const void* payload, size_t payloadSize, enet_uint32 flags);
bool NetPollMessage(NetMessageType* outType, void* outPayload, size_t payloadCapacity, size_t* outPayloadSize);
void NetPump(void);

#endif // NETCODE_H
