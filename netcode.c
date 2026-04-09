#include "netcode.h"
#include <stdio.h>
#include <string.h>

bool gNetConnected = false;

typedef struct {
    uint8_t type;
    uint16_t payloadSize;
} NetMessageHeader;

static ENetHost* gClientHost = NULL;
static ENetPeer* gRelayPeer = NULL;

static void NetResetState(void) {
    gClientHost = NULL;
    gRelayPeer = NULL;
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
    if (gRelayPeer != NULL) {
        enet_peer_disconnect_now(gRelayPeer, 0);
    }
    if (gClientHost != NULL) {
        enet_host_destroy(gClientHost);
    }
    NetResetState();
    enet_deinitialize();
}

bool NetConnectRelay(const char* ipAddress, int port) {
    gClientHost = enet_host_create(NULL, 1, 2, 0, 0);
    if (gClientHost == NULL) {
        return false;
    }

    ENetAddress address;
    if (enet_address_set_host(&address, ipAddress) != 0) {
        enet_host_destroy(gClientHost);
        NetResetState();
        return false;
    }
    address.port = (enet_uint16)port;

    gRelayPeer = enet_host_connect(gClientHost, &address, 2, 0);
    if (gRelayPeer == NULL) {
        enet_host_destroy(gClientHost);
        NetResetState();
        return false;
    }

    gNetConnected = false;
    return true;
}

bool NetSendMessage(NetMessageType type, const void* payload, size_t payloadSize, enet_uint32 flags) {
    unsigned char buffer[2048];
    NetMessageHeader header;
    size_t totalSize;

    if (!gNetConnected || gRelayPeer == NULL || payloadSize > 65535) {
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

    enet_peer_send(gRelayPeer, 0, enet_packet_create(buffer, totalSize, flags));
    return true;
}

bool NetSendAuth(const char* username) {
    NetAuthMessage auth = {0};
    snprintf(auth.username, sizeof(auth.username), "%s", username ? username : "");
    return NetSendMessage(NET_MSG_AUTH, &auth, sizeof(auth), ENET_PACKET_FLAG_RELIABLE);
}

bool NetJoinQueue(void) {
    return NetSendMessage(NET_MSG_QUEUE_JOIN, NULL, 0, ENET_PACKET_FLAG_RELIABLE);
}

bool NetLeaveQueue(void) {
    return NetSendMessage(NET_MSG_QUEUE_LEAVE, NULL, 0, ENET_PACKET_FLAG_RELIABLE);
}

bool NetRequestPlayerList(void) {
    return NetSendMessage(NET_MSG_PLAYER_LIST_REQUEST, NULL, 0, ENET_PACKET_FLAG_RELIABLE);
}

bool NetSendDirectChallenge(const char* opponentUsername) {
    NetDirectChallengeMessage msg = {0};
    snprintf(msg.opponentUsername, sizeof(msg.opponentUsername), "%s", opponentUsername ? opponentUsername : "");
    return NetSendMessage(NET_MSG_DIRECT_CHALLENGE, &msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
}

bool NetSendChallengeResponse(const char* opponentUsername, bool accepted) {
    NetDirectChallengeResponseMessage msg = {0};
    snprintf(msg.opponentUsername, sizeof(msg.opponentUsername), "%s", opponentUsername ? opponentUsername : "");
    msg.accepted = accepted ? 1 : 0;
    return NetSendMessage(NET_MSG_DIRECT_CHALLENGE_RESPONSE, &msg, sizeof(msg), ENET_PACKET_FLAG_RELIABLE);
}

bool NetSendRosterState(const NetRosterState* roster) {
    return NetSendMessage(NET_MSG_ROSTER, roster, sizeof(NetRosterState), ENET_PACKET_FLAG_RELIABLE);
}

bool NetSendInput(NetInput input) {
    return NetSendMessage(NET_MSG_INPUT, &input, sizeof(NetInput), ENET_PACKET_FLAG_UNSEQUENCED);
}

bool NetSendMatchLeave(void) {
    return NetSendMessage(NET_MSG_MATCH_LEAVE, NULL, 0, ENET_PACKET_FLAG_RELIABLE);
}

static bool NetHandleEvent(ENetEvent* event, NetMessageType* outType,
                           void* outPayload, size_t payloadCapacity, size_t* outPayloadSize) {
    if (event == NULL) {
        return false;
    }

    switch (event->type) {
        case ENET_EVENT_TYPE_CONNECT:
            gRelayPeer = event->peer;
            gNetConnected = true;
            break;

        case ENET_EVENT_TYPE_RECEIVE: {
            if (event->packet->dataLength >= sizeof(NetMessageHeader)) {
                NetMessageHeader header;
                size_t payloadSize;
                size_t availablePayload;

                memcpy(&header, event->packet->data, sizeof(header));
                payloadSize = (size_t)header.payloadSize;
                availablePayload = event->packet->dataLength - sizeof(NetMessageHeader);

                if (payloadSize <= availablePayload) {
                    if (outType != NULL) {
                        *outType = (NetMessageType)header.type;
                    }
                    if (outPayloadSize != NULL) {
                        *outPayloadSize = payloadSize;
                    }
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
        }

        case ENET_EVENT_TYPE_DISCONNECT:
            gNetConnected = false;
            gRelayPeer = NULL;
            break;

        case ENET_EVENT_TYPE_NONE:
            break;
    }

    return false;
}

bool NetPollMessage(NetMessageType* outType, void* outPayload, size_t payloadCapacity, size_t* outPayloadSize) {
    ENetEvent event;

    if (gClientHost == NULL) {
        return false;
    }

    if (outType != NULL) {
        *outType = NET_MSG_NONE;
    }
    if (outPayloadSize != NULL) {
        *outPayloadSize = 0;
    }

    while (enet_host_service(gClientHost, &event, 0) > 0) {
        if (NetHandleEvent(&event, outType, outPayload, payloadCapacity, outPayloadSize)) {
            return true;
        }
    }

    return false;
}

void NetPump(void) {
    NetPollMessage(NULL, NULL, 0, NULL);
}
