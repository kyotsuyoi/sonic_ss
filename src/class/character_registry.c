#include "character_registry.h"
#include "sonic.h"
#include "amy.h"
#include "tails.h"
#include "knuckles.h"
#include "shadow.h"

static ui_character_choice_t character_registry_choice_from_character_id(int character_id)
{
    switch (character_id)
    {
    case CHARACTER_ID_AMY:
        return UiCharacterAmy;
    case CHARACTER_ID_TAILS:
        return UiCharacterTails;
    case CHARACTER_ID_KNUCKLES:
        return UiCharacterKnuckles;
    case CHARACTER_ID_SHADOW:
        return UiCharacterShadow;
    case CHARACTER_ID_SONIC:
    default:
        return UiCharacterSonic;
    }
}

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

character_animation_profile_t character_registry_get_animation_profile(ui_character_choice_t choice)
{
    character_animation_profile_t profile;

    profile.move_count = 4;
    profile.stand_count = 3;
    profile.punch_count = 4;
    profile.kick_count = 4;

    switch (choice)
    {
    case UiCharacterAmy:
        profile.punch_count = 4;
        profile.kick_count = 4;
        break;
    case UiCharacterTails:
        profile.move_count = 4;
        profile.punch_count = 4;
        profile.kick_count = 4;
        break;
    case UiCharacterKnuckles:
        profile.move_count = 4;
        profile.stand_count = 4;
        profile.punch_count = 4;
        profile.kick_count = 4;
        break;
    case UiCharacterShadow:
    case UiCharacterSonic:
    default:
        break;
    }

    return profile;
}

character_combat_profile_t character_registry_get_combat_profile(ui_character_choice_t choice)
{
    character_combat_profile_t profile;

    profile.hit_range_punch1 = 10;
    profile.hit_range_punch2 = 11;
    profile.hit_range_kick1 = 11;
    profile.hit_range_kick2 = 12;
    profile.attack_forward_impulse_light = 0.60f;
    profile.attack_forward_impulse_heavy = 1.10f;
    profile.knockback_punch1 = 1.8f;
    profile.knockback_punch2 = 2.3f;
    profile.knockback_kick1 = 1.8f;
    profile.knockback_kick2 = 2.6f;
    profile.charged_kick_enabled = false;
    profile.damage_punch1 = 1;
    profile.damage_punch2 = 3;
    profile.damage_kick1 = 2;
    profile.damage_kick2 = 5;
    profile.charged_kick_damage = 10;
    profile.charged_kick_range_bonus = 4;
    profile.charged_kick_stun_bonus = 14;
    profile.charged_kick_knockback_mult = 1.70f;

    if (choice == UiCharacterTails)
    {
        profile.hit_range_kick1 = 16;
        profile.hit_range_kick2 = 20;
        profile.attack_forward_impulse_light = 0.65f;
        profile.attack_forward_impulse_heavy = 1.00f;
        profile.knockback_kick1 = 1.8f;
        profile.knockback_kick2 = 2.4f;
    }

    if (choice == UiCharacterKnuckles)
    {
        profile.hit_range_kick1 = 13;
        profile.hit_range_kick2 = 18;
        profile.attack_forward_impulse_light = 0.85f;
        profile.attack_forward_impulse_heavy = 1.75f;
        profile.knockback_kick1 = 2.4f;
        profile.knockback_kick2 = 3.6f;
        profile.charged_kick_enabled = true;
        /* Ensure Knuckles charged kick is noticeably stronger (matching PvP behavior). */
        profile.charged_kick_damage = 15;
    }

    return profile;
}

character_combat_profile_t character_registry_get_combat_profile_by_character_id(int character_id)
{
    return character_registry_get_combat_profile(character_registry_choice_from_character_id(character_id));
}

void character_registry_apply_combat_profile(character_t *character, ui_character_choice_t choice)
{
    character_combat_profile_t profile;

    if (character == JO_NULL)
        return;

    profile = character_registry_get_combat_profile(choice);

    character->hit_range_punch1 = profile.hit_range_punch1;
    character->hit_range_punch2 = profile.hit_range_punch2;
    character->hit_range_kick1 = profile.hit_range_kick1;
    character->hit_range_kick2 = profile.hit_range_kick2;
    character->attack_forward_impulse_light = profile.attack_forward_impulse_light;
    character->attack_forward_impulse_heavy = profile.attack_forward_impulse_heavy;
    character->knockback_punch1 = profile.knockback_punch1;
    character->knockback_punch2 = profile.knockback_punch2;
    character->knockback_kick1 = profile.knockback_kick1;
    character->knockback_kick2 = profile.knockback_kick2;
    character->charged_kick_enabled = profile.charged_kick_enabled;

    switch (choice)
    {
    case UiCharacterAmy:
        character->character_id = CHARACTER_ID_AMY;
        break;
    case UiCharacterTails:
        character->character_id = CHARACTER_ID_TAILS;
        break;
    case UiCharacterKnuckles:
        character->character_id = CHARACTER_ID_KNUCKLES;
        break;
    case UiCharacterShadow:
        character->character_id = CHARACTER_ID_SHADOW;
        break;
    case UiCharacterSonic:
    default:
        character->character_id = CHARACTER_ID_SONIC;
        break;
    }

    /* Default group assignment (no group) */
    character->group = 0;
}

