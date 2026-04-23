# Fix character_sukuna_sprite.h:
# 1. Add SFX support (load/play sounds at key events)
# 2. Fix group IDs using actual MegunaV5 SFF groups (from sff_info output)
# 3. Fix black sprite issue: only use groups that ACTUALLY exist in offsets.txt

import os, re

# Read what groups actually exist in the extracted sprites
offsets_path = r"assets\sprites\meguna\offsets.txt"
groups_with_frames = {}
with open(offsets_path, "r") as f:
    for line in f:
        parts = line.strip().split()
        if len(parts) >= 2:
            g = int(parts[0])
            i = int(parts[1])
            if g not in groups_with_frames:
                groups_with_frames[g] = []
            groups_with_frames[g].append(i)

print("Groups available:", sorted(groups_with_frames.keys())[:40])

# Based on MUGEN sff_info output and AIR file analysis:
# Action 0 = Idle: group 10000 items 2,4,5
# g=0 = palette/shared sprites (avoid - black)
# g=20 = walk: items 0-6
# g=40 = jump: items 0-2
# g=10 = crouch: items 0-1
# g=200 = attacks (many items)
# g=1000 = heavy hits
# g=11500 = intro
# Real idle is group 0 item 0..9? Let's check what Action 0 uses in AIR
# From AIR: 10000,4 means group=10000, item=4 for idle
# These ARE in offsets.txt with g=10000

available = sorted(groups_with_frames.keys())
print("All groups:", available[:60])
