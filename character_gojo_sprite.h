#ifndef CHARACTER_GOJO_SPRITE_H
#define CHARACTER_GOJO_SPRITE_H

#include "fighter.h"
#include "raylib.h"
#include <stdbool.h>

void LoadGojoSpritePack(const char* folderPath);
void UnloadGojoSpritePack(void);
bool GojoSpritePackReady(void);
bool DrawGojoSprite(const Fighter* fighter, bool isP1, float introProgress,
                    bool domainCast, bool domainCounter);

#ifdef CHARACTER_GOJO_SPRITE_IMPLEMENTATION

#include <math.h>
#include <stdio.h>

typedef struct {
    Texture2D frames[71];
    bool loaded[71];
    bool ready;
} GojoSpritePack;

static GojoSpritePack gGojoSpritePack = {0};

static int GojoResolveFrame(int frameNumber) {
    if (frameNumber < 1) frameNumber = 1;
    if (frameNumber > 70) frameNumber = 70;
    if (gGojoSpritePack.loaded[frameNumber]) return frameNumber;
    for (int i = frameNumber - 1; i >= 1; --i) {
        if (gGojoSpritePack.loaded[i]) return i;
    }
    for (int i = frameNumber + 1; i <= 70; ++i) {
        if (gGojoSpritePack.loaded[i]) return i;
    }
    return 0;
}

static int GojoSequenceFrame(int startFrame, int endFrame, float frameTime, bool loop) {
    int frameCount = (endFrame - startFrame) + 1;
    if (frameCount <= 1) return GojoResolveFrame(startFrame);
    int index = (int)floor(GetTime() / frameTime);
    if (loop) index %= frameCount;
    else if (index >= frameCount) index = frameCount - 1;
    return GojoResolveFrame(startFrame + index);
}

static int GojoProgressFrame(int startFrame, int endFrame, float progress) {
    int frameCount = (endFrame - startFrame) + 1;
    if (frameCount <= 1) return GojoResolveFrame(startFrame);
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    int index = (int)floor(progress * (float)frameCount);
    if (index >= frameCount) index = frameCount - 1;
    return GojoResolveFrame(startFrame + index);
}

void LoadGojoSpritePack(const char* folderPath) {
    char path[512];
    gGojoSpritePack.ready = false;

    for (int i = 1; i <= 70; ++i) {
        snprintf(path, sizeof(path), "%s/gojo_%02d - selection.png", folderPath, i);
        if (FileExists(path)) {
            gGojoSpritePack.frames[i] = LoadTexture(path);
            SetTextureFilter(gGojoSpritePack.frames[i], TEXTURE_FILTER_POINT);
            gGojoSpritePack.loaded[i] = true;
            gGojoSpritePack.ready = true;
        }
    }
}

void UnloadGojoSpritePack(void) {
    for (int i = 1; i <= 70; ++i) {
        if (gGojoSpritePack.loaded[i]) {
            UnloadTexture(gGojoSpritePack.frames[i]);
            gGojoSpritePack.loaded[i] = false;
        }
    }
    gGojoSpritePack.ready = false;
}

bool GojoSpritePackReady(void) {
    return gGojoSpritePack.ready;
}

static int GojoChooseFrame(const Fighter* fighter, float introProgress, bool domainCast, bool domainCounter) {
    if (introProgress >= 0.0f && introProgress < 1.0f) {
        return GojoProgressFrame(23, 31, introProgress);
    }

    if (domainCounter) {
        float progress = 1.0f;
        if (fighter->visualEventDuration > 0.0f) {
            progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
        }
        return GojoProgressFrame(65, 70, progress);
    }

    if (domainCast) {
        float progress = 1.0f;
        if (fighter->visualEventDuration > 0.0f) {
            progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
        }
        return GojoProgressFrame(58, 64, progress);
    }

    switch (fighter->visualEvent) {
        case VISUAL_EVENT_GOJO_PURPLE: {
            float progress = 1.0f;
            if (fighter->visualEventDuration > 0.0f) {
                progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
            }
            return GojoProgressFrame(46, 57, progress);
        }

        case VISUAL_EVENT_GOJO_RED:
        case VISUAL_EVENT_GOJO_BLUE: {
            float progress = 1.0f;
            if (fighter->visualEventDuration > 0.0f) {
                progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
            }
            return GojoProgressFrame(32, 34, progress);
        }

        case VISUAL_EVENT_GOJO_INFINITY:
            if (fighter->infinityActive) {
                float progress = 1.0f;
                if (fighter->visualEventDuration > 0.0f) {
                    progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
                }
                if (progress < 0.85f) return GojoProgressFrame(43, 45, progress / 0.85f);
                return GojoResolveFrame(45);
            }
            break;

        default:
            break;
    }

    if (fighter->isAttacking) {
        float progress = 1.0f - ((float)fighter->attackFrames / 14.0f);
        return GojoProgressFrame(50, 54, progress);
    }

    if (!fighter->onGround) {
        if (fighter->velY < -3.0f) return GojoResolveFrame(19);
        if (fighter->velY < 1.5f) return GojoResolveFrame(20);
        if (fighter->velY < 5.0f) return GojoResolveFrame(21);
        return GojoResolveFrame(22);
    }

    if (fighter->landingRecoverTimer > 0.0f) {
        return GojoResolveFrame(22);
    }

    if (fighter->isCrouching) {
        return GojoSequenceFrame(12, 15, 0.08f, false);
    }

    if (fighter->crouchRecoverTimer > 0.0f) {
        float progress = 1.0f - (fighter->crouchRecoverTimer / 0.18f);
        return GojoProgressFrame(16, 18, progress);
    }

    if (fabsf(fighter->hitbox.x - fighter->prevX) > 0.75f) {
        return GojoSequenceFrame(6, 11, 0.08f, true);
    }

    return GojoSequenceFrame(1, 5, 0.18f, true);
}

bool DrawGojoSprite(const Fighter* fighter, bool isP1, float introProgress,
                    bool domainCast, bool domainCounter) {
    int frame = 0;
    const Texture2D* texture;
    Rectangle src;
    Rectangle dst;
    float baseHeight;
    float aspect;
    float width;

    if (!gGojoSpritePack.ready || fighter->charData.id != CHAR_GOJO) return false;
    (void)isP1;

    frame = GojoChooseFrame(fighter, introProgress, domainCast, domainCounter);
    if (frame == 0 || !gGojoSpritePack.loaded[frame]) return false;

    texture = &gGojoSpritePack.frames[frame];
    src = (Rectangle){0.0f, 0.0f, (float)texture->width, (float)texture->height};
    if (fighter->facingDir < 0) {
        src.width = -src.width;
    }

    baseHeight = fighter->hitbox.height * 1.02f;
    aspect = (float)texture->width / (float)texture->height;
    width = baseHeight * aspect;
    dst = (Rectangle){
        fighter->hitbox.x + fighter->hitbox.width * 0.5f - width * 0.5f,
        fighter->hitbox.y + fighter->hitbox.height - baseHeight,
        width,
        baseHeight
    };
    DrawTexturePro(*texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);

    if (fighter->visualEvent == VISUAL_EVENT_GOJO_BLUE) {
        DrawCircle((int)(fighter->hitbox.x + fighter->hitbox.width * 0.5f), (int)(fighter->hitbox.y + 18.0f),
                   18.0f + 4.0f * sinf((float)GetTime() * 8.0f), ColorAlpha((Color){90, 170, 255, 255}, 0.18f));
    } else if (fighter->visualEvent == VISUAL_EVENT_GOJO_RED) {
        DrawCircle((int)(fighter->hitbox.x + fighter->hitbox.width * 0.5f), (int)(fighter->hitbox.y + 18.0f),
                   18.0f + 4.0f * sinf((float)GetTime() * 8.0f), ColorAlpha((Color){255, 95, 95, 255}, 0.18f));
    }

    return true;
}

#endif

#endif
