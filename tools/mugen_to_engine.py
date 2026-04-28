#!/usr/bin/env python3
"""
mugen_to_engine.py — Offline MUGEN → Engine Pipeline
Parses .def, .cns, .air, .cmd into a single chardata.json

Usage:
    python mugen_to_engine.py <mugen_char_dir> <output_dir>
    
Example:
    python tools/mugen_to_engine.py character_sprites/TND-Gojo assets/characters/gojo
"""

import sys, os, re, json, glob

# ─────────── PARSERS ───────────

def parse_def(path):
    """Parse .def file → dict of sections"""
    result = {}
    section = None
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith(';'):
                continue
            m = re.match(r'\[(\w+)\]', line, re.IGNORECASE)
            if m:
                section = m.group(1).lower()
                result[section] = {}
                continue
            if section and '=' in line:
                k, v = line.split('=', 1)
                result[section][k.strip().lower()] = v.strip().strip('"')
    return result

def parse_cns_constants(path):
    """Parse [Data], [Size], [Velocity], [Movement] from .cns"""
    result = {
        'data': {}, 'size': {}, 'velocity': {}, 'movement': {}
    }
    section = None
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith(';'):
                continue
            m = re.match(r'\[(\w+)\]', line, re.IGNORECASE)
            if m:
                sec = m.group(1).lower()
                if sec in result:
                    section = sec
                else:
                    section = None
                continue
            if section and '=' in line:
                k, v = line.split('=', 1)
                k = k.strip().lower()
                v = v.split(';')[0].strip()
                # Try to parse as number(s)
                parts = [p.strip() for p in v.split(',')]
                nums = []
                for p in parts:
                    try:
                        nums.append(float(p) if '.' in p else int(p))
                    except:
                        nums.append(p)
                result[section][k] = nums[0] if len(nums) == 1 else nums
    return result

def parse_statedefs(path):
    """Parse all [Statedef N] and their [State N, *] controllers from .cns files.
    Extracts: anim, HitDef damage/timing, PlaySnd, VelSet, ChangeState, etc."""
    states = {}
    current_statedef = None
    current_state_block = None
    
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            raw = line
            line = line.strip()
            if not line or line.startswith(';'):
                continue
            
            # Match [Statedef N] or [StateDef N]
            m = re.match(r'\[Statedef\s+(-?\d+)\]', line, re.IGNORECASE)
            if m:
                sid = int(m.group(1))
                current_statedef = sid
                states[sid] = {
                    'type': 'S', 'movetype': 'I', 'physics': 'S',
                    'anim': sid, 'velset': None, 'ctrl': None,
                    'hitdefs': [], 'sounds': [], 'velsets': [],
                    'changestates': [], 'helpers': []
                }
                current_state_block = None
                continue
            
            # Match [State N, label]
            m = re.match(r'\[State\s+[^,\]]*,?\s*(.*?)\]', line, re.IGNORECASE)
            if m and current_statedef is not None:
                current_state_block = {'type': None}
                continue
            
            if current_statedef is None:
                continue
            
            s = states[current_statedef]
            
            # Parse key = value
            if '=' in line:
                k, v = line.split('=', 1)
                k = k.strip().lower()
                v = v.split(';')[0].strip()
                
                # Statedef properties
                if k == 'type' and current_state_block is None:
                    s['type'] = v.upper()
                elif k == 'movetype' and current_state_block is None:
                    s['movetype'] = v.upper()
                elif k == 'physics' and current_state_block is None:
                    s['physics'] = v.upper()
                elif k == 'anim' and current_state_block is None:
                    try: s['anim'] = int(v)
                    except: pass
                elif k == 'ctrl' and current_state_block is None:
                    try: s['ctrl'] = int(v)
                    except: pass
                elif k == 'velset' and current_state_block is None:
                    try:
                        parts = v.split(',')
                        s['velset'] = [float(p.strip()) for p in parts]
                    except: pass
                
                # State controller properties
                if current_state_block is not None:
                    if k == 'type':
                        current_state_block['type'] = v
                        if v.lower() == 'hitdef':
                            s['hitdefs'].append({})
                        elif v.lower() == 'playsnd':
                            s['sounds'].append({})
                    elif k == 'damage' and s['hitdefs']:
                        try:
                            parts = v.split(',')
                            s['hitdefs'][-1]['damage'] = int(parts[0])
                            if len(parts) > 1:
                                s['hitdefs'][-1]['guard_damage'] = int(parts[1])
                        except: pass
                    elif k == 'animtype' and s['hitdefs']:
                        s['hitdefs'][-1]['animtype'] = v
                    elif k == 'ground.velocity' and s['hitdefs']:
                        try:
                            parts = v.split(',')
                            s['hitdefs'][-1]['ground_velocity'] = [float(p) for p in parts]
                        except: pass
                    elif k == 'air.velocity' and s['hitdefs']:
                        try:
                            parts = v.split(',')
                            s['hitdefs'][-1]['air_velocity'] = [float(p) for p in parts]
                        except: pass
                    elif k == 'pausetime' and s['hitdefs']:
                        try:
                            parts = v.split(',')
                            s['hitdefs'][-1]['pausetime'] = [int(p.strip()) for p in parts]
                        except: pass
                    elif k == 'ground.hittime' and s['hitdefs']:
                        try: s['hitdefs'][-1]['ground_hittime'] = int(v)
                        except: pass
                    elif k == 'ground.slidetime' and s['hitdefs']:
                        try: s['hitdefs'][-1]['ground_slidetime'] = int(v)
                        except: pass
                    elif k == 'value' and current_state_block.get('type','').lower() == 'playsnd' and s['sounds']:
                        s['sounds'][-1]['value'] = v
                    elif k == 'value' and current_state_block.get('type','').lower() == 'changestate':
                        try: s['changestates'].append(int(v))
                        except: pass
    
    return states

def parse_air(path):
    """Parse .air → dict of action_id → {frames: [...], loopStart: int}
    Each frame: {group, item, offsetX, offsetY, duration, flip, hitboxes, hurtboxes}"""
    actions = {}
    current_action = None
    current_frames = []
    loop_start = -1
    pending_clsn1 = []  # hitboxes (attack)
    pending_clsn2 = []  # hurtboxes (vulnerable)
    clsn1_default = []
    clsn2_default = []
    
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith(';'):
                continue
            
            # [Begin Action N]
            m = re.match(r'\[Begin Action\s+(\d+)\]', line, re.IGNORECASE)
            if m:
                if current_action is not None:
                    actions[current_action] = {
                        'frames': current_frames,
                        'loopStart': loop_start
                    }
                current_action = int(m.group(1))
                current_frames = []
                loop_start = -1
                pending_clsn1 = []
                pending_clsn2 = []
                clsn1_default = []
                clsn2_default = []
                continue
            
            if current_action is None:
                continue
            
            # Loopstart
            if line.lower() == 'loopstart':
                loop_start = len(current_frames)
                continue
            
            # Clsn1Default / Clsn2Default
            m = re.match(r'Clsn1Default:\s*(\d+)', line, re.IGNORECASE)
            if m:
                clsn1_default = []
                continue
            m = re.match(r'Clsn2Default:\s*(\d+)', line, re.IGNORECASE)
            if m:
                clsn2_default = []
                continue
            
            # Clsn1: N / Clsn2: N (per-frame)
            m = re.match(r'Clsn1:\s*(\d+)', line, re.IGNORECASE)
            if m:
                pending_clsn1 = []
                continue
            m = re.match(r'Clsn2:\s*(\d+)', line, re.IGNORECASE)
            if m:
                pending_clsn2 = []
                continue
            
            # Clsn box data
            m = re.match(r'Clsn([12])\[(\d+)\]\s*=\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)', line, re.IGNORECASE)
            if m:
                box = [int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6))]
                if m.group(1) == '1':
                    if not pending_clsn1 and clsn1_default is not None:
                        pending_clsn1 = list(clsn1_default) if clsn1_default else []
                    pending_clsn1.append(box)
                    clsn1_default.append(box)
                else:
                    if not pending_clsn2 and clsn2_default is not None:
                        pending_clsn2 = list(clsn2_default) if clsn2_default else []
                    pending_clsn2.append(box)
                    clsn2_default.append(box)
                continue
            
            # Frame data: group, item, offsetX, offsetY, duration[, flip][, blend]
            m = re.match(r'(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)(.*)', line)
            if m:
                group = int(m.group(1))
                item = int(m.group(2))
                offx = int(m.group(3))
                offy = int(m.group(4))
                dur = int(m.group(5))
                rest = m.group(6).strip().lstrip(',').strip()
                
                flip = ''
                if rest:
                    parts = [p.strip() for p in rest.split(',')]
                    for p in parts:
                        if p.upper() in ('H', 'V', 'HV', 'VH'):
                            flip = p.upper()
                
                frame = {
                    'group': group,
                    'item': item,
                    'offsetX': offx,
                    'offsetY': offy,
                    'duration': dur,
                    'flip': flip,
                    'hitboxes': pending_clsn1 if pending_clsn1 else clsn1_default[:],
                    'hurtboxes': pending_clsn2 if pending_clsn2 else clsn2_default[:]
                }
                current_frames.append(frame)
                pending_clsn1 = []
                pending_clsn2 = []
                continue
    
    # Save last action
    if current_action is not None:
        actions[current_action] = {
            'frames': current_frames,
            'loopStart': loop_start
        }
    
    return actions

def parse_cmd(path):
    """Parse .cmd → list of commands and state entries"""
    commands = []
    state_entries = []
    current = None
    in_statedef = False
    
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith(';'):
                continue
            
            # [Command]
            if re.match(r'\[command\]', line, re.IGNORECASE):
                current = {}
                commands.append(current)
                continue
            
            # [State -1, label]
            m = re.match(r'\[State\s+-1\s*,\s*(.*?)\]', line, re.IGNORECASE)
            if m:
                current = {'label': m.group(1), 'value': None, 'triggers': []}
                state_entries.append(current)
                continue
            
            if current and '=' in line:
                k, v = line.split('=', 1)
                k = k.strip().lower()
                v = v.split(';')[0].strip()
                
                if k == 'name':
                    current['name'] = v.strip('"')
                elif k == 'command':
                    current['command'] = v
                elif k == 'time':
                    try: current['time'] = int(v)
                    except: pass
                elif k == 'value':
                    try: current['value'] = int(v)
                    except: pass
    
    return {'commands': commands, 'state_entries': state_entries}


def build_chardata(mugen_dir, output_dir):
    """Main pipeline: read all files, build chardata.json"""
    
    # Find files
    def_files = glob.glob(os.path.join(mugen_dir, '*.def'))
    if not def_files:
        print(f"ERROR: No .def file found in {mugen_dir}")
        return
    
    def_data = parse_def(def_files[0])
    print(f"[DEF] Parsed: {def_files[0]}")
    
    # Get file paths from .def
    files_section = def_data.get('files', {})
    air_file = os.path.join(mugen_dir, files_section.get('anim', ''))
    cns_file = os.path.join(mugen_dir, files_section.get('cns', ''))
    cmd_file = os.path.join(mugen_dir, files_section.get('cmd', ''))
    common_cns = os.path.join(mugen_dir, files_section.get('stcommon', ''))
    
    # Parse constants from main CNS
    constants = {}
    if os.path.exists(cns_file):
        constants = parse_cns_constants(cns_file)
        print(f"[CNS] Constants: {cns_file}")
    
    # Parse statedefs from all CNS files
    all_states = {}
    for cns_path in [cns_file, common_cns] + glob.glob(os.path.join(mugen_dir, '*.cns')):
        if os.path.exists(cns_path):
            states = parse_statedefs(cns_path)
            for sid, sdata in states.items():
                if sid not in all_states or (sdata['hitdefs'] and not all_states[sid].get('hitdefs')):
                    all_states[sid] = sdata
            print(f"[CNS] States: {cns_path} ({len(states)} statedefs)")
    
    # Parse animations
    anims = {}
    if os.path.exists(air_file):
        anims = parse_air(air_file)
        print(f"[AIR] Animations: {air_file} ({len(anims)} actions)")
    
    # Parse commands
    cmd_data = {}
    if os.path.exists(cmd_file):
        cmd_data = parse_cmd(cmd_file)
        print(f"[CMD] Commands: {cmd_file} ({len(cmd_data.get('commands',[]))} commands, {len(cmd_data.get('state_entries',[]))} entries)")
    
    # Build physics from CNS constants
    vel = constants.get('velocity', {})
    mov = constants.get('movement', {})
    size = constants.get('size', {})
    data = constants.get('data', {})
    
    physics = {
        'walk_fwd': vel.get('walk.fwd', 1.5),
        'walk_back': vel.get('walk.back', -1.5),
        'run_fwd': vel.get('run.fwd', [10.6, 0]),
        'run_back': vel.get('run.back', [-4.5, -3.8]),
        'jump_y': vel.get('jump.neu', [0, -8.4]),
        'jump_fwd_x': vel.get('jump.fwd', 2.5),
        'jump_back_x': vel.get('jump.back', -2.55),
        'yaccel': mov.get('yaccel', 0.44),
        'stand_friction': mov.get('stand.friction', 0.85),
        'crouch_friction': mov.get('crouch.friction', 0.82),
        'airjump_num': mov.get('airjump.num', 0),
        'airjump_height': mov.get('airjump.height', 35),
    }
    
    char_size = {
        'xscale': size.get('xscale', 1),
        'yscale': size.get('yscale', 1),
        'ground_back': size.get('ground.back', 15),
        'ground_front': size.get('ground.front', 16),
        'height': size.get('height', 60),
        'attack_dist': size.get('attack.dist', 160),
        'head_pos': size.get('head.pos', [-5, -50]),
        'mid_pos': size.get('mid.pos', [-5, -25]),
    }
    
    char_data = {
        'life': data.get('life', 1000),
        'power': data.get('power', 3000),
        'attack': data.get('attack', 100),
        'defence': data.get('defence', 100),
    }
    
    # Build state → action mapping for key states
    state_action_map = {}
    for sid, sdata in all_states.items():
        entry = {
            'type': sdata['type'],
            'movetype': sdata['movetype'],
            'physics': sdata['physics'],
            'anim': sdata['anim'],
        }
        if sdata['velset']:
            entry['velset'] = sdata['velset']
        if sdata['hitdefs']:
            entry['hitdefs'] = sdata['hitdefs']
        if sdata['sounds']:
            entry['sounds'] = sdata['sounds']
        if sdata['changestates']:
            entry['next_states'] = sdata['changestates']
        state_action_map[str(sid)] = entry
    
    # Build command → state mapping from cmd
    input_map = {}
    if cmd_data.get('state_entries'):
        for entry in cmd_data['state_entries']:
            if entry.get('value') and entry.get('label'):
                input_map[entry['label']] = entry['value']
    
    # Assemble final JSON
    chardata = {
        'name': def_data.get('info', {}).get('displayname', 'Unknown'),
        'mugen_version': def_data.get('info', {}).get('mugenversion', '1.0'),
        'physics': physics,
        'size': char_size,
        'data': char_data,
        'input_map': input_map,
        'states': state_action_map,
    }
    
    # Write chardata.json
    os.makedirs(output_dir, exist_ok=True)
    chardata_path = os.path.join(output_dir, 'chardata.json')
    with open(chardata_path, 'w') as f:
        json.dump(chardata, f, indent=2)
    print(f"\n[OUTPUT] chardata.json -> {chardata_path} ({os.path.getsize(chardata_path)} bytes)")
    
    # Write anims.json
    anims_path = os.path.join(output_dir, 'anims.json')
    anims_out = {
        'source': os.path.basename(air_file),
        'actions': {str(k): v for k, v in anims.items()}
    }
    with open(anims_path, 'w') as f:
        json.dump(anims_out, f, indent=2)
    print(f"[OUTPUT] anims.json -> {anims_path} ({os.path.getsize(anims_path)} bytes)")
    
    # Summary
    print(f"\n{'='*60}")
    print(f"Character: {chardata['name']}")
    print(f"Physics: yaccel={physics['yaccel']}, walk_fwd={physics['walk_fwd']}")
    print(f"Size: height={char_size['height']}, ground_front={char_size['ground_front']}")
    print(f"States: {len(state_action_map)} total")
    print(f"Animations: {len(anims)} actions")
    
    # Find states with HitDefs
    hit_states = {k: v for k, v in state_action_map.items() if v.get('hitdefs')}
    print(f"Attack states: {len(hit_states)}")
    for sid, sdata in sorted(hit_states.items(), key=lambda x: int(x[0])):
        for hd in sdata['hitdefs']:
            dmg = hd.get('damage', '?')
            print(f"  State {sid}: damage={dmg}, anim={sdata['anim']}")
    
    print(f"{'='*60}")
    return chardata


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    build_chardata(sys.argv[1], sys.argv[2])
