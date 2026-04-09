// =============================================================================
// netcode.c — JJK Fighter: UDP Networking Layer Implementation
// =============================================================================

#include "netcode.h"
#include <stdio.h>
#include <string.h>

NetRole gNetRole = NET_ROLE_NONE;
bool gNetConnected = false;

static ENetHost* localNode = NULL;
static ENetPeer* connectedPeer = NULL;

typedef struct {
    uint8_t type;
    uint16_t payloadSize;
} NetMessageHeader;

typedef enum {
    LOBBY_CLIENT_EMPTY = 0,
    LOBBY_CLIENT_REGISTERED,
    LOBBY_CLIENT_QUEUED,
    LOBBY_CLIENT_PENDING_OUT,
    LOBBY_CLIENT_PENDING_IN,
    LOBBY_CLIENT_MATCHING
} LobbyClientState;

typedef struct {
    ENetPeer* peer;
    char username[NET_USERNAME_LEN];
    LobbyClientState state;
    int pendingIndex;
} LobbyClient;

static void NetResetState(void) {
    localNode = NULL;
    connectedPeer = NULL;
    gNetRole = NET_ROLE_NONE;
    gNetConnected = false;
}

bool NetInit(void) {
    if (enet_initialize() != 0) {
        printf("ERROR: Failed to initialize ENet.\n");
        return false;
    }
    return true;
}

void NetCleanup(void) {
    if (connectedPeer != NULL && gNetConnected) {
        enet_peer_disconnect_now(connectedPeer, 0);
    }
    if (localNode != NULL) enet_host_destroy(localNode);
    NetResetState();
    enet_deinitialize();
}

bool NetHostCreate(int port) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    // Create server: 1 outgoing connection, 2 channels, unlimited bandwidth
    localNode = enet_host_create(&address, 1, 2, 0, 0);
    if (localNode == NULL) return false;

    gNetRole = NET_ROLE_HOST;
    gNetConnected = false;
    printf("HOSTING on port %d... Waiting for challenger.\n", port);
    return true;
}

bool NetClientConnect(const char* ipAddress, int port) {
    localNode = enet_host_create(NULL, 1, 2, 0, 0);
    if (localNode == NULL) return false;

    ENetAddress address;
    enet_address_set_host(&address, ipAddress);
    address.port = port;

    // Initiate connection
    connectedPeer = enet_host_connect(localNode, &address, 2, 0);
    if (connectedPeer == NULL) return false;

    gNetRole = NET_ROLE_CLIENT;
    gNetConnected = false;
    printf("CONNECTING to %s:%d...\n", ipAddress, port);
    return true;
}

bool NetSendMessage(NetMessageType type, const void* payload, size_t payloadSize, enet_uint32 flags) {
    if (!gNetConnected || connectedPeer == NULL) return false;
    if (payloadSize > 65535) return false;

    size_t totalSize = sizeof(NetMessageHeader) + payloadSize;
    unsigned char buffer[2048];
    if (totalSize > sizeof(buffer)) return false;

    NetMessageHeader header;
    header.type = (uint8_t)type;
    header.payloadSize = (uint16_t)payloadSize;

    memcpy(buffer, &header, sizeof(header));
    if (payloadSize > 0 && payload != NULL) {
        memcpy(buffer + sizeof(header), payload, payloadSize);
    }

    ENetPacket* packet = enet_packet_create(buffer, totalSize, flags);
    enet_peer_send(connectedPeer, 0, packet);
    return true;
}

bool NetSendInput(NetInput input) {
    return NetSendMessage(NET_MSG_INPUT, &input, sizeof(NetInput), ENET_PACKET_FLAG_UNSEQUENCED);
}

static bool NetHandleEvent(ENetEvent* event, NetMessageType* outType,
                           void* outPayload, size_t payloadCapacity, size_t* outPayloadSize) {
    if (event == NULL) return false;

    switch (event->type) {
        case ENET_EVENT_TYPE_CONNECT:
            printf(">>> A NEW SORCERER HAS ENTERED THE DOMAIN <<<\n");
            connectedPeer = event->peer;
            gNetConnected = true;
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            if (event->packet->dataLength >= sizeof(NetMessageHeader)) {
                NetMessageHeader header;
                memcpy(&header, event->packet->data, sizeof(NetMessageHeader));
                size_t payloadSize = (size_t)header.payloadSize;
                size_t availablePayload = event->packet->dataLength - sizeof(NetMessageHeader);
                if (payloadSize <= availablePayload) {
                    if (outType != NULL) *outType = (NetMessageType)header.type;
                    if (outPayloadSize != NULL) *outPayloadSize = payloadSize;
                    if (outPayload != NULL && payloadSize > 0) {
                        size_t copySize = (payloadSize < payloadCapacity) ? payloadSize : payloadCapacity;
                        memcpy(outPayload, event->packet->data + sizeof(NetMessageHeader), copySize);
                    }
                    enet_packet_destroy(event->packet);
                    return true;
                }
            }
            enet_packet_destroy(event->packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            printf(">>> OPPONENT DISCONNECTED <<<\n");
            gNetConnected = false;
            connectedPeer = NULL;
            break;

        case ENET_EVENT_TYPE_NONE:
            break;
    }

    return false;
}

bool NetPollMessage(NetMessageType* outType, void* outPayload, size_t payloadCapacity, size_t* outPayloadSize) {
    if (localNode == NULL) return false;

    if (outType != NULL) *outType = NET_MSG_NONE;
    if (outPayloadSize != NULL) *outPayloadSize = 0;
    ENetEvent event;
    while (enet_host_service(localNode, &event, 0) > 0) {
        if (NetHandleEvent(&event, outType, outPayload, payloadCapacity, outPayloadSize)) {
            return true;
        }
    }
    return false;
}

void NetPump(void) {
    NetPollMessage(NULL, NULL, 0, NULL);
}

static bool ServerSendMessage(ENetPeer* peer, NetMessageType type, const void* payload, size_t payloadSize, enet_uint32 flags) {
    if (peer == NULL || payloadSize > 65535) return false;

    size_t totalSize = sizeof(NetMessageHeader) + payloadSize;
    unsigned char buffer[2048];
    if (totalSize > sizeof(buffer)) return false;

    NetMessageHeader header;
    header.type = (uint8_t)type;
    header.payloadSize = (uint16_t)payloadSize;
    memcpy(buffer, &header, sizeof(header));
    if (payloadSize > 0 && payload != NULL) {
        memcpy(buffer + sizeof(header), payload, payloadSize);
    }

    ENetPacket* packet = enet_packet_create(buffer, totalSize, flags);
    enet_peer_send(peer, 0, packet);
    return true;
}

static LobbyClient* FindLobbyClientByPeer(LobbyClient* clients, int maxClients, ENetPeer* peer) {
    for (int i = 0; i < maxClients; i++) {
        if (clients[i].peer == peer) return &clients[i];
    }
    return NULL;
}

static LobbyClient* FindLobbyClientByUsername(LobbyClient* clients, int maxClients, const char* username) {
    for (int i = 0; i < maxClients; i++) {
        if (clients[i].peer != NULL && strcmp(clients[i].username, username) == 0) return &clients[i];
    }
    return NULL;
}

static LobbyClient* AllocLobbyClient(LobbyClient* clients, int maxClients, ENetPeer* peer) {
    LobbyClient* existing = FindLobbyClientByPeer(clients, maxClients, peer);
    if (existing != NULL) return existing;

    for (int i = 0; i < maxClients; i++) {
        if (clients[i].peer == NULL) {
            memset(&clients[i], 0, sizeof(LobbyClient));
            clients[i].peer = peer;
            clients[i].pendingIndex = -1;
            return &clients[i];
        }
    }
    return NULL;
}

static int LobbyClientIndex(LobbyClient* clients, int maxClients, LobbyClient* client) {
    for (int i = 0; i < maxClients; i++) {
        if (&clients[i] == client) return i;
    }
    return -1;
}

static void ResetPendingState(LobbyClient* clients, int maxClients, LobbyClient* client) {
    if (client == NULL) return;
    int otherIndex = client->pendingIndex;
    if (otherIndex >= 0 && otherIndex < maxClients) {
        LobbyClient* other = &clients[otherIndex];
        if (other->peer != NULL) {
            other->state = LOBBY_CLIENT_REGISTERED;
            other->pendingIndex = -1;
        }
    }
    client->state = LOBBY_CLIENT_REGISTERED;
    client->pendingIndex = -1;
}

static void PairLobbyClients(LobbyClient* hostClient, LobbyClient* clientClient) {
    if (hostClient == NULL || clientClient == NULL) return;

    LobbyMatchFoundMessage hostMsg = {0};
    hostMsg.role = NET_ROLE_HOST;
    hostMsg.gamePort = 7777;
    snprintf(hostMsg.opponentUsername, sizeof(hostMsg.opponentUsername), "%s", clientClient->username);
    snprintf(hostMsg.hostIp, sizeof(hostMsg.hostIp), "127.0.0.1");
    ServerSendMessage(hostClient->peer, NET_MSG_LOBBY_MATCH_FOUND, &hostMsg, sizeof(hostMsg), ENET_PACKET_FLAG_RELIABLE);

    LobbyMatchFoundMessage clientMsg = {0};
    clientMsg.role = NET_ROLE_CLIENT;
    clientMsg.gamePort = 7777;
    snprintf(clientMsg.opponentUsername, sizeof(clientMsg.opponentUsername), "%s", hostClient->username);
    enet_address_get_host_ip(&hostClient->peer->address, clientMsg.hostIp, sizeof(clientMsg.hostIp));
    ServerSendMessage(clientClient->peer, NET_MSG_LOBBY_MATCH_FOUND, &clientMsg, sizeof(clientMsg), ENET_PACKET_FLAG_RELIABLE);

    hostClient->state = LOBBY_CLIENT_MATCHING;
    clientClient->state = LOBBY_CLIENT_MATCHING;
    hostClient->pendingIndex = -1;
    clientClient->pendingIndex = -1;
}

static void TryMatchQueuedPlayers(LobbyClient* clients, int maxClients) {
    LobbyClient* first = NULL;
    LobbyClient* second = NULL;
    for (int i = 0; i < maxClients; i++) {
        if (clients[i].peer != NULL && clients[i].state == LOBBY_CLIENT_QUEUED) {
            if (first == NULL) first = &clients[i];
            else {
                second = &clients[i];
                break;
            }
        }
    }

    if (first != NULL && second != NULL) {
        PairLobbyClients(first, second);
    }
}

static void BroadcastLobbyPlayerList(ENetHost* server, LobbyClient* clients, int maxClients) {
    (void)server;
    LobbyPlayerListMessage list = {0};
    for (int i = 0; i < maxClients && list.count < NET_MAX_LOBBY_PLAYERS; i++) {
        if (clients[i].peer == NULL || clients[i].username[0] == '\0') continue;
        LobbyPlayerEntry* entry = &list.players[list.count++];
        snprintf(entry->username, sizeof(entry->username), "%s", clients[i].username);
        entry->available = (clients[i].state == LOBBY_CLIENT_REGISTERED || clients[i].state == LOBBY_CLIENT_QUEUED);
        entry->queued = (clients[i].state == LOBBY_CLIENT_QUEUED);
    }

    for (int i = 0; i < maxClients; i++) {
        if (clients[i].peer != NULL) {
            ServerSendMessage(clients[i].peer, NET_MSG_LOBBY_PLAYER_LIST, &list, sizeof(list), ENET_PACKET_FLAG_RELIABLE);
        }
    }
}

int NetRunLobbyServer(int port) {
    if (enet_initialize() != 0) {
        printf("ERROR: Failed to initialize ENet lobby server.\n");
        return 1;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = (enet_uint16)port;
    ENetHost* server = enet_host_create(&address, 64, 2, 0, 0);
    if (server == NULL) {
        printf("ERROR: Could not start lobby server on port %d.\n", port);
        enet_deinitialize();
        return 1;
    }

    LobbyClient clients[64];
    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < 64; i++) clients[i].pendingIndex = -1;

    printf("URUSAI MANIA lobby server running on port %d\n", port);

    while (1) {
        ENetEvent event;
        while (enet_host_service(server, &event, 16) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                LobbyClient* client = AllocLobbyClient(clients, 64, event.peer);
                if (client == NULL) {
                    enet_peer_disconnect_now(event.peer, 0);
                }
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                LobbyClient* client = FindLobbyClientByPeer(clients, 64, event.peer);
                if (client != NULL) {
                    ResetPendingState(clients, 64, client);
                    memset(client, 0, sizeof(LobbyClient));
                    client->pendingIndex = -1;
                }
                TryMatchQueuedPlayers(clients, 64);
                BroadcastLobbyPlayerList(server, clients, 64);
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                LobbyClient* client = FindLobbyClientByPeer(clients, 64, event.peer);
                if (client == NULL || event.packet->dataLength < sizeof(NetMessageHeader)) {
                    enet_packet_destroy(event.packet);
                    continue;
                }

                NetMessageHeader header;
                memcpy(&header, event.packet->data, sizeof(header));
                const unsigned char* payload = event.packet->data + sizeof(header);
                size_t payloadSize = event.packet->dataLength - sizeof(header);

                if (header.type == NET_MSG_LOBBY_REGISTER && payloadSize >= sizeof(LobbyRegisterMessage)) {
                    const LobbyRegisterMessage* msg = (const LobbyRegisterMessage*)payload;
                    LobbyRegisterResultMessage result = {0};
                    if (msg->username[0] == '\0') {
                        snprintf(result.message, sizeof(result.message), "Username cannot be empty.");
                    } else if (FindLobbyClientByUsername(clients, 64, msg->username) != NULL &&
                               strcmp(client->username, msg->username) != 0) {
                        snprintf(result.message, sizeof(result.message), "Username is already taken.");
                    } else {
                        snprintf(client->username, sizeof(client->username), "%s", msg->username);
                        client->state = LOBBY_CLIENT_REGISTERED;
                        result.success = 1;
                        snprintf(result.message, sizeof(result.message), "Connected as %s", client->username);
                    }
                    ServerSendMessage(client->peer, NET_MSG_LOBBY_REGISTER_RESULT, &result, sizeof(result), ENET_PACKET_FLAG_RELIABLE);
                    BroadcastLobbyPlayerList(server, clients, 64);
                } else if (header.type == NET_MSG_LOBBY_CHALLENGE_REQUEST && payloadSize >= sizeof(LobbyChallengeRequestMessage)) {
                    const LobbyChallengeRequestMessage* msg = (const LobbyChallengeRequestMessage*)payload;
                    LobbyQueueStatusMessage status = {0};
                    LobbyClient* target = FindLobbyClientByUsername(clients, 64, msg->targetUsername);
                    if (client->state == LOBBY_CLIENT_EMPTY || client->username[0] == '\0') {
                        snprintf(status.message, sizeof(status.message), "Register a username first.");
                    } else if (target == NULL || target == client) {
                        snprintf(status.message, sizeof(status.message), "That username is not available.");
                    } else if (target->state != LOBBY_CLIENT_REGISTERED) {
                        snprintf(status.message, sizeof(status.message), "Target player is busy right now.");
                    } else {
                        int clientIndex = LobbyClientIndex(clients, 64, client);
                        int targetIndex = LobbyClientIndex(clients, 64, target);
                        client->state = LOBBY_CLIENT_PENDING_OUT;
                        client->pendingIndex = targetIndex;
                        target->state = LOBBY_CLIENT_PENDING_IN;
                        target->pendingIndex = clientIndex;
                        LobbyChallengeNotifyMessage notify = {0};
                        snprintf(notify.fromUsername, sizeof(notify.fromUsername), "%s", client->username);
                        ServerSendMessage(target->peer, NET_MSG_LOBBY_CHALLENGE_NOTIFY, &notify, sizeof(notify), ENET_PACKET_FLAG_RELIABLE);
                        status.queued = 1;
                        snprintf(status.message, sizeof(status.message), "Challenge sent to %s", target->username);
                    }
                    ServerSendMessage(client->peer, NET_MSG_LOBBY_QUEUE_STATUS, &status, sizeof(status), ENET_PACKET_FLAG_RELIABLE);
                    BroadcastLobbyPlayerList(server, clients, 64);
                } else if (header.type == NET_MSG_LOBBY_CHALLENGE_RESPONSE && payloadSize >= sizeof(LobbyChallengeResponseMessage)) {
                    const LobbyChallengeResponseMessage* msg = (const LobbyChallengeResponseMessage*)payload;
                    LobbyClient* challenger = FindLobbyClientByUsername(clients, 64, msg->fromUsername);
                    if (challenger != NULL) {
                        LobbyQueueStatusMessage status = {0};
                        if (msg->accepted) {
                            PairLobbyClients(challenger, client);
                        } else {
                            snprintf(status.message, sizeof(status.message), "%s declined your 1v1 request.", client->username);
                            ServerSendMessage(challenger->peer, NET_MSG_LOBBY_QUEUE_STATUS, &status, sizeof(status), ENET_PACKET_FLAG_RELIABLE);
                            ResetPendingState(clients, 64, challenger);
                            ResetPendingState(clients, 64, client);
                        }
                        BroadcastLobbyPlayerList(server, clients, 64);
                    }
                } else if (header.type == NET_MSG_LOBBY_QUEUE_JOIN) {
                    LobbyQueueStatusMessage status = {0};
                    if (client->username[0] == '\0') {
                        snprintf(status.message, sizeof(status.message), "Register a username first.");
                    } else {
                        client->state = LOBBY_CLIENT_QUEUED;
                        client->pendingIndex = -1;
                        status.queued = 1;
                        snprintf(status.message, sizeof(status.message), "Searching for a global match...");
                        ServerSendMessage(client->peer, NET_MSG_LOBBY_QUEUE_STATUS, &status, sizeof(status), ENET_PACKET_FLAG_RELIABLE);
                        TryMatchQueuedPlayers(clients, 64);
                        BroadcastLobbyPlayerList(server, clients, 64);
                    }
                }

                enet_packet_destroy(event.packet);
            }
        }
    }

    return 0;
}
