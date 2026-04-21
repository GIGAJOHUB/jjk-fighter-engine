#ifndef CHARACTER_NANAMI_DATA_H
#define CHARACTER_NANAMI_DATA_H

#include "characters.h"

static inline CharacterData BuildNanamiData(void) {
    CharacterData d = {0};
    d.id               = CHAR_NANAMI;
    d.name             = "NANAMI";
    d.fullTitle        = "Grade 1 Sorcerer";
    d.maxHP            = 430.0f;
    d.maxCE            = 240.0f;
    d.baseSpeed        = 5.1f;
    d.baseAttackDamage = 26.0f;
    d.projectileDamage = 0.0f;
    d.domainName       = "None";
    d.hasDomain        = false;
    d.ultimateName     = "Overtime Slash";
    d.bodyColor        = (Color){224, 198, 120, 255};
    d.ceColor          = (Color){255, 220, 140, 255};
    d.domainColor      = (Color){24, 18, 10, 255};
    d.domainAccentColor= (Color){255, 215, 110, 255};
    return d;
}

#endif
