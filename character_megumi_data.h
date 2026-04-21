#ifndef CHARACTER_MEGUMI_DATA_H
#define CHARACTER_MEGUMI_DATA_H

#include "characters.h"

static inline CharacterData BuildMegumiData(void) {
    CharacterData d = {0};
    d.id               = CHAR_MEGUMI;
    d.name             = "MEGUMI";
    d.fullTitle        = "Ten Shadows";
    d.maxHP            = 390.0f;
    d.maxCE            = 310.0f;
    d.baseSpeed        = 5.9f;
    d.baseAttackDamage = 22.0f;
    d.projectileDamage = 22.0f;
    d.domainName       = "Chimera Shadow Garden";
    d.hasDomain        = true;
    d.ultimateName     = "Mahoraga";
    d.bodyColor        = (Color){55, 80, 150, 255};
    d.ceColor          = (Color){120, 140, 255, 255};
    d.domainColor      = (Color){8, 10, 24, 255};
    d.domainAccentColor= (Color){70, 110, 255, 255};
    return d;
}

#endif
