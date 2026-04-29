/*
 * character_gojo_sprite.h
 * TND-Gojo MUGEN integration via mugen_anim.h
 *
 * PUBLIC API (render.c-compatible):
 *   LoadGojoSpritePack(path)   — loads anims.json + sprite folder
 *   UnloadGojoSpritePack()     — cleanup
 *   GojoSpritePackReady()      — is data loaded?
 *   DrawGojoSprite(...)        — maps Fighter state → MUGEN action → draw
 */

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

/* ────────────────────────────────────────────────────── */
#ifdef CHARACTER_GOJO_SPRITE_IMPLEMENTATION

#include "mugen_anim.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ── MUGEN Action IDs (mined from TND-Gojo.air) ── */
enum {
    GA_IDLE          =    0,
    GA_IDLE2         =    5,
    GA_CROUCH_DOWN   =   10,
    GA_CROUCHING     =   11,
    GA_STAND_UP      =   12,
    GA_WALK_FWD      =   20,
    GA_WALK_BACK     =   21,
    GA_JUMP_START    =   40,
    GA_JUMP_UP       =   41,
    GA_JUMP_BACK     =   42,
    GA_JUMP_FWD      =   43,
    GA_JUMP_LAND     =   44,
    GA_JUMP_FALL     =   45,
    GA_RUN_FWD       =  100,
    GA_RUN_BACK      =  105,
    GA_GUARD_STAND   =  120,
    GA_GUARD_CROUCH  =  130,
    GA_GUARD_AIR     =  140,
    GA_HIT_LIGHT     =  150,
    GA_HIT_MED       =  151,
    GA_HIT_HEAVY     =  152,
    GA_LIE_DOWN      =  170,
    GA_INTRO         =  190,
    GA_ATK_STAND_L   =  200,
    GA_ATK_STAND_M   =  210,
    GA_ATK_STAND_H   =  220,
    GA_ATK_STAND_3H  =  230,
    GA_ATK_CROUCH_L  =  400,
    GA_ATK_CROUCH_M  =  410,
    GA_ATK_CROUCH_H  =  420,
    GA_ATK_AIR_L     =  440,
    GA_ATK_AIR_M     =  450,
    /* Specials */
    GA_BLUE_START    = 3000,
    GA_BLUE_PROJ     = 3100,
    GA_RED_START     = 3200,
    GA_PURPLE_START  = 3300,
    GA_PURPLE_BEAM   = 3310,
    GA_DOMAIN_START  = 3400,
    GA_DOMAIN_LOOP   = 3420,
    /* Hit reactions */
    GA_HIT_HIGH      = 5000,
    GA_HIT_MID       = 5010,
    GA_KNOCKDOWN     = 5050,
};

/* ── State ── */
static MACharacter gGojoChar = {0};
static MAPlayback  gGojoPlay[2] = {{0}, {0}};  /* P1 and P2 slots */
static bool gGojoLoaded = false;

/*
 * Scale: The MUGEN axis Y tells us how many pixels from the top of the sprite
 * to the character's feet. The fighter hitbox height IS the character's visible
 * height. So scale = hitbox_height / axis_y (with some headroom for hair etc.)
 * We compute this per-frame since different sprites have different axes.
 */

/* ── Init / Cleanup ── */

void LoadGojoSpritePack(const char* folderPath) {
    const char* jsonPath = "assets/characters/gojo/anims.json";
    const char* spritePath = folderPath;

    if (!DirectoryExists(folderPath))
        spritePath = "assets/characters/gojo/sprites";

    MA_Init(&gGojoChar, spritePath, jsonPath);
    gGojoLoaded = gGojoChar.ready;

    if (gGojoLoaded) {
        MA_ForceAction(&gGojoPlay[0], &gGojoChar, GA_IDLE);
        MA_ForceAction(&gGojoPlay[1], &gGojoChar, GA_IDLE);
        TraceLog(LOG_INFO, "GOJO: Ready — %d actions, %d offsets, sprites at %s",
                 gGojoChar.actionCount, gGojoChar.offsetCount, spritePath);
    } else {
        TraceLog(LOG_WARNING, "GOJO: FAILED to load — sprites=%s, json=%s", spritePath, jsonPath);
    }
}

void UnloadGojoSpritePack(void) {
    MA_Cleanup(&gGojoChar);
    memset(gGojoPlay, 0, sizeof(gGojoPlay));
    gGojoLoaded = false;
}

bool GojoSpritePackReady(void) { return gGojoLoaded; }

/* ── State-to-Action mapping ──
 *
 * Priority order (highest first):
 *   1. Intro
 *   2. Domain cast/counter
 *   3. Visual events (abilities)
 *   4. Hit stun / knockdown
 *   5. Attacking
 *   6. Blocking
 *   7. Airborne
 *   8. Crouching
 *   9. Walking / running
 *  10. Idle
 */
static int GojoPickAction(const Fighter* f, float introProgress,
                          bool domainCast, bool domainCounter) {

    /* 1. Intro */
    if (introProgress >= 0.0f && introProgress < 1.0f)
        return GA_INTRO;

    /* 2. Domain */
    if (domainCast)    return GA_DOMAIN_START;
    if (domainCounter) return GA_DOMAIN_LOOP;

    /* 3. Visual events (specials) */
    switch (f->visualEvent) {
        case VISUAL_EVENT_GOJO_PURPLE: return GA_PURPLE_START;
        case VISUAL_EVENT_GOJO_BLUE:   return GA_BLUE_START;
        case VISUAL_EVENT_GOJO_RED:    return GA_RED_START;
        case VISUAL_EVENT_GOJO_INFINITY: return GA_IDLE2;
        default: break;
    }

    /* 4. Hit reactions */
    if (f->hitStunFrames > 0) {
        if (!f->onGround) return GA_KNOCKDOWN;
        if (f->isCrouching) return GA_HIT_MID;
        return GA_HIT_HIGH;
    }

    /* 5. Attacking */
    if (f->isAttacking) {
        if (!f->onGround)   return GA_ATK_AIR_L;
        if (f->isCrouching) return GA_ATK_CROUCH_L;
        /* Vary attack animation by a simple heuristic */
        return GA_ATK_STAND_M;
    }

    /* 6. Blocking */
    if (f->isBlocking) {
        if (!f->onGround)   return GA_GUARD_AIR;
        if (f->isCrouching) return GA_GUARD_CROUCH;
        return GA_GUARD_STAND;
    }

    /* 7. Airborne */
    if (!f->onGround) {
        if (f->velY < -3.0f) return GA_JUMP_UP;
        if (f->velY < 0.0f)  return GA_JUMP_FWD;
        if (f->velY < 3.0f)  return GA_JUMP_FALL;
        return GA_JUMP_LAND;
    }

    /* 8. Landing recovery */
    if (f->landingRecoverTimer > 0.0f)
        return GA_JUMP_LAND;

    /* 9. Crouching */
    if (f->isCrouching) return GA_CROUCHING;
    if (f->crouchRecoverTimer > 0.0f) return GA_STAND_UP;

    /* 10. Movement */
    float dx = f->hitbox.x - f->prevX;
    if (fabsf(dx) > 0.75f) {
        bool forward = (dx * f->facingDir) > 0;
        return forward ? GA_WALK_FWD : GA_WALK_BACK;
    }

    return GA_IDLE;
}

/* ── Draw ── */
bool DrawGojoSprite(const Fighter* fighter, bool isP1,
                    float introProgress, bool domainCast, bool domainCounter) {

    if (!gGojoLoaded || fighter->charData.id != CHAR_GOJO) return false;

    int slot = isP1 ? 0 : 1;
    MAPlayback* pb = &gGojoPlay[slot];

    /* Pick and set animation */
    int wanted = GojoPickAction(fighter, introProgress, domainCast, domainCounter);
    MA_SetAction(pb, &gGojoChar, wanted);
    MA_Tick(pb, &gGojoChar);

    /* Get the current frame to look up axis */
    const MAFrame* curFrame = MA_CurrentFrame(pb, &gGojoChar);
    if (!curFrame || curFrame->group < 0) return false;

    /* Look up axis for this sprite */
    int axisX = 0, axisY = 0;
    MA_FindOffset(&gGojoChar, curFrame->group, curFrame->item, &axisX, &axisY);

    /* Scale: axisY = distance from top of sprite to feet.
     * We want the character to fill the fighter hitbox height.
     * Add 10% headroom so hair/effects don't clip.
     * Fallback to a safe default if axis is zero (palette sprites). */
    float charPixelH = (axisY > 10) ? (float)axisY : 62.0f;
    float scale = (fighter->hitbox.height * 1.1f) / charPixelH;

    /* Draw position: center-bottom of hitbox = MUGEN feet position */
    float footX = fighter->hitbox.x + fighter->hitbox.width * 0.5f;
    float footY = fighter->hitbox.y + fighter->hitbox.height;

    MA_Draw(&gGojoChar, pb, footX, footY, (int)fighter->facingDir, scale);

    /* VFX overlays */
    if (fighter->visualEvent == VISUAL_EVENT_GOJO_BLUE) {
        float pulse = 18.0f + 4.0f * sinf((float)GetTime() * 8.0f);
        DrawCircle((int)footX, (int)(fighter->hitbox.y + 18.0f),
                   pulse, ColorAlpha((Color){90, 170, 255, 255}, 0.18f));
    } else if (fighter->visualEvent == VISUAL_EVENT_GOJO_RED) {
        float pulse = 18.0f + 4.0f * sinf((float)GetTime() * 8.0f);
        DrawCircle((int)footX, (int)(fighter->hitbox.y + 18.0f),
                   pulse, ColorAlpha((Color){255, 95, 95, 255}, 0.18f));
    }

    return true;
}

#endif /* CHARACTER_GOJO_SPRITE_IMPLEMENTATION */
#endif /* CHARACTER_GOJO_SPRITE_H */
