
#include "raylib.h"
#include <stdio.h>

int main(void)
{
    InitWindow(800, 450, "Test");
    SetTargetFPS(60);

    int state = 0; // 0: main, 1: multiplayer
    while (!WindowShouldClose())
    {
        if (state == 0) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                state = 1;
                printf("Changed to MULTIPLAYER on frame %d\n", GetFrameCount());
            }
        } else if (state == 1) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                state = 0;
                printf("Changed to MAIN on frame %d\n", GetFrameCount());
            }
        }
        
        BeginDrawing();
        ClearBackground(RAYWHITE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}

