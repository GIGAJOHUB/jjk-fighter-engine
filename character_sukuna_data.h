#ifndef CHARACTER_SUKUNA_DATA_H
#define CHARACTER_SUKUNA_DATA_H

#include "characters.h"

static inline CharacterData BuildSukunaData(void) {
    CharacterData d = {0};
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
    d.ultimateName     = "Fuga";
    d.traits.hasRCTEfficiency  = true;
    d.traits.rctHealMultiplier = 2.5f;
    d.bodyColor        = (Color){200, 40, 40, 255};
    d.ceColor          = (Color){255, 100, 30, 255};
    d.domainColor      = (Color){30, 0, 0, 255};
    d.domainAccentColor= (Color){180, 20, 20, 255};
    return d;
}

#endif
