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

#define NET_USERNAME_LEN 24
#define NET_STATUS_TEXT_LEN 96
#define NET_HOST_IP_LEN 64
#define NET_MAX_LOBBY_PLAYERS 32

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
    NET_MSG_MATCH_STATE,
    NET_MSG_LOBBY_REGISTER,
    NET_MSG_LOBBY_REGISTER_RESULT,
    NET_MSG_LOBBY_CHALLENGE_REQUEST,
    NET_MSG_LOBBY_CHALLENGE_NOTIFY,
    NET_MSG_LOBBY_CHALLENGE_RESPONSE,
    NET_MSG_LOBBY_QUEUE_JOIN,
    NET_MSG_LOBBY_QUEUE_STATUS,
    NET_MSG_LOBBY_MATCH_FOUND,
    NET_MSG_LOBBY_PLAYER_LIST
} NetMessageType;

typedef enum {
    NET_ROLE_NONE = 0,
    NET_ROLE_HOST,   // Player 1 (Server)
    NET_ROLE_CLIENT  // Player 2 (Joins the Server)
} NetRole;

extern NetRole gNetRole;
extern bool gNetConnected;

typedef struct {
    char username[NET_USERNAME_LEN];
} LobbyRegisterMessage;

typedef struct {
    int success;
    char message[NET_STATUS_TEXT_LEN];
} LobbyRegisterResultMessage;

typedef struct {
    char targetUsername[NET_USERNAME_LEN];
} LobbyChallengeRequestMessage;

typedef struct {
    char fromUsername[NET_USERNAME_LEN];
} LobbyChallengeNotifyMessage;

typedef struct {
    char fromUsername[NET_USERNAME_LEN];
    int accepted;
} LobbyChallengeResponseMessage;

typedef struct {
    int queued;
    char message[NET_STATUS_TEXT_LEN];
} LobbyQueueStatusMessage;

typedef struct {
    int role;
    int gamePort;
    char opponentUsername[NET_USERNAME_LEN];
    char hostIp[NET_HOST_IP_LEN];
} LobbyMatchFoundMessage;

typedef struct {
    char username[NET_USERNAME_LEN];
    int available;
    int queued;
} LobbyPlayerEntry;

typedef struct {
    int count;
    LobbyPlayerEntry players[NET_MAX_LOBBY_PLAYERS];
} LobbyPlayerListMessage;

// Core API
bool NetInit(void);
void NetCleanup(void);
int NetRunLobbyServer(int port);

// Connection Handling
bool NetHostCreate(int port);
bool NetClientConnect(const char* ipAddress, int port);

// Gameplay Sync
bool NetSendInput(NetInput input);
bool NetSendMessage(NetMessageType type, const void* payload, size_t payloadSize, enet_uint32 flags);
bool NetPollMessage(NetMessageType* outType, void* outPayload, size_t payloadCapacity, size_t* outPayloadSize);
void NetPump(void);

#endif // NETCODE_H
