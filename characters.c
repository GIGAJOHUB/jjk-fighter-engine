#include "characters.h"
#include <string.h>

static CharacterTraits DefaultTraits(void) {
    CharacterTraits t = {0};
    t.isHeavenlyRestricted = false;
    t.hasSixEyes           = false;
    t.ceCostMultiplier     = 1.0f;
    t.rctCostMultiplier    = 1.0f;
    t.hasRCTEfficiency     = false;
    t.rctHealMultiplier    = 1.0f;
    t.hasCopy              = false;
    t.hasBlackFlash        = false;
    t.blackFlashChance     = 0.0f;
    t.blackFlashMultiplier = 1.0f;
    return t;
}

CharacterData GetCharacterData(CharacterID id) {
    CharacterData d;
    memset(&d, 0, sizeof(CharacterData));
    d.traits = DefaultTraits();

    switch (id) {
        case CHAR_SUKUNA:
            d.id               = CHAR_SUKUNA;
            d.name             = "SUKUNA";
            d.fullTitle        = "King of Curses";
            d.maxHP            = 500.0f;
            d.maxCE            = 720.0f;
            d.baseSpeed        = 5.2f;
            d.baseAttackDamage = 28.0f;
            d.projectileDamage = 34.0f;
            d.domainName       = "Malevolent Shrine";
            d.hasDomain        = true;
            d.ultimateName     = "Dismantle + Cleave";
            d.traits.hasRCTEfficiency  = true;
            d.traits.rctHealMultiplier = 2.5f;
            d.bodyColor        = (Color){200, 40, 40, 255};
            d.ceColor          = (Color){255, 100, 30, 255};
            d.domainColor      = (Color){30, 0, 0, 255};
            d.domainAccentColor= (Color){180, 20, 20, 255};
            break;

        case CHAR_GOJO:
            d.id               = CHAR_GOJO;
            d.name             = "GOJO";
            d.fullTitle        = "The Honored One | Six Eyes User";
            d.maxHP            = 400.0f;
            d.maxCE            = 320.0f;
            d.baseSpeed        = 6.5f;
            d.baseAttackDamage = 22.0f;
            d.projectileDamage = 24.0f;
            d.domainName       = "Unlimited Void";
            d.hasDomain        = true;
            d.ultimateName     = "Hollow Purple";
            d.traits.hasSixEyes        = true;
            d.traits.ceCostMultiplier  = 0.85f;
            d.traits.rctCostMultiplier = 0.35f;
            d.bodyColor        = (Color){80, 180, 255, 255};
            d.ceColor          = (Color){180, 240, 255, 255};
            d.domainColor      = (Color){0, 0, 15, 255};
            d.domainAccentColor= (Color){200, 220, 255, 255};
            break;

        case CHAR_TOJI:
            d.id               = CHAR_TOJI;
            d.name             = "TOJI";
            d.fullTitle        = "Sorcerer Killer";
            d.maxHP            = 440.0f;
            d.maxCE            = 0.0f;
            d.baseSpeed        = 9.5f;
            d.baseAttackDamage = 38.0f;
            d.projectileDamage = 0.0f;
            d.domainName       = "None";
            d.hasDomain        = false;
            d.ultimateName     = "Heavenly Assault";
            d.traits.isHeavenlyRestricted = true;
            d.bodyColor        = (Color){200, 200, 200, 255};
            d.ceColor          = (Color){80, 80, 80, 255};
            d.domainColor      = (Color){20, 20, 20, 255};
            d.domainAccentColor= (Color){160, 160, 160, 255};
            break;

        case CHAR_YUTA:
            d.id               = CHAR_YUTA;
            d.name             = "YUTA";
            d.fullTitle        = "The Prodigy";
            d.maxHP            = 380.0f;
            d.maxCE            = 360.0f;
            d.baseSpeed        = 5.8f;
            d.baseAttackDamage = 24.0f;
            d.projectileDamage = 30.0f;
            d.domainName       = "Authentic Mutual Love";
            d.hasDomain        = true;
            d.ultimateName     = "Copy Technique";
            d.traits.hasCopy   = true;
            d.bodyColor        = (Color){180, 100, 255, 255};
            d.ceColor          = (Color){220, 160, 255, 255};
            d.domainColor      = (Color){20, 0, 35, 255};
            d.domainAccentColor= (Color){255, 180, 255, 255};
            break;

        case CHAR_YUJI:
            d.id               = CHAR_YUJI;
            d.name             = "YUJI";
            d.fullTitle        = "Tiger of West High";
            d.maxHP            = 420.0f;
            d.maxCE            = 180.0f;
            d.baseSpeed        = 6.2f;
            d.baseAttackDamage = 25.0f;
            d.projectileDamage = 24.0f;
            d.domainName       = "None";
            d.hasDomain        = false;
            d.ultimateName     = "Black Flash";
            d.traits.hasBlackFlash        = true;
            d.traits.blackFlashChance     = 0.0f;
            d.traits.blackFlashMultiplier = 2.5f;
            d.bodyColor        = (Color){255, 140, 20, 255};
            d.ceColor          = (Color){255, 200, 60, 255};
            d.domainColor      = (Color){30, 15, 0, 255};
            d.domainAccentColor= (Color){255, 80, 0, 255};
            break;

        default:
            d.id               = CHAR_YUJI;
            d.name             = "UNKNOWN";
            d.fullTitle        = "Fallback";
            d.maxHP            = 100.0f;
            d.maxCE            = 100.0f;
            d.baseSpeed        = 5.0f;
            d.baseAttackDamage = 20.0f;
            d.projectileDamage = 20.0f;
            d.domainName       = "None";
            d.hasDomain        = false;
            d.ultimateName     = "Unknown";
            d.bodyColor        = WHITE;
            d.ceColor          = SKYBLUE;
            d.domainColor      = BLACK;
            d.domainAccentColor= WHITE;
            break;
    }

    return d;
}

const char* GetCharacterName(CharacterID id) {
    return GetCharacterData(id).name;
}
