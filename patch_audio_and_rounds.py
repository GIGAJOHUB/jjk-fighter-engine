import re

with open("main.c", "r", encoding="utf-8") as f:
    c = f.read()

# 1. Add Sound variables for rounds and target volume
if "Sound gSfxRound1;" not in c:
    var_block = '''static float gTargetMusicVol = 0.6f;
static float gCurrentMusicVol = 0.0f;
static Sound gSfxRound1;
static Sound gSfxRound2;
static Sound gSfxRound3;
static Sound gSfxFight;
static bool gRoundsLoaded = false;
'''
    # insert after "#define HITSTOP_LIGHT"
    c = c.replace("#define HITSTOP_LIGHT", var_block + "\n#define HITSTOP_LIGHT")

# 2. Add loading of round sounds
load_block = '''    if (!gRoundsLoaded) {
        gSfxRound1 = LoadSound("assets/sounds/round1.ogg");
        gSfxRound2 = LoadSound("assets/sounds/round2.ogg");
        gSfxRound3 = LoadSound("assets/sounds/round3.ogg");
        gSfxFight  = LoadSound("assets/sounds/fight.ogg");
        gRoundsLoaded = true;
    }'''
# Find InitAudioDevice() and add after
if load_block not in c:
    c = c.replace("InitAudioDevice();", "InitAudioDevice();\n" + load_block)

# 3. Modify music routing for Menu and Intro
menu_music_old = '''{ int rndM = GetRandomValue(0, 1);
                  if (rndM == 0) bgm = LoadMusicStream("assets/music/track1.mp3");
                  else bgm = LoadMusicStream("assets/menu_theme.mp3"); }'''
menu_music_new = '''bgm = LoadMusicStream("assets/music/track1.mp3");
                gTargetMusicVol = 0.6f;
                gCurrentMusicVol = 0.6f;'''
c = c.replace(menu_music_old, menu_music_new)

# 4. Modify music routing for Battle (fade in)
battle_music_old = '''{ int rndB = GetRandomValue(0, 2);
              if (rndB == 0) bgm = LoadMusicStream("assets/music/track2.mp3");
              else if (rndB == 1) bgm = LoadMusicStream("assets/music/track3.mp3");
              else bgm = LoadMusicStream("assets/menu_theme.mp3"); }
            SetMusicVolume(bgm, 0.60f);'''
battle_music_new = '''{ int rndB = GetRandomValue(0, 2);
              if (rndB == 0) bgm = LoadMusicStream("assets/music/track2.mp3");
              else if (rndB == 1) bgm = LoadMusicStream("assets/music/track3.mp3");
              else bgm = LoadMusicStream("assets/menu_theme.mp3"); }
            gTargetMusicVol = 0.6f;
            gCurrentMusicVol = 0.0f;
            SetMusicVolume(bgm, gCurrentMusicVol);'''
c = c.replace(battle_music_old, battle_music_new)

# 5. Add Fade in/out logic to the main loop (where UpdateMusicStream is called)
update_music_old = '''if (musicLoaded) {
        UpdateMusicStream(bgm);
    }'''
update_music_new = '''if (musicLoaded) {
        if (state == STATE_GAME_OVER) { gTargetMusicVol = 0.0f; }
        else if (state == STATE_BATTLE) { gTargetMusicVol = 0.6f; }
        if (gCurrentMusicVol < gTargetMusicVol) {
            gCurrentMusicVol += GetFrameTime() * 0.3f;
            if (gCurrentMusicVol > gTargetMusicVol) gCurrentMusicVol = gTargetMusicVol;
        } else if (gCurrentMusicVol > gTargetMusicVol) {
            gCurrentMusicVol -= GetFrameTime() * 0.3f;
            if (gCurrentMusicVol < gTargetMusicVol) gCurrentMusicVol = gTargetMusicVol;
        }
        SetMusicVolume(bgm, gCurrentMusicVol);
        UpdateMusicStream(bgm);
    }'''
c = c.replace(update_music_old, update_music_new)

# 6. Countdown logic - Replace the entire countdown phase inside SimulateBattleFrame
countdown_old = '''    /* 3-2-1 countdown then FIGHT! */
    if (!round->countdownDone) {
        if (round->countdownTimer > 0.0f) {
            round->countdownTimer -= GetFrameTime();
            if (round->countdownTimer < 0.0f) round->countdownTimer = 0.0f;
            int cnt = (int)ceilf(round->countdownTimer);
            snprintf(round->bannerText, sizeof(round->bannerText), "%d", cnt > 0 ? cnt : 1);
            round->subText[0] = '\\0';
        } else if (round->fightBannerTimer < 2.0f) {
            round->fightBannerTimer += GetFrameTime();
            snprintf(round->bannerText, sizeof(round->bannerText), "FIGHT!");
            round->subText[0] = '\\0';
            if (!round->roundActive) round->roundActive = true;
        } else {
            round->countdownDone = true;
            round->bannerText[0] = '\\0';
            round->subText[0] = '\\0';
        }
        if (round->countdownTimer > 0.0f) canAct = false; /* freeze until countdown hits 0 */
        round->introTimer = 0.0f; /* neutralise old system */
    }'''

countdown_new = '''    /* 3-second ROUND text then FIGHT! with SFX */
    if (!round->countdownDone) {
        if (round->countdownTimer == 3.0f) {
            /* Play Round X sound right at the start */
            if (round->roundNumber == 1) PlaySound(gSfxRound1);
            else if (round->roundNumber == 2) PlaySound(gSfxRound2);
            else PlaySound(gSfxRound3);
            round->countdownTimer -= 0.001f; /* slightly nudge so it doesn't trigger again */
        }

        if (round->countdownTimer > 0.0f) {
            round->countdownTimer -= GetFrameTime();
            if (round->countdownTimer < 0.0f) round->countdownTimer = 0.0f;
            
            /* Text stays as ROUND X */
            snprintf(round->bannerText, sizeof(round->bannerText), "ROUND %d", round->roundNumber);
            round->subText[0] = '\\0';
            
            if (round->countdownTimer == 0.0f) {
                PlaySound(gSfxFight);
            }
        } else if (round->fightBannerTimer < 2.0f) {
            round->fightBannerTimer += GetFrameTime();
            snprintf(round->bannerText, sizeof(round->bannerText), "FIGHT!");
            round->subText[0] = '\\0';
            if (!round->roundActive) round->roundActive = true;
        } else {
            round->countdownDone = true;
            round->bannerText[0] = '\\0';
            round->subText[0] = '\\0';
        }
        if (round->countdownTimer > 0.0f) canAct = false; /* freeze until countdown hits 0 */
        round->introTimer = 0.0f; /* neutralise old system */
    }'''
c = c.replace(countdown_old, countdown_new)


with open("main.c", "w", encoding="utf-8") as f:
    f.write(c)

print("main.c patched with audio and rounds logic!")
