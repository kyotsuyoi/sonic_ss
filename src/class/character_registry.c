#include "character_registry.h"
#include "sonic.h"
#include "amy.h"
#include "tails.h"
#include "knuckles.h"
#include "shadow.h"

character_handlers_t character_registry_get(ui_character_choice_t choice)
{
    character_handlers_t handlers;

    switch (choice)
    {
    case UiCharacterAmy:
        handlers.load = load_amy;
        handlers.unload = unload_amy;
        handlers.update_animation = amy_running_animation_handling;
        handlers.display = display_amy;
        break;
    case UiCharacterTails:
        handlers.load = load_tails;
        handlers.unload = unload_tails;
        handlers.update_animation = tails_running_animation_handling;
        handlers.display = display_tails;
        break;
    case UiCharacterKnuckles:
        handlers.load = load_knuckles;
        handlers.unload = unload_knuckles;
        handlers.update_animation = knuckles_running_animation_handling;
        handlers.display = display_knuckles;
        break;
    case UiCharacterShadow:
        handlers.load = load_shadow;
        handlers.unload = unload_shadow;
        handlers.update_animation = shadow_running_animation_handling;
        handlers.display = display_shadow;
        break;
    case UiCharacterSonic:
    default:
        handlers.load = load_sonic;
        handlers.unload = unload_sonic;
        handlers.update_animation = sonic_running_animation_handling;
        handlers.display = display_sonic;
        break;
    }

    return handlers;
}
