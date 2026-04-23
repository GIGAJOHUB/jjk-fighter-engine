#ifndef CHARACTER_YUJI_SPRITE_H
#define CHARACTER_YUJI_SPRITE_H

#include "fighter.h"
#include "raylib.h"
#include <stdbool.h>

void LoadYujiSpritePack(const char* folderPath);
void UnloadYujiSpritePack(void);
bool YujiSpritePackReady(void);
bool DrawYujiSprite(const Fighter* fighter, bool isP1, float introProgress,
                    bool domainCast, bool domainCounter);

#ifdef CHARACTER_YUJI_SPRITE_IMPLEMENTATION

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YUJI_MAX_ANIM_FRAMES 128
typedef struct { int group; int item; } YujiSpriteRef;

typedef struct {
    YujiSpriteRef frames[YUJI_MAX_ANIM_FRAMES];
    int           frameCount;
} YujiAnim;

enum {
    YUJI_ANIM_IDLE = 0,
    YUJI_ANIM_WALK,
    YUJI_ANIM_JUMP,
    YUJI_ANIM_CROUCH,
    YUJI_ANIM_ATTACK_LIGHT,
    YUJI_ANIM_ATTACK_MED,
    YUJI_ANIM_ATTACK_HEAVY,
    YUJI_ANIM_BLOCK,
    YUJI_ANIM_HIT,
    YUJI_ANIM_KNOCKDOWN,
    YUJI_ANIM_DODGE,
    YUJI_ANIM_SPECIAL1,   /* BF Flow */
    YUJI_ANIM_SPECIAL2,   /* Divergent Fist */
    YUJI_ANIM_DOMAIN,
    YUJI_ANIM_INTRO,
    YUJI_ANIM_COUNT
};

#define YUJI_TEX_CAPACITY 3000
typedef struct {
    int group, item, axisx, axisy;
    Texture2D tex;
    bool valid;
} YujiTexEntry;

typedef struct {
    YujiTexEntry entries[YUJI_TEX_CAPACITY];
    int count;
    bool ready;
    char basePath[256];
    YujiAnim anims[YUJI_ANIM_COUNT];
} YujiSpritePack;

static YujiSpritePack gYuji = {0};

static YujiTexEntry* YujiGetTex(int group, int item) {
    for (int i = 0; i < gYuji.count; i++) {
        if (gYuji.entries[i].group == group && gYuji.entries[i].item == item) {
            if (!gYuji.entries[i].valid) {
                char path[512];
                snprintf(path, sizeof(path), "%s/g%04d_i%04d.png", gYuji.basePath, group, item);
                if (FileExists(path)) {
                    gYuji.entries[i].tex = LoadTexture(path);
                    SetTextureFilter(gYuji.entries[i].tex, TEXTURE_FILTER_POINT);
                    gYuji.entries[i].valid = true;
                }
            }
            return &gYuji.entries[i];
        }
    }
    return NULL;
}

static void YujiBuildAnim(int animId, int group, int maxItems) {
    YujiAnim* a = &gYuji.anims[animId];
    a->frameCount = 0;
    for (int i = 0; i < gYuji.count && a->frameCount < YUJI_MAX_ANIM_FRAMES; i++) {
        if (gYuji.entries[i].group == group) {
            a->frames[a->frameCount].group = group;
            a->frames[a->frameCount].item = gYuji.entries[i].item;
            a->frameCount++;
        }
    }
}

void LoadYujiSpritePack(const char* folderPath) {
    memset(&gYuji, 0, sizeof(gYuji));
    strncpy(gYuji.basePath, folderPath, sizeof(gYuji.basePath) - 1);
    
    char offsetPath[512];
    snprintf(offsetPath, sizeof(offsetPath), "%s/offsets.txt", folderPath);
    FILE* f = fopen(offsetPath, "r");
    if (f) {
        int g, i, x, y;
        while (fscanf(f, "%d %d %d %d", &g, &i, &x, &y) == 4) {
            if (gYuji.count < YUJI_TEX_CAPACITY) {
                gYuji.entries[gYuji.count].group = g;
                gYuji.entries[gYuji.count].item = i;
                gYuji.entries[gYuji.count].axisx = x;
                gYuji.entries[gYuji.count].axisy = y;
                gYuji.entries[gYuji.count].valid = false;
                gYuji.count++;
            }
        }
        fclose(f);
    }

    YujiBuildAnim(YUJI_ANIM_IDLE,         0,   10);
    YujiBuildAnim(YUJI_ANIM_WALK,         20,  10);
    YujiBuildAnim(YUJI_ANIM_JUMP,         40,  10);
    YujiBuildAnim(YUJI_ANIM_CROUCH,       10,  10);
    YujiBuildAnim(YUJI_ANIM_ATTACK_LIGHT, 200, 10);
    YujiBuildAnim(YUJI_ANIM_ATTACK_MED,   210, 10);
    YujiBuildAnim(YUJI_ANIM_ATTACK_HEAVY, 220, 10);
    YujiBuildAnim(YUJI_ANIM_BLOCK,        120, 10);
    YujiBuildAnim(YUJI_ANIM_HIT,          5000, 10);
    YujiBuildAnim(YUJI_ANIM_KNOCKDOWN,    5110, 20);
    YujiBuildAnim(YUJI_ANIM_DODGE,        100, 10);
    YujiBuildAnim(YUJI_ANIM_SPECIAL1,     1000, 20);
    YujiBuildAnim(YUJI_ANIM_SPECIAL2,     1010, 20);
    YujiBuildAnim(YUJI_ANIM_DOMAIN,       1090, 50);
    YujiBuildAnim(YUJI_ANIM_INTRO,        6100, 20);

    for (int a = YUJI_ANIM_IDLE; a <= YUJI_ANIM_WALK; a++) {
        for (int i = 0; i < gYuji.anims[a].frameCount; i++) {
            YujiGetTex(gYuji.anims[a].frames[i].group, gYuji.anims[a].frames[i].item);
        }
    }
    
    gYuji.ready = (gYuji.anims[YUJI_ANIM_IDLE].frameCount > 0);
}

void UnloadYujiSpritePack(void) {
    for (int i = 0; i < gYuji.count; i++) {
        if (gYuji.entries[i].valid) UnloadTexture(gYuji.entries[i].tex);
    }
    memset(&gYuji, 0, sizeof(gYuji));
}

bool YujiSpritePackReady(void) { return gYuji.ready; }

static YujiSpriteRef YujiAnimFrame(int animId, float frameTime, bool loop) {
    YujiAnim* a = &gYuji.anims[animId];
    if (a->frameCount <= 0) return (YujiSpriteRef){0, 0};
    int index = (int)floor(GetTime() / frameTime);
    if (loop) index %= a->frameCount;
    else if (index >= a->frameCount) index = a->frameCount - 1;
    return a->frames[index];
}

static YujiSpriteRef YujiAnimProgress(int animId, float progress) {
    YujiAnim* a = &gYuji.anims[animId];
    if (a->frameCount <= 0) return (YujiSpriteRef){0, 0};
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    int index = (int)(progress * a->frameCount);
    if (index >= a->frameCount) index = a->frameCount - 1;
    return a->frames[index];
}

static YujiSpriteRef YujiChooseFrame(const Fighter* fighter, float introProgress, bool domainCast, bool domainCounter) {
    if (introProgress >= 0.0f && introProgress < 1.0f) {
        if (gYuji.anims[YUJI_ANIM_INTRO].frameCount > 0)
            return YujiAnimProgress(YUJI_ANIM_INTRO, introProgress);
        return YujiAnimProgress(YUJI_ANIM_IDLE, introProgress);
    }
    if (domainCast || domainCounter) {
        float progress = 1.0f;
        if (fighter->visualEventDuration > 0.0f)
            progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
        if (gYuji.anims[YUJI_ANIM_DOMAIN].frameCount > 0)
            return YujiAnimProgress(YUJI_ANIM_DOMAIN, progress);
    }
    if (fighter->isKnockedDown && gYuji.anims[YUJI_ANIM_KNOCKDOWN].frameCount > 0)
        return YujiAnimFrame(YUJI_ANIM_KNOCKDOWN, 0.12f, false);
    if (fighter->hitStunFrames > 0 && gYuji.anims[YUJI_ANIM_HIT].frameCount > 0)
        return YujiAnimFrame(YUJI_ANIM_HIT, 0.08f, false);
    if (fighter->isDodging && gYuji.anims[YUJI_ANIM_DODGE].frameCount > 0)
        return YujiAnimFrame(YUJI_ANIM_DODGE, 0.06f, false);
    if (fighter->isAttacking) {
        float progress = 1.0f - ((float)fighter->attackFrames / 14.0f);
        if (fighter->blackFlashActive && gYuji.anims[YUJI_ANIM_ATTACK_HEAVY].frameCount > 0)
            return YujiAnimProgress(YUJI_ANIM_ATTACK_HEAVY, progress);
        if (!fighter->onGround && gYuji.anims[YUJI_ANIM_ATTACK_MED].frameCount > 0)
            return YujiAnimProgress(YUJI_ANIM_ATTACK_MED, progress);
        if (gYuji.anims[YUJI_ANIM_ATTACK_LIGHT].frameCount > 0)
            return YujiAnimProgress(YUJI_ANIM_ATTACK_LIGHT, progress);
    }
    if (fighter->isBlocking && gYuji.anims[YUJI_ANIM_BLOCK].frameCount > 0)
        return YujiAnimFrame(YUJI_ANIM_BLOCK, 0.1f, false);
    if (!fighter->onGround && gYuji.anims[YUJI_ANIM_JUMP].frameCount > 0) {
        float jumpProg = 0.5f;
        if (fighter->velY < -3.0f) jumpProg = 0.0f;
        else if (fighter->velY < 0.0f) jumpProg = 0.3f;
        else if (fighter->velY < 3.0f) jumpProg = 0.6f;
        else jumpProg = 0.9f;
        return YujiAnimProgress(YUJI_ANIM_JUMP, jumpProg);
    }
    if (fighter->isCrouching && gYuji.anims[YUJI_ANIM_CROUCH].frameCount > 0)
        return YujiAnimFrame(YUJI_ANIM_CROUCH, 0.1f, false);
    if (fabsf(fighter->hitbox.x - fighter->prevX) > 0.5f && gYuji.anims[YUJI_ANIM_WALK].frameCount > 0)
        return YujiAnimFrame(YUJI_ANIM_WALK, 0.1f, true);
    return YujiAnimFrame(YUJI_ANIM_IDLE, 0.16f, true);
}

bool DrawYujiSprite(const Fighter* fighter, bool isP1, float introProgress,
                    bool domainCast, bool domainCounter) {
    if (!gYuji.ready || fighter->charData.id != CHAR_YUJI) return false;
    YujiSpriteRef ref = YujiChooseFrame(fighter, introProgress, domainCast, domainCounter);
    YujiTexEntry* entry = YujiGetTex(ref.group, ref.item);
    if (!entry || entry->tex.id == 0) return false;

    float scale = 2.0f;
    float drawW = (float)entry->tex.width * scale;
    float drawH = (float)entry->tex.height * scale;
    float cx = fighter->hitbox.x + fighter->hitbox.width * 0.5f;
    float by = fighter->hitbox.y + fighter->hitbox.height;

    Rectangle source = { 0, 0, (float)entry->tex.width, (float)entry->tex.height };
    float destX;
    if (fighter->facingDir < 0) {
        source.width = -source.width;
        destX = cx - (entry->tex.width - entry->axisx) * scale;
    } else {
        destX = cx - entry->axisx * scale;
    }
    float destY = by - entry->axisy * scale;

    Rectangle dest = { destX, destY, drawW, drawH };
    DrawTexturePro(entry->tex, source, dest, (Vector2){0, 0}, 0.0f, WHITE);
    return true;
}

#endif /* CHARACTER_YUJI_SPRITE_IMPLEMENTATION */
#endif /* CHARACTER_YUJI_SPRITE_H */
