#include "character_registry.h"
#include "game_constants.h"

/*
 * These tile definitions are used by the bot system when it needs to load
 * a character's sprite assets independently of the player instance.
 *
 * The character module owns these definitions; bot.c only references them.
 */

static const jo_tile WalkTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 5, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 7, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile TailsMoveTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesTiles4[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile StandTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile StandTiles4[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile PunchTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 5, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 7, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 8, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 9, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile TailsPunchTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KickTiles13[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 5, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 7, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 8, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 9, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 10, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 11, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 12, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KickTiles8[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 5, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 7, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KickTiles1[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile PunchTiles4[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KickTiles4[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile TailsTailTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile BotDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT}
};

static const jo_tile* character_get_move_tiles_internal(ui_character_choice_t choice, int *out_count)
{
    switch (choice)
    {
        case UiCharacterAmy:
        case UiCharacterSonic:
            if (out_count) *out_count = JO_TILE_COUNT(WalkTiles);
            return WalkTiles;
        case UiCharacterTails:
            if (out_count) *out_count = JO_TILE_COUNT(TailsMoveTiles);
            return TailsMoveTiles;
        case UiCharacterKnuckles:
            if (out_count) *out_count = JO_TILE_COUNT(KnucklesTiles4);
            return KnucklesTiles4;
        case UiCharacterShadow:
            if (out_count) *out_count = JO_TILE_COUNT(WalkTiles);
            return WalkTiles;
        default:
            if (out_count) *out_count = JO_TILE_COUNT(WalkTiles);
            return WalkTiles;
    }
}

static const jo_tile* character_get_stand_tiles_internal(ui_character_choice_t choice, int *out_count)
{
    switch (choice)
    {
        case UiCharacterKnuckles:
            if (out_count) *out_count = JO_TILE_COUNT(StandTiles4);
            return StandTiles4;
        default:
            if (out_count) *out_count = JO_TILE_COUNT(StandTiles);
            return StandTiles;
    }
}

static const jo_tile* character_get_punch_tiles_internal(ui_character_choice_t choice, int *out_count)
{
    switch (choice)
    {
        case UiCharacterKnuckles:
            if (out_count) *out_count = JO_TILE_COUNT(PunchTiles4);
            return PunchTiles4;
        case UiCharacterTails:
            if (out_count) *out_count = JO_TILE_COUNT(TailsPunchTiles);
            return TailsPunchTiles;
        default:
            if (out_count) *out_count = JO_TILE_COUNT(PunchTiles);
            return PunchTiles;
    }
}

static const jo_tile* character_get_kick_tiles_internal(ui_character_choice_t choice, int *out_count)
{
    switch (choice)
    {
        case UiCharacterKnuckles:
            if (out_count) *out_count = JO_TILE_COUNT(KickTiles4);
            return KickTiles4;
        case UiCharacterTails:
            if (out_count) *out_count = JO_TILE_COUNT(KickTiles1);
            return KickTiles1;
        case UiCharacterAmy:
        case UiCharacterSonic:
        case UiCharacterShadow:
        default:
            if (out_count) *out_count = JO_TILE_COUNT(KickTiles13);
            return KickTiles13;
    }
}

static const jo_tile* character_get_tail_tiles_internal(ui_character_choice_t choice, int *out_count)
{
    if (choice == UiCharacterTails)
    {
        if (out_count) *out_count = JO_TILE_COUNT(TailsTailTiles);
        return TailsTailTiles;
    }

    if (out_count) *out_count = 0;
    return JO_NULL;
}

const jo_tile* character_get_move_tiles(ui_character_choice_t choice, int *out_count)
{
    return character_get_move_tiles_internal(choice, out_count);
}

const jo_tile* character_get_stand_tiles(ui_character_choice_t choice, int *out_count)
{
    return character_get_stand_tiles_internal(choice, out_count);
}

const jo_tile* character_get_punch_tiles(ui_character_choice_t choice, int *out_count)
{
    return character_get_punch_tiles_internal(choice, out_count);
}

const jo_tile* character_get_kick_tiles(ui_character_choice_t choice, int *out_count)
{
    return character_get_kick_tiles_internal(choice, out_count);
}

const jo_tile* character_get_tail_tiles(ui_character_choice_t choice, int *out_count)
{
    return character_get_tail_tiles_internal(choice, out_count);
}

const char *character_get_walk_tga(ui_character_choice_t choice)
{
    switch (choice)
    {
        case UiCharacterAmy: return "AMY_WLK.TGA";
        case UiCharacterTails: return "TLS_WLK.TGA";
        case UiCharacterKnuckles: return "KNK_WLK.TGA";
        case UiCharacterShadow: return "SDW_WLK.TGA";
        case UiCharacterSonic: 
        default: return "SNC_WLK.TGA";
    }
}

const char *character_get_run1_tga(ui_character_choice_t choice)
{
    switch (choice)
    {
        case UiCharacterAmy: return "AMY_RUN1.TGA";
        case UiCharacterTails: return "TLS_RUN1.TGA";
        case UiCharacterKnuckles: return "KNK_RUN1.TGA";
        case UiCharacterShadow: return "SDW_RUN1.TGA";
        case UiCharacterSonic:
        default: return "SNC_RUN1.TGA";
    }
}

const char *character_get_run2_tga(ui_character_choice_t choice)
{
    switch (choice)
    {
        case UiCharacterAmy: return "AMY_RUN2.TGA";
        case UiCharacterTails: return "TLS_RUN1.TGA"; /* tails reuses run1 */
        case UiCharacterKnuckles: return "KNK_RUN1.TGA"; /* knuckles reuses run1 */
        case UiCharacterShadow: return "SDW_RUN2.TGA";
        case UiCharacterSonic:
        default: return "SNC_RUN2.TGA";
    }
}

const char *character_get_stand_tga(ui_character_choice_t choice)
{
    switch (choice)
    {
        case UiCharacterAmy: return "AMY_STD.TGA";
        case UiCharacterTails: return "TLS_STD.TGA";
        case UiCharacterKnuckles: return "KNK_STD.TGA";
        case UiCharacterShadow: return "SDW_STD.TGA";
        case UiCharacterSonic:
        default: return "SNC_STD.TGA";
    }
}

const char *character_get_jump_tga(ui_character_choice_t choice)
{
    switch (choice)
    {
        case UiCharacterAmy: return "AMY_JMP.TGA";
        case UiCharacterTails: return "TLS_JMP.TGA";
        case UiCharacterKnuckles: return "KNK_JMP.TGA";
        case UiCharacterShadow: return "SDW_JMP.TGA";
        case UiCharacterSonic:
        default: return "SNC_JMP.TGA";
    }
}

const char *character_get_spin_tga(ui_character_choice_t choice)
{
    switch (choice)
    {
        case UiCharacterAmy: return "AMY_SPN.TGA";
        case UiCharacterTails: return "TLS_SPN.TGA";
        case UiCharacterKnuckles: return "KNK_SPN.TGA";
        case UiCharacterShadow: return "SDW_SPN.TGA";
        case UiCharacterSonic:
        default: return "SNC_SPN.TGA";
    }
}

const char *character_get_damage_tga(ui_character_choice_t choice)
{
    switch (choice)
    {
        case UiCharacterAmy: return "AMY_DMG.TGA";
        case UiCharacterTails: return "TLS_DMG.TGA";
        case UiCharacterKnuckles: return "KNK_DMG.TGA";
        case UiCharacterShadow: return "SDW_DMG.TGA";
        case UiCharacterSonic:
        default: return "SNC_DMG.TGA";
    }
}

const char *character_get_defeated_tga(ui_character_choice_t choice)
{
    switch (choice)
    {
        case UiCharacterAmy: return "AMY_DFT.TGA";
        case UiCharacterTails: return "TLS_DFT.TGA";
        case UiCharacterKnuckles: return "KNK_DFT.TGA";
        case UiCharacterShadow: return "SDW_DFT.TGA";
        case UiCharacterSonic:
        default: return "SNC_DFT.TGA";
    }
}

const jo_tile *character_get_defeated_tiles(int *out_count)
{
    if (out_count)
        *out_count = JO_TILE_COUNT(BotDefeatedTile);
    return BotDefeatedTile;
}

const char *character_get_punch_tga(ui_character_choice_t choice)
{
    switch (choice)
    {
        case UiCharacterAmy: return "AMY_PNC.TGA";
        case UiCharacterTails: return "TLS_PNC.TGA";
        case UiCharacterKnuckles: return "KNK_PNC.TGA";
        case UiCharacterShadow: return "SDW_PNC.TGA";
        case UiCharacterSonic:
        default: return "SNC_PNC.TGA";
    }
}

const char *character_get_kick_tga(ui_character_choice_t choice)
{
    switch (choice)
    {
        case UiCharacterAmy: return "AMY_KCK.TGA";
        case UiCharacterTails: return "TLS_KCK.TGA";
        case UiCharacterKnuckles: return "KNK_KCK.TGA";
        case UiCharacterShadow: return "SDW_KCK.TGA";
        case UiCharacterSonic:
        default: return "SNC_KCK.TGA";
    }
}

const char *character_get_tail_tga(ui_character_choice_t choice)
{
    if (choice == UiCharacterTails)
        return "TLS_TLS.TGA";

    return "";
}
