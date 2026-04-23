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
void TriggerSukunaSfx(int visualEvent);
void TriggerSukunaAttackSfx(void);
void TriggerSukunaDomainSfx(void);
void LoadSukunaSfx(const char* soundDir);

#ifdef CHARACTER_SUKUNA_SPRITE_IMPLEMENTATION

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Animation group definitions (MUGEN conventions) ---- */
#define SUKU_MAX_ANIM_FRAMES 128
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
    SUKU_ANIM_ULT,        /* Fuga */
    SUKU_ANIM_COUNT
};

/* ---- Texture cache with offsets ---- */
#define SUKU_TEX_CAPACITY 3000
typedef struct {
    int group;
    int item;
    int axisx;
    int axisy;
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

#ifdef CHARACTER_SUKUNA_SPRITE_IMPLEMENTATION
SukunaSpritePack gSukuna = {0};
#else
extern SukunaSpritePack gSukuna;
#endif

/* ---- Helpers ---- */
static SukunaTexEntry* SukunaGetTex(int group, int item) {
    /* Check cache first */
    for (int i = 0; i < gSukuna.count; i++) {
        if (gSukuna.entries[i].group == group && gSukuna.entries[i].item == item) {
            if (!gSukuna.entries[i].valid) {
                char path[512];
                snprintf(path, sizeof(path), "%s/g%04d_i%04d.png", gSukuna.basePath, group, item);
                if (FileExists(path)) {
                    gSukuna.entries[i].tex = LoadTexture(path);
                    SetTextureFilter(gSukuna.entries[i].tex, TEXTURE_FILTER_POINT);
                    gSukuna.entries[i].valid = true;
                }
            }
            return &gSukuna.entries[i];
        }
    }
    return NULL;
}

static void SukunaBuildAnim(int animId, int group, int maxItems) {
    SukunaAnim* a = &gSukuna.anims[animId];
    a->frameCount = 0;
    /* We just look for entries already in our offset table */
    for (int i = 0; i < gSukuna.count && a->frameCount < SUKU_MAX_ANIM_FRAMES; i++) {
        if (gSukuna.entries[i].group == group) {
            a->frames[a->frameCount].group = group;
            a->frames[a->frameCount].item = gSukuna.entries[i].item;
            a->frameCount++;
        }
    }
}

/* ---- Public API ---- */
void LoadSukunaSpritePack(const char* folderPath) {
    memset(&gSukuna, 0, sizeof(gSukuna));
    strncpy(gSukuna.basePath, folderPath, sizeof(gSukuna.basePath) - 1);
    
    /* Parse offsets.txt first to know what we have */
    char offsetPath[512];
    snprintf(offsetPath, sizeof(offsetPath), "%s/offsets.txt", folderPath);
    FILE* f = fopen(offsetPath, "r");
    if (f) {
        int g, i, x, y;
        while (fscanf(f, "%d %d %d %d", &g, &i, &x, &y) == 4) {
            if (gSukuna.count < SUKU_TEX_CAPACITY) {
                gSukuna.entries[gSukuna.count].group = g;
                gSukuna.entries[gSukuna.count].item = i;
                gSukuna.entries[gSukuna.count].axisx = x;
                gSukuna.entries[gSukuna.count].axisy = y;
                gSukuna.entries[gSukuna.count].valid = false; /* Load on demand */
                gSukuna.count++;
            }
        }
        fclose(f);
    }

    /* Build animation tables using MUGEN group IDs (shifted by 10000 if needed, but our extractor uses raw IDs) */
    /* Based on MUGEN Sukuna Ryomen S1: 
       Idle: 0, Walk: 20, Jump: 40, Crouch: 10, Block: 120, Hit: 5000, etc.
    */
    /* Verified group IDs from MegunaV5 SFF extraction */
    SukunaBuildAnim(SUKU_ANIM_IDLE,         10000, 20);  /* idle breathing loop */
    SukunaBuildAnim(SUKU_ANIM_WALK,            20, 20);  /* walk cycle */
    SukunaBuildAnim(SUKU_ANIM_JUMP,            40, 10);  /* jump arc */
    SukunaBuildAnim(SUKU_ANIM_CROUCH,          10, 10);  /* crouch */
    SukunaBuildAnim(SUKU_ANIM_ATTACK_LIGHT,   200, 30);  /* light jabs */
    SukunaBuildAnim(SUKU_ANIM_ATTACK_MED,     201, 20);  /* medium strikes */
    SukunaBuildAnim(SUKU_ANIM_ATTACK_HEAVY,   210, 30);  /* heavy combo */
    SukunaBuildAnim(SUKU_ANIM_BLOCK,         1000, 10);  /* guard stance */
    SukunaBuildAnim(SUKU_ANIM_HIT,           5000, 10);  /* hit reaction */
    SukunaBuildAnim(SUKU_ANIM_KNOCKDOWN,      210, 50);  /* tumble/fall */
    SukunaBuildAnim(SUKU_ANIM_DODGE,        10040, 10);  /* step dodge */
    SukunaBuildAnim(SUKU_ANIM_SPECIAL1,     11000, 20);  /* Dismantle slash */
    SukunaBuildAnim(SUKU_ANIM_SPECIAL2,     15000, 20);  /* Cleave sweep */
    SukunaBuildAnim(SUKU_ANIM_DOMAIN,       20300, 80);  /* Malevolent Shrine */
    SukunaBuildAnim(SUKU_ANIM_INTRO,         9898, 20);  /* intro pose */
    SukunaBuildAnim(SUKU_ANIM_ULT,          18400, 30);  /* Fuga Musou */

    /* Preload core animations */
    for (int a = SUKU_ANIM_IDLE; a <= SUKU_ANIM_WALK; a++) {
        for (int i = 0; i < gSukuna.anims[a].frameCount; i++) {
            SukunaGetTex(gSukuna.anims[a].frames[i].group, gSukuna.anims[a].frames[i].item);
        }
    }
    
    gSukuna.ready = (gSukuna.anims[SUKU_ANIM_IDLE].frameCount > 0);
    if (gSukuna.ready) {
        printf("[Sukuna] Sprite pack loaded from %s. %d total sprites found.\n", folderPath, gSukuna.count);
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

static SpriteRef SukunaAnimFrame(int animId, float frameTime, bool loop) {
    SukunaAnim* a = &gSukuna.anims[animId];
    if (a->frameCount <= 0) return (SpriteRef){0, 0};
    int index = (int)floor(GetTime() / frameTime);
    if (loop) index %= a->frameCount;
    else if (index >= a->frameCount) index = a->frameCount - 1;
    return a->frames[index];
}

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
    if (introProgress >= 0.0f && introProgress < 1.0f) {
        if (gSukuna.anims[SUKU_ANIM_INTRO].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_INTRO, introProgress);
        return SukunaAnimProgress(SUKU_ANIM_IDLE, introProgress);
    }
    if (domainCast || domainCounter) {
        float progress = 1.0f;
        if (fighter->visualEventDuration > 0.0f)
            progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
        if (gSukuna.anims[SUKU_ANIM_DOMAIN].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_DOMAIN, progress);
    }
    
    /* Special Abilities Visual Events */
    if (fighter->visualEvent == VISUAL_EVENT_SUKUNA_FUGA) {
        float progress = 1.0f;
        if (fighter->visualEventDuration > 0.0f)
            progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
        if (gSukuna.anims[SUKU_ANIM_ULT].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_ULT, progress);
    }
    if (fighter->visualEvent == VISUAL_EVENT_SUKUNA_DISMANTLE) {
        float progress = 1.0f;
        if (fighter->visualEventDuration > 0.0f)
            progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
        if (gSukuna.anims[SUKU_ANIM_SPECIAL1].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_SPECIAL1, progress);
    }
    if (fighter->visualEvent == VISUAL_EVENT_SUKUNA_CLEAVE) {
        float progress = 1.0f;
        if (fighter->visualEventDuration > 0.0f)
            progress = 1.0f - (fighter->visualEventTimer / fighter->visualEventDuration);
        if (gSukuna.anims[SUKU_ANIM_SPECIAL2].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_SPECIAL2, progress);
    }

    if (fighter->isKnockedDown && gSukuna.anims[SUKU_ANIM_KNOCKDOWN].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_KNOCKDOWN, 0.12f, false);
    if (fighter->hitStunFrames > 0 && gSukuna.anims[SUKU_ANIM_HIT].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_HIT, 0.08f, false);
    if (fighter->isDodging && gSukuna.anims[SUKU_ANIM_DODGE].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_DODGE, 0.06f, false);
    
    if (fighter->isAttacking) {
        float progress = 1.0f - ((float)fighter->attackFrames / 14.0f);
        if (fighter->blackFlashActive && gSukuna.anims[SUKU_ANIM_ATTACK_HEAVY].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_ATTACK_HEAVY, progress);
        if (!fighter->onGround && gSukuna.anims[SUKU_ANIM_ATTACK_MED].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_ATTACK_MED, progress);
        if (gSukuna.anims[SUKU_ANIM_ATTACK_LIGHT].frameCount > 0)
            return SukunaAnimProgress(SUKU_ANIM_ATTACK_LIGHT, progress);
    }
    
    if (fighter->isBlocking && gSukuna.anims[SUKU_ANIM_BLOCK].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_BLOCK, 0.1f, false);
    
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
    
    if (fighter->isCrouching && gSukuna.anims[SUKU_ANIM_CROUCH].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_CROUCH, 0.1f, false);
    
    if (fabsf(fighter->hitbox.x - fighter->prevX) > 0.5f && gSukuna.anims[SUKU_ANIM_WALK].frameCount > 0)
        return SukunaAnimFrame(SUKU_ANIM_WALK, 0.1f, true);
    
    return SukunaAnimFrame(SUKU_ANIM_IDLE, 0.16f, true);
}

/* --- Sukuna SFX ---
 * Group 980 sounds from MUGEN CNS:
 *   s980_0 = attack grunt, s980_1 = slash whoosh, s980_2 = heavy hit,
 *   s980_3 = cleave, s980_4 = domain cast, s980_5 = vocal yell,
 *   s980_7 = Dismantle, s980_10 = ultimate charge, s980_14 = Fuga blast
 */
#define SUKU_SFX_COUNT 8
typedef struct { Sound snd; bool loaded; } SukunaSfx;
static SukunaSfx gSfxAttack   = {0}; /* s980_0  */
static SukunaSfx gSfxSlash    = {0}; /* s980_1  */
static SukunaSfx gSfxCleave   = {0}; /* s980_3  */
static SukunaSfx gSfxDomain   = {0}; /* s980_4  */
static SukunaSfx gSfxYell     = {0}; /* s980_5  */
static SukunaSfx gSfxDismantle= {0}; /* s980_7  */
static SukunaSfx gSfxUltCharge= {0}; /* s980_10 */
static SukunaSfx gSfxFuga     = {0}; /* s980_14 */

void LoadSukunaSfx(const char* soundDir) {
    char p[256];
#define TRY_LOAD(sfxVar, filename) \
    snprintf(p, sizeof(p), "%s/" filename, soundDir); \
    if (FileExists(p)) { sfxVar.snd = LoadSound(p); sfxVar.loaded = true; }
    TRY_LOAD(gSfxAttack,    "s980_0.wav")
    TRY_LOAD(gSfxSlash,     "s980_1.wav")
    TRY_LOAD(gSfxCleave,    "s980_3.wav")
    TRY_LOAD(gSfxDomain,    "s980_4.wav")
    TRY_LOAD(gSfxYell,      "s980_5.wav")
    TRY_LOAD(gSfxDismantle, "s980_7.wav")
    TRY_LOAD(gSfxUltCharge, "s980_10.wav")
    TRY_LOAD(gSfxFuga,      "s980_14.wav")
#undef TRY_LOAD
}

static void PlaySukunaSfxInternal(SukunaSfx* s, float vol) {
    if (s->loaded && !IsSoundPlaying(s->snd)) {
        SetSoundVolume(s->snd, vol);
        PlaySound(s->snd);
    }
}

/* Call this from main when visual events fire */
void TriggerSukunaSfx(int visualEvent) {
    switch(visualEvent) {
        case VISUAL_EVENT_SUKUNA_DISMANTLE: PlaySukunaSfxInternal(&gSfxDismantle, 0.75f); break;
        case VISUAL_EVENT_SUKUNA_CLEAVE:    PlaySukunaSfxInternal(&gSfxCleave,    0.75f); break;
        case VISUAL_EVENT_SUKUNA_FUGA:      PlaySukunaSfxInternal(&gSfxFuga,      0.85f); break;
        default: break;
    }
}
void TriggerSukunaAttackSfx(void) { PlaySukunaSfxInternal(&gSfxAttack, 0.65f); }
void TriggerSukunaDomainSfx(void) { PlaySukunaSfxInternal(&gSfxDomain, 0.80f); }

bool DrawSukunaSprite(const Fighter* fighter, bool isP1, float introProgress,
                      bool domainCast, bool domainCounter) {
    if (!gSukuna.ready || fighter->charData.id != CHAR_SUKUNA) return false;

    SpriteRef ref = SukunaChooseFrame(fighter, introProgress, domainCast, domainCounter);
    SukunaTexEntry* entry = SukunaGetTex(ref.group, ref.item);
    /* Guard: skip if texture failed to load (prevents black squares) */
    if (!entry || !entry->valid || entry->tex.id == 0 || entry->tex.width == 0) return false;

    float scale = 1.6f;
    float drawW = (float)entry->tex.width * scale;
    float drawH = (float)entry->tex.height * scale;

    float cx = fighter->hitbox.x + fighter->hitbox.width * 0.5f;
    float by = fighter->hitbox.y + fighter->hitbox.height;

    float absW = (float)(entry->tex.width < 0 ? -entry->tex.width : entry->tex.width);
    Rectangle source = { 0, 0, absW, (float)entry->tex.height };

    float destX;
    if (fighter->facingDir < 0) {
        source.width = -absW;  /* flip horizontally */
        destX = cx - (absW - (float)entry->axisx) * scale;
    } else {
        destX = cx - (float)entry->axisx * scale;
    }
    float destY = by - (float)entry->axisy * scale;

    Rectangle dest = { destX, destY, drawW, drawH };
    DrawTexturePro(entry->tex, source, dest, (Vector2){0, 0}, 0.0f, WHITE);
    return true;
}

#endif /* CHARACTER_SUKUNA_SPRITE_IMPLEMENTATION */
#endif /* CHARACTER_SUKUNA_SPRITE_H */
