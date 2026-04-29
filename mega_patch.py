import re

with open("main.c", "r", encoding="utf-8", errors="replace") as f:
    c = f.read()

# ── 1. MUSIC: menu uses track1/menu_theme randomly ──────────────────────────
# Fix intro -> menu transition: only track1 or menu_theme
c = c.replace(
    '''if (musicLoaded) { StopMusicStream(bgm); UnloadMusicStream(bgm); }
                int rnd = GetRandomValue(1, 4);
                if (rnd == 1) bgm = LoadMusicStream("assets/music/track1.mp3");
                else if (rnd == 2) bgm = LoadMusicStream("assets/music/track2.mp3");
                else if (rnd == 3) bgm = LoadMusicStream("assets/music/track3.mp3");
                else bgm = LoadMusicStream("assets/menu_theme.mp3");
                SetMusicVolume(bgm, 0.60f);
                PlayMusicStream(bgm);
                musicLoaded = true;''',
    '''if (musicLoaded) { StopMusicStream(bgm); UnloadMusicStream(bgm); }
                { int rndM = GetRandomValue(0, 1);
                  if (rndM == 0) bgm = LoadMusicStream("assets/music/track1.mp3");
                  else bgm = LoadMusicStream("assets/menu_theme.mp3"); }
                SetMusicVolume(bgm, 0.60f);
                PlayMusicStream(bgm);
                musicLoaded = true;'''
)

# Fix battle music: track2/track3/menu_theme, ONE per fight (skip if already in battle)
c = c.replace(
    '''if (state == STATE_BATTLE && prevState != STATE_BATTLE && prevState != STATE_PAUSE) {
            int rnd = GetRandomValue(1, 3);
            if (musicLoaded) { StopMusicStream(bgm); UnloadMusicStream(bgm); }
            if (rnd == 1) bgm = LoadMusicStream("assets/music/track1.mp3");
            else if (rnd == 2) bgm = LoadMusicStream("assets/music/track2.mp3");
            else bgm = LoadMusicStream("assets/music/track3.mp3");
            SetMusicVolume(bgm, 0.60f);
            PlayMusicStream(bgm);
            musicLoaded = true;
        }''',
    '''/* Battle music: pick once per match (not per round) */
        if (state == STATE_BATTLE && prevState == STATE_CHAR_SELECT) {
            if (musicLoaded) { StopMusicStream(bgm); UnloadMusicStream(bgm); }
            { int rndB = GetRandomValue(0, 2);
              if (rndB == 0) bgm = LoadMusicStream("assets/music/track2.mp3");
              else if (rndB == 1) bgm = LoadMusicStream("assets/music/track3.mp3");
              else bgm = LoadMusicStream("assets/menu_theme.mp3"); }
            SetMusicVolume(bgm, 0.60f);
            PlayMusicStream(bgm);
            musicLoaded = true;
        }'''
)

# ── 2. DOMAIN CLASH: proportional CE comparison ────────────────────────────
old_clash = '''    clash->damage   = fabsf(caster->cursedEnergy - challenger->cursedEnergy);

    if (fabsf(caster->cursedEnergy - challenger->cursedEnergy) < 0.5f) {
        clash->winnerPlayer = 0;
        clash->damage = 0.0f;
    } else if (challenger->cursedEnergy > caster->cursedEnergy) {
        clash->winnerPlayer = challengerPlayer;
    } else {
        clash->winnerPlayer = casterPlayer;
    }'''

new_clash = '''    /* Proportional CE comparison: ratio of (my CE / maxCE) vs opponent */
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
    }'''

c = c.replace(old_clash, new_clash)

# ── 3. DOMAIN CE COST: 10% of caster's maxCE (fix CalcCECost usage) ────────
# Replace StartDomain ceCost line
c = c.replace(
    'static void StartDomain(Fighter* caster, Fighter* target, bool isP1,\n                        GameState* state, int* domainCasterPlayer, float* domainTimer) {\n    float ceCost = CalcCECost(caster, DOMAIN_CE_COST);\n    if (caster->isHeavenlyRestricted || !caster->hasDomain || caster->cursedEnergy < ceCost) return;\n\n    caster->cursedEnergy -= ceCost;',
    'static void StartDomain(Fighter* caster, Fighter* target, bool isP1,\n                        GameState* state, int* domainCasterPlayer, float* domainTimer) {\n    float ceCost = caster->maxCE * DOMAIN_CE_COST; /* 10% of max CE */\n    if (caster->isHeavenlyRestricted || !caster->hasDomain || caster->cursedEnergy < ceCost) return;\n\n    caster->cursedEnergy -= ceCost;'
)

# Replace TriggerDomainClash ceCost line
c = c.replace(
    'static void TriggerDomainClash(Fighter* challenger, Fighter* caster, bool challengerIsP1,\n                               GameState* state, float* domainTimer, DomainClashState* clash) {\n    float ceCost = CalcCECost(challenger, DOMAIN_CE_COST);\n    int challengerPlayer = challengerIsP1 ? 1 : 2;\n    int casterPlayer = challengerIsP1 ? 2 : 1;\n\n    if (challenger->isHeavenlyRestricted || !challenger->hasDomain || challenger->cursedEnergy < ceCost) return;\n\n    challenger->cursedEnergy -= ceCost;',
    'static void TriggerDomainClash(Fighter* challenger, Fighter* caster, bool challengerIsP1,\n                               GameState* state, float* domainTimer, DomainClashState* clash) {\n    float ceCost = challenger->maxCE * DOMAIN_CE_COST; /* 10% of max CE */\n    int challengerPlayer = challengerIsP1 ? 1 : 2;\n    int casterPlayer = challengerIsP1 ? 2 : 1;\n\n    if (challenger->isHeavenlyRestricted || !challenger->hasDomain || challenger->cursedEnergy < ceCost) return;\n\n    challenger->cursedEnergy -= ceCost;'
)

# Fix AI domain check
c = c.replace(
    'if (canDomainCounter && cpu->hasDomain && cpu->cursedEnergy >= CalcCECost(cpu, DOMAIN_CE_COST)) input.domain = true;',
    'if (canDomainCounter && cpu->hasDomain && cpu->cursedEnergy >= cpu->maxCE * DOMAIN_CE_COST) input.domain = true;'
)
# AI shouldn't spam domain at round start - add CE threshold
c = c.replace(
    'if (state == STATE_BATTLE && fullCe && gap > 120.0f && cpu->hasDomain && aggro > 0.65f) input.domain = true;',
    'if (state == STATE_BATTLE && fullCe && gap > 120.0f && cpu->hasDomain && aggro > 0.65f && round->roundTimer < ROUND_TIME_SECONDS - 15.0f) input.domain = true;'
)

# ── 4. ROUND COUNTDOWN: 3s countdown + 2s FIGHT banner ─────────────────────
# Replace ROUND_INTRO_TIME usage with new countdown system
# Add countdownPhase to RoundState - patch ResetRoundState and StartNextRound
c = c.replace(
    '    bool roundActive;\n    bool matchPoint;\n    char bannerText[64];\n    char subText[64];\n} RoundState;',
    '    bool roundActive;\n    bool matchPoint;\n    char bannerText[64];\n    char subText[64];\n    float countdownTimer; /* 3..0 countdown */\n    float fightBannerTimer; /* FIGHT! shown for 2s */\n    bool countdownDone;\n} RoundState;'
)

c = c.replace(
    '    round->introTimer = ROUND_INTRO_TIME;\n    round->endTimer = 0.0f;\n    round->roundActive = false;\n    round->matchPoint = false;\n    snprintf(round->bannerText, sizeof(round->bannerText), "ROUND 1");\n    snprintf(round->subText, sizeof(round->subText), "FIGHT");\n}',
    '    round->introTimer = ROUND_INTRO_TIME;\n    round->endTimer = 0.0f;\n    round->roundActive = false;\n    round->matchPoint = false;\n    round->countdownTimer = 3.0f;\n    round->fightBannerTimer = 0.0f;\n    round->countdownDone = false;\n    snprintf(round->bannerText, sizeof(round->bannerText), "ROUND 1");\n    snprintf(round->subText, sizeof(round->subText), "FIGHT");\n}'
)

c = c.replace(
    '    round->introTimer = ROUND_INTRO_TIME;\n    round->endTimer = 0.0f;\n    round->roundActive = false;\n    snprintf(round->bannerText, sizeof(round->bannerText), "ROUND %d", round->roundNumber);\n    snprintf(round->subText, sizeof(round->subText), "FIGHT");\n}',
    '    round->introTimer = ROUND_INTRO_TIME;\n    round->endTimer = 0.0f;\n    round->roundActive = false;\n    round->countdownTimer = 3.0f;\n    round->fightBannerTimer = 0.0f;\n    round->countdownDone = false;\n    snprintf(round->bannerText, sizeof(round->bannerText), "ROUND %d", round->roundNumber);\n    snprintf(round->subText, sizeof(round->subText), "FIGHT");\n}'
)

# Replace the introTimer countdown logic in SimulateBattleFrame
c = c.replace(
    '    if (round->introTimer > 0.0f) {\n        round->introTimer -= GetFrameTime();\n        if (round->introTimer <= 0.0f) {\n            round->introTimer = 0.0f;\n            round->roundActive = true;\n        }\n        canAct = false;\n    }',
    '''    /* 3-2-1 countdown then FIGHT! */
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
)

with open("main.c", "w", encoding="utf-8") as f:
    f.write(c)

print("main.c patched OK")
