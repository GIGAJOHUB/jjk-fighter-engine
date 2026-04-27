#include "raylib.h"
#include "raymath.h"
#include "netcode.h"
#include "render.h"
#include "character_gojo_sprite.h"
#include "character_sukuna_sprite.h"
#include "character_yuji_sprite.h"
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
static float gTargetMusicVol = 0.6f;
static float gCurrentMusicVol = 0.0f;
static Sound gSfxRound1;
static Sound gSfxRound2;
static Sound gSfxRound3;
static Sound gSfxFight;
static bool gRoundsLoaded = false;

#define HITSTOP_LIGHT            2
#define DODGE_INVUL_FRAMES       8
#define ATTACK_ACTIVE_START      4
#define ATTACK_ACTIVE_END       10
#define SHADOW_CLONE_TICK        0.45f
#define OVERTIME_THRESHOLD      60.0f
#define BOOGIE_MAX_CHARGES       2
#define BOOGIE_REGEN_TIME        7.0f
#define COOLDOWN_RCT             2.0f
#define COOLDOWN_CE_ATTACK       1.2f
#define COOLDOWN_ABILITY_E       5.0f
#define COOLDOWN_ABILITY_R       5.0f
#define COOLDOWN_ABILITY_F       4.0f
#define BIND_MOUSE_LEFT          10001
#define BIND_MOUSE_RIGHT         10002

#define DOMAIN_COUNTER_WINDOW    5.0f
#define DOMAIN_CLASH_DURATION    1.8f
#define DOMAIN_SUREHIT_DAMAGE  150.0f
#define DOMAIN_TOJI_DAMAGE      75.0f

#define RCT_CE_COST             40.0f
#define RCT_HEAL_AMOUNT         100.0f
#define CE_ATTACK_COST          15.0f
#define DOMAIN_CE_COST          0.10f // 10% of max CE pack
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
#define MAHORAGA_SPEED          4.2f
#define MAHORAGA_ATTACK_RANGE  86.0f
#define MAHORAGA_ATTACK_TIME    0.9f

#define ADAPT_BASIC            (1u << 0)
#define ADAPT_ABILITY1         (1u << 1)
#define ADAPT_ABILITY2         (1u << 2)
#define ADAPT_ABILITY3         (1u << 3)
#define ADAPT_ULT              (1u << 4)
#define ADAPT_PROJECTILE       (1u << 5)

static char gRelayServerIP[64] = "127.0.0.1";
#define ONLINE_RESULT_TIME      3.2f

static Font gRetroFont = {0};
static bool gRetroFontLoaded = false;
static Projectile gProjectiles[MAX_PROJECTILES] = {0};
static int gHitstopFrames = 0;

typedef enum {
    STATE_INTRO = 0,
    STATE_MAIN_MENU,
    STATE_ABOUT,
    STATE_CPU_DIFFICULTY,
    STATE_KEYBINDS,
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
    MATCH_MODE_CPU,
    MATCH_MODE_ONLINE
} MatchMode;

typedef enum {
    CPU_EASY = 0,
    CPU_NORMAL,
    CPU_HARD
} CpuDifficulty;

typedef enum {
    ACT_LEFT = 0,
    ACT_RIGHT,
    ACT_JUMP,
    ACT_CROUCH,
    ACT_ATTACK,
    ACT_BLOCK,
    ACT_TOOL1,
    ACT_RCT,
    ACT_DOMAIN,
    ACT_DASH,
    ACT_ABILITY1,
    ACT_ABILITY2,
    ACT_ABILITY3,
    ACT_ULT,
    ACT_COUNT
} ControlAction;

typedef struct {
    int keys[ACT_COUNT];
} ControlProfile;

static ControlProfile gControls[2];

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
    float countdownTimer; /* 3..0 countdown */
    float fightBannerTimer; /* FIGHT! shown for 2s */
    bool countdownDone;
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
    int charSelectFocus;
    int cpuDifficultyCursor;
    float charSelectScroll;
    int keybindCursor;
    int keybindPlayer;
    bool waitingForKeybind;
    float keybindScroll;
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

static const char* ActionLabel(ControlAction action) {
    static const char* labels[ACT_COUNT] = {
        "Move Left", "Move Right", "Jump", "Crouch", "Melee Attack",
        "Block / Parry", "Tool 1 / Technique", "RCT / Heal", "Domain / Amp", "Dash / Step",
        "Innate Technique", "Technique 2", "Technique 3", "Ultimate"
    };
    return labels[action];
}

static bool BindingDown(int binding) {
    if (binding == BIND_MOUSE_LEFT) return IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (binding == BIND_MOUSE_RIGHT) return IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    return IsKeyDown(binding);
}

static bool BindingPressed(int binding) {
    if (binding == BIND_MOUSE_LEFT) return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (binding == BIND_MOUSE_RIGHT) return IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    return IsKeyPressed(binding);
}

static const char* KeyLabel(int key) {
    switch (key) {
        case BIND_MOUSE_LEFT: return "LMB";
        case BIND_MOUSE_RIGHT: return "RMB";
        case KEY_A: return "A";
        case KEY_D: return "D";
        case KEY_W: return "W";
        case KEY_SPACE: return "SPACE";
        case KEY_LEFT_SHIFT: return "L-SHIFT";
        case KEY_RIGHT_SHIFT: return "R-SHIFT";
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        case KEY_UP: return "UP";
        case KEY_Q: return "Q";
        case KEY_E: return "E";
        case KEY_R: return "R";
        case KEY_F: return "F";
        case KEY_G: return "G";
        case KEY_H: return "H";
        case KEY_T: return "T";
        case KEY_X: return "X";
        case KEY_ONE: return "1";
        case KEY_TWO: return "2";
        case KEY_THREE: return "3";
        case KEY_KP_0: return "NUM0";
        case KEY_KP_1: return "NUM1";
        case KEY_KP_2: return "NUM2";
        case KEY_KP_3: return "NUM3";
        case KEY_KP_4: return "NUM4";
        case KEY_KP_5: return "NUM5";
        case KEY_KP_6: return "NUM6";
        case KEY_KP_7: return "NUM7";
        case KEY_KP_DECIMAL: return "NUM.";
        case KEY_ENTER: return "ENTER";
        default: return "?";
    }
}

static void SetDefaultControls(void) {
    gControls[0].keys[ACT_LEFT] = KEY_A;
    gControls[0].keys[ACT_RIGHT] = KEY_D;
    gControls[0].keys[ACT_JUMP] = KEY_SPACE;
    gControls[0].keys[ACT_CROUCH] = KEY_LEFT_SHIFT;
    gControls[0].keys[ACT_ATTACK] = BIND_MOUSE_LEFT;
    gControls[0].keys[ACT_BLOCK] = BIND_MOUSE_RIGHT;
    gControls[0].keys[ACT_TOOL1] = KEY_ONE;
    gControls[0].keys[ACT_RCT] = KEY_R;
    gControls[0].keys[ACT_DOMAIN] = KEY_S;
    gControls[0].keys[ACT_DASH] = KEY_Q;
    gControls[0].keys[ACT_ABILITY1] = KEY_ONE;
    gControls[0].keys[ACT_ABILITY2] = KEY_TWO;
    gControls[0].keys[ACT_ABILITY3] = KEY_THREE;
    gControls[0].keys[ACT_ULT] = KEY_X;

    gControls[1].keys[ACT_LEFT] = KEY_LEFT;
    gControls[1].keys[ACT_RIGHT] = KEY_RIGHT;
    gControls[1].keys[ACT_JUMP] = KEY_UP;
    gControls[1].keys[ACT_CROUCH] = KEY_RIGHT_SHIFT;
    gControls[1].keys[ACT_ATTACK] = KEY_KP_0;
    gControls[1].keys[ACT_BLOCK] = KEY_KP_DECIMAL;
    gControls[1].keys[ACT_TOOL1] = KEY_KP_1;
    gControls[1].keys[ACT_RCT] = KEY_KP_2;
    gControls[1].keys[ACT_DOMAIN] = KEY_KP_3;
    gControls[1].keys[ACT_DASH] = KEY_KP_8;
    gControls[1].keys[ACT_ABILITY1] = KEY_KP_4;
    gControls[1].keys[ACT_ABILITY2] = KEY_KP_5;
    gControls[1].keys[ACT_ABILITY3] = KEY_KP_6;
    gControls[1].keys[ACT_ULT] = KEY_KP_7;
}

static Rectangle KeybindRowRect(int index, float scroll) {
    return (Rectangle){ 118.0f, 132.0f + index * 28.0f - scroll, 724.0f, 22.0f };
}

static NetInput GatherPlayerOneControls(void) {
    NetInput input = {0};
    input.left = BindingDown(gControls[0].keys[ACT_LEFT]);
    input.right = BindingDown(gControls[0].keys[ACT_RIGHT]);
    input.jump = BindingDown(gControls[0].keys[ACT_JUMP]);
    input.crouch = BindingDown(gControls[0].keys[ACT_CROUCH]);
    input.attack = BindingDown(gControls[0].keys[ACT_ATTACK]);
    input.block = BindingDown(gControls[0].keys[ACT_BLOCK]);
    input.ceAttack = BindingDown(gControls[0].keys[ACT_TOOL1]);
    input.rct = BindingDown(gControls[0].keys[ACT_RCT]);
    input.domain = BindingDown(gControls[0].keys[ACT_DOMAIN]);
    input.dodge = BindingDown(gControls[0].keys[ACT_DASH]);
    input.abilityE = BindingDown(gControls[0].keys[ACT_ABILITY1]);
    input.abilityR = BindingDown(gControls[0].keys[ACT_ABILITY2]);
    input.abilityF = BindingDown(gControls[0].keys[ACT_ABILITY3]);
    input.ult = BindingDown(gControls[0].keys[ACT_ULT]);
    return input;
}

static bool __attribute__((unused)) NetInputChanged(const NetInput* a, const NetInput* b) {
    return memcmp(a, b, sizeof(NetInput)) != 0;
}

static const char* CpuDifficultyLabel(CpuDifficulty difficulty) {
    switch (difficulty) {
        case CPU_EASY: return "EASY";
        case CPU_NORMAL: return "NORMAL";
        case CPU_HARD: return "HARD";
        default: return "NORMAL";
    }
}

static float CpuDifficultyAggro(CpuDifficulty difficulty) {
    switch (difficulty) {
        case CPU_EASY: return 0.45f;
        case CPU_NORMAL: return 0.72f;
        case CPU_HARD: return 0.94f;
        default: return 0.72f;
    }
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
    f.mahoragaMaxHP        = f.maxHP;
    f.mahoragaHP           = f.maxHP;
    f.prevX                = startX;
    return f;
}

static void TriggerVisualEvent(Fighter* f, FighterVisualEvent event, float duration) {
    f->visualEvent = event;
    f->visualEventTimer = duration;
    f->visualEventDuration = duration;
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

static MatchSnapshot __attribute__((unused)) BuildMatchSnapshot(GameState state, int domainCasterPlayer, float domainTimer,
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

static void __attribute__((unused)) ApplyMatchSnapshot(const MatchSnapshot* snapshot, GameState* state, int* domainCasterPlayer,
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

    /* Infinity blocks ALL hits - only Domain penetrates */

    ParticleSpawnBurst(
        (Vector2){ target->hitbox.x + target->hitbox.width * 0.5f, target->hitbox.y + target->hitbox.height * 0.5f },
        14, (Color){150, 220, 255, 255}, 3.5f, 0.35f, 4.0f, PARTICLE_SPARK
    );
    return true;
}

static unsigned int AdaptMaskForHit(bool projectile, UltimateType ultType, int moveKind) {
    if (ultType != ULT_NONE) return ADAPT_ULT;
    if (projectile) return ADAPT_PROJECTILE;
    if (moveKind == 1) return ADAPT_ABILITY1;
    if (moveKind == 2) return ADAPT_ABILITY2;
    if (moveKind == 3) return ADAPT_ABILITY3;
    return ADAPT_BASIC;
}

static void SpinMahoragaWheel(Fighter* summoner, unsigned int mask) {
    if (!summoner->mahoragaActive || summoner->mahoragaHP <= 0.0f || mask == 0u) return;
    if ((summoner->mahoragaAdaptMask & mask) != 0u) return;
    summoner->mahoragaAdaptMask |= mask;
    summoner->mahoragaSpinFrames = 22;
    summoner->mahoragaWheelAngle += 60.0f;
    ParticleSpawnBurst((Vector2){ SCREEN_W * 0.5f, 86.0f }, 18, (Color){255, 245, 180, 255}, 4.2f, 0.5f, 4.0f, PARTICLE_SPARK);
}

static bool DamageMahoragaIfHit(Fighter* summoner, float damage, bool projectile, UltimateType ultType, int moveKind) {
    if (!summoner->mahoragaActive || summoner->mahoragaHP <= 0.0f) return false;
    unsigned int mask = AdaptMaskForHit(projectile, ultType, moveKind);
    if ((summoner->mahoragaAdaptMask & mask) != 0u) {
        ParticleSpawnBurst((Vector2){ summoner->mahoragaHitbox.x + summoner->mahoragaHitbox.width * 0.5f,
                                      summoner->mahoragaHitbox.y + summoner->mahoragaHitbox.height * 0.5f },
                           16, (Color){210, 240, 255, 255}, 3.2f, 0.35f, 3.8f, PARTICLE_SPARK);
        return true;
    }

    summoner->mahoragaHP -= damage;
    if (summoner->mahoragaHP <= 0.0f) {
        summoner->mahoragaHP = 0.0f;
        summoner->mahoragaActive = false;
    }
    SpinMahoragaWheel(summoner, mask);
    ParticleSpawnBurst((Vector2){ summoner->mahoragaHitbox.x + summoner->mahoragaHitbox.width * 0.5f,
                                  summoner->mahoragaHitbox.y + summoner->mahoragaHitbox.height * 0.5f },
                       16, (Color){240, 240, 240, 255}, 3.8f, 0.35f, 4.0f, PARTICLE_HIT_BURST);
    return true;
}

static void ClampHitboxX(Fighter* f) {
    if (f->hitbox.x < 0.0f) f->hitbox.x = 0.0f;
    if (f->hitbox.x > SCREEN_W - f->hitbox.width) f->hitbox.x = SCREEN_W - f->hitbox.width;
}

static bool ApplyCombatHit(Fighter* attacker, Fighter* target, float damage, float displacement,
                           int stunFrames, bool bypassInfinity, bool dodgeable, bool pullsTarget,
                           ParticleType particleType, int particleCount, bool projectileHit, int moveKind) {
    float damageScale = 1.0f;
    float direction = (float)attacker->facingDir;
    bool blocking = false;

    if (dodgeable && target->isDodging && target->dodgeInvulFrames > 0) return false;
    if (target->wakeupInvincFrames > 0) return false;
    if (IsInfinityBlocking(attacker, target, bypassInfinity, &damageScale)) return false;
    if (target->mahoragaActive &&
        ((projectileHit && fabsf((attacker->hitbox.x + attacker->hitbox.width * 0.5f) - (target->mahoragaHitbox.x + target->mahoragaHitbox.width * 0.5f)) < 220.0f) ||
         (!projectileHit && fabsf((attacker->hitbox.x + attacker->hitbox.width * 0.5f) - (target->mahoragaHitbox.x + target->mahoragaHitbox.width * 0.5f)) < 120.0f))) {
        if (DamageMahoragaIfHit(target, damage * damageScale, projectileHit, attacker->ultActive ? attacker->activeUlt : ULT_NONE, moveKind)) {
            TriggerHitstop(HITSTOP_LIGHT);
            return true;
        }
    }

    /* Guard & dizzy system */
    if (target->isDizzy) {
        damageScale *= 1.35f;  /* Dizzy takes bonus damage */
    }

    blocking = target->isBlocking && !target->isHeavenlyRestricted && !bypassInfinity;
    if (blocking) {
        /* SF2-style guard meter drain */
        float guardDrain = damage * 0.15f;
        target->guardMeter -= guardDrain;
        if (target->guardMeter <= 0.0f) {
            /* GUARD BREAK! */
            target->guardMeter = 0.0f;
            target->guardBreakStun = 45;
            target->isBlocking = false;
            blocking = false;
            damage *= 0.5f;  /* Guard break hit does half damage */
            stunFrames = 30;
            ShakeTrigger(14.0f, 0.4f);
            ParticleSpawnBurst(
                (Vector2){ target->hitbox.x + target->hitbox.width * 0.5f, target->hitbox.y + target->hitbox.height * 0.5f },
                28, (Color){255, 180, 50, 255}, 5.0f, 0.45f, 5.0f, PARTICLE_SPARK
            );
        } else {
            /* Normal block: chip damage */
            damage *= 0.08f;
            displacement = 18.0f;
            stunFrames = 12;
            ParticleSpawnBurst(
                (Vector2){ target->hitbox.x + target->hitbox.width * 0.5f, target->hitbox.y + target->hitbox.height * 0.5f },
                12, (Color){215, 225, 235, 255}, 3.0f, 0.25f, 3.2f, PARTICLE_SPARK
            );
        }
    } else {
        /* Not blocking: accumulate dizzy */
        target->dizzyMeter += damage * 0.12f;
        if (target->dizzyMeter >= target->dizzyMax && !target->isDizzy) {
            target->isDizzy = true;
            target->dizzyFrames = 90;  /* ~1.5 sec dizzy */
            target->dizzyMeter = 0.0f;
        }
    }

    target->hp -= damage * damageScale;
    target->lastDamageTaken = damage * damageScale;
    if (target->charData.id == CHAR_MEGUMI) {
        target->specialMeter += damage * damageScale * 0.55f;
        if (target->specialMeter > target->maxSpecialMeter) target->specialMeter = target->maxSpecialMeter;
    }
    if (stunFrames > target->hitStunFrames) target->hitStunFrames = stunFrames;
    target->knockbackVelX = direction * displacement * damageScale;
    if (damage >= 55.0f || attacker->ultActive) {
        target->isKnockedDown = true;
        target->knockdownFrames = 44;
    }

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
    if (!attacker->onGround) {
        height = 62.0f;
        verticalOffset = 34.0f;
    } else if (attacker->isCrouching) {
        height = 72.0f;
        verticalOffset = -14.0f;
    }
    atkBox.x = attacker->hitbox.x + (attacker->facingDir > 0 ? attacker->hitbox.width : -reach);
    atkBox.width = reach;
    atkBox.y = attacker->hitbox.y + verticalOffset;
    atkBox.height = height;

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
                           PARTICLE_HIT_BURST, blackFlashProc ? 20 : 10, false, 0)) {
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
    if (f->infinityActive) TriggerVisualEvent(f, VISUAL_EVENT_GOJO_INFINITY, 0.45f);
    else f->visualEvent = VISUAL_EVENT_NONE;
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
                TriggerVisualEvent(f, VISUAL_EVENT_GOJO_BLUE, 0.35f);
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
                TriggerVisualEvent(f, VISUAL_EVENT_SUKUNA_DISMANTLE, 0.35f);
                TriggerSukunaSfx(VISUAL_EVENT_SUKUNA_DISMANTLE);
            }
            break;

        case CHAR_YUTA: {
            Rectangle katana = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -96.0f), f->hitbox.y + 14.0f, 96.0f, 54.0f };
            f->specialAnimFrames = 12;
            if (CheckCollisionRecs(katana, opponent->hurtbox)) {
                ApplyCombatHit(f, opponent, 28.0f, 34.0f, 12, false, true, false, PARTICLE_SLASH_TRAIL, 16, false, 1);
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
                ApplyCombatHit(f, opponent, 18.0f + bonus, 36.0f, 14, false, true, false, PARTICLE_SPARK, 22, false, 1);
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
                ApplyCombatHit(f, opponent, damage, 52.0f, 18, false, true, false, PARTICLE_HIT_BURST, 22, false, 1);
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
                TriggerVisualEvent(f, VISUAL_EVENT_GOJO_RED, 0.4f);
            }
            break;

                case CHAR_SUKUNA: {
            Rectangle cleave = { f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -76.0f), f->hitbox.y + 10.0f, 76.0f, 60.0f };
            f->specialAnimFrames = 14;
            TriggerVisualEvent(f, VISUAL_EVENT_SUKUNA_CLEAVE, 0.45f);
            TriggerSukunaSfx(VISUAL_EVENT_SUKUNA_CLEAVE);
            if (HorizontalGap(f, opponent) < 82.0f && CheckCollisionRecs(cleave, opponent->hurtbox)) {
                ApplyCombatHit(f, opponent, 46.0f, 62.0f, 14, false, true, false, PARTICLE_SLASH_TRAIL, 20, false, 2);
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
                    ApplyCombatHit(f, opponent, 44.0f, 58.0f, 24, false, true, false, PARTICLE_HIT_BURST, 26, false, 2);
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
                ApplyCombatHit(f, opponent, 40.0f, 88.0f, 18, false, true, false, PARTICLE_SPARK, 28, false, 2);
            }
            break;
        }

        case CHAR_NOBARA:
            if (opponent->dollMarked && f->resonanceCooldown <= 0.0f) {
                f->specialAnimFrames = 14;
                ApplyCombatHit(f, opponent, f->lastDamageTaken > 0.0f ? f->lastDamageTaken : 24.0f, 0.0f, 12, true, false, false, PARTICLE_HIT_BURST, 18, false, 2);
                opponent->dollMarked = false;
                opponent->dollTimer = 0.0f;
                f->resonanceCooldown = 1.2f;
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
        case CHAR_SUKUNA:
            if (!f->ultActive && f->cursedEnergy >= 40.0f) {
                f->cursedEnergy -= 40.0f;
                f->activeUlt = ULT_FUGA;
                f->ultActive = true;
                f->ultDuration = 2.0f;
                f->ultTimer = f->ultDuration;
                f->ultStartupTimer = 0.45f;
                f->ultSpeed = 8.0f * (float)f->facingDir;
                f->ultHitbox = (Rectangle){
                    f->hitbox.x + (f->facingDir > 0 ? f->hitbox.width : -42.0f),
                    f->hitbox.y + 20.0f,
                    42.0f, 18.0f
                };
                TriggerVisualEvent(f, VISUAL_EVENT_SUKUNA_FUGA, 0.45f);
                TriggerSukunaSfx(VISUAL_EVENT_SUKUNA_FUGA);
                ParticleSpawnBurst((Vector2){f->hitbox.x + f->hitbox.width * 0.5f, f->hitbox.y + f->hitbox.height}, 25, (Color){255, 120, 30, 255}, 0.5f, 5.0f, 10.0f, PARTICLE_SPARK);
            }
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
            TriggerVisualEvent(user, VISUAL_EVENT_GOJO_PURPLE, user->ultDuration);
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
            TriggerVisualEvent(user, VISUAL_EVENT_SUKUNA_FUGA, 0.45f);
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
            user->ultDuration = 999.0f;
            user->ultTimer = user->ultDuration;
            user->ultSpeed = 0.0f;
            user->mahoragaActive = true;
            user->mahoragaHP = user->mahoragaMaxHP;
            user->mahoragaHitbox = (Rectangle){
                user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width + 20.0f : -132.0f),
                FLOOR_Y - 132.0f,
                132.0f, 132.0f
            };
            user->mahoragaAttackTimer = 0.0f;
            user->mahoragaDecisionTimer = 0.0f;
            user->mahoragaWheelAngle = 0.0f;
            user->mahoragaSpinFrames = 18;
            user->mahoragaAdaptMask = 0u;
            user->ultHitbox = user->mahoragaHitbox;
            user->ultDamage = 42.0f;
            user->specialMeter = 0.0f;
            ParticleSpawnBurst(
                (Vector2){ user->mahoragaHitbox.x + user->mahoragaHitbox.width * 0.5f, user->mahoragaHitbox.y + user->mahoragaHitbox.height * 0.5f },
                52, (Color){235, 235, 255, 255}, 5.2f, 0.8f, 5.0f, PARTICLE_DOMAIN_SHARD
            );
            ShakeTrigger(15.0f, 0.45f);
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
    bool wasOnGround = f->onGround;

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

    /* Always anchor on-ground fighters to floor (handles crouch height changes) */
    if (f->onGround) {
        f->hitbox.y = FLOOR_Y - f->hitbox.height;
    }

    if (!wasOnGround && f->onGround) {
        f->landingRecoverTimer = 0.14f;
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

        if (target->mahoragaActive && CheckCollisionRecs(proj->hitbox, target->mahoragaHitbox)) {
            DamageMahoragaIfHit(target, proj->damage, true, ULT_NONE,
                                (proj->type == PROJ_GOJO_BLUE || proj->type == PROJ_SUKUNA_DISMANTLE || proj->type == PROJ_MEGUMI_NUE || proj->type == PROJ_NOBARA_NAIL) ? 1 :
                                (proj->type == PROJ_GOJO_RED || proj->type == PROJ_MEGUMI_DOG) ? 2 : 3);
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
                               proj->type == PROJ_GOJO_RED ? 28 : 16, true,
                               (proj->type == PROJ_GOJO_BLUE || proj->type == PROJ_SUKUNA_DISMANTLE || proj->type == PROJ_MEGUMI_NUE || proj->type == PROJ_NOBARA_NAIL) ? 1 :
                               (proj->type == PROJ_GOJO_RED || proj->type == PROJ_MEGUMI_DOG) ? 2 : 3);
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
                        ApplyCombatHit(user, target, user->ultDamage, 18.0f, 8, false, false, false, PARTICLE_CE_MOTE, 10, false, 0);
                        user->beamTicksApplied++;
                    }
                }
            }
            break;

        case ULT_BLACK_FLASH:
        case ULT_HEAVENLY_ASSAULT:
        case ULT_ULTIMATE_TACKLE:
            user->hitbox.x += user->ultSpeed;
            if (user->hitbox.x < 0.0f) user->hitbox.x = 0.0f;
            if (user->hitbox.x > SCREEN_W - user->hitbox.width) user->hitbox.x = SCREEN_W - user->hitbox.width;
            user->ultHitbox.x = user->hitbox.x + (user->facingDir > 0 ? user->hitbox.width : -user->ultHitbox.width);
            user->ultHitbox.y = user->hitbox.y + 10.0f;
            break;

        case ULT_MAHORAGA:
            user->ultHitbox = user->mahoragaHitbox;
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
            if (ApplyCombatHit(user, target, target->maxHP * 0.8f, 95.0f, 24, false, true, false, PARTICLE_HOLLOW_PURPLE, 60, false, 0)) {
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
            ApplyCombatHit(user, target, user->ultDamage, 0.0f, 18, true, false, false, PARTICLE_BLACK_FLASH, 28, false, 0);
            user->ultHitApplied = true;
            user->ultActive = false;
            target->dollMarked = false;
            target->dollTimer = 0.0f;
        } else if (user->activeUlt == ULT_OVERTIME_SLASH) {
            ApplyCombatHit(user, target, user->ultDamage, 90.0f, 24, false, true, false, PARTICLE_SPARK, 34, false, 0);
            user->ultHitApplied = true;
            user->ultActive = false;
        } else if (user->activeUlt == ULT_ULTIMATE_TACKLE) {
            ApplyCombatHit(user, target, user->ultDamage, 110.0f, 30, false, false, false, PARTICLE_HIT_BURST, 30, false, 0);
            user->ultHitApplied = true;
            user->ultActive = false;
        } else {
            if (ApplyCombatHit(user, target, user->ultDamage, 88.0f, 20, false, true, false, PARTICLE_BLACK_FLASH, 34, false, 0)) {
                user->ultHitApplied = true;
                user->ultActive = false;
                user->blackFlashActive = (user->activeUlt == ULT_BLACK_FLASH);
            }
        }
    }

    if (user->activeUlt == ULT_PURE_LOVE_BEAM && user->beamTicksApplied >= MAX_BEAM_TICKS) {
        user->ultActive = false;
    }

    if (user->activeUlt == ULT_MAHORAGA) {
        if (!user->mahoragaActive || user->mahoragaHP <= 0.0f) {
            user->ultActive = false;
        }
        return;
    }

    if (user->ultTimer <= 0.0f || user->ultHitbox.x < -260.0f || user->ultHitbox.x > SCREEN_W + 260.0f) {
        user->ultActive = false;
    }
}

static void UpdateMahoraga(Fighter* summoner, Fighter* target) {
    if (!summoner->mahoragaActive || summoner->mahoragaHP <= 0.0f) return;

    float dt = GetFrameTime();
    float targetCenter = target->hitbox.x + target->hitbox.width * 0.5f;
    float ownCenter = summoner->mahoragaHitbox.x + summoner->mahoragaHitbox.width * 0.5f;
    float delta = targetCenter - ownCenter;

    summoner->mahoragaDecisionTimer += dt;
    summoner->mahoragaAttackTimer -= dt;
    if (summoner->mahoragaSpinFrames > 0) {
        summoner->mahoragaSpinFrames--;
        summoner->mahoragaWheelAngle += 18.0f;
    }

    if (fabsf(delta) > MAHORAGA_ATTACK_RANGE) {
        float step = (delta > 0.0f ? 1.0f : -1.0f) * MAHORAGA_SPEED;
        summoner->mahoragaHitbox.x += step;
    } else if (summoner->mahoragaAttackTimer <= 0.0f) {
        if (CheckCollisionRecs(summoner->mahoragaHitbox, target->hurtbox) ||
            fabsf(delta) < MAHORAGA_ATTACK_RANGE + 18.0f) {
            float damage = 34.0f + (summoner->mahoragaAdaptMask != 0u ? 12.0f : 0.0f);
            ApplyCombatHit(summoner, target, damage, 64.0f, 18, true, false, false, PARTICLE_HIT_BURST, 26, false, 0);
            summoner->mahoragaAttackTimer = MAHORAGA_ATTACK_TIME;
            summoner->mahoragaAdapted = (summoner->mahoragaAdaptMask != 0u);
        }
    }

    if (summoner->mahoragaHitbox.x < 0.0f) summoner->mahoragaHitbox.x = 0.0f;
    if (summoner->mahoragaHitbox.x > SCREEN_W - summoner->mahoragaHitbox.width) {
        summoner->mahoragaHitbox.x = SCREEN_W - summoner->mahoragaHitbox.width;
    }
    summoner->mahoragaHitbox.y = FLOOR_Y - summoner->mahoragaHitbox.height;
    summoner->ultHitbox = summoner->mahoragaHitbox;
}

static NetInput BuildCpuInput(const Fighter* cpu, const Fighter* player, CpuDifficulty difficulty,
                              GameState state, int domainCasterPlayer, float domainTimer) {
    NetInput input = {0};
    float gap = HorizontalGap(cpu, player);
    float aggro = CpuDifficultyAggro(difficulty);
    bool lowHealth = cpu->hp < cpu->maxHP * (difficulty == CPU_HARD ? 0.45f : 0.30f);
    bool fullCe = cpu->maxCE > 0.0f && cpu->cursedEnergy >= cpu->maxCE * 0.9f;
    bool enemyUlting = player->ultActive || player->activeUlt != ULT_NONE;
    bool canDomainCounter = (state == STATE_DOMAIN && domainTimer > 0.6f && domainCasterPlayer != 0);

    if (cpu->hitStunFrames > 0 || cpu->isKnockedDown) return input;

    /* ---- NON-SYMMETRIC MOVEMENT AI ---- */
    /* CPU picks a preferred spacing distance and only approaches/retreats
       based on randomized decisions, NOT by always mirroring the player. */
    float cpuCX = cpu->hitbox.x + cpu->hitbox.width * 0.5f;
    float plrCX = player->hitbox.x + player->hitbox.width * 0.5f;
    bool cpuIsRight = (cpuCX > plrCX);
    float preferredGap = 100.0f + (float)(GetRandomValue(0, 60));
    
    /* Every few frames, decide whether to approach, retreat, or hold position */
    int moveDecision = GetRandomValue(0, 100);
    
    if (gap > 250.0f) {
        /* Very far: approach most of the time, but occasionally hold */
        if (moveDecision < 75) {
            input.left = cpuIsRight;
            input.right = !cpuIsRight;
        }
        /* 25% of the time: stand still or jump approach */
        if (moveDecision > 90 && cpu->onGround) input.jump = true;
    } else if (gap > preferredGap + 30.0f) {
        /* Outside preferred range: approach cautiously */
        if (moveDecision < 55) {
            input.left = cpuIsRight;
            input.right = !cpuIsRight;
        } else if (moveDecision < 70) {
            /* Hold position */
        } else if (moveDecision < 85) {
            /* Dodge/sidestep */
            input.dodge = true;
        }
    } else if (gap < 55.0f && difficulty != CPU_EASY) {
        /* Too close: sometimes back off, sometimes press the attack */
        if (moveDecision < 35) {
            /* Back away */
            input.left = !cpuIsRight;
            input.right = cpuIsRight;
        } else if (moveDecision < 60) {
            /* Hold and attack */
        } else {
            /* Crossup: jump over the player */
            input.jump = true;
            input.left = cpuIsRight;
            input.right = !cpuIsRight;
        }
    } else {
        /* In comfortable range: mostly hold, occasionally adjust */
        if (moveDecision < 20) {
            input.left = cpuIsRight;
            input.right = !cpuIsRight;
        } else if (moveDecision < 30) {
            input.left = !cpuIsRight;
            input.right = cpuIsRight;
        }
        /* 70% of the time: stand still and fight from spacing */
    }

    if (enemyUlting && gap < 180.0f) input.block = true;
    if (enemyUlting && gap < 140.0f) input.dodge = true;
    if (lowHealth && cpu->cursedEnergy >= CalcRCTCost(cpu) && gap > 150.0f) input.rct = true;
    if (canDomainCounter && cpu->hasDomain && cpu->cursedEnergy >= cpu->maxCE * DOMAIN_CE_COST) input.domain = true;
    if (state == STATE_BATTLE && fullCe && gap > 120.0f && cpu->hasDomain && aggro > 0.65f) input.domain = true;
    if (cpu->charData.id == CHAR_GOJO && !cpu->infinityActive && cpu->cursedEnergy > 40.0f && gap < 120.0f) input.abilityF = true;

    if (!cpu->ultUsed) {
        switch (cpu->charData.id) {
            case CHAR_GOJO:
                if (cpu->cursedEnergy >= cpu->maxCE - 0.5f && gap > 150.0f && aggro > 0.7f) input.ult = true;
                break;
            case CHAR_SUKUNA:
                if (cpu->cursedEnergy >= GetCharacterData(CHAR_SUKUNA).maxCE * 0.5f && gap > 110.0f) input.ult = true;
                break;
            case CHAR_YUTA:
                if (gap > 90.0f && cpu->cursedEnergy >= GetUltimateCost(ResolveCopiedUltSource(cpu, player), cpu)) input.ult = true;
                break;
            case CHAR_MEGUMI:
                if (cpu->specialMeter >= cpu->maxSpecialMeter && !cpu->mahoragaActive) input.ult = true;
                break;
            case CHAR_NANAMI:
                if (cpu->passiveTimer >= OVERTIME_THRESHOLD) input.ult = true;
                break;
            case CHAR_NOBARA:
                if (player->dollMarked && cpu->hp > cpu->maxHP * 0.5f) input.ult = true;
                break;
            case CHAR_TODO:
            case CHAR_YUJI:
            case CHAR_TOJI:
                if (gap < 130.0f) input.ult = true;
                break;
            default:
                break;
        }
    }

    if (gap < 90.0f) {
        input.attack = true;
        if (difficulty != CPU_EASY && GetRandomValue(0, 100) < (int)(aggro * 70.0f)) input.abilityR = true;
        if (difficulty == CPU_HARD && GetRandomValue(0, 100) < 28) input.abilityE = true;
    } else if (gap < 180.0f) {
        if (GetRandomValue(0, 100) < (int)(aggro * 65.0f)) input.abilityE = true;
        if (GetRandomValue(0, 100) < (difficulty == CPU_HARD ? 35 : 16)) input.abilityR = true;
    } else {
        if (GetRandomValue(0, 100) < (int)(aggro * 55.0f)) input.ceAttack = true;
        if (GetRandomValue(0, 100) < (int)(aggro * 50.0f)) input.abilityE = true;
    }

    if (!cpu->onGround && difficulty != CPU_EASY && gap < 120.0f) input.attack = true;
    if (player->onGround && gap < 110.0f && player->velY < -4.0f) input.crouch = true;

    return input;
}

static void UpdateAttackCooldown(Fighter* f) {
    float dt = GetFrameTime();
    f->passiveTimer += dt;

    if (f->knockbackVelX != 0.0f) {
        f->hitbox.x += f->knockbackVelX;
        f->knockbackVelX *= 0.85f;
        if (fabsf(f->knockbackVelX) < 0.35f) f->knockbackVelX = 0.0f;
        ClampHitboxX(f);
    }

    if (f->hitStunFrames > 0) {
        f->hitStunFrames--;
    }

    if (f->isKnockedDown) {
        if (f->knockdownFrames > 0) {
            f->knockdownFrames--;
            f->isCrouching = true;
            f->hitbox.height = 42.0f;
        } else {
            f->isKnockedDown = false;
            f->wakeupInvincFrames = 8;
            f->hitbox.height = 90.0f;
        }
    }
    if (f->wakeupInvincFrames > 0) f->wakeupInvincFrames--;

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
        f->comboDisplayTimer -= dt;
        if (f->comboDisplayTimer <= 0.0f) {
            f->comboDisplayTimer = 0.0f;
            f->comboCounter = 0;
        }
    }

    if (f->visualEventTimer > 0.0f) {
        f->visualEventTimer -= dt;
        if (f->visualEventTimer <= 0.0f) {
            f->visualEventTimer = 0.0f;
            f->visualEventDuration = 0.0f;
            if (!(f->charData.id == CHAR_GOJO && f->infinityActive && f->visualEvent == VISUAL_EVENT_GOJO_INFINITY)) {
                f->visualEvent = VISUAL_EVENT_NONE;
            }
        }
    }

    if (f->crouchRecoverTimer > 0.0f) {
        f->crouchRecoverTimer -= dt;
        if (f->crouchRecoverTimer < 0.0f) f->crouchRecoverTimer = 0.0f;
    }

    if (f->landingRecoverTimer > 0.0f) {
        f->landingRecoverTimer -= dt;
        if (f->landingRecoverTimer < 0.0f) f->landingRecoverTimer = 0.0f;
    }

    if (f->infinityActive) {
        f->infinityDrainTimer += dt;
        if (f->infinityDrainTimer >= 0.15f) {
            f->infinityDrainTimer = 0.0f;
            f->cursedEnergy -= INFINITY_CE_DRAIN * 0.15f;
            if (f->cursedEnergy <= 0.0f) {
                f->cursedEnergy = 0.0f;
                f->infinityActive = false;
                f->visualEvent = VISUAL_EVENT_NONE;
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

    if (f->resonanceCooldown > 0.0f) {
        f->resonanceCooldown -= GetFrameTime();
        if (f->resonanceCooldown < 0.0f) f->resonanceCooldown = 0.0f;
    }
    if (f->rctCooldown > 0.0f) {
        f->rctCooldown -= dt;
        if (f->rctCooldown < 0.0f) f->rctCooldown = 0.0f;
    }
    if (f->ceAttackCooldown > 0.0f) {
        f->ceAttackCooldown -= dt;
        if (f->ceAttackCooldown < 0.0f) f->ceAttackCooldown = 0.0f;
    }
    if (f->abilityECooldown > 0.0f) {
        f->abilityECooldown -= dt;
        if (f->abilityECooldown < 0.0f) f->abilityECooldown = 0.0f;
    }
    if (f->abilityRCooldown > 0.0f) {
        f->abilityRCooldown -= dt;
        if (f->abilityRCooldown < 0.0f) f->abilityRCooldown = 0.0f;
    }
    if (f->abilityFCooldown > 0.0f) {
        f->abilityFCooldown -= dt;
        if (f->abilityFCooldown < 0.0f) f->abilityFCooldown = 0.0f;
    }
    if (f->rctGlowTimer > 0.0f) {
        f->rctGlowTimer -= dt;
        if (f->rctGlowTimer < 0.0f) f->rctGlowTimer = 0.0f;
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
    float ceCost = caster->maxCE * DOMAIN_CE_COST; /* 10% of max CE */
    if (caster->isHeavenlyRestricted || !caster->hasDomain || caster->cursedEnergy < ceCost) return;

    caster->cursedEnergy -= ceCost;
    caster->domainActive = true;
    caster->hasDomain    = false;
    target->isStunned    = !target->isHeavenlyRestricted;
    TriggerVisualEvent(caster, VISUAL_EVENT_GOJO_DOMAIN_CAST, 0.8f);
    *domainCasterPlayer  = isP1 ? 1 : 2;
    *domainTimer         = DOMAIN_COUNTER_WINDOW;
    *state               = STATE_DOMAIN;
    if (caster->charData.id == CHAR_SUKUNA) TriggerSukunaDomainSfx();

    AnnounceStart(caster->charData.domainName, caster->charData.name, caster->charData.domainAccentColor);
    ParticleSpawnBurst(
        (Vector2){ caster->hitbox.x + caster->hitbox.width * 0.5f, caster->hitbox.y + caster->hitbox.height * 0.5f },
        96, caster->charData.domainAccentColor, 8.0f, 1.2f, 6.0f, PARTICLE_DOMAIN_ANNOUNCE
    );
    ShakeTrigger(18.0f, 0.6f);
}

static void TriggerDomainClash(Fighter* challenger, Fighter* caster, bool challengerIsP1,
                               GameState* state, float* domainTimer, DomainClashState* clash) {
    float ceCost = challenger->maxCE * DOMAIN_CE_COST; /* 10% of max CE */
    int challengerPlayer = challengerIsP1 ? 1 : 2;
    int casterPlayer = challengerIsP1 ? 2 : 1;

    if (challenger->isHeavenlyRestricted || !challenger->hasDomain || challenger->cursedEnergy < ceCost) return;

    challenger->cursedEnergy -= ceCost;
    challenger->hasDomain     = false;
    challenger->domainActive  = false;
    caster->domainActive      = false;
    challenger->isStunned     = false;
    caster->isStunned         = false;
    TriggerVisualEvent(challenger, VISUAL_EVENT_GOJO_DOMAIN_COUNTER, 0.8f);

    clash->active   = true;
    clash->timer    = DOMAIN_CLASH_DURATION;
    clash->duration = DOMAIN_CLASH_DURATION;
    /* Proportional CE comparison: ratio of (my CE / maxCE) vs opponent */
    { float chalRatio = (challenger->maxCE > 0.0f) ? challenger->cursedEnergy / challenger->maxCE : 0.0f;
      float castRatio = (caster->maxCE > 0.0f)    ? caster->cursedEnergy    / caster->maxCE    : 0.0f;
      float diff = chalRatio - castRatio;
      float dmgBase = 80.0f;
      if (fabsf(diff) < 0.03f) {
          clash->winnerPlayer = 0;
          clash->damage = 0.0f;
      } else if (diff > 0.0f) {
          clash->winnerPlayer = challengerPlayer;
          clash->damage = dmgBase * diff;
      } else {
          clash->winnerPlayer = casterPlayer;
          clash->damage = dmgBase * (-diff);
      }
    }

    *state = STATE_DOMAIN_CLASH;
    if (challenger->charData.id == CHAR_SUKUNA) TriggerSukunaDomainSfx();
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
    int profile   = isP1 ? 0 : 1;
    int leftKey   = gControls[profile].keys[ACT_LEFT];
    int rightKey  = gControls[profile].keys[ACT_RIGHT];
    int jumpKey   = gControls[profile].keys[ACT_JUMP];
    int crouchKey = gControls[profile].keys[ACT_CROUCH];
    int atkKey    = gControls[profile].keys[ACT_ATTACK];
    int blockKey  = gControls[profile].keys[ACT_BLOCK];
    int toolKey   = gControls[profile].keys[ACT_TOOL1];
    int rctKey    = gControls[profile].keys[ACT_RCT];
    int domainKey = gControls[profile].keys[ACT_DOMAIN];
    int dodgeKey  = gControls[profile].keys[ACT_DASH];
    int ab1Key    = gControls[profile].keys[ACT_ABILITY1];
    int ab2Key    = gControls[profile].keys[ACT_ABILITY2];
    int ab3Key    = gControls[profile].keys[ACT_ABILITY3];
    int ultKey    = gControls[profile].keys[ACT_ULT];
    float spd     = f->speed;
    bool wasCrouching = f->isCrouching;

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
    if (f->isKnockedDown) return;

    f->isBlocking = BindingDown(blockKey) && !f->isAttacking && !f->ultActive && !f->isDodging && !f->isHeavenlyRestricted;

    if (BindingPressed(ab1Key) && f->abilityECooldown <= 0.0f) { UseAbility1(f, opponent, isP1); f->abilityECooldown = COOLDOWN_ABILITY_E; }
    if (BindingPressed(ab2Key) && f->abilityRCooldown <= 0.0f) { UseAbility2(f, opponent, isP1); f->abilityRCooldown = COOLDOWN_ABILITY_R; }
    if (BindingPressed(ab3Key) && f->abilityFCooldown <= 0.0f) { UseAbility3(f, isP1); f->abilityFCooldown = COOLDOWN_ABILITY_F; }

    if (BindingPressed(ultKey) && *state == STATE_BATTLE) {
        StartUltimate(f, opponent);
    }

    f->isCrouching = BindingDown(crouchKey);
    if (f->isCrouching) {
        f->hitbox.height = 52.0f;
        spd *= 0.5f;
    } else {
        f->hitbox.height = 90.0f;
        if (wasCrouching) f->crouchRecoverTimer = 0.18f;
    }

    if (!f->isDodging && !f->ultActive) {
        if (BindingDown(leftKey))  { f->hitbox.x -= spd; f->facingDir = -1; }
        if (BindingDown(rightKey)) { f->hitbox.x += spd; f->facingDir =  1; }
        if (BindingPressed(jumpKey) && f->onGround) {
            f->velY = JUMP_FORCE;
            f->onGround = false;
        }
    }

    if (BindingPressed(dodgeKey) && !f->isDodging && f->dodgeCooldown == 0 && f->onGround && !f->ultActive) {
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

    if (BindingPressed(atkKey) && !f->isAttacking && !f->ultActive) {
        bool forceMelee = true;
        f->isAttacking  = true;
        f->attackFrames = 14;
        f->attackLanded = false;
        if (f->charData.id == CHAR_SUKUNA) TriggerSukunaAttackSfx();
        if (!f->onGround) {
            f->attackFrames = 12;
        }
        (void)forceMelee;
        DoMeleeHit(f, opponent);
    }

    if (f->isAttacking) {
        DoMeleeHit(f, opponent);
    }

    if (BindingPressed(toolKey) && !f->isHeavenlyRestricted && !f->ultActive && f->ceAttackCooldown <= 0.0f) {
        float ceCost = CalcCECost(f, CE_ATTACK_COST);
        if (f->cursedEnergy >= ceCost) {
            f->cursedEnergy -= ceCost;
            f->ceAttackCooldown = COOLDOWN_CE_ATTACK;
            StartProjectileAttack(f, isP1);
        }
    }

    if (BindingPressed(rctKey) && !f->isHeavenlyRestricted && !f->ultActive && f->hp < f->maxHP - 0.5f && f->rctCooldown <= 0.0f) {
        float ceCost = CalcRCTCost(f);
        if (f->cursedEnergy >= ceCost) {
            float healAmt = RCT_HEAL_AMOUNT * f->charData.traits.rctHealMultiplier;
            f->cursedEnergy -= ceCost;
            f->hp += healAmt;
            f->rctCooldown = COOLDOWN_RCT;
            f->rctGlowTimer = 0.5f;
            ClampFighter(f);
            TriggerRCTFlash();
            SpawnRCTBurst((Vector2){ f->hitbox.x + f->hitbox.width * 0.5f, f->hitbox.y + 12.0f });
            FloatingTextSpawn((Vector2){ f->hitbox.x + f->hitbox.width * 0.5f, f->hitbox.y - 8.0f }, "+30 HP", (Color){80, 255, 120, 255}, 1.0f);
        }
    }
}

static void ProcessNetworkInput(Fighter* f, Fighter* opponent, bool stunLock, bool isP1,
                                const NetInput* input, const NetInput* prevInput, GameState* state,
                                int* domainCasterPlayer, float* domainTimer, DomainClashState* clash) {
    bool actuallyStunned = (stunLock && !f->isHeavenlyRestricted) || f->hitStunFrames > 0;
    int playerId = isP1 ? 1 : 2;
    float spd = f->speed;
    bool wasCrouching = f->isCrouching;
    bool pressedDomain = input->domain && !prevInput->domain;
    bool pressedUlt = input->ult && !prevInput->ult;
    bool pressedAb1 = input->abilityE && !prevInput->abilityE;
    bool pressedAb2 = input->abilityR && !prevInput->abilityR;
    bool pressedAb3 = input->abilityF && !prevInput->abilityF;
    bool pressedJump = input->jump && !prevInput->jump;
    bool pressedDodge = input->dodge && !prevInput->dodge;
    bool pressedAttack = input->attack && !prevInput->attack;
    bool pressedTool = input->ceAttack && !prevInput->ceAttack;
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
    if (f->isKnockedDown) return;

    f->isBlocking = input->block && !f->isAttacking && !f->ultActive && !f->isDodging && !f->isHeavenlyRestricted;

    if (pressedAb1 && f->abilityECooldown <= 0.0f) { UseAbility1(f, opponent, isP1); f->abilityECooldown = COOLDOWN_ABILITY_E; }
    if (pressedAb2 && f->abilityRCooldown <= 0.0f) { UseAbility2(f, opponent, isP1); f->abilityRCooldown = COOLDOWN_ABILITY_R; }
    if (pressedAb3 && f->abilityFCooldown <= 0.0f) { UseAbility3(f, isP1); f->abilityFCooldown = COOLDOWN_ABILITY_F; }

    if (pressedUlt && *state == STATE_BATTLE) {
        StartUltimate(f, opponent);
    }

    f->isCrouching = input->crouch;
    if (f->isCrouching) {
        f->hitbox.height = 52.0f;
        spd *= 0.5f;
    } else {
        f->hitbox.height = 90.0f;
        if (wasCrouching) f->crouchRecoverTimer = 0.18f;
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
        f->isAttacking = true;
        f->attackFrames = 14;
        f->attackLanded = false;
        if (!f->onGround) {
            f->attackFrames = 12;
        }
        DoMeleeHit(f, opponent);
    }

    if (f->isAttacking) {
        DoMeleeHit(f, opponent);
    }

    if (pressedTool && !f->isHeavenlyRestricted && !f->ultActive && f->ceAttackCooldown <= 0.0f) {
        float ceCost = CalcCECost(f, CE_ATTACK_COST);
        if (f->cursedEnergy >= ceCost) {
            f->cursedEnergy -= ceCost;
            f->ceAttackCooldown = COOLDOWN_CE_ATTACK;
            StartProjectileAttack(f, isP1);
        }
    }

    if (pressedRCT && !f->isHeavenlyRestricted && !f->ultActive && f->hp < f->maxHP - 0.5f && f->rctCooldown <= 0.0f) {
        float ceCost = CalcRCTCost(f);
        if (f->cursedEnergy >= ceCost) {
            float healAmt = RCT_HEAL_AMOUNT * f->charData.traits.rctHealMultiplier;
            f->cursedEnergy -= ceCost;
            f->hp += healAmt;
            f->rctCooldown = COOLDOWN_RCT;
            f->rctGlowTimer = 0.5f;
            ClampFighter(f);
            TriggerRCTFlash();
            SpawnRCTBurst((Vector2){ f->hitbox.x + f->hitbox.width * 0.5f, f->hitbox.y + 12.0f });
            FloatingTextSpawn((Vector2){ f->hitbox.x + f->hitbox.width * 0.5f, f->hitbox.y - 8.0f }, "+30 HP", (Color){80, 255, 120, 255}, 1.0f);
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
    round->countdownTimer = 3.0f;
    round->fightBannerTimer = 0.0f;
    round->countdownDone = false;
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
    round->countdownTimer = 3.0f;
    round->fightBannerTimer = 0.0f;
    round->countdownDone = false;
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
                                RoundState* round, bool cpuMode, CpuDifficulty cpuDifficulty,
                                NetInput* cpuPrevInput) {
    float p1Regen = CE_REGEN_RATE * (p1->charData.traits.hasCopy ? 2.0f : 1.0f);
    float p2Regen = CE_REGEN_RATE * (p2->charData.traits.hasCopy ? 2.0f : 1.0f);
    bool canAct = true;

    p1->prevX = p1->hitbox.x;
    p2->prevX = p2->hitbox.x;

    /* 3-second ROUND text then FIGHT! with SFX */
    if (!round->countdownDone) {
        static int lastPlayedRound = -1;
        if (round->countdownTimer > 2.85f && lastPlayedRound != round->roundNumber) {
            if (round->roundNumber == 1) PlaySound(gSfxRound1);
            else if (round->roundNumber == 2) PlaySound(gSfxRound2);
            else PlaySound(gSfxRound3);
            lastPlayedRound = round->roundNumber;
        }

        if (round->countdownTimer > 0.0f) {
            round->countdownTimer -= GetFrameTime();
            if (round->countdownTimer < 0.0f) round->countdownTimer = 0.0f;
            snprintf(round->bannerText, sizeof(round->bannerText), "ROUND %d", round->roundNumber);
            round->subText[0] = '\0';
            if (round->countdownTimer <= 0.0f) {
                PlaySound(gSfxFight);
            }
        } else if (round->fightBannerTimer < 2.0f) {
            round->fightBannerTimer += GetFrameTime();
            snprintf(round->bannerText, sizeof(round->bannerText), "FIGHT!");
            round->subText[0] = '\0';
            if (!round->roundActive) round->roundActive = true;
        } else {
            round->countdownDone = true;
            round->bannerText[0] = '\0';
            round->subText[0] = '\0';
            lastPlayedRound = -1;
        }
        if (round->countdownTimer > 0.0f) canAct = false;
        round->introTimer = 0.0f;
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

    bool p1Survival = (*state == STATE_DOMAIN && *domainCasterPlayer == 2 && !p1->hasDomain && !p1->isHeavenlyRestricted);
    bool p2Survival = (*state == STATE_DOMAIN && *domainCasterPlayer == 1 && !p2->hasDomain && !p2->isHeavenlyRestricted);
    bool p1Stun = (*state == STATE_DOMAIN && *domainCasterPlayer == 2 && !p1Survival && !p1->isHeavenlyRestricted);
    bool p2Stun = (*state == STATE_DOMAIN && *domainCasterPlayer == 1 && !p2Survival && !p2->isHeavenlyRestricted);

    if (*state == STATE_DOMAIN) {
        if (p1Survival) {
            p1->speed = p1->charData.baseSpeed * 1.4f;
            p2->speed = p2->charData.baseSpeed * 1.4f;
            p1->cursedEnergy += p1Regen * 2.0f;
            p2->cursedEnergy -= CE_REGEN_RATE;
        } else if (p2Survival) {
            p1->speed = p1->charData.baseSpeed * 1.4f;
            p2->speed = p2->charData.baseSpeed * 1.4f;
            p2->cursedEnergy += p2Regen * 2.0f;
            p1->cursedEnergy -= CE_REGEN_RATE;
        } else {
            if (p1->isHeavenlyRestricted && *domainCasterPlayer == 2) p1->speed = p1->charData.baseSpeed * 1.6f;
            if (p2->isHeavenlyRestricted && *domainCasterPlayer == 1) p2->speed = p2->charData.baseSpeed * 1.6f;
        }
    } else {
        p1->speed = p1->charData.baseSpeed * (p1->overtimeBuff ? 1.0f : 1.0f);
        p2->speed = p2->charData.baseSpeed * (p2->overtimeBuff ? 1.0f : 1.0f);
    }

    if (canAct) {
        if (cpuMode && p1Input != NULL && p1Prev != NULL && cpuPrevInput != NULL) {
            NetInput cpuInput = BuildCpuInput(p2, p1, cpuDifficulty, *state, *domainCasterPlayer, *domainTimer);
            ProcessNetworkInput(p1, p2, p1Stun, true, p1Input, p1Prev, state, domainCasterPlayer, domainTimer, clash);
            ProcessNetworkInput(p2, p1, p2Stun, false, &cpuInput, cpuPrevInput, state, domainCasterPlayer, domainTimer, clash);
            *cpuPrevInput = cpuInput;
        } else if (p1Input != NULL && p1Prev != NULL && p2Input != NULL && p2Prev != NULL) {
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
    UpdateMahoraga(p1, p2);
    UpdateMahoraga(p2, p1);
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
    return (Rectangle){ 280.0f, 178.0f + index * 40.0f, 400.0f, 30.0f };
}

static Rectangle MultiplayerRowRect(int index) {
    return (Rectangle){ 220.0f, 164.0f + index * 48.0f, 520.0f, 34.0f };
}

static Rectangle PauseMenuRowRect(int index) {
    return (Rectangle){ 334.0f, 214.0f + index * 34.0f, 290.0f, 26.0f };
}

static Rectangle CpuDifficultyRowRect(int index) {
    return (Rectangle){ 300.0f, 188.0f + index * 52.0f, 360.0f, 36.0f };
}

static Rectangle CharSelectCardRect(int index, int p1Cursor, int p2Cursor, bool cpuMode, int focusIndex) {
    int slotW = 206;
    int gap = 24;
    float stripW = (float)(CHAR_COUNT * slotW + (CHAR_COUNT - 1) * gap);
    float focus = cpuMode ? (float)(focusIndex == 0 ? p1Cursor : p2Cursor) : (((float)p1Cursor + (float)p2Cursor) * 0.5f);
    float targetCenter = focus * (float)(slotW + gap) + slotW * 0.5f;
    float offset = targetCenter - SCREEN_W * 0.5f;
    float visibleW = (float)SCREEN_W - 120.0f;
    float maxOffset = stripW - visibleW;
    if (maxOffset < 0.0f) maxOffset = 0.0f;
    offset = Clamp(offset, 0.0f, maxOffset);
    float x = 60.0f - offset + index * (float)(slotW + gap);
    float lift = (p1Cursor == index || p2Cursor == index) ? -10.0f : 0.0f;
    return (Rectangle){ x, 102.0f + lift, (float)slotW, 296.0f };
}

static void DrawMainMenu(const MenuVideo* video, int cursor, const char* username, bool signInOpen, float signInSavedTimer) {
    static const char* items[] = { "LOCAL", "COMPUTER", "MULTIPLAYER", "CHARACTER SELECT", "KEYBINDS", "INTRODUCE US", "EXIT" };
    int count = 7;
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

    DrawPixelPanel((Rectangle){ 228, 146, 504, 308 }, (Color){18, 14, 30, 225}, (Color){130, 185, 255, 255});

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

    RetroText("UP / DOWN OR W / S TO MOVE", (Vector2){ 284, 438 }, 12.0f, 1.0f, (Color){220, 220, 235, 240});
    RetroText("ENTER / SPACE TO SELECT", (Vector2){ 308, 458 }, 12.0f, 1.0f, (Color){220, 220, 235, 240});

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

static void DrawCpuDifficultyMenu(const MenuVideo* video, int cursor) {
    static const char* items[] = { "EASY", "NORMAL", "HARD", "BACK" };
    DrawMenuBackground(video);
    DrawPixelPanel((Rectangle){ 216, 88, 528, 340 }, (Color){16, 12, 28, 232}, (Color){255, 214, 118, 255});
    RetroText("COMPUTER BATTLE", (Vector2){ 308, 118 }, 28.0f, 1.0f, WHITE);
    RetroText("PICK THE CPU STYLE", (Vector2){ 360, 152 }, 14.0f, 1.0f, (Color){120, 220, 255, 255});
    for (int i = 0; i < 4; i++) {
        Rectangle row = CpuDifficultyRowRect(i);
        bool selected = (i == cursor);
        DrawRectangleRec(row, selected ? (Color){60, 90, 145, 220} : (Color){34, 28, 52, 205});
        DrawRectangleLinesEx(row, 2.0f, selected ? (Color){255, 230, 130, 255} : (Color){110, 100, 150, 220});
        Vector2 size = RetroMeasure(items[i], 18.0f, 1.0f);
        RetroText(items[i], (Vector2){ row.x + row.width * 0.5f - size.x * 0.5f, row.y + 7.0f }, 18.0f, 1.0f, selected ? WHITE : (Color){210, 220, 235, 240});
    }
    RetroText("THE CPU USES FULL CHARACTER KITS AND PLAYER 1 CONTROLS STAY YOURS.", (Vector2){ 232, 382 }, 11.0f, 1.0f, (Color){220, 230, 255, 235});
}

static void DrawKeybindMenu(const MenuVideo* video, const FrontendState* frontend) {
    static const char* kitLines[] = {
        "CONTROLS: A/D MOVE | SPACE JUMP | MOUSE L/R ATTACK/BLOCK",
        "R: CURSED REVERSAL (HEAL) | S: DOMAIN/SIMPLE DOMAIN",
        "Q: DASH | X: ULTIMATE | SHIFT: CROUCH",
        "",
        "--- CHARACTER ABILITIES (1 / 2 / 3) ---",
        "GOJO:   1 BLUE | 2 RED | 3 INFINITY | X HOLLOW PURPLE",
        "SUKUNA: 1 DISMANTLE | 2 CLEAVE | 3 FLAME | X FUGA",
        "YUTA:   1 KATANA | 2 RIKA | 3 COPY | X PURE LOVE BEAM",
        "YUJI:   1 BF FLOW | 2 DIVERGE | 3 PIVOT | X BLACK FLASH",
        "TOJI:   1 ASSAULT | 2 WEAPON | 3 INSTINCT | X HEAVENLY ASSAULT",
        "MEGUMI: 1 NUE | 2 DOGS | 3 SHADOW | X MAHORAGA",
        "NANAMI: 1 RATIO | 2 COLLAPSE | 3 OVERTIME | X OVERTIME SLASH",
        "NOBARA: 1 NAIL | 2 RESONANCE | 3 HAIRPIN | X MAXIMUM",
        "TODO:   1 STRIKE | 2 BOOGIE WOOGIE | 3 CLAP | X TACKLE"
    };
    int kitCount = (int)(sizeof(kitLines) / sizeof(kitLines[0]));
    float contentY = 118.0f - frontend->keybindScroll;
    DrawMenuBackground(video);
    DrawPixelPanel((Rectangle){ 74, 44, 812, 430 }, (Color){16, 12, 26, 230}, (Color){255, 214, 118, 255});
    RetroText("KEYBINDS", (Vector2){ 396, 66 }, 26.0f, 1.0f, WHITE);
    RetroText(frontend->keybindPlayer == 0 ? "PLAYER 1" : "PLAYER 2", (Vector2){ 136, 72 }, 16.0f, 1.0f, (Color){120, 220, 255, 255});
    RetroText("TAB SWITCHES PLAYER | CLICK/ENTER TO REBIND | WHEEL SCROLLS | ESC/BACK RETURNS", (Vector2){ 110, 96 }, 11.0f, 1.0f, (Color){220, 230, 255, 235});
    RetroText("GENERAL", (Vector2){ 118, contentY }, 12.0f, 1.0f, (Color){255, 214, 118, 255});

    for (int i = 0; i < ACT_COUNT; i++) {
        Rectangle row = KeybindRowRect(i, frontend->keybindScroll);
        if (row.y + row.height < 112.0f || row.y > 436.0f) continue;
        bool selected = (i == frontend->keybindCursor);
        DrawRectangleRec(row, selected ? (Color){58, 96, 138, 220} : (Color){26, 28, 44, 210});
        DrawRectangleLinesEx(row, 1.0f, selected ? (Color){255, 230, 130, 255} : (Color){110, 100, 150, 180});
        RetroText(ActionLabel((ControlAction)i), (Vector2){ row.x + 10.0f, row.y + 5.0f }, 11.0f, 1.0f, WHITE);
        RetroText(KeyLabel(gControls[frontend->keybindPlayer].keys[i]), (Vector2){ row.x + 560.0f, row.y + 5.0f }, 11.0f, 1.0f, (Color){120, 220, 255, 255});
        RetroText("EDIT", (Vector2){ row.x + 662.0f, row.y + 5.0f }, 11.0f, 1.0f, (Color){255, 214, 118, 255});
    }
    contentY += 28.0f * (ACT_COUNT + 1);
    RetroText("CHARACTER KITS", (Vector2){ 118, contentY }, 12.0f, 1.0f, (Color){255, 214, 118, 255});
    for (int i = 0; i < kitCount; i++) {
        float y = contentY + 24.0f + i * 20.0f;
        if (y < 112.0f || y > 438.0f) continue;
        RetroText(kitLines[i], (Vector2){ 118, y }, 10.0f, 1.0f, (Color){220, 230, 255, 235});
    }
    DrawRectangleRec((Rectangle){ 700, 430, 120, 26 }, (Color){28, 34, 52, 220});
    DrawRectangleLinesEx((Rectangle){ 700, 430, 120, 26 }, 2.0f, (Color){255, 214, 118, 255});
    RetroText(frontend->waitingForKeybind ? "PRESS INPUT" : "BACK", (Vector2){ frontend->waitingForKeybind ? 710.0f : 742.0f, 437.0f }, 12.0f, 1.0f, WHITE);
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

static void DrawFightVideoBackground(const FightVideo* video, bool domainActive, CharacterID casterId, float focalX) {
    if (video->count > 0 && IsTextureValid(video->frames[video->currentFrame])) {
        float zoom = 1.7f;
        float srcW = (float)video->frames[video->currentFrame].width / zoom;
        float srcH = (float)video->frames[video->currentFrame].height / zoom;
        float focalNorm = focalX / (float)SCREEN_W;
        float maxSrcX = (float)video->frames[video->currentFrame].width - srcW;
        float srcX = Clamp(maxSrcX * focalNorm, 0.0f, maxSrcX);
        Rectangle src = {srcX, 0, srcW, srcH};
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
    InitWindow(SCREEN_W, SCREEN_H, "URUSAI MANIA - Cursed Clash [v1.0.6]");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();
    if (!gRoundsLoaded) {
        gSfxRound1 = LoadSound("assets/sounds/round1.ogg");
        gSfxRound2 = LoadSound("assets/sounds/round2.ogg");
        gSfxRound3 = LoadSound("assets/sounds/round3.ogg");
        gSfxFight  = LoadSound("assets/sounds/fight.ogg");
        gRoundsLoaded = true;
    }
    SetDefaultControls();
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
    frontend.cpuDifficultyCursor = 1;
    frontend.charSelectFocus = 0;
    frontend.charSelectScroll = 0.0f;
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
    LoadGojoSpritePack("assets/characters/gojo/sprites");
    LoadSukunaSpritePack("assets/sprites/meguna_v5");
    LoadSukunaSfx("assets/sounds/meguna_v5");
    LoadYujiSpritePack("assets/sprites/yuji_s1");
    LoadSavedUsername(mpMenu.username, sizeof(mpMenu.username));

    GameState state = STATE_INTRO;
    GameState prevState = STATE_INTRO;
    int randomMusic = GetRandomValue(1, 3);
    MatchMode matchMode = MATCH_MODE_LOCAL;
    CpuDifficulty cpuDifficulty = CPU_NORMAL;
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
    NetInput cpuPrevInput = {0};

    while (!WindowShouldClose()) {

        /* Battle music: pick once per match (not per round) */
        if (state == STATE_BATTLE && prevState == STATE_CHAR_SELECT) {
            if (musicLoaded) { StopMusicStream(bgm); UnloadMusicStream(bgm); }
            { int rndB = GetRandomValue(0, 2);
              if (rndB == 0) bgm = LoadMusicStream("assets/music/track2.mp3");
              else if (rndB == 1) bgm = LoadMusicStream("assets/music/track3.mp3");
              else bgm = LoadMusicStream("assets/menu_theme.mp3"); }
            gTargetMusicVol = 0.6f;
            gCurrentMusicVol = 0.0f;
            SetMusicVolume(bgm, gCurrentMusicVol);
            PlayMusicStream(bgm);
            musicLoaded = true;
        }
        prevState = state;


        if (state == STATE_INTRO) {
            static int introFrame = 1;
            static float introTimer = 0.0f;
            static Texture2D introTex = {0};
            static bool introMusicPlaying = false;
            
            if (!introMusicPlaying) {
                if (musicLoaded) { StopMusicStream(bgm); UnloadMusicStream(bgm); }
                bgm = LoadMusicStream("assets/music/intro_audio.mp3");
                SetMusicVolume(bgm, 0.5f);
                PlayMusicStream(bgm);
                musicLoaded = true;
                introMusicPlaying = true;
            }
            
            introTimer += GetFrameTime();
            if (introTimer >= 1.0f / 12.0f) {
                introTimer = 0.0f;
                introFrame++;
                if (introFrame > 1078) introFrame = 1; // loop
                char path[256];
                sprintf(path, "assets/intro_frames/frame_%04d.jpg", introFrame);
                if (introTex.id != 0) UnloadTexture(introTex);
                introTex = LoadTexture(path);
            }
            
            BeginDrawing();
            ClearBackground(BLACK);
            if (introTex.id != 0) {
                DrawTexturePro(introTex, 
                    (Rectangle){0, 0, introTex.width, introTex.height}, 
                    (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()}, 
                    (Vector2){0,0}, 0.0f, WHITE);
            }
            
            static float textAlpha = 0.0f;
            static bool textFadeIn = true;
            if (textFadeIn) { textAlpha += GetFrameTime(); if (textAlpha >= 1.0f) { textAlpha = 1.0f; textFadeIn = false; } }
            else { textAlpha -= GetFrameTime(); if (textAlpha <= 0.0f) { textAlpha = 0.0f; textFadeIn = true; } }
            
            Color titleCol = (Color){255, 50, 50, 255};
            Color promptCol = (Color){255, 255, 255, (unsigned char)(textAlpha * 255.0f)};
            
            DrawTextEx(gRetroFont, "URUSAI MANIA", (Vector2){ GetScreenWidth()/2 - MeasureTextEx(gRetroFont, "URUSAI MANIA", 60, 2).x/2, 100 }, 60, 2, titleCol);
            DrawTextEx(gRetroFont, "PRESS ANY KEY TO START", (Vector2){ GetScreenWidth()/2 - MeasureTextEx(gRetroFont, "PRESS ANY KEY TO START", 30, 2).x/2, GetScreenHeight() - 100 }, 30, 2, promptCol);
            
            if (GetKeyPressed() > 0) {
                state = STATE_MAIN_MENU;
                if (musicLoaded) { StopMusicStream(bgm); UnloadMusicStream(bgm); }
                bgm = LoadMusicStream("assets/music/track1.mp3");
                gTargetMusicVol = 0.6f;
                gCurrentMusicVol = 0.6f;
                SetMusicVolume(bgm, 0.60f);
                PlayMusicStream(bgm);
                musicLoaded = true;
            }
            EndDrawing();
            continue;
        }

        if (musicLoaded) {
            /* Smooth volume transitions */
            if (gCurrentMusicVol < gTargetMusicVol) {
                gCurrentMusicVol += GetFrameTime() * 0.4f;
                if (gCurrentMusicVol > gTargetMusicVol) gCurrentMusicVol = gTargetMusicVol;
            } else if (gCurrentMusicVol > gTargetMusicVol) {
                gCurrentMusicVol -= GetFrameTime() * 0.4f;
                if (gCurrentMusicVol < gTargetMusicVol) gCurrentMusicVol = gTargetMusicVol;
            }
            SetMusicVolume(bgm, gCurrentMusicVol);

            UpdateMusicStream(bgm);
            if (!IsMusicStreamPlaying(bgm) || GetMusicTimePlayed(bgm) >= GetMusicTimeLength(bgm) - 0.05f) {
                StopMusicStream(bgm);
                PlayMusicStream(bgm);
            }
        }

        if (state == STATE_MAIN_MENU || state == STATE_ABOUT || state == STATE_CPU_DIFFICULTY || state == STATE_KEYBINDS || state == STATE_MULTIPLAYER || state == STATE_PAUSE) {
            UpdateMenuVideo(&menuVideo);
        }
        if (state == STATE_BATTLE || state == STATE_DOMAIN || state == STATE_DOMAIN_CLASH || state == STATE_GAME_OVER) {
            UpdateFightVideo(&fightVideo);
        }

        ShakeUpdate();
        ParticleUpdate();
        FloatingTextUpdate();
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

                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) frontend.cursor = (frontend.cursor + 1) % 7;
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) frontend.cursor = (frontend.cursor + 6) % 7;
                for (int i = 0; i < 7; i++) {
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
                            frontend.charSelectFocus = 0;
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
                            matchMode = MATCH_MODE_CPU;
                            frontend.cpuDifficultyCursor = (int)cpuDifficulty;
                            frontend.charSelectFocus = 0;
                            p1sel.confirmed = false;
                            p2sel.confirmed = false;
                            p2sel.cursor = GetRandomValue(0, CHAR_COUNT - 1);
                            p2sel.selected = (CharacterID)p2sel.cursor;
                            state = STATE_CPU_DIFFICULTY;
                            break;
                        case 2:
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
                        case 3:
                            matchMode = MATCH_MODE_LOCAL;
                            frontend.launchBattleAfterSelect = false;
                            frontend.charSelectFocus = 0;
                            state = STATE_CHAR_SELECT;
                            break;
                        case 4:
                            frontend.keybindCursor = 0;
                            frontend.keybindPlayer = 0;
                            frontend.waitingForKeybind = false;
                            frontend.keybindScroll = 0.0f;
                            state = STATE_KEYBINDS;
                            break;
                        case 5:
                            state = STATE_ABOUT;
                            break;
                        case 6:
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

            case STATE_CPU_DIFFICULTY: {
                int activateIndex = -1;
                Vector2 mousePos = GetMousePosition();
                if (IsKeyPressed(KEY_ESCAPE)) {
                    state = STATE_MAIN_MENU;
                    break;
                }
                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) frontend.cpuDifficultyCursor = (frontend.cpuDifficultyCursor + 1) % 4;
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) frontend.cpuDifficultyCursor = (frontend.cpuDifficultyCursor + 3) % 4;
                for (int i = 0; i < 4; i++) {
                    if (CheckCollisionPointRec(mousePos, CpuDifficultyRowRect(i))) {
                        frontend.cpuDifficultyCursor = i;
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) activateIndex = i;
                    }
                }
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) activateIndex = frontend.cpuDifficultyCursor;
                if (activateIndex == 0 || activateIndex == 1 || activateIndex == 2) {
                    cpuDifficulty = (CpuDifficulty)activateIndex;
                    frontend.launchBattleAfterSelect = true;
                    frontend.charSelectFocus = 0;
                    p1sel.confirmed = false;
                    p2sel.confirmed = false;
                    p2sel.cursor = GetRandomValue(0, CHAR_COUNT - 1);
                    p2sel.selected = (CharacterID)p2sel.cursor;
                    state = STATE_CHAR_SELECT;
                } else if (activateIndex == 3) {
                    state = STATE_MAIN_MENU;
                }
                break;
            }

            case STATE_KEYBINDS: {
                int clickedRow = -1;
                Vector2 mousePos = GetMousePosition();
                Rectangle backRect = { 700, 430, 120, 26 };
                if (frontend.waitingForKeybind) {
                    int pressed = GetKeyPressed();
                    if (pressed != 0 && pressed != KEY_ESCAPE) {
                        gControls[frontend.keybindPlayer].keys[frontend.keybindCursor] = pressed;
                        frontend.waitingForKeybind = false;
                    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        gControls[frontend.keybindPlayer].keys[frontend.keybindCursor] = BIND_MOUSE_LEFT;
                        frontend.waitingForKeybind = false;
                    } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                        gControls[frontend.keybindPlayer].keys[frontend.keybindCursor] = BIND_MOUSE_RIGHT;
                        frontend.waitingForKeybind = false;
                    } else if (IsKeyPressed(KEY_ESCAPE)) {
                        frontend.waitingForKeybind = false;
                    }
                    break;
                }
                if (IsKeyPressed(KEY_ESCAPE)) {
                    state = STATE_MAIN_MENU;
                    break;
                }
                if (IsKeyPressed(KEY_TAB)) frontend.keybindPlayer = 1 - frontend.keybindPlayer;
                frontend.keybindScroll -= GetMouseWheelMove() * 24.0f;
                if (frontend.keybindScroll < 0.0f) frontend.keybindScroll = 0.0f;
                if (frontend.keybindScroll > 180.0f) frontend.keybindScroll = 180.0f;
                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) frontend.keybindCursor = (frontend.keybindCursor + 1) % ACT_COUNT;
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) frontend.keybindCursor = (frontend.keybindCursor + ACT_COUNT - 1) % ACT_COUNT;
                for (int i = 0; i < ACT_COUNT; i++) {
                    if (CheckCollisionPointRec(mousePos, KeybindRowRect(i, frontend.keybindScroll))) {
                        frontend.keybindCursor = i;
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clickedRow = i;
                    }
                }
                if (CheckCollisionPointRec(mousePos, backRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    state = STATE_MAIN_MENU;
                    break;
                }
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) clickedRow = frontend.keybindCursor;
                if (clickedRow >= 0) frontend.waitingForKeybind = true;
                break;
            }

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
                    if (IsKeyPressed(KEY_A) && !localSel->confirmed) localSel->cursor = (localSel->cursor > 0) ? localSel->cursor - 1 : CHAR_COUNT - 1;
                    if (IsKeyPressed(KEY_D) && !localSel->confirmed) localSel->cursor = (localSel->cursor < CHAR_COUNT - 1) ? localSel->cursor + 1 : 0;
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
                        memset(&cpuPrevInput, 0, sizeof(cpuPrevInput));
                        state = STATE_BATTLE;
                    }
                } else if (matchMode == MATCH_MODE_CPU) {
                    Vector2 mousePos = GetMousePosition();
                    float wheel = GetMouseWheelMove();
                    SelectState* focusSel = (frontend.charSelectFocus == 0) ? &p1sel : &p2sel;
                    if (IsKeyPressed(KEY_TAB) && p1sel.confirmed) frontend.charSelectFocus = 1 - frontend.charSelectFocus;
                    if (IsKeyPressed(KEY_A) && !focusSel->confirmed) focusSel->cursor = (focusSel->cursor > 0) ? focusSel->cursor - 1 : CHAR_COUNT - 1;
                    if (IsKeyPressed(KEY_D) && !focusSel->confirmed) focusSel->cursor = (focusSel->cursor < CHAR_COUNT - 1) ? focusSel->cursor + 1 : 0;
                    if (wheel < -0.1f && !focusSel->confirmed && focusSel->cursor < CHAR_COUNT - 1) focusSel->cursor++;
                    if (wheel > 0.1f && !focusSel->confirmed && focusSel->cursor > 0) focusSel->cursor--;
                    for (int i = 0; i < CHAR_COUNT; i++) {
                        Rectangle card = CharSelectCardRect(i, p1sel.cursor, p2sel.cursor, true, frontend.charSelectFocus);
                        if (CheckCollisionPointRec(mousePos, card)) {
                            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                focusSel->cursor = i;
                                if (focusSel->confirmed && frontend.charSelectFocus == 0) {
                                    p1sel.confirmed = false;
                                } else if (focusSel->confirmed && frontend.charSelectFocus == 1) {
                                    p2sel.confirmed = false;
                                }
                            }
                        }
                    }
                    if (IsKeyPressed(KEY_BACKSPACE)) {
                        if (p2sel.confirmed) p2sel.confirmed = false;
                        else p1sel.confirmed = false;
                        frontend.charSelectFocus = p1sel.confirmed ? 1 : 0;
                    }
                    if (IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                        if (!p1sel.confirmed) {
                            p1sel.selected = (CharacterID)p1sel.cursor;
                            p1sel.confirmed = true;
                            frontend.charSelectFocus = 1;
                        } else if (!p2sel.confirmed) {
                            p2sel.selected = (CharacterID)p2sel.cursor;
                            p2sel.confirmed = true;
                        }
                    }
                    p1sel.selected = (CharacterID)p1sel.cursor;
                    p2sel.selected = (CharacterID)p2sel.cursor;
                    if (p1sel.confirmed && p2sel.confirmed) {
                        ResetRoundState(&round);
                        StartNextRound(&round, &p1, &p2, &p1sel, &p2sel, &domainCasterPlayer, &domainTimer, &clash);
                        memset(&localNetInput, 0, sizeof(localNetInput));
                        memset(&localNetPrevInput, 0, sizeof(localNetPrevInput));
                        memset(&cpuPrevInput, 0, sizeof(cpuPrevInput));
                        state = STATE_BATTLE;
                    }
                } else {
                    Vector2 mousePos = GetMousePosition();
                    float wheel = GetMouseWheelMove();
                    if (IsKeyPressed(KEY_A) && p1sel.cursor > 0) p1sel.cursor--;
                    if (IsKeyPressed(KEY_D) && p1sel.cursor < CHAR_COUNT - 1) p1sel.cursor++;
                    if (wheel < -0.1f && p1sel.cursor < CHAR_COUNT - 1) p1sel.cursor++;
                    if (wheel > 0.1f && p1sel.cursor > 0) p1sel.cursor--;
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
                    for (int i = 0; i < CHAR_COUNT; i++) {
                        Rectangle card = CharSelectCardRect(i, p1sel.cursor, p2sel.cursor, false, 0);
                        if (CheckCollisionPointRec(mousePos, card) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            if (!p1sel.confirmed) p1sel.cursor = i;
                            else if (!p2sel.confirmed) p2sel.cursor = i;
                        }
                    }
                    if (p1sel.confirmed && p2sel.confirmed) {
                        if (frontend.launchBattleAfterSelect) {
                            ResetRoundState(&round);
                            StartNextRound(&round, &p1, &p2, &p1sel, &p2sel, &domainCasterPlayer, &domainTimer, &clash);
                            memset(&cpuPrevInput, 0, sizeof(cpuPrevInput));
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
                                            &localNetInput, &localNetPrevInput, &remoteInput, &remotePrevInput, &round,
                                            false, cpuDifficulty, &cpuPrevInput);
                    } else {
                        SimulateBattleFrame(&p1, &p2, &state, &domainCasterPlayer, &domainTimer, &clash,
                                            &remoteInput, &remotePrevInput, &localNetInput, &localNetPrevInput, &round,
                                            false, cpuDifficulty, &cpuPrevInput);
                    }
                    localNetPrevInput = localNetInput;
                    remotePrevInput = remoteInput;
                } else {
                    localNetInput = GatherPlayerOneControls();
                    SimulateBattleFrame(&p1, &p2, &state, &domainCasterPlayer, &domainTimer, &clash,
                                        matchMode == MATCH_MODE_CPU ? &localNetInput : NULL,
                                        matchMode == MATCH_MODE_CPU ? &localNetPrevInput : NULL,
                                        NULL, NULL, &round,
                                        matchMode == MATCH_MODE_CPU, cpuDifficulty, &cpuPrevInput);
                    localNetPrevInput = localNetInput;
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

            case STATE_CPU_DIFFICULTY:
                DrawCpuDifficultyMenu(&menuVideo, frontend.cpuDifficultyCursor);
                break;

            case STATE_KEYBINDS:
                DrawKeybindMenu(&menuVideo, &frontend);
                break;

            case STATE_MULTIPLAYER:
                DrawMultiplayerMenu(&menuVideo, &mpMenu);
                break;

            case STATE_CHAR_SELECT:
                DrawCharSelectScreen(p1sel.cursor, p2sel.cursor,
                                     p1sel.confirmed, p2sel.confirmed,
                                     SCREEN_W, SCREEN_H, matchMode == MATCH_MODE_CPU,
                                     frontend.charSelectFocus,
                                     matchMode == MATCH_MODE_CPU ? CpuDifficultyLabel(cpuDifficulty) : "VERSUS");
                break;

            case STATE_BATTLE:
                DrawFightVideoBackground(&fightVideo, false, CHAR_COUNT, (p1.hitbox.x + p2.hitbox.x) * 0.5f);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                FloatingTextDraw();
                DrawProjectiles(gProjectiles, MAX_PROJECTILES);
                DrawFighterBody(&p1, true,
                                round.fightBannerTimer > 0.0f ? 1.0f - (round.fightBannerTimer / ROUND_INTRO_TIME) : -1.0f,
                                false, false);
                DrawFighterBody(&p2, false,
                                round.fightBannerTimer > 0.0f ? 1.0f - (round.fightBannerTimer / ROUND_INTRO_TIME) : -1.0f,
                                false, false);
                DrawHUD(&p1, &p2, domainTimer, false, SCREEN_W, round.roundTimer, round.p1Wins, round.p2Wins,
                        round.bannerText, round.subText, round.fightBannerTimer);
                AnnounceDraw(SCREEN_W, SCREEN_H);
                DrawRCTFlashOverlay(SCREEN_W, SCREEN_H);
                break;

            case STATE_DOMAIN: {
                Fighter* caster = (domainCasterPlayer == 1) ? &p1 : &p2;
                Fighter* target = (domainCasterPlayer == 1) ? &p2 : &p1;
                DrawFightVideoBackground(&fightVideo, true, caster->charData.id, (p1.hitbox.x + p2.hitbox.x) * 0.5f);
                DrawDomainBackground(caster->charData.id, domainTimer, SCREEN_W, SCREEN_H);
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0, 0, 0, 26});
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                FloatingTextDraw();
                DrawProjectiles(gProjectiles, MAX_PROJECTILES);
                DrawFighterBody(&p1, true,
                                round.fightBannerTimer > 0.0f ? 1.0f - (round.fightBannerTimer / ROUND_INTRO_TIME) : -1.0f,
                                domainCasterPlayer == 1, domainCasterPlayer == 2);
                DrawFighterBody(&p2, false,
                                round.fightBannerTimer > 0.0f ? 1.0f - (round.fightBannerTimer / ROUND_INTRO_TIME) : -1.0f,
                                domainCasterPlayer == 2, domainCasterPlayer == 1);
                DrawHUD(&p1, &p2, domainTimer, true, SCREEN_W, round.roundTimer, round.p1Wins, round.p2Wins,
                        round.bannerText, round.subText, round.fightBannerTimer);
                AnnounceDraw(SCREEN_W, SCREEN_H);
                DrawRCTFlashOverlay(SCREEN_W, SCREEN_H);
                if (target->isHeavenlyRestricted) {
                    const char* imm = "[ HEAVENLY RESTRICTION - DOMAIN STUN IMMUNE ]";
                    int iw = MeasureText(imm, 14);
                    DrawText(imm, SCREEN_W / 2 - iw / 2, (int)FLOOR_Y - 30, 14, GOLD);
                }
                break;
            }

            case STATE_DOMAIN_CLASH:
                DrawFightVideoBackground(&fightVideo, true, CHAR_COUNT, (p1.hitbox.x + p2.hitbox.x) * 0.5f);
                DrawDomainClashScene(&p1, &p2, clash.timer, clash.duration,
                                     clash.winnerPlayer, clash.damage, SCREEN_W, SCREEN_H);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                FloatingTextDraw();
                DrawProjectiles(gProjectiles, MAX_PROJECTILES);
                DrawFighterBody(&p1, true, -1.0f, domainCasterPlayer == 1, domainCasterPlayer == 2);
                DrawFighterBody(&p2, false, -1.0f, domainCasterPlayer == 2, domainCasterPlayer == 1);
                DrawHUD(&p1, &p2, clash.timer, true, SCREEN_W, round.roundTimer, round.p1Wins, round.p2Wins,
                        round.bannerText, round.subText, round.fightBannerTimer);
                DrawRCTFlashOverlay(SCREEN_W, SCREEN_H);
                break;

            case STATE_PAUSE:
                DrawPauseMenu(&menuVideo, frontend.pauseCursor);
                break;

            case STATE_GAME_OVER: {
                const char* winTxt = (matchMode == MATCH_MODE_ONLINE && frontend.onlineResultText[0] != '\0')
                    ? frontend.onlineResultText
                    : ((matchMode == MATCH_MODE_CPU)
                        ? ((p2.hp <= 0.0f) ? "YOU WIN!" : "COMPUTER WINS!")
                        : ((p2.hp <= 0.0f) ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!"));
                Color winCol = (p2.hp <= 0.0f) ? p1.charData.bodyColor : p2.charData.bodyColor;
                DrawFightVideoBackground(&fightVideo, false, CHAR_COUNT, (p1.hitbox.x + p2.hitbox.x) * 0.5f);
                DrawArena(SCREEN_W, SCREEN_H, FLOOR_Y);
                ParticleDraw();
                FloatingTextDraw();
                DrawProjectiles(gProjectiles, MAX_PROJECTILES);
                DrawFighterBody(&p1, true, -1.0f, false, false);
                DrawFighterBody(&p2, false, -1.0f, false, false);
                DrawHUD(&p1, &p2, 0.0f, false, SCREEN_W, round.roundTimer, round.p1Wins, round.p2Wins,
                        round.bannerText, round.subText, round.endTimer);
                DrawGameOverOverlay(winTxt, winCol, SCREEN_W, SCREEN_H);
                DrawRCTFlashOverlay(SCREEN_W, SCREEN_H);
                break;
            }
        }

        EndDrawing();
    }

    if (musicLoaded) UnloadMusicStream(bgm);
    if (gRetroFontLoaded) UnloadFont(gRetroFont);
    if (gojoPortraitLoaded) UnloadTexture(gojoPortrait);
    UnloadGojoSpritePack();
    UnloadSukunaSpritePack();
    UnloadYujiSpritePack();
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
