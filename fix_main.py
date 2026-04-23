import re

with open("main.c", "r") as f:
    content = f.read()

bad_code = r"""state = STATE_BATTLE;
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

content = content.replace(bad_code, "state = STATE_BATTLE;")

# Add prevState to main()
content = content.replace("GameState state = STATE_INTRO;", "GameState state = STATE_INTRO;\n    GameState prevState = STATE_INTRO;")

# Add transition check in main loop
transition_code = """
        if (state == STATE_BATTLE && prevState != STATE_BATTLE && prevState != STATE_PAUSE) {
            int rnd = GetRandomValue(1, 3);
            if (musicLoaded) { StopMusicStream(bgm); UnloadMusicStream(bgm); }
            if (rnd == 1) bgm = LoadMusicStream("assets/music/track1.mp3");
            else if (rnd == 2) bgm = LoadMusicStream("assets/music/track2.mp3");
            else bgm = LoadMusicStream("assets/music/track3.mp3");
            SetMusicVolume(bgm, 0.60f);
            PlayMusicStream(bgm);
            musicLoaded = true;
        }
        prevState = state;
"""

# Insert right after `while (!WindowShouldClose()) {`
idx = content.find("while (!WindowShouldClose()) {")
idx_end = idx + len("while (!WindowShouldClose()) {")
content = content[:idx_end] + "\n" + transition_code + content[idx_end:]

with open("main.c", "w") as f:
    f.write(content)
