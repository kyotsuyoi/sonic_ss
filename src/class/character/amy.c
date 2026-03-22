#include <jo/jo.h>
#include <string.h>
#include "amy.h"
#include "player.h"
#include "character_common.h"
#include "character_registry.h"
#include "sprite_safe.h"
#include "runtime_log.h"
#include "ram_cart.h"

extern jo_sidescroller_physics_params physics; // global physics state (from main.c)

static character_t *amy_ref = &player;
static jo_sidescroller_physics_params *amy_physics = &physics;

#define character_ref (*amy_ref)
#define physics (*amy_physics)

void amy_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    if (chr != NULL)
        amy_ref = chr;
    if (phy != NULL)
        amy_physics = phy;
}

#define SPRITE_DIR "SPT"

static bool amy_loaded = false;
static bool amy_sheet_ready = false;
static int amy_sheet_width = 0;
static int amy_sheet_height = 0;
static int amy_sprite_id = -1;

static bool amy_defeated_sheet_ready = false;
static jo_img amy_defeated_sheet = {0};

// NOTE: per-character animation state is stored directly on the character so
// multiple instances (Player 1 / Player 2 / bots) do not interfere with each other.
// This avoids the case where P2 overwrites P1's current animation/frame.

// Dummy anims used for internal timing / combo logic (punch/kick/speed).
static int amy_stand_anim_id = -1;
static int amy_walking_anim_id = -1;
static int amy_running1_anim_id = -1;
static int amy_running2_anim_id = -1;
static int amy_punch_anim_id = -1;
static int amy_kick_anim_id = -1;

static int amy_defeated_sprite_id;

// AMY_FUL.TGA layout (8x6):
// Row 0: 0-3 = stand, 4 = jump, 5 = damage, 6 = spin, 7 = unused
// Row 1: walk (8 frames)
// Row 2: run1 (8 frames)
// Row 3: run2 (8 frames)
// Row 4: punch (4 punch1 + 4 punch2)
// Row 5: kick  (4 kick1 + 4 kick2)
static const jo_tile AmyStandTiles[] =
{
    {CHARACTER_WIDTH * 0, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyJumpTile[] =
{
    {CHARACTER_WIDTH * 4, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyDamageTile[] =
{
    {CHARACTER_WIDTH * 5, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmySpinTile[] =
{
    {CHARACTER_WIDTH * 6, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyWalkingTiles[] =
{
    {CHARACTER_WIDTH * 0, CHARACTER_HEIGHT * 1, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, CHARACTER_HEIGHT * 1, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, CHARACTER_HEIGHT * 1, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, CHARACTER_HEIGHT * 1, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, CHARACTER_HEIGHT * 1, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 5, CHARACTER_HEIGHT * 1, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, CHARACTER_HEIGHT * 1, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 7, CHARACTER_HEIGHT * 1, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyRunning1Tiles[] =
{
    {CHARACTER_WIDTH * 0, CHARACTER_HEIGHT * 2, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, CHARACTER_HEIGHT * 2, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, CHARACTER_HEIGHT * 2, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, CHARACTER_HEIGHT * 2, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, CHARACTER_HEIGHT * 2, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 5, CHARACTER_HEIGHT * 2, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, CHARACTER_HEIGHT * 2, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 7, CHARACTER_HEIGHT * 2, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyRunning2Tiles[] =
{
    {CHARACTER_WIDTH * 0, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 5, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 7, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyPunchTiles[] =
{
    {CHARACTER_WIDTH * 0, CHARACTER_HEIGHT * 4, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, CHARACTER_HEIGHT * 4, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, CHARACTER_HEIGHT * 4, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, CHARACTER_HEIGHT * 4, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, CHARACTER_HEIGHT * 4, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 5, CHARACTER_HEIGHT * 4, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, CHARACTER_HEIGHT * 4, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 7, CHARACTER_HEIGHT * 4, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyKickTiles[] =
{
    {CHARACTER_WIDTH * 0, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 5, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 7, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void amy_copy_defeated_sheet_frame_to_sprite(int sprite_id)
{
    character_copy_defeated_sheet_frame_to_sprite(sprite_id, &amy_defeated_sheet, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT);
}

static int amy_ensure_defeated_wram_sprite(character_t *chr)
{
    return character_ensure_defeated_wram_sprite(chr, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT);
}

static void amy_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y)
{
    character_copy_cart_sheet_frame_to_sprite(sprite_id, "AMY_FUL", amy_sheet_width, amy_sheet_height, frame_x, frame_y, CHARACTER_WIDTH, CHARACTER_HEIGHT);
}

static int amy_create_blank_animation(int frame_count)
{
    return character_create_blank_animation(frame_count);
}

static int amy_create_blank_sprite(void)
{
    return character_create_blank_sprite();
}

// The full WRAM-per-character sprite approach avoids P1/P2 sharing the same
// backing sprite, which would cause one entity to overwrite the other's frame
// each update.
static int amy_ensure_wram_sprite(character_t *chr)
{
    return character_ensure_wram_sprite(chr, &amy_sprite_id);
}

/*
Speed -> Animation mapping:
- `speed_step = (int)JO_ABS(physics.speed)` converts the physics speed to a discrete level.
- The code maps speed ranges to an animation variant and a sprite frame rate:
    * `speed_step >= 6`: use `running2` with `frame_rate = 3` (fast)
    * `speed_step >= 5`: use `running1` with `frame_rate = 4`
    * `speed_step >= 4`: use `running1` with `frame_rate = 5`
    * `speed_step >= 3`: use `running1` with `frame_rate = DEFAULT_SPRITE_FRAME_DURATION` (medium)
    * `speed_step >= 2`: use `running1` with `frame_rate = 7`
    * `speed_step >= 1`: use `walking` with `frame_rate = 8`
    * `else`: use `walking` with `frame_rate = 9`
- Lower `frame_rate` values advance animation frames faster (so `3` animates quicker than `9`).
- `run` selects which animation variant is drawn (0 = walking, 1 = running1, 2 = running2).
This mapping keeps the visual animation speed consistent with the character's physical speed.
*/
/*
Per-character WRAM animation timing is now driven by the dummy sprite animations
(the ones created via `sprite_safe_create_anim`) rather than manually ticking a
frame counter.

This matches Amy's behavior and ensures stand/walk/run timing is consistent.
*/

void amy_running_animation_handling(void)
{
    character_running_animation_handling(&character_ref, &physics);
}

static void amy_draw_for_character(character_t *chr)
{
    if (!amy_sheet_ready)
    {
        // Legacy per-frame animation path.
        if (chr->spin)
        {
            jo_sprite_draw3D_and_rotate2(chr->spin_sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z, chr->angle);
            if (chr->flip)
                chr->angle -= CHARACTER_SPIN_SPEED;
            else
                chr->angle += CHARACTER_SPIN_SPEED;
            return;
        }

        if (chr->life <= 0 && chr->defeated_sprite_id >= 0)
        {
            jo_sprite_draw3D2(chr->defeated_sprite_id,
                              chr->x,
                              chr->y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT),
                              CHARACTER_SPRITE_Z);
            return;
        }

        if (chr->stun_timer > 0 && chr->damage_sprite_id >= 0)
        {
            jo_sprite_draw3D2(chr->damage_sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
            return;
        }

        if (chr->punch || chr->punch2)
        {
            int anim_sprite_id = (chr->punch_anim_id >= 0) ? jo_get_anim_sprite(chr->punch_anim_id) : -1;
            if (anim_sprite_id >= 0)
                jo_sprite_draw3D2(anim_sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
            return;
        }

        if (chr->kick || chr->kick2)
        {
            int anim_sprite_id = (chr->kick_anim_id >= 0) ? jo_get_anim_sprite(chr->kick_anim_id) : -1;
            if (anim_sprite_id >= 0)
                jo_sprite_draw3D2(anim_sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
            return;
        }

        if (chr->jump)
        {
            jo_sprite_draw3D2(chr->jump_sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
            return;
        }

        int anim_sprite_id;
        if (chr->walk && chr->run == 0)
            anim_sprite_id = (chr->walking_anim_id >= 0) ? jo_get_anim_sprite(chr->walking_anim_id) : -1;
        else if (chr->walk && chr->run == 1)
            anim_sprite_id = (chr->running1_anim_id >= 0) ? jo_get_anim_sprite(chr->running1_anim_id) : -1;
        else if (chr->walk && chr->run == 2)
            anim_sprite_id = (chr->running2_anim_id >= 0) ? jo_get_anim_sprite(chr->running2_anim_id) : -1;
        else
            anim_sprite_id = (chr->stand_sprite_id >= 0) ? jo_get_anim_sprite(chr->stand_sprite_id) : -1;

        if (anim_sprite_id >= 0)
            jo_sprite_draw3D2(anim_sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
        return;
    }

    if (character_draw_defeated(chr, amy_ensure_defeated_wram_sprite(chr), &amy_defeated_sheet, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT))
        return;

    int sprite_id = amy_ensure_wram_sprite(chr);
    if (sprite_id < 0)
        return;

    if (character_draw_cart_frame(chr, sprite_id, "AMY_FUL", amy_sheet_width, amy_sheet_height))
        return;

    if (chr->spin)
    {
        jo_sprite_draw3D_and_rotate2(sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z, chr->angle);
        if (chr->flip)
            chr->angle -= CHARACTER_SPIN_SPEED;
        else
            chr->angle += CHARACTER_SPIN_SPEED;
    }
    else
    {
        jo_sprite_draw3D2(sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
    }
}

void amy_draw(character_t *chr)
{
    amy_draw_for_character(chr);
}

void display_amy(void)
{
    if (!physics.is_in_air)
    {
        character_ref.spin = false;
        character_ref.jump = false;
        character_ref.angle = 0;
    }

    if (character_ref.flip)
        jo_sprite_enable_horizontal_flip();

    // Use the generic draw path so it also works for P2 / bots if needed.
    amy_draw_for_character(&character_ref);

    if (character_ref.flip)
        jo_sprite_disable_horizontal_flip();

    int life_percent = (character_ref.life * 100) / 50;
    int bar_max_width = 20;
    int bar_width = (life_percent * bar_max_width) / 100;
    char bar[bar_max_width + 1];
    for (int i = 0; i < bar_max_width; ++i)
        bar[i] = (i < bar_width) ? '#' : '-';
    bar[bar_max_width] = '\0';
    // jo_printf(1, 26, "P1 : [%s] %d%%", bar, life_percent); // debug life bar (temporário)
}

void load_amy(void)
{
    if (!amy_loaded)
    {
        if (!amy_sheet_ready)
        {
            jo_img sheet = {0};
            if (character_load_sheet(&sheet, "AMY_FUL.TGA", SPRITE_DIR, JO_COLOR_Green))
            {
                if (ram_cart_store_sprite("AMY_FUL", sheet.data, (size_t)sheet.width * (size_t)sheet.height * sizeof(unsigned short)))
                {
                    amy_sheet_width = sheet.width;
                    amy_sheet_height = sheet.height;
                    amy_sheet_ready = true;
                }
                character_unload_sheet(&sheet);
            }
        }

        if (amy_sprite_id < 0)
            amy_sprite_id = character_create_blank_sprite();

        /* Use the global sprite as the base WRAM sprite for this character; P2/bots
         * will allocate their own WRAM sprite as needed. */
        character_ref.wram_sprite_id = amy_sprite_id;

        // Dummy animation objects (used only for timing / combo logic).
        if (sprite_safe_can_create_anim())
        {
            amy_stand_anim_id = sprite_safe_create_anim(amy_create_blank_animation(JO_TILE_COUNT(AmyStandTiles)), JO_TILE_COUNT(AmyStandTiles), DEFAULT_SPRITE_FRAME_DURATION);
            amy_walking_anim_id = sprite_safe_create_anim(amy_create_blank_animation(JO_TILE_COUNT(AmyWalkingTiles)), JO_TILE_COUNT(AmyWalkingTiles), DEFAULT_SPRITE_FRAME_DURATION);
            amy_running1_anim_id = sprite_safe_create_anim(amy_create_blank_animation(JO_TILE_COUNT(AmyRunning1Tiles)), JO_TILE_COUNT(AmyRunning1Tiles), DEFAULT_SPRITE_FRAME_DURATION);
            amy_running2_anim_id = sprite_safe_create_anim(amy_create_blank_animation(JO_TILE_COUNT(AmyRunning2Tiles)), JO_TILE_COUNT(AmyRunning2Tiles), DEFAULT_SPRITE_FRAME_DURATION);
            amy_punch_anim_id = sprite_safe_create_anim(amy_create_blank_animation(JO_TILE_COUNT(AmyPunchTiles)), JO_TILE_COUNT(AmyPunchTiles), DEFAULT_SPRITE_FRAME_DURATION);
            amy_kick_anim_id = sprite_safe_create_anim(amy_create_blank_animation(JO_TILE_COUNT(AmyKickTiles)), JO_TILE_COUNT(AmyKickTiles), DEFAULT_SPRITE_FRAME_DURATION);
        }

        amy_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "AMY_DFT.TGA", JO_COLOR_Green, AmyDefeatedTile, JO_TILE_COUNT(AmyDefeatedTile));

        amy_loaded = true;
    }

    character_ref.walking_anim_id = amy_walking_anim_id;
    character_ref.running1_anim_id = amy_running1_anim_id;
    character_ref.running2_anim_id = amy_running2_anim_id;
    character_ref.stand_sprite_id = amy_stand_anim_id;
    character_ref.spin_sprite_id = amy_sprite_id;
    character_ref.jump_sprite_id = amy_sprite_id;
    character_ref.damage_sprite_id = amy_sprite_id;
    character_ref.defeated_sprite_id = amy_defeated_sprite_id;
    character_ref.defeated_wram_sprite_id = -1;
    character_ref.punch_anim_id = amy_punch_anim_id;
    character_ref.kick_anim_id = amy_kick_anim_id;
    character_registry_apply_combat_profile(&character_ref, UiCharacterAmy);

    // Always ensure this module has a valid physics context, even if it was
    // cleared earlier during a character swap.
    amy_physics = &physics;
    character_ref.charged_kick_hold_ms = 0;
    character_ref.charged_kick_ready = false;
    character_ref.charged_kick_active = false;
    character_ref.charged_kick_phase = 0;
    character_ref.charged_kick_phase_timer = 0;
    character_ref.hit_done_punch1 = false;
    character_ref.hit_done_punch2 = false;
    character_ref.hit_done_kick1 = false;
    character_ref.hit_done_kick2 = false;
    character_ref.attack_cooldown = 0;

    // Inicializa vida
    character_ref.life = 50;
}

void unload_amy(void)
{
    if (!amy_loaded)
        return;

    if (amy_walking_anim_id >= 0) jo_remove_sprite_anim(amy_walking_anim_id);
    if (amy_running1_anim_id >= 0) jo_remove_sprite_anim(amy_running1_anim_id);
    if (amy_running2_anim_id >= 0) jo_remove_sprite_anim(amy_running2_anim_id);
    if (amy_stand_anim_id >= 0) jo_remove_sprite_anim(amy_stand_anim_id);
    if (amy_punch_anim_id >= 0) jo_remove_sprite_anim(amy_punch_anim_id);
    if (amy_kick_anim_id >= 0) jo_remove_sprite_anim(amy_kick_anim_id);

    if (amy_sheet_ready)
    {
        ram_cart_delete_sprite("AMY_FUL");
        amy_sheet_ready = false;
        amy_sheet_width = 0;
        amy_sheet_height = 0;
    }

    if (amy_defeated_sheet_ready)
    {
        character_unload_sheet(&amy_defeated_sheet);
        amy_defeated_sheet_ready = false;
    }

    amy_stand_anim_id = -1;
    amy_walking_anim_id = -1;
    amy_running1_anim_id = -1;
    amy_running2_anim_id = -1;
    amy_punch_anim_id = -1;
    amy_kick_anim_id = -1;
    amy_defeated_sprite_id = -1;

    amy_sprite_id = -1;
    character_ref.wram_sprite_id = -1;
    amy_loaded = false;
}
