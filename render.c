#include "render.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FLOOR_Y 400.0f

Particle gParticles[MAX_PARTICLES];
ScreenShake gShake = {0};
DomainAnnounce gDomainAnnounce = {0};
static Font gUiFont = {0};
static bool gUiFontLoaded = false;
static Texture2D gGojoPortrait = {0};
static bool gGojoPortraitLoaded = false;

static Vector2 UiMeasure(const char* text, float fontSize, float spacing) {
    if (gUiFontLoaded) return MeasureTextEx(gUiFont, text, fontSize, spacing);
    return (Vector2){ (float)MeasureText(text, (int)fontSize), fontSize };
}

static void UiText(const char* text, Vector2 pos, float fontSize, float spacing, Color color) {
    if (gUiFontLoaded) DrawTextEx(gUiFont, text, pos, fontSize, spacing, color);
    else DrawText(text, (int)pos.x, (int)pos.y, (int)fontSize, color);
}

void SetUIFont(Font font, bool loaded) {
    gUiFont = font;
    gUiFontLoaded = loaded;
}

void SetGojoPortrait(Texture2D portrait, bool loaded) {
    gGojoPortrait = portrait;
    gGojoPortraitLoaded = loaded;
}

static Color Lighten(Color c, int amount) {
    c.r = (unsigned char)Clamp(c.r + amount, 0, 255);
    c.g = (unsigned char)Clamp(c.g + amount, 0, 255);
    c.b = (unsigned char)Clamp(c.b + amount, 0, 255);
    return c;
}

void ParticleSpawn(Vector2 pos, Vector2 vel, Color col, float life, float size, ParticleType type) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!gParticles[i].active) {
            gParticles[i].pos = pos;
            gParticles[i].vel = vel;
            gParticles[i].color = col;
            gParticles[i].life = life;
            gParticles[i].maxLife = life;
            gParticles[i].size = size;
            gParticles[i].type = type;
            gParticles[i].active = true;
            return;
        }
    }
}

void ParticleSpawnBurst(Vector2 pos, int count, Color col, float speed, float life, float size, ParticleType type) {
    for (int i = 0; i < count; i++) {
        float angle = ((float)GetRandomValue(0, 3600) / 10.0f) * DEG2RAD;
        float mag = speed * (0.45f + (float)GetRandomValue(0, 100) / 100.0f);
        Vector2 vel = { cosf(angle) * mag, sinf(angle) * mag };
        Color c = col;
        c.r = (unsigned char)Clamp(c.r + GetRandomValue(-30, 30), 0, 255);
        c.g = (unsigned char)Clamp(c.g + GetRandomValue(-30, 30), 0, 255);
        c.b = (unsigned char)Clamp(c.b + GetRandomValue(-30, 30), 0, 255);
        ParticleSpawn(pos, vel, c, life * (0.7f + (float)GetRandomValue(0, 30) / 100.0f), size, type);
    }
}

void ParticleUpdate(void) {
    float dt = GetFrameTime();
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!gParticles[i].active) continue;
        gParticles[i].pos.x += gParticles[i].vel.x;
        gParticles[i].pos.y += gParticles[i].vel.y;

        if (gParticles[i].type == PARTICLE_SPARK ||
            gParticles[i].type == PARTICLE_HIT_BURST ||
            gParticles[i].type == PARTICLE_SLASH_TRAIL) {
            gParticles[i].vel.y += 0.12f;
        }

        if (gParticles[i].type == PARTICLE_CE_MOTE ||
            gParticles[i].type == PARTICLE_HOLLOW_PURPLE) {
            gParticles[i].vel.x *= 0.97f;
            gParticles[i].vel.y *= 0.97f;
        }

        gParticles[i].life -= dt;
        if (gParticles[i].life <= 0.0f) gParticles[i].active = false;
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
            case PARTICLE_DOMAIN_ANNOUNCE:
                DrawPoly(gParticles[i].pos, 4, sz, 45.0f, c);
                break;

            case PARTICLE_BLACK_FLASH:
                DrawRectangle((int)(gParticles[i].pos.x - sz * 0.5f), (int)(gParticles[i].pos.y - 2), (int)sz, 4, c);
                DrawRectangle((int)(gParticles[i].pos.x - 2), (int)(gParticles[i].pos.y - sz * 0.5f), 4, (int)sz, c);
                break;

            case PARTICLE_REGEN:
                DrawRectangle((int)(gParticles[i].pos.x - sz * 0.5f), (int)(gParticles[i].pos.y - 1), (int)sz, 2, c);
                DrawRectangle((int)(gParticles[i].pos.x - 1), (int)(gParticles[i].pos.y - sz * 0.5f), 2, (int)sz, c);
                break;

            case PARTICLE_CLASH_ARC:
                DrawCircleLines((int)gParticles[i].pos.x, (int)gParticles[i].pos.y, sz, c);
                break;

            case PARTICLE_HOLLOW_PURPLE:
                DrawCircleV(gParticles[i].pos, sz * 1.3f, ColorAlpha(c, alpha * 0.25f));
                DrawCircleV(gParticles[i].pos, sz, c);
                break;

            case PARTICLE_SLASH_TRAIL:
                DrawLineEx(
                    (Vector2){ gParticles[i].pos.x - sz, gParticles[i].pos.y - sz * 0.35f },
                    (Vector2){ gParticles[i].pos.x + sz, gParticles[i].pos.y + sz * 0.35f },
                    2.5f, c
                );
                break;

            default:
                DrawCircleV(gParticles[i].pos, sz, c);
                break;
        }
    }
}

void ShakeTrigger(float intensity, float duration) {
    gShake.intensity = intensity;
    gShake.duration = duration;
    gShake.timer = duration;
}

Vector2 ShakeOffset(void) {
    if (gShake.timer <= 0.0f) return (Vector2){0, 0};
    float t = gShake.timer / gShake.duration;
    float strength = gShake.intensity * t;
    return (Vector2){
        (float)GetRandomValue(-100, 100) / 100.0f * strength,
        (float)GetRandomValue(-100, 100) / 100.0f * strength
    };
}

void ShakeUpdate(void) {
    if (gShake.timer > 0.0f) {
        gShake.timer -= GetFrameTime();
        if (gShake.timer < 0.0f) gShake.timer = 0.0f;
    }
}

void AnnounceStart(const char* domainName, const char* casterName, Color col) {
    snprintf(gDomainAnnounce.text, sizeof(gDomainAnnounce.text), "%s", domainName);
    snprintf(gDomainAnnounce.subtext, sizeof(gDomainAnnounce.subtext),
             "%s opened a 5 second counter window", casterName);
    gDomainAnnounce.color = col;
    gDomainAnnounce.maxTime = 1.8f;
    gDomainAnnounce.timer = 1.8f;
    gDomainAnnounce.active = true;
    gDomainAnnounce.flash = 0;
}

void AnnounceUpdate(void) {
    if (!gDomainAnnounce.active) return;
    gDomainAnnounce.timer -= GetFrameTime();
    gDomainAnnounce.flash++;
    if (gDomainAnnounce.timer <= 0.0f) gDomainAnnounce.active = false;
}

void AnnounceDraw(int screenW, int screenH) {
    if (!gDomainAnnounce.active) return;

    float alpha = gDomainAnnounce.timer / gDomainAnnounce.maxTime;
    Color banner = gDomainAnnounce.color;
    banner.a = (unsigned char)(alpha * 185.0f);

    DrawRectangle(0, screenH / 2 - 74, screenW, 148, ColorAlpha(BLACK, 0.45f));
    DrawRectangle(0, screenH / 2 - 68, screenW, 136, banner);
    for (int y = screenH / 2 - 68; y < screenH / 2 + 68; y += 6) {
        DrawRectangle(0, y, screenW, 2, ColorAlpha(BLACK, 0.12f));
    }

    const char* label = "DOMAIN EXPANSION";
    int labelSize = 34;
    int lw = (int)UiMeasure(label, (float)labelSize, 1.0f).x;
    UiText(label, (Vector2){ (float)(screenW / 2 - lw / 2), (float)(screenH / 2 - 54) }, (float)labelSize, 1.0f, WHITE);

    int nameSize = 28;
    int nw = (int)UiMeasure(gDomainAnnounce.text, (float)nameSize, 1.0f).x;
    UiText(gDomainAnnounce.text, (Vector2){ (float)(screenW / 2 - nw / 2), (float)(screenH / 2 - 10) }, (float)nameSize, 1.0f, Lighten(gDomainAnnounce.color, 80));

    int sw = (int)UiMeasure(gDomainAnnounce.subtext, 16.0f, 1.0f).x;
    UiText(gDomainAnnounce.subtext, (Vector2){ (float)(screenW / 2 - sw / 2), (float)(screenH / 2 + 28) }, 16.0f, 1.0f, (Color){255, 240, 170, (unsigned char)(alpha * 255.0f)});
}

static void DrawStars(int screenW, int screenH, Color color, float speed) {
    for (int i = 0; i < 70; i++) {
        float x = fmodf((float)(i * 137) + (float)GetTime() * speed, (float)screenW);
        float y = (float)((i * 211) % screenH);
        float pulse = 0.4f + 0.6f * sinf((float)GetTime() * 2.0f + i);
        DrawCircle((int)x, (int)y, 1.0f + pulse, ColorAlpha(color, 0.35f + pulse * 0.3f));
    }
}

static void DrawSukunaDomain(float timer, int screenW, int screenH) {
    float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f);
    ClearBackground((Color){18, 0, 0, 255});
    DrawRectangleGradientV(0, 0, screenW, screenH, (Color){10, 0, 0, 255}, (Color){65, 0, 0, 255});
    for (int i = 0; i < 14; i++) {
        float x = fmodf((float)GetTime() * 160.0f + i * 90.0f, (float)(screenW + 180)) - 90.0f;
        DrawLineEx((Vector2){x, -20.0f}, (Vector2){x + 200.0f, (float)screenH + 20.0f}, 2.0f + pulse * 2.0f, (Color){255, 70, 70, 70});
    }
    DrawCircle(screenW / 2, screenH / 2, 120.0f + pulse * 40.0f, (Color){140, 0, 0, 40});
    DrawCircleLines(screenW / 2, screenH / 2, 160.0f + pulse * 25.0f, (Color){255, 110, 110, 90});
}

static void DrawGojoDomain(int screenW, int screenH) {
    ClearBackground((Color){2, 2, 14, 255});
    DrawRectangleGradientV(0, 0, screenW, screenH, (Color){0, 0, 18, 255}, (Color){6, 6, 30, 255});
    for (int i = 0; i < 18; i++) {
        float r = 24.0f + i * 26.0f + fmodf((float)GetTime() * 22.0f, 26.0f);
        DrawCircleLines(screenW / 2, screenH / 2, r, (Color){100, 140, 255, (unsigned char)(120 - i * 5)});
    }
    DrawCircleGradient(screenW / 2, screenH / 2, 180, ColorAlpha((Color){80, 130, 255, 255}, 0.18f), ColorAlpha(BLACK, 0.0f));
    DrawStars(screenW, screenH, (Color){210, 230, 255, 255}, 8.0f);
}

static void DrawYutaDomain(int screenW, int screenH) {
    ClearBackground((Color){12, 0, 22, 255});
    DrawRectangleGradientV(0, 0, screenW, screenH, (Color){18, 0, 30, 255}, (Color){40, 0, 64, 255});
    for (int i = 0; i < 8; i++) {
        float angle = (float)GetTime() * 0.6f + i * 0.7f;
        float x = screenW / 2 + cosf(angle) * (90.0f + i * 16.0f);
        float y = screenH / 2 + sinf(angle) * (55.0f + i * 10.0f);
        DrawCircle((int)x, (int)y, 18.0f + 5.0f * sinf((float)GetTime() * 1.8f + i), (Color){255, 160, 255, 65});
    }
    DrawCircleGradient(screenW / 2, screenH / 2, 150, (Color){200, 80, 255, 55}, (Color){20, 0, 35, 0});
}

void DrawDomainBackground(CharacterID casterID, float timer, int screenW, int screenH) {
    switch (casterID) {
        case CHAR_SUKUNA: DrawSukunaDomain(timer, screenW, screenH); break;
        case CHAR_GOJO: DrawGojoDomain(screenW, screenH); break;
        case CHAR_YUTA: DrawYutaDomain(screenW, screenH); break;
        default:
            ClearBackground((Color){12, 8, 18, 255});
            DrawRectangleGradientV(0, 0, screenW, screenH, (Color){20, 15, 30, 255}, (Color){50, 35, 70, 255});
            break;
    }
}

void DrawDomainClashScene(Fighter* p1, Fighter* p2, float timer, float duration,
                          int winnerPlayer, float clashDamage, int screenW, int screenH) {
    float t = (duration > 0.0f) ? timer / duration : 0.0f;
    ClearBackground((Color){6, 4, 10, 255});
    DrawRectangleGradientH(0, 0, screenW / 2, screenH,
                           ColorAlpha(p1->charData.domainAccentColor, 0.55f),
                           ColorAlpha(BLACK, 0.15f));
    DrawRectangleGradientH(screenW / 2, 0, screenW / 2, screenH,
                           ColorAlpha(BLACK, 0.15f),
                           ColorAlpha(p2->charData.domainAccentColor, 0.55f));
    DrawCircleGradient(screenW / 2, screenH / 2, 170.0f + (1.0f - t) * 40.0f,
                       ColorAlpha((Color){255, 220, 130, 255}, 0.25f),
                       ColorAlpha((Color){20, 10, 10, 255}, 0.0f));
    for (int i = 0; i < 10; i++) {
        float r = 40.0f + i * 22.0f + sinf((float)GetTime() * 4.0f + i) * 8.0f;
        DrawCircleLines(screenW / 2, screenH / 2, r, (Color){255, 220, 120, (unsigned char)(140 - i * 10)});
    }

    const char* title = "DOMAIN CLASH";
    int tw = (int)UiMeasure(title, 44.0f, 1.0f).x;
    UiText(title, (Vector2){ (float)(screenW / 2 - tw / 2), 42.0f }, 44.0f, 1.0f, WHITE);

    char result[96];
    if (winnerPlayer == 0) snprintf(result, sizeof(result), "Perfect collision - no sure hit");
    else snprintf(result, sizeof(result), "Player %d overwhelms the clash", winnerPlayer);
    int rw = (int)UiMeasure(result, 18.0f, 1.0f).x;
    UiText(result, (Vector2){ (float)(screenW / 2 - rw / 2), 96.0f }, 18.0f, 1.0f, (Color){255, 235, 170, 255});

    char dmg[64];
    snprintf(dmg, sizeof(dmg), "Damage on resolve: %.0f", clashDamage);
    int dw = (int)UiMeasure(dmg, 22.0f, 1.0f).x;
    UiText(dmg, (Vector2){ (float)(screenW / 2 - dw / 2), 126.0f }, 22.0f, 1.0f, (Color){255, 255, 255, 220});
}

void DrawArena(int screenW, int screenH, float floorY) {
    DrawRectangleGradientV(0, 0, screenW, (int)floorY, (Color){9, 14, 28, 255}, (Color){32, 16, 48, 255});
    DrawCircleGradient(screenW - 170, 95, 86, (Color){240, 245, 255, 70}, (Color){240, 245, 255, 0});

    for (int i = 0; i < 6; i++) {
        int x = 70 + i * 150;
        int h = 120 + (i % 3) * 35;
        DrawRectangle(x, (int)floorY - h, 38, h, (Color){28, 20, 45, 220});
        DrawRectangle(x + 4, (int)floorY - h + 8, 30, h - 8, (Color){48, 30, 68, 160});
    }

    DrawRectangle(0, (int)floorY, screenW, screenH - (int)floorY, (Color){20, 14, 32, 255});
    DrawRectangleGradientV(0, (int)floorY - 48, screenW, 48, (Color){120, 60, 200, 0}, (Color){120, 60, 200, 55});
    DrawRectangle(0, (int)floorY, screenW, 4, (Color){160, 105, 245, 190});

    for (int x = 0; x < screenW; x += 40) DrawLine(x, (int)floorY, x, screenH, (Color){255, 255, 255, 10});
    for (int y = (int)floorY; y < screenH; y += 28) DrawLine(0, y, screenW, y, (Color){255, 255, 255, 12});
}

static void DrawUltimateEffect(Fighter* f) {
    if (!f->ultActive) return;

    switch (f->activeUlt) {
        case ULT_HOLLOW_PURPLE: {
            Vector2 center = { f->ultHitbox.x + f->ultHitbox.width * 0.5f, f->ultHitbox.y + f->ultHitbox.height * 0.5f };
            float pulse = 1.0f + 0.2f * sinf((float)GetTime() * 8.0f);
            DrawCircleV(center, 30.0f * pulse, ColorAlpha((Color){100, 150, 255, 255}, 0.22f));
            DrawCircleV(center, 24.0f * pulse, ColorAlpha((Color){255, 70, 90, 255}, 0.18f));
            DrawCircleV(center, 18.0f * pulse, (Color){180, 70, 255, 240});
            DrawCircleLines((int)center.x, (int)center.y, 26.0f * pulse, WHITE);
            break;
        }

        case ULT_DISMANTLE_CLEAVE:
            for (int i = 0; i < 4; i++) {
                float y = f->ultHitbox.y + 10.0f + i * 16.0f;
                DrawLineEx((Vector2){ f->ultHitbox.x, y }, (Vector2){ f->ultHitbox.x + f->ultHitbox.width, y + 12.0f }, 3.0f, (Color){255, 100, 100, 220});
            }
            break;

        case ULT_BLACK_FLASH:
            DrawRectangleRounded(f->ultHitbox, 0.25f, 8, (Color){25, 25, 25, 210});
            DrawRectangleRoundedLines(f->ultHitbox, 0.25f, 8, (Color){255, 200, 70, 230});
            break;

        case ULT_HEAVENLY_ASSAULT:
            DrawRectangleRounded(f->ultHitbox, 0.18f, 8, (Color){210, 210, 220, 180});
            DrawRectangleRoundedLines(f->ultHitbox, 0.18f, 8, WHITE);
            break;

        case ULT_NONE:
        default:
            break;
    }
}

void DrawFighterBody(Fighter* f, bool isP1) {
    if (f->hp <= 0.0f) return;

    Vector2 shake = ShakeOffset();
    float bx = f->hitbox.x + shake.x;
    float by = f->hitbox.y + shake.y;
    float bw = f->hitbox.width;
    float bh = f->hitbox.height;
    float pulse = 1.0f + 0.05f * sinf((float)GetTime() * 4.0f + (isP1 ? 0.0f : 1.5f));

    DrawEllipse((int)(bx + bw * 0.5f), (int)(FLOOR_Y + 10.0f), 36.0f, 12.0f, (Color){0, 0, 0, 80});

    if (!f->isHeavenlyRestricted && f->cursedEnergy > f->maxCE * 0.25f) {
        DrawRectangleRounded((Rectangle){ bx - 8.0f, by - 8.0f, bw + 16.0f, bh + 16.0f },
                             0.18f, 8, ColorAlpha(f->charData.ceColor, 0.18f * pulse));
    }

    if (f->isDodging) {
        Color ghost = ColorAlpha(f->bodyColor, 0.25f);
        DrawRectangleRounded((Rectangle){ bx + (f->facingDir > 0 ? -18.0f : 18.0f), by + 4.0f, bw, bh }, 0.15f, 8, ghost);
    }

    DrawRectangleRounded((Rectangle){ bx, by, bw, bh }, 0.15f, 8, f->bodyColor);
    DrawRectangleRounded((Rectangle){ bx + 5.0f, by + 5.0f, bw - 10.0f, bh * 0.38f }, 0.15f, 8, Lighten(f->bodyColor, 45));
    DrawRectangleRoundedLines((Rectangle){ bx, by, bw, bh }, 0.15f, 8, Lighten(f->bodyColor, 85));

    float eyeX = (f->facingDir > 0) ? bx + bw - 16.0f : bx + 16.0f;
    DrawCircle((int)eyeX, (int)(by + 18.0f), 7, WHITE);
    DrawCircle((int)(eyeX + f->facingDir * 2), (int)(by + 18.0f), 3, BLACK);

    if (f->charData.traits.hasSixEyes) {
        DrawCircle((int)eyeX, (int)(by + 18.0f), 11, ColorAlpha((Color){100, 210, 255, 255}, 0.45f + 0.2f * sinf((float)GetTime() * 5.0f)));
    }

    if (f->isAttacking) {
        Rectangle slash = {
            bx + (f->facingDir > 0 ? bw - 4.0f : -66.0f),
            by + 20.0f, 68.0f, 40.0f
        };
        DrawRectangleRounded(slash, 0.2f, 8, ColorAlpha(f->isHeavenlyRestricted ? LIGHTGRAY : f->charData.ceColor, 0.5f));
    }

    if (f->blackFlashActive) {
        DrawRectangleRounded((Rectangle){ bx - 5.0f, by - 5.0f, bw + 10.0f, bh + 10.0f }, 0.16f, 8, (Color){12, 12, 12, 220});
        DrawRectangleRoundedLines((Rectangle){ bx - 5.0f, by - 5.0f, bw + 10.0f, bh + 10.0f }, 0.16f, 8, (Color){255, 205, 70, 220});
    }

    if (f->projectileActive) {
        Vector2 p = { f->projectile.x + f->projectile.width * 0.5f, f->projectile.y + f->projectile.height * 0.5f };
        DrawCircleV(p, 18.0f, ColorAlpha(f->charData.ceColor, 0.20f));
        DrawCircleV(p, 10.0f, f->charData.ceColor);
        DrawCircleLines((int)p.x, (int)p.y, 12.0f, WHITE);
    }

    DrawUltimateEffect(f);

    if (f->isStunned) {
        const char* txt = "STUNNED";
        int sw = MeasureText(txt, 12);
        DrawText(txt, (int)(bx + bw * 0.5f - sw * 0.5f), (int)(by - 22.0f), 12, (Color){255, 240, 80, 230});
    }

    if (f->ultReady && !f->ultUsed) {
        const char* txt = "ULT READY";
        int sw = MeasureText(txt, 12);
        DrawText(txt, (int)(bx + bw * 0.5f - sw * 0.5f), (int)(by - 38.0f), 12, (Color){255, 210, 100, 230});
    }
}

void DrawFighterEffects(Fighter* f) {
    if (!f->isHeavenlyRestricted && f->cursedEnergy > 8.0f && GetRandomValue(0, 5) == 0) {
        Vector2 pos = {
            f->hitbox.x + (float)GetRandomValue(0, (int)f->hitbox.width),
            f->hitbox.y + (float)GetRandomValue(0, (int)f->hitbox.height)
        };
        Vector2 vel = {
            (float)GetRandomValue(-12, 12) / 10.0f,
            -1.0f - (float)GetRandomValue(0, 15) / 10.0f
        };
        ParticleSpawn(pos, vel, f->charData.ceColor, 0.55f, 3.0f, PARTICLE_CE_MOTE);
    }

    if (f->ultReady && GetRandomValue(0, 4) == 0) {
        Vector2 pos = { f->hitbox.x + f->hitbox.width * 0.5f, f->hitbox.y - 4.0f };
        ParticleSpawn(pos, (Vector2){ 0.0f, -1.0f }, (Color){255, 205, 70, 255}, 0.35f, 3.0f, PARTICLE_BLACK_FLASH);
    }
}

static void DrawPremiumBar(float val, float maxVal, int x, int y, int w, int h,
                           Color fill, Color bg, bool alignRight) {
    int drawX = alignRight ? x - w : x;
    DrawRectangle(drawX, y, w, h, bg);

    if (maxVal > 0.0f) {
        int filled = (int)((val / maxVal) * w);
        if (filled < 0) filled = 0;
        if (filled > w) filled = w;
        DrawRectangle(drawX, y, filled, h, fill);
        DrawRectangle(drawX, y, filled, h / 3, ColorAlpha(Lighten(fill, 60), 0.45f));
    }

    DrawRectangleLines(drawX, y, w, h, (Color){225, 225, 235, 180});
}

static void DrawTraitLine(Fighter* f, int x, int y, bool alignRight) {
    char line[128] = "";
    if (f->charData.traits.hasSixEyes) strcat(line, "[SIX EYES] ");
    if (f->charData.traits.hasCopy) strcat(line, "[COPY] ");
    if (f->charData.traits.hasBlackFlash) strcat(line, "[BLACK FLASH] ");
    if (f->isHeavenlyRestricted) strcat(line, "[HEAVENLY RESTRICTION] ");

    if (line[0] == '\0') return;
    int w = (int)UiMeasure(line, 10.0f, 1.0f).x;
    UiText(line, (Vector2){ (float)(alignRight ? x - w : x), (float)y }, 10.0f, 1.0f, ColorAlpha(WHITE, 0.8f));
}

void DrawHUD(Fighter* p1, Fighter* p2, float domainTimer, bool domainActive, int screenW) {
    int barW = 300;

    UiText(p1->charData.name, (Vector2){42, 12}, 20.0f, 1.0f, p1->charData.bodyColor);
    UiText(p1->charData.fullTitle, (Vector2){42, 34}, 12.0f, 1.0f, (Color){200, 205, 220, 210});
    DrawPremiumBar(p1->hp, p1->maxHP, 42, 54, barW, 24, (Color){80, 230, 120, 255}, (Color){40, 18, 22, 220}, false);
    DrawPremiumBar(p1->cursedEnergy, p1->maxCE, 42, 84, barW, 14, p1->charData.ceColor, (Color){18, 16, 28, 220}, false);
    DrawTraitLine(p1, 42, 104, false);

    char p1Ult[128];
    snprintf(p1Ult, sizeof(p1Ult), "ULT [%s]: %s", "X",
             p1->ultUsed ? "USED" : (p1->ultReady || !p1->ultUsed ? p1->charData.ultimateName : "LOCKED"));
    UiText(p1Ult, (Vector2){42, 118}, 11.0f, 1.0f, p1->ultUsed ? (Color){180, 110, 110, 220} : (Color){255, 220, 140, 220});
    if (p1->charData.id == CHAR_YUJI && !p1->ultUsed) {
        char combo[32];
        snprintf(combo, sizeof(combo), "Combo: %d / 3", p1->comboHits);
        UiText(combo, (Vector2){42, 132}, 11.0f, 1.0f, (Color){255, 210, 120, 220});
    }

    int rightX = screenW - 42;
    int p2NameW = (int)UiMeasure(p2->charData.name, 20.0f, 1.0f).x;
    UiText(p2->charData.name, (Vector2){ (float)(rightX - p2NameW), 12.0f }, 20.0f, 1.0f, p2->charData.bodyColor);
    int p2TitleW = (int)UiMeasure(p2->charData.fullTitle, 12.0f, 1.0f).x;
    UiText(p2->charData.fullTitle, (Vector2){ (float)(rightX - p2TitleW), 34.0f }, 12.0f, 1.0f, (Color){200, 205, 220, 210});
    DrawPremiumBar(p2->hp, p2->maxHP, rightX, 54, barW, 24, (Color){80, 230, 120, 255}, (Color){40, 18, 22, 220}, true);
    DrawPremiumBar(p2->cursedEnergy, p2->maxCE, rightX, 84, barW, 14, p2->charData.ceColor, (Color){18, 16, 28, 220}, true);
    DrawTraitLine(p2, rightX, 104, true);

    char p2Ult[128];
    snprintf(p2Ult, sizeof(p2Ult), "ULT [%s]: %s", "NUM4",
             p2->ultUsed ? "USED" : (p2->ultReady || !p2->ultUsed ? p2->charData.ultimateName : "LOCKED"));
    int p2UltW = (int)UiMeasure(p2Ult, 11.0f, 1.0f).x;
    UiText(p2Ult, (Vector2){ (float)(rightX - p2UltW), 118.0f }, 11.0f, 1.0f, p2->ultUsed ? (Color){180, 110, 110, 220} : (Color){255, 220, 140, 220});
    if (p2->charData.id == CHAR_YUJI && !p2->ultUsed) {
        char combo[32];
        snprintf(combo, sizeof(combo), "Combo: %d / 3", p2->comboHits);
        int cw = (int)UiMeasure(combo, 11.0f, 1.0f).x;
        UiText(combo, (Vector2){ (float)(rightX - cw), 132.0f }, 11.0f, 1.0f, (Color){255, 210, 120, 220});
    }

    const char* center = domainActive ? "DOMAIN ACTIVE" : "VS";
    int cw = (int)UiMeasure(center, 26.0f, 1.0f).x;
    UiText(center, (Vector2){ (float)(screenW / 2 - cw / 2), 24.0f }, 26.0f, 1.0f, (Color){230, 220, 255, 230});
    if (domainActive) {
        char timer[64];
        snprintf(timer, sizeof(timer), "Window %.1fs", domainTimer);
        int tw = (int)UiMeasure(timer, 18.0f, 1.0f).x;
        UiText(timer, (Vector2){ (float)(screenW / 2 - tw / 2), 56.0f }, 18.0f, 1.0f, (Color){255, 240, 120, 240});
    }
}

void DrawCharSelectScreen(int p1Cursor, int p2Cursor, bool p1Confirmed, bool p2Confirmed,
                          int screenW, int screenH) {
    DrawRectangleGradientV(0, 0, screenW, screenH, (Color){5, 3, 16, 255}, (Color){22, 10, 38, 255});
    DrawStars(screenW, screenH, (Color){150, 170, 255, 255}, 4.0f);

    const char* title = "SELECT YOUR SORCERER";
    int tw = (int)UiMeasure(title, 30.0f, 1.0f).x;
    UiText(title, (Vector2){ (float)(screenW / 2 - tw / 2), 26.0f }, 30.0f, 1.0f, (Color){235, 220, 255, 255});

    int slotW = 150;
    int slotH = 236;
    int gap = 16;
    int total = CHAR_COUNT * slotW + (CHAR_COUNT - 1) * gap;
    int startX = (screenW - total) / 2;

    for (int i = 0; i < CHAR_COUNT; i++) {
        CharacterData cd = GetCharacterData((CharacterID)i);
        int x = startX + i * (slotW + gap);
        bool p1Here = (p1Cursor == i);
        bool p2Here = (p2Cursor == i);

        DrawRectangleRounded((Rectangle){ x, 92, slotW, slotH }, 0.08f, 8, (Color){24, 16, 40, 220});
        DrawRectangleRounded((Rectangle){ x + 10, 102, slotW - 20, 110 }, 0.08f, 8, ColorAlpha(cd.bodyColor, 0.85f));
        if (cd.id == CHAR_GOJO && gGojoPortraitLoaded) {
            Rectangle src = {0, 0, (float)gGojoPortrait.width, (float)gGojoPortrait.height};
            Rectangle dst = {(float)(x + 26), 102.0f, 98.0f, 110.0f};
            DrawTexturePro(gGojoPortrait, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        } else {
            DrawCircle(x + slotW / 2, 150, 22, ColorAlpha(BLACK, 0.20f));
            DrawRectangle(x + slotW / 2 - 14, 154, 28, 34, ColorAlpha(BLACK, 0.25f));
        }

        Color border = (Color){70, 50, 120, 180};
        if (p1Here && p2Here) border = (Color){255, 220, 70, 255};
        else if (p1Here) border = (Color){90, 160, 255, 255};
        else if (p2Here) border = (Color){255, 90, 90, 255};
        DrawRectangleRoundedLines((Rectangle){ x, 92, slotW, slotH }, 0.08f, 8, border);

        int nw = (int)UiMeasure(cd.name, 16.0f, 1.0f).x;
        UiText(cd.name, (Vector2){ (float)(x + slotW / 2 - nw / 2), 220.0f }, 16.0f, 1.0f, WHITE);
        int fw = (int)UiMeasure(cd.fullTitle, 9.0f, 1.0f).x;
        UiText(cd.fullTitle, (Vector2){ (float)(x + slotW / 2 - fw / 2), 242.0f }, 9.0f, 1.0f, (Color){200, 205, 220, 220});
        int uw = (int)UiMeasure(cd.ultimateName, 9.0f, 1.0f).x;
        UiText(cd.ultimateName, (Vector2){ (float)(x + slotW / 2 - uw / 2), 258.0f }, 9.0f, 1.0f, ColorAlpha(cd.ceColor, 0.95f));
    }

    UiText("P1: A / D + SPACE", (Vector2){120, (float)(screenH - 64)}, 14.0f, 1.0f, (Color){120, 190, 255, 255});
    UiText("P2: LEFT / RIGHT + ENTER", (Vector2){620, (float)(screenH - 64)}, 14.0f, 1.0f, (Color){255, 130, 130, 255});
    if (p1Confirmed) UiText("PLAYER 1 LOCKED", (Vector2){120, (float)(screenH - 38)}, 12.0f, 1.0f, (Color){120, 190, 255, 255});
    if (p2Confirmed) UiText("PLAYER 2 LOCKED", (Vector2){650, (float)(screenH - 38)}, 12.0f, 1.0f, (Color){255, 130, 130, 255});
}

void DrawBattleBackground(int screenW, int screenH) {
    DrawRectangleGradientV(0, 0, screenW, screenH, (Color){9, 10, 22, 255}, (Color){18, 12, 30, 255});
}

void DrawGameOverOverlay(const char* winnerText, Color winnerColor, int screenW, int screenH) {
    DrawRectangle(0, 0, screenW, screenH, (Color){0, 0, 0, 160});
    DrawRectangle(0, screenH / 2 - 84, screenW, 168, (Color){14, 10, 22, 230});
    DrawRectangle(0, screenH / 2 - 84, screenW, 4, winnerColor);
    DrawRectangle(0, screenH / 2 + 80, screenW, 4, winnerColor);

    int tw = (int)UiMeasure(winnerText, 42.0f, 1.0f).x;
    UiText(winnerText, (Vector2){ (float)(screenW / 2 - tw / 2), (float)(screenH / 2 - 34) }, 42.0f, 1.0f, winnerColor);

    const char* sub = "PRESS ENTER TO REMATCH";
    int sw = (int)UiMeasure(sub, 16.0f, 1.0f).x;
    UiText(sub, (Vector2){ (float)(screenW / 2 - sw / 2), (float)(screenH / 2 + 34) }, 16.0f, 1.0f, (Color){220, 220, 220, 220});
}
