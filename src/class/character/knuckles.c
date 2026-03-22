#include <jo/jo.h>
#include <string.h>
#include "knuckles.h"
#include "player.h"
#include "character_common.h"
#include "character_registry.h"
#include "input_mapping.h"
#include "sprite_safe.h"
#include "runtime_log.h"
#include "ram_cart.h"

extern jo_sidescroller_physics_params physics; // global physics state (from main.c)

static character_t *knuckles_ref = &player;
static jo_sidescroller_physics_params *knuckles_physics = &physics;

#define character_ref (*knuckles_ref)
#define physics (*knuckles_physics)

#define SPRITE_DIR "SPT"

static bool knuckles_loaded = false;
static bool knuckles_sheet_ready = false;
static bool knuckles_use_cart_ram = false;
static jo_img knuckles_sheet_img = {0};
static int knuckles_sheet_width = 0;
static int knuckles_sheet_height = 0;
static int knuckles_sprite_id = -1;
static int knuckles_punch_sprite_id = -1;
static int knuckles_damaged_sprite_id = -1;
static int knuckles_defeated_sprite_id = -1;

#define KNUCKLES_IDLE_FRAMES 8
#define KNUCKLES_WALK_FRAMES 8
#define KNUCKLES_RUN_FRAMES 6
#define KNUCKLES_JUMP_FRAMES 5
#define KNUCKLES_FALL_FRAMES 4
#define KNUCKLES_LAND_FRAMES 3
#define KNUCKLES_PUNCH_FRAMES 6
#define KNUCKLES_DAMAGED_FRAMES 3

typedef enum
{
    KnucklesAnimIdle = 0,
    KnucklesAnimWalk,
    KnucklesAnimRun,
    KnucklesAnimJump,
    KnucklesAnimFall,
    KnucklesAnimLand,
    KnucklesAnimPunch,
    KnucklesAnimDamaged
} knuckles_anim_mode_t;

static const jo_tile KnucklesDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static bool character_has_movement_input_for_character(const character_t *chr)
{
    if (chr == JO_NULL)
        return false;

    return chr->walk || chr->spin || chr->punch || chr->punch2 || chr->kick || chr->kick2;
}

static void knuckles_copy_sheet_frame_to_sprite(int sprite_id, int frame_x, int frame_y, int frame_width, int target_width)
{
    // NOTE: temporary path supports both Cart RAM and WRAM sprite source.
    // This should be migrated to Cart RAM only in the future so we can support
    // larger character sprite sets and reduce WRAM usage for additional characters.
    if (target_width == frame_width)
    {
        if (knuckles_use_cart_ram)
        {
            character_copy_cart_sheet_frame_to_sprite(sprite_id, "KNK_FUL", knuckles_sheet_width, knuckles_sheet_height, frame_x, frame_y, frame_width, CHARACTER_HEIGHT);
            return;
        }

        if (knuckles_sheet_img.data != JO_NULL)
        {
            character_copy_sheet_frame_to_sprite_with_size(sprite_id, &knuckles_sheet_img, frame_x, frame_y, frame_width, CHARACTER_HEIGHT);
            return;
        }
    }

    // For punch frames that use a target width larger than frame_width (e.g. 48), use padding.
    if (knuckles_sheet_img.data != JO_NULL)
    {
        jo_img tmp = {0};
        tmp.width = target_width;
        tmp.height = CHARACTER_HEIGHT;
        tmp.data = jo_malloc((size_t)target_width * (size_t)CHARACTER_HEIGHT * sizeof(unsigned short));
        if (tmp.data != JO_NULL)
        {
            unsigned short *dst = (unsigned short *)tmp.data;
            unsigned short *src = (unsigned short *)knuckles_sheet_img.data;
            size_t pixel_count = (size_t)target_width * (size_t)CHARACTER_HEIGHT;

            for (size_t i = 0; i < pixel_count; ++i)
                dst[i] = JO_COLOR_Transparent;

            for (int y = 0; y < CHARACTER_HEIGHT; ++y)
            {
                unsigned short *src_row = src + (frame_y + y) * knuckles_sheet_img.width + frame_x;
                unsigned short *dst_row = dst + (size_t)y * target_width;
                memcpy(dst_row, src_row, (size_t)frame_width * sizeof(unsigned short));
            }

            jo_sprite_replace(&tmp, sprite_id);
            jo_free_img(&tmp);
        }
        return;
    }

    // Fallback to cart path for exact width.
    if (knuckles_use_cart_ram)
    {
        character_copy_cart_sheet_frame_to_sprite(sprite_id, "KNK_FUL", knuckles_sheet_width, knuckles_sheet_height, frame_x, frame_y, frame_width, CHARACTER_HEIGHT);
    }
}

static void knuckles_update_animation_frame(character_t *chr, int frame_count, bool hold_last)
{
    if (chr == JO_NULL)
        return;

    if (chr->knuckles_anim_frame >= frame_count)
        chr->knuckles_anim_frame = frame_count - 1;

    chr->knuckles_anim_ticks++;
    if (chr->knuckles_anim_ticks < DEFAULT_SPRITE_FRAME_DURATION)
        return;

    chr->knuckles_anim_ticks = 0;

    if (hold_last && chr->knuckles_anim_frame >= frame_count - 1)
        return;

    chr->knuckles_anim_frame++;

    if (chr->knuckles_anim_frame >= frame_count)
        chr->knuckles_anim_frame = 0;
}

static int knuckles_calc_frame(character_t *chr, int mode)
{
    int frame_count;
    bool hold_last = false;

    switch (mode)
    {
        case KnucklesAnimIdle:
            frame_count = KNUCKLES_IDLE_FRAMES;
            break;
        case KnucklesAnimWalk:
            frame_count = KNUCKLES_WALK_FRAMES;
            break;
        case KnucklesAnimRun:
            frame_count = KNUCKLES_RUN_FRAMES;
            break;
        case KnucklesAnimJump:
            frame_count = KNUCKLES_JUMP_FRAMES;
            hold_last = true;
            break;
        case KnucklesAnimFall:
            frame_count = KNUCKLES_FALL_FRAMES;
            hold_last = true;
            break;
        case KnucklesAnimLand:
            frame_count = KNUCKLES_LAND_FRAMES;
            hold_last = true;
            break;
        case KnucklesAnimPunch:
            frame_count = KNUCKLES_PUNCH_FRAMES;
            hold_last = true; // stop on last frame until punch state clears
            break;
        case KnucklesAnimDamaged:
            frame_count = KNUCKLES_DAMAGED_FRAMES;
            break;
        default:
            frame_count = KNUCKLES_IDLE_FRAMES;
            break;
    }

    if (chr->knuckles_anim_mode != mode)
    {
        chr->knuckles_anim_mode = mode;
        chr->knuckles_anim_frame = 0;
        chr->knuckles_anim_ticks = 0;
    }

    knuckles_update_animation_frame(chr, frame_count, hold_last);
    return chr->knuckles_anim_frame;
}

static bool knuckles_has_movement_input(const character_t *chr)
{
    if (chr == JO_NULL)
        return false;

    return chr->walk || chr->spin || chr->punch || chr->punch2 || chr->kick || chr->kick2;
}

static void knuckles_render_current_frame_for(character_t *chr, int sprite_id)
{
    if (!knuckles_sheet_ready || sprite_id < 0)
        return;

    if (chr->life <= 0)
        return;

    int mode = chr->knuckles_anim_mode;
    if (mode < KnucklesAnimIdle || mode > KnucklesAnimDamaged)
        mode = KnucklesAnimIdle;

    int frame = chr->knuckles_anim_frame;
    int frame_x = 0;
    int frame_y = 0;

    int frame_width = CHARACTER_WIDTH;

    switch (mode)
    {
        case KnucklesAnimIdle:
            frame_x = frame * CHARACTER_WIDTH;
            frame_y = 0;
            frame_width = CHARACTER_WIDTH;
            break;
        case KnucklesAnimWalk:
            frame_x = frame * CHARACTER_WIDTH;
            frame_y = CHARACTER_HEIGHT;
            frame_width = CHARACTER_WIDTH;
            break;
        case KnucklesAnimRun:
            frame_x = frame * CHARACTER_WIDTH;
            frame_y = CHARACTER_HEIGHT * 2;
            frame_width = CHARACTER_WIDTH;
            break;
        case KnucklesAnimJump:
            frame_x = frame * CHARACTER_WIDTH;
            frame_y = CHARACTER_HEIGHT * 3;
            frame_width = CHARACTER_WIDTH;
            break;
        case KnucklesAnimFall:
            frame_x = frame * CHARACTER_WIDTH;
            frame_y = CHARACTER_HEIGHT * 4;
            frame_width = CHARACTER_WIDTH;
            break;
        case KnucklesAnimLand:
            frame_x = (4 + frame) * CHARACTER_WIDTH;
            frame_y = CHARACTER_HEIGHT * 4;
            frame_width = CHARACTER_WIDTH;
            if (chr->landed && knuckles_has_movement_input(chr))
            {
                chr->landed = false;
                chr->knuckles_anim_mode = KnucklesAnimIdle;
            }
            break;
        case KnucklesAnimPunch:
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
        case KnucklesAnimDamaged:
            frame_y = CHARACTER_HEIGHT * 5;
            frame_x = (3 + frame) * CHARACTER_WIDTH;
            frame_width = CHARACTER_WIDTH;
            if (chr->stun_timer <= 0)
            {
                chr->knuckles_anim_mode = KnucklesAnimIdle;
                chr->knuckles_anim_frame = 0;
                chr->knuckles_anim_ticks = 0;
            }
            break;
        default:
            frame_x = 0;
            frame_y = 0;
            frame_width = CHARACTER_WIDTH;
            break;
    }

    int target_width = CHARACTER_WIDTH;
    if (mode == KnucklesAnimPunch && (frame == 2 || frame == 3))
        target_width = 48;

    knuckles_copy_sheet_frame_to_sprite(sprite_id, frame_x, frame_y, frame_width, target_width);
}

static int knuckles_ensure_wram_sprite(character_t *chr)
{
    return character_ensure_wram_sprite(chr, &knuckles_sprite_id);
}

static int knuckles_ensure_punch_wram_sprite(character_t *chr)
{
    if (knuckles_punch_sprite_id < 0)
        knuckles_punch_sprite_id = character_create_blank_sprite_with_size(48, CHARACTER_HEIGHT);
    return knuckles_punch_sprite_id;
}

static int knuckles_ensure_damaged_wram_sprite(character_t *chr)
{
    if (knuckles_damaged_sprite_id < 0)
        knuckles_damaged_sprite_id = character_create_blank_sprite_with_size(CHARACTER_WIDTH, CHARACTER_HEIGHT);
    return knuckles_damaged_sprite_id;
}

#define KNUCKLES_LONG_FALL_MS 1000

void knuckles_running_animation_handling(void)
{
    character_t *chr = &character_ref;

    if (chr->life <= 0)
        return;

    if (chr->stun_timer > 0)
    {
        if (chr->knuckles_anim_mode != KnucklesAnimDamaged)
        {
            chr->knuckles_anim_mode = KnucklesAnimDamaged;
            chr->knuckles_anim_frame = 0;
            chr->knuckles_anim_ticks = 0;
        }
        return;
    }

    if (chr->punch || chr->punch2)
    {
        if (chr->knuckles_anim_mode != KnucklesAnimPunch)
        {
            chr->knuckles_anim_mode = KnucklesAnimPunch;
            chr->knuckles_anim_frame = 0;
            chr->knuckles_anim_ticks = 0;
        }

        if (chr->knuckles_anim_frame >= KNUCKLES_PUNCH_FRAMES - 1)
        {
            chr->punch = false;
            chr->punch2 = false;
            chr->punch2_requested = false;
            chr->perform_kick2 = false;
            chr->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
            chr->knuckles_anim_mode = KnucklesAnimIdle;
            chr->knuckles_anim_frame = 0;
            chr->knuckles_anim_ticks = 0;
        }
        return;
    }

    if (physics.is_in_air)
    {
        chr->knuckles_land_pending = false;

        if (physics.speed_y < 0.0f)
        {
            if (chr->knuckles_anim_mode != KnucklesAnimJump)
            {
                chr->knuckles_anim_mode = KnucklesAnimJump;
                chr->knuckles_anim_frame = 0;
                chr->knuckles_anim_ticks = 0;
            }
            chr->knuckles_fall_time_ms = 0;
            return;
        }

        // Furthest downward state: Fall.
        if (chr->knuckles_anim_mode != KnucklesAnimFall)
        {
            chr->knuckles_anim_mode = KnucklesAnimFall;
            chr->knuckles_anim_frame = 0;
            chr->knuckles_anim_ticks = 0;
        }

        chr->knuckles_fall_time_ms += GAME_FRAME_MS;
        return;
    }

    if (chr->knuckles_land_pending)
    {
        if (character_has_movement_input_for_character(chr))
        {
            chr->knuckles_land_pending = false;
        }
        else
        {
            if (chr->knuckles_anim_mode != KnucklesAnimLand)
            {
                chr->knuckles_anim_mode = KnucklesAnimLand;
                chr->knuckles_anim_frame = 0;
                chr->knuckles_anim_ticks = 0;
            }
            return;
        }
    }

    if (chr->knuckles_fall_time_ms >= KNUCKLES_LONG_FALL_MS)
    {
        chr->knuckles_land_pending = true;
        chr->knuckles_fall_time_ms = 0;
        chr->knuckles_anim_mode = KnucklesAnimLand;
        chr->knuckles_anim_frame = 0;
        chr->knuckles_anim_ticks = 0;
        return;
    }

    chr->knuckles_fall_time_ms = 0;

    if (chr->walk && chr->run == 2)
        chr->knuckles_anim_mode = KnucklesAnimRun;
    else if (chr->walk)
        chr->knuckles_anim_mode = KnucklesAnimWalk;
    else
        chr->knuckles_anim_mode = KnucklesAnimIdle;
}

static void knuckles_draw_for_character(character_t *chr)
{
    if (!knuckles_sheet_ready)
    {
        // fallback to legacy non-wram draw might be not implemented yet.
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

    int mode = chr->knuckles_anim_mode;
    if (mode < KnucklesAnimIdle || mode > KnucklesAnimDamaged)
        mode = KnucklesAnimIdle;

    knuckles_calc_frame(chr, mode);

    int sprite_id;
    if (mode == KnucklesAnimPunch &&
        (chr->knuckles_anim_frame == 2 || chr->knuckles_anim_frame == 3))
    {
        sprite_id = knuckles_ensure_punch_wram_sprite(chr);
    }
    else
    {
        sprite_id = knuckles_ensure_wram_sprite(chr);
    }

    if (sprite_id < 0)
        return;

    knuckles_render_current_frame_for(chr, sprite_id);

    int draw_x = chr->x;
    if (chr->flip && chr->knuckles_anim_mode == KnucklesAnimPunch &&
        (chr->knuckles_anim_frame == 2 || chr->knuckles_anim_frame == 3))
    {
        draw_x -= 16;
    }

    jo_sprite_draw3D2(sprite_id, draw_x, chr->y, CHARACTER_SPRITE_Z);
}

void knuckles_draw(character_t *chr)
{
    knuckles_draw_for_character(chr);
}

void display_knuckles(void)
{
    if (!physics.is_in_air)
    {
        character_ref.spin = false;
        character_ref.jump = false;
        character_ref.angle = 0;
    }

    if (character_ref.flip)
        jo_sprite_enable_horizontal_flip();

    knuckles_draw_for_character(&character_ref);

    if (character_ref.flip)
        jo_sprite_disable_horizontal_flip();
}

void knuckles_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    if (chr != JO_NULL)
        knuckles_ref = chr;
    if (phy != JO_NULL)
        knuckles_physics = phy;
}

void load_knuckles(void)
{
    if (!knuckles_loaded)
    {
        if (!knuckles_sheet_ready)
        {
            jo_img sheet = {0};
            if (character_load_sheet(&sheet, "KNK_FUL.TGA", SPRITE_DIR, JO_COLOR_Green))
            {
                knuckles_sheet_width = sheet.width;
                knuckles_sheet_height = sheet.height;
                if (ram_cart_store_sprite("KNK_FUL", sheet.data, (size_t)sheet.width * (size_t)sheet.height * sizeof(unsigned short)))
                {
                    knuckles_sheet_ready = true;
                    knuckles_use_cart_ram = true;
                    knuckles_sheet_img = sheet; // keep sheet loaded for frame padding operations
                }
                else
                {
                    knuckles_sheet_ready = true;
                    knuckles_use_cart_ram = false;
                    knuckles_sheet_img = sheet;
                }
            }
        }

        if (knuckles_sprite_id < 0)
            knuckles_sprite_id = character_create_blank_sprite();

        if (knuckles_punch_sprite_id < 0)
            knuckles_punch_sprite_id = character_create_blank_sprite_with_size(48, CHARACTER_HEIGHT);

        if (knuckles_damaged_sprite_id < 0)
            knuckles_damaged_sprite_id = character_create_blank_sprite();

        knuckles_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_DFT.TGA", JO_COLOR_Green, KnucklesDefeatedTile, JO_TILE_COUNT(KnucklesDefeatedTile));

        knuckles_loaded = true;
    }

    character_ref.wram_sprite_id = knuckles_sprite_id;
    character_ref.damage_sprite_id = knuckles_damaged_sprite_id;
    character_ref.defeated_sprite_id = knuckles_defeated_sprite_id;
    character_ref.defeated_wram_sprite_id = -1;

    character_registry_apply_combat_profile(&character_ref, UiCharacterKnuckles);

    knuckles_physics = &physics;
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
    character_ref.knuckles_anim_mode = KnucklesAnimIdle;
    character_ref.knuckles_anim_frame = 0;
    character_ref.knuckles_anim_ticks = 0;
    character_ref.knuckles_fall_time_ms = 0;
    character_ref.knuckles_land_pending = false;
    character_ref.life = 50;
}

void unload_knuckles(void)
{
    if (!knuckles_loaded)
        return;

    if (knuckles_sheet_ready)
    {
        if (knuckles_use_cart_ram)
        {
            ram_cart_delete_sprite("KNK_FUL");
        }
        else if (knuckles_sheet_img.data != JO_NULL)
        {
            jo_free_img(&knuckles_sheet_img);
        }

        knuckles_sheet_ready = false;
        knuckles_use_cart_ram = false;
        knuckles_sheet_width = 0;
        knuckles_sheet_height = 0;
    }

    knuckles_defeated_sprite_id = -1;
    knuckles_damaged_sprite_id = -1;
    knuckles_sprite_id = -1;
    character_ref.wram_sprite_id = -1;
    knuckles_loaded = false;
}
