// =============================================================================
// netcode.h — JJK Fighter: UDP Networking Layer (ENet)
// =============================================================================

#ifndef NETCODE_H
#define NETCODE_H

#include <stdbool.h>
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
} NetInput;

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
void NetSendInput(NetInput input);
bool NetReceiveInput(NetInput* outInput);

#endif // NETCODE_H