#!/usr/bin/env python3
"""
MUGEN .AIR → JSON Converter
Parses a MUGEN animation file and outputs clean structured JSON
for use by a custom C/raylib fighting game engine.

Usage:
    python air_to_json.py <path_to.air> <output.json>

Output format:
{
  "actions": {
    "0": {
      "frames": [
        {
          "group": 0, "item": 2,
          "offsetX": 0, "offsetY": 0,
          "duration": 9,
          "flip": "",
          "hurtboxes": [[-13, -67, 15, 1]],
          "hitboxes": []
        }, ...
      ],
      "loopStart": -1
    }, ...
  }
}
"""

import re
import json
import sys
import os


def parse_air(filepath):
    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    actions = {}
    # Split into action blocks
    pattern = r"\[Begin Action (\d+)\]\s*\n(.*?)(?=\[Begin Action|\Z)"
    for m in re.finditer(pattern, content, re.DOTALL | re.IGNORECASE):
        action_id = m.group(1)
        body = m.group(2)

        frames = []
        loop_start = -1
        frame_idx = 0

        # Current collision boxes (apply to next frame line)
        current_clsn1 = []  # hitboxes (attack)
        current_clsn2 = []  # hurtboxes (vulnerable)
        current_clsn1_default = []
        current_clsn2_default = []
        reading_clsn1 = False
        reading_clsn2 = False

        for raw_line in body.split("\n"):
            line = raw_line.strip()

            # Skip empty / comment
            if not line or line.startswith(";"):
                continue

            # LoopStart marker
            if line.lower() == "loopstart":
                loop_start = frame_idx
                continue

            # Interpolate lines (skip for now)
            if line.lower().startswith("interpolate"):
                continue

            # Clsn1Default (attack hitbox defaults)
            clsn1d_match = re.match(r"Clsn1Default:\s*(\d+)", line, re.IGNORECASE)
            if clsn1d_match:
                current_clsn1_default = []
                reading_clsn1 = True
                reading_clsn2 = False
                continue

            # Clsn2Default (hurt hitbox defaults)
            clsn2d_match = re.match(r"Clsn2Default:\s*(\d+)", line, re.IGNORECASE)
            if clsn2d_match:
                current_clsn2_default = []
                reading_clsn2 = True
                reading_clsn1 = False
                continue

            # Clsn1 (per-frame attack hitbox header)
            clsn1_match = re.match(r"Clsn1:\s*(\d+)", line, re.IGNORECASE)
            if clsn1_match:
                current_clsn1 = []
                reading_clsn1 = True
                reading_clsn2 = False
                continue

            # Clsn2 (per-frame hurt hitbox header)
            clsn2_match = re.match(r"Clsn2:\s*(\d+)", line, re.IGNORECASE)
            if clsn2_match:
                current_clsn2 = []
                reading_clsn2 = True
                reading_clsn1 = False
                continue

            # Clsn box definition
            box_match = re.match(
                r"Clsn[12]\[\d+\]\s*=\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)",
                line, re.IGNORECASE,
            )
            if box_match:
                box = [int(box_match.group(i)) for i in range(1, 5)]
                if reading_clsn1:
                    current_clsn1.append(box)
                elif reading_clsn2:
                    current_clsn2.append(box)
                continue

            # Frame line: group,item, offsetX,offsetY, duration[, flip[, blend[, scaleX,scaleY]]]
            frame_match = re.match(
                r"(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)(.*)?",
                line,
            )
            if frame_match:
                group = int(frame_match.group(1))
                item = int(frame_match.group(2))
                ox = int(frame_match.group(3))
                oy = int(frame_match.group(4))
                dur = int(frame_match.group(5))
                rest = frame_match.group(6) or ""

                # Parse optional flip field
                flip = ""
                extra_parts = [p.strip() for p in rest.split(",")]
                if len(extra_parts) >= 2 and extra_parts[1]:
                    flip = extra_parts[1]

                # Build hurtboxes: per-frame overrides defaults
                hurtboxes = current_clsn2 if current_clsn2 else list(current_clsn2_default)
                hitboxes = current_clsn1 if current_clsn1 else list(current_clsn1_default)

                frames.append({
                    "group": group,
                    "item": item,
                    "offsetX": ox,
                    "offsetY": oy,
                    "duration": dur,
                    "flip": flip,
                    "hurtboxes": hurtboxes,
                    "hitboxes": hitboxes,
                })

                # Reset per-frame boxes
                current_clsn1 = []
                current_clsn2 = []
                reading_clsn1 = False
                reading_clsn2 = False
                frame_idx += 1
                continue

        if frames:
            actions[action_id] = {
                "frames": frames,
                "loopStart": loop_start,
            }

    return actions


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.air> <output.json>")
        sys.exit(1)

    air_path = sys.argv[1]
    out_path = sys.argv[2]

    if not os.path.exists(air_path):
        print(f"ERROR: File not found: {air_path}")
        sys.exit(1)

    actions = parse_air(air_path)

    result = {"source": os.path.basename(air_path), "actions": actions}

    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)

    print(f"Converted {len(actions)} actions -> {out_path}")


if __name__ == "__main__":
    main()
