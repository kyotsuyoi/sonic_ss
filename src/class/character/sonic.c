#include <jo/jo.h>
#include <stddef.h>
#include <string.h>
#include "sonic.h"
#include "player.h"
#include "character_registry.h"

#define character_ref player
extern jo_sidescroller_physics_params physics;
#define SPRITE_DIR "SPT"
#define DEFEATED_SPRITE_WIDTH 40
#define DEFEATED_SPRITE_HEIGHT 32

static bool sonic_loaded = false;
static bool sonic_sheet_ready = false;
static jo_img sonic_sheet = {0};
static int sonic_sprite_id = -1;

typedef enum
{
    SONIC_ANIM_NONE = -1,
    SONIC_ANIM_STAND,
    SONIC_ANIM_WALK,
    SONIC_ANIM_RUN1,
    SONIC_ANIM_RUN2,
    SONIC_ANIM_PUNCH,
    SONIC_ANIM_KICK,
    SONIC_ANIM_JUMP,
    SONIC_ANIM_DAMAGE,
    SONIC_ANIM_SPIN,
} sonic_anim_t;

static sonic_anim_t sonic_current_anim = SONIC_ANIM_NONE;
static int sonic_current_frame = 0;
static int sonic_current_frame_ticks = 0;
static int sonic_current_frame_duration = DEFAULT_SPRITE_FRAME_DURATION;
static int sonic_current_frame_count = 1;

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
    {0, 0, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void sonic_reset_animation_lists_except(int active_anim_id)
{
    if (sonic_stand_anim_id >= 0 && sonic_stand_anim_id != active_anim_id)
        jo_reset_sprite_anim(sonic_stand_anim_id);
    if (sonic_walking_anim_id >= 0 && sonic_walking_anim_id != active_anim_id)
        jo_reset_sprite_anim(sonic_walking_anim_id);
    if (sonic_running1_anim_id >= 0 && sonic_running1_anim_id != active_anim_id)
        jo_reset_sprite_anim(sonic_running1_anim_id);
    if (sonic_running2_anim_id >= 0 && sonic_running2_anim_id != active_anim_id)
        jo_reset_sprite_anim(sonic_running2_anim_id);
    if (sonic_punch_anim_id >= 0 && sonic_punch_anim_id != active_anim_id)
        jo_reset_sprite_anim(sonic_punch_anim_id);
    if (sonic_kick_anim_id >= 0 && sonic_kick_anim_id != active_anim_id)
        jo_reset_sprite_anim(sonic_kick_anim_id);
}

static void sonic_copy_sheet_frame_to_sprite(int frame_x, int frame_y)
{
    if (!sonic_sheet_ready || sonic_sprite_id < 0)
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
    unsigned short *src = (unsigned short *)sonic_sheet.data;
    int sheet_width = sonic_sheet.width;

    for (int y = 0; y < CHARACTER_HEIGHT; ++y)
    {
        unsigned short *src_row = src + (frame_y + y) * sheet_width + frame_x;
        unsigned short *dst_row = dst + y * CHARACTER_WIDTH;
        memcpy(dst_row, src_row, CHARACTER_WIDTH * sizeof(unsigned short));
    }

    jo_sprite_replace(&tmp, sonic_sprite_id);
}

static int sonic_create_blank_animation(int frame_count)
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

static void sonic_render_current_frame(void)
{
    if (!sonic_sheet_ready || sonic_sprite_id < 0)
        return;

    int row = 0;
    int col = 0;

    switch (sonic_current_anim)
    {
        case SONIC_ANIM_STAND:
            row = 0;
            col = jo_get_sprite_anim_frame(sonic_stand_anim_id);
            break;
        case SONIC_ANIM_WALK:
            row = 1;
            col = jo_get_sprite_anim_frame(sonic_walking_anim_id);
            break;
        case SONIC_ANIM_RUN1:
            row = 2;
            col = jo_get_sprite_anim_frame(sonic_running1_anim_id);
            break;
        case SONIC_ANIM_RUN2:
            row = 3;
            col = jo_get_sprite_anim_frame(sonic_running2_anim_id);
            break;
        case SONIC_ANIM_PUNCH:
            row = 4;
            col = jo_get_sprite_anim_frame(sonic_punch_anim_id);
            break;
        case SONIC_ANIM_KICK:
            row = 5;
            col = jo_get_sprite_anim_frame(sonic_kick_anim_id);
            break;
        case SONIC_ANIM_JUMP:
            row = 0;
            col = 4;
            break;
        case SONIC_ANIM_DAMAGE:
            row = 0;
            col = 5;
            break;
        case SONIC_ANIM_SPIN:
            row = 0;
            col = 6;
            break;
        default:
            return;
    }

    sonic_copy_sheet_frame_to_sprite(col * CHARACTER_WIDTH, row * CHARACTER_HEIGHT);
}

static void sonic_set_current_anim(sonic_anim_t anim, int frame_count, int frame_duration)
{
    if (anim != sonic_current_anim)
    {
        sonic_current_anim = anim;
        sonic_current_frame = 0;
        sonic_current_frame_ticks = 0;
        sonic_current_frame_duration = frame_duration;
        sonic_current_frame_count = frame_count;
    }
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
inline void sonic_running_animation_handling(void)
{
    int speed_step;

    if (jo_physics_is_standing(&physics))
    {
        // Standing animation
        sonic_set_current_anim(SONIC_ANIM_STAND, JO_TILE_COUNT(SonicStandTiles), DEFAULT_SPRITE_FRAME_DURATION);

        jo_reset_sprite_anim(character_ref.running1_anim_id);
        jo_reset_sprite_anim(character_ref.running2_anim_id);

        if (!jo_is_sprite_anim_stopped(character_ref.walking_anim_id))
        {
            jo_reset_sprite_anim(character_ref.walking_anim_id);
            jo_reset_sprite_anim(character_ref.stand_sprite_id);
        }
        else
        {
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
        // Running/walking animation selection
        if (character_ref.run == 0)
        {
            sonic_set_current_anim(SONIC_ANIM_WALK, JO_TILE_COUNT(SonicWalkingTiles), DEFAULT_SPRITE_FRAME_DURATION);

            jo_reset_sprite_anim(character_ref.running1_anim_id);
            jo_reset_sprite_anim(character_ref.running2_anim_id);

            if (jo_is_sprite_anim_stopped(character_ref.walking_anim_id))
                jo_start_sprite_anim_loop(character_ref.walking_anim_id);
        }
        else if (character_ref.run == 1)
        {
            sonic_set_current_anim(SONIC_ANIM_RUN1, JO_TILE_COUNT(SonicRunning1Tiles), DEFAULT_SPRITE_FRAME_DURATION);

            jo_reset_sprite_anim(character_ref.walking_anim_id);
            jo_reset_sprite_anim(character_ref.running2_anim_id);

            if (jo_is_sprite_anim_stopped(character_ref.running1_anim_id))
                jo_start_sprite_anim_loop(character_ref.running1_anim_id);
        }
        else if (character_ref.run == 2)
        {
            sonic_set_current_anim(SONIC_ANIM_RUN2, JO_TILE_COUNT(SonicRunning2Tiles), DEFAULT_SPRITE_FRAME_DURATION);

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

inline void display_sonic(void)
{
    if (!physics.is_in_air)
    {
        character_ref.spin = false;
        character_ref.jump = false;
        character_ref.angle = 0;
    }

    if (character_ref.flip)
        jo_sprite_enable_horizontal_flip();

    if (character_ref.spin)
    {
        sonic_reset_animation_lists_except(-1);
        sonic_set_current_anim(SONIC_ANIM_SPIN, 1, DEFAULT_SPRITE_FRAME_DURATION);
        sonic_render_current_frame();
        jo_sprite_draw3D_and_rotate2(sonic_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z, character_ref.angle);
        if (character_ref.flip)
            character_ref.angle -= CHARACTER_SPIN_SPEED;
        else
            character_ref.angle += CHARACTER_SPIN_SPEED;
    }
    else if (character_ref.life <= 0 && sonic_defeated_sprite_id >= 0)
    {
        sonic_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(sonic_defeated_sprite_id, character_ref.x, character_ref.y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
    }
    else if (character_ref.stun_timer > 0 && sonic_sheet_ready)
    {
        sonic_reset_animation_lists_except(-1);
        sonic_set_current_anim(SONIC_ANIM_DAMAGE, 1, DEFAULT_SPRITE_FRAME_DURATION);
        sonic_render_current_frame();
        jo_sprite_draw3D2(sonic_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    }
    else if (character_ref.punch || character_ref.punch2)
    {
        sonic_reset_animation_lists_except(character_ref.punch_anim_id);
        sonic_set_current_anim(SONIC_ANIM_PUNCH, JO_TILE_COUNT(SonicPunchTiles), DEFAULT_SPRITE_FRAME_DURATION);
        sonic_render_current_frame();
        jo_sprite_draw3D2(sonic_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    }
    else if (character_ref.kick || character_ref.kick2)
    {
        sonic_reset_animation_lists_except(character_ref.kick_anim_id);
        sonic_set_current_anim(SONIC_ANIM_KICK, JO_TILE_COUNT(SonicKickTiles), DEFAULT_SPRITE_FRAME_DURATION);
        sonic_render_current_frame();
        jo_sprite_draw3D2(sonic_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    }
    else if (character_ref.jump)
    {
        sonic_reset_animation_lists_except(-1);
        sonic_set_current_anim(SONIC_ANIM_JUMP, 1, DEFAULT_SPRITE_FRAME_DURATION);
        sonic_render_current_frame();
        jo_sprite_draw3D2(sonic_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    }
    else
    {
        if (character_ref.walk && character_ref.run == 0)
        {
            sonic_reset_animation_lists_except(character_ref.walking_anim_id);
            sonic_set_current_anim(SONIC_ANIM_WALK, JO_TILE_COUNT(SonicWalkingTiles), DEFAULT_SPRITE_FRAME_DURATION);
            sonic_render_current_frame();
            jo_sprite_draw3D2(sonic_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 1)
        {
            sonic_reset_animation_lists_except(character_ref.running1_anim_id);
            sonic_set_current_anim(SONIC_ANIM_RUN1, JO_TILE_COUNT(SonicRunning1Tiles), DEFAULT_SPRITE_FRAME_DURATION);
            sonic_render_current_frame();
            jo_sprite_draw3D2(sonic_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 2)
        {
            sonic_reset_animation_lists_except(character_ref.running2_anim_id);
            sonic_set_current_anim(SONIC_ANIM_RUN2, JO_TILE_COUNT(SonicRunning2Tiles), DEFAULT_SPRITE_FRAME_DURATION);
            sonic_render_current_frame();
            jo_sprite_draw3D2(sonic_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else
        {
            sonic_reset_animation_lists_except(character_ref.stand_sprite_id);
            sonic_set_current_anim(SONIC_ANIM_STAND, JO_TILE_COUNT(SonicStandTiles), DEFAULT_SPRITE_FRAME_DURATION);
            sonic_render_current_frame();
            jo_sprite_draw3D2(sonic_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);        
        }
    }

    if (character_ref.flip)
        jo_sprite_disable_horizontal_flip();

    // Barra de vidas textual
    int life_percent = (character_ref.life * 100) / 50;
    int bar_max_width = 20; // 20 caracteres
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
        {
            char *sheet_data = jo_fs_read_file_in_dir("SNC_FUL.TGA", SPRITE_DIR, JO_NULL);
            if (sheet_data != JO_NULL)
            {
                if (jo_tga_loader_from_stream(&sonic_sheet, sheet_data, JO_COLOR_Green) == JO_TGA_OK)
                    sonic_sheet_ready = true;
                jo_free(sheet_data);
            }
        }

        // Create a single VRAM sprite that will be updated each frame.
        if (sonic_sprite_id < 0)
        {
            jo_img blank;
            blank.width = CHARACTER_WIDTH;
            blank.height = CHARACTER_HEIGHT;
            blank.data = (unsigned short *)jo_malloc((size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT * sizeof(unsigned short));
            if (blank.data != JO_NULL)
            {
                for (size_t i = 0; i < (size_t)CHARACTER_WIDTH * (size_t)CHARACTER_HEIGHT; ++i)
                    ((unsigned short *)blank.data)[i] = JO_COLOR_Transparent;

                sonic_sprite_id = jo_sprite_add(&blank);
                jo_free_img(&blank);
            }
        }

        // Dummy animation objects (used only for timing / combo logic).
        sonic_stand_anim_id = jo_create_sprite_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicStandTiles)), JO_TILE_COUNT(SonicStandTiles), DEFAULT_SPRITE_FRAME_DURATION);
        sonic_walking_anim_id = jo_create_sprite_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicWalkingTiles)), JO_TILE_COUNT(SonicWalkingTiles), DEFAULT_SPRITE_FRAME_DURATION);
        sonic_running1_anim_id = jo_create_sprite_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicRunning1Tiles)), JO_TILE_COUNT(SonicRunning1Tiles), DEFAULT_SPRITE_FRAME_DURATION);
        sonic_running2_anim_id = jo_create_sprite_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicRunning2Tiles)), JO_TILE_COUNT(SonicRunning2Tiles), DEFAULT_SPRITE_FRAME_DURATION);
        sonic_punch_anim_id = jo_create_sprite_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicPunchTiles)), JO_TILE_COUNT(SonicPunchTiles), DEFAULT_SPRITE_FRAME_DURATION);
        sonic_kick_anim_id = jo_create_sprite_anim(sonic_create_blank_animation(JO_TILE_COUNT(SonicKickTiles)), JO_TILE_COUNT(SonicKickTiles), DEFAULT_SPRITE_FRAME_DURATION);

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
        jo_free_img(&sonic_sheet);
        sonic_sheet_ready = false;
    }

    sonic_stand_anim_id = -1;
    sonic_walking_anim_id = -1;
    sonic_running1_anim_id = -1;
    sonic_running2_anim_id = -1;
    sonic_punch_anim_id = -1;
    sonic_kick_anim_id = -1;
    sonic_sprite_id = -1;
    sonic_defeated_sprite_id = -1;
    sonic_current_anim = SONIC_ANIM_NONE;

    sonic_loaded = false;
}
