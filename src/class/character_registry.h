#ifndef CHARACTER_REGISTRY_H
#define CHARACTER_REGISTRY_H

#include "ui_control.h"

typedef struct
{
    void (*load)(void);
    void (*unload)(void);
    void (*update_animation)(void);
    void (*display)(void);
} character_handlers_t;

character_handlers_t character_registry_get(ui_character_choice_t choice);

#endif
