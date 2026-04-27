/*
 * mugen_anim.h — Reusable MUGEN Animation System
 *
 * This system loads animations from JSON (pre-converted from .air files)
 * and sprites from extracted SFF folders. It is character-agnostic and
 * designed to work with ANY MUGEN character.
 *
 * Architecture:
 *   1. Offline: Python tools convert .air → JSON, .sff → PNGs
 *   2. Runtime: This header loads the JSON + sprites
 *   3. Gameplay: Fighter struct sets currentAnim, renderer plays it
 */

#ifndef MUGEN_ANIM_H
#define MUGEN_ANIM_H

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ──────────────────── CONFIGURATION ──────────────────── */

#define MA_MAX_ACTIONS       512   /* max different animations per character */
#define MA_MAX_FRAMES        128   /* max frames per animation */
#define MA_MAX_BOXES         8     /* max hit/hurtboxes per frame */
#define MA_SPRITE_CACHE_SIZE 512   /* LRU sprite cache size */

/* ──────────────────── CORE STRUCTS ──────────────────── */

/* A single collision box (in MUGEN coordinate space) */
typedef struct {
    int x1, y1, x2, y2;
} MABox;

/* A single animation frame */
typedef struct {
    int   group;           /* sprite group ID */
    int   item;            /* sprite item ID */
    int   offsetX, offsetY;/* per-frame offset */
    int   duration;        /* ticks (at 60fps) */
    char  flip[8];         /* "H", "V", "HV", or "" */
    MABox hitboxes[MA_MAX_BOXES];   /* attack boxes (Clsn1) */
    int   hitboxCount;
    MABox hurtboxes[MA_MAX_BOXES];  /* vulnerable boxes (Clsn2) */
    int   hurtboxCount;
} MAFrame;

/* A complete animation (one [Begin Action N]) */
typedef struct {
    int     actionId;                /* MUGEN action number */
    MAFrame frames[MA_MAX_FRAMES];
    int     frameCount;
    int     loopStart;               /* -1 = no loop, else frame index */
    int     totalDuration;           /* sum of all frame durations */
} MAAction;

/* ── Sprite cache entry ── */
typedef struct {
    int       group;
    int       item;
    int       axisX, axisY;  /* from offsets.txt */
    Texture2D tex;
    bool      loaded;
    int       lastUsed;      /* frame counter for LRU eviction */
} MACacheEntry;

/* ── The full character animation pack ── */
typedef struct {
    /* Actions indexed by a lookup table */
    MAAction  actions[MA_MAX_ACTIONS];
    int       actionCount;

    /* Sprite data */
    char      spritePath[256];
    
    /* Offset table: group,item → axisX,axisY */
    struct { int group, item, axisX, axisY; } offsets[16000];
    int       offsetCount;

    /* Texture cache (LRU) */
    MACacheEntry cache[MA_SPRITE_CACHE_SIZE];
    int          cacheUsed;
    int          frameCounter;

    bool ready;
} MACharacter;

/* ── Playback state (lives in Fighter or similar struct) ── */
typedef struct {
    int   actionId;       /* which action is playing */
    int   actionIndex;    /* index into MACharacter.actions[] */
    int   currentFrame;   /* current frame index */
    int   ticksInFrame;   /* ticks spent in current frame */
    bool  finished;       /* true if non-looping anim reached end */
} MAPlayback;

/* ──────────────────── SPRITE LOADING ──────────────────── */

/* Find offset entry for a group,item pair */
static int MA_FindOffset(const MACharacter* c, int group, int item, int* axisX, int* axisY) {
    for (int i = 0; i < c->offsetCount; i++) {
        if (c->offsets[i].group == group && c->offsets[i].item == item) {
            *axisX = c->offsets[i].axisX;
            *axisY = c->offsets[i].axisY;
            return 1;
        }
    }
    *axisX = 0;
    *axisY = 0;
    return 0;
}

/* Get a sprite texture, loading it on demand with LRU caching */
static MACacheEntry* MA_GetSprite(MACharacter* c, int group, int item) {
    c->frameCounter++;

    /* Check cache */
    for (int i = 0; i < c->cacheUsed; i++) {
        if (c->cache[i].group == group && c->cache[i].item == item) {
            c->cache[i].lastUsed = c->frameCounter;
            return &c->cache[i];
        }
    }

    /* Not cached — load it */
    char path[512];
    snprintf(path, sizeof(path), "%s/g%04d_i%04d.png", c->spritePath, group, item);

    if (!FileExists(path)) return NULL;

    /* Find slot: unused or LRU evict */
    int slot = -1;
    if (c->cacheUsed < MA_SPRITE_CACHE_SIZE) {
        slot = c->cacheUsed++;
    } else {
        /* Evict LRU */
        int oldest = c->frameCounter;
        slot = 0;
        for (int i = 0; i < MA_SPRITE_CACHE_SIZE; i++) {
            if (c->cache[i].lastUsed < oldest) {
                oldest = c->cache[i].lastUsed;
                slot = i;
            }
        }
        if (c->cache[slot].loaded && c->cache[slot].tex.id > 0) {
            UnloadTexture(c->cache[slot].tex);
        }
    }

    MACacheEntry* e = &c->cache[slot];
    e->group = group;
    e->item = item;
    e->tex = LoadTexture(path);
    e->loaded = (e->tex.id > 0);
    e->lastUsed = c->frameCounter;
    MA_FindOffset(c, group, item, &e->axisX, &e->axisY);

    return e->loaded ? e : NULL;
}

/* ──────────────────── JSON LOADING ──────────────────── */

/* Minimal JSON number parser */
static int MA_ParseInt(const char** p) {
    while (**p && (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r' || **p == ':' || **p == ',')) (*p)++;
    int neg = 0;
    if (**p == '-') { neg = 1; (*p)++; }
    int v = 0;
    while (**p >= '0' && **p <= '9') { v = v * 10 + (**p - '0'); (*p)++; }
    return neg ? -v : v;
}

/* Skip to next occurrence of character */
static void MA_SkipTo(const char** p, char ch) {
    while (**p && **p != ch) (*p)++;
    if (**p == ch) (*p)++;
}

/* Skip whitespace */
static void MA_SkipWS(const char** p) {
    while (**p && (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r')) (*p)++;
}

/* Read a JSON string value (returns pointer into buffer, null-terminates) */
static const char* MA_ReadString(const char** p, char* buf, int bufSize) {
    MA_SkipTo(p, '"');
    int i = 0;
    while (**p && **p != '"' && i < bufSize - 1) { buf[i++] = **p; (*p)++; }
    buf[i] = '\0';
    if (**p == '"') (*p)++;
    return buf;
}

/* Parse a box array like [[-13, -67, 15, 1], ...] */
static int MA_ParseBoxArray(const char** p, MABox* boxes, int maxBoxes) {
    int count = 0;
    MA_SkipTo(p, '[');  /* outer [ */
    MA_SkipWS(p);
    
    while (**p && **p != ']' && count < maxBoxes) {
        if (**p == '[') {
            (*p)++;
            boxes[count].x1 = MA_ParseInt(p);
            boxes[count].y1 = MA_ParseInt(p);
            boxes[count].x2 = MA_ParseInt(p);
            boxes[count].y2 = MA_ParseInt(p);
            MA_SkipTo(p, ']');
            count++;
        } else {
            (*p)++;
        }
    }
    if (**p == ']') (*p)++;
    return count;
}

/* Load animations from JSON file */
static void MA_LoadAnims(MACharacter* c, const char* jsonPath) {
    /* Read entire file */
    int fileSize = 0;
    char* json = LoadFileText(jsonPath);
    if (!json) {
        TraceLog(LOG_WARNING, "MA: Could not load %s", jsonPath);
        return;
    }

    const char* p = json;

    /* Find "actions" object */
    const char* actionsStr = strstr(p, "\"actions\"");
    if (!actionsStr) { UnloadFileText(json); return; }
    p = actionsStr;
    MA_SkipTo(&p, '{');  /* enter actions object */

    c->actionCount = 0;

    /* Parse each action: "ID": { "frames": [...], "loopStart": N } */
    while (*p && *p != '}' && c->actionCount < MA_MAX_ACTIONS) {
        MA_SkipWS(&p);
        if (*p == '}') break;

        /* Read action ID string */
        char idBuf[16];
        if (*p == '"') {
            MA_ReadString(&p, idBuf, sizeof(idBuf));
        } else if (*p == ',') {
            p++;
            continue;
        } else {
            p++;
            continue;
        }

        int actionId = atoi(idBuf);
        MAAction* a = &c->actions[c->actionCount];
        memset(a, 0, sizeof(MAAction));
        a->actionId = actionId;
        a->loopStart = -1;
        a->frameCount = 0;

        MA_SkipTo(&p, '{');  /* enter this action's object */

        /* Parse fields until closing } */
        int depth = 1;
        while (*p && depth > 0) {
            MA_SkipWS(&p);

            if (*p == '}') { depth--; p++; continue; }
            if (*p == '{') { depth++; p++; continue; }

            if (*p == '"') {
                char key[32];
                MA_ReadString(&p, key, sizeof(key));
                MA_SkipTo(&p, ':');
                MA_SkipWS(&p);

                if (strcmp(key, "loopStart") == 0) {
                    a->loopStart = MA_ParseInt(&p);
                }
                else if (strcmp(key, "frames") == 0) {
                    MA_SkipTo(&p, '[');  /* enter frames array */
                    MA_SkipWS(&p);

                    while (*p && *p != ']' && a->frameCount < MA_MAX_FRAMES) {
                        if (*p == '{') {
                            p++;
                            MAFrame* f = &a->frames[a->frameCount];
                            memset(f, 0, sizeof(MAFrame));

                            /* Parse frame fields */
                            while (*p && *p != '}') {
                                MA_SkipWS(&p);
                                if (*p == '"') {
                                    char fkey[32];
                                    MA_ReadString(&p, fkey, sizeof(fkey));
                                    MA_SkipTo(&p, ':');
                                    MA_SkipWS(&p);

                                    if (strcmp(fkey, "group") == 0) f->group = MA_ParseInt(&p);
                                    else if (strcmp(fkey, "item") == 0) f->item = MA_ParseInt(&p);
                                    else if (strcmp(fkey, "offsetX") == 0) f->offsetX = MA_ParseInt(&p);
                                    else if (strcmp(fkey, "offsetY") == 0) f->offsetY = MA_ParseInt(&p);
                                    else if (strcmp(fkey, "duration") == 0) f->duration = MA_ParseInt(&p);
                                    else if (strcmp(fkey, "flip") == 0) MA_ReadString(&p, f->flip, sizeof(f->flip));
                                    else if (strcmp(fkey, "hitboxes") == 0) f->hitboxCount = MA_ParseBoxArray(&p, f->hitboxes, MA_MAX_BOXES);
                                    else if (strcmp(fkey, "hurtboxes") == 0) f->hurtboxCount = MA_ParseBoxArray(&p, f->hurtboxes, MA_MAX_BOXES);
                                    else { /* skip unknown value */ MA_SkipTo(&p, ','); }
                                } else {
                                    p++;
                                }
                            }
                            if (*p == '}') p++;
                            a->frameCount++;
                        } else {
                            p++;
                        }
                    }
                    if (*p == ']') p++;
                }
                else {
                    /* Skip unknown field value */
                    if (*p == '"') { MA_SkipTo(&p, '"'); MA_SkipTo(&p, '"'); }
                    else if (*p == '[') {
                        int d2 = 1; p++;
                        while (*p && d2 > 0) { if (*p == '[') d2++; else if (*p == ']') d2--; p++; }
                    }
                    else if (*p == '{') {
                        int d2 = 1; p++;
                        while (*p && d2 > 0) { if (*p == '{') d2++; else if (*p == '}') d2--; p++; }
                    }
                }
            } else {
                p++;
            }
        }

        /* Compute total duration */
        a->totalDuration = 0;
        for (int i = 0; i < a->frameCount; i++) {
            a->totalDuration += a->frames[i].duration;
        }

        c->actionCount++;
    }

    UnloadFileText(json);
    TraceLog(LOG_INFO, "MA: Loaded %d actions from %s", c->actionCount, jsonPath);
}

/* Load offset table from offsets.txt */
static void MA_LoadOffsets(MACharacter* c, const char* offsetPath) {
    FILE* f = fopen(offsetPath, "r");
    if (!f) return;

    c->offsetCount = 0;
    int g, i, x, y;
    while (fscanf(f, "%d %d %d %d", &g, &i, &x, &y) == 4) {
        if (c->offsetCount < 16000) {
            c->offsets[c->offsetCount].group = g;
            c->offsets[c->offsetCount].item = i;
            c->offsets[c->offsetCount].axisX = x;
            c->offsets[c->offsetCount].axisY = y;
            c->offsetCount++;
        }
    }
    fclose(f);
    TraceLog(LOG_INFO, "MA: Loaded %d sprite offsets", c->offsetCount);
}

/* ──────────────────── PUBLIC API ──────────────────── */

/* Initialize a character from pre-converted data */
static void MA_Init(MACharacter* c, const char* spritePath, const char* animJsonPath) {
    memset(c, 0, sizeof(MACharacter));
    strncpy(c->spritePath, spritePath, sizeof(c->spritePath) - 1);

    char offsetPath[512];
    snprintf(offsetPath, sizeof(offsetPath), "%s/offsets.txt", spritePath);
    MA_LoadOffsets(c, offsetPath);
    MA_LoadAnims(c, animJsonPath);

    c->ready = (c->actionCount > 0);
    TraceLog(LOG_INFO, "MA: Character ready with %d actions, %d offsets",
             c->actionCount, c->offsetCount);
}

/* Clean up */
static void MA_Cleanup(MACharacter* c) {
    for (int i = 0; i < c->cacheUsed; i++) {
        if (c->cache[i].loaded && c->cache[i].tex.id > 0) {
            UnloadTexture(c->cache[i].tex);
        }
    }
    memset(c, 0, sizeof(MACharacter));
}

/* Find an action by MUGEN action ID */
static int MA_FindAction(const MACharacter* c, int actionId) {
    for (int i = 0; i < c->actionCount; i++) {
        if (c->actions[i].actionId == actionId) return i;
    }
    return -1;
}

/* Set playback to a specific action */
static void MA_SetAction(MAPlayback* pb, const MACharacter* c, int actionId) {
    int idx = MA_FindAction(c, actionId);
    if (idx < 0) return;
    if (pb->actionId == actionId && !pb->finished) return; /* already playing */
    pb->actionId = actionId;
    pb->actionIndex = idx;
    pb->currentFrame = 0;
    pb->ticksInFrame = 0;
    pb->finished = false;
}

/* Force-set (even if same action) */
static void MA_ForceAction(MAPlayback* pb, const MACharacter* c, int actionId) {
    int idx = MA_FindAction(c, actionId);
    if (idx < 0) return;
    pb->actionId = actionId;
    pb->actionIndex = idx;
    pb->currentFrame = 0;
    pb->ticksInFrame = 0;
    pb->finished = false;
}

/* Advance playback by one tick */
static void MA_Tick(MAPlayback* pb, const MACharacter* c) {
    if (pb->finished) return;
    if (pb->actionIndex < 0 || pb->actionIndex >= c->actionCount) return;

    const MAAction* a = &c->actions[pb->actionIndex];
    if (a->frameCount <= 0) return;

    pb->ticksInFrame++;

    const MAFrame* f = &a->frames[pb->currentFrame];
    if (pb->ticksInFrame >= f->duration) {
        pb->ticksInFrame = 0;
        pb->currentFrame++;

        if (pb->currentFrame >= a->frameCount) {
            if (a->loopStart >= 0) {
                pb->currentFrame = a->loopStart;
            } else {
                pb->currentFrame = a->frameCount - 1;
                pb->finished = true;
            }
        }
    }
}

/* Get the current frame */
static const MAFrame* MA_CurrentFrame(const MAPlayback* pb, const MACharacter* c) {
    if (pb->actionIndex < 0 || pb->actionIndex >= c->actionCount) return NULL;
    const MAAction* a = &c->actions[pb->actionIndex];
    if (a->frameCount <= 0) return NULL;
    int idx = pb->currentFrame;
    if (idx < 0) idx = 0;
    if (idx >= a->frameCount) idx = a->frameCount - 1;
    return &a->frames[idx];
}

/* Get animation progress (0.0 to 1.0) */
static float MA_Progress(const MAPlayback* pb, const MACharacter* c) {
    if (pb->actionIndex < 0 || pb->actionIndex >= c->actionCount) return 0.0f;
    const MAAction* a = &c->actions[pb->actionIndex];
    if (a->totalDuration <= 0) return 0.0f;

    int elapsed = 0;
    for (int i = 0; i < pb->currentFrame && i < a->frameCount; i++) {
        elapsed += a->frames[i].duration;
    }
    elapsed += pb->ticksInFrame;
    return (float)elapsed / (float)a->totalDuration;
}

/* ──────────────────── RENDERING ──────────────────── */

/* Draw the current animation frame */
static bool MA_Draw(MACharacter* c, const MAPlayback* pb,
                    float posX, float posY, int facingDir, float scale) {
    const MAFrame* f = MA_CurrentFrame(pb, c);
    if (!f) return false;
    if (f->group < 0) return false;  /* group -1 = invisible frame */

    MACacheEntry* spr = MA_GetSprite(c, f->group, f->item);
    if (!spr || !spr->loaded) return false;

    float drawW = (float)spr->tex.width * scale;
    float drawH = (float)spr->tex.height * scale;

    Rectangle source = { 0, 0, (float)spr->tex.width, (float)spr->tex.height };

    /* Apply facing direction + flip flags */
    bool flipH = (facingDir < 0);
    if (f->flip[0] == 'H' || (f->flip[0] == 'H' && f->flip[1] == 'V')) flipH = !flipH;
    if (flipH) source.width = -source.width;
    if (f->flip[0] == 'V' || (f->flip[1] == 'V')) source.height = -source.height;

    /* Origin = axis point from offsets.txt, scaled */
    Vector2 origin = {
        (float)(spr->axisX + f->offsetX) * scale,
        (float)(spr->axisY + f->offsetY) * scale
    };

    Rectangle dest = { posX, posY, drawW, drawH };

    DrawTexturePro(spr->tex, source, dest, origin, 0.0f, WHITE);
    return true;
}

/* Get hitboxes in world space for the current frame */
static int MA_GetHitboxes(const MAPlayback* pb, const MACharacter* c,
                          float posX, float posY, int facingDir, float scale,
                          Rectangle* outBoxes, int maxOut) {
    const MAFrame* f = MA_CurrentFrame(pb, c);
    if (!f) return 0;
    int count = 0;
    for (int i = 0; i < f->hitboxCount && count < maxOut; i++) {
        float x1 = (float)f->hitboxes[i].x1 * scale;
        float y1 = (float)f->hitboxes[i].y1 * scale;
        float x2 = (float)f->hitboxes[i].x2 * scale;
        float y2 = (float)f->hitboxes[i].y2 * scale;
        if (facingDir < 0) { x1 = -x1; x2 = -x2; }
        float lx = fminf(x1, x2);
        float ly = fminf(y1, y2);
        outBoxes[count++] = (Rectangle){
            posX + lx, posY + ly,
            fabsf(x2 - x1), fabsf(y2 - y1)
        };
    }
    return count;
}

static int MA_GetHurtboxes(const MAPlayback* pb, const MACharacter* c,
                           float posX, float posY, int facingDir, float scale,
                           Rectangle* outBoxes, int maxOut) {
    const MAFrame* f = MA_CurrentFrame(pb, c);
    if (!f) return 0;
    int count = 0;
    for (int i = 0; i < f->hurtboxCount && count < maxOut; i++) {
        float x1 = (float)f->hurtboxes[i].x1 * scale;
        float y1 = (float)f->hurtboxes[i].y1 * scale;
        float x2 = (float)f->hurtboxes[i].x2 * scale;
        float y2 = (float)f->hurtboxes[i].y2 * scale;
        if (facingDir < 0) { x1 = -x1; x2 = -x2; }
        float lx = fminf(x1, x2);
        float ly = fminf(y1, y2);
        outBoxes[count++] = (Rectangle){
            posX + lx, posY + ly,
            fabsf(x2 - x1), fabsf(y2 - y1)
        };
    }
    return count;
}

#endif /* MUGEN_ANIM_H */
