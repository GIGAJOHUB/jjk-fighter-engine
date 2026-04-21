#ifndef CHARACTER_NOBARA_DATA_H
#define CHARACTER_NOBARA_DATA_H

#include "characters.h"

static inline CharacterData BuildNobaraData(void) {
    CharacterData d = {0};
    d.id               = CHAR_NOBARA;
    d.name             = "NOBARA";
    d.fullTitle        = "Straw Doll Technique";
    d.maxHP            = 360.0f;
    d.maxCE            = 260.0f;
    d.baseSpeed        = 6.1f;
    d.baseAttackDamage = 20.0f;
    d.projectileDamage = 18.0f;
    d.domainName       = "None";
    d.hasDomain        = false;
    d.ultimateName     = "Maximum Resonance";
    d.bodyColor        = (Color){180, 95, 130, 255};
    d.ceColor          = (Color){255, 140, 180, 255};
    d.domainColor      = (Color){30, 10, 20, 255};
    d.domainAccentColor= (Color){255, 160, 210, 255};
    return d;
}

#endif
