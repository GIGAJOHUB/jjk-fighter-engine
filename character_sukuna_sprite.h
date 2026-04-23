#ifndef CHARACTER_SUKUNA_SPRITE_H
#define CHARACTER_SUKUNA_SPRITE_H

#include "fighter.h"
#include "raylib.h"
#include <stdbool.h>

void LoadSukunaSpritePack(const char* folderPath);
void UnloadSukunaSpritePack(void);
bool SukunaSpritePackReady(void);
bool DrawSukunaSprite(const Fighter* fighter, bool isP1, float introProgress,
                      bool domainCast, bool domainCounter);

#ifdef CHARACTER_SUKUNA_SPRITE_IMPLEMENTATION

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- Animation group definitions (MUGEN conventions) ---- */
#define SUKU_MAX_ANIM_FRAMES 64
typedef struct { int group; int item; } SpriteRef;

typedef struct {
    SpriteRef frames[SUKU_MAX_ANIM_FRAMES];
    int       frameCount;
} SukunaAnim;

/* Animations we care about */
enum {
    SUKU_ANIM_IDLE = 0,
    SUKU_ANIM_WALK,
    SUKU_ANIM_JUMP,
    SUKU_ANIM_CROUCH,
    SUKU_ANIM_ATTACK_LIGHT,
    SUKU_ANIM_ATTACK_MED,
    SUKU_ANIM_ATTACK_HEAVY,
    SUKU_ANIM_BLOCK,
    SUKU_ANIM_HIT,
    SUKU_ANIM_KNOCKDOWN,
    SUKU_ANIM_DODGE,
    SUKU_ANIM_SPECIAL1,   /* Dismantle */
    SUKU_ANIM_SPECIAL2,   /* Cleave */
    SUKU_ANIM_DOMAIN,
    SUKU_ANIM_INTRO,
    SUKU_ANIM_COUNT
};

/* ---- Texture cache ---- */
#define SUKU_TEX_CAPACITY 400
typedef struct {
    int group;
    int item;
    Texture2D tex;
    bool valid;
} SukunaTexEntry;

typedef struct {
    SukunaTexEntry entries[SUKU_TEX_CAPACITY];
    int count;
    bool ready;
    char basePath[256];
    SukunaAnim anims[SUKU_ANIM_COUNT];
} SukunaSpritePack;

static SukunaSpritePack gSukuna = {0};

/* ---- Helpers ---- */
static Texture2D SukunaGetTex(int group, int item) {
    /* Check cache first */
    for (int i = 0; i < gSukuna.count; i++) {
        if (gSukuna.entries[i].group == group && gSukuna.entries[i].item == item && gSukuna.entries[i].valid) {
            return gSukuna.entries[i].tex;
        }
    }
    /* Try loading from disk */
    if (gSukuna.count < SUKU_TEX_CAPACITY) {
        char path[512];
        snprintf(path, sizeof(path), "%s/g%04d_i%04d.png", gSukuna.basePath, group, item);
        if (FileExists(path)) {
            int idx = gSukuna.count++;
            gSukuna.entries[idx].group = group;
            gSukuna.entries[idx].item = item;
            gSukuna.entries[idx].tex = LoadTexture(path);
            SetTextureFilter(gSukuna.entries[idx].tex, TEXTURE_FILTER_POINT);
            gSukuna.entries[idx].valid = true;
            return gSukuna.entries[idx].tex;
        }
    }
    return (Texture2D){0};
}

static void SukunaBuildAnim(int animId, int group, int maxItems) {
    SukunaAnim* a = &gSukuna.anims[animId];
    a->frameCount = 0;
    for (int i = 0; i < maxItems && a->frameCount < SUKU_MAX_ANIM_FRAMES; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/g%04d_i%04d.png", gSukuna.basePath, group, i);
        if (FileExists(path)) {
            a->frames[a->frameCount].group = group;
            a->frames[a->frameCount].item = i;
            a->frameCount++;
        }
    }
}

/* ---- Public API ---- */
void LoadSukunaSpritePack(const char* folderPath) {
    memset(&gSukuna, 0, sizeof(gSukuna));
    strncpy(gSukuna.basePath, folderPath, sizeof(gSukuna.basePath) - 1);
    
    /* Build animation tables from extracted sprites */
    SukunaBuildAnim(SUKU_ANIM_IDLE,         0,   10);
    SukunaBuildAnim(SUKU_ANIM_WALK,         20,  10);
    SukunaBuildAnim(SUKU_ANIM_JUMP,         40,  10);
    SukunaBuildAnim(SUKU_ANIM_CROUCH,       50,  10);
    SukunaBuildAnim(SUKU_ANIM_ATTACK_LIGHT, 200, 10);
    SukunaBuildAnim(SUKU_ANIM_ATTACK_MED,   210, 10);
    SukunaBuildAnim(SUKU_ANIM_ATTACK_HEAVY, 220, 10);
    SukunaBuildAnim(SUKU_ANIM_BLOCK,        400, 10);
    SukunaBuildAnim(SUKU_ANIM_HIT,          5000, 10);
    SukunaBuildAnim(SUKU_ANIM_KNOCKDOWN,    5300, 20);
    SukunaBuildAnim(SUKU_ANIM_DODGE,        190, 10);
    SukunaBuildAnim(SUKU_ANIM_SPECIAL1,     1050, 20);
    SukunaBuildAnim(SUKU_ANIM_SPECIAL2,     1060, 20);
    SukunaBuildAnim(SUKU_ANIM_DOMAIN,       1090, 50);
    SukunaBuildAnim(SUKU_ANIM_INTRO,        6100, 20);

    /* Preload idle + walk to ensure sprites render immediately */
    for (int a = SUKU_ANIM_IDLE; a <= SUKU_ANIM_WALK; a++) {
        for (int i = 0; i < gSukuna.anims[a].frameCount; i++) {
            SukunaGetTex(gSukuna.anims[a].frames[i].group, gSukuna.anims[a].frames[i].item);
        }
    }
    
    gSukuna.ready = (gSukuna.anims[SUKU_ANIM_IDLE].frameCount > 0);
    if (gSukuna.ready) {
        printf("[Sukuna] Sprite pack loaded: %d idle, %d walk, %d jump, %d crouch, %d attack frames\n",
            gSukuna.anims[SUKU_ANIM_IDLE].frameCount,
            gSukuna.anims[SUKU_ANIM_WALK].frameCount,
            gSukuna.anims[SUKU_ANIM_JUMP].frameCount,
            gSukuna.anims[SUKU_ANIM_CROUCH].frameCount,
            gSukuna.anims[SUKU_ANIM_ATTACK_LIGHT].frameCount +
            gSukuna.anims[SUKU_ANIM_ATTACK_MED].frameCount +
            gSukuna.anims[SUKU_ANIM_ATTACK_HEAVY].frameCount);
    }
}

void UnloadSukunaSpritePack(void) {
    for (int i = 0; i < gSukuna.count; i++) {
        if (gSukuna.entries[i].valid) {
            UnloadTexture(gSukuna.entries[i].tex);
        }
    }
    memset(&gSukuna, 0, sizeof(gSukuna));
}

bool SukunaSpritePackReady(void) {
    return gSukuna.ready;
}

/* Get a frame from an animation by time-based index */
static SpriteRef SukunaAnimFrame(int animId, float frameTime, bool loop) {
    SukunaAnim* a = &gSukuna.anims[animId];
    if (a->frameCount <= 0) return (SpriteRef){0, 0};
    int index = (int)floor(GetTime() / frameTime);
    if (loop) index %= a->frameCount;
    else if (index >= a->frameCount) index = a->frameCount - 1;
    return a->frames[index];
}

/* Get a frame from an animation by progress (0.0-1.0) */
static SpriteRef SukunaAnimProgress(int animId, float progress) {
    SukunaAnim* a = &gSukuna.anims[animId];
    if (a->frameCount <= 0) return (SpriteRef){0, 0};
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    int index = (int)(progress * a->frameCount);
    if (index >= a->frameCount) index = a->frameCount - 1;
    return a->frames[index];
}

static SpriteRef SukunaChooseFrame(const Fighter* fighter, float introProgress, bool domainCast, bool domainCounter) {
    /* Intro */
    if (introProgress >= 0.0f && introProgress < 1.0f) {
        if (gSukuna.anims[SUKU_ANIM_INTRO].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_INTRO, introProgress);
        return SukunaAnimProgress(SUKU_ANIM_IDLE, introProgress);
    }

    /* Domain */
    if (domainCast || domainCounter) {
        float progress = 1.0f;
        if (fighter->visualEventDuration > 0.0f)
            progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
        if (gSukuna.anims[SUKU_ANIM_DOMAIN].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_DOMAIN, progress);
    }

    /* Knockdown */
    if (fighter->isKnockedDown && gSukuna.anims[SUKU_ANIM_KNOCKDOWN].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_KNOCKDOWN, 0.12f, false);

    /* Hit stun */
    if (fighter->hitStunFrames > 0 && gSukuna.anims[SUKU_ANIM_HIT].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_HIT, 0.08f, false);

    /* Dodge */
    if (fighter->isDodging && gSukuna.anims[SUKU_ANIM_DODGE].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_DODGE, 0.06f, false);

    /* Attacking */
    if (fighter->isAttacking) {
        float progress = 1.0f - ((float)fighter->attackFrames / 14.0f);
        /* Use heavy attack for special attacks, medium for aerial, light for ground */
        if (fighter->blackFlashActive && gSukuna.anims[SUKU_ANIM_ATTACK_HEAVY].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_ATTACK_HEAVY, progress);
        if (!fighter->onGround && gSukuna.anims[SUKU_ANIM_ATTACK_MED].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_ATTACK_MED, progress);
        if (gSukuna.anims[SUKU_ANIM_ATTACK_LIGHT].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_ATTACK_LIGHT, progress);
    }

    /* Blocking */
    if (fighter->isBlocking && gSukuna.anims[SUKU_ANIM_BLOCK].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_BLOCK, 0.1f, false);

    /* Airborne */
    if (!fighter->onGround) {
        if (gSukuna.anims[SUKU_ANIM_JUMP].frameCount > 0) {
            float jumpProg = 0.5f;
            if (fighter->velY < -3.0f) jumpProg = 0.0f;
            else if (fighter->velY < 0.0f) jumpProg = 0.3f;
            else if (fighter->velY < 3.0f) jumpProg = 0.6f;
            else jumpProg = 0.9f;
            return SukunaAnimProgress(SUKU_ANIM_JUMP, jumpProg);
        }
    }

    /* Crouching */
    if (fighter->isCrouching && gSukuna.anims[SUKU_ANIM_CROUCH].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_CROUCH, 0.1f, false);

    /* Walking */
    if (fabsf(fighter->hitbox.x - fighter->prevX) > 0.5f && gSukuna.anims[SUKU_ANIM_WALK].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_WALK, 0.1f, true);

    /* Idle (default) */
    return SukunaAnimFrame(SUKU_ANIM_IDLE, 0.16f, true);
}

bool DrawSukunaSprite(const Fighter* fighter, bool isP1, float introProgress,
                      bool domainCast, bool domainCounter) {
    if (!gSukuna.ready || fighter->charData.id != CHAR_SUKUNA) return false;

    SpriteRef ref = SukunaChooseFrame(fighter, introProgress, domainCast, domainCounter);
    Texture2D tex = SukunaGetTex(ref.group, ref.item);
    if (tex.id == 0) return false;

    float scale = 2.0f; /* Scale MUGEN sprites up to match game size */
    float drawW = (float)tex.width * scale;
    float drawH = (float)tex.height * scale;

    /* Anchor the sprite at the fighter's bottom-center */
    float cx = fighter->hitbox.x + fighter->hitbox.width * 0.5f;
    float by = fighter->hitbox.y + fighter->hitbox.height;

    Rectangle source = { 0, 0, (float)tex.width, (float)tex.height };
    /* Flip horizontally if facing left */
    if (fighter->facingDir < 0) source.width = -source.width;

    Rectangle dest = { cx - drawW * 0.5f, by - drawH, drawW, drawH };

    DrawTexturePro(tex, source, dest, (Vector2){0, 0}, 0.0f, WHITE);
    return true;
}

#endif /* CHARACTER_SUKUNA_SPRITE_IMPLEMENTATION */
#endif /* CHARACTER_SUKUNA_SPRITE_H */
