import os

with open("character_sukuna_sprite.h", "r", encoding="utf-8") as f:
    c = f.read()

# Replace SukunaBuildAnim
old_func = '''static void SukunaBuildAnim(int animId, int group, int maxItems) {
    SukunaAnim* a = &gSukuna.anims[animId];
    a->frameCount = 0;
    /* We just look for entries already in our offset table */
    for (int i = 0; i < gSukuna.count && a->frameCount < SUKU_MAX_ANIM_FRAMES; i++) {
        if (gSukuna.entries[i].group == group) {
            a->frames[a->frameCount].group = group;
            a->frames[a->frameCount].item = gSukuna.entries[i].item;
            a->frameCount++;
        }
    }
}'''

new_func = '''static void SukunaBuildAnimExact(int animId, int group, int count, const int* items) {
    SukunaAnim* a = &gSukuna.anims[animId];
    a->frameCount = 0;
    for (int k = 0; k < count && a->frameCount < SUKU_MAX_ANIM_FRAMES; k++) {
        /* Only add if it exists in the loaded entries, though we will trust the caller mostly */
        a->frames[a->frameCount].group = group;
        a->frames[a->frameCount].item = items[k];
        a->frameCount++;
    }
}'''
if old_func in c:
    c = c.replace(old_func, new_func)

old_builds = '''    /* Verified group IDs from MegunaV5 SFF extraction */
    SukunaBuildAnim(SUKU_ANIM_IDLE,         10000, 20);  /* idle breathing loop */
    SukunaBuildAnim(SUKU_ANIM_WALK,            20, 20);  /* walk cycle */
    SukunaBuildAnim(SUKU_ANIM_JUMP,            40, 10);  /* jump arc */
    SukunaBuildAnim(SUKU_ANIM_CROUCH,          10, 10);  /* crouch */
    SukunaBuildAnim(SUKU_ANIM_ATTACK_LIGHT,   200, 30);  /* light jabs */
    SukunaBuildAnim(SUKU_ANIM_ATTACK_MED,     201, 20);  /* medium strikes */
    SukunaBuildAnim(SUKU_ANIM_ATTACK_HEAVY,   210, 30);  /* heavy combo */
    SukunaBuildAnim(SUKU_ANIM_BLOCK,         1000, 10);  /* guard stance */
    SukunaBuildAnim(SUKU_ANIM_HIT,           5000, 10);  /* hit reaction */
    SukunaBuildAnim(SUKU_ANIM_KNOCKDOWN,      210, 50);  /* tumble/fall */
    SukunaBuildAnim(SUKU_ANIM_DODGE,        10040, 10);  /* step dodge */
    SukunaBuildAnim(SUKU_ANIM_SPECIAL1,     11000, 20);  /* Dismantle slash */
    SukunaBuildAnim(SUKU_ANIM_SPECIAL2,     15000, 20);  /* Cleave sweep */
    SukunaBuildAnim(SUKU_ANIM_DOMAIN,       20300, 80);  /* Malevolent Shrine */
    SukunaBuildAnim(SUKU_ANIM_INTRO,         9898, 20);  /* intro pose */
    SukunaBuildAnim(SUKU_ANIM_ULT,          18400, 30);  /* Fuga Musou */'''

new_builds = '''    /* Exact frame maps from KingRyomenSukuna.air */
    { const int items[] = {4, 5, 2, 5}; SukunaBuildAnimExact(SUKU_ANIM_IDLE, 10000, 4, items); }
    { const int items[] = {8, 9, 10, 11, 12, 13, 14}; SukunaBuildAnimExact(SUKU_ANIM_WALK, 10020, 7, items); }
    { const int items[] = {0}; SukunaBuildAnimExact(SUKU_ANIM_JUMP, 10010, 1, items); }
    { const int items[] = {0, 1}; SukunaBuildAnimExact(SUKU_ANIM_CROUCH, 10010, 2, items); }
    { const int items[] = {0, 1, 2, 3, 4, 5, 6}; SukunaBuildAnimExact(SUKU_ANIM_ATTACK_LIGHT, 200, 7, items); }
    { const int items[] = {0, 1, 2, 3}; SukunaBuildAnimExact(SUKU_ANIM_ATTACK_MED, 210, 4, items); }
    { const int items[] = {0, 1, 2, 3, 4, 5, 6, 7}; SukunaBuildAnimExact(SUKU_ANIM_ATTACK_HEAVY, 500, 8, items); }
    { const int items[] = {8}; SukunaBuildAnimExact(SUKU_ANIM_BLOCK, 10040, 1, items); }
    { const int items[] = {0, 1, 2, 3, 4, 5}; SukunaBuildAnimExact(SUKU_ANIM_HIT, 5000, 6, items); }
    { const int items[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}; SukunaBuildAnimExact(SUKU_ANIM_KNOCKDOWN, 5050, 12, items); }
    { const int items[] = {0, 1, 2, 3, 4, 5, 6}; SukunaBuildAnimExact(SUKU_ANIM_DODGE, 100, 7, items); }
    { const int items[] = {0, 1, 2, 3}; SukunaBuildAnimExact(SUKU_ANIM_SPECIAL1, 11000, 4, items); }
    { const int items[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}; SukunaBuildAnimExact(SUKU_ANIM_SPECIAL2, 15000, 15, items); }
    { const int items[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30}; SukunaBuildAnimExact(SUKU_ANIM_DOMAIN, 20300, 31, items); }
    { const int items[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}; SukunaBuildAnimExact(SUKU_ANIM_INTRO, 190, 10, items); }
    { const int items[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26}; SukunaBuildAnimExact(SUKU_ANIM_ULT, 18400, 27, items); }'''
if old_builds in c:
    c = c.replace(old_builds, new_builds)

with open("character_sukuna_sprite.h", "w", encoding="utf-8") as f:
    f.write(c)

print("character_sukuna_sprite.h patched!")
