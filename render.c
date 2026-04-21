#include "render.h"
#include "raymath.h"
#define CHARACTER_GOJO_SPRITE_IMPLEMENTATION
#include "character_gojo_sprite.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FLOOR_Y 400.0f

Particle gParticles[MAX_PARTICLES];
ScreenShake gShake = {0};
DomainAnnounce gDomainAnnounce = {0};
FloatingText gFloatingTexts[16];
static Font gUiFont = {0};
static bool gUiFontLoaded = false;
static Texture2D gGojoPortrait = {0};
static bool gGojoPortraitLoaded = false;
static int gRCTFlashFrames = 0;

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

void FloatingTextSpawn(Vector2 pos, const char* text, Color color, float life) {
    for (int i = 0; i < 16; i++) {
        if (!gFloatingTexts[i].active) {
            gFloatingTexts[i].pos = pos;
            snprintf(gFloatingTexts[i].text, sizeof(gFloatingTexts[i].text), "%s", text);
            gFloatingTexts[i].color = color;
            gFloatingTexts[i].life = life;
            gFloatingTexts[i].maxLife = life;
            gFloatingTexts[i].active = true;
            return;
        }
    }
}

void FloatingTextUpdate(void) {
    float dt = GetFrameTime();
    for (int i = 0; i < 16; i++) {
        if (!gFloatingTexts[i].active) continue;
        gFloatingTexts[i].life -= dt;
        gFloatingTexts[i].pos.y -= 20.0f * dt;
        if (gFloatingTexts[i].life <= 0.0f) {
            gFloatingTexts[i].active = false;
        }
    }
}

void FloatingTextDraw(void) {
    for (int i = 0; i < 16; i++) {
        if (!gFloatingTexts[i].active) continue;
        float alpha = gFloatingTexts[i].life / gFloatingTexts[i].maxLife;
        Color c = gFloatingTexts[i].color;
        c.a = (unsigned char)(255.0f * alpha);
        Vector2 size = UiMeasure(gFloatingTexts[i].text, 14.0f, 1.0f);
        UiText(gFloatingTexts[i].text, (Vector2){ gFloatingTexts[i].pos.x - size.x * 0.5f, gFloatingTexts[i].pos.y }, 14.0f, 1.0f, c);
    }
}

void TriggerRCTFlash(void) {
    gRCTFlashFrames = 4;
}

void DrawRCTFlashOverlay(int screenW, int screenH) {
    if (gRCTFlashFrames > 0) {
        DrawRectangle(0, 0, screenW, screenH, ColorAlpha(WHITE, 0.18f));
    }
}

void SpawnRCTBurst(Vector2 center) {
    for (int i = 0; i < 20; i++) {
        float angle = (float)(i % 4) * PI * 0.5f;
        float lane = 0.65f + (float)(i / 4) * 0.12f;
        Vector2 vel = { cosf(angle) * 1.4f * lane, -fabsf(sinf(angle)) * 1.8f * lane - 0.2f };
        ParticleSpawn(center, vel, (Color){80, 255, 120, 255}, 0.55f, 5.0f, PARTICLE_REGEN);
    }
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
    if (gRCTFlashFrames > 0) gRCTFlashFrames--;
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
    (void)timer;
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
    for (int i = 0; i < 6; i++) {
        int x = 70 + i * 150;
        int h = 120 + (i % 3) * 35;
        DrawRectangle(x, (int)floorY - h, 24, h, (Color){10, 8, 18, 65});
        DrawRectangle(x + 3, (int)floorY - h + 10, 18, h - 10, (Color){32, 24, 52, 40});
    }

    DrawRectangleGradientV(0, (int)floorY, screenW, screenH - (int)floorY, (Color){8, 8, 14, 90}, (Color){3, 3, 6, 145});
    DrawRectangleGradientV(0, (int)floorY - 26, screenW, 26, (Color){255, 255, 255, 0}, (Color){120, 140, 255, 22});
    DrawRectangle(0, (int)floorY, screenW, 3, (Color){190, 210, 255, 120});

    for (int x = 0; x < screenW; x += 40) DrawLine(x, (int)floorY, x, screenH, (Color){255, 255, 255, 5});
    for (int y = (int)floorY; y < screenH; y += 28) DrawLine(0, y, screenW, y, (Color){255, 255, 255, 6});
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

        case ULT_FUGA:
            DrawEllipse((int)(f->ultHitbox.x + f->ultHitbox.width * 0.5f), (int)(f->ultHitbox.y + f->ultHitbox.height * 0.5f),
                        28.0f, 12.0f, ColorAlpha((Color){255, 120, 20, 255}, 0.3f));
            DrawCircleV((Vector2){ f->ultHitbox.x + f->ultHitbox.width * 0.5f, f->ultHitbox.y + f->ultHitbox.height * 0.5f },
                        10.0f, (Color){255, 170, 70, 240});
            break;

        case ULT_PURE_LOVE_BEAM:
            DrawRectangleRounded(f->ultHitbox, 0.1f, 8, ColorAlpha((Color){225, 235, 255, 255}, 0.55f));
            DrawRectangleRounded((Rectangle){ f->ultHitbox.x, f->ultHitbox.y + 12.0f, f->ultHitbox.width, f->ultHitbox.height - 24.0f },
                                 0.1f, 8, ColorAlpha((Color){170, 220, 255, 255}, 0.65f));
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

void DrawFighterBody(Fighter* f, bool isP1, float introProgress, bool domainCast, bool domainCounter) {
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

    if (f->infinityActive) {
        float repel = 1.0f + 0.15f * sinf((float)GetTime() * 8.0f);
        DrawCircleLines((int)(bx + bw * 0.5f), (int)(by + bh * 0.45f), 34.0f * repel, ColorAlpha((Color){180, 235, 255, 255}, 0.8f));
        DrawCircleLines((int)(bx + bw * 0.5f), (int)(by + bh * 0.45f), 46.0f * repel, ColorAlpha((Color){120, 200, 255, 255}, 0.45f));
    }

    if (!DrawGojoSprite(f, isP1, introProgress, domainCast, domainCounter)) {
        DrawRectangleRounded((Rectangle){ bx, by, bw, bh }, 0.15f, 8, f->bodyColor);
        DrawRectangleRounded((Rectangle){ bx + 5.0f, by + 5.0f, bw - 10.0f, bh * 0.38f }, 0.15f, 8, Lighten(f->bodyColor, 45));
        DrawRectangleRoundedLines((Rectangle){ bx, by, bw, bh }, 0.15f, 8, Lighten(f->bodyColor, 85));
    }

    if (f->isBlocking) {
        DrawRectangleRounded((Rectangle){ bx, by, bw, bh }, 0.15f, 8, ColorAlpha(LIGHTGRAY, 0.45f));
    }

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

    if (f->specialAnimFrames > 0) {
        DrawRectangleRoundedLines((Rectangle){ bx - 10.0f, by - 10.0f, bw + 20.0f, bh + 20.0f },
                                  0.18f, 8, ColorAlpha(f->charData.ceColor, 0.75f));
    }

    if (f->blackFlashActive) {
        DrawRectangleRounded((Rectangle){ bx - 5.0f, by - 5.0f, bw + 10.0f, bh + 10.0f }, 0.16f, 8, (Color){12, 12, 12, 220});
        DrawRectangleRoundedLines((Rectangle){ bx - 5.0f, by - 5.0f, bw + 10.0f, bh + 10.0f }, 0.16f, 8, (Color){255, 205, 70, 220});
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

    if (f->mahoragaActive && f->mahoragaHP > 0.0f) {
        Rectangle m = f->mahoragaHitbox;
        DrawEllipse((int)(m.x + m.width * 0.5f), (int)(FLOOR_Y + 10.0f), 54.0f, 14.0f, (Color){0, 0, 0, 95});
        DrawRectangleRounded(m, 0.2f, 8, ColorAlpha((Color){215, 215, 225, 255}, 0.92f));
        DrawRectangleRounded((Rectangle){ m.x + 10.0f, m.y + 10.0f, m.width - 20.0f, m.height * 0.30f }, 0.18f, 8, ColorAlpha((Color){245, 245, 255, 255}, 0.75f));
        DrawRectangleRoundedLines(m, 0.2f, 8, (Color){255, 245, 210, 255});
        DrawCircleLines((int)(m.x + m.width * 0.5f), (int)(m.y - 14.0f), 15.0f, (Color){255, 240, 180, 255});
        DrawLineEx((Vector2){ m.x + m.width * 0.5f, m.y - 14.0f }, (Vector2){ m.x + m.width * 0.5f + cosf(DEG2RAD * f->mahoragaWheelAngle) * 12.0f, m.y - 14.0f + sinf(DEG2RAD * f->mahoragaWheelAngle) * 12.0f }, 2.0f, (Color){255, 240, 180, 255});
        UiText("MAHORAGA", (Vector2){ m.x + 10.0f, m.y + m.height + 8.0f }, 10.0f, 1.0f, (Color){245, 245, 255, 255});
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

void DrawProjectiles(const Projectile* projectiles, int count) {
    for (int i = 0; i < count; i++) {
        const Projectile* p = &projectiles[i];
        Vector2 center;
        if (!p->active) continue;
        center = (Vector2){ p->hitbox.x + p->hitbox.width * 0.5f, p->hitbox.y + p->hitbox.height * 0.5f };

        switch (p->type) {
            case PROJ_GOJO_RED:
                DrawCircleV(center, 18.0f, ColorAlpha((Color){255, 70, 70, 255}, 0.2f));
                DrawCircleV(center, 11.0f, (Color){255, 100, 100, 240});
                break;
            case PROJ_GOJO_BLUE:
                DrawCircleV(center, 18.0f, ColorAlpha((Color){90, 160, 255, 255}, 0.24f));
                DrawCircleV(center, 11.0f, (Color){110, 180, 255, 240});
                break;
            case PROJ_SUKUNA_DISMANTLE:
                DrawLineEx((Vector2){ p->hitbox.x, p->hitbox.y }, (Vector2){ p->hitbox.x + p->hitbox.width, p->hitbox.y + p->hitbox.height },
                           3.0f, (Color){255, 100, 100, 220});
                break;
            case PROJ_FUGA_ARROW:
                DrawEllipse((int)center.x, (int)center.y, 20.0f, 8.0f, ColorAlpha((Color){255, 140, 20, 255}, 0.32f));
                DrawCircleV(center, 9.0f, (Color){255, 180, 70, 245});
                break;
            case PROJ_CE_BLAST:
            case PROJ_NONE:
            default:
                DrawCircleV(center, 16.0f, ColorAlpha(p->color, 0.20f));
                DrawCircleV(center, 10.0f, p->color);
                DrawCircleLines((int)center.x, (int)center.y, 12.0f, WHITE);
                break;
        }
    }
}

static const char* AbilityRowText(const Fighter* f, bool isP1) {
    switch (f->charData.id) {
        case CHAR_GOJO:
            return isP1 ? "[LMB] Hit | [1] Tool | [E] Blue | [F] Red | [G] Infinity | [H] Domain | [X] Purple"
                        : "[N0] Hit | [N1] Tool | [N4] Blue | [N5] Red | [N6] Infinity | [N3] Domain | [N7] Purple";
        case CHAR_SUKUNA:
            return isP1 ? "[LMB] Hit | [1] Tool | [E] Dismantle | [F] Cleave | [H] Domain | [X] Fuga"
                        : "[N0] Hit | [N1] Tool | [N4] Dismantle | [N5] Cleave | [N3] Domain | [N7] Fuga";
        case CHAR_YUTA:
            return isP1 ? "[LMB] Hit | [1] Tool | [E] Katana | [F] Rika | [H] Domain | [X] Love Beam"
                        : "[N0] Hit | [N1] Tool | [N4] Katana | [N5] Rika | [N3] Domain | [N7] Love Beam";
        case CHAR_MEGUMI:
            return isP1 ? "[LMB] Hit | [1] Tool | [E] Nue | [F] Dogs | [G] Shadow | [H] Domain | [X] Mahoraga"
                        : "[N0] Hit | [N1] Tool | [N4] Nue | [N5] Dogs | [N6] Shadow | [N3] Domain | [N7] Mahoraga";
        case CHAR_NANAMI:
            return isP1 ? "[LMB] Hit | [E] Ratio | [F] Collapse | [G] Overtime | [X] Slash"
                        : "[N0] Hit | [N4] Ratio | [N5] Collapse | [N6] Overtime | [N7] Slash";
        case CHAR_NOBARA:
            return isP1 ? "[LMB] Hit | [E] Nail | [F] Resonance | [G] Hairpin | [X] Maximum"
                        : "[N0] Hit | [N4] Nail | [N5] Resonance | [N6] Hairpin | [N7] Maximum";
        case CHAR_TODO:
            return isP1 ? "[LMB] Hit | [E] Strike | [F] Swap | [G] Clap | [X] Tackle"
                        : "[N0] Hit | [N4] Strike | [N5] Swap | [N6] Clap | [N7] Tackle";
        case CHAR_YUJI:
            return isP1 ? "[LMB] Hit | [Q] Dash | [RMB] Parry | [X] Black Flash"
                        : "[N0] Hit | [NUM.] Parry | [N8] Dash | [N7] Black Flash";
        case CHAR_TOJI:
            return isP1 ? "[LMB] Hit | [Q] Dash | [X] Assault"
                        : "[N0] Hit | [N8] Dash | [N7] Assault";
        default:
            return isP1 ? "[LMB] Hit | [1] Tool | [R] RCT | [H] Domain | [X] Ult"
                        : "[N0] Hit | [N1] Tool | [N2] RCT | [N3] Domain | [N7] Ult";
    }
}

static void DrawSpecialStatus(const Fighter* f, int x, int y, bool alignRight) {
    char text[96];
    if (f->charData.id == CHAR_MEGUMI) {
        int w = 120;
        int drawX = alignRight ? x - w : x;
        DrawRectangle(drawX, y, w, 8, (Color){18, 20, 30, 220});
        DrawRectangle(drawX, y, (int)(w * (f->specialMeter / f->maxSpecialMeter)), 8, (Color){90, 100, 255, 255});
        DrawRectangleLines(drawX, y, w, 8, WHITE);
        snprintf(text, sizeof(text), "SHADOW");
        if (alignRight) {
            int tw = (int)UiMeasure(text, 8.0f, 1.0f).x;
            UiText(text, (Vector2){ (float)(x - tw), (float)(y - 12) }, 8.0f, 1.0f, WHITE);
        } else {
            UiText(text, (Vector2){ (float)x, (float)(y - 12) }, 8.0f, 1.0f, WHITE);
        }
    } else if (f->charData.id == CHAR_TODO) {
        snprintf(text, sizeof(text), "CHARGES %d/%d", f->boogieCharges, 2);
        if (alignRight) {
            int tw = (int)UiMeasure(text, 8.0f, 1.0f).x;
            UiText(text, (Vector2){ (float)(x - tw), (float)y }, 8.0f, 1.0f, (Color){255, 214, 118, 240});
        } else {
            UiText(text, (Vector2){ (float)x, (float)y }, 8.0f, 1.0f, (Color){255, 214, 118, 240});
        }
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

static float CooldownFill(float remaining, float maxCooldown) {
    if (maxCooldown <= 0.0f) return 1.0f;
    float fill = 1.0f - (remaining / maxCooldown);
    if (fill < 0.0f) fill = 0.0f;
    if (fill > 1.0f) fill = 1.0f;
    return fill;
}

static void DrawCooldownBar(int x, int y, const char* label, float remaining, float maxCooldown, Color color, bool alignRight) {
    int w = 64;
    int h = 6;
    int drawX = alignRight ? x - w : x;
    float fill = CooldownFill(remaining, maxCooldown);
    DrawRectangle(drawX, y, w, h, (Color){42, 42, 52, 215});
    DrawRectangle(drawX, y, (int)(w * fill), h, color);
    DrawRectangleLines(drawX, y, w, h, ColorAlpha(WHITE, 0.4f));
    if (alignRight) {
        int lw = (int)UiMeasure(label, 8.0f, 1.0f).x;
        UiText(label, (Vector2){ (float)(drawX + w - lw), (float)(y - 11) }, 8.0f, 1.0f, WHITE);
    } else {
        UiText(label, (Vector2){ (float)drawX, (float)(y - 11) }, 8.0f, 1.0f, WHITE);
    }
}

static void DrawCooldownHudRow(const Fighter* f, int anchorX, int y, bool alignRight) {
    int step = 70;
    int startX = anchorX;
    if (alignRight) startX = anchorX;
    DrawCooldownBar(startX + (alignRight ? 0 : 0) * step, y, "[LMB]", 0.0f, 1.0f, WHITE, alignRight);
    DrawCooldownBar(startX + (alignRight ? -1 : 1) * step, y, "[RMB]", f->isBlocking ? 0.2f : 0.0f, 0.2f, LIGHTGRAY, alignRight);
    DrawCooldownBar(startX + (alignRight ? -2 : 2) * step, y, "[1]", f->ceAttackCooldown, 1.2f, f->charData.ceColor, alignRight);
    DrawCooldownBar(startX + (alignRight ? -3 : 3) * step, y, "[E]", f->abilityECooldown, 5.0f, SKYBLUE, alignRight);
    DrawCooldownBar(startX + (alignRight ? -4 : 4) * step, y, "[F]", f->abilityRCooldown, 5.0f, ORANGE, alignRight);
    DrawCooldownBar(startX + (alignRight ? -5 : 5) * step, y, "[G/H]", f->abilityFCooldown, 4.0f, PURPLE, alignRight);
    {
        int ultX = startX + (alignRight ? -6 : 6) * step;
        int boxX = alignRight ? ultX - 64 : ultX;
        DrawRectangle(boxX, y, 64, 6, (Color){42, 42, 52, 215});
        DrawRectangle(boxX, y, f->ultUsed ? 0 : 64, 6, GOLD);
        DrawRectangleLines(boxX, y, 64, 6, ColorAlpha(WHITE, 0.4f));
        UiText("[X]", (Vector2){ (float)boxX, (float)(y - 11) }, 8.0f, 1.0f, WHITE);
    }
}

static void DrawGhostBar(float ghostVal, float val, float maxVal, int x, int y, int w, int h, bool alignRight) {
    int drawX = alignRight ? x - w : x;
    int ghostFilled = (int)((ghostVal / maxVal) * w);
    int realFilled = (int)((val / maxVal) * w);
    if (ghostFilled < realFilled) ghostFilled = realFilled;
    DrawRectangle(drawX + realFilled, y, ghostFilled - realFilled, h, ColorAlpha((Color){255, 210, 90, 255}, 0.75f));
    DrawRectangle(drawX, y, realFilled, h, WHITE);
}

static void DrawRoundIcons(int x, int y, int wins, bool alignRight) {
    for (int i = 0; i < 2; i++) {
        int px = alignRight ? x - i * 18 : x + i * 18;
        DrawCircle(px, y, 5.0f, i < wins ? (Color){255, 214, 118, 255} : (Color){70, 70, 90, 255});
        DrawCircleLines(px, y, 6.5f, WHITE);
    }
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

void DrawHUD(Fighter* p1, Fighter* p2, float domainTimer, bool domainActive, int screenW,
             float roundTimer, int p1Rounds, int p2Rounds,
             const char* bannerText, const char* subText, float bannerTimer) {
    int screenH = GetScreenHeight();
    int leftX = 24;
    int rightX = screenW - 24;
    int barW = 330;
    int hpY = 18;
    int ceY = 46;

    DrawRectangle(0, 0, screenW, 96, ColorAlpha(BLACK, 0.36f));
    DrawRectangleGradientV(0, 0, screenW, 96, ColorAlpha((Color){20, 14, 26, 255}, 0.9f), ColorAlpha(BLACK, 0.0f));

    DrawPremiumBar(p1->hp, p1->maxHP, leftX + 56, hpY, barW, 22, (Color){210, 56, 80, 255}, (Color){36, 20, 26, 220}, false);
    DrawGhostBar(p1->ghostHP, p1->hp, p1->maxHP, leftX + 56, hpY, barW, 22, false);
    DrawPremiumBar(p1->cursedEnergy, p1->maxCE, leftX + 56, ceY, barW, 10, p1->charData.ceColor, (Color){18, 16, 28, 220}, false);
    if (p1->rctGlowTimer > 0.0f) {
        float pulse = 0.25f + 0.35f * (0.5f + 0.5f * sinf((float)GetTime() * 12.0f));
        DrawRectangleLinesEx((Rectangle){ leftX + 54.0f, hpY - 2.0f, barW + 4.0f, 26.0f }, 2.0f, ColorAlpha((Color){80, 255, 120, 255}, pulse));
    }
    DrawRectangle(leftX, hpY - 2, 44, 44, ColorAlpha(p1->charData.bodyColor, 0.9f));
    DrawRectangleLines(leftX, hpY - 2, 44, 44, WHITE);
    UiText(p1->charData.name, (Vector2){ (float)(leftX + 56), 66.0f }, 16.0f, 1.0f, WHITE);
    DrawRoundIcons(leftX + 64, 88, p1Rounds, false);

    DrawPremiumBar(p2->hp, p2->maxHP, rightX - 56, hpY, barW, 22, (Color){210, 56, 80, 255}, (Color){36, 20, 26, 220}, true);
    DrawGhostBar(p2->ghostHP, p2->hp, p2->maxHP, rightX - 56, hpY, barW, 22, true);
    DrawPremiumBar(p2->cursedEnergy, p2->maxCE, rightX - 56, ceY, barW, 10, p2->charData.ceColor, (Color){18, 16, 28, 220}, true);
    if (p2->rctGlowTimer > 0.0f) {
        float pulse = 0.25f + 0.35f * (0.5f + 0.5f * sinf((float)GetTime() * 12.0f));
        DrawRectangleLinesEx((Rectangle){ rightX - 56.0f - barW - 2.0f, hpY - 2.0f, barW + 4.0f, 26.0f }, 2.0f, ColorAlpha((Color){80, 255, 120, 255}, pulse));
    }
    DrawRectangle(rightX - 44, hpY - 2, 44, 44, ColorAlpha(p2->charData.bodyColor, 0.9f));
    DrawRectangleLines(rightX - 44, hpY - 2, 44, 44, WHITE);
    {
        int nameW = (int)UiMeasure(p2->charData.name, 16.0f, 1.0f).x;
        UiText(p2->charData.name, (Vector2){ (float)(rightX - 56 - nameW), 66.0f }, 16.0f, 1.0f, WHITE);
    }
    DrawRoundIcons(rightX - 64, 88, p2Rounds, true);

    DrawTraitLine(p1, leftX + 56, 82, false);
    DrawTraitLine(p2, rightX - 56, 82, true);
    DrawSpecialStatus(p1, leftX + 56, 94, false);
    DrawSpecialStatus(p2, rightX - 56, 94, true);

    {
        char timerText[32];
        snprintf(timerText, sizeof(timerText), "%02d", (int)ceilf(roundTimer));
        DrawRectangleRounded((Rectangle){ screenW * 0.5f - 44.0f, 10.0f, 88.0f, 42.0f }, 0.16f, 8, (Color){20, 18, 32, 235});
        DrawRectangleRoundedLines((Rectangle){ screenW * 0.5f - 44.0f, 10.0f, 88.0f, 42.0f }, 0.16f, 8, WHITE);
        {
            int tw = (int)UiMeasure(timerText, 24.0f, 1.0f).x;
            UiText(timerText, (Vector2){ (float)(screenW * 0.5f - tw * 0.5f), 18.0f }, 24.0f, 1.0f, (Color){255, 230, 140, 255});
        }
        if (domainActive) {
            char timer[48];
            snprintf(timer, sizeof(timer), "DOMAIN %.1f", domainTimer);
            int dw = (int)UiMeasure(timer, 12.0f, 1.0f).x;
            UiText(timer, (Vector2){ (float)(screenW * 0.5f - dw * 0.5f), 56.0f }, 12.0f, 1.0f, (Color){255, 240, 120, 240});
        }
    }

    UiText(AbilityRowText(p1, true), (Vector2){ 24.0f, 102.0f }, 13.0f, 1.0f, (Color){235, 238, 245, 235});
    {
        const char* p2Abilities = AbilityRowText(p2, false);
        int aw = (int)UiMeasure(p2Abilities, 12.0f, 1.0f).x;
        UiText(p2Abilities, (Vector2){ (float)(screenW - 24 - aw), 102.0f }, 13.0f, 1.0f, (Color){235, 238, 245, 235});
    }
    DrawCooldownHudRow(p1, 42, 148, false);
    DrawCooldownHudRow(p2, screenW - 42, 148, true);

    if (p1->mahoragaActive && p1->mahoragaHP > 0.0f) {
        UiText("MAHORAGA", (Vector2){ screenW * 0.5f - 152.0f, 86.0f }, 12.0f, 1.0f, (Color){240, 240, 255, 255});
        DrawPremiumBar(p1->mahoragaHP, p1->mahoragaMaxHP, screenW * 0.5f - 60, 84, 120, 10, (Color){230, 230, 245, 255}, (Color){32, 30, 46, 220}, false);
        DrawCircleLines((int)(screenW * 0.5f), 68, 18.0f, (Color){255, 240, 180, 255});
        DrawLineEx((Vector2){ screenW * 0.5f, 68.0f }, (Vector2){ screenW * 0.5f + cosf(DEG2RAD * p1->mahoragaWheelAngle) * 16.0f, 68.0f + sinf(DEG2RAD * p1->mahoragaWheelAngle) * 16.0f }, 2.0f, (Color){255, 240, 180, 255});
    } else if (p2->mahoragaActive && p2->mahoragaHP > 0.0f) {
        UiText("MAHORAGA", (Vector2){ screenW * 0.5f + 34.0f, 86.0f }, 12.0f, 1.0f, (Color){240, 240, 255, 255});
        DrawPremiumBar(p2->mahoragaHP, p2->mahoragaMaxHP, screenW * 0.5f + 60, 84, 120, 10, (Color){230, 230, 245, 255}, (Color){32, 30, 46, 220}, true);
        DrawCircleLines((int)(screenW * 0.5f), 68, 18.0f, (Color){255, 240, 180, 255});
        DrawLineEx((Vector2){ screenW * 0.5f, 68.0f }, (Vector2){ screenW * 0.5f + cosf(DEG2RAD * p2->mahoragaWheelAngle) * 16.0f, 68.0f + sinf(DEG2RAD * p2->mahoragaWheelAngle) * 16.0f }, 2.0f, (Color){255, 240, 180, 255});
    }

    if (p1->comboDisplayTimer > 0.0f && p1->comboCounter >= 2) {
        char combo[32];
        snprintf(combo, sizeof(combo), "%d HIT COMBO", p1->comboCounter);
        UiText(combo, (Vector2){ screenW * 0.5f + 36.0f, 188.0f }, 36.0f, 1.0f, p1->charData.ceColor);
    } else if (p2->comboDisplayTimer > 0.0f && p2->comboCounter >= 2) {
        char combo[32];
        snprintf(combo, sizeof(combo), "%d HIT COMBO", p2->comboCounter);
        UiText(combo, (Vector2){ screenW * 0.5f - 250.0f, 188.0f }, 36.0f, 1.0f, p2->charData.ceColor);
    }
    if (domainActive && !p1->hasDomain && !p1->isHeavenlyRestricted) {
        UiText("SURVIVE!", (Vector2){ screenW * 0.5f - 82.0f, 214.0f }, 24.0f, 1.0f, (Color){255, 80, 80, 240});
    } else if (domainActive && !p2->hasDomain && !p2->isHeavenlyRestricted) {
        UiText("SURVIVE!", (Vector2){ screenW * 0.5f - 82.0f, 214.0f }, 24.0f, 1.0f, (Color){255, 80, 80, 240});
    }

    if (bannerTimer > 0.0f) {
        int bw = (int)UiMeasure(bannerText, 34.0f, 1.0f).x;
        DrawRectangle(0, screenH / 2 - 52, screenW, 104, ColorAlpha(BLACK, 0.5f));
        UiText(bannerText, (Vector2){ (float)(screenW / 2 - bw * 0.5f), (float)(screenH / 2 - 26) }, 34.0f, 1.0f, (Color){255, 225, 140, 255});
        if (subText != NULL && subText[0] != '\0') {
            int sw = (int)UiMeasure(subText, 16.0f, 1.0f).x;
            UiText(subText, (Vector2){ (float)(screenW / 2 - sw * 0.5f), (float)(screenH / 2 + 14) }, 16.0f, 1.0f, WHITE);
        }
    }
}

void DrawCharSelectScreen(int p1Cursor, int p2Cursor, bool p1Confirmed, bool p2Confirmed,
                          int screenW, int screenH, bool cpuMode, int focusIndex, const char* modeLabel) {
    static float smoothedOffset = 0.0f;
    DrawRectangleGradientV(0, 0, screenW, screenH, (Color){5, 3, 16, 255}, (Color){22, 10, 38, 255});
    DrawStars(screenW, screenH, (Color){150, 170, 255, 255}, 4.0f);

    const char* title = "SELECT YOUR SORCERER";
    int tw = (int)UiMeasure(title, 30.0f, 1.0f).x;
    UiText(title, (Vector2){ (float)(screenW / 2 - tw / 2), 26.0f }, 30.0f, 1.0f, (Color){235, 220, 255, 255});

    int slotW = 206;
    int slotH = 296;
    int gap = 24;
    float stripW = (float)(CHAR_COUNT * slotW + (CHAR_COUNT - 1) * gap);
    float focus = cpuMode ? (float)(focusIndex == 0 ? p1Cursor : p2Cursor)
                          : (((float)p1Cursor + (float)p2Cursor) * 0.5f);
    float targetCenter = focus * (float)(slotW + gap) + slotW * 0.5f;
    float offset = targetCenter - screenW * 0.5f;
    float visibleW = (float)screenW - 120.0f;
    float maxOffset = stripW - visibleW;
    if (maxOffset < 0.0f) maxOffset = 0.0f;
    offset = Clamp(offset, 0.0f, maxOffset);
    smoothedOffset += (offset - smoothedOffset) * 0.16f;
    float startX = 60.0f - smoothedOffset;

    DrawRectangle(0, 82, screenW, 330, (Color){0, 0, 0, 58});
    DrawLine(0, 412, screenW, 412, (Color){255, 214, 118, 120});

    for (int i = 0; i < CHAR_COUNT; i++) {
        CharacterData cd = GetCharacterData((CharacterID)i);
        float x = startX + i * (float)(slotW + gap);
        bool p1Here = (p1Cursor == i);
        bool p2Here = (p2Cursor == i);
        float lift = (p1Here || p2Here) ? -10.0f : 0.0f;
        Rectangle outer = { x, 102.0f + lift, (float)slotW, (float)slotH };
        Rectangle portrait = { x + 12.0f, 116.0f + lift, slotW - 24.0f, 156.0f };

        if (outer.x + outer.width < -10.0f || outer.x > screenW + 10.0f) continue;

        DrawRectangleRounded(outer, 0.06f, 8, (Color){18, 14, 32, 235});
        DrawRectangleRounded(portrait, 0.06f, 8, ColorAlpha(cd.bodyColor, 0.85f));
        if (cd.id == CHAR_GOJO && gGojoPortraitLoaded) {
            Rectangle src = {0, 0, (float)gGojoPortrait.width, (float)gGojoPortrait.height};
            Rectangle dst = {x + 36.0f, 112.0f + lift, 132.0f, 162.0f};
            DrawTexturePro(gGojoPortrait, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        } else {
            DrawCircle((int)(x + slotW / 2), (int)(182.0f + lift), 34, ColorAlpha(BLACK, 0.20f));
            DrawRectangle((int)(x + slotW / 2 - 20), (int)(190.0f + lift), 40, 54, ColorAlpha(BLACK, 0.25f));
        }

        Color border = (Color){70, 50, 120, 180};
        if (p1Here && p2Here) border = (Color){255, 220, 70, 255};
        else if (p1Here) border = (Color){90, 160, 255, 255};
        else if (p2Here) border = (Color){255, 90, 90, 255};
        DrawRectangleRoundedLines(outer, 0.06f, 8, border);
        DrawRectangleRoundedLines((Rectangle){ outer.x + 6.0f, outer.y + 6.0f, outer.width - 12.0f, outer.height - 12.0f }, 0.06f, 8, ColorAlpha(WHITE, 0.12f));

        int nw = (int)UiMeasure(cd.name, 18.0f, 1.0f).x;
        UiText(cd.name, (Vector2){ x + slotW * 0.5f - nw * 0.5f, 286.0f + lift }, 18.0f, 1.0f, WHITE);
        int fw = (int)UiMeasure(cd.fullTitle, 10.0f, 1.0f).x;
        UiText(cd.fullTitle, (Vector2){ x + slotW * 0.5f - fw * 0.5f, 314.0f + lift }, 10.0f, 1.0f, (Color){200, 205, 220, 220});
        int uw = (int)UiMeasure(cd.ultimateName, 10.0f, 1.0f).x;
        UiText(cd.ultimateName, (Vector2){ x + slotW * 0.5f - uw * 0.5f, 336.0f + lift }, 10.0f, 1.0f, ColorAlpha(cd.ceColor, 0.95f));

        if (p1Here) UiText(cpuMode ? "YOU" : "P1", (Vector2){ x + 18.0f, 86.0f + lift }, 12.0f, 1.0f, (Color){120, 190, 255, 255});
        if (p2Here) UiText(cpuMode ? "CPU" : "P2", (Vector2){ x + slotW - (cpuMode ? 54.0f : 42.0f), 86.0f + lift }, 12.0f, 1.0f, (Color){255, 130, 130, 255});
        if (cpuMode && p1Here && focusIndex == 0) UiText("SELECTING", (Vector2){ x + 52.0f, 86.0f + lift }, 10.0f, 1.0f, (Color){255, 214, 118, 255});
        if (cpuMode && p2Here && focusIndex == 1) UiText("SELECTING", (Vector2){ x + slotW - 118.0f, 86.0f + lift }, 10.0f, 1.0f, (Color){255, 214, 118, 255});
    }

    UiText(cpuMode ? "A / D MOVE | TAB SWITCHES YOU / CPU | SPACE LOCKS | BACKSPACE UNLOCKS" : "P1: A / D + SPACE", (Vector2){38, (float)(screenH - 68)}, 14.0f, 1.0f, (Color){120, 190, 255, 255});
    UiText(cpuMode ? modeLabel : "P2: LEFT / RIGHT + ENTER", (Vector2){548, (float)(screenH - 68)}, 14.0f, 1.0f, (Color){255, 130, 130, 255});
    UiText("RETRO FIGHTER STRIP GLIDES WITH YOUR CURSOR", (Vector2){252, (float)(screenH - 44)}, 10.0f, 1.0f, (Color){255, 214, 118, 240});
    if (p1Confirmed) UiText(cpuMode ? "PLAYER LOCKED" : "PLAYER 1 LOCKED", (Vector2){70, (float)(screenH - 24)}, 12.0f, 1.0f, (Color){120, 190, 255, 255});
    if (p2Confirmed) UiText(cpuMode ? "CPU LOCKED" : "PLAYER 2 LOCKED", (Vector2){694, (float)(screenH - 24)}, 12.0f, 1.0f, (Color){255, 130, 130, 255});
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
