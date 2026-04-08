# JJK Fighter Engine

A custom-built 2D fighting game engine in C inspired by *Jujutsu Kaisen*, focused on asymmetric character rules, hard resource tradeoffs, and high-impact state transitions. The project uses Raylib for rendering and keeps gameplay, data, and presentation responsibilities separated so the codebase stays readable as mechanics scale.

## Architecture

The project is organized around a decoupled gameplay loop rather than a monolithic single-file prototype:

- `main.c` is the gameplay coordinator. It drives the main loop, input processing, finite state machine transitions, movement, CE spending, healing, projectile flow, and domain-state resolution.
- `characters.c` and `characters.h` form the character data dictionary. They provide the single source of truth for roster identity, base stats, domain ownership, trait flags, and per-character combat modifiers.
- `render.c` and `render.h` implement the view pipeline. This layer owns arena drawing, HUD rendering, particle systems, announcements, screen shake, and domain visuals so rendering logic stays out of the gameplay controller.
- `netcode.c` and `netcode.h` provide a dedicated ENet-based networking layer for transmitting player input without polluting the core combat loop with transport concerns.

This separation makes the engine easier to extend with new fighters, new rendering effects, and future combat subsystems without forcing every change through the main loop.

## Core Mechanics

### Cursed Energy (CE) Economy

Cursed Energy is the engine's central resource gate for standard sorcerers. Each eligible fighter manages a CE reserve beneath the HP bar, and that reserve determines whether they can sustain offense, healing, or a late-round power spike.

- **Projectiles** consume Positive Cursed Energy and turn CE into ranged pressure.
- **Reversed Cursed Technique (RCT)** converts CE directly into survivability, letting players rapidly spend meter to recover HP.
- **Domain Expansion** is a high-commitment ultimate that requires a large flat CE cost.
- **Passive regeneration** creates the strategic tension: spending too aggressively on RCT or ranged pressure can leave a player unable to access their Domain when the round pivots.

The result is a deliberate resource-management loop where meter discipline matters as much as execution.

### Domain Expansions and Domain Clashes

Domain Expansion is the engine's signature state-machine event. Both players begin with access to their Domain when their character supports one, and triggering it shifts the match into `STATE_DOMAIN`.

When a Domain is cast:

- the screen transitions into a dedicated domain presentation state,
- the trapped opponent is immediately stunned,
- a visible counter window begins,
- and the defender is given a narrow opportunity to respond with their own Domain.

If the defending player has sufficient CE and activates their Domain during the counter window, the game resolves a **Domain Clash**. The clash winner is decided by remaining CE reserve, and the loser takes damage equal to the CE difference. If the timer expires first, the sure-hit effect lands for **150 HP**, or **75 HP** against Toji.

This mechanic gives the engine a dramatic mid-round state swing while still preserving counterplay through meter awareness and timing discipline.

### Heavenly Restriction: Toji Fushiguro

Toji is the deliberate exception that proves the system's rules. His `isHeavenlyRestricted` flag fundamentally changes how the engine treats him:

- he has **no CE bar** and remains locked at zero meter,
- he cannot use projectiles, RCT, or Domain Expansion,
- he gains elite mobility and high physical damage,
- and he has a unique directional dash mapped to `Q`.

Most importantly, Toji breaks the Domain rule set:

- he is **immune to Domain Expansion stun lock**,
- he can move freely while an opponent's Domain is active,
- and the sure-hit penalty is reduced from **150 HP** to **75 HP**.

That asymmetry gives the roster a strong systems identity: most characters solve problems with cursed techniques, while Toji wins by violating the cursed-technique economy entirely.

## Build Instructions

### 1. Install Raylib in MSYS2 UCRT64

```bash
pacman -S mingw-w64-ucrt-x86_64-raylib
```

### 2. Build the game with GCC

```bash
gcc main.c characters.c render.c -o jjk_game -lraylib -lgdi32 -lwinmm
```

If you plan to wire in the networking layer during your local build, make sure ENet is also installed and linked for `netcode.c` as part of your MSYS2 toolchain.

## Downloadable Build

The repository includes a ready-to-run Windows build:

- `jjk_game.exe`
- `Urusai_Mania_Portable/`
- `assets/menu_theme.mp3`
- `assets/menu_frames/`
- `assets/fonts/GOUDOS.TTF`

For the easiest Git download, use the full `Urusai_Mania_Portable/` folder so the executable, looping music, retro font, and animated menu frames stay together.

## Controls

### Character Select

- **Player 1**: `A` / `D` to move the selection cursor, `Space` to confirm.
- **Player 2**: `Left Arrow` / `Right Arrow` to move the selection cursor, `Enter` to confirm.

### In-Match Controls

| Action | Player 1 | Player 2 |
| --- | --- | --- |
| Move Left | `A` | `Left Arrow` |
| Move Right | `D` | `Right Arrow` |
| Jump | `W` | `Up Arrow` |
| Crouch | `Left Shift` | `Right Shift` |
| Attack | `F` | `Numpad 0` |
| Reversed Cursed Technique (Heal) | `C` | `Numpad 1` |
| Domain Expansion | `R` | `Numpad 2` |
| Dodge / Toji Dash | `Q` | `Numpad 3` |
| Ultimate Ability | `X` | `Numpad 4` |

### Match Flow

- `Enter` restarts from the game-over screen.
- `Esc` opens the in-match pause menu and also returns from sub-menus.
- Toji uses the same dodge input as other fighters, but because of Heavenly Restriction this functions as his signature directional mobility tool instead of a cursed-technique action.

### Front-End Menu

- `W` / `S` or `Up Arrow` / `Down Arrow` move through the main menu and pause menu.
- `Enter` or `Space` confirms a menu selection.
- The title screen loops the bundled 8-bit opening theme and animated menu background.
- The front-end uses a bundled retro DOS-style font for the `Urusai Mania` presentation.

## Repository Contents

```text
jjk_game.exe   -> packaged Windows build for direct play
main.c         -> game loop, state machine, input, and combat flow
characters.c   -> character stats, traits, and roster configuration
characters.h   -> character data contracts and trait definitions
render.c       -> Raylib rendering, particles, HUD, and screen effects
render.h       -> rendering subsystem interfaces
netcode.c      -> ENet-based input synchronization layer
netcode.h      -> networking API and packet structures
```

## Version Control Policy

The repository tracks source, headers, documentation, and the primary packaged Windows build `jjk_game.exe`. Auxiliary binaries and intermediate artifacts remain excluded so the repo stays focused on the canonical source plus one directly playable executable.
