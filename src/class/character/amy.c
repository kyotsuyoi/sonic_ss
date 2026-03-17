#include <jo/jo.h>
#include <string.h>
#include "sonic.h"
#include "amy.h"
#include "player.h"
#include "game_constants.h"
#include "character_registry.h"
#include "sprite_safe.h"
#include "runtime_log.h"

extern jo_sidescroller_physics_params physics;

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
 // used for initialization

#define SPRITE_DIR "SPT"
#define DEFEATED_SPRITE_WIDTH 40
#define DEFEATED_SPRITE_HEIGHT 32

static bool amy_loaded = false;
static int amy_walking_base_id;
static int amy_running1_base_id;
static int amy_running2_base_id;
static int amy_stand_base_id;
static int amy_punch_base_id;
static int amy_kick_base_id;
static int amy_walking_anim_id;
static int amy_running1_anim_id;
static int amy_running2_anim_id;
static int amy_stand_anim_id;
static int amy_punch_anim_id;
static int amy_kick_anim_id;
static int amy_spin_sprite_id;
static int amy_jump_sprite_id;
static int amy_damage_sprite_id;
static int amy_defeated_sprite_id;

// WRAM-backed sprite sheet (AMY_FUL.TGA) for faster VRAM uploads.
static bool amy_sheet_ready = false;
static jo_img amy_sheet = {0};
static int amy_sprite_id = -1;

static bool amy_defeated_sheet_ready = false;
static jo_img amy_defeated_sheet = {0};

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
    {0, 0, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void amy_copy_defeated_sheet_frame_to_sprite(int sprite_id)
{
    if (!amy_defeated_sheet_ready || sprite_id < 0)
        return;

    static jo_img tmp = {0};
    if (tmp.data == JO_NULL)
    {
        tmp.width = DEFEATED_SPRITE_WIDTH;
        tmp.height = DEFEATED_SPRITE_HEIGHT;
        tmp.data = jo_malloc((size_t)DEFEATED_SPRITE_WIDTH * (size_t)DEFEATED_SPRITE_HEIGHT * sizeof(unsigned short));
        if (tmp.data == JO_NULL)
            return;
    }

    unsigned short *dst = (unsigned short *)tmp.data;
    unsigned short *src = (unsigned short *)amy_defeated_sheet.data;
    int sheet_width = amy_defeated_sheet.width;

    for (int y = 0; y < DEFEATED_SPRITE_HEIGHT; ++y)
    {
        unsigned short *src_row = src + y * sheet_width;
        unsigned short *dst_row = dst + y * DEFEATED_SPRITE_WIDTH;
        memcpy(dst_row, src_row, DEFEATED_SPRITE_WIDTH * sizeof(unsigned short));
    }

    jo_sprite_replace(&tmp, sprite_id);
}

static int amy_ensure_defeated_wram_sprite(character_t *chr)
{
    if (chr->defeated_sprite_id < 0)
    {
        jo_img blank;
        blank.width = DEFEATED_SPRITE_WIDTH;
        blank.height = DEFEATED_SPRITE_HEIGHT;
        blank.data = (unsigned short *)jo_malloc((size_t)DEFEATED_SPRITE_WIDTH * (size_t)DEFEATED_SPRITE_HEIGHT * sizeof(unsigned short));
        if (blank.data == JO_NULL)
            return -1;

        for (size_t i = 0; i < (size_t)DEFEATED_SPRITE_WIDTH * (size_t)DEFEATED_SPRITE_HEIGHT; ++i)
            ((unsigned short *)blank.data)[i] = JO_COLOR_Transparent;

        int id = jo_sprite_add(&blank);
        jo_free_img(&blank);
        chr->defeated_sprite_id = id;
    }

    return chr->defeated_sprite_id;
}

static int amy_try_add_tga_tileset(const char *filename, const jo_tile *tiles, int count)
{
    char *data = jo_fs_read_file_in_dir(filename, SPRITE_DIR, JO_NULL);
    if (data == JO_NULL)
        return -1;
    jo_free(data);
    return jo_sprite_add_tga_tileset(SPRITE_DIR, filename, JO_COLOR_Green, tiles, count);
}

static int amy_try_add_tga(const char *filename)
{
    char *data = jo_fs_read_file_in_dir(filename, SPRITE_DIR, JO_NULL);
    if (data == JO_NULL)
        return -1;
    jo_free(data);
    return jo_sprite_add_tga(SPRITE_DIR, filename, JO_COLOR_Green);
}

static void amy_reset_animation_lists_except(int active_anim_id)
{
    if (character_ref.walking_anim_id >= 0 && character_ref.walking_anim_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.walking_anim_id);
    if (character_ref.running1_anim_id >= 0 && character_ref.running1_anim_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.running1_anim_id);
    if (character_ref.running2_anim_id >= 0 && character_ref.running2_anim_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.running2_anim_id);
    if (character_ref.stand_sprite_id >= 0 && character_ref.stand_sprite_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.stand_sprite_id);
    if (character_ref.punch_anim_id >= 0 && character_ref.punch_anim_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.punch_anim_id);
    if (character_ref.kick_anim_id >= 0 && character_ref.kick_anim_id != active_anim_id)
        jo_reset_sprite_anim(character_ref.kick_anim_id);
}

static void amy_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y)
{
    if (!amy_sheet_ready || sprite_id < 0)
        return;

    static jo_img tmp = {0};
    if (tmp.data == JO_NULL)
    {
        tmp.width = CHARACTER_WIDTH;
        tmp.height = CHARACTER_HEIGHT;
        tmp.data = jo_malloc((size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT * sizeof(unsigned short));
        if (tmp.data == JO_NULL)
            return;
    }

    unsigned short *dst = (unsigned short *)tmp.data;
    unsigned short *src = (unsigned short *)amy_sheet.data;
    int sheet_width = amy_sheet.width;

    for (int y = 0; y < CHARACTER_HEIGHT; ++y)
    {
        unsigned short *src_row = src + (frame_y + y) * sheet_width + frame_x;
        unsigned short *dst_row = dst + y * CHARACTER_WIDTH;
        memcpy(dst_row, src_row, CHARACTER_WIDTH * sizeof(unsigned short));
    }

    jo_sprite_replace(&tmp, sprite_id);
}

static int amy_create_blank_animation(int frame_count)
{
    jo_img blank;
    blank.width = CHARACTER_WIDTH;
    blank.height = CHARACTER_HEIGHT;
    blank.data = (unsigned short *)jo_malloc((size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT * sizeof(unsigned short));
    if (blank.data == JO_NULL)
        return -1;

    for (size_t i = 0; i < (size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT; ++i)
        ((unsigned short *)blank.data)[i] = JO_COLOR_Transparent;

    int base = -1;
    for (int i = 0; i < frame_count; ++i)
    {
        int id = jo_sprite_add(&blank);
        if (id < 0)
            break;
        if (base < 0)
            base = id;
    }

    jo_free_img(&blank);
    return base;
}

static int amy_create_blank_sprite(void)
{
    jo_img blank;
    blank.width = CHARACTER_WIDTH;
    blank.height = CHARACTER_HEIGHT;
    blank.data = (unsigned short *)jo_malloc((size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT * sizeof(unsigned short));
    if (blank.data == JO_NULL)
        return -1;

    for (size_t i = 0; i < (size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT; ++i)
        ((unsigned short *)blank.data)[i] = JO_COLOR_Transparent;

    int id = jo_sprite_add(&blank);
    jo_free_img(&blank);
    return id;
}

static void amy_render_current_frame_for(const character_t *chr, int sprite_id)
{
    if (!amy_sheet_ready || sprite_id < 0)
        return;

    int row = 0;
    int col = 0;

    if (chr->spin)
    {
        row = 0;
        col = 6; // spin in AMY_FUL.TGA is at row 0, col 6
    }
    else if (chr->life <= 0)
    {
        int defeated_sprite_id = amy_ensure_defeated_wram_sprite((character_t *)chr);
        if (defeated_sprite_id >= 0)
            amy_copy_defeated_sheet_frame_to_sprite(defeated_sprite_id);
        return;
    }
    else if (chr->stun_timer > 0)
    {
        row = 0;
        col = 5; // damage in AMY_FUL.TGA is at row 0, col 5
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
        col = 4; // jump in AMY_FUL.TGA is at row 0, col 4
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

    // Copy from the sheet into the requested sprite
    amy_copy_sheet_frame_to_sprite(sprite_id, col * CHARACTER_WIDTH, row * CHARACTER_HEIGHT);
}

static void amy_render_current_frame(void)
{
    amy_render_current_frame_for(&character_ref, amy_sprite_id);
}

static int amy_ensure_wram_sprite(character_t *chr)
{
    if (chr->wram_sprite_id < 0)
    {
        chr->wram_sprite_id = amy_create_blank_sprite();
        runtime_log("amy: created wram sprite id=%d for char=%d", chr->wram_sprite_id, chr->character_id);
    }

    if (chr->wram_sprite_id < 0)
        runtime_log("amy: failed to create wram sprite for char=%d", chr->character_id);

    return chr->wram_sprite_id;
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

    if (chr->life <= 0)
    {
        int defeated_sprite_id = amy_ensure_defeated_wram_sprite(chr);
        if (defeated_sprite_id >= 0)
            amy_copy_defeated_sheet_frame_to_sprite(defeated_sprite_id);

        if (defeated_sprite_id >= 0)
            jo_sprite_draw3D2(defeated_sprite_id,
                              chr->x,
                              chr->y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT),
                              CHARACTER_SPRITE_Z);
        return;
    }

    int sprite_id = amy_ensure_wram_sprite(chr);
    if (sprite_id < 0)
        return;

    amy_render_current_frame_for(chr, sprite_id);

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

void amy_running_animation_handling(void)
{
    int speed_step;

    if (jo_physics_is_standing(&physics))
    {
        jo_reset_sprite_anim(character_ref.running1_anim_id);
        jo_reset_sprite_anim(character_ref.running2_anim_id);

        if (!jo_is_sprite_anim_stopped(character_ref.walking_anim_id)){
            jo_reset_sprite_anim(character_ref.walking_anim_id);
            jo_reset_sprite_anim(character_ref.stand_sprite_id);
        }else{
            if (jo_is_sprite_anim_stopped(character_ref.stand_sprite_id))
                jo_start_sprite_anim_loop(character_ref.stand_sprite_id);

            if (jo_get_sprite_anim_frame(character_ref.stand_sprite_id) == 0)
                jo_set_sprite_anim_frame_rate(character_ref.stand_sprite_id, 70);
            else
                jo_set_sprite_anim_frame_rate(character_ref.stand_sprite_id, 2);
        }
    }
    else
    {
        if (character_ref.run == 0)
        {
            jo_reset_sprite_anim(character_ref.running1_anim_id);
            jo_reset_sprite_anim(character_ref.running2_anim_id);

            if (jo_is_sprite_anim_stopped(character_ref.walking_anim_id))
                jo_start_sprite_anim_loop(character_ref.walking_anim_id);
        }
        else if (character_ref.run == 1)
        {
            jo_reset_sprite_anim(character_ref.walking_anim_id);
            jo_reset_sprite_anim(character_ref.running2_anim_id);

            if (jo_is_sprite_anim_stopped(character_ref.running1_anim_id))
                jo_start_sprite_anim_loop(character_ref.running1_anim_id);
        }
        else if (character_ref.run == 2)
        {
            jo_reset_sprite_anim(character_ref.walking_anim_id);
            jo_reset_sprite_anim(character_ref.running1_anim_id);

            if (jo_is_sprite_anim_stopped(character_ref.running2_anim_id))
                jo_start_sprite_anim_loop(character_ref.running2_anim_id);
        }

        speed_step = (int)JO_ABS(physics.speed);

        if (speed_step >= 6){
            jo_set_sprite_anim_frame_rate(character_ref.running2_anim_id, 3);
            character_ref.run = 2;
        }else if (speed_step >= 5){
            jo_set_sprite_anim_frame_rate(character_ref.running1_anim_id, 4);
            character_ref.run = 1;
        }else if (speed_step >= 4){
            jo_set_sprite_anim_frame_rate(character_ref.running1_anim_id, 5);
            character_ref.run = 1;
        }else if (speed_step >= 3){
            jo_set_sprite_anim_frame_rate(character_ref.running1_anim_id, 6);
            character_ref.run = 1;
        }else if (speed_step >= 2){
            jo_set_sprite_anim_frame_rate(character_ref.running1_anim_id, 7);
            character_ref.run = 1;
        }else if (speed_step >= 1){
            jo_set_sprite_anim_frame_rate(character_ref.walking_anim_id, 8);
            character_ref.run = 0;
        }else{
            jo_set_sprite_anim_frame_rate(character_ref.walking_anim_id, 9);
            character_ref.run = 0;
        }
    }

    player_update_punch_state_for_character(&character_ref);
    player_update_kick_state_for_character(&character_ref);
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
    static bool debug_once = false;

    if (!amy_loaded)
    {
        // Try to load the combined WRAM sheet first.
        if (!amy_sheet_ready)
        {
            char *sheet_data = jo_fs_read_file_in_dir("AMY_FUL.TGA", SPRITE_DIR, JO_NULL);
            if (sheet_data != JO_NULL)
            {
                if (jo_tga_loader_from_stream(&amy_sheet, sheet_data, JO_COLOR_Green) == JO_TGA_OK)
                {
                    amy_sheet_ready = true;
                    runtime_log("amy: WRAM sheet loaded");
                }
                else
                {
                    runtime_log("amy: WRAM sheet failed to load");
                }
                jo_free(sheet_data);
            }
            else
            {
                runtime_log("amy: WRAM sheet missing");
            }
        }

        // Load defeated sprite sheet into WRAM so it can also be copied on demand.
        if (!amy_defeated_sheet_ready)
        {
            char *sheet_data = jo_fs_read_file_in_dir("AMY_DFT.TGA", SPRITE_DIR, JO_NULL);
            if (sheet_data != JO_NULL)
            {
                if (jo_tga_loader_from_stream(&amy_defeated_sheet, sheet_data, JO_COLOR_Green) == JO_TGA_OK)
                    amy_defeated_sheet_ready = true;
                jo_free(sheet_data);
            }
        }

        // Create a single VRAM sprite that will be updated each frame.
        if (amy_sprite_id < 0)
        {
            jo_img blank;
            blank.width = CHARACTER_WIDTH;
            blank.height = CHARACTER_HEIGHT;
            blank.data = (unsigned short *)jo_malloc((size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT * sizeof(unsigned short));
            if (blank.data != JO_NULL)
            {
                for (size_t i = 0; i < (size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT; ++i)
                    ((unsigned short *)blank.data)[i] = JO_COLOR_Transparent;

                amy_sprite_id = jo_sprite_add(&blank);
                jo_free_img(&blank);
            }
        }

        if (amy_sprite_id >= 0)
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
    character_ref.spin_sprite_id = amy_spin_sprite_id;
    character_ref.jump_sprite_id = amy_jump_sprite_id;
    character_ref.damage_sprite_id = amy_damage_sprite_id;
    character_ref.defeated_sprite_id = amy_defeated_sprite_id;
    character_ref.punch_anim_id = amy_punch_anim_id;
    character_ref.kick_anim_id = amy_kick_anim_id;
    character_registry_apply_combat_profile(&character_ref, UiCharacterAmy);
    amy_physics = &physics; // ensure we always have a valid physics context
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
        jo_free_img(&amy_sheet);
        amy_sheet_ready = false;
    }

    if (amy_defeated_sheet_ready)
    {
        jo_free_img(&amy_defeated_sheet);
        amy_defeated_sheet_ready = false;
    }

    amy_stand_anim_id = -1;
    amy_walking_anim_id = -1;
    amy_running1_anim_id = -1;
    amy_running2_anim_id = -1;
    amy_punch_anim_id = -1;
    amy_kick_anim_id = -1;

    amy_sprite_id = -1;
    character_ref.wram_sprite_id = -1;
    amy_loaded = false;

    amy_spin_sprite_id = -1;
    amy_jump_sprite_id = -1;
    amy_damage_sprite_id = -1;
    amy_defeated_sprite_id = -1;

    amy_walking_base_id = -1;
    amy_running1_base_id = -1;
    amy_running2_base_id = -1;
    amy_stand_base_id = -1;
    amy_punch_base_id = -1;
    amy_kick_base_id = -1;
}
