
#include <stdio.h>
#include <stdbool.h>

#define STATE_CHAR_SELECT 1
#define STATE_BATTLE 2
#define STATE_DOMAIN 3
#define STATE_DOMAIN_CLASH 4
#define STATE_GAME_OVER 5
#define STATE_MULTIPLAYER 6
#define STATE_MAIN_MENU 0

int main() {
    int state = STATE_MULTIPLAYER;
    bool authRequested = false;
    bool connectedToLobby = true;
    bool gNetConnected = false;

    if ((state == STATE_CHAR_SELECT || state == STATE_BATTLE ||
         state == STATE_DOMAIN || state == STATE_DOMAIN_CLASH || state == STATE_GAME_OVER ||
         (state == STATE_MULTIPLAYER && authRequested)) &&
        connectedToLobby && !gNetConnected) {
        printf("TRIGGERED!\n");
    } else {
        printf("NOT TRIGGERED!\n");
    }
    return 0;
}

