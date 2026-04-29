import sys
import re
with open("main.c", "r") as f:
    content = f.read()

target = 'bgm = LoadMusicStream("assets/menu_theme.mp3");\n                SetMusicVolume(bgm, 0.55f);'
replacement = '''int rnd = GetRandomValue(1, 4);
                if (rnd == 1) bgm = LoadMusicStream("assets/music/track1.mp3");
                else if (rnd == 2) bgm = LoadMusicStream("assets/music/track2.mp3");
                else if (rnd == 3) bgm = LoadMusicStream("assets/music/track3.mp3");
                else bgm = LoadMusicStream("assets/menu_theme.mp3");
                SetMusicVolume(bgm, 0.60f);'''

content = content.replace(target, replacement)
with open("main.c", "w") as f:
    f.write(content)
