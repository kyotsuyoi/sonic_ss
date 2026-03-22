#include <jo/jo.h>
#include <string.h>
#include "sonic.h"
#include "player.h"
#include "character_common.h"
#include "character_registry.h"
#include "input_mapping.h"
#include "sprite_safe.h"
#include "runtime_log.h"
#include "ram_cart.h"

extern jo_sidescroller_physics_params physics; // global physics state (from main.c)

static character_t *sonic_ref = &player;
static jo_sidescroller_physics_params *sonic_physics = &physics;

#define character_ref (*sonic_ref)
#define physics (*sonic_physics)

#define SPRITE_DIR "SPT"

static bool sonic_loaded = false;
static bool sonic_sheet_ready = false;
static bool sonic_use_cart_ram = false;
static jo_img sonic_sheet_img = {0};
static int sonic_sheet_width = 0;
static int sonic_sheet_height = 0;
static int sonic_sprite_id = -1;
static int sonic_punch_sprite_id = -1;
static int sonic_damaged_sprite_id = -1;
static int sonic_defeated_sprite_id = -1;

#define SONIC_IDLE_FRAMES 6
#define SONIC_WALK_FRAMES 8
#define SONIC_RUN_FRAMES 6
#define SONIC_JUMP_FRAMES 5
#define SONIC_FALL_FRAMES 4
#define SONIC_LAND_FRAMES 3
#define SONIC_PUNCH_FRAMES 6
#define SONIC_DAMAGED_FRAMES 3
typedef enum
{
    SonicAnimIdle = 0,
    SonicAnimWalk,
    SonicAnimRun,
    SonicAnimJump,
    SonicAnimFall,
    SonicAnimLand,
    SonicAnimPunch,
    SonicAnimDamaged
} sonic_anim_mode_t;

static const jo_tile SonicDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static bool character_has_movement_input_for_character(const character_t *chr)
{
    if (chr == JO_NULL)
        return false;

    return chr->walk || chr->spin || chr->punch || chr->punch2 || chr->kick || chr->kick2;
}

static void sonic_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y, int frame_width, int target_width)
{
    // NOTE: temporary path supports both Cart RAM and WRAM sprite source.
    // This should be migrated to Cart RAM only in the future so we can support
    // larger character sprite sets and reduce WRAM usage for additional characters.
    if (target_width == frame_width)
    {
        if (sonic_use_cart_ram)
        {
            character_copy_cart_sheet_frame_to_sprite(sprite_id, "SNC_FUL", sonic_sheet_width, sonic_sheet_height, frame_x, frame_y, frame_width, CHARACTER_HEIGHT);
            return;
        }

        if (sonic_sheet_img.data != JO_NULL)
        {
            character_copy_sheet_frame_to_sprite_with_size(sprite_id, &sonic_sheet_img, frame_x, frame_y, frame_width, CHARACTER_HEIGHT);
            return;
        }
    }

    // For punch frames that use a target width larger than frame_width (e.g. 48), use padding.
    if (sonic_sheet_img.data != JO_NULL)
    {
        jo_img tmp = {0};
        tmp.width = target_width;
        tmp.height = CHARACTER_HEIGHT;
        tmp.data = jo_malloc((size_t)target_width * (size_t)CHARACTER_HEIGHT * sizeof(unsigned short));
        if (tmp.data != JO_NULL)
        {
            unsigned short *dst = (unsigned short *)tmp.data;
            unsigned short *src = (unsigned short *)sonic_sheet_img.data;
            size_t pixel_count = (size_t)target_width * (size_t)CHARACTER_HEIGHT;

            for (size_t i = 0; i < pixel_count; ++i)
                dst[i] = JO_COLOR_Transparent;

            for (int y = 0; y < CHARACTER_HEIGHT; ++y)
            {
                unsigned short *src_row = src + (frame_y + y) * sonic_sheet_img.width + frame_x;
                unsigned short *dst_row = dst + (size_t)y * target_width;
                memcpy(dst_row, src_row, (size_t)frame_width * sizeof(unsigned short));
            }

            jo_sprite_replace(&tmp, sprite_id);
            jo_free_img(&tmp);
        }
        return;
    }

    // Fallback to cart path for exact width.
    if (sonic_use_cart_ram)
    {
        character_copy_cart_sheet_frame_to_sprite(sprite_id, "SNC_FUL", sonic_sheet_width, sonic_sheet_height, frame_x, frame_y, frame_width, CHARACTER_HEIGHT);
    }
}

static int sonic_ensure_wram_sprite(character_t *chr)
{
    return character_ensure_wram_sprite(chr, &sonic_sprite_id);
}

static int sonic_ensure_punch_wram_sprite(character_t *chr)
{
    if (sonic_punch_sprite_id < 0)
        sonic_punch_sprite_id = character_create_blank_sprite_with_size(48, CHARACTER_HEIGHT);
    return sonic_punch_sprite_id;
}

static int sonic_ensure_damaged_wram_sprite(character_t *chr)
{
    if (sonic_damaged_sprite_id < 0)
        sonic_damaged_sprite_id = character_create_blank_sprite_with_size(CHARACTER_WIDTH, CHARACTER_HEIGHT);
    return sonic_damaged_sprite_id;
}

static void sonic_update_animation_frame(character_t *chr, int frame_count, bool hold_last)
{
    if (chr == JO_NULL)
        return;

    if (chr->sonic_anim_frame >= frame_count)
        chr->sonic_anim_frame = frame_count - 1;

    chr->sonic_anim_ticks++;
    if (chr->sonic_anim_ticks < DEFAULT_SPRITE_FRAME_DURATION)
        return;

    chr->sonic_anim_ticks = 0;

    if (hold_last && chr->sonic_anim_frame >= frame_count - 1)
        return;

    chr->sonic_anim_frame++;

    if (chr->sonic_anim_frame >= frame_count)
        chr->sonic_anim_frame = 0;
}

static int sonic_calc_frame(character_t *chr, int mode)
{
    int frame_count;
    bool hold_last = false;

    switch (mode)
    {
        case SonicAnimIdle:
            frame_count = SONIC_IDLE_FRAMES;
            break;
        case SonicAnimWalk:
            frame_count = SONIC_WALK_FRAMES;
            break;
        case SonicAnimRun:
            frame_count = SONIC_RUN_FRAMES;
            break;
        case SonicAnimJump:
            frame_count = SONIC_JUMP_FRAMES;
            hold_last = true;
            break;
        case SonicAnimFall:
            frame_count = SONIC_FALL_FRAMES;
            hold_last = true;
            break;
        case SonicAnimLand:
            frame_count = SONIC_LAND_FRAMES;
            hold_last = true;
            break;
        case SonicAnimPunch:
            frame_count = SONIC_PUNCH_FRAMES;
            hold_last = true; // stop on last frame until punch flag is cleared
            break;
        case SonicAnimDamaged:
            frame_count = SONIC_DAMAGED_FRAMES;
            break;
        default:
            frame_count = SONIC_IDLE_FRAMES;
            break;
    }

    if (chr->sonic_anim_mode != mode)
    {
        chr->sonic_anim_mode = mode;
        chr->sonic_anim_frame = 0;
        chr->sonic_anim_ticks = 0;
    }

    sonic_update_animation_frame(chr, frame_count, hold_last);
    return chr->sonic_anim_frame;
}

static bool sonic_has_movement_input(const character_t *chr)
{
    if (chr == JO_NULL)
        return false;

    return chr->walk || chr->spin || chr->punch || chr->punch2 || chr->kick || chr->kick2;
}

static void sonic_render_current_frame_for(character_t *chr, int sprite_id)
{
    if (!sonic_sheet_ready || sprite_id < 0)
        return;

    if (chr->life <= 0)
        return;

    int mode = chr->sonic_anim_mode;
    if (mode < SonicAnimIdle || mode > SonicAnimDamaged)
        mode = SonicAnimIdle;

    int frame = chr->sonic_anim_frame;
    int frame_x = 0;
    int frame_y = 0;

    int frame_width = CHARACTER_WIDTH;

    switch (mode)
    {
        case SonicAnimIdle:
            frame_x = frame * CHARACTER_WIDTH;
            frame_y = 0;
            frame_width = CHARACTER_WIDTH;
            break;
        case SonicAnimWalk:
            frame_x = frame * CHARACTER_WIDTH;
            frame_y = CHARACTER_HEIGHT;
            frame_width = CHARACTER_WIDTH;
            break;
        case SonicAnimRun:
            frame_x = frame * CHARACTER_WIDTH;
            frame_y = CHARACTER_HEIGHT * 2;
            frame_width = CHARACTER_WIDTH;
            break;
        case SonicAnimJump:
            frame_x = frame * CHARACTER_WIDTH;
            frame_y = CHARACTER_HEIGHT * 3;
            frame_width = CHARACTER_WIDTH;
            break;
        case SonicAnimFall:
            frame_x = frame * CHARACTER_WIDTH;
            frame_y = CHARACTER_HEIGHT * 4;
            frame_width = CHARACTER_WIDTH;
            break;
        case SonicAnimLand:
            frame_x = (4 + frame) * CHARACTER_WIDTH;
            frame_y = CHARACTER_HEIGHT * 4;
            frame_width = CHARACTER_WIDTH;
            if (chr->landed && sonic_has_movement_input(chr))
            {
                chr->landed = false;
                chr->sonic_anim_mode = SonicAnimIdle;
            }
            break;
        case SonicAnimPunch:
            frame_y = CHARACTER_HEIGHT * 6;
            switch (frame)
            {
                case 0:
                    frame_x = 0;
                    frame_width = 32;
                    break;
                case 1:
                    frame_x = 32;
                    frame_width = 32;
                    break;
                case 2:
                    frame_x = 64;
                    frame_width = 48;
                    break;
                case 3:
                    frame_x = 112;
                    frame_width = 48;
                    break;
                case 4:
                    frame_x = 160;
                    frame_width = 32;
                    break;
                case 5:
                default:
                    frame_x = 192;
                    frame_width = 32;
                    break;
            }
            break;
        case SonicAnimDamaged:
            frame_y = CHARACTER_HEIGHT * 5;
            frame_x = (3 + frame) * CHARACTER_WIDTH; // 3,4,5 columns
            frame_width = CHARACTER_WIDTH;
            if (chr->stun_timer <= 0)
            {
                chr->sonic_anim_mode = SonicAnimIdle;
                chr->sonic_anim_frame = 0;
                chr->sonic_anim_ticks = 0;
            }
            break;
        default:
            frame_x = 0;
            frame_y = 0;
            frame_width = CHARACTER_WIDTH;
            break;
    }

    int target_width = CHARACTER_WIDTH;
    if (mode == SonicAnimPunch && (frame == 2 || frame == 3))
        target_width = 48;

    sonic_copy_sheet_frame_to_sprite(sprite_id, frame_x, frame_y, frame_width, target_width);
}

#define SONIC_LONG_FALL_MS 1000

void sonic_running_animation_handling(void)
{
    character_t *chr = &character_ref;

    if (chr->life <= 0)
        return;

    if (chr->stun_timer > 0)
    {
        if (chr->sonic_anim_mode != SonicAnimDamaged)
        {
            chr->sonic_anim_mode = SonicAnimDamaged;
            chr->sonic_anim_frame = 0;
            chr->sonic_anim_ticks = 0;
        }
        return;
    }

    if (chr->punch || chr->punch2)
    {
        if (chr->sonic_anim_mode != SonicAnimPunch)
        {
            chr->sonic_anim_mode = SonicAnimPunch;
            chr->sonic_anim_frame = 0;
            chr->sonic_anim_ticks = 0;
        }

        if (chr->sonic_anim_frame >= SONIC_PUNCH_FRAMES - 1)
        {
            chr->punch = false;
            chr->punch2 = false;
            chr->punch2_requested = false;
            chr->perform_punch2 = false;
            chr->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
            chr->sonic_anim_mode = SonicAnimIdle;
            chr->sonic_anim_frame = 0;
            chr->sonic_anim_ticks = 0;
        }
        return;
    }

    if (physics.is_in_air)
    {
        chr->sonic_land_pending = false;

        if (physics.speed_y < 0.0f)
        {
            if (chr->sonic_anim_mode != SonicAnimJump)
            {
                chr->sonic_anim_mode = SonicAnimJump;
                chr->sonic_anim_frame = 0;
                chr->sonic_anim_ticks = 0;
            }
            chr->sonic_fall_time_ms = 0;
            return;
        }

        if (chr->sonic_anim_mode != SonicAnimFall)
        {
            chr->sonic_anim_mode = SonicAnimFall;
            chr->sonic_anim_frame = 0;
            chr->sonic_anim_ticks = 0;
        }

        chr->sonic_fall_time_ms += GAME_FRAME_MS;
        return;
    }

    if (chr->sonic_land_pending)
    {
        if (character_has_movement_input_for_character(chr))
        {
            chr->sonic_land_pending = false;
        }
        else
        {
            if (chr->sonic_anim_mode != SonicAnimLand)
            {
                chr->sonic_anim_mode = SonicAnimLand;
                chr->sonic_anim_frame = 0;
                chr->sonic_anim_ticks = 0;
            }
            return;
        }
    }

    if (chr->sonic_fall_time_ms >= SONIC_LONG_FALL_MS)
    {
        chr->sonic_land_pending = true;
        chr->sonic_fall_time_ms = 0;
        chr->sonic_anim_mode = SonicAnimLand;
        chr->sonic_anim_frame = 0;
        chr->sonic_anim_ticks = 0;
        return;
    }

    chr->sonic_fall_time_ms = 0;

    if (chr->walk && chr->run == 2)
        chr->sonic_anim_mode = SonicAnimRun;
    else if (chr->walk)
        chr->sonic_anim_mode = SonicAnimWalk;
    else
        chr->sonic_anim_mode = SonicAnimIdle;
}

static void sonic_draw_for_character(character_t *chr)
{
    if (!sonic_sheet_ready)
    {
        if (chr->defeated_sprite_id >= 0 && chr->life <= 0)
        {
            jo_sprite_draw3D2(chr->defeated_sprite_id,
                              chr->x,
                              chr->y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT),
                              CHARACTER_SPRITE_Z);
        }
        return;
    }

    if (chr->life <= 0)
    {
        if (chr->defeated_wram_sprite_id >= 0)
            jo_sprite_draw3D2(chr->defeated_wram_sprite_id,
                              chr->x,
                              chr->y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT),
                              CHARACTER_SPRITE_Z);
        return;
    }

    int mode = chr->sonic_anim_mode;
    if (mode < SonicAnimIdle || mode > SonicAnimDamaged)
        mode = SonicAnimIdle;

    sonic_calc_frame(chr, mode);

    int sprite_id;
    if (mode == SonicAnimPunch &&
        (chr->sonic_anim_frame == 2 || chr->sonic_anim_frame == 3))
    {
        sprite_id = sonic_ensure_punch_wram_sprite(chr);
    }
    else
    {
        sprite_id = sonic_ensure_wram_sprite(chr);
    }

    if (sprite_id < 0)
        return;

    sonic_render_current_frame_for(chr, sprite_id);

    int draw_x = chr->x;
    if (chr->flip && chr->sonic_anim_mode == SonicAnimPunch &&
        (chr->sonic_anim_frame == 2 || chr->sonic_anim_frame == 3))
    {
        draw_x -= 16;
    }

    jo_sprite_draw3D2(sprite_id, draw_x, chr->y, CHARACTER_SPRITE_Z);
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

    sonic_draw_for_character(&character_ref);

    if (character_ref.flip)
        jo_sprite_disable_horizontal_flip();
}

void sonic_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    if (chr != JO_NULL)
        sonic_ref = chr;
    if (phy != JO_NULL)
        sonic_physics = phy;
}

void load_sonic(void)
{
    if (!sonic_loaded)
    {
        if (!sonic_sheet_ready)
        {
            jo_img sheet = {0};
            if (character_load_sheet(&sheet, "SNC_FUL.TGA", SPRITE_DIR, JO_COLOR_Green))
            {
                sonic_sheet_width = sheet.width;
                sonic_sheet_height = sheet.height;
                if (ram_cart_store_sprite("SNC_FUL", sheet.data, (size_t)sheet.width * (size_t)sheet.height * sizeof(unsigned short)))
                {
                    sonic_sheet_ready = true;
                    sonic_use_cart_ram = true;
                    sonic_sheet_img = sheet; // keep sheet loaded for frame padding operations
                }
                else
                {
                    sonic_sheet_ready = true;
                    sonic_use_cart_ram = false;
                    sonic_sheet_img = sheet;
                }
            }
        }

        if (sonic_sprite_id < 0)
            sonic_sprite_id = character_create_blank_sprite();

        if (sonic_punch_sprite_id < 0)
            sonic_punch_sprite_id = character_create_blank_sprite_with_size(48, CHARACTER_HEIGHT);

        if (sonic_damaged_sprite_id < 0)
            sonic_damaged_sprite_id = character_create_blank_sprite();

        sonic_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "SNC_DFT.TGA", JO_COLOR_Green, SonicDefeatedTile, JO_TILE_COUNT(SonicDefeatedTile));

        sonic_loaded = true;
    }

    character_ref.wram_sprite_id = sonic_sprite_id;
    character_ref.damage_sprite_id = sonic_damaged_sprite_id;
    character_ref.defeated_sprite_id = sonic_defeated_sprite_id;
    character_ref.defeated_wram_sprite_id = -1;

    character_registry_apply_combat_profile(&character_ref, UiCharacterSonic);

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
    character_ref.sonic_anim_mode = SonicAnimIdle;
    character_ref.sonic_anim_frame = 0;
    character_ref.sonic_anim_ticks = 0;
    character_ref.sonic_fall_time_ms = 0;
    character_ref.sonic_land_pending = false;
    character_ref.life = 50;
}

void unload_sonic(void)
{
    if (!sonic_loaded)
        return;

    if (sonic_sheet_ready)
    {
        if (sonic_use_cart_ram)
        {
            ram_cart_delete_sprite("SNC_FUL");
        }
        else if (sonic_sheet_img.data != JO_NULL)
        {
            jo_free_img(&sonic_sheet_img);
        }

        sonic_sheet_ready = false;
        sonic_use_cart_ram = false;
        sonic_sheet_width = 0;
        sonic_sheet_height = 0;
    }

    sonic_defeated_sprite_id = -1;
    sonic_damaged_sprite_id = -1;
    sonic_sprite_id = -1;
    character_ref.wram_sprite_id = -1;
    sonic_loaded = false;
}
