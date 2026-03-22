#include <jo/jo.h>
#include <string.h>
#include "tails.h"
#include "player.h"
#include "character_common.h"
#include "character_registry.h"
#include "sprite_safe.h"
#include "runtime_log.h"
#include "ram_cart.h"

extern jo_sidescroller_physics_params physics; // global physics state (from main.c)

static character_t *tails_ref = &player;
static jo_sidescroller_physics_params *tails_physics = &physics;

#define character_ref (*tails_ref)
#define physics (*tails_physics)

void tails_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    if (chr != NULL)
        tails_ref = chr;
    if (phy != NULL)
        tails_physics = phy;
}

#define SPRITE_DIR "SPT"

static bool tails_loaded = false;
static bool tails_sheet_ready = false;
static int tails_sheet_width = 0;
static int tails_sheet_height = 0;
static bool tails_tail_sheet_ready = false;
static jo_img tails_tail_sheet = {0};
static int tails_sprite_id = -1;
static int tails_kick_wram_sprite_id = -1;
static int tails_run2_wram_sprite_id = -1;
static int tails_tail_wram_sprite_id = -1;
static int tails_tail_anim_id = -1;

static bool tails_defeated_sheet_ready = false;
static jo_img tails_defeated_sheet = {0};

// NOTE: per-character animation state is stored directly on the character so
// multiple instances (Player 1 / Player 2 / bots) do not interfere with each other.
// This avoids the case where P2 overwrites P1's current animation/frame.

// Dummy anims used for internal timing / combo logic (punch/kick/speed).
static int tails_stand_anim_id = -1;
static int tails_walking_anim_id = -1;
static int tails_running1_anim_id = -1;
static int tails_running2_anim_id = -1;
static int tails_punch_anim_id = -1;
static int tails_kick_anim_id = -1;

static int tails_defeated_sprite_id;

// TAILS_FUL.TGA layout (8x6):
// Row 0: 0-3 = stand, 4 = jump, 5 = damage, 6 = spin, 7 = unused
// Row 1: walk (8 frames)
// Row 2: run1 (8 frames)
// Row 3: run2 (8 frames)
// Row 4: punch (4 punch1 + 4 punch2)
// Row 5: kick  (4 kick1 + 4 kick2)
static const jo_tile TailsStandTiles[] =
{
    {CHARACTER_WIDTH * 0, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile TailsJumpTile[] __attribute__((unused)) =
{
    {CHARACTER_WIDTH * 4, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile TailsDamageTile[] __attribute__((unused)) =
{
    {CHARACTER_WIDTH * 5, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile TailsSpinTile[] __attribute__((unused)) =
{
    {CHARACTER_WIDTH * 6, CHARACTER_HEIGHT * 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile TailsWalkingTiles[] =
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

static const jo_tile TailsRunning1Tiles[] =
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

static const jo_tile TailsRunning2Tiles[] =
{
    // Tails RUN2 agora tem 4 sprites 64x36 (correção de peculiaridade do asset)
    {0, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH * 2, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH * 2, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH * 2, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, CHARACTER_HEIGHT * 3, CHARACTER_WIDTH * 2, CHARACTER_HEIGHT},
};

static const jo_tile TailsTailTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 5, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 6, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 7, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile TailsPunchTiles[] =
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

static const jo_tile TailsKickTiles[] =
{
    {CHARACTER_WIDTH * 0, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, CHARACTER_HEIGHT * 5, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile TailsDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void tails_copy_defeated_sheet_frame_to_sprite(int sprite_id) __attribute__((unused));
static void tails_copy_defeated_sheet_frame_to_sprite(int sprite_id)
{
    character_copy_defeated_sheet_frame_to_sprite(sprite_id, &tails_defeated_sheet, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT);
}

static int tails_ensure_defeated_wram_sprite(character_t *chr) __attribute__((unused));
static int tails_ensure_defeated_wram_sprite(character_t *chr)
{
    return character_ensure_defeated_wram_sprite(chr, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT);
}

static void tails_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y) __attribute__((unused));
static void tails_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y)
{
    character_copy_cart_sheet_frame_to_sprite(sprite_id, "TAILS_FUL", tails_sheet_width, tails_sheet_height, frame_x, frame_y, CHARACTER_WIDTH, CHARACTER_HEIGHT);
}

static int tails_create_blank_animation(int frame_count)
{
    return character_create_blank_animation(frame_count);
}

static int tails_create_blank_sprite(void) __attribute__((unused));
static int tails_create_blank_sprite(void)
{
    return character_create_blank_sprite();
}

// The full WRAM-per-character sprite approach avoids P1/P2 sharing the same
// backing sprite, which would cause one entity to overwrite the other's frame
// each update.
static int tails_ensure_wram_sprite(character_t *chr)
{
    return character_ensure_wram_sprite(chr, &tails_sprite_id);
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

This matches Tails's behavior and ensures stand/walk/run timing is consistent.
*/

void tails_running_animation_handling(void)
{
    character_running_animation_handling(&character_ref, &physics);
}

static void tails_draw_for_character(character_t *chr)
{
    if (!tails_sheet_ready)
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

    if (character_draw_defeated(chr, tails_ensure_defeated_wram_sprite(chr), &tails_defeated_sheet, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT))
        return;

    // Tail overlay: loop 0..7 do TLS_TLS.TGA, aparece atrás do personagem.
    // Não exibir em spin, defeated, stun, run2 ou kicks ativos.
    if (!chr->spin && chr->life > 0 && chr->stun_timer <= 0 && !chr->kick && !chr->kick2 && chr->run != 2 && tails_tail_sheet_ready && tails_tail_wram_sprite_id >= 0 && tails_tail_anim_id >= 0)
    {
        if (jo_is_sprite_anim_stopped(tails_tail_anim_id))
            jo_start_sprite_anim_loop(tails_tail_anim_id);

        int tail_frame = jo_get_sprite_anim_frame(tails_tail_anim_id) % JO_TILE_COUNT(TailsTailTiles);
        if (tail_frame < 0)
            tail_frame = 0;

        character_copy_sheet_frame_to_sprite_with_size(
            tails_tail_wram_sprite_id,
            &tails_tail_sheet,
            tail_frame * CHARACTER_WIDTH,
            0,
            CHARACTER_WIDTH,
            CHARACTER_HEIGHT);

        int tail_x = chr->x + (chr->flip ? 16 : -16);
        int tail_y = chr->y + 4;
        jo_sprite_draw3D2(tails_tail_wram_sprite_id, tail_x, tail_y, CHARACTER_SPRITE_Z + 1);
    }

    if (chr->kick || chr->kick2)
    {
        int kick_frame = (chr->kick_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->kick_anim_id) : 0;
        int frame_x;
        int frame_width;

        if (kick_frame <= 1)
        {
            frame_x = kick_frame * CHARACTER_WIDTH;
            frame_width = CHARACTER_WIDTH;
        }
        else
        {
            frame_x = 2 * CHARACTER_WIDTH + (kick_frame - 2) * (CHARACTER_WIDTH * 2);
            frame_width = CHARACTER_WIDTH * 2;
        }

        if (tails_kick_wram_sprite_id < 0)
            tails_kick_wram_sprite_id = character_create_blank_sprite_with_size(CHARACTER_WIDTH * 2, CHARACTER_HEIGHT);

        if (!character_copy_cart_sheet_frame_to_sprite(tails_kick_wram_sprite_id, "TLS_FUL", tails_sheet_width, tails_sheet_height, frame_x, CHARACTER_HEIGHT * 5, frame_width, CHARACTER_HEIGHT))
            return;

        int draw_x = chr->x;
        if (kick_frame <= 1 && chr->flip)
        {
            // For left-facing small frames, adjust left to keep body steady.
            draw_x = chr->x - (CHARACTER_WIDTH);
        }

        if (kick_frame >= 2 && chr->flip)
        {
            // For left-facing large frames, adjust left to center on same body location.
            draw_x = chr->x - (CHARACTER_WIDTH-16);
        }

        if (kick_frame >= 2 && !chr->flip)
        {
            // For right-facing large frames, adjust left to center on same body location.
            draw_x = chr->x - (CHARACTER_WIDTH / 2);
        }

        jo_sprite_draw3D2(tails_kick_wram_sprite_id, draw_x, chr->y, CHARACTER_SPRITE_Z);
        return;
    }

    if (chr->walk && chr->run == 2)
    {
        int run_frame = (chr->running2_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->running2_anim_id) : 0;
        int frame_index = run_frame % 4;

        if (tails_run2_wram_sprite_id < 0)
            tails_run2_wram_sprite_id = character_create_blank_sprite_with_size(CHARACTER_WIDTH * 2, CHARACTER_HEIGHT);

        if (!character_copy_cart_sheet_frame_to_sprite(tails_run2_wram_sprite_id,
            "TLS_FUL",
            tails_sheet_width,
            tails_sheet_height,
            frame_index * (CHARACTER_WIDTH * 2),
            CHARACTER_HEIGHT * 3,
            CHARACTER_WIDTH * 2,
            CHARACTER_HEIGHT))
            return;

        int draw_x = chr->x - (CHARACTER_WIDTH / 2);
        jo_sprite_draw3D2(tails_run2_wram_sprite_id, draw_x, chr->y, CHARACTER_SPRITE_Z);
        return;
    }

    int sprite_id = tails_ensure_wram_sprite(chr);
    if (sprite_id < 0)
        return;

    if (character_draw_cart_frame(chr, sprite_id, "TAILS_FUL", tails_sheet_width, tails_sheet_height))
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

void tails_draw(character_t *chr)
{
    tails_draw_for_character(chr);
}

void display_tails(void)
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
    tails_draw_for_character(&character_ref);

    if (character_ref.flip)
        jo_sprite_disable_horizontal_flip();

    // debug life bar (mantendo compatibilidade):
    // int life_percent = (character_ref.life * 100) / 50;
    // int bar_max_width = 20;
    // int bar_width = (life_percent * bar_max_width) / 100;
    // char bar[bar_max_width + 1];
    // for (int i = 0; i < bar_max_width; ++i)
    //     bar[i] = (i < bar_width) ? '#' : '-';
    // bar[bar_max_width] = '\0';
    // jo_printf(1, 26, "P1 : [%s] %d%%", bar, life_percent); // debug life bar temporário
}

void load_tails(void)
{
    if (!tails_loaded)
    {
        if (!tails_sheet_ready)
        {
            jo_img sheet = {0};
            if (character_load_sheet(&sheet, "TLS_FUL.TGA", SPRITE_DIR, JO_COLOR_Green))
            {
                if (ram_cart_store_sprite("TAILS_FUL", sheet.data, (size_t)sheet.width * (size_t)sheet.height * sizeof(unsigned short)))
                {
                    tails_sheet_width = sheet.width;
                    tails_sheet_height = sheet.height;
                    tails_sheet_ready = true;
                }
                character_unload_sheet(&sheet);
            }
        }

        if (!tails_tail_sheet_ready)
            tails_tail_sheet_ready = character_load_sheet(&tails_tail_sheet, "TLS_TLS.TGA", SPRITE_DIR, JO_COLOR_Green);

        runtime_log("tails_load: sheet=%d defeated=%d tail=%d wsprite=%d", (int)tails_sheet_ready, (int)tails_defeated_sheet_ready, (int)tails_tail_sheet_ready, tails_sprite_id);

        // Create a single VRAM sprite that will be updated each frame.
        if (tails_sprite_id < 0)
            tails_sprite_id = character_create_blank_sprite();

        // Create an optional WRAM sprite for large kick frames (64x36) in tails.
        if (tails_kick_wram_sprite_id < 0)
            tails_kick_wram_sprite_id = character_create_blank_sprite_with_size(CHARACTER_WIDTH * 2, CHARACTER_HEIGHT);

        // Create an optional WRAM sprite for run2 64x36.
        if (tails_run2_wram_sprite_id < 0)
            tails_run2_wram_sprite_id = character_create_blank_sprite_with_size(CHARACTER_WIDTH * 2, CHARACTER_HEIGHT);

        // Create an optional WRAM sprite for tail overlay (32x36).
        if (tails_tail_wram_sprite_id < 0)
            tails_tail_wram_sprite_id = character_create_blank_sprite();

        /* Use the global sprite as the base WRAM sprite for this character; P2/bots
         * will allocate their own WRAM sprite as needed. */
        character_ref.wram_sprite_id = tails_sprite_id;

        // Dummy animation objects (used only for timing / combo logic).
        if (sprite_safe_can_create_anim())
        {
            tails_stand_anim_id = sprite_safe_create_anim(tails_create_blank_animation(JO_TILE_COUNT(TailsStandTiles)), JO_TILE_COUNT(TailsStandTiles), DEFAULT_SPRITE_FRAME_DURATION);
            tails_walking_anim_id = sprite_safe_create_anim(tails_create_blank_animation(JO_TILE_COUNT(TailsWalkingTiles)), JO_TILE_COUNT(TailsWalkingTiles), DEFAULT_SPRITE_FRAME_DURATION);
            tails_running1_anim_id = sprite_safe_create_anim(tails_create_blank_animation(JO_TILE_COUNT(TailsRunning1Tiles)), JO_TILE_COUNT(TailsRunning1Tiles), DEFAULT_SPRITE_FRAME_DURATION);
            tails_running2_anim_id = sprite_safe_create_anim(tails_create_blank_animation(JO_TILE_COUNT(TailsRunning2Tiles)), JO_TILE_COUNT(TailsRunning2Tiles), DEFAULT_SPRITE_FRAME_DURATION);
            tails_punch_anim_id = sprite_safe_create_anim(tails_create_blank_animation(JO_TILE_COUNT(TailsPunchTiles)), JO_TILE_COUNT(TailsPunchTiles), DEFAULT_SPRITE_FRAME_DURATION);
            tails_kick_anim_id = sprite_safe_create_anim(tails_create_blank_animation(JO_TILE_COUNT(TailsKickTiles)), JO_TILE_COUNT(TailsKickTiles), DEFAULT_SPRITE_FRAME_DURATION);
            tails_tail_anim_id = sprite_safe_create_anim(tails_create_blank_animation(JO_TILE_COUNT(TailsTailTiles)), JO_TILE_COUNT(TailsTailTiles), DEFAULT_SPRITE_FRAME_DURATION);
        }

        tails_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "TLS_DFT.TGA", JO_COLOR_Green, TailsDefeatedTile, JO_TILE_COUNT(TailsDefeatedTile));

        tails_loaded = true;
    }

    character_ref.walking_anim_id = tails_walking_anim_id;
    character_ref.running1_anim_id = tails_running1_anim_id;
    character_ref.running2_anim_id = tails_running2_anim_id;
    character_ref.stand_sprite_id = tails_stand_anim_id;
    character_ref.spin_sprite_id = tails_sprite_id;
    character_ref.jump_sprite_id = tails_sprite_id;
    character_ref.damage_sprite_id = tails_sprite_id;
    character_ref.defeated_sprite_id = tails_defeated_sprite_id;
    character_ref.punch_anim_id = tails_punch_anim_id;
    character_ref.kick_anim_id = tails_kick_anim_id;
        character_ref.defeated_wram_sprite_id = -1;
    // Always ensure this module has a valid physics context, even if it was
    // cleared earlier during a character swap.
    tails_physics = &physics;
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

void unload_tails(void)
{
    if (!tails_loaded)
        return;

    if (tails_walking_anim_id >= 0) jo_remove_sprite_anim(tails_walking_anim_id);
    if (tails_running1_anim_id >= 0) jo_remove_sprite_anim(tails_running1_anim_id);
    if (tails_running2_anim_id >= 0) jo_remove_sprite_anim(tails_running2_anim_id);
    if (tails_stand_anim_id >= 0) jo_remove_sprite_anim(tails_stand_anim_id);
    if (tails_punch_anim_id >= 0) jo_remove_sprite_anim(tails_punch_anim_id);
    if (tails_kick_anim_id >= 0) jo_remove_sprite_anim(tails_kick_anim_id);

    if (tails_kick_wram_sprite_id >= 0)
    {
        jo_sprite_free_from(tails_kick_wram_sprite_id);
        tails_kick_wram_sprite_id = -1;
    }

    if (tails_run2_wram_sprite_id >= 0)
    {
        jo_sprite_free_from(tails_run2_wram_sprite_id);
        tails_run2_wram_sprite_id = -1;
    }

    if (tails_tail_wram_sprite_id >= 0)
    {
        jo_sprite_free_from(tails_tail_wram_sprite_id);
        tails_tail_wram_sprite_id = -1;
    }

    if (character_ref.defeated_wram_sprite_id >= 0)
    {
        jo_sprite_free_from(character_ref.defeated_wram_sprite_id);
        character_ref.defeated_wram_sprite_id = -1;
    }

    if (tails_tail_anim_id >= 0)
    {
        jo_remove_sprite_anim(tails_tail_anim_id);
        tails_tail_anim_id = -1;
    }

    if (tails_sheet_ready)
    {
        ram_cart_delete_sprite("TAILS_FUL");
        tails_sheet_ready = false;
        tails_sheet_width = 0;
        tails_sheet_height = 0;
    }

    if (tails_defeated_sheet_ready)
    {
        tails_defeated_sheet_ready = false;
    }

    if (tails_tail_sheet_ready)
    {
        character_unload_sheet(&tails_tail_sheet);
        tails_tail_sheet_ready = false;
    }

    tails_stand_anim_id = -1;
    tails_walking_anim_id = -1;
    tails_running1_anim_id = -1;
    tails_running2_anim_id = -1;
    tails_punch_anim_id = -1;
    tails_kick_anim_id = -1;
    tails_defeated_sprite_id = -1;

    tails_sprite_id = -1;
    character_ref.wram_sprite_id = -1;
    tails_loaded = false;
}
