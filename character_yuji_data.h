#ifndef CHARACTER_YUJI_DATA_H
#define CHARACTER_YUJI_DATA_H

#include "characters.h"

static inline CharacterData BuildYujiData(void) {
    CharacterData d = {0};
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
    return d;
}

#endif
