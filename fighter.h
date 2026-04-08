#ifndef FIGHTER_H
#define FIGHTER_H

#include "characters.h"
#include "raylib.h"
#include <stdbool.h>

typedef enum {
    ULT_NONE = 0,
    ULT_HOLLOW_PURPLE,
    ULT_DISMANTLE_CLEAVE,
    ULT_BLACK_FLASH,
    ULT_HEAVENLY_ASSAULT
} UltimateType;

typedef struct Fighter {
    CharacterData   charData;
    Rectangle       hitbox;
    float           hp;
    float           maxHP;
    float           cursedEnergy;
    float           maxCE;
    float           speed;
    float           attackDamage;

    bool            isCrouching;
    bool            isStunned;
    bool            hasDomain;
    bool            domainActive;
    bool            isHeavenlyRestricted;

    int             facingDir;
    float           velY;
    bool            onGround;

    bool            isAttacking;
    int             attackFrames;
    bool            attackLanded;

    bool            isDodging;
    int             dodgeFrames;
    float           dodgeVelX;
    int             dodgeCooldown;

    bool            projectileActive;
    Rectangle       projectile;
    float           projectileSpeed;

    bool            blackFlashActive;
    int             blackFlashFrames;

    bool            ultUsed;
    bool            ultActive;
    bool            ultReady;
    bool            ultHitApplied;
    UltimateType    activeUlt;
    Rectangle       ultHitbox;
    float           ultSpeed;
    float           ultDamage;
    float           ultTimer;
    float           ultDuration;
    int             comboHits;
    float           comboTimer;
    CharacterID     copiedUltSource;

    Color           bodyColor;
    const char*     name;
} Fighter;

#endif // FIGHTER_H
