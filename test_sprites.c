#include "raylib.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    Image img = LoadImage("sukuna sprite sheet.png");
    Color* pixels = LoadImageColors(img);
    Color bg = pixels[0];
    
    int w = 72;
    int h = 94;
    for (int row = 0; row < 10; row++) {
        for (int col = 0; col < 10; col++) {
            int nonbg = 0;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    Color p = pixels[(row * h + y) * img.width + (col * w + x)];
                    int diff = abs(p.r - bg.r) + abs(p.g - bg.g) + abs(p.b - bg.b);
                    if (diff > 10) nonbg++;
                }
            }
            if (col == 0 && row < 3) printf("Row %d Col %d: %d non-bg pixels\n", row, col, nonbg);
        }
    }
    
    UnloadImageColors(pixels);
    UnloadImage(img);
    return 0;
}
