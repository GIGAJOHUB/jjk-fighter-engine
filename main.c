// =============================================================================
// main.c — JJK 2D Fighter: Master Game Loop
// =============================================================================
// Architecture:
//   characters.h / characters.c  — stat data layer
//   renders.h / renders.c        — all drawing / particles / effects
//   combat.h / combat.c          — damage math, hit registration (stub calls below)
//   main.c                       — game loop, state machine, input, physics
// =============================================================================

#include "raylib.h"
#include "raymath.h"
#include "characters.h"
#include "render.h"
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// =============================================================================
// CONSTANTS
// =============================================================================
#define SCREEN_W            960
#define SCREEN_H            540
#define FLOOR_Y             400.0f
#define GRAVITY             0.55f
#define JUMP_FORCE         -14.0f
#define PROJECTILE_SPD      9.0f

#define DOMAIN_COUNTER_WINDOW   2.5f
#define DOMAIN_SUREHIT_PCT      0.80f
#define DOMAIN_TOJI_PCT         0.50f

#define RCT_CE_COST             40.0f
#define RCT_HEAL_AMOUNT         30.0f
#define CE_ATTACK_COST          15.0f
#define DOMAIN_CE_COST          50.0f
#define CE_REGEN_RATE           0.025f

#define DODGE_FRAMES            18
#define DODGE_SPEED             9.0f
#define DODGE_COOLDOWN_FRAMES   40

// =============================================================================
// FIGHTER STRUCT — full definition (mirrors renders.c forward-declare)
// =============================================================================
typedef struct Fighter {
    CharacterData   charData;
    Rectangle       hitbox;
    float           hp;
    float           maxHP;
    float           cursedEnergy;
    float           maxCE;
    float           speed;
    float           attackDamage;

    // State flags
    bool            isCrouching;
    bool            isStunned;
    bool            hasDomain;
    bool            domainActive;
    bool            isHeavenlyRestricted;

    // Physics
    int             facingDir;
    float           velY;
    bool            onGround;

    // Attack
    bool            isAttacking;
    int             attackFrames;
    bool            attackLanded;

    // Dodge
    bool            isDodging;
    int             dodgeFrames;
    float           dodgeVelX;
    int             dodgeCooldown;

    // Projectile
    bool            projectileActive;
    Rectangle       projectile;
    float           projectileSpeed;

    // Black Flash visual
    bool            blackFlashActive;
    int             blackFlashFrames;

    // Visual references (pulled from charData for convenience)
    Color           bodyColor;
    const char*     name;
} Fighter;

// =============================================================================
// GAME STATE
// =============================================================================
typedef enum {
    STATE_CHAR_SELECT = 0,
    STATE_BATTLE,
    STATE_DOMAIN,
    STATE_GAME_OVER
} GameState;

// =============================================================================
// INIT FIGHTER FROM CHAR DATA
// =============================================================================
Fighter InitFighter(CharacterID id, float startX, int facingDir) {
    Fighter f;
    memset(&f, 0, sizeof(Fighter));
    f.charData   = GetCharacterData(id);
    f.maxHP      = f.charData.maxHP;
    f.hp         = f.maxHP;
    f.maxCE      = f.charData.maxCE;
    f.cursedEnergy = f.maxCE;
    f.speed      = f.charData.baseSpeed;
    f.attackDamage = f.charData.baseAttackDamage;
    f.isHeavenlyRestricted = f.charData.traits.isHeavenlyRestricted;
    f.hasDomain  = f.charData.hasDomain;
    f.facingDir  = facingDir;
    f.bodyColor  = f.charData.bodyColor;
    f.name       = f.charData.name;
    f.hitbox     = (Rectangle){ startX, FLOOR_Y - 90.0f, 52.0f, 90.0f };
    f.onGround   = true;
    return f;
}

// =============================================================================
// CHECK DOMAIN ACCESS LOSS
// =============================================================================
void CheckDomainLost(Fighter* f) {
    if (!f->hasDomain) return;
    bool lowHP = (f->hp / f->maxHP) < 0.30f;
    bool lowCE = (f->maxCE > 0.0f) ? ((f->cursedEnergy / f->maxCE) < 0.20f) : true;
    if (lowHP && lowCE) f->hasDomain = false;
}

// =============================================================================
// PHYSICS
// =============================================================================
void ApplyPhysics(Fighter* f) {
    if (f->isDodging) {
        f->hitbox.x += f->dodgeVelX;
        f->dodgeFrames--;
        if (f->dodgeFrames <= 0) {
            f->isDodging = false;
            f->dodgeVelX = 0;
        }
    }

    if (!f->onGround) {
        f->velY += GRAVITY;
        f->hitbox.y += f->velY;
    }

    float floorContact = FLOOR_Y - f->hitbox.height;
    if (f->hitbox.y >= floorContact) {
        f->hitbox.y = floorContact;
        f->velY     = 0;
        f->onGround = true;
    }

    // Wall clamp
    if (f->hitbox.x < 0) f->hitbox.x = 0;
    if (f->hitbox.x > SCREEN_W - f->hitbox.width)
        f->hitbox.x = SCREEN_W - f->hitbox.width;
}

// =============================================================================
// CE COST CALCULATION (applies Six Eyes multiplier)
// =============================================================================
float CalcCECost(Fighter* f, float baseCost) {
    return baseCost * f->charData.traits.ceCostMultiplier;
}

// =============================================================================
// MELEE HIT CHECK
// Combat.c hook: call ApplyMeleeHit(shooter, target, dmg) for full pipeline.
// =============================================================================
void DoMeleeHit(Fighter* attacker, Fighter* target) {
    if (attacker->attackLanded) return;

    Rectangle atkBox = {
        attacker->hitbox.x + (attacker->facingDir > 0 ? attacker->hitbox.width : -65.0f),
        attacker->hitbox.y + 15.0f,
        65.0f, 55.0f
    };

    if (target->isDodging) return; // Dodge grants i-frames

    // TODO (combat.c): ApplyMeleeHit(&atkBox, target, finalDmg);
    if (CheckCollisionRecs(atkBox, target->hitbox)) {
        attacker->attackLanded = true;

        float dmg = attacker->attackDamage;
        bool blackFlashProc = false;

        // Toji physical dash bonus
        if (attacker->isHeavenlyRestricted) dmg *= 1.5f;

        // Black Flash check
        // TODO (combat.c): RollBlackFlash(attacker, &dmg) should handle this
        if (attacker->charData.traits.hasBlackFlash) {
            float roll = (float)GetRandomValue(0, 1000) / 1000.0f;
            if (roll < attacker->charData.traits.blackFlashChance) {
                dmg *= attacker->charData.traits.blackFlashMultiplier;
                blackFlashProc = true;
                attacker->blackFlashActive = true;
                attacker->blackFlashFrames = 12;
                // Big Black Flash burst
                Vector2 hitPos = {
                    target->hitbox.x + target->hitbox.width / 2,
                    target->hitbox.y + target->hitbox.height / 2
                };
                ParticleSpawnBurst(hitPos, 30, (Color){255,200,30,255},
                                   5.0f, 0.5f, 6.0f, PARTICLE_BLACK_FLASH);
                ShakeTrigger(10.0f, 0.3f);
            }
        }

        target->hp -= dmg;

        // Hit particles
        Vector2 hitPos = {
            target->hitbox.x + target->hitbox.width / 2,
            target->hitbox.y + target->hitbox.height / 2
        };
        ParticleSpawnBurst(hitPos, blackFlashProc ? 20 : 10,
                           attacker->charData.ceColor,
                           blackFlashProc ? 4.5f : 2.5f,
                           blackFlashProc ? 0.4f : 0.25f,
                           blackFlashProc ? 5.0f : 3.0f,
                           PARTICLE_HIT_BURST);
        ShakeTrigger(blackFlashProc ? 8.0f : 4.0f, 0.15f);
    }
}

// =============================================================================
// INPUT — P1 uses WASD+Shift+F+C+R+Q
//         P2 uses Arrows+RShift+KP0+KP1+KP2+KP3
// stunLock: skip input if true (and not Heavenly Restricted)
// =============================================================================
void ProcessInput(Fighter* f, Fighter* opponent, bool stunLock, bool isP1,
                  GameState* state, int* domainCasterPlayer,
                  float* domainTimer, bool* domainCountered) {

    bool actuallyStunned = stunLock && !f->isHeavenlyRestricted;
    if (actuallyStunned) return;

    // Cooldowns
    if (f->dodgeCooldown > 0) f->dodgeCooldown--;

    // Key bindings
    int leftKey   = isP1 ? KEY_A         : KEY_LEFT;
    int rightKey  = isP1 ? KEY_D         : KEY_RIGHT;
    int jumpKey   = isP1 ? KEY_W         : KEY_UP;
    int crouchKey = isP1 ? KEY_LEFT_SHIFT: KEY_RIGHT_SHIFT;
    int atkKey    = isP1 ? KEY_F         : KEY_KP_0;
    int rctKey    = isP1 ? KEY_C         : KEY_KP_1;
    int domainKey = isP1 ? KEY_R         : KEY_KP_2;
    int dodgeKey  = isP1 ? KEY_Q         : KEY_KP_3;

    float spd = f->speed;

    // ----- CROUCH -----
    f->isCrouching = IsKeyDown(crouchKey);
    if (f->isCrouching) {
        f->hitbox.height = 52.0f;
        spd *= 0.5f;
    } else {
        f->hitbox.height = 90.0f;
    }

    // ----- MOVEMENT (no move while dodging) -----
    if (!f->isDodging) {
        if (IsKeyDown(leftKey))  { f->hitbox.x -= spd; f->facingDir = -1; }
        if (IsKeyDown(rightKey)) { f->hitbox.x += spd; f->facingDir =  1; }
        if (IsKeyPressed(jumpKey) && f->onGround) {
            f->velY = JUMP_FORCE;
            f->onGround = false;
        }
    }

    // ----- DODGE (Q / KP3) -----
    // Grants full i-frames for DODGE_FRAMES. Short cooldown prevents spam.
    if (IsKeyPressed(dodgeKey) && !f->isDodging && f->dodgeCooldown == 0 && f->onGround) {
        f->isDodging      = true;
        f->dodgeFrames    = DODGE_FRAMES;
        f->dodgeCooldown  = DODGE_COOLDOWN_FRAMES;
        f->dodgeVelX      = (float)f->facingDir * DODGE_SPEED;
        // Dodge dash particles
        Vector2 pos = { f->hitbox.x + f->hitbox.width/2, f->hitbox.y + f->hitbox.height/2 };
        ParticleSpawnBurst(pos, 8, f->charData.ceColor, 2.5f, 0.2f, 3.0f, PARTICLE_CE_MOTE);
    }

    // ----- ATTACK (F / KP0) -----
    if (IsKeyPressed(atkKey) && !f->isAttacking) {
        f->isAttacking  = true;
        f->attackFrames = 14;
        f->attackLanded = false;

        if (f->isHeavenlyRestricted) {
            // Toji: immediate physical dash strike — check hit now
            DoMeleeHit(f, opponent);
        } else {
            float ceCost = CalcCECost(f, CE_ATTACK_COST);
            if (f->cursedEnergy >= ceCost) {
                f->cursedEnergy -= ceCost;
                // Fire projectile
                f->projectile = (Rectangle){
                    f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -22.0f),
                    f->hitbox.y + 38.0f, 22.0f, 22.0f
                };
                f->projectileActive = true;
                f->projectileSpeed  = (float)f->facingDir * PROJECTILE_SPD;
            } else {
                // Not enough CE → fallback melee
                DoMeleeHit(f, opponent);
            }
        }
    }

    // Melee hit window (ongoing attack frames for non-projectile)
    if (f->isAttacking && !f->projectileActive) {
        DoMeleeHit(f, opponent);
    }

    // ----- RCT: Reversed Cursed Technique (C / KP1) -----
    if (IsKeyPressed(rctKey) && !f->isHeavenlyRestricted) {
        float ceCost = CalcCECost(f, RCT_CE_COST);
        if (f->cursedEnergy >= ceCost) {
            f->cursedEnergy -= ceCost;
            // Sukuna heals more (rctHealMultiplier)
            float healAmt = RCT_HEAL_AMOUNT * f->charData.traits.rctHealMultiplier;
            f->hp += healAmt;
            if (f->hp > f->maxHP) f->hp = f->maxHP;
            // Heal particles
            Vector2 pos = { f->hitbox.x + f->hitbox.width/2, f->hitbox.y };
            ParticleSpawnBurst(pos, 12, (Color){80,255,120,255},
                               1.8f, 0.6f, 4.0f, PARTICLE_REGEN);
            // TODO (combat.c): LogRCT(f);
        }
    }

    // ----- DOMAIN EXPANSION (R / KP2) -----
    if (IsKeyPressed(domainKey)) {

        // If a domain is active and this player is NOT caster → COUNTER attempt
        if (*state == STATE_DOMAIN && !f->domainActive && !(*domainCountered)) {
            if (*domainTimer > 0.0f) {
                *domainCountered = true;
                // Domain clash — both lose domains
                // TODO (combat.c): ResolveDomainClash(caster, f);
                Vector2 center = { (float)SCREEN_W/2, (float)SCREEN_H/2 };
                ParticleSpawnBurst(center, 60, (Color){255,200,255,255},
                                   7.0f, 0.8f, 5.0f, PARTICLE_DOMAIN_SHARD);
                ShakeTrigger(14.0f, 0.5f);
            }
            return;
        }

        // Try to cast domain
        if (*state == STATE_BATTLE && !f->isHeavenlyRestricted) {
            CheckDomainLost(f);
            float ceCost = CalcCECost(f, DOMAIN_CE_COST);
            if (f->hasDomain && f->cursedEnergy >= ceCost) {
                f->cursedEnergy -= ceCost;
                f->domainActive  = true;
                f->hasDomain     = false; // domain expended after use
                *domainTimer     = DOMAIN_COUNTER_WINDOW;
                *domainCasterPlayer = isP1 ? 1 : 2;
                *state = STATE_DOMAIN;

                // Stun opponent (Heavenly Restriction immune check handled in ProcessInput)
                opponent->isStunned = !opponent->isHeavenlyRestricted;

                // Big domain announcement + particles
                AnnounceStart(f->charData.domainName,
                              f->charData.name,
                              f->charData.domainAccentColor);

                Vector2 center = { f->hitbox.x + f->hitbox.width/2,
                                   f->hitbox.y + f->hitbox.height/2 };
                ParticleSpawnBurst(center, 80, f->charData.domainAccentColor,
                                   8.0f, 1.2f, 6.0f, PARTICLE_DOMAIN_ANNOUNCE);
                ShakeTrigger(18.0f, 0.6f);
            }
        }
    }
}

// =============================================================================
// UPDATE PROJECTILE
// =============================================================================
void UpdateProjectile(Fighter* shooter, Fighter* target) {
    if (!shooter->projectileActive) return;

    shooter->projectile.x += shooter->projectileSpeed;

    if (shooter->projectile.x < -30.0f || shooter->projectile.x > SCREEN_W + 30.0f) {
        shooter->projectileActive = false;
        return;
    }

    if (!target->isDodging && CheckCollisionRecs(shooter->projectile, target->hitbox)) {
        // TODO (combat.c): ApplyProjectileHit(&shooter->projectile, target, dmg);
        float dmg = shooter->attackDamage * 0.85f;
        target->hp -= dmg;
        shooter->projectileActive = false;

        Vector2 hitPos = {
            target->hitbox.x + target->hitbox.width/2,
            target->hitbox.y + target->hitbox.height/2
        };
        ParticleSpawnBurst(hitPos, 14, shooter->charData.ceColor,
                           3.5f, 0.35f, 4.5f, PARTICLE_HIT_BURST);
        ShakeTrigger(5.0f, 0.15f);
    }
}

// =============================================================================
// UPDATE ATTACK COOLDOWN
// =============================================================================
void UpdateAttackCooldown(Fighter* f) {
    if (f->isAttacking) {
        f->attackFrames--;
        if (f->attackFrames <= 0) {
            f->isAttacking  = false;
            f->attackLanded = false;
        }
    }
    if (f->blackFlashActive) {
        f->blackFlashFrames--;
        if (f->blackFlashFrames <= 0) f->blackFlashActive = false;
    }
}

// =============================================================================
// UPDATE DOMAIN STATE — runs every frame while STATE_DOMAIN
// =============================================================================
void UpdateDomain(Fighter* caster, Fighter* target,
                  GameState* state, float* domainTimer,
                  bool* domainCountered, int* domainCasterPlayer) {

    if (*domainCountered) {
        // Clash resolution
        caster->domainActive = false;
        target->isStunned    = false;
        caster->hasDomain    = false;
        target->hasDomain    = false;
        *state = STATE_BATTLE;
        *domainTimer = 0.0f;
        *domainCasterPlayer = 0;
        gDomainAnnounce.active = false;
        return;
    }

    *domainTimer -= GetFrameTime();

    if (*domainTimer <= 0.0f) {
        // Sure-hit resolution
        // TODO (combat.c): ResolveDomainSureHit(caster, target);
        float pct = target->isHeavenlyRestricted ? DOMAIN_TOJI_PCT : DOMAIN_SUREHIT_PCT;
        float dmg = target->maxHP * pct;
        target->hp -= dmg;
        if (target->hp < 0) target->hp = 0;

        // Massive sure-hit effect
        Vector2 center = { (float)SCREEN_W/2, (float)SCREEN_H/2 };
        ParticleSpawnBurst(center, 100, caster->charData.domainAccentColor,
                           10.0f, 1.5f, 7.0f, PARTICLE_DOMAIN_SHARD);
        ShakeTrigger(20.0f, 0.8f);

        caster->domainActive = false;
        target->isStunned    = false;
        *state = STATE_BATTLE;
        *domainTimer = 0.0f;
        *domainCasterPlayer = 0;
        gDomainAnnounce.active = false;
    }
}

// =============================================================================
// CHARACTER SELECT STATE
// =============================================================================
typedef struct {
    int cursor;
    CharacterID selected;
    bool confirmed;
} SelectState;

// =============================================================================
// MAIN
// =============================================================================
int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "JUJUTSU KAISEN — Cursed Clash");
    SetTargetFPS(60);

    // Init particle pool
    for (int i = 0; i < MAX_PARTICLES; i++) gParticles[i].active = false;

    GameState state = STATE_CHAR_SELECT;

    SelectState p1sel = { 0, CHAR_SUKUNA, false };
    SelectState p2sel = { 4, CHAR_YUJI,   false };

    Fighter p1, p2;
    memset(&p1, 0, sizeof(Fighter));
    memset(&p2, 0, sizeof(Fighter));

    int    domainCasterPlayer = 0;
    float  domainTimer        = 0.0f;
    bool   domainCountered    = false;

    while (!WindowShouldClose()) {

        // =====================================================================
        // UPDATE
        // =====================================================================
        ShakeUpdate();
        ParticleUpdate();
        AnnounceUpdate();

        switch (state) {

            // -----------------------------------------------------------------
            case STATE_CHAR_SELECT: {
                // P1 cursor
                if (IsKeyPressed(KEY_A)     && p1sel.cursor > 0) p1sel.cursor--;
                if (IsKeyPressed(KEY_D)     && p1sel.cursor < CHAR_COUNT-1) p1sel.cursor++;
                if (IsKeyPressed(KEY_SPACE) && !p1sel.confirmed) {
                    p1sel.selected  = (CharacterID)p1sel.cursor;
                    p1sel.confirmed = true;
                }
                // P2 cursor
                if (IsKeyPressed(KEY_LEFT)  && p2sel.cursor > 0) p2sel.cursor--;
                if (IsKeyPressed(KEY_RIGHT) && p2sel.cursor < CHAR_COUNT-1) p2sel.cursor++;
                if (IsKeyPressed(KEY_ENTER) && !p2sel.confirmed) {
                    p2sel.selected  = (CharacterID)p2sel.cursor;
                    p2sel.confirmed = true;
                }
                // Both confirmed
                if (p1sel.confirmed && p2sel.confirmed) {
                    p1 = InitFighter(p1sel.selected, 160.0f,  1);
                    p2 = InitFighter(p2sel.selected, 740.0f, -1);
                    for (int i = 0; i < MAX_PARTICLES; i++) gParticles[i].active = false;
                    state = STATE_BATTLE;
                }
                break;
            }

            // -----------------------------------------------------------------
            case STATE_BATTLE:
            case STATE_DOMAIN: {
                // CE regen (passive) — Yuta gets 2×
                float p1Regen = CE_REGEN_RATE * (p1.charData.traits.hasCopy ? 2.0f : 1.0f);
                float p2Regen = CE_REGEN_RATE * (p2.charData.traits.hasCopy ? 2.0f : 1.0f);
                if (p1.cursedEnergy < p1.maxCE) p1.cursedEnergy += p1Regen;
                if (p2.cursedEnergy < p2.maxCE) p2.cursedEnergy += p2Regen;

                // Stun locks
                bool p1Stun = (state == STATE_DOMAIN && domainCasterPlayer == 2);
                bool p2Stun = (state == STATE_DOMAIN && domainCasterPlayer == 1);

                // Input
                ProcessInput(&p1, &p2, p1Stun, true,  &state,
                             &domainCasterPlayer, &domainTimer, &domainCountered);
                ProcessInput(&p2, &p1, p2Stun, false, &state,
                             &domainCasterPlayer, &domainTimer, &domainCountered);

                // Physics
                ApplyPhysics(&p1);
                ApplyPhysics(&p2);

                // Projectiles
                UpdateProjectile(&p1, &p2);
                UpdateProjectile(&p2, &p1);

                // Attack cooldowns
                UpdateAttackCooldown(&p1);
                UpdateAttackCooldown(&p2);

                // Auto-face opponent
                if (p1.hitbox.x < p2.hitbox.x) { p1.facingDir =  1; p2.facingDir = -1; }
                else                             { p1.facingDir = -1; p2.facingDir =  1; }

                // Domain update
                if (state == STATE_DOMAIN) {
                    Fighter* caster = (domainCasterPlayer == 1) ? &p1 : &p2;
                    Fighter* target = (domainCasterPlayer == 1) ? &p2 : &p1;
                    UpdateDomain(caster, target, &state, &domainTimer,
                                 &domainCountered, &domainCasterPlayer);
                }

                // Domain loss check
                CheckDomainLost(&p1);
                CheckDomainLost(&p2);

                // Passive CE mote particles
                DrawFighterEffects(&p1);
                DrawFighterEffects(&p2);

                // HP clamp
                if (p1.hp < 0) p1.hp = 0;
                if (p2.hp < 0) p2.hp = 0;

                // Game over
                if (p1.hp <= 0 || p2.hp <= 0) {
                    state = STATE_GAME_OVER;
                    Vector2 center = { (float)SCREEN_W/2, (float)SCREEN_H/2 };
                    ParticleSpawnBurst(center, 80, GOLD, 6.0f, 1.5f, 5.0f, PARTICLE_SPARK);
                    ShakeTrigger(15.0f, 0.6f);
                }
                break;
            }

            // -----------------------------------------------------------------
            case STATE_GAME_OVER: {
                if (IsKeyPressed(KEY_ENTER)) {
                    state = STATE_CHAR_SELECT;
                    p1sel.confirmed = false;
                    p2sel.confirmed = false;
                    domainCasterPlayer = 0;
                    domainTimer = 0.0f;
                    domainCountered = false;
                    for (int i = 0; i < MAX_PARTICLES; i++) gParticles[i].active = false;
                    gDomainAnnounce.active = false;
                }
                break;
            }
        }

        // =====================================================================
        // DRAW
        // =====================================================================
        BeginDrawing();

        switch (state) {

            case STATE_CHAR_SELECT: {
                DrawCharSelectScreen(p1sel.cursor, p2sel.cursor,
                                     p1sel.confirmed, p2sel.confirmed,
                                     SCREEN_W, SCREEN_H);
                break;
            }

            case STATE_BATTLE: {
                DrawBattleBackground(SCREEN_W, SCREEN_H);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, domainTimer, false, SCREEN_W);
                AnnounceDraw(SCREEN_W, SCREEN_H);
                break;
            }

            case STATE_DOMAIN: {
                Fighter* caster = (domainCasterPlayer == 1) ? &p1 : &p2;
                DrawDomainBackground(caster->charData.id, domainTimer, SCREEN_W, SCREEN_H);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, domainTimer, true, SCREEN_W);
                AnnounceDraw(SCREEN_W, SCREEN_H);

                // Toji immunity label
                Fighter* target = (domainCasterPlayer == 1) ? &p2 : &p1;
                if (target->isHeavenlyRestricted) {
                    const char* imm = "[ HEAVENLY RESTRICTION — STUN IMMUNE ]";
                    int iw = MeasureText(imm, 14);
                    DrawText(imm, SCREEN_W/2 - iw/2, (int)FLOOR_Y - 30, 14, GOLD);
                }
                break;
            }

            case STATE_GAME_OVER: {
                DrawBattleBackground(SCREEN_W, SCREEN_H);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, 0.0f, false, SCREEN_W);
                const char* winTxt = (p2.hp <= 0) ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!";
                Color winCol       = (p2.hp <= 0)
                    ? p1.charData.bodyColor : p2.charData.bodyColor;
                DrawGameOverOverlay(winTxt, winCol, SCREEN_W, SCREEN_H);
                break;
            }
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}