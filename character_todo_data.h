#ifndef CHARACTER_TODO_DATA_H
#define CHARACTER_TODO_DATA_H

#include "characters.h"

static inline CharacterData BuildTodoData(void) {
    CharacterData d = {0};
    d.id               = CHAR_TODO;
    d.name             = "TODO";
    d.fullTitle        = "Boogie Woogie";
    d.maxHP            = 470.0f;
    d.maxCE            = 220.0f;
    d.baseSpeed        = 5.7f;
    d.baseAttackDamage = 30.0f;
    d.projectileDamage = 0.0f;
    d.domainName       = "None";
    d.hasDomain        = false;
    d.ultimateName     = "Ultimate Tackle";
    d.bodyColor        = (Color){120, 80, 60, 255};
    d.ceColor          = (Color){255, 170, 90, 255};
    d.domainColor      = (Color){26, 16, 10, 255};
    d.domainAccentColor= (Color){255, 150, 80, 255};
    return d;
}

#endif
