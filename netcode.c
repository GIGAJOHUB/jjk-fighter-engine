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
