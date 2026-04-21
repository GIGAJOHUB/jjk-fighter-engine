#ifndef CHARACTER_GOJO_DATA_H
#define CHARACTER_GOJO_DATA_H

#include "characters.h"

static inline CharacterData BuildGojoData(void) {
    CharacterData d = {0};
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
    return d;
}

#endif
