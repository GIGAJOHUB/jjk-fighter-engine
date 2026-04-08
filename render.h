// =============================================================================
// renders.h — JJK Fighter: Visual Rendering Subsystem
// =============================================================================
// Handles ALL drawing calls: particles, domain effects, HUD, arena, fighters.
// main.c calls these functions — zero rendering logic lives in main.c.
// =============================================================================

#ifndef RENDERS_H
#define RENDERS_H

#include "raylib.h"
#include "characters.h"
#include <stdbool.h>

// ---------------------------------------------------------------------------
// PARTICLE SYSTEM
// ---------------------------------------------------------------------------
#define MAX_PARTICLES 512

typedef enum {
    PARTICLE_SPARK = 0,
    PARTICLE_CE_MOTE,
    PARTICLE_BLACK_FLASH,
    PARTICLE_DOMAIN_SHARD,
    PARTICLE_HIT_BURST,
    PARTICLE_REGEN,
    PARTICLE_DOMAIN_ANNOUNCE, // Big domain notification burst
} ParticleType;

typedef struct {
    Vector2  pos;
    Vector2  vel;
    Color    color;
    float    life;     // 0..1, counted down
    float    maxLife;
    float    size;
    ParticleType type;
    bool     active;
} Particle;

// Global particle pool (declared in renders.c)
extern Particle gParticles[MAX_PARTICLES];

void ParticleSpawn(Vector2 pos, Vector2 vel, Color col, float life, float size, ParticleType type);
void ParticleSpawnBurst(Vector2 pos, int count, Color col, float speed, float life, float size, ParticleType type);
void ParticleUpdate(void);
void ParticleDraw(void);

// ---------------------------------------------------------------------------
// SCREEN SHAKE
// ---------------------------------------------------------------------------
typedef struct {
    float intensity;
    float duration;
    float timer;
} ScreenShake;

extern ScreenShake gShake;
void ShakeTrigger(float intensity, float duration);
Vector2 ShakeOffset(void);
void ShakeUpdate(void);

// ---------------------------------------------------------------------------
// DOMAIN ANNOUNCEMENT
// ---------------------------------------------------------------------------
typedef struct {
    char    text[64];
    Color   color;
    float   timer;   // countdown from ~2.5s
    float   maxTime;
    bool    active;
    int     flash;   // frame counter for flashing
} DomainAnnounce;

extern DomainAnnounce gDomainAnnounce;
void AnnounceStart(const char* domainName, const char* casterName, Color col);
void AnnounceUpdate(void);
void AnnounceDraw(int screenW, int screenH);

// ---------------------------------------------------------------------------
// DOMAIN BACKGROUND EFFECTS
// ---------------------------------------------------------------------------
void DrawDomainBackground(CharacterID casterID, float timer, int screenW, int screenH);

// ---------------------------------------------------------------------------
// ARENA
// ---------------------------------------------------------------------------
void DrawArena(int screenW, int screenH, float floorY);

// ---------------------------------------------------------------------------
// FIGHTER RENDERING
// ---------------------------------------------------------------------------
// Forward-declare Fighter so renders.h can accept it.
// Full definition is in main.c / combat.h.
typedef struct Fighter Fighter;

void DrawFighterBody(Fighter* f, bool isP1);
void DrawFighterEffects(Fighter* f); // particles, glow, black flash flash

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
void DrawHUD(Fighter* p1, Fighter* p2, float domainTimer,
             bool domainActive, int screenW);
void DrawCharSelectScreen(int p1Cursor, int p2Cursor,
                          bool p1Confirmed, bool p2Confirmed,
                          int screenW, int screenH);

// ---------------------------------------------------------------------------
// FULL DRAW PASS HELPERS (called by main.c per state)
// ---------------------------------------------------------------------------
void DrawBattleBackground(int screenW, int screenH);
void DrawGameOverOverlay(const char* winnerText, Color winnerColor, int screenW, int screenH);

#endif // RENDERS_H