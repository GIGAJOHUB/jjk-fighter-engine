#ifndef RENDER_H
#define RENDER_H

#include "fighter.h"
#include "raylib.h"
#include <stdbool.h>

#define MAX_PARTICLES 1024

typedef enum {
    PARTICLE_SPARK = 0,
    PARTICLE_CE_MOTE,
    PARTICLE_BLACK_FLASH,
    PARTICLE_DOMAIN_SHARD,
    PARTICLE_HIT_BURST,
    PARTICLE_REGEN,
    PARTICLE_DOMAIN_ANNOUNCE,
    PARTICLE_CLASH_ARC,
    PARTICLE_HOLLOW_PURPLE,
    PARTICLE_SLASH_TRAIL
} ParticleType;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float life;
    float maxLife;
    float size;
    ParticleType type;
    bool active;
} Particle;

extern Particle gParticles[MAX_PARTICLES];

void ParticleSpawn(Vector2 pos, Vector2 vel, Color col, float life, float size, ParticleType type);
void ParticleSpawnBurst(Vector2 pos, int count, Color col, float speed, float life, float size, ParticleType type);
void ParticleUpdate(void);
void ParticleDraw(void);

typedef struct {
    float intensity;
    float duration;
    float timer;
} ScreenShake;

extern ScreenShake gShake;
void ShakeTrigger(float intensity, float duration);
Vector2 ShakeOffset(void);
void ShakeUpdate(void);

typedef struct {
    char    text[64];
    char    subtext[96];
    Color   color;
    float   timer;
    float   maxTime;
    bool    active;
    int     flash;
} DomainAnnounce;

extern DomainAnnounce gDomainAnnounce;
void AnnounceStart(const char* domainName, const char* casterName, Color col);
void AnnounceUpdate(void);
void AnnounceDraw(int screenW, int screenH);
void SetUIFont(Font font, bool loaded);
void SetGojoPortrait(Texture2D portrait, bool loaded);

void DrawDomainBackground(CharacterID casterID, float timer, int screenW, int screenH);
void DrawDomainClashScene(Fighter* p1, Fighter* p2, float timer, float duration,
                          int winnerPlayer, float clashDamage, int screenW, int screenH);

void DrawArena(int screenW, int screenH, float floorY);
void DrawProjectiles(const Projectile* projectiles, int count);
void DrawFighterBody(Fighter* f, bool isP1);
void DrawFighterEffects(Fighter* f);
void DrawHUD(Fighter* p1, Fighter* p2, float domainTimer, bool domainActive, int screenW,
             float roundTimer, int p1Rounds, int p2Rounds,
             const char* bannerText, const char* subText, float bannerTimer);
void DrawCharSelectScreen(int p1Cursor, int p2Cursor,
                          bool p1Confirmed, bool p2Confirmed,
                          int screenW, int screenH);
void DrawBattleBackground(int screenW, int screenH);
void DrawGameOverOverlay(const char* winnerText, Color winnerColor, int screenW, int screenH);

#endif // RENDER_H
