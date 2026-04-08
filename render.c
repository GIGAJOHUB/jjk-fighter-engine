// =============================================================================
// renders.c — JJK Fighter: Visual Rendering Implementation
// =============================================================================
// All draw calls, particle systems, domain effects, HUD, shake, announcements.
// =============================================================================

#include "render.h"
#include "characters.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Forward-declare Fighter struct (defined in main.c / combat.h)
// ---------------------------------------------------------------------------
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
    bool            projectileActive;
    Rectangle       projectile;
    float           projectileSpeed;
    bool            blackFlashActive; // true for a few frames on Black Flash proc
    int             blackFlashFrames;
    Color           bodyColor;
    const char*     name;
} Fighter;

// ---------------------------------------------------------------------------
// PARTICLE SYSTEM
// ---------------------------------------------------------------------------
Particle gParticles[MAX_PARTICLES];

void ParticleSpawn(Vector2 pos, Vector2 vel, Color col,
                   float life, float size, ParticleType type) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!gParticles[i].active) {
            gParticles[i].pos     = pos;
            gParticles[i].vel     = vel;
            gParticles[i].color   = col;
            gParticles[i].life    = life;
            gParticles[i].maxLife = life;
            gParticles[i].size    = size;
            gParticles[i].type    = type;
            gParticles[i].active  = true;
            return;
        }
    }
}

void ParticleSpawnBurst(Vector2 pos, int count, Color col,
                        float speed, float life, float size, ParticleType type) {
    for (int i = 0; i < count; i++) {
        float angle = ((float)GetRandomValue(0, 3600)) / 10.0f * DEG2RAD;
        float spd   = speed * (0.4f + ((float)GetRandomValue(0, 100)) / 100.0f);
        Vector2 vel = { cosf(angle) * spd, sinf(angle) * spd };
        // Slight color variation
        Color c = col;
        c.r = (unsigned char)Clamp(col.r + GetRandomValue(-30, 30), 0, 255);
        c.g = (unsigned char)Clamp(col.g + GetRandomValue(-30, 30), 0, 255);
        c.b = (unsigned char)Clamp(col.b + GetRandomValue(-30, 30), 0, 255);
        ParticleSpawn(pos, vel, c, life * (0.6f + ((float)GetRandomValue(0,40))/100.0f), size, type);
    }
}

void ParticleUpdate(void) {
    float dt = GetFrameTime();
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!gParticles[i].active) continue;
        gParticles[i].pos.x += gParticles[i].vel.x;
        gParticles[i].pos.y += gParticles[i].vel.y;
        // Gravity for sparks
        if (gParticles[i].type == PARTICLE_SPARK ||
            gParticles[i].type == PARTICLE_HIT_BURST) {
            gParticles[i].vel.y += 0.18f;
        }
        // Slow down CE motes
        if (gParticles[i].type == PARTICLE_CE_MOTE) {
            gParticles[i].vel.x *= 0.96f;
            gParticles[i].vel.y *= 0.96f;
        }
        gParticles[i].life -= dt;
        if (gParticles[i].life <= 0) gParticles[i].active = false;
    }
}

void ParticleDraw(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!gParticles[i].active) continue;
        float alpha = gParticles[i].life / gParticles[i].maxLife;
        Color c = gParticles[i].color;
        c.a = (unsigned char)(alpha * 255.0f);
        float sz = gParticles[i].size * alpha;

        switch (gParticles[i].type) {
            case PARTICLE_DOMAIN_SHARD:
            case PARTICLE_DOMAIN_ANNOUNCE: {
                // Diamond shard
                Vector2 p = gParticles[i].pos;
                DrawPoly(p, 4, sz, 45.0f, c);
                break;
            }
            case PARTICLE_BLACK_FLASH: {
                // Bright cross
                Vector2 p = gParticles[i].pos;
                DrawRectangle((int)(p.x - sz/2), (int)(p.y - 2), (int)sz, 4, c);
                DrawRectangle((int)(p.x - 2), (int)(p.y - sz/2), 4, (int)sz, c);
                break;
            }
            case PARTICLE_REGEN: {
                // Small plus
                Vector2 p = gParticles[i].pos;
                DrawRectangle((int)(p.x - sz/2), (int)(p.y - 1), (int)sz, 2, c);
                DrawRectangle((int)(p.x - 1), (int)(p.y - sz/2), 2, (int)sz, c);
                break;
            }
            default:
                DrawCircleV(gParticles[i].pos, sz, c);
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// SCREEN SHAKE
// ---------------------------------------------------------------------------
ScreenShake gShake = {0};

void ShakeTrigger(float intensity, float duration) {
    gShake.intensity = intensity;
    gShake.duration  = duration;
    gShake.timer     = duration;
}

Vector2 ShakeOffset(void) {
    if (gShake.timer <= 0.0f) return (Vector2){0,0};
    float t = gShake.timer / gShake.duration;
    float strength = gShake.intensity * t;
    return (Vector2){
        (float)GetRandomValue(-1, 1) * strength,
        (float)GetRandomValue(-1, 1) * strength
    };
}

void ShakeUpdate(void) {
    float dt = GetFrameTime();
    if (gShake.timer > 0) gShake.timer -= dt;
    if (gShake.timer < 0) gShake.timer = 0;
}

// ---------------------------------------------------------------------------
// DOMAIN ANNOUNCEMENT
// ---------------------------------------------------------------------------
DomainAnnounce gDomainAnnounce = {0};

void AnnounceStart(const char* domainName, const char* casterName, Color col) {
    snprintf(gDomainAnnounce.text, 64, "%s", domainName);
    gDomainAnnounce.color   = col;
    gDomainAnnounce.maxTime = 2.5f;
    gDomainAnnounce.timer   = 2.5f;
    gDomainAnnounce.active  = true;
    gDomainAnnounce.flash   = 0;
}

void AnnounceUpdate(void) {
    if (!gDomainAnnounce.active) return;
    float dt = GetFrameTime();
    gDomainAnnounce.timer -= dt;
    gDomainAnnounce.flash++;
    if (gDomainAnnounce.timer <= 0) gDomainAnnounce.active = false;
}

void AnnounceDraw(int screenW, int screenH) {
    if (!gDomainAnnounce.active) return;

    float alpha = gDomainAnnounce.timer / gDomainAnnounce.maxTime;
    // Flash effect: blink during last 0.8s
    if (gDomainAnnounce.timer < 0.8f && (gDomainAnnounce.flash / 4) % 2 == 0) return;

    // Large background banner
    Color bannerBg = gDomainAnnounce.color;
    bannerBg.a = (unsigned char)(alpha * 160.0f);
    DrawRectangle(0, screenH/2 - 70, screenW, 140, bannerBg);

    // Scanline texture on banner
    for (int y = screenH/2 - 70; y < screenH/2 + 70; y += 4) {
        DrawRectangle(0, y, screenW, 1, (Color){0,0,0,(unsigned char)(alpha*60)});
    }

    // "DOMAIN EXPANSION!" label
    const char* label = "DOMAIN EXPANSION!";
    int labelSz = 36;
    int lw = MeasureText(label, labelSz);
    Color labelCol = WHITE;
    labelCol.a = (unsigned char)(alpha * 255.0f);
    DrawText(label, screenW/2 - lw/2, screenH/2 - 58, labelSz, labelCol);

    // Domain name
    int nameSz = 28;
    int nw = MeasureText(gDomainAnnounce.text, nameSz);
    Color nameCol = gDomainAnnounce.color;
    nameCol.r = (unsigned char)Clamp(nameCol.r + 80, 0, 255);
    nameCol.g = (unsigned char)Clamp(nameCol.g + 80, 0, 255);
    nameCol.b = (unsigned char)Clamp(nameCol.b + 80, 0, 255);
    nameCol.a = (unsigned char)(alpha * 255.0f);
    DrawText(gDomainAnnounce.text, screenW/2 - nw/2, screenH/2 - 10, nameSz, nameCol);

    // "COUNTER NOW!" sub-label
    const char* counterLabel = "—  COUNTER NOW OR FACE SURE-HIT  —";
    int clw = MeasureText(counterLabel, 16);
    Color cCol = (Color){255, 255, 100, (unsigned char)(alpha * 200.0f)};
    DrawText(counterLabel, screenW/2 - clw/2, screenH/2 + 28, 16, cCol);
}

// ---------------------------------------------------------------------------
// DOMAIN BACKGROUND EFFECTS
// ---------------------------------------------------------------------------

// Helper: draw tiling slashes (Malevolent Shrine)
static void DrawShrineSlashes(float timer, int screenW, int screenH) {
    float t = (float)GetTime();
    for (int i = 0; i < 8; i++) {
        float x1 = fmodf(t * 60.0f + i * 130.0f, (float)(screenW + 200)) - 100.0f;
        float angle = -35.0f + (float)(i % 3) * 8.0f;
        float rad   = angle * DEG2RAD;
        float len   = 400.0f;
        float x2    = x1 + cosf(rad) * len;
        float y1    = -50.0f;
        float y2    = y1 + sinf(rad) * len;
        Color slashCol = { 180, 20, 20, 80 };
        DrawLineEx((Vector2){x1, y1}, (Vector2){x2, screenH + 50.0f}, 2.0f, slashCol);
    }
}

// Helper: draw void rings (Unlimited Void)
static void DrawVoidRings(int screenW, int screenH) {
    double t = GetTime();
    for (int i = 0; i < 16; i++) {
        float r   = (float)(i * 28) + (float)(fmod(t * 18.0, 28.0));
        float alp = 1.0f - (r / 450.0f);
        if (alp < 0) alp = 0;
        Color rc = { 60, 80, 255, (unsigned char)(alp * 90.0f) };
        DrawCircleLines(screenW/2, screenH/2, r, rc);
    }
    // Stars
    for (int i = 0; i < 80; i++) {
        int sx = (i * 137 + 7) % screenW;
        int sy = (i * 211 + 13) % screenH;
        float blink = 0.4f + 0.6f * sinf((float)GetTime() * 2.0f + i * 0.7f);
        Color sc = { 200, 220, 255, (unsigned char)(blink * 160.0f) };
        DrawCircle(sx, sy, 1.5f, sc);
    }
}

// Helper: draw Rika domain (Authentic Mutual Love)
static void DrawRikaDomain(int screenW, int screenH) {
    double t = GetTime();
    for (int i = 0; i < 6; i++) {
        float angle = (float)(t * 0.5 + i * (3.14159 * 2.0 / 6.0));
        float r1 = 80.0f + 30.0f * sinf((float)t + i);
        float x  = screenW/2 + cosf(angle) * r1;
        float y  = screenH/2 + sinf(angle) * r1 * 0.5f;
        Color c  = { 255, 150, 255, 60 };
        DrawCircle((int)x, (int)y, 18.0f + 6.0f * sinf((float)t * 2 + i), c);
    }
    // Central glow
    Color glow = { 200, 80, 255, 30 };
    DrawCircle(screenW/2, screenH/2, 120, glow);
}

void DrawDomainBackground(CharacterID casterID, float timer, int screenW, int screenH) {
    switch (casterID) {

        case CHAR_SUKUNA:
            // Malevolent Shrine — deep blood void + animated slashes
            ClearBackground((Color){18, 0, 0, 255});
            // Radial dark-red gradient effect
            for (int r = 300; r > 0; r -= 30) {
                float alp = 1.0f - (float)r / 300.0f;
                DrawCircle(screenW/2, screenH/2, (float)r,
                           (Color){60, 0, 0, (unsigned char)(alp * 40.0f)});
            }
            DrawShrineSlashes(timer, screenW, screenH);
            break;

        case CHAR_GOJO:
            // Unlimited Void — absolute black + expanding ring lattice
            ClearBackground((Color){2, 2, 12, 255});
            DrawVoidRings(screenW, screenH);
            break;

        case CHAR_YUTA:
            // Authentic Mutual Love — deep violet + Rika orbs
            ClearBackground((Color){12, 0, 22, 255});
            DrawRikaDomain(screenW, screenH);
            break;

        default:
            // Generic domain fallback
            ClearBackground((Color){8, 8, 18, 255});
            break;
    }

    // Vignette overlay (always)
    for (int r = 600; r > 0; r -= 60) {
        float alp = (float)r / 600.0f * 0.35f;
        DrawCircle(screenW/2, screenH/2, (float)r,
                   (Color){0, 0, 0, (unsigned char)(alp * 255.0f * 0)});
    }
    // Corner vignette
    DrawRectangleGradientEx(
        (Rectangle){0, 0, (float)screenW, (float)screenH},
        (Color){0,0,0,120}, (Color){0,0,0,0},
        (Color){0,0,0,0},   (Color){0,0,0,120}
    );
}

// ---------------------------------------------------------------------------
// ARENA
// ---------------------------------------------------------------------------
void DrawArena(int screenW, int screenH, float floorY) {
    // Sky gradient
    DrawRectangleGradientV(0, 0, screenW, (int)floorY,
                           (Color){12, 8, 22, 255},
                           (Color){25, 15, 40, 255});

    // Background pillars (depth effect)
    for (int i = 0; i < 7; i++) {
        int px = 60 + i * 130;
        int ph = 80 + (i % 3) * 40;
        DrawRectangle(px, (int)floorY - ph, 30, ph, (Color){35, 20, 50, 200});
        DrawRectangle(px + 2, (int)floorY - ph + 2, 26, ph - 2, (Color){45, 28, 62, 150});
    }

    // Floor base slab
    DrawRectangle(0, (int)floorY, screenW, screenH - (int)floorY,
                  (Color){20, 12, 32, 255});

    // Floor top edge with hex/rune pattern
    for (int i = 0; i < screenW; i += 40) {
        Color runeCol = { 80, 50, 120, 60 };
        DrawRectangle(i, (int)floorY, 2, 6, runeCol);
    }

    // Floor highlight
    DrawRectangle(0, (int)floorY, screenW, 3, (Color){120, 80, 200, 180});
    DrawRectangle(0, (int)floorY + 3, screenW, 1, (Color){60, 40, 100, 100});

    // Ambient floor glow
    DrawRectangleGradientV(0, (int)floorY - 40, screenW, 40,
                           (Color){80, 40, 140, 0},
                           (Color){80, 40, 140, 40});
}

// ---------------------------------------------------------------------------
// FIGHTER RENDERING
// ---------------------------------------------------------------------------
void DrawFighterBody(Fighter* f, bool isP1) {
    if (f->hp <= 0) return;

    Vector2 shake = ShakeOffset();
    float bx = f->hitbox.x + shake.x;
    float by = f->hitbox.y + shake.y;
    float bw = f->hitbox.width;
    float bh = f->hitbox.height;

    // Dodge afterimage
    if (f->isDodging) {
        Color ghost = f->bodyColor;
        ghost.a = 60;
        DrawRectangle((int)(bx + (f->facingDir == 1 ? -20 : 20)), (int)by,
                      (int)bw, (int)bh, ghost);
    }

    // CE glow outline (not Toji)
    if (!f->isHeavenlyRestricted && f->cursedEnergy > f->maxCE * 0.3f) {
        Color glow = f->charData.ceColor;
        glow.a = 60;
        float pulse = 1.0f + 0.08f * sinf((float)GetTime() * 4.0f);
        DrawRectangle((int)(bx - 3*pulse), (int)(by - 3*pulse),
                      (int)(bw + 6*pulse), (int)(bh + 6*pulse), glow);
    }

    // Main body
    DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, f->bodyColor);

    // Body shading — inner lighter panel
    Color highlight = f->bodyColor;
    highlight.r = (unsigned char)Clamp(highlight.r + 40, 0, 255);
    highlight.g = (unsigned char)Clamp(highlight.g + 40, 0, 255);
    highlight.b = (unsigned char)Clamp(highlight.b + 40, 0, 255);
    DrawRectangle((int)(bx + 4), (int)(by + 4), (int)(bw - 8), (int)(bh / 3), highlight);

    // Body outline
    Color outline = f->bodyColor;
    outline.r = (unsigned char)Clamp(outline.r + 80, 0, 255);
    outline.g = (unsigned char)Clamp(outline.g + 80, 0, 255);
    outline.b = (unsigned char)Clamp(outline.b + 80, 0, 255);
    DrawRectangleLines((int)bx, (int)by, (int)bw, (int)bh, outline);

    // Eyes
    float eyeXBase = (f->facingDir == 1) ? bx + bw - 18.0f : bx + 8.0f;
    DrawCircle((int)eyeXBase, (int)(by + 20), 7, WHITE);
    DrawCircle((int)(eyeXBase + f->facingDir * 3), (int)(by + 20), 4, BLACK);

    // Gojo: Six Eyes glow
    if (f->charData.traits.hasSixEyes) {
        float eyeGlowAlpha = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f);
        Color eyeGlow = { 100, 200, 255, (unsigned char)(eyeGlowAlpha * 180.0f) };
        DrawCircle((int)eyeXBase, (int)(by + 20), 10, eyeGlow);
    }

    // Attack flash
    if (f->isAttacking) {
        Color flash = f->isHeavenlyRestricted
            ? (Color){220, 220, 220, 180}   // Toji: silver physical
            : f->charData.ceColor;
        flash.a = 160;
        DrawRectangle(
            (int)(bx + (f->facingDir > 0 ? bw : -65.0f)),
            (int)(by + 20), 65, 45, flash
        );
        // Arc glow
        Color arc = flash;
        arc.a = 60;
        DrawRectangle(
            (int)(bx + (f->facingDir > 0 ? bw - 5 : -75.0f)),
            (int)(by + 12), 80, 60, arc
        );
    }

    // Black Flash visual
    if (f->blackFlashActive) {
        Color bfCol = { 20, 20, 20, 220 };
        DrawRectangle((int)bx - 6, (int)by - 6, (int)bw + 12, (int)bh + 12, bfCol);
        Color bfBright = { 255, 200, 50, 200 };
        DrawRectangleLines((int)bx - 4, (int)by - 4, (int)bw + 8, (int)bh + 8, bfBright);
        // Cross flash
        DrawRectangle((int)(bx + bw/2) - 3, (int)by - 10, 6, (int)bh + 20, bfBright);
        DrawRectangle((int)bx - 10, (int)(by + bh/2) - 3, (int)bw + 20, 6, bfBright);
    }

    // Active projectile
    if (f->projectileActive) {
        Color pCol = f->charData.ceColor;
        // Glow layers
        Color pGlow = pCol; pGlow.a = 60;
        DrawRectangle((int)f->projectile.x - 4, (int)f->projectile.y - 4,
                      (int)f->projectile.width + 8, (int)f->projectile.height + 8, pGlow);
        DrawRectangleRec(f->projectile, pCol);
        DrawRectangleLines((int)f->projectile.x, (int)f->projectile.y,
                           (int)f->projectile.width, (int)f->projectile.height, WHITE);
    }

    // Stun indicator
    if (f->isStunned) {
        const char* stunTxt = "* STUNNED *";
        int sw = MeasureText(stunTxt, 14);
        DrawText(stunTxt, (int)(bx + bw/2 - sw/2), (int)(by - 24), 14,
                 (Color){255, 255, 60, 200});
    }

    // Dodge invincibility flash
    if (f->isDodging) {
        Color dashColor = { 200, 200, 255, 80 };
        DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, dashColor);
    }
}

void DrawFighterEffects(Fighter* f) {
    // Passive CE mote emission for non-Toji
    if (!f->isHeavenlyRestricted && f->cursedEnergy > 10.0f) {
        if (GetRandomValue(0, 6) == 0) {
            Vector2 pos = {
                f->hitbox.x + (float)GetRandomValue(0, (int)f->hitbox.width),
                f->hitbox.y + (float)GetRandomValue(0, (int)f->hitbox.height)
            };
            Vector2 vel = {
                (float)GetRandomValue(-20, 20) / 10.0f,
                -1.5f - (float)GetRandomValue(0, 15) / 10.0f
            };
            Color c = f->charData.ceColor;
            c.a = 200;
            ParticleSpawn(pos, vel, c, 0.6f + (float)GetRandomValue(0,40)/100.0f,
                          3.0f, PARTICLE_CE_MOTE);
        }
    }
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------

// Draw a premium stat bar with gradient and shine
static void DrawPremiumBar(float val, float maxVal, int x, int y,
                            int barW, int barH, Color fillColor,
                            Color bgColor, bool alignRight) {
    if (maxVal <= 0) { // No CE (Toji)
        int dx = alignRight ? x - barW : x;
        DrawRectangle(dx, y, barW, barH, (Color){30,30,30,180});
        DrawRectangleLines(dx, y, barW, barH, (Color){60,60,60,200});
        return;
    }
    int filled = (int)((val / maxVal) * barW);
    if (filled < 0) filled = 0;
    if (filled > barW) filled = barW;
    int drawX = alignRight ? x - barW : x;

    // Background
    DrawRectangle(drawX, y, barW, barH, bgColor);

    // Fill
    DrawRectangle(drawX, y, filled, barH, fillColor);

    // Shine on top third
    Color shine = fillColor;
    shine.r = (unsigned char)Clamp(shine.r + 60, 0, 255);
    shine.g = (unsigned char)Clamp(shine.g + 60, 0, 255);
    shine.b = (unsigned char)Clamp(shine.b + 60, 0, 255);
    shine.a = 120;
    DrawRectangle(drawX, y, filled, barH / 3, shine);

    // Damage indicator (gray zone between filled and max)
    Color dmgCol = { 120, 0, 0, 120 };
    DrawRectangle(drawX + filled, y, barW - filled, barH, dmgCol);

    // Notches
    for (int i = 1; i < 5; i++) {
        int nx = drawX + (barW * i / 5);
        DrawRectangle(nx, y, 1, barH, (Color){0,0,0,80});
    }

    // Border
    DrawRectangleLines(drawX, y, barW, barH, (Color){180, 180, 180, 180});
}

void DrawHUD(Fighter* p1, Fighter* p2, float domainTimer,
             bool domainActive, int screenW) {

    int barW = 300, hpH = 26, ceH = 14;
    int p1X = 50, p2X = screenW - 50;

    // ---- P1 Panel ----
    // Name with title
    char p1Label[64];
    snprintf(p1Label, sizeof(p1Label), "%s", p1->charData.name);
    DrawText(p1Label, p1X, 10, 18, p1->charData.bodyColor);
    DrawText(p1->charData.fullTitle, p1X, 30, 11, (Color){160, 160, 180, 200});

    // HP bar
    DrawPremiumBar(p1->hp, p1->maxHP, p1X, 46, barW, hpH, GREEN, (Color){30,10,10,220}, false);
    char p1HPStr[16]; snprintf(p1HPStr, 16, "%.0f / %.0f", p1->hp, p1->maxHP);
    DrawText(p1HPStr, p1X + 4, 50, 12, WHITE);

    // CE bar
    DrawPremiumBar(p1->cursedEnergy, p1->maxCE, p1X, 76, barW, ceH,
                   p1->charData.ceColor, (Color){15,10,25,220}, false);

    // Trait icons
    int traitX = p1X;
    if (p1->charData.traits.hasSixEyes) {
        DrawText("[∞ SIX EYES]", traitX, 95, 10, (Color){100,200,255,220});
        traitX += MeasureText("[∞ SIX EYES]", 10) + 6;
    }
    if (p1->charData.traits.isHeavenlyRestricted)
        DrawText("[HVN RST]", traitX, 95, 10, (Color){200,200,200,220});
    if (p1->charData.traits.hasBlackFlash)
        DrawText("[BLACK FLASH]", traitX, 95, 10, (Color){255,180,30,220});
    if (p1->charData.traits.hasCopy)
        DrawText("[COPY]", traitX, 95, 10, (Color){200,140,255,220});
    if (!p1->hasDomain && !p1->isHeavenlyRestricted)
        DrawText("✗ DOMAIN LOST", p1X, 108, 10, (Color){255,60,60,255});

    // ---- P2 Panel ----
    char p2Label[64];
    snprintf(p2Label, sizeof(p2Label), "%s", p2->charData.name);
    int p2NameW = MeasureText(p2Label, 18);
    DrawText(p2Label, p2X - p2NameW, 10, 18, p2->charData.bodyColor);
    int p2TitleW = MeasureText(p2->charData.fullTitle, 11);
    DrawText(p2->charData.fullTitle, p2X - p2TitleW, 30, 11, (Color){160,160,180,200});

    DrawPremiumBar(p2->hp, p2->maxHP, p2X, 46, barW, hpH, GREEN, (Color){30,10,10,220}, true);
    char p2HPStr[16]; snprintf(p2HPStr, 16, "%.0f / %.0f", p2->hp, p2->maxHP);
    DrawText(p2HPStr, p2X - barW + 4, 50, 12, WHITE);

    DrawPremiumBar(p2->cursedEnergy, p2->maxCE, p2X, 76, barW, ceH,
                   p2->charData.ceColor, (Color){15,10,25,220}, true);

    if (p2->charData.traits.isHeavenlyRestricted) {
        int tw = MeasureText("[HVN RST]", 10);
        DrawText("[HVN RST]", p2X - tw, 95, 10, (Color){200,200,200,220});
    }
    if (!p2->hasDomain && !p2->isHeavenlyRestricted) {
        int tw = MeasureText("✗ DOMAIN LOST", 10);
        DrawText("✗ DOMAIN LOST", p2X - tw, 108, 10, (Color){255,60,60,255});
    }

    // ---- Center: Timer / VS ---
    const char* vsText = "VS";
    int vsTW = MeasureText(vsText, 24);
    DrawText(vsText, screenW/2 - vsTW/2, 30, 24, (Color){200,180,220,180});

    // Domain counter window timer
    if (domainActive && domainTimer > 0.0f) {
        char timerStr[32];
        snprintf(timerStr, sizeof(timerStr), "COUNTER: %.1fs", domainTimer);
        float flash = 0.5f + 0.5f * sinf((float)GetTime() * 8.0f);
        Color timerCol = { 255, 255, 80, (unsigned char)(flash * 255.0f) };
        int tw = MeasureText(timerStr, 20);
        DrawText(timerStr, screenW/2 - tw/2, screenW > 800 ? 115 : 110, 20, timerCol);
    }
}

// ---------------------------------------------------------------------------
// CHARACTER SELECT SCREEN
// ---------------------------------------------------------------------------
void DrawCharSelectScreen(int p1Cursor, int p2Cursor,
                          bool p1Confirmed, bool p2Confirmed,
                          int screenW, int screenH) {

    // Background
    DrawRectangleGradientV(0, 0, screenW, screenH,
                           (Color){5, 3, 15, 255}, (Color){18, 10, 32, 255});

    // Animated hex grid
    double t = GetTime();
    for (int gx = 0; gx < screenW; gx += 60) {
        for (int gy = 0; gy < screenH; gy += 52) {
            float dist = Vector2Distance((Vector2){(float)gx,(float)gy},
                                         (Vector2){(float)screenW/2.0f,(float)screenH/2.0f});
            float wave = sinf((float)(t * 1.2 + dist * 0.015));
            Color hexCol = { 40, 20, 70, (unsigned char)((wave * 0.5f + 0.5f) * 30.0f) };
            DrawCircleLines(gx, gy, 20, hexCol);
        }
    }

    // Title
    const char* title = "SELECT YOUR FIGHTER";
    int titleSize = 34;
    int tw = MeasureText(title, titleSize);
    // Shadow
    DrawText(title, screenW/2 - tw/2 + 2, 32, titleSize, (Color){100,0,180,150});
    DrawText(title, screenW/2 - tw/2, 30, titleSize, (Color){220, 180, 255, 255});

    // Decorative underline
    DrawRectangle(screenW/2 - tw/2, 68, tw, 2, (Color){150, 80, 220, 200});

    // Character slots
    int slotW = 140, slotH = 190, gap = 18;
    int totalW = CHAR_COUNT * (slotW + gap) - gap;
    int startX = (screenW - totalW) / 2;
    int slotY  = 100;

    for (int i = 0; i < CHAR_COUNT; i++) {
        CharacterData cd = GetCharacterData((CharacterID)i);
        int sx = startX + i * (slotW + gap);

        bool p1Here = (p1Cursor == i);
        bool p2Here = (p2Cursor == i);

        // Slot background with glow if selected
        Color slotBg = (Color){22, 14, 38, 230};
        if (p1Here || p2Here) slotBg = (Color){35, 18, 60, 255};
        DrawRectangle(sx, slotY, slotW, slotH, slotBg);

        // Character color panel
        Color panelCol = cd.bodyColor;
        panelCol.a = 200;
        DrawRectangle(sx + 8, slotY + 8, slotW - 16, 95, panelCol);
        // Shine
        Color panelShine = panelCol;
        panelShine.r = (unsigned char)Clamp(panelShine.r + 60, 0, 255);
        panelShine.g = (unsigned char)Clamp(panelShine.g + 60, 0, 255);
        panelShine.b = (unsigned char)Clamp(panelShine.b + 60, 0, 255);
        panelShine.a = 80;
        DrawRectangle(sx + 8, slotY + 8, slotW - 16, 30, panelShine);

        // Fighter silhouette (stylized rectangle person)
        int figX = sx + slotW/2 - 12;
        int figY = slotY + 20;
        DrawRectangle(figX, figY, 24, 32, (Color){0,0,0,120});   // body
        DrawCircle(figX + 12, figY - 8, 12, (Color){0,0,0,120});  // head

        // Border: P1 = blue, P2 = red, both = yellow pulse, unselected = dark
        Color borderCol = (Color){50, 30, 80, 180};
        float pulseAlpha = 0.7f + 0.3f * sinf((float)GetTime() * 4.0f);
        if (p1Here && p2Here) borderCol = (Color){255, 220, 50, (unsigned char)(pulseAlpha*255)};
        else if (p1Here)      borderCol = (Color){80,  140, 255, (unsigned char)(pulseAlpha*255)};
        else if (p2Here)      borderCol = (Color){255, 80,  80,  (unsigned char)(pulseAlpha*255)};
        DrawRectangleLines(sx, slotY, slotW, slotH, borderCol);
        // Double border on selected
        if (p1Here || p2Here) DrawRectangleLines(sx+1, slotY+1, slotW-2, slotH-2, borderCol);

        // Name
        int nameSz = 15;
        int nw = MeasureText(cd.name, nameSz);
        DrawText(cd.name, sx + slotW/2 - nw/2, slotY + 108, nameSz, WHITE);

        // Full title
        int titleFontSz = 9;
        int ftw = MeasureText(cd.fullTitle, titleFontSz);
        DrawText(cd.fullTitle, sx + slotW/2 - ftw/2, slotY + 126,
                 titleFontSz, (Color){160, 160, 180, 200});

        // Stats mini-bar (HP)
        float hpPct = cd.maxHP / 250.0f;
        DrawRectangle(sx + 8, slotY + 142, (int)((slotW - 16) * hpPct), 4, GREEN);
        DrawRectangleLines(sx + 8, slotY + 142, slotW - 16, 4, (Color){80,80,80,180});

        // Stats mini-bar (CE)
        if (cd.maxCE > 0) {
            float cePct = cd.maxCE / 500.0f;
            DrawRectangle(sx + 8, slotY + 150, (int)((slotW - 16) * cePct), 4, cd.ceColor);
            DrawRectangleLines(sx + 8, slotY + 150, slotW - 16, 4, (Color){80,80,80,180});
        } else {
            DrawText("HEAVENLY", sx + 8, slotY + 148, 8, GOLD);
        }

        // Domain name
        if (cd.hasDomain) {
            int dnw = MeasureText(cd.domainName, 8);
            DrawText(cd.domainName, sx + slotW/2 - dnw/2, slotY + 162, 8,
                     (Color){200, 150, 255, 200});
        } else {
            int noDomW = MeasureText("No Domain", 8);
            DrawText("No Domain", sx + slotW/2 - noDomW/2, slotY + 162, 8,
                     (Color){120, 120, 120, 180});
        }

        // P1 / P2 confirmed indicators
        if (p1Confirmed && p1Cursor == i) {
            DrawText("P1", sx + 6, slotY + slotH - 18, 14,
                     (Color){80, 140, 255, 255});
        }
        if (p2Confirmed && p2Cursor == i) {
            int p2tw = MeasureText("P2", 14);
            DrawText("P2", sx + slotW - p2tw - 6, slotY + slotH - 18, 14,
                     (Color){255, 80, 80, 255});
        }
    }

    // Controls legend
    DrawText("P1: A/D  SPACE=Select", 20, screenH - 55, 12, (Color){120, 120, 180, 220});
    DrawText("P2: ←/→  ENTER=Select", 20, screenH - 35, 12, (Color){180, 100, 100, 220});

    // Bottom: both confirmed message
    if (p1Confirmed && p2Confirmed) {
        const char* readyMsg = "BOTH READY — FIGHT BEGINS!";
        int rmw = MeasureText(readyMsg, 22);
        float rPulse = 0.5f + 0.5f * sinf((float)GetTime() * 6.0f);
        Color rCol = { 255, 220, 50, (unsigned char)(rPulse * 255.0f) };
        DrawText(readyMsg, screenW/2 - rmw/2, screenH - 70, 22, rCol);
    }
}

// ---------------------------------------------------------------------------
// BATTLE BACKGROUND
// ---------------------------------------------------------------------------
void DrawBattleBackground(int screenW, int screenH) {
    // Handled by DrawArena — this is for any pre-arena BG pass
    // Dark gradient sky
    DrawRectangleGradientV(0, 0, screenW, screenH,
                           (Color){8, 5, 18, 255}, (Color){18, 12, 30, 255});
}

// ---------------------------------------------------------------------------
// GAME OVER OVERLAY
// ---------------------------------------------------------------------------
void DrawGameOverOverlay(const char* winnerText, Color winnerColor,
                         int screenW, int screenH) {
    // Darken overlay
    DrawRectangle(0, 0, screenW, screenH, (Color){0, 0, 0, 160});

    // Dramatic banner
    int bannerH = 160;
    DrawRectangle(0, screenH/2 - bannerH/2, screenW, bannerH,
                  (Color){10, 5, 20, 220});
    DrawRectangle(0, screenH/2 - bannerH/2, screenW, 3, winnerColor);
    DrawRectangle(0, screenH/2 + bannerH/2 - 3, screenW, 3, winnerColor);

    // Winner text
    int fontSize = 52;
    int tw = MeasureText(winnerText, fontSize);
    // Shadow
    DrawText(winnerText, screenW/2 - tw/2 + 3, screenH/2 - 44, fontSize,
             (Color){0,0,0,180});
    DrawText(winnerText, screenW/2 - tw/2, screenH/2 - 46, fontSize, winnerColor);

    // Subtext
    const char* rematch = "PRESS ENTER TO REMATCH";
    float blink = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f);
    Color blinkCol = { 200, 200, 200, (unsigned char)(blink * 220.0f) };
    int rtw = MeasureText(rematch, 18);
    DrawText(rematch, screenW/2 - rtw/2, screenH/2 + 30, 18, blinkCol);
}