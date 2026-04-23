import sys
import re
import os

def patch_character_sukuna_sprite():
    path = "character_sukuna_sprite.h"
    with open(path, "r") as f:
        content = f.read()

    # Update LoadSukunaSpritePack paths if any, wait, it's done in main.c
    
    # Update anim values
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_IDLE,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_IDLE,         10000, 10);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_WALK,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_WALK,         10020, 10);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_JUMP,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_JUMP,         10040, 10);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_CROUCH,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_CROUCH,       10010, 10);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_ATTACK_LIGHT,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_ATTACK_LIGHT, 11000, 10);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_ATTACK_MED,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_ATTACK_MED,   11000, 10);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_ATTACK_HEAVY,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_ATTACK_HEAVY, 11000, 10);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_BLOCK,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_BLOCK,        10000, 10);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_HIT,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_HIT,          10000, 10);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_KNOCKDOWN,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_KNOCKDOWN,    10500, 20);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_DODGE,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_DODGE,        10040, 10);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_SPECIAL1,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_SPECIAL1,     11000, 20);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_SPECIAL2,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_SPECIAL2,     15000, 20);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_DOMAIN,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_DOMAIN,       10000, 50);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_INTRO,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_INTRO,        10000, 20);', content)
    content = re.sub(r'SukunaBuildAnim\(SUKU_ANIM_ULT,\s*\d+,\s*\d+\);', 'SukunaBuildAnim(SUKU_ANIM_ULT,          18400, 20);', content)
    
    with open(path, "w") as f:
        f.write(content)

def patch_main_c():
    path = "main.c"
    with open(path, "r") as f:
        content = f.read()

    # Update Sukuna path
    content = content.replace('LoadSukunaSpritePack("assets/sprites/sukuna_s1");', 'LoadSukunaSpritePack("assets/sprites/meguna");')

    # Update GameState state
    content = content.replace('GameState state = STATE_MAIN_MENU;', 'GameState state = STATE_INTRO;\n    int randomMusic = GetRandomValue(1, 3);')

    # Find the main while loop: "while (!WindowShouldClose()) {"
    idx = content.find("while (!WindowShouldClose()) {")
    if idx == -1: return

    # Add the intro state handling right inside the loop
    intro_code = """
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
                bgm = LoadMusicStream("assets/menu_theme.mp3");
                SetMusicVolume(bgm, 0.55f);
                PlayMusicStream(bgm);
                musicLoaded = true;
            }
            EndDrawing();
            continue;
        }
"""
    # Insert right after `while (!WindowShouldClose()) {`
    idx_end = idx + len("while (!WindowShouldClose()) {")
    content = content[:idx_end] + "\n" + intro_code + content[idx_end:]

    # Now handle random music when entering STATE_BATTLE
    # I'll just find all 'state = STATE_BATTLE;' and append a music switch
    # We should add a helper function `ChangeBattleMusic` or just inline it.
    
    inline_music_code = r"""state = STATE_BATTLE;
                        {
                            randomMusic = GetRandomValue(1, 3);
                            if (musicLoaded) { StopMusicStream(bgm); UnloadMusicStream(bgm); }
                            char mpath[256];
                            if (randomMusic == 1) strcpy(mpath, "assets/music/track1.mp3");
                            else if (randomMusic == 2) strcpy(mpath, "assets/music/track2.mp3");
                            else strcpy(mpath, "assets/music/track3.mp3");
                            bgm = LoadMusicStream(mpath);
                            SetMusicVolume(bgm, 0.60f);
                            PlayMusicStream(bgm);
                            musicLoaded = true;
                        }"""
    content = content.replace("state = STATE_BATTLE;", inline_music_code)

    with open(path, "w") as f:
        f.write(content)

patch_character_sukuna_sprite()
patch_main_c()
