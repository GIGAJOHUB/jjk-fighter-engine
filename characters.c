// =============================================================================
// characters.c — JJK Fighter: Character Data Layer Implementation
// =============================================================================
// All stat values here are the single source of truth. Do NOT hard-code
// character stats elsewhere. The combat engine reads these through
// GetCharacterData() at fighter init time.
// =============================================================================

#include "characters.h"
#include <string.h>

// ---------------------------------------------------------------------------
// INTERNAL: Default / "no trait" bundle — applied to all characters first,
// then overridden per-character below.
// ---------------------------------------------------------------------------
static CharacterTraits DefaultTraits(void) {
    CharacterTraits t = {0};
    t.isHeavenlyRestricted = false;
    t.hasSixEyes           = false;
    t.ceCostMultiplier     = 1.0f;
    t.hasRCTEfficiency     = false;
    t.rctHealMultiplier    = 1.0f;
    t.hasCopy              = false;
    t.hasBlackFlash        = false;
    t.blackFlashChance     = 0.0f;
    t.blackFlashMultiplier = 1.0f;
    return t;
}

// ---------------------------------------------------------------------------
// GetCharacterData()
// ---------------------------------------------------------------------------
CharacterData GetCharacterData(CharacterID id) {
    CharacterData d;
    memset(&d, 0, sizeof(CharacterData));

    // Start every character with default (no-op) traits
    d.traits = DefaultTraits();

    switch (id) {

        // -------------------------------------------------------------------
        // 0. RYOMEN SUKUNA — King of Curses
        // -------------------------------------------------------------------
        case CHAR_SUKUNA: {
            d.id           = CHAR_SUKUNA;
            d.name         = "SUKUNA";
            d.fullTitle    = "King of Curses";

            // Highest overall: HP, CE, damage
            d.maxHP           = 250.0f;
            d.maxCE           = 500.0f; // Absolute highest CE pool
            d.baseSpeed       = 5.2f;
            d.baseAttackDamage= 28.0f;

            // Domain: Malevolent Shrine — slash sure-hit
            d.domainName = "Malevolent Shrine";
            d.hasDomain  = true;

            // Trait: Reverse Cursed Technique Efficiency
            // Combat engine: healAmt = BASE_RCT_HEAL * traits.rctHealMultiplier
            d.traits.hasRCTEfficiency  = true;
            d.traits.rctHealMultiplier = 2.5f; // Heals 2.5× base RCT amount

            // Visuals
            d.bodyColor        = (Color){200, 40,  40,  255}; // Blood crimson
            d.ceColor          = (Color){255, 100, 30,  255}; // Scorched orange CE
            d.domainColor      = (Color){30,  0,   0,   255}; // Deep void red
            d.domainAccentColor= (Color){180, 20,  20,  255}; // Slash glow
            break;
        }

        // -------------------------------------------------------------------
        // 1. SATORU GOJO — The Honored One
        // -------------------------------------------------------------------
        case CHAR_GOJO: {
            d.id           = CHAR_GOJO;
            d.name         = "GOJO";
            d.fullTitle    = "The Honored One";

            d.maxHP           = 200.0f;
            d.maxCE           = 300.0f; // High — but Six Eyes burns almost none
            d.baseSpeed       = 6.5f;   // Extremely high — fastest non-Toji
            d.baseAttackDamage= 22.0f;

            // Domain: Unlimited Void — absolute stun lock, no counter window
            // Combat engine note: when Gojo's domain fires, skip the counter
            // window entirely (domainCounterWindow = 0.0f).
            d.domainName = "Unlimited Void";
            d.hasDomain  = true;

            // Trait: Six Eyes — near-zero CE costs
            // Combat engine: actualCost = baseCost * traits.ceCostMultiplier
            d.traits.hasSixEyes      = true;
            d.traits.ceCostMultiplier= 0.01f; // 1% of any CE cost

            // Visuals
            d.bodyColor        = (Color){80,  180, 255, 255}; // Ice blue
            d.ceColor          = (Color){180, 240, 255, 255}; // Pale cyan CE
            d.domainColor      = (Color){0,   0,   15,  255}; // Void black
            d.domainAccentColor= (Color){200, 220, 255, 255}; // White starfield
            break;
        }

        // -------------------------------------------------------------------
        // 2. TOJI FUSHIGURO — Sorcerer Killer
        // -------------------------------------------------------------------
        case CHAR_TOJI: {
            d.id           = CHAR_TOJI;
            d.name         = "TOJI";
            d.fullTitle    = "Sorcerer Killer";

            d.maxHP           = 220.0f;
            d.maxCE           = 0.0f;   // STRICTLY zero — Heavenly Restriction
            d.baseSpeed       = 9.5f;   // Absolute fastest in the roster
            d.baseAttackDamage= 38.0f;  // Massive physical multiplier

            // No domain — Heavenly Restriction prevents CE techniques entirely
            d.domainName = "None";
            d.hasDomain  = false;

            // Trait: Heavenly Restriction
            // Combat engine usage:
            //   • Block CE gates: if (traits.isHeavenlyRestricted) return; // no CE move
            //   • Skip domain stun: stunned = stunned && !traits.isHeavenlyRestricted
            //   • Domain sure-hit: pct = isHeavenlyRestricted ? 0.50f : 0.80f
            d.traits.isHeavenlyRestricted = true;

            // Visuals
            d.bodyColor        = (Color){200, 200, 200, 255}; // Stark silver
            d.ceColor          = (Color){80,  80,  80,  255}; // No CE — grey bar
            d.domainColor      = (Color){20,  20,  20,  255}; // N/A but dark
            d.domainAccentColor= (Color){160, 160, 160, 255};
            break;
        }

        // -------------------------------------------------------------------
        // 3. YUTA OKKOTSU — The Prodigy
        // -------------------------------------------------------------------
        case CHAR_YUTA: {
            d.id           = CHAR_YUTA;
            d.name         = "YUTA";
            d.fullTitle    = "The Prodigy";

            d.maxHP           = 190.0f;
            d.maxCE           = 400.0f; // 2nd highest, behind only Sukuna
            d.baseSpeed       = 5.8f;
            d.baseAttackDamage= 24.0f;

            // Domain: Authentic Mutual Love — Rika manifestation
            d.domainName = "Authentic Mutual Love";
            d.hasDomain  = true;

            // Trait: Copy — doubles CE regen, can mirror projectile damage
            // Combat engine: if (traits.hasCopy) ceRegenRate *= 2.0f
            d.traits.hasCopy = true;

            // Visuals
            d.bodyColor        = (Color){180, 100, 255, 255}; // Deep violet
            d.ceColor          = (Color){220, 160, 255, 255}; // Soft lavender CE
            d.domainColor      = (Color){20,  0,   35,  255}; // Rika void
            d.domainAccentColor= (Color){255, 180, 255, 255}; // Rika pink glow
            break;
        }

        // -------------------------------------------------------------------
        // 4. YUJI ITADORI — Tiger of West High
        // -------------------------------------------------------------------
        case CHAR_YUJI: {
            d.id           = CHAR_YUJI;
            d.name         = "YUJI";
            d.fullTitle    = "Tiger of West High";

            d.maxHP           = 210.0f; // High physical durability
            d.maxCE           = 150.0f; // Moderate CE — brawler focus
            d.baseSpeed       = 6.2f;   // High physical speed
            d.baseAttackDamage= 25.0f;

            // No domain in this build (balance decision)
            d.domainName = "None";
            d.hasDomain  = false;

            // Trait: Black Flash
            // Combat engine per melee hit:
            //   roll = (float)GetRandomValue(0,1000) / 1000.0f;
            //   if (traits.hasBlackFlash && roll < traits.blackFlashChance)
            //       damage *= traits.blackFlashMultiplier;
            d.traits.hasBlackFlash        = true;
            d.traits.blackFlashChance     = 0.20f; // 20% per hit
            d.traits.blackFlashMultiplier = 2.5f;  // 2.5× crit

            // Visuals
            d.bodyColor        = (Color){255, 140, 20,  255}; // Fierce orange
            d.ceColor          = (Color){255, 200, 60,  255}; // Amber CE
            d.domainColor      = (Color){30,  15,  0,   255}; // N/A — dark amber
            d.domainAccentColor= (Color){255, 80,  0,   255}; // Black Flash burst
            break;
        }

        default:
            // Fallback — should never hit this in production
            d.id    = CHAR_YUJI;
            d.name  = "UNKNOWN";
            d.maxHP = 100.0f;
            break;
    }

    return d;
}

// ---------------------------------------------------------------------------
// GetCharacterName() — lightweight convenience wrapper
// ---------------------------------------------------------------------------
const char* GetCharacterName(CharacterID id) {
    return GetCharacterData(id).name;
}
