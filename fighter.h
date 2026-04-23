#ifndef FIGHTER_H
#define FIGHTER_H

#include "characters.h"
#include "raylib.h"
#include <stdbool.h>

typedef enum {
    ULT_NONE = 0,
    ULT_HOLLOW_PURPLE,
    ULT_FUGA,
    ULT_PURE_LOVE_BEAM,
    ULT_MAHORAGA,
    ULT_OVERTIME_SLASH,
    ULT_MAXIMUM_RESONANCE,
    ULT_ULTIMATE_TACKLE,
    ULT_BLACK_FLASH,
    ULT_HEAVENLY_ASSAULT
} UltimateType;

typedef enum {
    PROJ_NONE = 0,
    PROJ_CE_BLAST,
    PROJ_GOJO_RED,
    PROJ_GOJO_BLUE,
    PROJ_SUKUNA_DISMANTLE,
    PROJ_FUGA_ARROW,
    PROJ_MEGUMI_NUE,
    PROJ_MEGUMI_DOG,
    PROJ_NOBARA_NAIL,
    PROJ_NOBARA_HAIRPIN,
    PROJ_MAHORAGA
} ProjectileType;

typedef enum {
    VISUAL_EVENT_NONE = 0,
    VISUAL_EVENT_GOJO_BLUE,
    VISUAL_EVENT_GOJO_RED,
    VISUAL_EVENT_GOJO_INFINITY,
    VISUAL_EVENT_GOJO_PURPLE,
    VISUAL_EVENT_GOJO_DOMAIN_CAST,
    VISUAL_EVENT_GOJO_DOMAIN_COUNTER
} FighterVisualEvent;

#define MAX_PROJECTILES 32

typedef struct {
    bool            active;
    ProjectileType  type;
    Rectangle       hitbox;
    Vector2         velocity;
    float           damage;
    float           lifetime;
    float           pushStrength;
    float           explosionRadius;
    float           explosionDamage;
    bool            pullsTarget;
    bool            dodgeable;
    bool            ownerIsP1;
    CharacterID     ownerCharacter;
    Color           color;
} Projectile;

typedef struct Fighter {
    CharacterData   charData;
    Rectangle       hitbox;
    Rectangle       hurtbox;
    Rectangle       pushbox;
    float           hp;
    float           maxHP;
    float           ghostHP;
    float           cursedEnergy;
    float           maxCE;
    float           specialMeter;
    float           maxSpecialMeter;
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
    int             hitStunFrames;
    float           knockbackVelX;
    bool            isBlocking;
    bool            isKnockedDown;
    int             knockdownFrames;
    int             wakeupInvincFrames;

    bool            isDodging;
    int             dodgeFrames;
    int             dodgeInvulFrames;
    float           dodgeVelX;
    int             dodgeCooldown;

    bool            blackFlashActive;
    int             blackFlashFrames;
    int             specialAnimFrames;
    int             beamTicksApplied;
    float           beamTickTimer;
    float           ultStartupTimer;
    bool            infinityActive;
    float           infinityDrainTimer;
    bool            clapBuff;
    bool            overtimeBuff;
    bool            bindingVowUsed;
    bool            mahoragaAdapted;
    bool            dollMarked;
    float           dollTimer;
    float           resonanceCooldown;
    float           rctCooldown;
    float           ceAttackCooldown;
    float           abilityECooldown;
    float           abilityRCooldown;
    float           abilityFCooldown;
    float           rctGlowTimer;
    float           lastDamageTaken;
    float           ratioBonusDamage;
    float           wallBounceVelocity;
    float           passiveTimer;
    float           boogieChargeTimer;
    int             boogieCharges;
    int             roundWins;
    int             comboCounter;
    float           comboDisplayTimer;
    float           prevX;
    float           crouchRecoverTimer;
    float           landingRecoverTimer;
    FighterVisualEvent visualEvent;
    float           visualEventTimer;
    float           visualEventDuration;

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
    bool            mahoragaActive;
    Rectangle       mahoragaHitbox;
    float           mahoragaHP;
    float           mahoragaMaxHP;
    float           mahoragaAttackTimer;
    float           mahoragaDecisionTimer;
    float           mahoragaWheelAngle;
    int             mahoragaSpinFrames;
    unsigned int    mahoragaAdaptMask;

    Color           bodyColor;
    const char*     name;

    /* SF2-style guard meter */
    float           guardMeter;
    float           guardMax;
    int             guardBreakStun;

    /* Dizzy/stun system */
    float           dizzyMeter;
    float           dizzyMax;
    bool            isDizzy;
    int             dizzyFrames;
} Fighter;

#endif // FIGHTER_H
