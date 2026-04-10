#include "raylib.h"
#include "netcode.h"
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
#define MAX_BEAM_TICKS          12
#define INFINITY_CE_DRAIN       18.0f
#define GOJO_BLUE_COST          18.0f
#define GOJO_RED_COST           36.0f
#define SUKUNA_DISMANTLE_COST   12.0f
#define YUTA_RIKA_COST          28.0f
#define FUGA_BLAST_RADIUS       120.0f
#define PURPLE_STARTUP_TIME      0.65f
#define ROUND_TIME_SECONDS     120.0f
#define ROUND_INTRO_TIME         1.8f
#define ROUND_END_TIME           2.2f
#define HITSTOP_HEAVY            4
#define HITSTOP_LIGHT            2
#define DODGE_INVUL_FRAMES       8
#define ATTACK_ACTIVE_START      4
#define ATTACK_ACTIVE_END       10
#define SHADOW_CLONE_TICK        0.45f
#define OVERTIME_THRESHOLD      60.0f
#define BOOGIE_MAX_CHARGES       2
#define BOOGIE_REGEN_TIME        7.0f

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
#define MAX_MENU_FRAMES        128
#define MAX_FIGHT_FRAMES       192
#define NET_RELAY_PORT        8999
#define RELAY_CONFIG_PATH     "relay.cfg"
#define PROFILE_PATH          "player_profile.txt"

static char gRelayServerIP[64] = "127.0.0.1";
#define ONLINE_RESULT_TIME      3.2f

static Font gRetroFont = {0};
static bool gRetroFontLoaded = false;
static Projectile gProjectiles[MAX_PROJECTILES] = {0};
static int gHitstopFrames = 0;

typedef enum {
    STATE_MAIN_MENU = 0,
    STATE_ABOUT,
    STATE_MULTIPLAYER,
    STATE_CHAR_SELECT,
    STATE_BATTLE,
    STATE_DOMAIN,
    STATE_DOMAIN_CLASH,
    STATE_PAUSE,
    STATE_GAME_OVER
} GameState;

typedef enum {
    MATCH_MODE_LOCAL = 0,
    MATCH_MODE_ONLINE
} MatchMode;

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

typedef struct {
    int roundNumber;
    int p1Wins;
    int p2Wins;
    float roundTimer;
    float introTimer;
    float endTimer;
    bool roundActive;
    bool matchPoint;
    char bannerText[64];
    char subText[64];
} RoundState;

typedef struct {
    Texture2D frames[MAX_MENU_FRAMES];
    int count;
    int currentFrame;
    float timer;
    float frameDuration;
} MenuVideo;

typedef struct {
    Texture2D frames[MAX_FIGHT_FRAMES];
    int count;
    int currentFrame;
    float timer;
    float frameDuration;
} FightVideo;

typedef struct {
    int cursor;
    int pauseCursor;
    bool launchBattleAfterSelect;
    GameState pausedFromState;
    bool signInOpen;
    float signInSavedTimer;
    float onlineResultTimer;
    char onlineResultText[96];
} FrontendState;

typedef struct {
    int cursor;
    int activeField;
    char username[NET_USERNAME_LEN];
    char targetUsername[NET_USERNAME_LEN];
    char incomingChallenge[NET_USERNAME_LEN];
    char matchedOpponent[NET_USERNAME_LEN];
    char statusText[128];
    bool connectedToLobby;
    bool authenticated;
    bool authRequested;
    bool waitingForMatch;
    bool hasIncomingChallenge;
    bool inOnlineMatch;
    int localPlayerIndex;
    int playerListCursor;
    bool globalListOpen;
    NetPlayerListMessage playerList;
} MultiplayerMenuState;

typedef struct {
    int charId;
    Rectangle hitbox;
    float hp;
    float maxHP;
    float cursedEnergy;
    float maxCE;
    float speed;
    float attackDamage;
    int isCrouching;
    int isStunned;
    int hasDomain;
    int domainActive;
    int isHeavenlyRestricted;
    int facingDir;
    float velY;
    int onGround;
    int isAttacking;
    int attackFrames;
    int attackLanded;
    int hitStunFrames;
    int isDodging;
    int dodgeFrames;
    float dodgeVelX;
    int dodgeCooldown;
    int blackFlashActive;
    int blackFlashFrames;
    int specialAnimFrames;
    int beamTicksApplied;
    float beamTickTimer;
    float ultStartupTimer;
    int infinityActive;
    float infinityDrainTimer;
    int ultUsed;
    int ultActive;
    int ultReady;
    int ultHitApplied;
    int activeUlt;
    Rectangle ultHitbox;
    float ultSpeed;
    float ultDamage;
    float ultTimer;
    float ultDuration;
    int comboHits;
    float comboTimer;
    int copiedUltSource;
} FighterSnapshot;

typedef struct {
    int gameState;
    int domainCasterPlayer;
    float domainTimer;
    int clashActive;
    float clashTimer;
    float clashDuration;
    int clashWinnerPlayer;
    float clashDamage;
    NetRosterState p1sel;
    NetRosterState p2sel;
    FighterSnapshot p1;
    FighterSnapshot p2;
} MatchSnapshot;

static Vector2 RetroMeasure(const char* text, float fontSize, float spacing) {
    if (gRetroFontLoaded) return MeasureTextEx(gRetroFont, text, fontSize, spacing);
    return (Vector2){ (float)MeasureText(text, (int)fontSize), fontSize };
}

static void RetroText(const char* text, Vector2 pos, float fontSize, float spacing, Color color) {
    if (gRetroFontLoaded) DrawTextEx(gRetroFont, text, pos, fontSize, spacing, color);
    else DrawText(text, (int)pos.x, (int)pos.y, (int)fontSize, color);
}

static NetInput GatherPlayerOneControls(void) {
    NetInput input = {0};
    input.left = IsKeyDown(KEY_A);
    input.right = IsKeyDown(KEY_D);
    input.jump = IsKeyDown(KEY_W);
    input.crouch = IsKeyDown(KEY_LEFT_SHIFT);
    input.attack = IsKeyDown(KEY_ONE);
    input.rct = IsKeyDown(KEY_TWO);
    input.domain = IsKeyDown(KEY_THREE);
    input.dodge = IsKeyDown(KEY_Q);
    input.ability1 = IsKeyDown(KEY_E);
    input.ability2 = IsKeyDown(KEY_R);
    input.ability3 = IsKeyDown(KEY_F);
    input.ult = IsKeyDown(KEY_X);
    return input;
}

static bool NetInputChanged(const NetInput* a, const NetInput* b) {
    return memcmp(a, b, sizeof(NetInput)) != 0;
}

static void LoadRelayConfig(void) {
    FILE* f = fopen(RELAY_CONFIG_PATH, "r");
    if (f == NULL) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strcspn(line, "\r\n");
        line[len] = '\0';
        if (len > 0) {
            snprintf(gRelayServerIP, sizeof(gRelayServerIP), "%s", line);
            break;
        }
    }
    fclose(f);
}

static void LoadSavedUsername(char* buffer, int bufferSize) {
    FILE* file = fopen(PROFILE_PATH, "rb");
    if (file == NULL) {
        buffer[0] = '\0';
        return;
    }

    if (fgets(buffer, bufferSize, file) == NULL) {
        buffer[0] = '\0';
    } else {
        size_t len = strcspn(buffer, "\r\n");
        buffer[len] = '\0';
    }
    fclose(file);
}

static void SaveUsername(const char* username) {
    FILE* file = fopen(PROFILE_PATH, "wb");
    if (file == NULL) return;
    fputs(username, file);
    fputc('\n', file);
    fclose(file);
}

static void SetOnlineResult(FrontendState* frontend, const char* username, bool won) {
    snprintf(frontend->onlineResultText, sizeof(frontend->onlineResultText),
             won ? "%s: Nah I'd WIN" : "%s: NAH, I'd FRAUD", username);
    frontend->onlineResultTimer = ONLINE_RESULT_TIME;
}

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
    f.hurtbox              = f.hitbox;
    f.pushbox              = f.hitbox;
    f.ghostHP              = f.maxHP;
    f.maxSpecialMeter      = 100.0f;
    f.onGround             = true;
    f.boogieCharges        = BOOGIE_MAX_CHARGES;
    f.copiedUltSource      = CHAR_COUNT;
    return f;
}

static void UpdateFighterBoxes(Fighter* f) {
    float crouchShrink = f->isCrouching ? 16.0f : 0.0f;
    f->hurtbox = (Rectangle){
        f->hitbox.x + 6.0f,
        f->hitbox.y + 6.0f + crouchShrink * 0.25f,
        f->hitbox.width - 12.0f,
        f->hitbox.height - 12.0f - crouchShrink * 0.5f
    };
    f->pushbox = (Rectangle){
        f->hitbox.x + 10.0f,
        f->hitbox.y + 14.0f,
        f->hitbox.width - 20.0f,
        f->hitbox.height - 14.0f
    };
}

static NetRosterState PackRosterState(const SelectState* sel) {
    NetRosterState packet = {0};
    packet.cursor = sel->cursor;
    packet.selected = (int)sel->selected;
    packet.confirmed = sel->confirmed ? 1 : 0;
    return packet;
}

static void UnpackRosterState(SelectState* sel, const NetRosterState* packet) {
    sel->cursor = packet->cursor;
    if (sel->cursor < 0) sel->cursor = 0;
    if (sel->cursor >= CHAR_COUNT) sel->cursor = CHAR_COUNT - 1;
    sel->selected = (packet->selected >= 0 && packet->selected < CHAR_COUNT)
        ? (CharacterID)packet->selected
        : CHAR_SUKUNA;
    sel->confirmed = packet->confirmed != 0;
}

static FighterSnapshot PackFighterSnapshot(const Fighter* f) {
    FighterSnapshot s = {0};
    s.charId = (int)f->charData.id;
    s.hitbox = f->hitbox;
    s.hp = f->hp;
    s.maxHP = f->maxHP;
    s.cursedEnergy = f->cursedEnergy;
    s.maxCE = f->maxCE;
    s.speed = f->speed;
    s.attackDamage = f->attackDamage;
    s.isCrouching = f->isCrouching;
    s.isStunned = f->isStunned;
    s.hasDomain = f->hasDomain;
    s.domainActive = f->domainActive;
    s.isHeavenlyRestricted = f->isHeavenlyRestricted;
    s.facingDir = f->facingDir;
    s.velY = f->velY;
    s.onGround = f->onGround;
    s.isAttacking = f->isAttacking;
    s.attackFrames = f->attackFrames;
    s.attackLanded = f->attackLanded;
    s.hitStunFrames = f->hitStunFrames;
    s.isDodging = f->isDodging;
    s.dodgeFrames = f->dodgeFrames;
    s.dodgeVelX = f->dodgeVelX;
    s.dodgeCooldown = f->dodgeCooldown;
    s.blackFlashActive = f->blackFlashActive;
    s.blackFlashFrames = f->blackFlashFrames;
    s.specialAnimFrames = f->specialAnimFrames;
    s.beamTicksApplied = f->beamTicksApplied;
    s.beamTickTimer = f->beamTickTimer;
    s.ultStartupTimer = f->ultStartupTimer;
    s.infinityActive = f->infinityActive;
    s.infinityDrainTimer = f->infinityDrainTimer;
    s.ultUsed = f->ultUsed;
    s.ultActive = f->ultActive;
    s.ultReady = f->ultReady;
    s.ultHitApplied = f->ultHitApplied;
    s.activeUlt = (int)f->activeUlt;
    s.ultHitbox = f->ultHitbox;
    s.ultSpeed = f->ultSpeed;
    s.ultDamage = f->ultDamage;
    s.ultTimer = f->ultTimer;
    s.ultDuration = f->ultDuration;
    s.comboHits = f->comboHits;
    s.comboTimer = f->comboTimer;
    s.copiedUltSource = (int)f->copiedUltSource;
    return s;
}

static void UnpackFighterSnapshot(Fighter* f, const FighterSnapshot* s) {
    CharacterID id = (s->charId >= 0 && s->charId < CHAR_COUNT) ? (CharacterID)s->charId : CHAR_SUKUNA;
    CharacterData data = GetCharacterData(id);
    memset(f, 0, sizeof(Fighter));
    f->charData = data;
    f->bodyColor = data.bodyColor;
    f->name = data.name;
    f->hitbox = s->hitbox;
    f->hp = s->hp;
    f->maxHP = s->maxHP;
    f->cursedEnergy = s->cursedEnergy;
    f->maxCE = s->maxCE;
    f->speed = s->speed;
    f->attackDamage = s->attackDamage;
    f->isCrouching = s->isCrouching != 0;
    f->isStunned = s->isStunned != 0;
    f->hasDomain = s->hasDomain != 0;
    f->domainActive = s->domainActive != 0;
    f->isHeavenlyRestricted = s->isHeavenlyRestricted != 0;
    f->facingDir = s->facingDir;
    f->velY = s->velY;
    f->onGround = s->onGround != 0;
    f->isAttacking = s->isAttacking != 0;
    f->attackFrames = s->attackFrames;
    f->attackLanded = s->attackLanded != 0;
    f->hitStunFrames = s->hitStunFrames;
    f->isDodging = s->isDodging != 0;
    f->dodgeFrames = s->dodgeFrames;
    f->dodgeVelX = s->dodgeVelX;
    f->dodgeCooldown = s->dodgeCooldown;
    f->blackFlashActive = s->blackFlashActive != 0;
    f->blackFlashFrames = s->blackFlashFrames;
    f->specialAnimFrames = s->specialAnimFrames;
    f->beamTicksApplied = s->beamTicksApplied;
    f->beamTickTimer = s->beamTickTimer;
    f->ultStartupTimer = s->ultStartupTimer;
    f->infinityActive = s->infinityActive != 0;
    f->infinityDrainTimer = s->infinityDrainTimer;
    f->ultUsed = s->ultUsed != 0;
    f->ultActive = s->ultActive != 0;
    f->ultReady = s->ultReady != 0;
    f->ultHitApplied = s->ultHitApplied != 0;
    f->activeUlt = (UltimateType)s->activeUlt;
    f->ultHitbox = s->ultHitbox;
    f->ultSpeed = s->ultSpeed;
    f->ultDamage = s->ultDamage;
    f->ultTimer = s->ultTimer;
    f->ultDuration = s->ultDuration;
    f->comboHits = s->comboHits;
    f->comboTimer = s->comboTimer;
    f->copiedUltSource = (s->copiedUltSource >= 0 && s->copiedUltSource < CHAR_COUNT)
        ? (CharacterID)s->copiedUltSource
        : CHAR_COUNT;
}

static MatchSnapshot BuildMatchSnapshot(GameState state, int domainCasterPlayer, float domainTimer,
                                        const DomainClashState* clash, const SelectState* p1sel,
                                        const SelectState* p2sel, const Fighter* p1, const Fighter* p2) {
    MatchSnapshot snapshot = {0};
    snapshot.gameState = (int)state;
    snapshot.domainCasterPlayer = domainCasterPlayer;
    snapshot.domainTimer = domainTimer;
    snapshot.clashActive = clash->active ? 1 : 0;
    snapshot.clashTimer = clash->timer;
    snapshot.clashDuration = clash->duration;
    snapshot.clashWinnerPlayer = clash->winnerPlayer;
    snapshot.clashDamage = clash->damage;
    snapshot.p1sel = PackRosterState(p1sel);
    snapshot.p2sel = PackRosterState(p2sel);
    snapshot.p1 = PackFighterSnapshot(p1);
    snapshot.p2 = PackFighterSnapshot(p2);
    return snapshot;
}

static void ApplyMatchSnapshot(const MatchSnapshot* snapshot, GameState* state, int* domainCasterPlayer,
                               float* domainTimer, DomainClashState* clash, SelectState* p1sel,
                               SelectState* p2sel, Fighter* p1, Fighter* p2) {
    *state = (GameState)snapshot->gameState;
    *domainCasterPlayer = snapshot->domainCasterPlayer;
    *domainTimer = snapshot->domainTimer;
    clash->active = snapshot->clashActive != 0;
    clash->timer = snapshot->clashTimer;
    clash->duration = snapshot->clashDuration;
    clash->winnerPlayer = snapshot->clashWinnerPlayer;
    clash->damage = snapshot->clashDamage;
    UnpackRosterState(p1sel, &snapshot->p1sel);
    UnpackRosterState(p2sel, &snapshot->p2sel);
    UnpackFighterSnapshot(p1, &snapshot->p1);
    UnpackFighterSnapshot(p2, &snapshot->p2);
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
        case CHAR_SUKUNA: return ULT_FUGA;
        case CHAR_YUTA: return ULT_PURE_LOVE_BEAM;
        case CHAR_MEGUMI: return ULT_MAHORAGA;
        case CHAR_NANAMI: return ULT_OVERTIME_SLASH;
        case CHAR_NOBARA: return ULT_MAXIMUM_RESONANCE;
        case CHAR_TODO: return ULT_ULTIMATE_TACKLE;
        case CHAR_YUJI: return ULT_BLACK_FLASH;
        case CHAR_TOJI: return ULT_HEAVENLY_ASSAULT;
        default: return ULT_NONE;
    }
}

static float GetUltimateCost(CharacterID source, const Fighter* user) {
    switch (source) {
        case CHAR_GOJO:
            return (user->charData.id == CHAR_GOJO) ? user->maxCE : GetCharacterData(CHAR_GOJO).maxCE;
        case CHAR_SUKUNA:
            return GetCharacterData(CHAR_SUKUNA).maxCE * 0.5f;
        case CHAR_YUTA:
            return GetCharacterData(CHAR_YUTA).maxCE * 0.65f;
        case CHAR_MEGUMI:
            return 0.0f;
        case CHAR_NANAMI:
            return 0.0f;
        case CHAR_NOBARA:
            return 0.0f;
        case CHAR_TODO:
            return GetCharacterData(CHAR_TODO).maxCE * 0.45f;
        case CHAR_YUJI:
        case CHAR_TOJI:
        default:
            return 0.0f;
    }
}

static float GetUltimateDamage(CharacterID source) {
    switch (source) {
        case CHAR_GOJO: return 0.0f;
        case CHAR_SUKUNA: return 0.0f;
        case CHAR_YUTA: return 12.0f;
        case CHAR_MEGUMI: return 60.0f;
        case CHAR_NANAMI: return 110.0f;
        case CHAR_NOBARA: return 0.0f;
        case CHAR_TODO: return 65.0f;
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

static void ResetProjectiles(void) {
    memset(gProjectiles, 0, sizeof(gProjectiles));
}

static void TriggerHitstop(int frames) {
    if (frames > gHitstopFrames) gHitstopFrames = frames;
}

static Projectile* AllocateProjectile(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!gProjectiles[i].active) {
            memset(&gProjectiles[i], 0, sizeof(gProjectiles[i]));
            gProjectiles[i].active = true;
            return &gProjectiles[i];
        }
    }
    return NULL;
}

static Projectile* SpawnProjectile(ProjectileType type, const Fighter* owner, bool ownerIsP1,
                                   Rectangle hitbox, Vector2 velocity, float damage, float lifetime,
                                   float pushStrength, bool pullsTarget, bool dodgeable,
                                   float explosionRadius, float explosionDamage, Color color) {
    Projectile* p = AllocateProjectile();
    if (p == NULL) return NULL;
    p->type = type;
    p->ownerIsP1 = ownerIsP1;
    p->ownerCharacter = owner->charData.id;
    p->hitbox = hitbox;
    p->velocity = velocity;
    p->damage = damage;
    p->lifetime = lifetime;
    p->pushStrength = pushStrength;
    p->pullsTarget = pullsTarget;
    p->dodgeable = dodgeable;
    p->explosionRadius = explosionRadius;
    p->explosionDamage = explosionDamage;
    p->color = color;
    return p;
}

static bool IsInfinityBlocking(const Fighter* attacker, Fighter* target, bool bypassInfinity, float* damageScale) {
    if (bypassInfinity || target->charData.id != CHAR_GOJO || !target->infinityActive) return false;

    if (attacker->charData.id == CHAR_TOJI) {
        *damageScale *= 0.6f;
        return false;
    }

    ParticleSpawnBurst(
        (Vector2){ target->hitbox.x + target->hitbox.width * 0.5f, target->hitbox.y + target->hitbox.height * 0.5f },
        14, (Color){150, 220, 255, 255}, 3.5f, 0.35f, 4.0f, PARTICLE_SPARK
    );
    return true;
}

static void ClampHitboxX(Fighter* f) {
    if (f->hitbox.x < 0.0f) f->hitbox.x = 0.0f;
    if (f->hitbox.x > SCREEN_W - f->hitbox.width) f->hitbox.x = SCREEN_W - f->hitbox.width;
}

static bool ApplyCombatHit(Fighter* attacker, Fighter* target, float damage, float displacement,
                           int stunFrames, bool bypassInfinity, bool dodgeable, bool pullsTarget,
                           ParticleType particleType, int particleCount) {
    float damageScale = 1.0f;
    float direction;

    if (dodgeable && target->isDodging && target->dodgeInvulFrames > 0) return false;
    if (IsInfinityBlocking(attacker, target, bypassInfinity, &damageScale)) return false;

    target->hp -= damage * damageScale;
    target->lastDamageTaken = damage * damageScale;
    if (target->charData.id == CHAR_MEGUMI) {
        target->specialMeter += damage * damageScale * 0.55f;
        if (target->specialMeter > target->maxSpecialMeter) target->specialMeter = target->maxSpecialMeter;
    }
    if (stunFrames > target->hitStunFrames) target->hitStunFrames = stunFrames;

    if (displacement != 0.0f) {
        if (pullsTarget) {
            direction = (attacker->hitbox.x > target->hitbox.x) ? 1.0f : -1.0f;
        } else {
            direction = (float)attacker->facingDir;
        }
        target->hitbox.x += direction * displacement * damageScale;
        ClampHitboxX(target);
    }

    SpawnHitBurst(attacker, target, particleType, particleCount, 4.0f, 0.35f, 4.5f);
    ShakeTrigger(5.0f + displacement * 0.03f, 0.16f);
    TriggerHitstop((damage >= 60.0f || displacement >= 80.0f) ? HITSTOP_HEAVY : HITSTOP_LIGHT);
    attacker->comboCounter++;
    attacker->comboDisplayTimer = 1.3f;
    target->comboCounter = 0;
    target->comboDisplayTimer = 0.0f;
    ClampFighter(target);
    return true;
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
    float reach = 65.0f;
    float height = 55.0f;
    float verticalOffset = 15.0f;
    Rectangle atkBox = {
        attacker->hitbox.x + (attacker->facingDir > 0 ? attacker->hitbox.width : -reach),
        attacker->hitbox.y + verticalOffset,
        reach, height
    };

    if (attacker->charData.id == CHAR_TOJI) reach = 84.0f;
    if (attacker->charData.id == CHAR_NOBARA) reach = 56.0f;
    atkBox.x = attacker->hitbox.x + (attacker->facingDir > 0 ? attacker->hitbox.width : -reach);
    atkBox.width = reach;

    if (attacker->attackLanded || attacker->attackFrames > ATTACK_ACTIVE_END || attacker->attackFrames < ATTACK_ACTIVE_START) return;

    if (CheckCollisionRecs(atkBox, target->hurtbox)) {
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

        if (ApplyCombatHit(attacker, target, dmg, attacker->isHeavenlyRestricted ? 40.0f : 18.0f,
                           blackFlashProc ? 18 : 8, false, true, false,
                           PARTICLE_HIT_BURST, blackFlashProc ? 20 : 10)) {
            RegisterYujiCombo(attacker);
        }
    }
}

static void StartProjectileAttack(Fighter* f, bool ownerIsP1) {
    Rectangle hitbox = {
        f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -22.0f),
        f->hitbox.y + 38.0f,
        22.0f, 22.0f
    };
    SpawnProjectile(PROJ_CE_BLAST, f, ownerIsP1, hitbox,
                    (Vector2){ (float)f->facingDir * PROJECTILE_SPD, 0.0f },
                    f->charData.projectileDamage, 2.3f, 18.0f, false, true, 0.0f, 0.0f, f->charData.ceColor);
}

static void ToggleInfinity(Fighter* f) {
    if (f->charData.id != CHAR_GOJO || f->isHeavenlyRestricted) return;
    if (!f->infinityActive && f->cursedEnergy < 12.0f) return;
    f->infinityActive = !f->infinityActive;
    f->specialAnimFrames = 18;
}

static void UseAbility1(Fighter* f, Fighter* opponent, bool ownerIsP1) {
    if (f->ultActive || f->hp <= 0.0f) return;

    switch (f->charData.id) {
        case CHAR_GOJO:
            if (f->cursedEnergy >= CalcCECost(f, GOJO_BLUE_COST)) {
                Rectangle blue = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -24.0f), f->hitbox.y + 26.0f, 24.0f, 24.0f };
                f->cursedEnergy -= CalcCECost(f, GOJO_BLUE_COST);
                SpawnProjectile(PROJ_GOJO_BLUE, f, ownerIsP1, blue,
                                (Vector2){ (float)f->facingDir * 6.0f, 0.0f }, 20.0f, 3.2f, 70.0f, true, true, 0.0f, 0.0f,
                                (Color){110, 180, 255, 255});
                f->specialAnimFrames = 14;
            }
            break;

        case CHAR_SUKUNA:
            if (f->cursedEnergy >= CalcCECost(f, SUKUNA_DISMANTLE_COST)) {
                Rectangle slash = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -34.0f), f->hitbox.y + 28.0f, 34.0f, 16.0f };
                f->cursedEnergy -= CalcCECost(f, SUKUNA_DISMANTLE_COST);
                SpawnProjectile(PROJ_SUKUNA_DISMANTLE, f, ownerIsP1, slash,
                                (Vector2){ (float)f->facingDir * 15.0f, 0.0f }, 16.0f, 1.7f, 10.0f, false, true, 0.0f, 0.0f,
                                (Color){255, 90, 90, 255});
                f->specialAnimFrames = 10;
            }
            break;

        case CHAR_YUTA: {
            Rectangle katana = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -96.0f), f->hitbox.y + 14.0f, 96.0f, 54.0f };
            f->specialAnimFrames = 12;
            if (CheckCollisionRecs(katana, opponent->hurtbox)) {
                ApplyCombatHit(f, opponent, 28.0f, 34.0f, 12, false, true, false, PARTICLE_SLASH_TRAIL, 16);
            }
            break;
        }

        case CHAR_MEGUMI:
            if (f->cursedEnergy >= 18.0f) {
                Rectangle nue = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -26.0f), f->hitbox.y + 18.0f, 26.0f, 18.0f };
                f->cursedEnergy -= 18.0f;
                SpawnProjectile(PROJ_MEGUMI_NUE, f, ownerIsP1, nue,
                                (Vector2){ (float)f->facingDir * 8.5f, (opponent->hitbox.y < f->hitbox.y ? -0.5f : 0.5f) }, 24.0f, 2.8f,
                                22.0f, false, true, 0.0f, 0.0f, (Color){120, 150, 255, 255});
                f->specialAnimFrames = 14;
            }
            break;

        case CHAR_NANAMI: {
            Rectangle ratio = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -88.0f), f->hitbox.y + 8.0f, 88.0f, 70.0f };
            f->specialAnimFrames = 18;
            if (CheckCollisionRecs(ratio, opponent->hurtbox)) {
                float bonus = (opponent->cursedEnergy * 7.0f) / 3.0f;
                ApplyCombatHit(f, opponent, 18.0f + bonus, 36.0f, 14, false, true, false, PARTICLE_SPARK, 22);
            }
            break;
        }

        case CHAR_NOBARA:
            if (f->cursedEnergy >= 16.0f) {
                Rectangle nail = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -20.0f), f->hitbox.y + 24.0f, 20.0f, 10.0f };
                f->cursedEnergy -= 16.0f;
                SpawnProjectile(PROJ_NOBARA_NAIL, f, ownerIsP1, nail,
                                (Vector2){ (float)f->facingDir * 11.0f, 0.0f }, 18.0f, 1.9f, 12.0f, false, true, 0.0f, 0.0f,
                                (Color){255, 170, 190, 255});
                f->specialAnimFrames = 11;
            }
            break;

        case CHAR_TODO: {
            Rectangle smash = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -100.0f), f->hitbox.y + 10.0f, 100.0f, 72.0f };
            f->specialAnimFrames = 16;
            if (CheckCollisionRecs(smash, opponent->hurtbox)) {
                float damage = f->clapBuff ? 56.0f : 36.0f;
                ApplyCombatHit(f, opponent, damage, 52.0f, 18, false, true, false, PARTICLE_HIT_BURST, 22);
                f->clapBuff = false;
            }
            break;
        }

        default:
            break;
    }
}

static void UseAbility2(Fighter* f, Fighter* opponent, bool ownerIsP1) {
    if (f->ultActive || f->hp <= 0.0f) return;

    switch (f->charData.id) {
        case CHAR_GOJO:
            if (f->cursedEnergy >= CalcCECost(f, GOJO_RED_COST)) {
                Rectangle red = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -26.0f), f->hitbox.y + 22.0f, 26.0f, 26.0f };
                f->cursedEnergy -= CalcCECost(f, GOJO_RED_COST);
                SpawnProjectile(PROJ_GOJO_RED, f, ownerIsP1, red,
                                (Vector2){ (float)f->facingDir * 13.5f, 0.0f }, 40.0f, 1.7f, 120.0f, false, true, 0.0f, 0.0f,
                                (Color){255, 90, 90, 255});
                f->specialAnimFrames = 16;
            }
            break;

        case CHAR_SUKUNA: {
            Rectangle cleave = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -76.0f), f->hitbox.y + 10.0f, 76.0f, 60.0f };
            f->specialAnimFrames = 14;
            if (HorizontalGap(f, opponent) < 82.0f && CheckCollisionRecs(cleave, opponent->hurtbox)) {
                ApplyCombatHit(f, opponent, 46.0f, 62.0f, 14, false, true, false, PARTICLE_SLASH_TRAIL, 20);
            }
            break;
        }

        case CHAR_YUTA: {
            float ceCost = CalcCECost(f, YUTA_RIKA_COST);
            Rectangle rika = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width + 8.0f : -104.0f), f->hitbox.y + 6.0f, 104.0f, 74.0f };
            if (f->cursedEnergy >= ceCost) {
                f->cursedEnergy -= ceCost;
                f->specialAnimFrames = 18;
                if (CheckCollisionRecs(rika, opponent->hurtbox)) {
                    ApplyCombatHit(f, opponent, 44.0f, 58.0f, 24, false, true, false, PARTICLE_HIT_BURST, 26);
                }
            }
            break;
        }

        case CHAR_MEGUMI:
            if (f->cursedEnergy >= 20.0f) {
                Rectangle dog1 = { f->hitbox.x - 24.0f, f->hitbox.y + 34.0f, 24.0f, 18.0f };
                Rectangle dog2 = { f->hitbox.x + f->hitbox.width, f->hitbox.y + 20.0f, 24.0f, 18.0f };
                f->cursedEnergy -= 20.0f;
                SpawnProjectile(PROJ_MEGUMI_DOG, f, ownerIsP1, dog1, (Vector2){ (float)f->facingDir * 9.0f, -0.1f }, 18.0f, 1.6f, 18.0f, false, true, 0.0f, 0.0f, WHITE);
                SpawnProjectile(PROJ_MEGUMI_DOG, f, ownerIsP1, dog2, (Vector2){ (float)f->facingDir * 10.0f, 0.15f }, 18.0f, 1.6f, 18.0f, false, true, 0.0f, 0.0f, (Color){180, 180, 255, 255});
                f->specialAnimFrames = 16;
            }
            break;

        case CHAR_NANAMI: {
            Rectangle collapse = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -110.0f), f->hitbox.y - 6.0f, 110.0f, 92.0f };
            f->specialAnimFrames = 18;
            if (CheckCollisionRecs(collapse, opponent->hurtbox)) {
                ApplyCombatHit(f, opponent, 40.0f, 88.0f, 18, false, true, false, PARTICLE_SPARK, 28);
            }
            break;
        }

        case CHAR_NOBARA:
            if (opponent->dollMarked) {
                f->specialAnimFrames = 14;
                ApplyCombatHit(f, opponent, f->lastDamageTaken > 0.0f ? f->lastDamageTaken : 24.0f, 0.0f, 12, true, false, false, PARTICLE_HIT_BURST, 18);
            }
            break;

        case CHAR_TODO:
            if (f->boogieCharges > 0) {
                float tempX = f->hitbox.x;
                f->hitbox.x = opponent->hitbox.x;
                opponent->hitbox.x = tempX;
                ClampHitboxX(f);
                ClampHitboxX(opponent);
                UpdateFighterBoxes(f);
                UpdateFighterBoxes(opponent);
                f->boogieCharges--;
                f->boogieChargeTimer = 0.0f;
                f->specialAnimFrames = 14;
                TriggerHitstop(HITSTOP_LIGHT);
            }
            break;

        default:
            break;
    }
}

static void UseAbility3(Fighter* f, bool ownerIsP1) {
    switch (f->charData.id) {
        case CHAR_GOJO:
            ToggleInfinity(f);
            break;
        case CHAR_MEGUMI:
            f->specialAnimFrames = 16;
            f->hasDomain = true;
            break;
        case CHAR_NANAMI:
            if (!f->bindingVowUsed && f->hp > f->maxHP * 0.35f) {
                f->hp -= f->maxHP * 0.30f;
                f->attackDamage *= 1.35f;
                f->bindingVowUsed = true;
                f->overtimeBuff = true;
                f->specialAnimFrames = 18;
                ClampFighter(f);
            }
            break;
        case CHAR_NOBARA:
            if (f->cursedEnergy >= 14.0f) {
                Rectangle hairpin = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -18.0f), f->hitbox.y + 20.0f, 18.0f, 14.0f };
                f->cursedEnergy -= 14.0f;
                SpawnProjectile(PROJ_NOBARA_HAIRPIN, f, ownerIsP1, hairpin,
                                (Vector2){ (float)f->facingDir * 9.0f, -4.8f }, 22.0f, 1.5f, 12.0f, false, true, 0.0f, 0.0f,
                                (Color){255, 190, 210, 255});
                f->specialAnimFrames = 12;
            }
            break;
        case CHAR_TODO:
            f->clapBuff = true;
            f->specialAnimFrames = 12;
            break;
        default:
            break;
    }
}

static void StartUltimate(Fighter* user, Fighter* target) {
    CharacterID source = (user->charData.id == CHAR_YUTA)
        ? ResolveCopiedUltSource(user, target)
        : user->charData.id;
    float cost = GetUltimateCost(source, user);

    if (user->ultUsed || user->ultActive || user->hp <= 0.0f) return;
    if (user->charData.id == CHAR_YUJI && !user->ultReady) return;
    if (user->charData.id == CHAR_MEGUMI && user->specialMeter < user->maxSpecialMeter) return;
    if (user->charData.id == CHAR_NANAMI && user->passiveTimer < OVERTIME_THRESHOLD) return;
    if (user->charData.id == CHAR_NOBARA && !target->dollMarked) return;
    if (source == CHAR_GOJO && user->charData.id == CHAR_GOJO && user->cursedEnergy < user->maxCE - 0.5f) return;
    if (user->cursedEnergy + 0.01f < cost) return;
    if (source == CHAR_NOBARA && user->hp <= user->maxHP * 0.45f) return;

    user->cursedEnergy -= cost;
    if (source == CHAR_NOBARA) {
        user->hp -= user->maxHP * 0.4f;
    }
    user->ultUsed = true;
    user->ultActive = true;
    user->ultHitApplied = false;
    user->activeUlt = GetUltimateType(source);
    user->ultDamage = GetUltimateDamage(source);
    user->copiedUltSource = source;
    user->ultStartupTimer = 0.0f;
    user->beamTicksApplied = 0;
    user->beamTickTimer = 0.0f;

    switch (user->activeUlt) {
        case ULT_HOLLOW_PURPLE:
            user->ultDuration = 2.4f;
            user->ultTimer    = user->ultDuration;
            user->ultStartupTimer = PURPLE_STARTUP_TIME;
            user->ultSpeed    = 6.0f * (float)user->facingDir;
            user->ultHitbox   = (Rectangle){
                user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width + 20.0f : -120.0f),
                user->hitbox.y + 10.0f,
                120.0f, 56.0f
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

        case ULT_FUGA:
            user->ultDuration = 2.0f;
            user->ultTimer    = user->ultDuration;
            user->ultStartupTimer = 0.45f;
            user->ultSpeed    = 8.0f * (float)user->facingDir;
            user->ultHitbox   = (Rectangle){
                user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width : -42.0f),
                user->hitbox.y + 20.0f,
                42.0f, 18.0f
            };
            ParticleSpawnBurst(
                (Vector2){ user->hitbox.x + user->hitbox.width * 0.5f, user->hitbox.y + 30.0f },
                36, (Color){255, 130, 40, 255}, 7.0f, 0.6f, 5.0f, PARTICLE_SPARK
            );
            ShakeTrigger(11.0f, 0.35f);
            break;

        case ULT_PURE_LOVE_BEAM:
            user->ultDuration = 1.35f;
            user->ultTimer = user->ultDuration;
            user->ultStartupTimer = 0.25f;
            user->ultHitbox = (Rectangle){
                user->facingDir > 0 ? user->hitbox.x + user->hitbox.width : 0.0f,
                user->hitbox.y + 8.0f,
                user->facingDir > 0 ? SCREEN_W - (user->hitbox.x + user->hitbox.width) : user->hitbox.x,
                70.0f
            };
            user->ultDamage = 12.0f;
            user->beamTicksApplied = 0;
            user->beamTickTimer = 0.0f;
            ShakeTrigger(12.0f, 0.35f);
            break;

        case ULT_MAHORAGA:
            user->ultDuration = 3.2f;
            user->ultTimer = user->ultDuration;
            user->ultSpeed = 4.0f * (float)user->facingDir;
            user->ultHitbox = (Rectangle){
                user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width : -96.0f),
                user->hitbox.y - 8.0f,
                96.0f, 96.0f
            };
            user->ultDamage = user->mahoragaAdapted ? 90.0f : 60.0f;
            user->specialMeter = 0.0f;
            break;

        case ULT_OVERTIME_SLASH:
            user->ultDuration = 0.4f;
            user->ultTimer = user->ultDuration;
            user->ultHitbox = (Rectangle){
                user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width : -140.0f),
                user->hitbox.y + 4.0f,
                140.0f, 82.0f
            };
            user->ultDamage = 115.0f;
            TriggerHitstop(HITSTOP_LIGHT);
            break;

        case ULT_MAXIMUM_RESONANCE:
            user->ultDuration = 0.3f;
            user->ultTimer = user->ultDuration;
            user->ultHitbox = target->hurtbox;
            user->ultDamage = target->maxHP * 0.6f;
            break;

        case ULT_ULTIMATE_TACKLE:
            user->ultDuration = 1.0f;
            user->ultTimer = user->ultDuration;
            user->ultSpeed = 16.0f * (float)user->facingDir;
            user->ultHitbox = (Rectangle){
                user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width : -120.0f),
                user->hitbox.y + 8.0f,
                120.0f, 72.0f
            };
            user->ultDamage = 65.0f;
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

    UpdateFighterBoxes(f);
}

static void ResolvePushboxes(Fighter* p1, Fighter* p2) {
    if (CheckCollisionRecs(p1->pushbox, p2->pushbox)) {
        float overlap = (p1->pushbox.x + p1->pushbox.width) - p2->pushbox.x;
        if (overlap > 0.0f) {
            p1->hitbox.x -= overlap * 0.5f;
            p2->hitbox.x += overlap * 0.5f;
            ClampHitboxX(p1);
            ClampHitboxX(p2);
            UpdateFighterBoxes(p1);
            UpdateFighterBoxes(p2);
        }
    }
}

static void TriggerFugaExplosion(const Fighter* attacker, Fighter* target, Vector2 center) {
    float damageScale = 1.0f;
    bool dodged = target->isDodging;
    bool blocked = IsInfinityBlocking(attacker, target, false, &damageScale);

    ParticleSpawnBurst(center, 54, (Color){255, 150, 40, 255}, 8.0f, 0.8f, 6.5f, PARTICLE_SPARK);
    ParticleSpawnBurst(center, 40, (Color){255, 70, 30, 255}, 6.0f, 0.7f, 5.0f, PARTICLE_HIT_BURST);

    if (!dodged && !blocked &&
        CheckCollisionCircles(center, FUGA_BLAST_RADIUS, (Vector2){ target->hitbox.x + target->hitbox.width * 0.5f, target->hitbox.y + target->hitbox.height * 0.5f }, target->hitbox.width * 0.55f)) {
        target->hp -= target->maxHP * 0.5f * damageScale;
        target->hitStunFrames = 24;
        target->hitbox.x += attacker->facingDir * 90.0f * damageScale;
        ClampHitboxX(target);
    }
    ShakeTrigger(16.0f, 0.45f);
    ClampFighter(target);
}

static void UpdateProjectiles(Fighter* p1, Fighter* p2) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile* proj = &gProjectiles[i];
        Fighter* attacker;
        Fighter* target;
        Vector2 center;

        if (!proj->active) continue;
        attacker = proj->ownerIsP1 ? p1 : p2;
        target = proj->ownerIsP1 ? p2 : p1;

        proj->hitbox.x += proj->velocity.x;
        proj->hitbox.y += proj->velocity.y;
        proj->lifetime -= GetFrameTime();
        if (proj->type == PROJ_MEGUMI_NUE) {
            float targetCenterY = target->hurtbox.y + target->hurtbox.height * 0.5f;
            float projCenterY = proj->hitbox.y + proj->hitbox.height * 0.5f;
            float drift = (targetCenterY - projCenterY) * 0.02f;
            if (drift < -1.0f) drift = -1.0f;
            if (drift > 1.0f) drift = 1.0f;
            proj->velocity.y = drift;
        }

        center = (Vector2){ proj->hitbox.x + proj->hitbox.width * 0.5f, proj->hitbox.y + proj->hitbox.height * 0.5f };
        ParticleSpawn(center, (Vector2){ -proj->velocity.x * 0.08f, 0.0f }, proj->color, 0.28f, 3.0f,
                      proj->type == PROJ_SUKUNA_DISMANTLE ? PARTICLE_SLASH_TRAIL : PARTICLE_CE_MOTE);

        if (proj->lifetime <= 0.0f || proj->hitbox.x < -220.0f || proj->hitbox.x > SCREEN_W + 220.0f) {
            if (proj->type == PROJ_FUGA_ARROW) {
                TriggerFugaExplosion(attacker, target, center);
            }
            proj->active = false;
            continue;
        }

        if (CheckCollisionRecs(proj->hitbox, target->hurtbox)) {
            if (proj->type == PROJ_FUGA_ARROW) {
                TriggerFugaExplosion(attacker, target, center);
            } else {
                ApplyCombatHit(attacker, target, proj->damage, proj->pushStrength,
                               (proj->type == PROJ_GOJO_RED || proj->type == PROJ_NOBARA_HAIRPIN) ? 20 : 10, false, proj->dodgeable,
                               proj->pullsTarget,
                               (proj->type == PROJ_SUKUNA_DISMANTLE || proj->type == PROJ_NOBARA_NAIL) ? PARTICLE_SLASH_TRAIL : PARTICLE_HIT_BURST,
                               proj->type == PROJ_GOJO_RED ? 28 : 16);
                if (proj->type == PROJ_NOBARA_NAIL) {
                    target->dollMarked = true;
                    target->dollTimer = 3.0f;
                }
                if (proj->type == PROJ_NOBARA_HAIRPIN) {
                    target->velY = -8.5f;
                    target->onGround = false;
                }
            }
            proj->active = false;
        }
    }
}

static void UpdateUltimate(Fighter* user, Fighter* target) {
    float dt = GetFrameTime();

    if (!user->ultActive) return;

    user->ultTimer -= dt;
    if (user->ultStartupTimer > 0.0f) {
        user->ultStartupTimer -= dt;
        if (user->ultStartupTimer < 0.0f) user->ultStartupTimer = 0.0f;
    }

    switch (user->activeUlt) {
        case ULT_HOLLOW_PURPLE:
            if (user->ultStartupTimer <= 0.0f) {
                user->ultHitbox.x += user->ultSpeed;
                user->ultHitbox.y = user->hitbox.y + 10.0f + sinf((float)GetTime() * 8.0f) * 4.0f;
                if (GetRandomValue(0, 1) == 0) {
                    ParticleSpawn(
                        (Vector2){ user->ultHitbox.x + user->ultHitbox.width * 0.5f, user->ultHitbox.y + user->ultHitbox.height * 0.5f },
                        (Vector2){ (float)GetRandomValue(-20, 20) / 10.0f, (float)GetRandomValue(-20, 20) / 10.0f },
                        (Color){185, 70, 255, 255}, 0.35f, 5.0f, PARTICLE_HOLLOW_PURPLE
                    );
                }
            }
            break;

        case ULT_FUGA:
            if (user->ultStartupTimer <= 0.0f) {
                user->ultHitbox.x += user->ultSpeed;
                user->ultHitbox.y = user->hitbox.y + 20.0f;
                ParticleSpawn(
                    (Vector2){ user->ultHitbox.x + user->ultHitbox.width * 0.5f, user->ultHitbox.y + user->ultHitbox.height * 0.5f },
                    (Vector2){ 0.0f, (float)GetRandomValue(-6, 6) / 10.0f },
                    (Color){255, 140, 30, 255}, 0.30f, 6.0f, PARTICLE_SPARK
                );
            }
            break;

        case ULT_PURE_LOVE_BEAM:
            if (user->ultStartupTimer <= 0.0f) {
                user->ultHitbox.x = (user->facingDir > 0) ? user->hitbox.x + user->hitbox.width : 0.0f;
                user->ultHitbox.y = user->hitbox.y + 10.0f;
                user->ultHitbox.width = (user->facingDir > 0) ? SCREEN_W - user->ultHitbox.x : user->hitbox.x;
                user->beamTickTimer += dt;
                if (user->beamTickTimer >= 0.12f) {
                    user->beamTickTimer = 0.0f;
                    if (CheckCollisionRecs(user->ultHitbox, target->hitbox)) {
                        ApplyCombatHit(user, target, user->ultDamage, 18.0f, 8, false, false, false, PARTICLE_CE_MOTE, 10);
                        user->beamTicksApplied++;
                    }
                }
            }
            break;

        case ULT_BLACK_FLASH:
        case ULT_HEAVENLY_ASSAULT:
        case ULT_MAHORAGA:
        case ULT_ULTIMATE_TACKLE:
            user->hitbox.x += user->ultSpeed;
            if (user->hitbox.x < 0.0f) user->hitbox.x = 0.0f;
            if (user->hitbox.x > SCREEN_W - user->hitbox.width) user->hitbox.x = SCREEN_W - user->hitbox.width;
            user->ultHitbox.x = user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width : -user->ultHitbox.width);
            user->ultHitbox.y = user->hitbox.y + 10.0f;
            break;

        case ULT_OVERTIME_SLASH:
        case ULT_MAXIMUM_RESONANCE:
            break;

        case ULT_NONE:
        default:
            break;
    }

    if (!user->ultHitApplied && user->ultStartupTimer <= 0.0f &&
        (user->activeUlt == ULT_HOLLOW_PURPLE || user->activeUlt == ULT_FUGA ||
         user->activeUlt == ULT_BLACK_FLASH || user->activeUlt == ULT_HEAVENLY_ASSAULT ||
         user->activeUlt == ULT_MAHORAGA || user->activeUlt == ULT_OVERTIME_SLASH ||
         user->activeUlt == ULT_MAXIMUM_RESONANCE || user->activeUlt == ULT_ULTIMATE_TACKLE) &&
        CheckCollisionRecs(user->ultHitbox, target->hurtbox)) {
        if (user->activeUlt == ULT_HOLLOW_PURPLE) {
            if (ApplyCombatHit(user, target, target->maxHP * 0.8f, 95.0f, 24, false, true, false, PARTICLE_HOLLOW_PURPLE, 60)) {
                user->ultHitApplied = true;
                user->ultActive = false;
                ShakeTrigger(18.0f, 0.6f);
            }
        } else if (user->activeUlt == ULT_FUGA) {
            TriggerFugaExplosion(user, target,
                                 (Vector2){ user->ultHitbox.x + user->ultHitbox.width * 0.5f, user->ultHitbox.y + user->ultHitbox.height * 0.5f });
            user->ultHitApplied = true;
            user->ultActive = false;
        } else if (user->activeUlt == ULT_MAXIMUM_RESONANCE) {
            ApplyCombatHit(user, target, user->ultDamage, 0.0f, 18, true, false, false, PARTICLE_BLACK_FLASH, 28);
            user->ultHitApplied = true;
            user->ultActive = false;
            target->dollMarked = false;
            target->dollTimer = 0.0f;
        } else if (user->activeUlt == ULT_OVERTIME_SLASH) {
            ApplyCombatHit(user, target, user->ultDamage, 90.0f, 24, false, true, false, PARTICLE_SPARK, 34);
            user->ultHitApplied = true;
            user->ultActive = false;
        } else if (user->activeUlt == ULT_ULTIMATE_TACKLE) {
            ApplyCombatHit(user, target, user->ultDamage, 110.0f, 30, false, false, false, PARTICLE_HIT_BURST, 30);
            user->ultHitApplied = true;
            user->ultActive = false;
        } else {
            if (ApplyCombatHit(user, target, user->ultDamage, 88.0f, 20, false, true, false, PARTICLE_BLACK_FLASH, 34)) {
                user->ultHitApplied = true;
                user->ultActive = false;
                user->blackFlashActive = (user->activeUlt == ULT_BLACK_FLASH);
            }
        }
    }

    if (user->activeUlt == ULT_PURE_LOVE_BEAM && user->beamTicksApplied >= MAX_BEAM_TICKS) {
        user->ultActive = false;
    }

    if (user->activeUlt == ULT_MAHORAGA && !user->ultHitApplied && user->ultTimer <= 0.0f) {
        user->mahoragaAdapted = true;
    }

    if (user->ultTimer <= 0.0f || user->ultHitbox.x < -260.0f || user->ultHitbox.x > SCREEN_W + 260.0f) {
        user->ultActive = false;
    }
}

static void UpdateAttackCooldown(Fighter* f) {
    f->passiveTimer += GetFrameTime();

    if (f->hitStunFrames > 0) {
        f->hitStunFrames--;
    }

    if (f->dodgeInvulFrames > 0) {
        f->dodgeInvulFrames--;
    }

    if (f->isAttacking) {
        f->attackFrames--;
        if (f->attackFrames <= 0) {
            f->isAttacking = false;
            f->attackLanded = false;
        }
    }

    if (f->specialAnimFrames > 0) {
        f->specialAnimFrames--;
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

    if (f->comboDisplayTimer > 0.0f) {
        f->comboDisplayTimer -= GetFrameTime();
        if (f->comboDisplayTimer <= 0.0f) {
            f->comboDisplayTimer = 0.0f;
            f->comboCounter = 0;
        }
    }

    if (f->infinityActive) {
        f->infinityDrainTimer += GetFrameTime();
        if (f->infinityDrainTimer >= 0.15f) {
            f->infinityDrainTimer = 0.0f;
            f->cursedEnergy -= INFINITY_CE_DRAIN * 0.15f;
            if (f->cursedEnergy <= 0.0f) {
                f->cursedEnergy = 0.0f;
                f->infinityActive = false;
            }
        }
    } else {
        f->infinityDrainTimer = 0.0f;
    }

    if (f->charData.id == CHAR_TODO) {
        if (f->boogieCharges < BOOGIE_MAX_CHARGES) {
            f->boogieChargeTimer += GetFrameTime();
            if (f->boogieChargeTimer >= BOOGIE_REGEN_TIME) {
                f->boogieChargeTimer = 0.0f;
                f->boogieCharges++;
            }
        }
    }

    if (f->charData.id == CHAR_NOBARA && f->dollTimer > 0.0f) {
        f->dollTimer -= GetFrameTime();
        if (f->dollTimer <= 0.0f) {
            f->dollTimer = 0.0f;
            f->dollMarked = false;
        }
    }

    if (f->ghostHP > f->hp) {
        f->ghostHP -= GetFrameTime() * (f->maxHP * 0.18f);
        if (f->ghostHP < f->hp) f->ghostHP = f->hp;
    } else {
        f->ghostHP = f->hp;
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
    bool actuallyStunned = (stunLock && !f->isHeavenlyRestricted) || f->hitStunFrames > 0;
    int playerId = isP1 ? 1 : 2;
    int leftKey   = isP1 ? KEY_A          : KEY_LEFT;
    int rightKey  = isP1 ? KEY_D          : KEY_RIGHT;
    int jumpKey   = isP1 ? KEY_W          : KEY_UP;
    int crouchKey = isP1 ? KEY_LEFT_SHIFT : KEY_RIGHT_SHIFT;
    int atkKey    = isP1 ? KEY_ONE        : KEY_KP_1;
    int rctKey    = isP1 ? KEY_TWO        : KEY_KP_2;
    int domainKey = isP1 ? KEY_THREE      : KEY_KP_3;
    int dodgeKey  = isP1 ? KEY_Q          : KEY_KP_0;
    int ab1Key    = isP1 ? KEY_E          : KEY_KP_4;
    int ab2Key    = isP1 ? KEY_R          : KEY_KP_5;
    int ab3Key    = isP1 ? KEY_F          : KEY_KP_6;
    int ultKey    = isP1 ? KEY_X          : KEY_KP_7;
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

    if (IsKeyPressed(ab1Key)) UseAbility1(f, opponent, isP1);
    if (IsKeyPressed(ab2Key)) UseAbility2(f, opponent, isP1);
    if (IsKeyPressed(ab3Key)) UseAbility3(f, isP1);

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
        f->dodgeInvulFrames = DODGE_INVUL_FRAMES;
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
                StartProjectileAttack(f, isP1);
            } else {
                DoMeleeHit(f, opponent);
            }
        }
    }

    if (f->isAttacking) {
        DoMeleeHit(f, opponent);
    }

    if (IsKeyPressed(rctKey) && !f->isHeavenlyRestricted && !f->ultActive && f->hp < f->maxHP - 0.5f) {
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

static void ProcessNetworkInput(Fighter* f, Fighter* opponent, bool stunLock, bool isP1,
                                const NetInput* input, const NetInput* prevInput, GameState* state,
                                int* domainCasterPlayer, float* domainTimer, DomainClashState* clash) {
    bool actuallyStunned = (stunLock && !f->isHeavenlyRestricted) || f->hitStunFrames > 0;
    int playerId = isP1 ? 1 : 2;
    float spd = f->speed;
    bool pressedDomain = input->domain && !prevInput->domain;
    bool pressedUlt = input->ult && !prevInput->ult;
    bool pressedAb1 = input->ability1 && !prevInput->ability1;
    bool pressedAb2 = input->ability2 && !prevInput->ability2;
    bool pressedAb3 = input->ability3 && !prevInput->ability3;
    bool pressedJump = input->jump && !prevInput->jump;
    bool pressedDodge = input->dodge && !prevInput->dodge;
    bool pressedAttack = input->attack && !prevInput->attack;
    bool pressedRCT = input->rct && !prevInput->rct;

    if (f->dodgeCooldown > 0) f->dodgeCooldown--;

    if (pressedDomain) {
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

    if (pressedAb1) UseAbility1(f, opponent, isP1);
    if (pressedAb2) UseAbility2(f, opponent, isP1);
    if (pressedAb3) UseAbility3(f, isP1);

    if (pressedUlt && *state == STATE_BATTLE) {
        StartUltimate(f, opponent);
    }

    f->isCrouching = input->crouch;
    if (f->isCrouching) {
        f->hitbox.height = 52.0f;
        spd *= 0.5f;
    } else {
        f->hitbox.height = 90.0f;
    }

    if (!f->isDodging && !f->ultActive) {
        if (input->left)  { f->hitbox.x -= spd; f->facingDir = -1; }
        if (input->right) { f->hitbox.x += spd; f->facingDir =  1; }
        if (pressedJump && f->onGround) {
            f->velY = JUMP_FORCE;
            f->onGround = false;
        }
    }

    if (pressedDodge && !f->isDodging && f->dodgeCooldown == 0 && f->onGround && !f->ultActive) {
        f->isDodging = true;
        f->dodgeFrames = DODGE_FRAMES;
        f->dodgeInvulFrames = DODGE_INVUL_FRAMES;
        f->dodgeCooldown = DODGE_COOLDOWN_FRAMES;
        f->dodgeVelX = (float)f->facingDir * DODGE_SPEED;
        ParticleSpawnBurst(
            (Vector2){ f->hitbox.x + f->hitbox.width * 0.5f, f->hitbox.y + f->hitbox.height * 0.5f },
            8, f->charData.ceColor, 2.5f, 0.2f, 3.0f, PARTICLE_CE_MOTE
        );
    }

    if (pressedAttack && !f->isAttacking && !f->ultActive) {
        bool forceMelee = f->isHeavenlyRestricted || f->charData.id == CHAR_YUJI || HorizontalGap(f, opponent) < 95.0f;
        f->isAttacking = true;
        f->attackFrames = 14;
        f->attackLanded = false;

        if (forceMelee) {
            DoMeleeHit(f, opponent);
        } else {
            float ceCost = CalcCECost(f, CE_ATTACK_COST);
            if (f->cursedEnergy >= ceCost) {
                f->cursedEnergy -= ceCost;
                StartProjectileAttack(f, isP1);
            } else {
                DoMeleeHit(f, opponent);
            }
        }
    }

    if (f->isAttacking) {
        DoMeleeHit(f, opponent);
    }

    if (pressedRCT && !f->isHeavenlyRestricted && !f->ultActive && f->hp < f->maxHP - 0.5f) {
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

static bool HasConfirmedRoster(const SelectState* p1sel, const SelectState* p2sel) {
    return p1sel->confirmed && p2sel->confirmed;
}

static void ResetBattleState(Fighter* p1, Fighter* p2, const SelectState* p1sel, const SelectState* p2sel,
                             int* domainCasterPlayer, float* domainTimer, DomainClashState* clash);

static void ResetRoundState(RoundState* round) {
    round->roundNumber = 1;
    round->p1Wins = 0;
    round->p2Wins = 0;
    round->roundTimer = ROUND_TIME_SECONDS;
    round->introTimer = ROUND_INTRO_TIME;
    round->endTimer = 0.0f;
    round->roundActive = false;
    round->matchPoint = false;
    snprintf(round->bannerText, sizeof(round->bannerText), "ROUND 1");
    snprintf(round->subText, sizeof(round->subText), "FIGHT");
}

static void StartNextRound(RoundState* round, Fighter* p1, Fighter* p2,
                           const SelectState* p1sel, const SelectState* p2sel,
                           int* domainCasterPlayer, float* domainTimer, DomainClashState* clash) {
    ResetBattleState(p1, p2, p1sel, p2sel, domainCasterPlayer, domainTimer, clash);
    p1->roundWins = round->p1Wins;
    p2->roundWins = round->p2Wins;
    round->roundTimer = ROUND_TIME_SECONDS;
    round->introTimer = ROUND_INTRO_TIME;
    round->endTimer = 0.0f;
    round->roundActive = false;
    snprintf(round->bannerText, sizeof(round->bannerText), "ROUND %d", round->roundNumber);
    snprintf(round->subText, sizeof(round->subText), "FIGHT");
}

static void ResetBattleState(Fighter* p1, Fighter* p2, const SelectState* p1sel, const SelectState* p2sel,
                             int* domainCasterPlayer, float* domainTimer, DomainClashState* clash) {
    *p1 = InitFighter(p1sel->selected, 160.0f, 1);
    *p2 = InitFighter(p2sel->selected, 740.0f, -1);
    UpdateFighterBoxes(p1);
    UpdateFighterBoxes(p2);
    *domainCasterPlayer = 0;
    *domainTimer = 0.0f;
    clash->active = false;
    clash->timer = 0.0f;
    gDomainAnnounce.active = false;
    gHitstopFrames = 0;
    ResetProjectiles();
    for (int i = 0; i < MAX_PARTICLES; i++) gParticles[i].active = false;
}

static void SimulateBattleFrame(Fighter* p1, Fighter* p2, GameState* state, int* domainCasterPlayer,
                                float* domainTimer, DomainClashState* clash,
                                const NetInput* p1Input, const NetInput* p1Prev,
                                const NetInput* p2Input, const NetInput* p2Prev,
                                RoundState* round) {
    float p1Regen = CE_REGEN_RATE * (p1->charData.traits.hasCopy ? 2.0f : 1.0f);
    float p2Regen = CE_REGEN_RATE * (p2->charData.traits.hasCopy ? 2.0f : 1.0f);
    bool canAct = true;

    if (round->introTimer > 0.0f) {
        round->introTimer -= GetFrameTime();
        if (round->introTimer <= 0.0f) {
            round->introTimer = 0.0f;
            round->roundActive = true;
        }
        canAct = false;
    }

    if (round->roundActive && (*state == STATE_BATTLE || *state == STATE_DOMAIN || *state == STATE_DOMAIN_CLASH)) {
        round->roundTimer -= GetFrameTime();
        if (round->roundTimer < 0.0f) round->roundTimer = 0.0f;
    }

    if (p1->charData.id == CHAR_NANAMI && round->roundTimer <= ROUND_TIME_SECONDS - OVERTIME_THRESHOLD) {
        p1->overtimeBuff = true;
    }
    if (p2->charData.id == CHAR_NANAMI && round->roundTimer <= ROUND_TIME_SECONDS - OVERTIME_THRESHOLD) {
        p2->overtimeBuff = true;
    }
    if (p1->overtimeBuff) p1->attackDamage = p1->charData.baseAttackDamage * 1.5f;
    if (p2->overtimeBuff) p2->attackDamage = p2->charData.baseAttackDamage * 1.5f;

    if (!clash->active && *state != STATE_DOMAIN) {
        p1->cursedEnergy += p1Regen;
        p2->cursedEnergy += p2Regen;
    }
    ClampFighter(p1);
    ClampFighter(p2);

    bool p1Stun = (*state == STATE_DOMAIN && *domainCasterPlayer == 2);
    bool p2Stun = (*state == STATE_DOMAIN && *domainCasterPlayer == 1);

    if (canAct) {
        if (p1Input != NULL && p1Prev != NULL && p2Input != NULL && p2Prev != NULL) {
            ProcessNetworkInput(p1, p2, p1Stun, true, p1Input, p1Prev, state, domainCasterPlayer, domainTimer, clash);
            ProcessNetworkInput(p2, p1, p2Stun, false, p2Input, p2Prev, state, domainCasterPlayer, domainTimer, clash);
        } else {
            ProcessInput(p1, p2, p1Stun, true, state, domainCasterPlayer, domainTimer, clash);
            ProcessInput(p2, p1, p2Stun, false, state, domainCasterPlayer, domainTimer, clash);
        }
    }

    ApplyPhysics(p1);
    ApplyPhysics(p2);
    ResolvePushboxes(p1, p2);
    UpdateProjectiles(p1, p2);
    UpdateUltimate(p1, p2);
    UpdateUltimate(p2, p1);
    UpdateAttackCooldown(p1);
    UpdateAttackCooldown(p2);

    if (p1->hitbox.x < p2->hitbox.x) {
        p1->facingDir = 1;
        p2->facingDir = -1;
    } else {
        p1->facingDir = -1;
        p2->facingDir = 1;
    }

    if (*state == STATE_DOMAIN) {
        Fighter* caster = (*domainCasterPlayer == 1) ? p1 : p2;
        Fighter* target = (*domainCasterPlayer == 1) ? p2 : p1;
        if (caster->charData.id == CHAR_MEGUMI && GetRandomValue(0, 100) < 16) {
            Fighter* victim = target;
            victim->hp -= 2.0f;
            victim->ghostHP = fmaxf(victim->ghostHP, victim->hp);
            ParticleSpawn((Vector2){ (float)GetRandomValue(80, SCREEN_W - 80), (float)GetRandomValue(120, (int)FLOOR_Y - 20) },
                          (Vector2){0.0f, -0.3f}, caster->charData.ceColor, 0.35f, 4.0f, PARTICLE_DOMAIN_SHARD);
        }
        UpdateDomain(caster, target, state, domainTimer, domainCasterPlayer);
    } else if (*state == STATE_DOMAIN_CLASH && clash->active) {
        UpdateDomainClash(state, clash, p1, p2, domainCasterPlayer);
    }

    if (p1->isAttacking && p2->isAttacking && p1->attackFrames == p2->attackFrames && HorizontalGap(p1, p2) < 90.0f) {
        p1->hitbox.x -= 12.0f;
        p2->hitbox.x += 12.0f;
        ParticleSpawnBurst((Vector2){ SCREEN_W * 0.5f, FLOOR_Y - 40.0f }, 28, GOLD, 5.0f, 0.35f, 4.0f, PARTICLE_CLASH_ARC);
        TriggerHitstop(HITSTOP_LIGHT);
    }

    CheckDomainLost(p1);
    CheckDomainLost(p2);
    DrawFighterEffects(p1);
    DrawFighterEffects(p2);
    ClampFighter(p1);
    ClampFighter(p2);

    if (round->roundActive && (p1->hp <= 0.0f || p2->hp <= 0.0f || round->roundTimer <= 0.0f)) {
        bool p1WinsRound = (p2->hp <= 0.0f) || (round->roundTimer <= 0.0f && p1->hp >= p2->hp);
        if (p1WinsRound) round->p1Wins++;
        else round->p2Wins++;
        p1->roundWins = round->p1Wins;
        p2->roundWins = round->p2Wins;
        round->endTimer = ROUND_END_TIME;
        round->roundActive = false;
        if (p1WinsRound) {
            snprintf(round->bannerText, sizeof(round->bannerText), p1->hp >= p1->maxHP - 0.5f ? "PERFECT!" : "KO!");
            snprintf(round->subText, sizeof(round->subText), "P1 TAKES ROUND %d", round->roundNumber);
        } else {
            snprintf(round->bannerText, sizeof(round->bannerText), p2->hp >= p2->maxHP - 0.5f ? "PERFECT!" : "KO!");
            snprintf(round->subText, sizeof(round->subText), "P2 TAKES ROUND %d", round->roundNumber);
        }
        *state = STATE_GAME_OVER;
        ParticleSpawnBurst(
            (Vector2){ SCREEN_W * 0.5f, SCREEN_H * 0.5f },
            80, GOLD, 6.0f, 1.5f, 5.0f, PARTICLE_SPARK
        );
        ShakeTrigger(15.0f, 0.6f);
    }
}

static void UpdateMenuVideo(MenuVideo* video) {
    if (video->count <= 0) return;
    video->timer += GetFrameTime();
    if (video->timer >= video->frameDuration) {
        video->timer = 0.0f;
        video->currentFrame = (video->currentFrame + 1) % video->count;
    }
}

static void DrawMenuBackground(const MenuVideo* video) {
    if (video->count > 0 && IsTextureValid(video->frames[video->currentFrame])) {
        DrawTexture(video->frames[video->currentFrame], 0, 0, WHITE);
    } else {
        DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, (Color){8, 6, 20, 255}, (Color){24, 10, 36, 255});
    }
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0, 0, 0, 105});
    for (int y = 0; y < SCREEN_H; y += 4) {
        DrawRectangle(0, y, SCREEN_W, 1, (Color){255, 255, 255, 9});
    }
}

static void DrawPixelPanel(Rectangle panel, Color fill, Color border) {
    DrawRectangleRec(panel, fill);
    DrawRectangleLinesEx(panel, 3.0f, border);
    DrawRectangleLinesEx((Rectangle){ panel.x + 6, panel.y + 6, panel.width - 12, panel.height - 12 }, 1.0f, ColorAlpha(WHITE, 0.18f));
}

static Rectangle MainMenuRowRect(int index) {
    return (Rectangle){ 294.0f, 196.0f + index * 44.0f, 372.0f, 32.0f };
}

static Rectangle MultiplayerRowRect(int index) {
    return (Rectangle){ 220.0f, 164.0f + index * 48.0f, 520.0f, 34.0f };
}

static Rectangle PauseMenuRowRect(int index) {
    return (Rectangle){ 334.0f, 214.0f + index * 34.0f, 290.0f, 26.0f };
}

static void DrawMainMenu(const MenuVideo* video, int cursor, const char* username, bool signInOpen, float signInSavedTimer) {
    static const char* items[] = { "LOCAL", "MULTIPLAYER", "CHARACTER SELECT", "INTRODUCE US", "EXIT" };
    int count = 5;
    DrawMenuBackground(video);

    DrawPixelPanel((Rectangle){ 170, 38, 620, 96 }, (Color){14, 10, 22, 225}, (Color){255, 214, 118, 255});
    RetroText("URUSAI MANIA", (Vector2){ 245, 56 }, 34.0f, 1.0f, (Color){240, 244, 255, 255});
    RetroText("FIGHTER GAME ENGINE", (Vector2){ 230, 94 }, 18.0f, 1.0f, (Color){255, 214, 118, 255});
    DrawPixelPanel((Rectangle){ 42, 44, 138, 40 }, (Color){12, 18, 30, 220}, (Color){255, 214, 118, 255});
    RetroText("SIGN IN", (Vector2){ 72, 56 }, 14.0f, 1.0f, (Color){255, 214, 118, 255});
    if (username[0] != '\0') {
        char greet[96];
        snprintf(greet, sizeof(greet), "Hi %s!", username);
        RetroText(greet, (Vector2){ 44, 98 }, 16.0f, 1.0f, (Color){255, 214, 118, 255});
    }

    DrawPixelPanel((Rectangle){ 240, 146, 480, 292 }, (Color){18, 14, 30, 225}, (Color){130, 185, 255, 255});

    for (int i = 0; i < count; i++) {
        Rectangle row = MainMenuRowRect(i);
        bool selected = (i == cursor);
        DrawRectangleRec(row, selected ? (Color){60, 90, 145, 220} : (Color){34, 28, 52, 205});
        DrawRectangleLinesEx(row, 2.0f, selected ? (Color){255, 230, 130, 255} : (Color){110, 100, 150, 220});
        Vector2 size = RetroMeasure(items[i], 16.0f, 1.0f);
        RetroText(items[i],
                  (Vector2){ row.x + row.width * 0.5f - size.x * 0.5f, row.y + 6.0f },
                  16.0f, 1.0f, selected ? WHITE : (Color){210, 210, 230, 240});
    }

    RetroText("UP / DOWN OR W / S TO MOVE", (Vector2){ 284, 430 }, 12.0f, 1.0f, (Color){220, 220, 235, 240});
    RetroText("ENTER / SPACE TO SELECT", (Vector2){ 308, 450 }, 12.0f, 1.0f, (Color){220, 220, 235, 240});

    if (signInOpen) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, ColorAlpha(BLACK, 0.55f));
        DrawPixelPanel((Rectangle){ 258, 162, 444, 152 }, (Color){18, 14, 30, 240}, (Color){255, 214, 118, 255});
        RetroText("ENTER USERNAME", (Vector2){ 356, 190 }, 18.0f, 1.0f, (Color){255, 214, 118, 255});
        DrawRectangleRec((Rectangle){ 300, 234, 360, 28 }, (Color){12, 20, 34, 230});
        DrawRectangleLinesEx((Rectangle){ 300, 234, 360, 28 }, 2.0f, (Color){255, 214, 118, 255});
        RetroText(username[0] ? username : "type here", (Vector2){ 314, 241 }, 14.0f, 1.0f, WHITE);
        RetroText("ENTER TO SAVE  |  ESC TO CLOSE", (Vector2){ 318, 278 }, 11.0f, 1.0f, (Color){220, 230, 255, 240});
        if (signInSavedTimer > 0.0f) {
            RetroText("SIGNED IN", (Vector2){ 450, 206 }, 11.0f, 1.0f, (Color){255, 230, 120, 255});
        }
    }
}

static void DrawMultiplayerMenu(const MenuVideo* video, const MultiplayerMenuState* mpMenu) {
    static const char* items[] = { "DIRECT 1V1 USERNAME", "SEND DIRECT 1V1", "GLOBAL MATCHMAKING", "BACK" };
    DrawMenuBackground(video);

    DrawPixelPanel((Rectangle){ 148, 56, 664, 426 }, (Color){16, 12, 26, 228}, (Color){110, 220, 255, 255});
    RetroText("MULTIPLAYER DOMAIN", (Vector2){ 240, 82 }, 28.0f, 1.0f, WHITE);
    RetroText("USERNAME CHALLENGES + GLOBAL QUEUE", (Vector2){ 218, 118 }, 14.0f, 1.0f, (Color){120, 220, 255, 255});

    if (!mpMenu->globalListOpen) {
        for (int i = 0; i < 4; i++) {
            Rectangle row = MultiplayerRowRect(i);
            bool selected = (i == mpMenu->cursor);
            DrawRectangleRec(row, selected ? (Color){42, 110, 156, 220} : (Color){34, 28, 52, 205});
            DrawRectangleLinesEx(row, 2.0f, selected ? (Color){255, 230, 130, 255} : (Color){110, 100, 150, 220});
            if (i == 0) {
                RetroText(items[i], (Vector2){ row.x + 12.0f, row.y + 8.0f }, 12.0f, 1.0f, (Color){255, 214, 118, 255});
                RetroText(mpMenu->targetUsername[0] ? mpMenu->targetUsername : "type opponent username",
                          (Vector2){ row.x + 240.0f, row.y + 8.0f }, 12.0f, 1.0f, selected ? WHITE : (Color){210, 220, 230, 240});
            } else {
                Vector2 size = RetroMeasure(items[i], 14.0f, 1.0f);
                RetroText(items[i], (Vector2){ row.x + row.width * 0.5f - size.x * 0.5f, row.y + 8.0f },
                          14.0f, 1.0f, selected ? WHITE : (Color){210, 220, 230, 240});
            }
        }
    } else {
        DrawPixelPanel((Rectangle){ 198, 156, 564, 232 }, (Color){12, 16, 32, 220}, (Color){255, 214, 118, 220});
        RetroText("GLOBAL MATCHMAKING", (Vector2){ 348, 174 }, 18.0f, 1.0f, (Color){255, 214, 118, 255});
        RetroText("SELECT A WAITING PLAYER FOR 1V1", (Vector2){ 300, 204 }, 12.0f, 1.0f, (Color){120, 220, 255, 240});
        if (mpMenu->playerList.count == 0) {
            RetroText("No waiting players online right now.", (Vector2){ 286, 264 }, 14.0f, 1.0f, (Color){220, 230, 255, 220});
        } else {
            for (int i = 0; i < mpMenu->playerList.count && i < 7; i++) {
                const NetWaitingPlayerEntry* entry = &mpMenu->playerList.players[i];
                Rectangle row = { 246, 236.0f + i * 24.0f, 468, 20 };
                bool selected = (i == mpMenu->playerListCursor);
                DrawRectangleRec(row, selected ? (Color){58, 96, 138, 220} : (Color){22, 28, 44, 205});
                DrawRectangleLinesEx(row, 1.0f, selected ? (Color){255, 230, 130, 255} : (Color){110, 100, 150, 180});
                RetroText(entry->username, (Vector2){ row.x + 8, row.y + 4 }, 10.0f, 1.0f, WHITE);
            }
        }
        DrawRectangleRec((Rectangle){ 368, 356, 224, 28 }, (Color){28, 34, 52, 220});
        DrawRectangleLinesEx((Rectangle){ 368, 356, 224, 28 }, 2.0f, (Color){255, 214, 118, 255});
        RetroText("BACK", (Vector2){ 453, 364 }, 12.0f, 1.0f, WHITE);
        RetroText("ENTER TO CHALLENGE  |  ESC TO GO BACK", (Vector2){ 278, 354 }, 11.0f, 1.0f, (Color){220, 230, 255, 235});
    }

    DrawPixelPanel((Rectangle){ 192, 396, 576, 62 }, (Color){12, 16, 32, 220}, (Color){255, 214, 118, 220});
    RetroText(mpMenu->statusText, (Vector2){ 212, 414 }, 12.0f, 1.0f, (Color){220, 230, 255, 240});
    RetroText("SERVER: URUSAI RELAY", (Vector2){ 212, 436 }, 11.0f, 1.0f, (Color){120, 220, 255, 240});

    if (mpMenu->hasIncomingChallenge) {
        DrawPixelPanel((Rectangle){ 244, 252, 472, 88 }, (Color){24, 18, 36, 240}, (Color){255, 96, 96, 255});
        char incoming[96];
        snprintf(incoming, sizeof(incoming), "%s WANTS A 1V1", mpMenu->incomingChallenge);
        RetroText(incoming, (Vector2){ 276, 274 }, 16.0f, 1.0f, WHITE);
        RetroText("PRESS Y TO ACCEPT OR N TO DECLINE", (Vector2){ 268, 304 }, 12.0f, 1.0f, (Color){255, 214, 118, 255});
    }
}

static void DrawAboutScreen(const MenuVideo* video) {
    DrawMenuBackground(video);
    DrawPixelPanel((Rectangle){ 140, 82, 680, 360 }, (Color){18, 14, 30, 225}, (Color){255, 215, 120, 255});
    RetroText("INTRODUCE US", (Vector2){ 336, 108 }, 24.0f, 1.0f, WHITE);
    RetroText("URUSAI MANIA is a custom 2D C + Raylib arena brawler.", (Vector2){ 174, 164 }, 15.0f, 1.0f, (Color){220, 230, 255, 240});
    RetroText("This build adds a sharper retro frontend, pause flow,", (Vector2){ 174, 198 }, 15.0f, 1.0f, (Color){220, 230, 255, 240});
    RetroText("looped opening theme, animated menu background,", (Vector2){ 174, 224 }, 15.0f, 1.0f, (Color){220, 230, 255, 240});
    RetroText("and signature domain / ultimate spectacle.", (Vector2){ 174, 250 }, 15.0f, 1.0f, (Color){220, 230, 255, 240});
    RetroText("CONTROLS: X / NUM4 = ULT, ESC = PAUSE / BACK", (Vector2){ 176, 304 }, 14.0f, 1.0f, (Color){255, 222, 140, 240});
    RetroText("PRESS ENTER OR ESC TO RETURN", (Vector2){ 258, 370 }, 16.0f, 1.0f, (Color){255, 255, 255, 240});
}

static void DrawPauseMenu(const MenuVideo* video, int cursor) {
    static const char* items[] = { "RESUME", "CHARACTER SELECT", "MAIN MENU", "EXIT GAME" };
    DrawMenuBackground(video);
    DrawPixelPanel((Rectangle){ 300, 140, 360, 230 }, (Color){16, 12, 26, 230}, (Color){255, 205, 120, 255});
    RetroText("PAUSED", (Vector2){ 408, 166 }, 26.0f, 1.0f, WHITE);
    for (int i = 0; i < 4; i++) {
        Rectangle row = PauseMenuRowRect(i);
        bool selected = (i == cursor);
        DrawRectangleRec(row, selected ? (Color){90, 80, 150, 220} : (Color){32, 28, 48, 215});
        DrawRectangleLinesEx(row, 2.0f, selected ? (Color){255, 230, 130, 255} : (Color){110, 100, 150, 220});
        Vector2 size = RetroMeasure(items[i], 14.0f, 1.0f);
        RetroText(items[i], (Vector2){ row.x + row.width * 0.5f - size.x * 0.5f, row.y + 5.0f }, 14.0f, 1.0f,
                  selected ? WHITE : (Color){215, 215, 230, 240});
    }
    RetroText("ESC RESUMES INSTANTLY", (Vector2){ 356, 336 }, 12.0f, 1.0f, (Color){220, 220, 235, 235});
}

static void DisconnectMultiplayer(MatchMode* matchMode, MultiplayerMenuState* mpMenu) {
    NetDisconnectOnly();
    *matchMode = MATCH_MODE_LOCAL;
    mpMenu->connectedToLobby = false;
    mpMenu->authenticated = false;
    mpMenu->authRequested = false;
    mpMenu->waitingForMatch = false;
    mpMenu->hasIncomingChallenge = false;
    mpMenu->inOnlineMatch = false;
    mpMenu->globalListOpen = false;
    mpMenu->incomingChallenge[0] = '\0';
    mpMenu->matchedOpponent[0] = '\0';
    mpMenu->localPlayerIndex = 0;
    snprintf(mpMenu->statusText, sizeof(mpMenu->statusText), "Connection closed.");
}

static void SendRosterState(const SelectState* sel) {
    NetRosterState packet = PackRosterState(sel);
    NetSendRosterState(&packet);
}

static void UpdateTextField(char* buffer, int maxLen, bool allowDots) {
    int key = GetCharPressed();
    while (key > 0) {
        int len = (int)strlen(buffer);
        bool valid = (key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z') ||
                     (key >= 'a' && key <= 'z') || key == '_' || key == '-';
        if (allowDots) valid = valid || key == '.';
        if (len < maxLen - 1 && valid) {
            buffer[len] = (char)key;
            buffer[len + 1] = '\0';
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = (int)strlen(buffer);
        if (len > 0) buffer[len - 1] = '\0';
    }
}

static void LeaveOnlineMatch(MatchMode* matchMode, MultiplayerMenuState* mpMenu) {
    if (mpMenu->inOnlineMatch && gNetConnected) {
        NetSendMatchLeave();
    } else if (mpMenu->globalListOpen && gNetConnected) {
        NetLeaveQueue();
    }
    DisconnectMultiplayer(matchMode, mpMenu);
}

static void UpdateFightVideo(FightVideo* video) {
    if (video->count <= 0) return;
    video->timer += GetFrameTime();
    if (video->timer >= video->frameDuration) {
        video->timer = 0.0f;
        video->currentFrame = (video->currentFrame + 1) % video->count;
    }
}

static void DrawFightVideoBackground(const FightVideo* video, bool domainActive, CharacterID casterId) {
    if (video->count > 0 && IsTextureValid(video->frames[video->currentFrame])) {
        Rectangle src = {0, 0, (float)video->frames[video->currentFrame].width, (float)video->frames[video->currentFrame].height};
        Rectangle dst = {0, 0, SCREEN_W, SCREEN_H};
        DrawTexturePro(video->frames[video->currentFrame], src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, (Color){12, 10, 22, 255}, (Color){25, 16, 36, 255});
    }

    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0, 0, 0, 24});

    if (domainActive) {
        Color tint = (Color){120, 60, 180, 34};
        if (casterId == CHAR_GOJO) tint = (Color){60, 100, 255, 40};
        else if (casterId == CHAR_SUKUNA) tint = (Color){255, 50, 60, 38};
        else if (casterId == CHAR_YUTA) tint = (Color){220, 90, 255, 36};
        DrawRectangleGradientH(0, 0, SCREEN_W, SCREEN_H, ColorAlpha(tint, 0.04f), tint);
        DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, ColorAlpha(BLACK, 0.0f), ColorAlpha(tint, 0.06f));
        DrawRectangleGradientV(0, 0, SCREEN_W, 120, ColorAlpha(BLACK, 0.55f), ColorAlpha(BLACK, 0.0f));
        DrawRectangleGradientV(0, SCREEN_H - 120, SCREEN_W, 120, ColorAlpha(BLACK, 0.0f), ColorAlpha(BLACK, 0.55f));
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    InitWindow(SCREEN_W, SCREEN_H, "URUSAI MANIA - Cursed Clash");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();
    NetInit();
    LoadRelayConfig();

    for (int i = 0; i < MAX_PARTICLES; i++) gParticles[i].active = false;

    Music bgm = {0};
    bool musicLoaded = false;
    MenuVideo menuVideo = {0};
    FightVideo fightVideo = {0};
    FrontendState frontend = {0};
    MultiplayerMenuState mpMenu = {0};
    Texture2D gojoPortrait = {0};
    bool gojoPortraitLoaded = false;
    frontend.cursor = 0;
    frontend.pauseCursor = 0;
    frontend.launchBattleAfterSelect = false;
    frontend.pausedFromState = STATE_BATTLE;
    frontend.signInOpen = false;
    frontend.signInSavedTimer = 0.0f;
    mpMenu.cursor = 0;
    mpMenu.activeField = 0;
    mpMenu.targetUsername[0] = '\0';
    mpMenu.incomingChallenge[0] = '\0';
    mpMenu.matchedOpponent[0] = '\0';
    mpMenu.localPlayerIndex = 0;
    mpMenu.connectedToLobby = false;
    mpMenu.authenticated = false;
    mpMenu.authRequested = false;
    mpMenu.waitingForMatch = false;
    mpMenu.inOnlineMatch = false;
    mpMenu.globalListOpen = false;
    snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Connect to multiplayer to reach the relay.");

    FilePathList menuPaths = LoadDirectoryFilesEx("assets/menu_frames", ".png", false);
    if (menuPaths.count > 0) {
        menuVideo.count = (menuPaths.count < MAX_MENU_FRAMES) ? menuPaths.count : MAX_MENU_FRAMES;
        menuVideo.frameDuration = 1.0f / 12.0f;
        for (int i = 0; i < menuVideo.count; i++) {
            menuVideo.frames[i] = LoadTexture(menuPaths.paths[i]);
        }
    }
    UnloadDirectoryFiles(menuPaths);

    if (FileExists("assets/menu_theme.mp3")) {
        bgm = LoadMusicStream("assets/menu_theme.mp3");
        SetMusicVolume(bgm, 0.55f);
        PlayMusicStream(bgm);
        musicLoaded = true;
    }

    FilePathList fightPaths = LoadDirectoryFilesEx("assets/fight_frames", ".png", false);
    if (fightPaths.count > 0) {
        fightVideo.count = (fightPaths.count < MAX_FIGHT_FRAMES) ? fightPaths.count : MAX_FIGHT_FRAMES;
        fightVideo.frameDuration = 1.0f / 8.0f;
        for (int i = 0; i < fightVideo.count; i++) {
            fightVideo.frames[i] = LoadTexture(fightPaths.paths[i]);
        }
    }
    UnloadDirectoryFiles(fightPaths);

    if (FileExists("assets/fonts/Retro Gaming.ttf")) {
        gRetroFont = LoadFontEx("assets/fonts/Retro Gaming.ttf", 32, 0, 0);
        gRetroFontLoaded = true;
        SetTextureFilter(gRetroFont.texture, TEXTURE_FILTER_POINT);
    }
    SetUIFont(gRetroFont, gRetroFontLoaded);

    if (FileExists("assets/gojo.png")) {
        gojoPortrait = LoadTexture("assets/gojo.png");
        SetTextureFilter(gojoPortrait, TEXTURE_FILTER_POINT);
        gojoPortraitLoaded = true;
    }
    SetGojoPortrait(gojoPortrait, gojoPortraitLoaded);
    LoadSavedUsername(mpMenu.username, sizeof(mpMenu.username));

    GameState state = STATE_MAIN_MENU;
    MatchMode matchMode = MATCH_MODE_LOCAL;
    SelectState p1sel = { 0, CHAR_SUKUNA, false };
    SelectState p2sel = { 4, CHAR_YUJI, false };
    p1sel.selected = CHAR_SUKUNA;
    p2sel.selected = CHAR_YUJI;
    Fighter p1;
    Fighter p2;
    memset(&p1, 0, sizeof(Fighter));
    memset(&p2, 0, sizeof(Fighter));

    int domainCasterPlayer = 0;
    float domainTimer = 0.0f;
    DomainClashState clash = {0};
    RoundState round = {0};
    ResetRoundState(&round);
    NetInput remoteInput = {0};
    NetInput remotePrevInput = {0};
    NetInput localNetInput = {0};
    NetInput localNetPrevInput = {0};

    while (!WindowShouldClose()) {
        if (musicLoaded) {
            UpdateMusicStream(bgm);
            if (!IsMusicStreamPlaying(bgm) || GetMusicTimePlayed(bgm) >= GetMusicTimeLength(bgm) - 0.05f) {
                StopMusicStream(bgm);
                PlayMusicStream(bgm);
            }
        }

        if (state == STATE_MAIN_MENU || state == STATE_ABOUT || state == STATE_MULTIPLAYER || state == STATE_PAUSE) {
            UpdateMenuVideo(&menuVideo);
        }
        if (state == STATE_BATTLE || state == STATE_DOMAIN || state == STATE_DOMAIN_CLASH || state == STATE_GAME_OVER) {
            UpdateFightVideo(&fightVideo);
        }

        ShakeUpdate();
        ParticleUpdate();
        AnnounceUpdate();

        if (frontend.signInSavedTimer > 0.0f) {
            frontend.signInSavedTimer -= GetFrameTime();
            if (frontend.signInSavedTimer <= 0.0f) {
                frontend.signInSavedTimer = 0.0f;
                frontend.signInOpen = false;
            }
        }

        if (matchMode == MATCH_MODE_ONLINE || state == STATE_MULTIPLAYER) {
            bool remoteMatchClosed = false;
            unsigned char netBuffer[2048] = {0};
            NetMessageType msgType = NET_MSG_NONE;
            size_t payloadSize = 0;

            while (NetPollMessage(&msgType, netBuffer, sizeof(netBuffer), &payloadSize)) {
                if (msgType == NET_MSG_AUTH_RESULT && payloadSize == sizeof(NetAuthResultMessage)) {
                    const NetAuthResultMessage* result = (const NetAuthResultMessage*)netBuffer;
                    mpMenu.authenticated = result->success != 0;
                    snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "%s", result->message);
                } else if (msgType == NET_MSG_STATUS && payloadSize == sizeof(NetStatusMessage)) {
                    const NetStatusMessage* statusMsg = (const NetStatusMessage*)netBuffer;
                    snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "%s", statusMsg->message);
                } else if (msgType == NET_MSG_PLAYER_LIST && payloadSize == sizeof(NetPlayerListMessage)) {
                    memcpy(&mpMenu.playerList, netBuffer, sizeof(NetPlayerListMessage));
                    if (mpMenu.playerList.count > 0) {
                        if (mpMenu.playerListCursor >= mpMenu.playerList.count) mpMenu.playerListCursor = mpMenu.playerList.count - 1;
                        if (mpMenu.playerListCursor < 0) mpMenu.playerListCursor = 0;
                    } else {
                        mpMenu.playerListCursor = 0;
                    }
                } else if (msgType == NET_MSG_DIRECT_CHALLENGE_NOTIFY && payloadSize == sizeof(NetDirectChallengeNotifyMessage)) {
                    const NetDirectChallengeNotifyMessage* notify = (const NetDirectChallengeNotifyMessage*)netBuffer;
                    snprintf(mpMenu.incomingChallenge, sizeof(mpMenu.incomingChallenge), "%s", notify->fromUsername);
                    mpMenu.hasIncomingChallenge = true;
                    snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "%s wants to 1v1 you.", notify->fromUsername);
                } else if (msgType == NET_MSG_MATCH_FOUND && payloadSize == sizeof(NetMatchFoundMessage)) {
                    const NetMatchFoundMessage* found = (const NetMatchFoundMessage*)netBuffer;
                    mpMenu.waitingForMatch = false;
                    mpMenu.hasIncomingChallenge = false;
                    mpMenu.globalListOpen = false;
                    mpMenu.inOnlineMatch = true;
                    mpMenu.localPlayerIndex = found->localPlayerIndex;
                    snprintf(mpMenu.matchedOpponent, sizeof(mpMenu.matchedOpponent), "%s", found->opponentUsername);
                    snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Match found vs %s. Choose your sorcerer.", found->opponentUsername);
                    p1sel = (SelectState){ 0, CHAR_SUKUNA, false };
                    p2sel = (SelectState){ 4, CHAR_YUJI, false };
                    memset(&remoteInput, 0, sizeof(remoteInput));
                    memset(&remotePrevInput, 0, sizeof(remotePrevInput));
                    memset(&localNetInput, 0, sizeof(localNetInput));
                    memset(&localNetPrevInput, 0, sizeof(localNetPrevInput));
                    frontend.launchBattleAfterSelect = true;
                    state = STATE_CHAR_SELECT;
                } else if (msgType == NET_MSG_MATCH_LEAVE) {
                    remoteMatchClosed = true;
                    snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Opponent left the match.");
                } else if (msgType == NET_MSG_ROSTER && payloadSize == sizeof(NetRosterState)) {
                    SelectState* remoteSel = (mpMenu.localPlayerIndex == 0) ? &p2sel : &p1sel;
                    UnpackRosterState(remoteSel, (const NetRosterState*)netBuffer);
                } else if (msgType == NET_MSG_INPUT && payloadSize == sizeof(NetInput)) {
                    memcpy(&remoteInput, netBuffer, sizeof(NetInput));
                }
            }

            if ((state == STATE_CHAR_SELECT || state == STATE_BATTLE ||
                 state == STATE_DOMAIN || state == STATE_DOMAIN_CLASH || state == STATE_GAME_OVER ||
                 (state == STATE_MULTIPLAYER && mpMenu.authRequested)) &&
                mpMenu.connectedToLobby && !gNetConnected) {
                snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Relay server disconnected.");
                DisconnectMultiplayer(&matchMode, &mpMenu);
                p1sel.confirmed = false;
                p2sel.confirmed = false;
                state = STATE_MAIN_MENU;
            }

            if (remoteMatchClosed) {
                DisconnectMultiplayer(&matchMode, &mpMenu);
                p1sel.confirmed = false;
                p2sel.confirmed = false;
                state = STATE_MAIN_MENU;
            }

            if (state == STATE_MULTIPLAYER && mpMenu.connectedToLobby && gNetConnected && mpMenu.username[0] != '\0' && !mpMenu.authenticated && !mpMenu.authRequested) {
                NetSendAuth(mpMenu.username);
                mpMenu.authRequested = true;
                snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Signing in as %s...", mpMenu.username);
            }
        }

        switch (state) {
            case STATE_MAIN_MENU: {
                int activateIndex = -1;
                Rectangle signInButton = { 42, 44, 138, 40 };
                Vector2 mousePos = GetMousePosition();
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), signInButton)) {
                    frontend.signInOpen = true;
                    frontend.signInSavedTimer = 0.0f;
                }

                if (frontend.signInOpen) {
                    UpdateTextField(mpMenu.username, sizeof(mpMenu.username), false);
                    if (IsKeyPressed(KEY_ENTER)) {
                        if (mpMenu.username[0] != '\0') {
                            SaveUsername(mpMenu.username);
                            frontend.signInSavedTimer = 1.0f;
                        }
                    }
                    if (IsKeyPressed(KEY_ESCAPE)) {
                        frontend.signInOpen = false;
                        frontend.signInSavedTimer = 0.0f;
                    }
                    break;
                }

                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) frontend.cursor = (frontend.cursor + 1) % 5;
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) frontend.cursor = (frontend.cursor + 4) % 5;
                for (int i = 0; i < 5; i++) {
                    if (CheckCollisionPointRec(mousePos, MainMenuRowRect(i))) {
                        frontend.cursor = i;
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            activateIndex = i;
                        }
                    }
                }

                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                    activateIndex = frontend.cursor;
                }

                if (activateIndex >= 0) {
                    switch (activateIndex) {
                        case 0:
                            matchMode = MATCH_MODE_LOCAL;
                            if (HasConfirmedRoster(&p1sel, &p2sel)) {
                                ResetRoundState(&round);
                                StartNextRound(&round, &p1, &p2, &p1sel, &p2sel, &domainCasterPlayer, &domainTimer, &clash);
                                state = STATE_BATTLE;
                            } else {
                                frontend.launchBattleAfterSelect = true;
                                state = STATE_CHAR_SELECT;
                            }
                            break;
                        case 1:
                            if (mpMenu.connectedToLobby || gNetConnected) {
                                DisconnectMultiplayer(&matchMode, &mpMenu);
                            }
                            matchMode = MATCH_MODE_ONLINE;
                            p1sel.confirmed = false;
                            p2sel.confirmed = false;
                            mpMenu.cursor = 0;
                            mpMenu.authenticated = false;
                            mpMenu.authRequested = false;
                            mpMenu.waitingForMatch = false;
                            mpMenu.hasIncomingChallenge = false;
                            mpMenu.inOnlineMatch = false;
                            mpMenu.globalListOpen = false;
                            mpMenu.playerList.count = 0;
                            if (NetConnectRelay(gRelayServerIP, NET_RELAY_PORT)) {
                                mpMenu.connectedToLobby = true;
                                snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Connecting to the relay server...");
                            } else {
                                mpMenu.connectedToLobby = false;
                                snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Could not reach the relay server.");
                            }
                            state = STATE_MULTIPLAYER;
                            break;
                        case 2:
                            matchMode = MATCH_MODE_LOCAL;
                            frontend.launchBattleAfterSelect = false;
                            state = STATE_CHAR_SELECT;
                            break;
                        case 3:
                            state = STATE_ABOUT;
                            break;
                        case 4:
                            CloseWindow();
                            break;
                    }
                }
                break;
            }

            case STATE_ABOUT:
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE) ||
                    IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    state = STATE_MAIN_MENU;
                }
                break;

            case STATE_MULTIPLAYER:
                {
                    int activateIndex = -1;
                    Vector2 mousePos = GetMousePosition();
                    Rectangle globalBackRect = { 368, 356, 224, 28 };

                    if (IsKeyPressed(KEY_ESCAPE)) {
                        if (mpMenu.globalListOpen) {
                            if (gNetConnected) NetLeaveQueue();
                            mpMenu.globalListOpen = false;
                            mpMenu.waitingForMatch = false;
                            snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Back to multiplayer menu.");
                            break;
                        }
                        LeaveOnlineMatch(&matchMode, &mpMenu);
                        state = STATE_MAIN_MENU;
                        break;
                    }

                    if (mpMenu.hasIncomingChallenge) {
                        if (IsKeyPressed(KEY_Y)) {
                            NetSendChallengeResponse(mpMenu.incomingChallenge, true);
                            mpMenu.hasIncomingChallenge = false;
                            mpMenu.waitingForMatch = true;
                            snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Accepting %s's challenge...", mpMenu.incomingChallenge);
                        } else if (IsKeyPressed(KEY_N)) {
                            NetSendChallengeResponse(mpMenu.incomingChallenge, false);
                            mpMenu.hasIncomingChallenge = false;
                            snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Challenge declined.");
                        }
                    }

                    if (mpMenu.globalListOpen) {
                        if (mpMenu.playerList.count > 0) {
                            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) mpMenu.playerListCursor = (mpMenu.playerListCursor + mpMenu.playerList.count - 1) % mpMenu.playerList.count;
                            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) mpMenu.playerListCursor = (mpMenu.playerListCursor + 1) % mpMenu.playerList.count;
                            for (int i = 0; i < mpMenu.playerList.count && i < 7; i++) {
                                Rectangle row = { 246.0f, 236.0f + i * 24.0f, 468.0f, 20.0f };
                                if (CheckCollisionPointRec(mousePos, row)) {
                                    mpMenu.playerListCursor = i;
                                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                        activateIndex = i;
                                    }
                                }
                            }
                        }
                        if (CheckCollisionPointRec(mousePos, globalBackRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            if (gNetConnected) NetLeaveQueue();
                            mpMenu.globalListOpen = false;
                            mpMenu.waitingForMatch = false;
                            snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Back to multiplayer menu.");
                            break;
                        }
                        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) && mpMenu.playerList.count > 0) {
                            activateIndex = mpMenu.playerListCursor;
                        }
                        if (activateIndex >= 0 && activateIndex < mpMenu.playerList.count) {
                            const NetWaitingPlayerEntry* target = &mpMenu.playerList.players[activateIndex];
                            snprintf(mpMenu.targetUsername, sizeof(mpMenu.targetUsername), "%s", target->username);
                            if (mpMenu.username[0] == '\0') {
                                snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Use Sign In on the main menu first.");
                            } else if (!mpMenu.authenticated) {
                                snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Wait until your relay sign-in completes.");
                            } else {
                                NetSendDirectChallenge(target->username);
                                snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Sending global match request to %s...", target->username);
                            }
                        }
                        break;
                    }

                    if (mpMenu.activeField == 1) {
                        UpdateTextField(mpMenu.targetUsername, sizeof(mpMenu.targetUsername), false);
                        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
                            mpMenu.activeField = 0;
                        }
                        break;
                    }

                    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) mpMenu.cursor = (mpMenu.cursor + 1) % 4;
                    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) mpMenu.cursor = (mpMenu.cursor + 3) % 4;
                    for (int i = 0; i < 4; i++) {
                        if (CheckCollisionPointRec(mousePos, MultiplayerRowRect(i))) {
                            mpMenu.cursor = i;
                            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                activateIndex = i;
                            }
                        }
                    }
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                        activateIndex = mpMenu.cursor;
                    }

                    if (activateIndex == 0) {
                        mpMenu.activeField = 1;
                    } else if (activateIndex == 1) {
                        if (mpMenu.username[0] == '\0') {
                            snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Use Sign In on the main menu first.");
                        } else if (mpMenu.targetUsername[0] == '\0') {
                            snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Enter the opponent username to challenge.");
                        } else if (!mpMenu.authenticated) {
                            snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Wait until your relay sign-in completes.");
                        } else {
                            NetSendDirectChallenge(mpMenu.targetUsername);
                            snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Sending direct 1v1 to %s...", mpMenu.targetUsername);
                        }
                    } else if (activateIndex == 2) {
                        mpMenu.globalListOpen = true;
                        mpMenu.waitingForMatch = true;
                        if (gNetConnected) {
                            NetJoinQueue();
                            NetRequestPlayerList();
                        }
                        snprintf(mpMenu.statusText, sizeof(mpMenu.statusText), "Choose a waiting player for global matchmaking.");
                    } else if (activateIndex == 3) {
                        LeaveOnlineMatch(&matchMode, &mpMenu);
                        state = STATE_MAIN_MENU;
                    }
                }
                break;

            case STATE_CHAR_SELECT:
                if (IsKeyPressed(KEY_ESCAPE)) {
                    p1sel.confirmed = false;
                    p2sel.confirmed = false;
                    if (matchMode == MATCH_MODE_ONLINE) {
                        LeaveOnlineMatch(&matchMode, &mpMenu);
                    }
                    state = STATE_MAIN_MENU;
                    break;
                }
                if (matchMode == MATCH_MODE_ONLINE) {
                    SelectState* localSel = (mpMenu.localPlayerIndex == 0) ? &p1sel : &p2sel;
                    NetRosterState beforeSend = PackRosterState(localSel);
                    if (IsKeyPressed(KEY_A) && localSel->cursor > 0 && !localSel->confirmed) localSel->cursor--;
                    if (IsKeyPressed(KEY_D) && localSel->cursor < CHAR_COUNT - 1 && !localSel->confirmed) localSel->cursor++;
                    if (IsKeyPressed(KEY_SPACE) && !localSel->confirmed) {
                        localSel->selected = (CharacterID)localSel->cursor;
                        localSel->confirmed = true;
                    }
                    if (localSel->confirmed) localSel->selected = (CharacterID)localSel->cursor;
                    NetRosterState afterSend = PackRosterState(localSel);
                    if (memcmp(&beforeSend, &afterSend, sizeof(NetRosterState)) != 0) {
                        SendRosterState(localSel);
                    }

                    if (p1sel.confirmed && p2sel.confirmed) {
                        ResetRoundState(&round);
                        StartNextRound(&round, &p1, &p2, &p1sel, &p2sel, &domainCasterPlayer, &domainTimer, &clash);
                        memset(&remoteInput, 0, sizeof(remoteInput));
                        memset(&remotePrevInput, 0, sizeof(remotePrevInput));
                        memset(&localNetInput, 0, sizeof(localNetInput));
                        memset(&localNetPrevInput, 0, sizeof(localNetPrevInput));
                        state = STATE_BATTLE;
                    }
                } else {
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
                        if (frontend.launchBattleAfterSelect) {
                            ResetRoundState(&round);
                            StartNextRound(&round, &p1, &p2, &p1sel, &p2sel, &domainCasterPlayer, &domainTimer, &clash);
                            state = STATE_BATTLE;
                        } else {
                            state = STATE_MAIN_MENU;
                        }
                    }
                }
                break;

            case STATE_BATTLE:
            case STATE_DOMAIN:
            case STATE_DOMAIN_CLASH: {
                if (IsKeyPressed(KEY_ESCAPE)) {
                    if (matchMode == MATCH_MODE_ONLINE) {
                        LeaveOnlineMatch(&matchMode, &mpMenu);
                        p1sel.confirmed = false;
                        p2sel.confirmed = false;
                        state = STATE_MAIN_MENU;
                    } else {
                        frontend.pausedFromState = state;
                        frontend.pauseCursor = 0;
                        state = STATE_PAUSE;
                    }
                    break;
                }
                if (gHitstopFrames > 0) {
                    gHitstopFrames--;
                    break;
                }
                if (matchMode == MATCH_MODE_ONLINE) {
                    localNetInput = GatherPlayerOneControls();
                    NetSendInput(localNetInput);
                    if (mpMenu.localPlayerIndex == 0) {
                        SimulateBattleFrame(&p1, &p2, &state, &domainCasterPlayer, &domainTimer, &clash,
                                            &localNetInput, &localNetPrevInput, &remoteInput, &remotePrevInput, &round);
                    } else {
                        SimulateBattleFrame(&p1, &p2, &state, &domainCasterPlayer, &domainTimer, &clash,
                                            &remoteInput, &remotePrevInput, &localNetInput, &localNetPrevInput, &round);
                    }
                    localNetPrevInput = localNetInput;
                    remotePrevInput = remoteInput;
                } else {
                    SimulateBattleFrame(&p1, &p2, &state, &domainCasterPlayer, &domainTimer, &clash,
                                        NULL, NULL, NULL, NULL, &round);
                }
                if (matchMode == MATCH_MODE_ONLINE && state == STATE_GAME_OVER && frontend.onlineResultTimer <= 0.0f) {
                    bool p1Won = (p2.hp <= 0.0f);
                    bool localWon = (mpMenu.localPlayerIndex == 0) ? p1Won : !p1Won;
                    SetOnlineResult(&frontend, mpMenu.username, localWon);
                }
                break;
            }
            case STATE_PAUSE:
                if (IsKeyPressed(KEY_ESCAPE)) {
                    state = frontend.pausedFromState;
                    break;
                }
                {
                    int activateIndex = -1;
                    Vector2 mousePos = GetMousePosition();
                    for (int i = 0; i < 4; i++) {
                        if (CheckCollisionPointRec(mousePos, PauseMenuRowRect(i))) {
                            frontend.pauseCursor = i;
                            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                activateIndex = i;
                            }
                        }
                    }
                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) frontend.pauseCursor = (frontend.pauseCursor + 1) % 4;
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) frontend.pauseCursor = (frontend.pauseCursor + 3) % 4;
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) activateIndex = frontend.pauseCursor;
                if (activateIndex >= 0) {
                    if (activateIndex == 0) {
                        state = frontend.pausedFromState;
                    } else if (activateIndex == 1) {
                        p1sel.confirmed = false;
                        p2sel.confirmed = false;
                        frontend.launchBattleAfterSelect = false;
                        matchMode = MATCH_MODE_LOCAL;
                        state = STATE_CHAR_SELECT;
                    } else if (activateIndex == 2) {
                        p1sel.confirmed = false;
                        p2sel.confirmed = false;
                        matchMode = MATCH_MODE_LOCAL;
                        state = STATE_MAIN_MENU;
                    } else {
                        CloseWindow();
                    }
                }
                }
                break;
            case STATE_GAME_OVER:
                if (round.endTimer > 0.0f) {
                    round.endTimer -= GetFrameTime();
                    if (round.endTimer <= 0.0f) {
                        if (round.p1Wins >= 2 || round.p2Wins >= 2) {
                            round.endTimer = 0.0f;
                        } else {
                            round.roundNumber++;
                            StartNextRound(&round, &p1, &p2, &p1sel, &p2sel, &domainCasterPlayer, &domainTimer, &clash);
                            state = STATE_BATTLE;
                            break;
                        }
                    }
                }
                if (matchMode == MATCH_MODE_ONLINE && frontend.onlineResultTimer > 0.0f) {
                    frontend.onlineResultTimer -= GetFrameTime();
                    if (frontend.onlineResultTimer <= 0.0f) {
                        frontend.onlineResultTimer = 0.0f;
                        LeaveOnlineMatch(&matchMode, &mpMenu);
                        p1sel.confirmed = false;
                        p2sel.confirmed = false;
                        state = STATE_MAIN_MENU;
                        break;
                    }
                }
                if (IsKeyPressed(KEY_ESCAPE)) {
                    if (matchMode == MATCH_MODE_ONLINE) {
                        LeaveOnlineMatch(&matchMode, &mpMenu);
                    }
                    state = STATE_MAIN_MENU;
                    p1sel.confirmed = false;
                    p2sel.confirmed = false;
                    ResetRoundState(&round);
                    break;
                }
                if (IsKeyPressed(KEY_ENTER)) {
                    if (matchMode != MATCH_MODE_ONLINE) {
                        if (round.p1Wins >= 2 || round.p2Wins >= 2) {
                            ResetRoundState(&round);
                            StartNextRound(&round, &p1, &p2, &p1sel, &p2sel, &domainCasterPlayer, &domainTimer, &clash);
                            state = STATE_BATTLE;
                        }
                    }
                }
                break;
        }

        BeginDrawing();

        switch (state) {
            case STATE_MAIN_MENU:
                DrawMainMenu(&menuVideo, frontend.cursor, mpMenu.username, frontend.signInOpen, frontend.signInSavedTimer);
                break;

            case STATE_ABOUT:
                DrawAboutScreen(&menuVideo);
                break;

            case STATE_MULTIPLAYER:
                DrawMultiplayerMenu(&menuVideo, &mpMenu);
                break;

            case STATE_CHAR_SELECT:
                DrawCharSelectScreen(p1sel.cursor, p2sel.cursor,
                                     p1sel.confirmed, p2sel.confirmed,
                                     SCREEN_W, SCREEN_H);
                break;

            case STATE_BATTLE:
                DrawFightVideoBackground(&fightVideo, false, CHAR_COUNT);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawProjectiles(gProjectiles, MAX_PROJECTILES);
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, domainTimer, false, SCREEN_W, round.roundTimer, round.p1Wins, round.p2Wins,
                        round.bannerText, round.subText, round.introTimer);
                AnnounceDraw(SCREEN_W, SCREEN_H);
                break;

            case STATE_DOMAIN: {
                Fighter* caster = (domainCasterPlayer == 1) ? &p1 : &p2;
                Fighter* target = (domainCasterPlayer == 1) ? &p2 : &p1;
                DrawFightVideoBackground(&fightVideo, true, caster->charData.id);
                DrawDomainBackground(caster->charData.id, domainTimer, SCREEN_W, SCREEN_H);
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0, 0, 0, 26});
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawProjectiles(gProjectiles, MAX_PROJECTILES);
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, domainTimer, true, SCREEN_W, round.roundTimer, round.p1Wins, round.p2Wins,
                        round.bannerText, round.subText, round.introTimer);
                AnnounceDraw(SCREEN_W, SCREEN_H);
                if (target->isHeavenlyRestricted) {
                    const char* imm = "[ HEAVENLY RESTRICTION - DOMAIN STUN IMMUNE ]";
                    int iw = MeasureText(imm, 14);
                    DrawText(imm, SCREEN_W / 2 - iw / 2, (int)FLOOR_Y - 30, 14, GOLD);
                }
                break;
            }

            case STATE_DOMAIN_CLASH:
                DrawFightVideoBackground(&fightVideo, true, CHAR_COUNT);
                DrawDomainClashScene(&p1, &p2, clash.timer, clash.duration,
                                     clash.winnerPlayer, clash.damage, SCREEN_W, SCREEN_H);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawProjectiles(gProjectiles, MAX_PROJECTILES);
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, clash.timer, true, SCREEN_W, round.roundTimer, round.p1Wins, round.p2Wins,
                        round.bannerText, round.subText, round.introTimer);
                break;

            case STATE_PAUSE:
                DrawPauseMenu(&menuVideo, frontend.pauseCursor);
                break;

            case STATE_GAME_OVER: {
                const char* winTxt = (matchMode == MATCH_MODE_ONLINE && frontend.onlineResultText[0] != '\0')
                    ? frontend.onlineResultText
                    : ((p2.hp <= 0.0f) ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!");
                Color winCol = (p2.hp <= 0.0f) ? p1.charData.bodyColor : p2.charData.bodyColor;
                DrawFightVideoBackground(&fightVideo, false, CHAR_COUNT);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                DrawProjectiles(gProjectiles, MAX_PROJECTILES);
                DrawFighterBody(&p1, true);
                DrawFighterBody(&p2, false);
                DrawHUD(&p1, &p2, 0.0f, false, SCREEN_W, round.roundTimer, round.p1Wins, round.p2Wins,
                        round.bannerText, round.subText, round.endTimer);
                DrawGameOverOverlay(winTxt, winCol, SCREEN_W, SCREEN_H);
                break;
            }
        }

        EndDrawing();
    }

    if (musicLoaded) UnloadMusicStream(bgm);
    if (gRetroFontLoaded) UnloadFont(gRetroFont);
    if (gojoPortraitLoaded) UnloadTexture(gojoPortrait);
    for (int i = 0; i < menuVideo.count; i++) {
        if (IsTextureValid(menuVideo.frames[i])) UnloadTexture(menuVideo.frames[i]);
    }
    for (int i = 0; i < fightVideo.count; i++) {
        if (IsTextureValid(fightVideo.frames[i])) UnloadTexture(fightVideo.frames[i]);
    }
    NetCleanup();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
