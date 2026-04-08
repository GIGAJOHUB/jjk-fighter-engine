// =============================================================================
// characters.h — JJK Fighter: Character Data Layer Interface
// =============================================================================
// This file defines the CharacterData struct and the public API for retrieving
// per-character base stats. All combat.c / main.c logic should call
// GetCharacterData(charID) rather than hard-coding values.
// =============================================================================

#ifndef CHARACTERS_H
#define CHARACTERS_H

#include "raylib.h"
#include <stdbool.h>

// ---------------------------------------------------------------------------
// CHARACTER IDs — used everywhere as a stable integer key
// ---------------------------------------------------------------------------
typedef enum {
    CHAR_SUKUNA = 0,
    CHAR_GOJO   = 1,
    CHAR_TOJI   = 2,
    CHAR_YUTA   = 3,
    CHAR_YUJI   = 4,
    CHAR_COUNT
} CharacterID;

// ---------------------------------------------------------------------------
// TRAIT FLAGS — booleans the combat engine reads each frame / per-hit
// ---------------------------------------------------------------------------
typedef struct {

    // --- Toji Fushiguro: Heavenly Restriction ---
    // When true:
    //   • maxCE is forced to 0. All CE-cost gates return false.
    //   • Cannot use CE attacks, projectiles, or RCT.
    //   • Immune to Domain stun-lock (skip IsStunned flag).
    //   • Domain sure-hit damage reduced to 50% MaxHP instead of 80%.
    bool isHeavenlyRestricted;

    // --- Satoru Gojo: Six Eyes ---
    // When true:
    //   • Multiply all CE costs by ceCostMultiplier (near zero).
    //   • Combat engine: cost = baseCost * data.ceCostMultiplier
    bool hasSixEyes;
    float ceCostMultiplier; // 0.01f for Gojo, 1.0f for everyone else
    float rctCostMultiplier;

    // --- Ryomen Sukuna: Reverse Cursed Technique Efficiency ---
    // When true:
    //   • RCT heals rctHealMultiplier × base heal amount.
    //   • Combat engine: healAmt = BASE_RCT_HEAL * data.rctHealMultiplier
    bool hasRCTEfficiency;
    float rctHealMultiplier; // 2.5f for Sukuna, 1.0f for others

    // --- Yuta Okkotsu: Copy ---
    // When true:
    //   • On killing a projectile with a block, copy its damage type.
    //   • CE regeneration rate is doubled.
    //   • Combat engine: if (attacker.hasCopy) ceRegen *= 2.0f
    bool hasCopy;

    // --- Yuji Itadori: Black Flash ---
    // When true:
    //   • Each standard melee hit rolls blackFlashChance (0.0–1.0).
    //   • On success, damage *= blackFlashMultiplier.
    //   • Combat engine: if (hasBF && roll < chance) dmg *= multiplier
    bool hasBlackFlash;
    float blackFlashChance;     // 0.20f = 20% per hit
    float blackFlashMultiplier; // 2.5f damage multiplier on proc

} CharacterTraits;

// ---------------------------------------------------------------------------
// CHARACTER DATA — complete snapshot for one character
// ---------------------------------------------------------------------------
typedef struct {

    // Identity
    CharacterID id;
    const char* name;
    const char* fullTitle;         // Flavor name, e.g. "King of Curses"

    // Core combat stats
    float maxHP;
    float maxCE;                   // 0.0f for Heavenly Restricted
    float baseSpeed;
    float baseAttackDamage;
    float projectileDamage;

    // Domain Expansion
    const char* domainName;        // "None" if unavailable
    bool        hasDomain;         // false for Toji and (lore-balanced) Yuji
    const char* ultimateName;

    // Unique trait bundle — read by combat engine every calculation
    CharacterTraits traits;

    // Visuals (Raylib)
    Color bodyColor;               // Rectangle fill on the arena
    Color ceColor;                 // CE bar accent color
    Color domainColor;             // Screen tint during Domain Expansion
    Color domainAccentColor;       // Secondary domain particle/glow color

} CharacterData;

// ---------------------------------------------------------------------------
// PUBLIC API
// ---------------------------------------------------------------------------

// Returns a fully populated CharacterData for the given CharacterID.
// Call once during fighter initialization; cache the result in Fighter struct.
CharacterData GetCharacterData(CharacterID id);

// Returns the display name for a CharacterID (convenience wrapper).
const char* GetCharacterName(CharacterID id);

#endif // CHARACTERS_H
