#include "netcode.h"
#include <enet/enet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVER_MAX_CLIENTS 128
#define SERVER_DEFAULT_PORT 8999

typedef struct {
    uint8_t type;
    uint16_t payloadSize;
} NetMessageHeader;

typedef struct {
    ENetPeer* peer;
    char username[NET_USERNAME_LEN];
    bool authenticated;
    bool queued;
    int opponentIndex;
    int pendingFromIndex;
} RelayClient;

static bool SendPacket(ENetPeer* peer, NetMessageType type, const void* payload, size_t payloadSize, enet_uint32 flags) {
    unsigned char buffer[2048];
    NetMessageHeader header;
    size_t totalSize;

    if (peer == NULL || payloadSize > 65535) {
        return false;
    }

    totalSize = sizeof(header) + payloadSize;
    if (totalSize > sizeof(buffer)) {
        return false;
    }

    header.type = (uint8_t)type;
    header.payloadSize = (uint16_t)payloadSize;
    memcpy(buffer, &header, sizeof(header));
    if (payloadSize > 0 && payload != NULL) {
        memcpy(buffer + sizeof(header), payload, payloadSize);
    }

    enet_peer_send(peer, 0, enet_packet_create(buffer, totalSize, flags));
    return true;
}

static void SendStatus(ENetPeer* peer, const char* text) {
    NetStatusMessage msg = {0};
    snprintf(msg.message, sizeof(msg.message), "%s", text);
    SendPacket(peer, NET_MSG_STATUS, &msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
}

static void SendAuthResult(ENetPeer* peer, bool success, const char* text) {
    NetAuthResultMessage msg = {0};
    msg.success = success ? 1 : 0;
    snprintf(msg.message, sizeof(msg.message), "%s", text);
    SendPacket(peer, NET_MSG_AUTH_RESULT, &msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
}

static RelayClient* FindClientByPeer(RelayClient* clients, ENetPeer* peer) {
    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        if (clients[i].peer == peer) {
            return &clients[i];
        }
    }
    return NULL;
}

static int FindClientIndexByUsername(RelayClient* clients, const char* username) {
    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        if (clients[i].peer != NULL && clients[i].authenticated && strcmp(clients[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

static RelayClient* AllocateClient(RelayClient* clients, ENetPeer* peer) {
    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        if (clients[i].peer == NULL) {
            memset(&clients[i], 0, sizeof(clients[i]));
            clients[i].peer = peer;
            clients[i].opponentIndex = -1;
            clients[i].pendingFromIndex = -1;
            return &clients[i];
        }
    }
    return NULL;
}

static void BroadcastWaitingPlayers(RelayClient* clients) {
    NetPlayerListMessage list = {0};

    for (int i = 0; i < SERVER_MAX_CLIENTS && list.count < NET_MAX_WAITING_PLAYERS; i++) {
        if (!clients[i].authenticated || !clients[i].queued || clients[i].opponentIndex >= 0) {
            continue;
        }
        snprintf(list.players[list.count].username, sizeof(list.players[list.count].username), "%s", clients[i].username);
        list.count++;
    }

    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        if (clients[i].peer != NULL && clients[i].authenticated) {
            SendPacket(clients[i].peer, NET_MSG_PLAYER_LIST, &list, sizeof(list), ENET_PACKET_FLAG_RELIABLE);
        }
    }
}

static void ClearPendingForAll(RelayClient* clients, int clientIndex) {
    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        if (clients[i].pendingFromIndex == clientIndex) {
            clients[i].pendingFromIndex = -1;
        }
    }
}

static void EndMatch(RelayClient* clients, int clientIndex, const char* reason, bool notifyOpponent) {
    RelayClient* client = &clients[clientIndex];
    int opponentIndex = client->opponentIndex;

    client->opponentIndex = -1;
    client->queued = false;
    client->pendingFromIndex = -1;

    if (opponentIndex >= 0 && opponentIndex < SERVER_MAX_CLIENTS && clients[opponentIndex].peer != NULL) {
        RelayClient* opponent = &clients[opponentIndex];
        opponent->opponentIndex = -1;
        opponent->queued = false;
        opponent->pendingFromIndex = -1;
        if (notifyOpponent) {
            if (reason != NULL && reason[0] != '\0') {
                SendStatus(opponent->peer, reason);
            }
            SendPacket(opponent->peer, NET_MSG_MATCH_LEAVE, NULL, 0, ENET_PACKET_FLAG_RELIABLE);
        }
    }

    BroadcastWaitingPlayers(clients);
}

static void CreateMatch(RelayClient* clients, int firstIndex, int secondIndex) {
    NetMatchFoundMessage first = {0};
    NetMatchFoundMessage second = {0};

    clients[firstIndex].queued = false;
    clients[secondIndex].queued = false;
    clients[firstIndex].pendingFromIndex = -1;
    clients[secondIndex].pendingFromIndex = -1;
    clients[firstIndex].opponentIndex = secondIndex;
    clients[secondIndex].opponentIndex = firstIndex;

    first.localPlayerIndex = 0;
    second.localPlayerIndex = 1;
    snprintf(first.opponentUsername, sizeof(first.opponentUsername), "%s", clients[secondIndex].username);
    snprintf(second.opponentUsername, sizeof(second.opponentUsername), "%s", clients[firstIndex].username);

    SendPacket(clients[firstIndex].peer, NET_MSG_MATCH_FOUND, &first, sizeof(first), ENET_PACKET_FLAG_RELIABLE);
    SendPacket(clients[secondIndex].peer, NET_MSG_MATCH_FOUND, &second, sizeof(second), ENET_PACKET_FLAG_RELIABLE);
    BroadcastWaitingPlayers(clients);
}

static void TryAutoMatchQueue(RelayClient* clients) {
    int firstIndex = -1;
    int secondIndex = -1;

    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        if (clients[i].authenticated && clients[i].queued && clients[i].opponentIndex < 0) {
            if (firstIndex < 0) {
                firstIndex = i;
            } else {
                secondIndex = i;
                break;
            }
        }
    }

    if (firstIndex >= 0 && secondIndex >= 0) {
        CreateMatch(clients, firstIndex, secondIndex);
    }
}

static void HandleAuth(RelayClient* clients, RelayClient* client, const NetAuthMessage* auth) {
    if (auth->username[0] == '\0') {
        SendAuthResult(client->peer, false, "Username cannot be empty.");
        return;
    }

    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        if (&clients[i] != client && clients[i].authenticated && strcmp(clients[i].username, auth->username) == 0) {
            SendAuthResult(client->peer, false, "That username is already online.");
            return;
        }
    }

    snprintf(client->username, sizeof(client->username), "%s", auth->username);
    client->authenticated = true;
    client->queued = false;
    client->opponentIndex = -1;
    client->pendingFromIndex = -1;
    SendAuthResult(client->peer, true, "Signed in to the relay server.");
    BroadcastWaitingPlayers(clients);
}

static void HandleDirectChallenge(RelayClient* clients, RelayClient* client, int clientIndex, const NetDirectChallengeMessage* msg) {
    int targetIndex;
    NetDirectChallengeNotifyMessage notify = {0};

    if (!client->authenticated) {
        SendStatus(client->peer, "Sign in before sending a challenge.");
        return;
    }
    if (msg->opponentUsername[0] == '\0') {
        SendStatus(client->peer, "Enter an opponent username first.");
        return;
    }

    targetIndex = FindClientIndexByUsername(clients, msg->opponentUsername);
    if (targetIndex < 0) {
        SendStatus(client->peer, "That player is not online.");
        return;
    }
    if (targetIndex == clientIndex) {
        SendStatus(client->peer, "You cannot challenge yourself.");
        return;
    }
    if (clients[targetIndex].opponentIndex >= 0) {
        SendStatus(client->peer, "That player is already in a match.");
        return;
    }

    clients[targetIndex].pendingFromIndex = clientIndex;
    snprintf(notify.fromUsername, sizeof(notify.fromUsername), "%s", client->username);
    SendPacket(clients[targetIndex].peer, NET_MSG_DIRECT_CHALLENGE_NOTIFY, &notify, sizeof(notify), ENET_PACKET_FLAG_RELIABLE);
    SendStatus(client->peer, "Direct challenge sent.");
}

static void HandleChallengeResponse(RelayClient* clients, RelayClient* client, const NetDirectChallengeResponseMessage* msg) {
    int opponentIndex = FindClientIndexByUsername(clients, msg->opponentUsername);

    if (!client->authenticated || opponentIndex < 0) {
        SendStatus(client->peer, "That challenge is no longer available.");
        return;
    }
    if (client->pendingFromIndex != opponentIndex) {
        SendStatus(client->peer, "No matching incoming challenge was found.");
        return;
    }

    client->pendingFromIndex = -1;
    if (!msg->accepted) {
        SendStatus(client->peer, "Challenge declined.");
        if (clients[opponentIndex].peer != NULL) {
            SendStatus(clients[opponentIndex].peer, "Your challenge was declined.");
        }
        return;
    }

    if (clients[opponentIndex].opponentIndex >= 0) {
        SendStatus(client->peer, "That player is already busy.");
        return;
    }

    CreateMatch(clients, opponentIndex, (int)(client - clients));
}

static void HandleQueueJoin(RelayClient* clients, RelayClient* client) {
    if (!client->authenticated) {
        SendStatus(client->peer, "Sign in before joining matchmaking.");
        return;
    }
    if (client->opponentIndex >= 0) {
        SendStatus(client->peer, "You are already in a match.");
        return;
    }

    client->queued = true;
    SendStatus(client->peer, "You are now visible in global matchmaking.");
    BroadcastWaitingPlayers(clients);
}

static void HandleQueueLeave(RelayClient* clients, RelayClient* client) {
    client->queued = false;
    SendStatus(client->peer, "Left global matchmaking.");
    BroadcastWaitingPlayers(clients);
}

static void HandlePlayerListRequest(RelayClient* clients, RelayClient* client) {
    NetPlayerListMessage list = {0};

    for (int i = 0; i < SERVER_MAX_CLIENTS && list.count < NET_MAX_WAITING_PLAYERS; i++) {
        if (!clients[i].authenticated || !clients[i].queued || clients[i].opponentIndex >= 0) {
            continue;
        }
        if (&clients[i] == client) {
            continue;
        }
        snprintf(list.players[list.count].username, sizeof(list.players[list.count].username), "%s", clients[i].username);
        list.count++;
    }

    SendPacket(client->peer, NET_MSG_PLAYER_LIST, &list, sizeof(list), ENET_PACKET_FLAG_RELIABLE);
}

static void ForwardToOpponent(RelayClient* clients, RelayClient* client, ENetPacket* packet) {
    if (client->opponentIndex < 0 || client->opponentIndex >= SERVER_MAX_CLIENTS) {
        return;
    }
    if (clients[client->opponentIndex].peer == NULL) {
        return;
    }
    enet_peer_send(clients[client->opponentIndex].peer, 0,
                   enet_packet_create(packet->data, packet->dataLength, packet->flags));
}

int main(int argc, char** argv) {
    ENetAddress address;
    ENetHost* server;
    RelayClient clients[SERVER_MAX_CLIENTS];
    int port = SERVER_DEFAULT_PORT;

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0) {
            port = SERVER_DEFAULT_PORT;
        }
    }

    if (enet_initialize() != 0) {
        fprintf(stderr, "Failed to initialize ENet.\n");
        return 1;
    }

    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        clients[i].opponentIndex = -1;
        clients[i].pendingFromIndex = -1;
    }

    address.host = ENET_HOST_ANY;
    address.port = (enet_uint16)port;
    server = enet_host_create(&address, SERVER_MAX_CLIENTS, 2, 0, 0);
    if (server == NULL) {
        fprintf(stderr, "Could not create relay server on UDP port %d.\n", port);
        enet_deinitialize();
        return 1;
    }

    printf("Urusai Mania relay server listening on UDP %d\n", port);

    for (;;) {
        ENetEvent event;

        while (enet_host_service(server, &event, 16) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                RelayClient* client = AllocateClient(clients, event.peer);
                if (client == NULL) {
                    enet_peer_disconnect_now(event.peer, 0);
                } else {
                    SendStatus(event.peer, "Connected to relay. Sign in to begin.");
                }
                continue;
            }

            if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                RelayClient* client = FindClientByPeer(clients, event.peer);
                if (client != NULL) {
                    int clientIndex = (int)(client - clients);
                    ClearPendingForAll(clients, clientIndex);
                    if (client->opponentIndex >= 0) {
                        EndMatch(clients, clientIndex, "Opponent disconnected.", true);
                    }
                    memset(client, 0, sizeof(*client));
                    client->opponentIndex = -1;
                    client->pendingFromIndex = -1;
                    BroadcastWaitingPlayers(clients);
                }
                continue;
            }

            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                RelayClient* client = FindClientByPeer(clients, event.peer);
                NetMessageHeader header;
                const unsigned char* payload;
                size_t payloadSize;

                if (client == NULL || event.packet->dataLength < sizeof(header)) {
                    enet_packet_destroy(event.packet);
                    continue;
                }

                memcpy(&header, event.packet->data, sizeof(header));
                payload = event.packet->data + sizeof(header);
                payloadSize = event.packet->dataLength - sizeof(header);
                if (payloadSize < (size_t)header.payloadSize) {
                    enet_packet_destroy(event.packet);
                    continue;
                }

                switch ((NetMessageType)header.type) {
                    case NET_MSG_AUTH:
                        if (payloadSize >= sizeof(NetAuthMessage)) {
                            HandleAuth(clients, client, (const NetAuthMessage*)payload);
                        }
                        break;

                    case NET_MSG_DIRECT_CHALLENGE:
                        if (payloadSize >= sizeof(NetDirectChallengeMessage)) {
                            HandleDirectChallenge(clients, client, (int)(client - clients),
                                                  (const NetDirectChallengeMessage*)payload);
                        }
                        break;

                    case NET_MSG_DIRECT_CHALLENGE_RESPONSE:
                        if (payloadSize >= sizeof(NetDirectChallengeResponseMessage)) {
                            HandleChallengeResponse(clients, client, (const NetDirectChallengeResponseMessage*)payload);
                        }
                        break;

                    case NET_MSG_QUEUE_JOIN:
                        HandleQueueJoin(clients, client);
                        TryAutoMatchQueue(clients);
                        break;

                    case NET_MSG_QUEUE_LEAVE:
                        HandleQueueLeave(clients, client);
                        break;

                    case NET_MSG_PLAYER_LIST_REQUEST:
                        HandlePlayerListRequest(clients, client);
                        break;

                    case NET_MSG_ROSTER:
                    case NET_MSG_INPUT:
                        ForwardToOpponent(clients, client, event.packet);
                        break;

                    case NET_MSG_MATCH_LEAVE:
                        if (client->opponentIndex >= 0) {
                            EndMatch(clients, (int)(client - clients), "Opponent left the match.", true);
                        }
                        break;

                    default:
                        break;
                }

                enet_packet_destroy(event.packet);
            }
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}
