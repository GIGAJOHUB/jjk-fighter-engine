#include "raylib.h"
#include "render.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W                 960
#define SCREEN_H                 540
#define FLOOR_Y                  400.0f
#define GRAVITY                  0.55f
#define JUMP_FORCE              -14.0f
#define PROJECTILE_SPD           9.0f

#define DOMAIN_COUNTER_WINDOW    5.0f
#define DOMAIN_CLASH_DURATION    1.8f
#define DOMAIN_SUREHIT_DAMAGE  150.0f
#define DOMAIN_TOJI_DAMAGE      75.0f

#define RCT_CE_COST             40.0f
#define RCT_HEAL_AMOUNT         30.0f
#define CE_ATTACK_COST          15.0f
#define DOMAIN_CE_COST          50.0f
#define CE_REGEN_RATE            0.025f

#define DODGE_FRAMES            18
#define DODGE_SPEED              9.0f
#define DODGE_COOLDOWN_FRAMES   40
#define YUJI_COMBO_WINDOW        2.2f

typedef enum {
    STATE_CHAR_SELECT = 0,
    STATE_BATTLE,
    STATE_DOMAIN,
    STATE_DOMAIN_CLASH,
    STATE_GAME_OVER
} GameState;

typedef struct {
    int cursor;
    CharacterID selected;
    bool confirmed;
} SelectState;

typedef struct {
    bool active;
    float timer;
    float duration;
    int winnerPlayer;
    float damage;
} DomainClashState;

static Fighter InitFighter(CharacterID id, float startX, int facingDir) {
    Fighter f;
    memset(&f, 0, sizeof(Fighter));
    f.charData             = GetCharacterData(id);
    f.maxHP                = f.charData.maxHP;
    f.hp                   = f.maxHP;
    f.maxCE                = f.charData.maxCE;
    f.cursedEnergy         = f.maxCE;
    f.speed                = f.charData.baseSpeed;
    f.attackDamage         = f.charData.baseAttackDamage;
    f.isHeavenlyRestricted = f.charData.traits.isHeavenlyRestricted;
    f.hasDomain            = f.charData.hasDomain;
    f.facingDir            = facingDir;
    f.bodyColor            = f.charData.bodyColor;
    f.name                 = f.charData.name;
    f.hitbox               = (Rectangle){ startX, FLOOR_Y - 90.0f, 52.0f, 90.0f };
    f.onGround             = true;
    f.copiedUltSource      = CHAR_COUNT;
    return f;
}

static void ClampFighter(Fighter* f) {
    if (f->hp < 0.0f) f->hp = 0.0f;
    if (f->hp > f->maxHP) f->hp = f->maxHP;
    if (f->cursedEnergy < 0.0f) f->cursedEnergy = 0.0f;
    if (f->cursedEnergy > f->maxCE) f->cursedEnergy = f->maxCE;
}

static float HorizontalGap(const Fighter* a, const Fighter* b) {
    float ax = a->hitbox.x + a->hitbox.width * 0.5f;
    float bx = b->hitbox.x + b->hitbox.width * 0.5f;
    return fabsf(ax - bx);
}

static float CalcCECost(const Fighter* f, float baseCost) {
    return baseCost * f->charData.traits.ceCostMultiplier;
}

static float CalcRCTCost(const Fighter* f) {
    return CalcCECost(f, RCT_CE_COST) * f->charData.traits.rctCostMultiplier;
}

static float GetDomainDamage(const Fighter* target) {
    return target->isHeavenlyRestricted ? DOMAIN_TOJI_DAMAGE : DOMAIN_SUREHIT_DAMAGE;
}

static void CheckDomainLost(Fighter* f) {
    if (!f->hasDomain) return;
    if (f->isHeavenlyRestricted) {
        f->hasDomain = false;
        return;
    }
    if ((f->hp / f->maxHP) < 0.30f && (f->cursedEnergy / f->maxCE) < 0.20f) {
        f->hasDomain = false;
    }
}

static CharacterID ResolveCopiedUltSource(const Fighter* copier, const Fighter* opponent) {
    CharacterID source = opponent->charData.id;
    if (source == CHAR_YUTA) {
        if (opponent->copiedUltSource < CHAR_COUNT) return opponent->copiedUltSource;
        return CHAR_GOJO;
    }
    if (source >= CHAR_COUNT) return CHAR_GOJO;
    if (copier->charData.id != CHAR_YUTA) return copier->charData.id;
    return source;
}

static UltimateType GetUltimateType(CharacterID source) {
    switch (source) {
        case CHAR_GOJO: return ULT_HOLLOW_PURPLE;
        case CHAR_SUKUNA: return ULT_DISMANTLE_CLEAVE;
        case CHAR_YUJI: return ULT_BLACK_FLASH;
        case CHAR_TOJI: return ULT_HEAVENLY_ASSAULT;
        case CHAR_YUTA: return ULT_HOLLOW_PURPLE;
        default: return ULT_NONE;
    }
}

static float GetUltimateCost(CharacterID source, const Fighter* user) {
    switch (source) {
        case CHAR_GOJO:
            return (user->charData.id == CHAR_GOJO) ? user->maxCE : GetCharacterData(CHAR_GOJO).maxCE;
        case CHAR_SUKUNA:
            return GetCharacterData(CHAR_SUKUNA).maxCE * 0.5f;
        case CHAR_YUJI:
        case CHAR_TOJI:
        default:
            return 0.0f;
    }
}

static float GetUltimateDamage(CharacterID source) {
    switch (source) {
        case CHAR_GOJO: return 150.0f;
        case CHAR_SUKUNA: return 80.0f;
        case CHAR_YUJI: return 130.0f;
        case CHAR_TOJI: return 100.0f;
        default: return 0.0f;
    }
}

static void SpawnHitBurst(const Fighter* attacker, const Fighter* target, ParticleType type,
                          int count, float speed, float life, float size) {
    Vector2 hitPos = {
        target->hitbox.x + target->hitbox.width * 0.5f,
        target->hitbox.y + target->hitbox.height * 0.5f
    };
    ParticleSpawnBurst(hitPos, count, attacker->charData.ceColor, speed, life, size, type);
}

static void RegisterYujiCombo(Fighter* attacker) {
    if (attacker->charData.id != CHAR_YUJI || attacker->ultUsed) return;
    attacker->comboHits++;
    attacker->comboTimer = YUJI_COMBO_WINDOW;
    if (attacker->comboHits >= 3) {
        attacker->ultReady = true;
        attacker->comboHits = 3;
    }
}

static void DoMeleeHit(Fighter* attacker, Fighter* target) {
    Rectangle atkBox = {
        attacker->hitbox.x + (attacker->facingDir > 0 ? attacker->hitbox.width : -65.0f),
        attacker->hitbox.y + 15.0f,
        65.0f, 55.0f
    };

    if (attacker->attackLanded || target->isDodging) return;

    if (CheckCollisionRecs(atkBox, target->hitbox)) {
        float dmg = attacker->attackDamage;
        bool blackFlashProc = false;
        attacker->attackLanded = true;

        if (attacker->isHeavenlyRestricted) dmg *= 1.5f;

        if (attacker->charData.traits.hasBlackFlash && attacker->charData.id != CHAR_YUJI) {
            float roll = (float)GetRandomValue(0, 1000) / 1000.0f;
            if (roll < attacker->charData.traits.blackFlashChance) {
                dmg *= attacker->charData.traits.blackFlashMultiplier;
                blackFlashProc = true;
                attacker->blackFlashActive = true;
                attacker->blackFlashFrames = 12;
                ParticleSpawnBurst(
                    (Vector2){ target->hitbox.x + target->hitbox.width * 0.5f, target->hitbox.y + target->hitbox.height * 0.5f },
                    30, (Color){255, 200, 30, 255}, 5.0f, 0.5f, 6.0f, PARTICLE_BLACK_FLASH
                );
                ShakeTrigger(10.0f, 0.3f);
            }
        }

        target->hp -= dmg;
        RegisterYujiCombo(attacker);
        SpawnHitBurst(attacker, target, PARTICLE_HIT_BURST,
                      blackFlashProc ? 20 : 10,
                      blackFlashProc ? 4.5f : 2.5f,
                      blackFlashProc ? 0.45f : 0.25f,
                      blackFlashProc ? 5.0f : 3.0f);
        ShakeTrigger(blackFlashProc ? 8.0f : 4.0f, 0.15f);
    }
}

static void StartProjectileAttack(Fighter* f) {
    f->projectile = (Rectangle){
        f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -22.0f),
        f->hitbox.y + 38.0f,
        22.0f, 22.0f
    };
    f->projectileActive = true;
    f->projectileSpeed  = (float)f->facingDir * PROJECTILE_SPD;
}

static void StartUltimate(Fighter* user, Fighter* target) {
    CharacterID source = (user->charData.id == CHAR_YUTA)
        ? ResolveCopiedUltSource(user, target)
        : user->charData.id;
    float cost = GetUltimateCost(source, user);

    if (user->ultUsed || user->ultActive || user->hp <= 0.0f) return;
    if (user->charData.id == CHAR_YUJI && !user->ultReady) return;
    if (source == CHAR_GOJO && user->charData.id == CHAR_GOJO && user->cursedEnergy < user->maxCE - 0.5f) return;
    if (user->cursedEnergy + 0.01f < cost) return;

    user->cursedEnergy -= cost;
    user->ultUsed = true;
    user->ultActive = true;
    user->ultHitApplied = false;
    user->activeUlt = GetUltimateType(source);
    user->ultDamage = GetUltimateDamage(source);
    user->copiedUltSource = source;

    switch (user->activeUlt) {
        case ULT_HOLLOW_PURPLE:
            user->ultDuration = 2.8f;
            user->ultTimer    = user->ultDuration;
            user->ultSpeed    = 8.5f * (float)user->facingDir;
            user->ultHitbox   = (Rectangle){
                user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width + 8.0f : -52.0f),
                user->hitbox.y + 18.0f,
                46.0f, 46.0f
            };
            ParticleSpawnBurst(
                (Vector2){ user->ultHitbox.x + 23.0f, user->ultHitbox.y + 23.0f },
                44, (Color){180, 70, 255, 255}, 5.5f, 0.8f, 6.5f, PARTICLE_HOLLOW_PURPLE
            );
            ParticleSpawnBurst(
                (Vector2){ user->hitbox.x + user->hitbox.width * 0.5f, user->hitbox.y + 10.0f },
                24, (Color){80, 180, 255, 255}, 4.0f, 0.6f, 4.5f, PARTICLE_CE_MOTE
            );
            ParticleSpawnBurst(
                (Vector2){ user->hitbox.x + user->hitbox.width * 0.5f, user->hitbox.y + 10.0f },
                24, (Color){255, 60, 80, 255}, 4.0f, 0.6f, 4.5f, PARTICLE_CE_MOTE
            );
            ShakeTrigger(12.0f, 0.45f);
            break;

        case ULT_DISMANTLE_CLEAVE:
            user->ultDuration = 1.5f;
            user->ultTimer    = user->ultDuration;
            user->ultSpeed    = 15.0f * (float)user->facingDir;
            user->ultHitbox   = (Rectangle){
                user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width : -140.0f),
                user->hitbox.y + 8.0f,
                140.0f, 75.0f
            };
            ParticleSpawnBurst(
                (Vector2){ user->hitbox.x + user->hitbox.width * 0.5f, user->hitbox.y + 30.0f },
                36, (Color){240, 60, 60, 255}, 7.0f, 0.6f, 5.0f, PARTICLE_SLASH_TRAIL
            );
            ShakeTrigger(11.0f, 0.35f);
            break;

        case ULT_BLACK_FLASH:
            user->ultDuration = 0.55f;
            user->ultTimer    = user->ultDuration;
            user->ultSpeed    = 17.0f * (float)user->facingDir;
            user->ultHitbox   = (Rectangle){
                user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width : -72.0f),
                user->hitbox.y + 12.0f,
                72.0f, 62.0f
            };
            user->ultReady = false;
            user->comboHits = 0;
            user->comboTimer = 0.0f;
            user->blackFlashActive = true;
            user->blackFlashFrames = 18;
            ParticleSpawnBurst(
                (Vector2){ user->hitbox.x + user->hitbox.width * 0.5f, user->hitbox.y + 30.0f },
                28, (Color){255, 190, 50, 255}, 5.5f, 0.5f, 5.0f, PARTICLE_BLACK_FLASH
            );
            ShakeTrigger(14.0f, 0.35f);
            break;

        case ULT_HEAVENLY_ASSAULT:
            user->ultDuration = 0.45f;
            user->ultTimer    = user->ultDuration;
            user->ultSpeed    = 19.0f * (float)user->facingDir;
            user->ultHitbox   = (Rectangle){
                user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width : -92.0f),
                user->hitbox.y + 5.0f,
                92.0f, 78.0f
            };
            ParticleSpawnBurst(
                (Vector2){ user->hitbox.x + user->hitbox.width * 0.5f, user->hitbox.y + 24.0f },
                26, (Color){220, 220, 220, 255}, 5.0f, 0.45f, 4.5f, PARTICLE_SPARK
            );
            ShakeTrigger(10.0f, 0.25f);
            break;

        case ULT_NONE:
        default:
            user->ultActive = false;
            user->ultUsed = false;
            break;
    }

    ClampFighter(user);
}

static void ApplyPhysics(Fighter* f) {
    if (f->isDodging) {
        f->hitbox.x += f->dodgeVelX;
        f->dodgeFrames--;
        if (f->dodgeFrames <= 0) {
            f->isDodging = false;
            f->dodgeVelX = 0.0f;
        }
    }

    if (!f->onGround) {
        f->velY += GRAVITY;
        f->hitbox.y += f->velY;
    }

    if (f->hitbox.y >= FLOOR_Y - f->hitbox.height) {
        f->hitbox.y = FLOOR_Y - f->hitbox.height;
        f->velY     = 0.0f;
        f->onGround = true;
    }

    if (f->hitbox.x < 0.0f) f->hitbox.x = 0.0f;
    if (f->hitbox.x > SCREEN_W - f->hitbox.width) {
        f->hitbox.x = SCREEN_W - f->hitbox.width;
    }
}

static void UpdateProjectile(Fighter* shooter, Fighter* target) {
    if (!shooter->projectileActive) return;

    shooter->projectile.x += shooter->projectileSpeed;

    if (GetRandomValue(0, 2) == 0) {
        Vector2 p = {
            shooter->projectile.x + shooter->projectile.width * 0.5f,
            shooter->projectile.y + shooter->projectile.height * 0.5f
        };
        ParticleSpawn(p, (Vector2){ -shooter->projectileSpeed * 0.15f, 0.0f },
                      shooter->charData.ceColor, 0.35f, 3.0f, PARTICLE_CE_MOTE);
    }

    if (shooter->projectile.x < -40.0f || shooter->projectile.x > SCREEN_W + 40.0f) {
        shooter->projectileActive = false;
        return;
    }

    if (!target->isDodging && CheckCollisionRecs(shooter->projectile, target->hitbox)) {
        target->hp -= shooter->charData.projectileDamage;
        shooter->projectileActive = false;
        SpawnHitBurst(shooter, target, PARTICLE_HIT_BURST, 16, 3.8f, 0.35f, 4.5f);
        ShakeTrigger(5.0f, 0.15f);
    }
}

static void UpdateUltimate(Fighter* user, Fighter* target) {
    float dt = GetFrameTime();

    if (!user->ultActive) return;

    user->ultTimer -= dt;

    switch (user->activeUlt) {
        case ULT_HOLLOW_PURPLE:
            user->ultHitbox.x += user->ultSpeed;
            user->ultHitbox.y = user->hitbox.y + 16.0f + sinf((float)GetTime() * 8.0f) * 4.0f;
            if (GetRandomValue(0, 1) == 0) {
                ParticleSpawn(
                    (Vector2){ user->ultHitbox.x + user->ultHitbox.width * 0.5f, user->ultHitbox.y + user->ultHitbox.height * 0.5f },
                    (Vector2){ (float)GetRandomValue(-20, 20) / 10.0f, (float)GetRandomValue(-20, 20) / 10.0f },
                    (Color){185, 70, 255, 255}, 0.35f, 5.0f, PARTICLE_HOLLOW_PURPLE
                );
            }
            break;

        case ULT_DISMANTLE_CLEAVE:
            user->ultHitbox.x += user->ultSpeed;
            user->ultHitbox.y = target->hitbox.y + 8.0f;
            ParticleSpawn(
                (Vector2){ user->ultHitbox.x + (user->facingDir > 0 ? 0.0f : user->ultHitbox.width), user->ultHitbox.y + user->ultHitbox.height * 0.5f },
                (Vector2){ 0.0f, (float)GetRandomValue(-6, 6) / 10.0f },
                (Color){255, 90, 90, 255}, 0.25f, 6.0f, PARTICLE_SLASH_TRAIL
            );
            break;

        case ULT_BLACK_FLASH:
        case ULT_HEAVENLY_ASSAULT:
            user->hitbox.x += user->ultSpeed;
            if (user->hitbox.x < 0.0f) user->hitbox.x = 0.0f;
            if (user->hitbox.x > SCREEN_W - user->hitbox.width) user->hitbox.x = SCREEN_W - user->hitbox.width;
            user->ultHitbox.x = user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width : -user->ultHitbox.width);
            user->ultHitbox.y = user->hitbox.y + 10.0f;
            break;

        case ULT_NONE:
        default:
            break;
    }

    if (!user->ultHitApplied && !target->isDodging && CheckCollisionRecs(user->ultHitbox, target->hitbox)) {
        target->hp -= user->ultDamage;
        user->ultHitApplied = true;
        user->ultActive = false;
        user->blackFlashActive = (user->activeUlt == ULT_BLACK_FLASH);

        if (user->activeUlt == ULT_HOLLOW_PURPLE) {
            ParticleSpawnBurst(
                (Vector2){ target->hitbox.x + target->hitbox.width * 0.5f, target->hitbox.y + target->hitbox.height * 0.5f },
                60, (Color){200, 80, 255, 255}, 7.5f, 0.8f, 7.5f, PARTICLE_HOLLOW_PURPLE
            );
            ShakeTrigger(18.0f, 0.6f);
        } else if (user->activeUlt == ULT_DISMANTLE_CLEAVE) {
            ParticleSpawnBurst(
                (Vector2){ target->hitbox.x + target->hitbox.width * 0.5f, target->hitbox.y + target->hitbox.height * 0.5f },
                42, (Color){255, 70, 70, 255}, 6.5f, 0.55f, 5.0f, PARTICLE_SLASH_TRAIL
            );
            ShakeTrigger(14.0f, 0.4f);
        } else {
            ParticleSpawnBurst(
                (Vector2){ target->hitbox.x + target->hitbox.width * 0.5f, target->hitbox.y + target->hitbox.height * 0.5f },
                34, (Color){255, 200, 60, 255}, 6.0f, 0.45f, 5.5f, PARTICLE_BLACK_FLASH
            );
            ShakeTrigger(16.0f, 0.35f);
        }
    }

    if (user->ultTimer <= 0.0f || user->ultHitbox.x < -200.0f || user->ultHitbox.x > SCREEN_W + 200.0f) {
        user->ultActive = false;
    }
}

static void UpdateAttackCooldown(Fighter* f) {
    if (f->isAttacking) {
        f->attackFrames--;
        if (f->attackFrames <= 0) {
            f->isAttacking = false;
            f->attackLanded = false;
        }
    }

    if (f->blackFlashActive) {
        f->blackFlashFrames--;
        if (f->blackFlashFrames <= 0) {
            f->blackFlashActive = false;
        }
    }

    if (f->comboTimer > 0.0f) {
        f->comboTimer -= GetFrameTime();
        if (f->comboTimer <= 0.0f) {
            f->comboTimer = 0.0f;
            f->comboHits = 0;
            if (!f->ultUsed) f->ultReady = false;
        }
    }
}

static void StartDomain(Fighter* caster, Fighter* target, bool isP1,
                        GameState* state, int* domainCasterPlayer, float* domainTimer) {
    float ceCost = CalcCECost(caster, DOMAIN_CE_COST);
    if (caster->isHeavenlyRestricted || !caster->hasDomain || caster->cursedEnergy < ceCost) return;

    caster->cursedEnergy -= ceCost;
    caster->domainActive = true;
    caster->hasDomain    = false;
    target->isStunned    = !target->isHeavenlyRestricted;
    *domainCasterPlayer  = isP1 ? 1 : 2;
    *domainTimer         = DOMAIN_COUNTER_WINDOW;
    *state               = STATE_DOMAIN;

    AnnounceStart(caster->charData.domainName, caster->charData.name, caster->charData.domainAccentColor);
    ParticleSpawnBurst(
        (Vector2){ caster->hitbox.x + caster->hitbox.width * 0.5f, caster->hitbox.y + caster->hitbox.height * 0.5f },
        96, caster->charData.domainAccentColor, 8.0f, 1.2f, 6.0f, PARTICLE_DOMAIN_ANNOUNCE
    );
    ShakeTrigger(18.0f, 0.6f);
}

static void TriggerDomainClash(Fighter* challenger, Fighter* caster, bool challengerIsP1,
                               GameState* state, float* domainTimer, DomainClashState* clash) {
    float ceCost = CalcCECost(challenger, DOMAIN_CE_COST);
    int challengerPlayer = challengerIsP1 ? 1 : 2;
    int casterPlayer = challengerIsP1 ? 2 : 1;

    if (challenger->isHeavenlyRestricted || !challenger->hasDomain || challenger->cursedEnergy < ceCost) return;

    challenger->cursedEnergy -= ceCost;
    challenger->hasDomain     = false;
    challenger->domainActive  = false;
    caster->domainActive      = false;
    challenger->isStunned     = false;
    caster->isStunned         = false;

    clash->active   = true;
    clash->timer    = DOMAIN_CLASH_DURATION;
    clash->duration = DOMAIN_CLASH_DURATION;
    clash->damage   = fabsf(caster->cursedEnergy - challenger->cursedEnergy);

    if (fabsf(caster->cursedEnergy - challenger->cursedEnergy) < 0.5f) {
        clash->winnerPlayer = 0;
        clash->damage = 0.0f;
    } else if (challenger->cursedEnergy > caster->cursedEnergy) {
        clash->winnerPlayer = challengerPlayer;
    } else {
        clash->winnerPlayer = casterPlayer;
    }

    *state = STATE_DOMAIN_CLASH;
    *domainTimer = 0.0f;
    gDomainAnnounce.active = false;

    ParticleSpawnBurst(
        (Vector2){ SCREEN_W * 0.5f, SCREEN_H * 0.5f },
        180, (Color){255, 220, 120, 255}, 9.0f, 1.2f, 7.0f, PARTICLE_CLASH_ARC
    );
    ParticleSpawnBurst(
        (Vector2){ SCREEN_W * 0.5f, SCREEN_H * 0.5f },
        120, caster->charData.domainAccentColor, 8.0f, 0.9f, 6.0f, PARTICLE_DOMAIN_SHARD
    );
    ParticleSpawnBurst(
        (Vector2){ SCREEN_W * 0.5f, SCREEN_H * 0.5f },
        120, challenger->charData.domainAccentColor, 8.0f, 0.9f, 6.0f, PARTICLE_DOMAIN_SHARD
    );
    ShakeTrigger(22.0f, 0.8f);
}

static void UpdateDomain(Fighter* caster, Fighter* target, GameState* state,
                         float* domainTimer, int* domainCasterPlayer) {
    *domainTimer -= GetFrameTime();

    if (*domainTimer <= 0.0f) {
        target->hp -= GetDomainDamage(target);
        caster->domainActive = false;
        target->isStunned    = false;
        *domainTimer         = 0.0f;
        *domainCasterPlayer  = 0;
        *state               = STATE_BATTLE;
        gDomainAnnounce.active = false;

        ParticleSpawnBurst(
            (Vector2){ SCREEN_W * 0.5f, SCREEN_H * 0.5f },
            140, caster->charData.domainAccentColor, 10.0f, 1.35f, 7.0f, PARTICLE_DOMAIN_SHARD
        );
        ShakeTrigger(20.0f, 0.75f);
    }
}

static void UpdateDomainClash(GameState* state, DomainClashState* clash, Fighter* p1, Fighter* p2, int* domainCasterPlayer) {
    clash->timer -= GetFrameTime();

    if (GetRandomValue(0, 1) == 0) {
        ParticleSpawn(
            (Vector2){ SCREEN_W * 0.5f + (float)GetRandomValue(-180, 180), SCREEN_H * 0.5f + (float)GetRandomValue(-90, 90) },
            (Vector2){ (float)GetRandomValue(-30, 30) / 10.0f, (float)GetRandomValue(-24, 24) / 10.0f },
            (Color){255, 220, 120, 255}, 0.25f, 4.0f, PARTICLE_CLASH_ARC
        );
    }

    if (clash->timer <= 0.0f) {
        if (clash->winnerPlayer == 1) {
            p2->hp -= clash->damage;
        } else if (clash->winnerPlayer == 2) {
            p1->hp -= clash->damage;
        }

        p1->domainActive = false;
        p2->domainActive = false;
        p1->isStunned = false;
        p2->isStunned = false;
        clash->active = false;
        clash->timer = 0.0f;
        *domainCasterPlayer = 0;
        *state = STATE_BATTLE;
    }
}

static void ProcessInput(Fighter* f, Fighter* opponent, bool stunLock, bool isP1,
                         GameState* state, int* domainCasterPlayer, float* domainTimer,
                         DomainClashState* clash) {
    bool actuallyStunned = stunLock && !f->isHeavenlyRestricted;
    int playerId = isP1 ? 1 : 2;
    int leftKey   = isP1 ? KEY_A          : KEY_LEFT;
    int rightKey  = isP1 ? KEY_D          : KEY_RIGHT;
    int jumpKey   = isP1 ? KEY_W          : KEY_UP;
    int crouchKey = isP1 ? KEY_LEFT_SHIFT : KEY_RIGHT_SHIFT;
    int atkKey    = isP1 ? KEY_F          : KEY_KP_0;
    int rctKey    = isP1 ? KEY_C          : KEY_KP_1;
    int domainKey = isP1 ? KEY_R          : KEY_KP_2;
    int dodgeKey  = isP1 ? KEY_Q          : KEY_KP_3;
    int ultKey    = isP1 ? KEY_X          : KEY_KP_4;
    float spd     = f->speed;

    if (f->dodgeCooldown > 0) f->dodgeCooldown--;

    if (IsKeyPressed(domainKey)) {
        if (*state == STATE_DOMAIN && *domainCasterPlayer != playerId && *domainTimer > 0.0f) {
            TriggerDomainClash(f, opponent, isP1, state, domainTimer, clash);
            return;
        }

        if (*state == STATE_BATTLE) {
            CheckDomainLost(f);
            StartDomain(f, opponent, isP1, state, domainCasterPlayer, domainTimer);
            if (*state == STATE_DOMAIN) return;
        }
    }

    if (*state == STATE_DOMAIN_CLASH) return;

    if (*state == STATE_DOMAIN) {
        if (f->domainActive) return;
        if (actuallyStunned) return;
        if (!f->isHeavenlyRestricted) return;
    }

    if (actuallyStunned) return;

    if (IsKeyPressed(ultKey) && *state == STATE_BATTLE) {
        StartUltimate(f, opponent);
    }

    f->isCrouching = IsKeyDown(crouchKey);
    if (f->isCrouching) {
        f->hitbox.height = 52.0f;
        spd *= 0.5f;
    } else {
        f->hitbox.height = 90.0f;
    }

    if (!f->isDodging && !f->ultActive) {
        if (IsKeyDown(leftKey))  { f->hitbox.x -= spd; f->facingDir = -1; }
        if (IsKeyDown(rightKey)) { f->hitbox.x += spd; f->facingDir =  1; }
        if (IsKeyPressed(jumpKey) && f->onGround) {
            f->velY = JUMP_FORCE;
            f->onGround = false;
        }
    }

    if (IsKeyPressed(dodgeKey) && !f->isDodging && f->dodgeCooldown == 0 && f->onGround && !f->ultActive) {
        f->isDodging      = true;
        f->dodgeFrames    = DODGE_FRAMES;
        f->dodgeCooldown  = DODGE_COOLDOWN_FRAMES;
        f->dodgeVelX      = (float)f->facingDir * DODGE_SPEED;
        ParticleSpawnBurst(
            (Vector2){ f->hitbox.x + f->hitbox.width * 0.5f, f->hitbox.y + f->hitbox.height * 0.5f },
            8, f->charData.ceColor, 2.5f, 0.2f, 3.0f, PARTICLE_CE_MOTE
        );
    }

    if (IsKeyPressed(atkKey) && !f->isAttacking && !f->ultActive) {
        bool forceMelee = f->isHeavenlyRestricted || f->charData.id == CHAR_YUJI || HorizontalGap(f, opponent) < 95.0f;
        f->isAttacking  = true;
        f->attackFrames = 14;
        f->attackLanded = false;

        if (forceMelee) {
            DoMeleeHit(f, opponent);
        } else {
            float ceCost = CalcCECost(f, CE_ATTACK_COST);
            if (f->cursedEnergy >= ceCost) {
                f->cursedEnergy -= ceCost;
                StartProjectileAttack(f);
            } else {
                DoMeleeHit(f, opponent);
            }
        }
    }

    if (f->isAttacking && !f->projectileActive) {
        DoMeleeHit(f, opponent);
    }

    if (IsKeyPressed(rctKey) && !f->isHeavenlyRestricted && !f->ultActive) {
        float ceCost = CalcRCTCost(f);
        if (f->cursedEnergy >= ceCost) {
            float healAmt = RCT_HEAL_AMOUNT * f->charData.traits.rctHealMultiplier;
            f->cursedEnergy -= ceCost;
            f->hp += healAmt;
            ClampFighter(f);
            ParticleSpawnBurst(
                (Vector2){ f->hitbox.x + f->hitbox.width * 0.5f, f->hitbox.y },
                12, (Color){80, 255, 120, 255}, 1.8f, 0.6f, 4.0f, PARTICLE_REGEN
            );
        }
    }
}

int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "JUJUTSU KAISEN - Cursed Clash");
    SetTargetFPS(60);

    for (int i = 0; i < MAX_PARTICLES; i++) gParticles[i].active = false;

    GameState state = STATE_CHAR_SELECT;
    SelectState p1sel = { 0, CHAR_SUKUNA, false };
    SelectState p2sel = { 4, CHAR_YUJI, false };
    Fighter p1;
    Fighter p2;
    memset(&p1, 0, sizeof(Fighter));
    memset(&p2, 0, sizeof(Fighter));

    int domainCasterPlayer = 0;
    float domainTimer = 0.0f;
    DomainClashState clash = {0};

    while (!WindowShouldClose()) {
        ShakeUpdate();
        ParticleUpdate();
        AnnounceUpdate();

        switch (state) {
            case STATE_CHAR_SELECT:
                if (IsKeyPressed(KEY_A) && p1sel.cursor > 0) p1sel.cursor--;
                if (IsKeyPressed(KEY_D) && p1sel.cursor < CHAR_COUNT - 1) p1sel.cursor++;
                if (IsKeyPressed(KEY_SPACE) && !p1sel.confirmed) {
                    p1sel.selected = (CharacterID)p1sel.cursor;
                    p1sel.confirmed = true;
                }
                if (IsKeyPressed(KEY_LEFT) && p2sel.cursor > 0) p2sel.cursor--;
                if (IsKeyPressed(KEY_RIGHT) && p2sel.cursor < CHAR_COUNT - 1) p2sel.cursor++;
                if (IsKeyPressed(KEY_ENTER) && !p2sel.confirmed) {
                    p2sel.selected = (CharacterID)p2sel.cursor;
                    p2sel.confirmed = true;
                }
                if (p1sel.confirmed && p2sel.confirmed) {
                    p1 = InitFighter(p1sel.selected, 160.0f, 1);
                    p2 = InitFighter(p2sel.selected, 740.0f, -1);
                    for (int i = 0; i < MAX_PARTICLES; i++) gParticles[i].active = false;
                    gDomainAnnounce.active = false;
                    domainCasterPlayer = 0;
                    domainTimer = 0.0f;
                    clash.active = false;
                    state = STATE_BATTLE;
                }
                break;

            case STATE_BATTLE:
            case STATE_DOMAIN:
            case STATE_DOMAIN_CLASH: {
                float p1Regen = CE_REGEN_RATE * (p1.charData.traits.hasCopy ? 2.0f : 1.0f);
                float p2Regen = CE_REGEN_RATE * (p2.charData.traits.hasCopy ? 2.0f : 1.0f);
                if (!clash.active && state != STATE_DOMAIN) {
                    p1.cursedEnergy += p1Regen;
                    p2.cursedEnergy += p2Regen;
                }
                ClampFighter(&p1);
                ClampFighter(&p2);

                bool p1Stun = (state == STATE_DOMAIN && domainCasterPlayer == 2);
                bool p2Stun = (state == STATE_DOMAIN && domainCasterPlayer == 1);

                ProcessInput(&p1, &p2, p1Stun, true, &state, &domainCasterPlayer, &domainTimer, &clash);
                ProcessInput(&p2, &p1, p2Stun, false, &state, &domainCasterPlayer, &domainTimer, &clash);

                ApplyPhysics(&p1);
                ApplyPhysics(&p2);
                UpdateProjectile(&p1, &p2);
                UpdateProjectile(&p2, &p1);
                UpdateUltimate(&p1, &p2);
                UpdateUltimate(&p2, &p1);
                UpdateAttackCooldown(&p1);
                UpdateAttackCooldown(&p2);

                if (p1.hitbox.x < p2.hitbox.x) {
                    p1.facingDir = 1;
                    p2.facingDir = -1;
                } else {
                    p1.facingDir = -1;
                    p2.facingDir = 1;
                }

                if (state == STATE_DOMAIN) {
                    Fighter* caster = (domainCasterPlayer == 1) ? &p1 : &p2;
                    Fighter* target = (domainCasterPlayer == 1) ? &p2 : &p1;
                    UpdateDomain(caster, target, &state, &domainTimer, &domainCasterPlayer);
                } else if (state == STATE_DOMAIN_CLASH && clash.active) {
                    UpdateDomainClash(&state, &clash, &p1, &p2, &domainCasterPlayer);
                }

                CheckDomainLost(&p1);
                CheckDomainLost(&p2);
                DrawFighterEffects(&p1);
                DrawFighterEffects(&p2);
                ClampFighter(&p1);
                ClampFighter(&p2);

                if (p1.hp <= 0.0f || p2.hp <= 0.0f) {
                    state = STATE_GAME_OVER;
                    ParticleSpawnBurst(
                        (Vector2){ SCREEN_W * 0.5f, SCREEN_H * 0.5f },
                        80, GOLD, 6.0f, 1.5f, 5.0f, PARTICLE_SPARK
                    );
                    ShakeTrigger(15.0f, 0.6f);
                }
                break;
            }
            case STATE_GAME_OVER:
                if (IsKeyPressed(KEY_ENTER)) {
                    state = STATE_CHAR_SELECT;
                    p1sel.confirmed = false;
                    p2sel.confirmed = false;
                    domainCasterPlayer = 0;
                    domainTimer = 0.0f;
                    clash.active = false;
                    for (int i = 0; i < MAX_PARTICLES; i++) gParticles[i].active = false;
                    gDomainAnnounce.active = false;
                }
                break;
        }

        BeginDrawing();

        switch (state) {
            case STATE_CHAR_SELECT:
                DrawCharSelectScreen(p1sel.cursor, p2sel.cursor,
                                     p1sel.confirmed, p2sel.confirmed,
                                     SCREEN_W, SCREEN_H);
                break;

            case STATE_BATTLE:
                DrawBattleBackground(SCREEN_W, SCREEN_H);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, domainTimer, false, SCREEN_W);
                AnnounceDraw(SCREEN_W, SCREEN_H);
                break;

            case STATE_DOMAIN: {
                Fighter* caster = (domainCasterPlayer == 1) ? &p1 : &p2;
                Fighter* target = (domainCasterPlayer == 1) ? &p2 : &p1;
                DrawDomainBackground(caster->charData.id, domainTimer, SCREEN_W, SCREEN_H);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, domainTimer, true, SCREEN_W);
                AnnounceDraw(SCREEN_W, SCREEN_H);
                if (target->isHeavenlyRestricted) {
                    const char* imm = "[ HEAVENLY RESTRICTION - DOMAIN STUN IMMUNE ]";
                    int iw = MeasureText(imm, 14);
                    DrawText(imm, SCREEN_W / 2 - iw / 2, (int)FLOOR_Y - 30, 14, GOLD);
                }
                break;
            }

            case STATE_DOMAIN_CLASH:
                DrawDomainClashScene(&p1, &p2, clash.timer, clash.duration,
                                     clash.winnerPlayer, clash.damage, SCREEN_W, SCREEN_H);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, clash.timer, true, SCREEN_W);
                break;

            case STATE_GAME_OVER: {
                const char* winTxt = (p2.hp <= 0.0f) ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!";
                Color winCol = (p2.hp <= 0.0f) ? p1.charData.bodyColor : p2.charData.bodyColor;
                DrawBattleBackground(SCREEN_W, SCREEN_H);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, 0.0f, false, SCREEN_W);
                DrawGameOverOverlay(winTxt, winCol, SCREEN_W, SCREEN_H);
                break;
            }
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
