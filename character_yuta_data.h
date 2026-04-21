#ifndef CHARACTER_YUTA_DATA_H
#define CHARACTER_YUTA_DATA_H

#include "characters.h"

static inline CharacterData BuildYutaData(void) {
    CharacterData d = {0};
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
    d.ultimateName     = "Pure Love Beam";
    d.traits.hasCopy   = true;
    d.bodyColor        = (Color){180, 100, 255, 255};
    d.ceColor          = (Color){220, 160, 255, 255};
    d.domainColor      = (Color){20, 0, 35, 255};
    d.domainAccentColor= (Color){255, 180, 255, 255};
    return d;
}

#endif
