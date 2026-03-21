#include <jo/jo.h>
#include <string.h>
#include "sonic.h"
#include "player.h"
#include "character_common.h"
#include "character_registry.h"
#include "sprite_safe.h"
#include "runtime_log.h"

extern jo_sidescroller_physics_params physics; // global physics state (from main.c)

static character_t *sonic_ref = &player;
static jo_sidescroller_physics_params *sonic_physics = &physics;

#define character_ref (*sonic_ref)
#define physics (*sonic_physics)

void sonic_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    if (chr != NULL)
        sonic_ref = chr;
    if (phy != NULL)
        sonic_physics = phy;
}

#define SPRITE_DIR "SPT"
#define DEFEATED_SPRITE_WIDTH 48
#define DEFEATED_SPRITE_HEIGHT 32

static bool sonic_loaded = false;
static bool sonic_sheet_ready = false;
static jo_img sonic_sheet = {0};
static int sonic_sprite_id = -1;

static bool sonic_defeated_sheet_ready = false;
static jo_img sonic_defeated_sheet = {0};

// NOTE: per-character animation state is stored directly on the character so
// multiple instances (Player 1 / Player 2 / bots) do not interfere with each other.
// This avoids the case where P2 overwrites P1's current animation/frame.

// Dummy anims used for internal timing / combo logic (punch/kick/speed).
static int sonic_stand_anim_id = -1;
static int sonic_walking_anim_id = -1;
static int sonic_running1_anim_id = -1;
static int sonic_running2_anim_id = -1;
static int sonic_punch_anim_id = -1;
static int sonic_kick_anim_id = -1;

static int sonic_defeated_sprite_id;

// SNC_FUL.TGA layout (8x6):
// Row 0: 0-3 = stand, 4 = jump, 5 = damage, 6 = spin, 7 = unused
// Row 1: walk (8 frames)
// Row 2: run1 (8 frames)
// Row 3: run2 (8 frames)
// Row 4: punch (4 punch1 + 4 punch2)
// Row 5: kick  (4 kick1 + 4 kick2)
static const jo_tile SonicStandTiles[] =
{
    {CHARACTER_WIDTH * 0, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile SonicJumpTile[] =
{
    {CHARACTER_WIDTH * 4, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile SonicDamageTile[] =
{
    {CHARACTER_WIDTH * 5, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile SonicSpinTile[] =
{
    {CHARACTER_WIDTH * 6, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile SonicWalkingTiles[] =
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

static const jo_tile SonicRunning1Tiles[] =
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

static const jo_tile SonicRunning2Tiles[] =
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

static const jo_tile SonicPunchTiles[] =
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

static const jo_tile SonicKickTiles[] =
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

static const jo_tile SonicDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void sonic_copy_defeated_sheet_frame_to_sprite(int sprite_id)
{
    character_copy_defeated_sheet_frame_to_sprite(sprite_id, &sonic_defeated_sheet, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT);
}

static int sonic_ensure_defeated_wram_sprite(character_t *chr)
{
    return character_ensure_defeated_wram_sprite(chr, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT);
}

static void sonic_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y)
{
    character_copy_sheet_frame_to_sprite(sprite_id, &sonic_sheet, frame_x, frame_y);
}

static int sonic_create_blank_animation(int frame_count)
{
    return character_create_blank_animation(frame_count);
}

static int sonic_create_blank_sprite(void)
{
    return character_create_blank_sprite();
}

static void sonic_render_current_frame_for(const character_t *chr, int sprite_id)
{
    if (!sonic_sheet_ready || sprite_id < 0)
        return;

    int row = 0;
    int col = 0;

    if (chr->spin)
    {
        row = 0;
        col = 6;
    }
    else if (chr->life <= 0)
    {
        int defeated_sprite_id = sonic_ensure_defeated_wram_sprite((character_t *)chr);
        if (defeated_sprite_id >= 0)
            sonic_copy_defeated_sheet_frame_to_sprite(defeated_sprite_id);
        return;
    }
    else if (chr->stun_timer > 0)
    {
        row = 0;
        col = 5;
    }
    else if (chr->punch || chr->punch2)
    {
        row = 4;
        col = (chr->punch_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->punch_anim_id) : 0;
    }
    else if (chr->kick || chr->kick2)
    {
        row = 5;
        col = (chr->kick_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->kick_anim_id) : 0;
    }
    else if (chr->jump)
    {
        row = 0;
        col = 4;
    }
    else
    {
        if (chr->walk && chr->run == 0)
        {
            row = 1;
            col = (chr->walking_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->walking_anim_id) : 0;
        }
        else if (chr->walk && chr->run == 1)
        {
            row = 2;
            col = (chr->running1_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->running1_anim_id) : 0;
        }
        else if (chr->walk && chr->run == 2)
        {
            row = 3;
            col = (chr->running2_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->running2_anim_id) : 0;
        }
        else
        {
            row = 0;
            col = (chr->stand_sprite_id >= 0) ? jo_get_sprite_anim_frame(chr->stand_sprite_id) : 0;
        }
    }

    sonic_copy_sheet_frame_to_sprite(sprite_id, col * CHARACTER_WIDTH, row * CHARACTER_HEIGHT);
}

// The full WRAM-per-character sprite approach avoids P1/P2 sharing the same
// backing sprite, which would cause one entity to overwrite the other's frame
// each update.
static int sonic_ensure_wram_sprite(character_t *chr)
{
    return character_ensure_wram_sprite(chr, &sonic_sprite_id);
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

void sonic_running_animation_handling(void)
{
    character_running_animation_handling(&character_ref, &physics);
}

static void sonic_draw_for_character(character_t *chr)
{
    if (!sonic_sheet_ready)
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

    if (character_draw_defeated(chr, sonic_ensure_defeated_wram_sprite(chr), &sonic_defeated_sheet, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT))
        return;

    int sprite_id = sonic_ensure_wram_sprite(chr);
    if (sprite_id < 0)
        return;

    if (character_draw_sheet_frame(chr, sprite_id, &sonic_sheet))
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

void sonic_draw(character_t *chr)
{
    sonic_draw_for_character(chr);
}

void display_sonic(void)
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
    sonic_draw_for_character(&character_ref);

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

void load_sonic(void)
{
    if (!sonic_loaded)
    {
        // Load the combined sheet into WRAM so we can copy frames on demand.
        if (!sonic_sheet_ready)
            sonic_sheet_ready = character_load_sheet(&sonic_sheet, "SNC_FUL.TGA", SPRITE_DIR, JO_COLOR_Green);

        // Load defeated sprite sheet into WRAM so it can also be copied on demand.
        if (!sonic_defeated_sheet_ready)
            sonic_defeated_sheet_ready = character_load_sheet(&sonic_defeated_sheet, "SNC_DFT.TGA", SPRITE_DIR, JO_COLOR_Green);

        // Create a single VRAM sprite that will be updated each frame.
        if (sonic_sprite_id < 0)
            sonic_sprite_id = character_create_blank_sprite();

        /* Use the global sprite as the base WRAM sprite for this character; P2/bots
         * will allocate their own WRAM sprite as needed. */
        character_ref.wram_sprite_id = sonic_sprite_id;

        // Dummy animation objects (used only for timing / combo logic).
        if (sprite_safe_can_create_anim())
        {
            sonic_stand_anim_id = sprite_safe_create_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicStandTiles)), JO_TILE_COUNT(SonicStandTiles), DEFAULT_SPRITE_FRAME_DURATION);
            sonic_walking_anim_id = sprite_safe_create_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicWalkingTiles)), JO_TILE_COUNT(SonicWalkingTiles), DEFAULT_SPRITE_FRAME_DURATION);
            sonic_running1_anim_id = sprite_safe_create_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicRunning1Tiles)), JO_TILE_COUNT(SonicRunning1Tiles), DEFAULT_SPRITE_FRAME_DURATION);
            sonic_running2_anim_id = sprite_safe_create_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicRunning2Tiles)), JO_TILE_COUNT(SonicRunning2Tiles), DEFAULT_SPRITE_FRAME_DURATION);
            sonic_punch_anim_id = sprite_safe_create_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicPunchTiles)), JO_TILE_COUNT(SonicPunchTiles), DEFAULT_SPRITE_FRAME_DURATION);
            sonic_kick_anim_id = sprite_safe_create_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicKickTiles)), JO_TILE_COUNT(SonicKickTiles), DEFAULT_SPRITE_FRAME_DURATION);
        }

        sonic_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SNC_DFT.TGA", JO_COLOR_Green, SonicDefeatedTile, JO_TILE_COUNT(SonicDefeatedTile));

        sonic_loaded = true;
    }

    character_ref.walking_anim_id = sonic_walking_anim_id;
    character_ref.running1_anim_id = sonic_running1_anim_id;
    character_ref.running2_anim_id = sonic_running2_anim_id;
    character_ref.stand_sprite_id = sonic_stand_anim_id;
    character_ref.spin_sprite_id = sonic_sprite_id;
    character_ref.jump_sprite_id = sonic_sprite_id;
    character_ref.damage_sprite_id = sonic_sprite_id;
    character_ref.defeated_sprite_id = sonic_defeated_sprite_id;
    character_ref.punch_anim_id = sonic_punch_anim_id;
    character_ref.kick_anim_id = sonic_kick_anim_id;
    character_registry_apply_combat_profile(&character_ref, UiCharacterSonic);

    // Always ensure this module has a valid physics context, even if it was
    // cleared earlier during a character swap.
    sonic_physics = &physics;
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

void unload_sonic(void)
{
    if (!sonic_loaded)
        return;

    if (sonic_walking_anim_id >= 0) jo_remove_sprite_anim(sonic_walking_anim_id);
    if (sonic_running1_anim_id >= 0) jo_remove_sprite_anim(sonic_running1_anim_id);
    if (sonic_running2_anim_id >= 0) jo_remove_sprite_anim(sonic_running2_anim_id);
    if (sonic_stand_anim_id >= 0) jo_remove_sprite_anim(sonic_stand_anim_id);
    if (sonic_punch_anim_id >= 0) jo_remove_sprite_anim(sonic_punch_anim_id);
    if (sonic_kick_anim_id >= 0) jo_remove_sprite_anim(sonic_kick_anim_id);

    if (sonic_sheet_ready)
    {
        character_unload_sheet(&sonic_sheet);
        sonic_sheet_ready = false;
    }

    if (sonic_defeated_sheet_ready)
    {
        character_unload_sheet(&sonic_defeated_sheet);
        sonic_defeated_sheet_ready = false;
    }

    sonic_stand_anim_id = -1;
    sonic_walking_anim_id = -1;
    sonic_running1_anim_id = -1;
    sonic_running2_anim_id = -1;
    sonic_punch_anim_id = -1;
    sonic_kick_anim_id = -1;
    sonic_defeated_sprite_id = -1;

    sonic_sprite_id = -1;
    character_ref.wram_sprite_id = -1;
    sonic_loaded = false;
}
