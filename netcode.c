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

bool NetInit(void) {
    if (enet_initialize() != 0) {
        printf("ERROR: Failed to initialize ENet.\n");
        return false;
    }
    return true;
}

void NetCleanup(void) {
    if (localNode != NULL) enet_host_destroy(localNode);
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
    printf("CONNECTING to %s:%d...\n", ipAddress, port);
    return true;
}

void NetSendInput(NetInput input) {
    if (!gNetConnected || connectedPeer == NULL) return;

    // ENET_PACKET_FLAG_UNSEQUENCED is crucial for fighting games. 
    // It acts like pure UDP (Drops the packet if lost, no lag spikes).
    ENetPacket* packet = enet_packet_create(&input, sizeof(NetInput), ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(connectedPeer, 0, packet);
}

bool NetReceiveInput(NetInput* outInput) {
    if (localNode == NULL) return false;

    ENetEvent event;
    bool receivedNewInput = false;

    // Poll the network card for incoming data
    while (enet_host_service(localNode, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                printf(">>> A NEW SORCERER HAS ENTERED THE DOMAIN <<<\n");
                connectedPeer = event.peer;
                gNetConnected = true;
                break;
                
            case ENET_EVENT_TYPE_RECEIVE:
                if (event.packet->dataLength == sizeof(NetInput)) {
                    memcpy(outInput, event.packet->data, sizeof(NetInput));
                    receivedNewInput = true;
                }
                enet_packet_destroy(event.packet);
                break;
                
            case ENET_EVENT_TYPE_DISCONNECT:
                printf(">>> OPPONENT DISCONNECTED <<<\n");
                gNetConnected = false;
                connectedPeer = NULL;
                break;
                
            case ENET_EVENT_TYPE_NONE:
                break;
        }
    }
    return receivedNewInput;
}