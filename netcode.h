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
#define NET_MAX_WAITING_PLAYERS 32

typedef struct {
    bool left;
    bool right;
    bool jump;
    bool crouch;
    bool attack;
    bool rct;
    bool domain;
    bool dodge;
    bool ability1;
    bool ability2;
    bool ability3;
    bool ult;
} NetInput;

typedef enum {
    NET_MSG_NONE = 0,
    NET_MSG_AUTH,
    NET_MSG_AUTH_RESULT,
    NET_MSG_STATUS,
    NET_MSG_PLAYER_LIST_REQUEST,
    NET_MSG_PLAYER_LIST,
    NET_MSG_QUEUE_JOIN,
    NET_MSG_QUEUE_LEAVE,
    NET_MSG_DIRECT_CHALLENGE,
    NET_MSG_DIRECT_CHALLENGE_NOTIFY,
    NET_MSG_DIRECT_CHALLENGE_RESPONSE,
    NET_MSG_MATCH_FOUND,
    NET_MSG_MATCH_LEAVE,
    NET_MSG_ROSTER,
    NET_MSG_INPUT
} NetMessageType;

extern bool gNetConnected;

typedef struct {
    char username[NET_USERNAME_LEN];
} NetAuthMessage;

typedef struct {
    int success;
    char message[NET_STATUS_TEXT_LEN];
} NetAuthResultMessage;

typedef struct {
    char message[NET_STATUS_TEXT_LEN];
} NetStatusMessage;

typedef struct {
    char opponentUsername[NET_USERNAME_LEN];
} NetDirectChallengeMessage;

typedef struct {
    char fromUsername[NET_USERNAME_LEN];
} NetDirectChallengeNotifyMessage;

typedef struct {
    char opponentUsername[NET_USERNAME_LEN];
    int accepted;
} NetDirectChallengeResponseMessage;

typedef struct {
    char username[NET_USERNAME_LEN];
} NetWaitingPlayerEntry;

typedef struct {
    int count;
    NetWaitingPlayerEntry players[NET_MAX_WAITING_PLAYERS];
} NetPlayerListMessage;

typedef struct {
    int localPlayerIndex;
    char opponentUsername[NET_USERNAME_LEN];
} NetMatchFoundMessage;

typedef struct {
    int cursor;
    int selected;
    int confirmed;
} NetRosterState;

bool NetInit(void);
void NetCleanup(void);
void NetDisconnectOnly(void);
bool NetConnectRelay(const char* ipAddress, int port);
bool NetSendAuth(const char* username);
bool NetJoinQueue(void);
bool NetLeaveQueue(void);
bool NetRequestPlayerList(void);
bool NetSendDirectChallenge(const char* opponentUsername);
bool NetSendChallengeResponse(const char* opponentUsername, bool accepted);
bool NetSendRosterState(const NetRosterState* roster);
bool NetSendInput(NetInput input);
bool NetSendMatchLeave(void);
bool NetSendMessage(NetMessageType type, const void* payload, size_t payloadSize, enet_uint32 flags);
bool NetPollMessage(NetMessageType* outType, void* outPayload, size_t payloadCapacity, size_t* outPayloadSize);
void NetPump(void);

#endif
