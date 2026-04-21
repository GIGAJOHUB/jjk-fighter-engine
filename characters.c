#include "characters.h"
#include <string.h>
#include "character_sukuna_data.h"
#include "character_gojo_data.h"
#include "character_toji_data.h"
#include "character_yuta_data.h"
#include "character_yuji_data.h"
#include "character_megumi_data.h"
#include "character_nanami_data.h"
#include "character_nobara_data.h"
#include "character_todo_data.h"

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
    CharacterTraits defaults = DefaultTraits();
    memset(&d, 0, sizeof(CharacterData));
    d.traits = defaults;

    switch (id) {
        case CHAR_SUKUNA: d = BuildSukunaData(); break;
        case CHAR_GOJO:   d = BuildGojoData(); break;
        case CHAR_TOJI:   d = BuildTojiData(); break;
        case CHAR_YUTA:   d = BuildYutaData(); break;
        case CHAR_YUJI:   d = BuildYujiData(); break;
        case CHAR_MEGUMI: d = BuildMegumiData(); break;
        case CHAR_NANAMI: d = BuildNanamiData(); break;
        case CHAR_NOBARA: d = BuildNobaraData(); break;
        case CHAR_TODO:   d = BuildTodoData(); break;

        default:
            d.id               = CHAR_TODO;
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

    if (d.traits.ceCostMultiplier == 0.0f) d.traits.ceCostMultiplier = defaults.ceCostMultiplier;
    if (d.traits.rctCostMultiplier == 0.0f) d.traits.rctCostMultiplier = defaults.rctCostMultiplier;
    if (d.traits.rctHealMultiplier == 0.0f) d.traits.rctHealMultiplier = defaults.rctHealMultiplier;
    if (d.traits.blackFlashMultiplier == 0.0f) d.traits.blackFlashMultiplier = defaults.blackFlashMultiplier;

    return d;
}

const char* GetCharacterName(CharacterID id) {
    return GetCharacterData(id).name;
}
