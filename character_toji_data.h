#ifndef CHARACTER_TOJI_DATA_H
#define CHARACTER_TOJI_DATA_H

#include "characters.h"

static inline CharacterData BuildTojiData(void) {
    CharacterData d = {0};
    d.id               = CHAR_TOJI;
    d.name             = "TOJI";
    d.fullTitle        = "Heavenly Restriction";
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
    return d;
}

#endif
