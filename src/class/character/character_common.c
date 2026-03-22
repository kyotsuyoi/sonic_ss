#include <stddef.h>
#include <string.h>
#include <jo/jo.h>
#include "character_common.h"
#include "game_constants.h"
#include "player.h"
#include "character_registry.h"
#include "ram_cart.h"

int character_create_blank_sprite(void)
{
    return character_create_blank_sprite_with_size(CHARACTER_WIDTH, CHARACTER_HEIGHT);
}

int character_create_blank_sprite_with_size(int width, int height)
{
    if (width <= 0 || height <= 0)
        return -1;

    jo_img blank;
    blank.width = width;
    blank.height = height;
    blank.data = (unsigned short *)jo_malloc((size_t)width * (size_t)height * sizeof(unsigned short));
    if (blank.data == JO_NULL)
        return -1;

    for (size_t i = 0; i < (size_t)width * (size_t)height; ++i)
        ((unsigned short *)blank.data)[i] = JO_COLOR_Transparent;

    int id = jo_sprite_add(&blank);
    jo_free_img(&blank);
    return id;
}

int character_create_blank_animation(int frame_count)
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

int character_ensure_wram_sprite(character_t *chr, int *global_sprite_id)
{
    if (chr == JO_NULL)
        return -1;

    runtime_log_verbose("WRA_SPT: ENTRY char=%d cur=%d", chr->character_id, chr->wram_sprite_id);

    if (chr->wram_sprite_id >= 0)
    {
        runtime_log_verbose("WRA_SPT: SAME char=%d id=%d", chr->character_id, chr->wram_sprite_id);
        return chr->wram_sprite_id;
    }

    /* Prefer reusing the global sprite only for the primary player instance */
    extern character_t player;
    if (chr == &player && global_sprite_id != JO_NULL && *global_sprite_id >= 0)
        chr->wram_sprite_id = *global_sprite_id;

    if (chr->wram_sprite_id < 0)
        chr->wram_sprite_id = character_create_blank_sprite();

    if (chr->wram_sprite_id >= 0)
        runtime_log("WRA_SPT: ASSIGN char=%d id=%d", chr->character_id, chr->wram_sprite_id);
    else
        runtime_log("WRA_SPT: FAIL char=%d", chr->character_id);

    return chr->wram_sprite_id;

}

int character_ensure_punch_wram_sprite(character_t *chr)
{
    if (chr == JO_NULL)
        return -1;

    if (chr->punch_wram_sprite_id < 0)
        chr->punch_wram_sprite_id = character_create_blank_sprite_with_size(48, CHARACTER_HEIGHT);

    return chr->punch_wram_sprite_id;
}

int character_ensure_damaged_wram_sprite(character_t *chr)
{
    if (chr == JO_NULL)
        return -1;

    if (chr->damaged_wram_sprite_id < 0)
        chr->damaged_wram_sprite_id = character_create_blank_sprite_with_size(CHARACTER_WIDTH, CHARACTER_HEIGHT);

    return chr->damaged_wram_sprite_id;
}

bool character_has_movement_input(const character_t *chr)
{
    if (chr == JO_NULL)
        return false;

    return chr->walk || chr->spin || chr->punch || chr->punch2 || chr->kick || chr->kick2;
}

void character_update_animation_frame_generic(int *anim_frame, int *anim_ticks, int frame_count, bool hold_last)
{
    if (anim_frame == JO_NULL || anim_ticks == JO_NULL)
        return;

    if (*anim_frame >= frame_count)
        *anim_frame = frame_count - 1;

    (*anim_ticks)++;
    if (*anim_ticks < DEFAULT_SPRITE_FRAME_DURATION)
        return;

    *anim_ticks = 0;

    if (hold_last && *anim_frame >= frame_count - 1)
        return;

    (*anim_frame)++;
    if (*anim_frame >= frame_count)
        *anim_frame = 0;
}

int character_calc_frame_generic(int *anim_mode, int *anim_frame, int *anim_ticks, int mode,
                                 int idle_frames, int walk_frames, int run_frames,
                                 int jump_frames, int fall_frames, int land_frames,
                                 int punch_frames, int damaged_frames)
{
    if (anim_mode == JO_NULL || anim_frame == JO_NULL || anim_ticks == JO_NULL)
        return 0;

    int frame_count;
    bool hold_last = false;

    switch (mode)
    {
        case 0: frame_count = idle_frames; break;
        case 1: frame_count = walk_frames; break;
        case 2: frame_count = run_frames; break;
        case 3: frame_count = jump_frames; hold_last = true; break;
        case 4: frame_count = fall_frames; hold_last = true; break;
        case 5: frame_count = land_frames; hold_last = true; break;
        case 6: frame_count = punch_frames; hold_last = true; break;
        case 7: frame_count = damaged_frames; break;
        default: frame_count = idle_frames; break;
    }

    if (*anim_mode != mode)
    {
        *anim_mode = mode;
        *anim_frame = 0;
        *anim_ticks = 0;
    }

    character_update_animation_frame_generic(anim_frame, anim_ticks, frame_count, hold_last);
    return *anim_frame;
}

void character_copy_sheet_frame_to_sprite_with_cart(int sprite_id, const char *cart_name, int sheet_width, int sheet_height, const jo_img *sheet_img, bool use_cart_ram, int frame_x, int frame_y, int frame_width, int target_width)
{
    if (sprite_id < 0 || frame_width <= 0 || target_width <= 0)
        return;

    if (target_width == frame_width)
    {
        if (use_cart_ram && cart_name != JO_NULL)
        {
            character_copy_cart_sheet_frame_to_sprite(sprite_id, cart_name, sheet_width, sheet_height, frame_x, frame_y, frame_width, CHARACTER_HEIGHT);
            return;
        }

        if (sheet_img != JO_NULL && sheet_img->data != JO_NULL)
        {
            character_copy_sheet_frame_to_sprite_with_size(sprite_id, sheet_img, frame_x, frame_y, frame_width, CHARACTER_HEIGHT);
            return;
        }
    }

    if (sheet_img != JO_NULL && sheet_img->data != JO_NULL)
    {
        jo_img tmp = {0};
        tmp.width = target_width;
        tmp.height = CHARACTER_HEIGHT;
        tmp.data = jo_malloc((size_t)target_width * (size_t)CHARACTER_HEIGHT * sizeof(unsigned short));
        if (tmp.data != JO_NULL)
        {
            unsigned short *dst = (unsigned short *)tmp.data;
            unsigned short *src = (unsigned short *)sheet_img->data;
            size_t pixel_count = (size_t)target_width * (size_t)CHARACTER_HEIGHT;
            for (size_t i = 0; i < pixel_count; ++i)
                dst[i] = JO_COLOR_Transparent;

            for (int y = 0; y < CHARACTER_HEIGHT; ++y)
            {
                unsigned short *src_row = src + (frame_y + y) * sheet_img->width + frame_x;
                unsigned short *dst_row = dst + (size_t)y * target_width;
                memcpy(dst_row, src_row, (size_t)frame_width * sizeof(unsigned short));
            }

            jo_sprite_replace(&tmp, sprite_id);
            jo_free_img(&tmp);
        }
        return;
    }

    if (use_cart_ram && cart_name != JO_NULL)
    {
        character_copy_cart_sheet_frame_to_sprite(sprite_id, cart_name, sheet_width, sheet_height, frame_x, frame_y, frame_width, CHARACTER_HEIGHT);
    }
}

int character_ensure_defeated_wram_sprite(character_t *chr, int defeated_width, int defeated_height)
{
    if (chr->defeated_wram_sprite_id < 0)
    {
        jo_img blank;
        blank.width = defeated_width;
        blank.height = defeated_height;
        blank.data = (unsigned short *)jo_malloc((size_t)defeated_width * (size_t)defeated_height * sizeof(unsigned short));
        if (blank.data == JO_NULL)
            return -1;

        for (size_t i = 0; i < (size_t)defeated_width * (size_t)defeated_height; ++i)
            ((unsigned short *)blank.data)[i] = JO_COLOR_Transparent;

        int id = jo_sprite_add(&blank);
        jo_free_img(&blank);
        chr->defeated_wram_sprite_id = id;
    }

    return chr->defeated_wram_sprite_id;
}

void character_copy_sheet_frame_to_sprite(int sprite_id, const jo_img *sheet, int frame_x, int frame_y)
{
    character_copy_sheet_frame_to_sprite_with_size(sprite_id, sheet, frame_x, frame_y, CHARACTER_WIDTH, CHARACTER_HEIGHT);
}

void character_copy_sheet_frame_to_sprite_with_size(int sprite_id, const jo_img *sheet, int frame_x, int frame_y, int width, int height)
{
    if (sprite_id < 0 || sheet == JO_NULL || sheet->data == JO_NULL || width <= 0 || height <= 0)
        return;

    jo_img tmp = {0};
    tmp.width = width;
    tmp.height = height;
    tmp.data = jo_malloc((size_t)width * (size_t)height * sizeof(unsigned short));
    if (tmp.data == JO_NULL)
        return;

    unsigned short *dst = (unsigned short *)tmp.data;
    unsigned short *src = (unsigned short *)sheet->data;
    int sheet_width = sheet->width;

    for (int y = 0; y < height; ++y)
    {
        unsigned short *src_row = src + (frame_y + y) * sheet_width + frame_x;
        unsigned short *dst_row = dst + y * width;
        memcpy(dst_row, src_row, width * sizeof(unsigned short));
    }

    jo_sprite_replace(&tmp, sprite_id);
    jo_free_img(&tmp);
}

bool character_copy_cart_sheet_frame_to_sprite(int sprite_id, const char *cart_name, int sheet_width, int sheet_height, int frame_x, int frame_y, int width, int height)
{
    if (cart_name == JO_NULL || width <= 0 || height <= 0 || sprite_id < 0)
        return false;

    return ram_cart_draw_frame(cart_name, sheet_width, sheet_height, frame_x, frame_y, width, height, sprite_id);
}

bool character_draw_cart_frame(character_t *chr, int sprite_id, const char *cart_name, int sheet_width, int sheet_height)
{
    if (chr == JO_NULL || sprite_id < 0 || cart_name == JO_NULL)
        return false;

    int row = 0;
    int col = 0;
    if (!character_get_sheet_frame_coords(chr, &row, &col))
        return false;

    if (!character_copy_cart_sheet_frame_to_sprite(sprite_id, cart_name, sheet_width, sheet_height, col * CHARACTER_WIDTH, row * CHARACTER_HEIGHT, CHARACTER_WIDTH, CHARACTER_HEIGHT))
        return false;

    if (chr->spin)
    {
        jo_sprite_draw3D_and_rotate2(sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z, chr->angle);
        if (chr->flip)
            chr->angle -= CHARACTER_SPIN_SPEED;
        else
            chr->angle += CHARACTER_SPIN_SPEED;
        return true;
    }

    jo_sprite_draw3D2(sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
    return true;
}

void character_copy_defeated_sheet_frame_to_sprite(int sprite_id, const jo_img *sheet, int defeated_width, int defeated_height)
{
    if (sprite_id < 0 || sheet == JO_NULL || sheet->data == JO_NULL)
        return;

    static jo_img tmp = {0};
    if (tmp.data == JO_NULL || tmp.width != defeated_width || tmp.height != defeated_height)
    {
        if (tmp.data != JO_NULL)
            jo_free_img(&tmp);
        tmp.width = defeated_width;
        tmp.height = defeated_height;
        tmp.data = jo_malloc((size_t)defeated_width * (size_t)defeated_height * sizeof(unsigned short));
        if (tmp.data == JO_NULL)
            return;
    }

    unsigned short *dst = (unsigned short *)tmp.data;
    unsigned short *src = (unsigned short *)sheet->data;
    int sheet_width = sheet->width;
    int copy_width = defeated_width;
    int x_offset = 0;

    if (sheet_width < defeated_width)
    {
        copy_width = sheet_width;
        x_offset = (defeated_width - sheet_width) / 2;
    }
    else if (sheet_width > defeated_width)
    {
        copy_width = defeated_width;
    }

    for (int y = 0; y < defeated_height; ++y)
    {
        unsigned short *src_row = src + y * sheet_width;
        unsigned short *dst_row = dst + y * defeated_width;

        for (int x = 0; x < defeated_width; ++x)
            dst_row[x] = JO_COLOR_Transparent;

        memcpy(dst_row + x_offset, src_row, copy_width * sizeof(unsigned short));
    }

    jo_sprite_replace(&tmp, sprite_id);
}

bool character_load_sheet(jo_img *sheet, const char *filename, const char *dir, jo_color transparent)
{
    if (sheet == JO_NULL || filename == JO_NULL || dir == JO_NULL)
        return false;

    if (sheet->data != JO_NULL)
        return true;

    char *sheet_data = jo_fs_read_file_in_dir(filename, dir, JO_NULL);
    if (sheet_data == JO_NULL)
        return false;

    bool ok = false;
    if (jo_tga_loader_from_stream(sheet, sheet_data, transparent) == JO_TGA_OK)
        ok = true;

    jo_free(sheet_data);
    return ok;
}

void character_unload_sheet(jo_img *sheet)
{
    if (sheet == JO_NULL)
        return;
    if (sheet->data != JO_NULL)
        jo_free_img(sheet);
}

bool character_get_sheet_frame_coords(const character_t *chr, int *row, int *col)
{
    if (chr == JO_NULL || row == JO_NULL || col == JO_NULL)
        return false;

    if (chr->life <= 0)
        return false;

    if (chr->spin)
    {
        *row = 0;
        *col = 6;
        return true;
    }

    if (chr->stun_timer > 0)
    {
        *row = 0;
        *col = 5;
        return true;
    }

    if (chr->punch || chr->punch2)
    {
        *row = 4;
        *col = (chr->punch_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->punch_anim_id) : 0;
        return true;
    }

    if (chr->kick || chr->kick2)
    {
        *row = 5;
        *col = (chr->kick_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->kick_anim_id) : 0;
        return true;
    }

    if (chr->jump)
    {
        *row = 0;
        *col = 4;
        return true;
    }

    if (chr->walk && chr->run == 0)
    {
        *row = 1;
        *col = (chr->walking_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->walking_anim_id) : 0;
        return true;
    }

    if (chr->walk && chr->run == 1)
    {
        *row = 2;
        *col = (chr->running1_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->running1_anim_id) : 0;
        return true;
    }

    if (chr->walk && chr->run == 2)
    {
        *row = 3;
        *col = (chr->running2_anim_id >= 0) ? jo_get_sprite_anim_frame(chr->running2_anim_id) : 0;
        return true;
    }

    *row = 0;
    *col = (chr->stand_sprite_id >= 0) ? jo_get_sprite_anim_frame(chr->stand_sprite_id) : 0;
    return true;
}

bool character_draw_sheet_frame(character_t *chr, int sprite_id, const jo_img *sheet)
{
    if (chr == JO_NULL || sprite_id < 0 || sheet == JO_NULL || sheet->data == JO_NULL)
        return false;

    int row, col;
    if (!character_get_sheet_frame_coords(chr, &row, &col))
        return false;

    character_copy_sheet_frame_to_sprite(sprite_id, sheet, col * CHARACTER_WIDTH, row * CHARACTER_HEIGHT);

    if (chr->spin)
    {
        jo_sprite_draw3D_and_rotate2(sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z, chr->angle);
        if (chr->flip)
            chr->angle -= CHARACTER_SPIN_SPEED;
        else
            chr->angle += CHARACTER_SPIN_SPEED;
        return true;
    }

    jo_sprite_draw3D2(sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
    return true;
}

bool character_draw_defeated(character_t *chr, int sprite_id, const jo_img *sheet, int defeated_width, int defeated_height)
{
    if (chr == JO_NULL || sprite_id < 0 || sheet == JO_NULL || sheet->data == JO_NULL)
        return false;

    if (chr->life > 0)
        return false;

    int defeated_sprite_id = character_ensure_defeated_wram_sprite(chr, defeated_width, defeated_height);
    if (defeated_sprite_id < 0)
        return false;

    character_copy_defeated_sheet_frame_to_sprite(defeated_sprite_id, sheet, defeated_width, defeated_height);

    int draw_x = chr->x - (defeated_width - CHARACTER_WIDTH) / 2;
    jo_sprite_draw3D2(defeated_sprite_id,
                      chr->x,
                      chr->y + (CHARACTER_HEIGHT - defeated_height),
                      CHARACTER_SPRITE_Z);
    return true;
}

void character_draw_legacy_frame(const character_t *chr)
{
    if (chr == JO_NULL)
        return;

    if (chr->spin)
    {
        jo_sprite_draw3D_and_rotate2(chr->spin_sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z, chr->angle);
        if (chr->flip)
            ((character_t *)chr)->angle -= CHARACTER_SPIN_SPEED;
        else
            ((character_t *)chr)->angle += CHARACTER_SPIN_SPEED;
        return;
    }

    if (chr->life <= 0)
        return;

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
}

void character_draw_sheet_character(character_t *chr,
                                    bool sheet_ready,
                                    int *anim_mode,
                                    int *anim_frame,
                                    character_calc_frame_fn calc_frame_fn,
                                    character_ensure_sprite_fn ensure_wram_fn,
                                    character_ensure_sprite_fn ensure_punch_fn,
                                    character_render_frame_fn render_frame_fn,
                                    bool flip)
{
    if (chr == JO_NULL || anim_mode == JO_NULL || anim_frame == JO_NULL || calc_frame_fn == JO_NULL || ensure_wram_fn == JO_NULL || ensure_punch_fn == JO_NULL || render_frame_fn == JO_NULL)
        return;

    if (!sheet_ready)
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

    int mode = *anim_mode;
    if (mode < CHARACTER_ANIM_IDLE || mode > CHARACTER_ANIM_DAMAGED)
        mode = CHARACTER_ANIM_IDLE;

    calc_frame_fn(chr, mode);

    int sprite_id;
    if (mode == CHARACTER_ANIM_PUNCH && (*anim_frame == 2 || *anim_frame == 3))
        sprite_id = ensure_punch_fn(chr);
    else
        sprite_id = ensure_wram_fn(chr);

    if (sprite_id < 0)
        return;

    render_frame_fn(chr, sprite_id);

    int draw_x = chr->x;
    if (flip && mode == CHARACTER_ANIM_PUNCH && (*anim_frame == 2 || *anim_frame == 3))
        draw_x -= 16;

    jo_sprite_draw3D2(sprite_id, draw_x, chr->y, CHARACTER_SPRITE_Z);
}

void character_get_frame_coords(character_t *chr,
                                int mode,
                                int frame,
                                int *frame_x,
                                int *frame_y,
                                int *frame_width,
                                int *target_width)
{
    if (chr == JO_NULL || frame_x == JO_NULL || frame_y == JO_NULL || frame_width == JO_NULL || target_width == JO_NULL)
        return;

    *target_width = CHARACTER_WIDTH;

    switch (mode)
    {
        case CHARACTER_ANIM_IDLE:
            *frame_x = frame * CHARACTER_WIDTH;
            *frame_y = 0;
            *frame_width = CHARACTER_WIDTH;
            break;
        case CHARACTER_ANIM_WALK:
            *frame_x = frame * CHARACTER_WIDTH;
            *frame_y = CHARACTER_HEIGHT;
            *frame_width = CHARACTER_WIDTH;
            break;
        case CHARACTER_ANIM_RUN:
            *frame_x = frame * CHARACTER_WIDTH;
            *frame_y = CHARACTER_HEIGHT * 2;
            *frame_width = CHARACTER_WIDTH;
            break;
        case CHARACTER_ANIM_JUMP:
            *frame_x = frame * CHARACTER_WIDTH;
            *frame_y = CHARACTER_HEIGHT * 3;
            *frame_width = CHARACTER_WIDTH;
            break;
        case CHARACTER_ANIM_FALL:
            *frame_x = frame * CHARACTER_WIDTH;
            *frame_y = CHARACTER_HEIGHT * 4;
            *frame_width = CHARACTER_WIDTH;
            break;
        case CHARACTER_ANIM_LAND:
            *frame_x = (4 + frame) * CHARACTER_WIDTH;
            *frame_y = CHARACTER_HEIGHT * 4;
            *frame_width = CHARACTER_WIDTH;
            if (chr->landed && character_has_movement_input(chr))
            {
                chr->landed = false;
                if (chr->character_id == CHARACTER_ID_SONIC)
                {
                    chr->sonic_anim_mode = CHARACTER_ANIM_IDLE;
                }
                else if (chr->character_id == CHARACTER_ID_KNUCKLES)
                {
                    chr->knuckles_anim_mode = CHARACTER_ANIM_IDLE;
                }
                else if (chr->character_id == CHARACTER_ID_AMY)
                {
                    chr->amy_anim_mode = CHARACTER_ANIM_IDLE;
                }
                else if (chr->character_id == CHARACTER_ID_TAILS)
                {
                    chr->tails_anim_mode = CHARACTER_ANIM_IDLE;
                }
            }
            break;
        case CHARACTER_ANIM_PUNCH:
            *frame_y = CHARACTER_HEIGHT * 6;
            *frame_width = CHARACTER_WIDTH;

            switch (frame)
            {
                case 0:
                    *frame_x = 0;
                    *frame_width = 32;
                    break;
                case 1:
                    *frame_x = 32;
                    *frame_width = 32;
                    break;
                case 2:
                    *frame_x = 64;
                    *frame_width = 48;
                    *target_width = 48;
                    break;
                case 3:
                    *frame_x = 112;
                    *frame_width = 48;
                    *target_width = 48;
                    break;
                case 4:
                    *frame_x = 160;
                    *frame_width = 32;
                    break;
                case 5:
                default:
                    *frame_x = 192;
                    *frame_width = 32;
                    break;
            }
            break;
        case CHARACTER_ANIM_DAMAGED:
            *frame_y = CHARACTER_HEIGHT * 5;
            *frame_x = (3 + frame) * CHARACTER_WIDTH;
            *frame_width = CHARACTER_WIDTH;
            if (chr->stun_timer <= 0)
            {
                if (chr->character_id == CHARACTER_ID_SONIC)
                {
                    chr->sonic_anim_mode = CHARACTER_ANIM_IDLE;
                    chr->sonic_anim_frame = 0;
                    chr->sonic_anim_ticks = 0;
                }
                else if (chr->character_id == CHARACTER_ID_KNUCKLES)
                {
                    chr->knuckles_anim_mode = CHARACTER_ANIM_IDLE;
                    chr->knuckles_anim_frame = 0;
                    chr->knuckles_anim_ticks = 0;
                }
                else if (chr->character_id == CHARACTER_ID_AMY)
                {
                    chr->amy_anim_mode = CHARACTER_ANIM_IDLE;
                    chr->amy_anim_frame = 0;
                    chr->amy_anim_ticks = 0;
                }
                else if (chr->character_id == CHARACTER_ID_TAILS)
                {
                    chr->tails_anim_mode = CHARACTER_ANIM_IDLE;
                    chr->tails_anim_frame = 0;
                    chr->tails_anim_ticks = 0;
                }
            }
            return;
        default:
            *frame_x = 0;
            *frame_y = 0;
            *frame_width = CHARACTER_WIDTH;
            break;
    }
}

void character_render_sheet_frame(character_t *chr,
                                  int sprite_id,
                                  bool sheet_ready,
                                  int *anim_mode,
                                  int *anim_frame,
                                  character_frame_coords_fn frame_coords_fn,
                                  character_copy_frame_fn copy_frame_fn)
{
    if (chr == JO_NULL || anim_mode == JO_NULL || anim_frame == JO_NULL || frame_coords_fn == JO_NULL || copy_frame_fn == JO_NULL)
        return;

    if (!sheet_ready)
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

    int mode = *anim_mode;
    if (mode < CHARACTER_ANIM_IDLE || mode > CHARACTER_ANIM_DAMAGED)
        mode = CHARACTER_ANIM_IDLE;

    int frame = *anim_frame;
    int frame_x = 0;
    int frame_y = 0;
    int frame_width = CHARACTER_WIDTH;
    int target_width = CHARACTER_WIDTH;

    frame_coords_fn(chr, mode, frame, &frame_x, &frame_y, &frame_width, &target_width);

    copy_frame_fn(sprite_id, frame_x, frame_y, frame_width, target_width);
}

void character_display(character_t *chr,
                       jo_sidescroller_physics_params *physics,
                       void (*draw_fn)(character_t *chr))
{
    if (chr == JO_NULL || physics == JO_NULL || draw_fn == JO_NULL)
        return;

    if (!physics->is_in_air)
    {
        chr->spin = false;
        chr->jump = false;
        chr->angle = 0;
    }

    if (chr->flip)
        jo_sprite_enable_horizontal_flip();

    draw_fn(chr);

    if (chr->flip)
        jo_sprite_disable_horizontal_flip();
}

void character_set_current(character_t *chr,
                           jo_sidescroller_physics_params *phy,
                           character_t **ref,
                           jo_sidescroller_physics_params **phy_ref)
{
    if (chr != JO_NULL && ref != JO_NULL)
        *ref = chr;
    if (phy != JO_NULL && phy_ref != JO_NULL)
        *phy_ref = phy;
}

static void character_apply_common_load_state(character_t *chr,
                                              int sprite_id,
                                              int damaged_sprite_id,
                                              int defeated_sprite_id,
                                              ui_character_choice_t profile)
{
    if (chr == JO_NULL)
        return;

    chr->wram_sprite_id = sprite_id;
    chr->punch_wram_sprite_id = -1;
    chr->damaged_wram_sprite_id = -1;
    chr->damage_sprite_id = damaged_sprite_id;
    chr->defeated_sprite_id = defeated_sprite_id;
    chr->defeated_wram_sprite_id = -1;

    character_registry_apply_combat_profile(chr, profile);

    chr->charged_kick_hold_ms = 0;
    chr->charged_kick_ready = false;
    chr->charged_kick_active = false;
    chr->charged_kick_phase = 0;
    chr->charged_kick_phase_timer = 0;
    chr->hit_done_punch1 = false;
    chr->hit_done_punch2 = false;
    chr->hit_done_kick1 = false;
    chr->hit_done_kick2 = false;
    chr->attack_cooldown = 0;
    chr->life = 50;

    chr->wram_anim_state = 0;
    chr->wram_anim_frame = 0;
    chr->wram_anim_frame_ticks = 0;
    chr->wram_anim_frame_duration = 0;
    chr->wram_anim_frame_count = 0;
}

bool character_load_generic(character_t *chr,
                            bool *loaded,
                            bool *sheet_ready,
                            bool *use_cart_ram,
                            jo_img *sheet_img,
                            int *sheet_width,
                            int *sheet_height,
                            int *sprite_id,
                            int *punch_sprite_id,
                            int *damaged_sprite_id,
                            int *defeated_sprite_id,
                            const char *sprite_dir,
                            const char *sheet_name,
                            const char *defeated_tile_name,
                            const jo_tile *defeated_tile,
                            int defeated_tile_count,
                            ui_character_choice_t profile)
{
    if (chr == JO_NULL || loaded == JO_NULL || sheet_ready == JO_NULL || use_cart_ram == JO_NULL || sheet_img == JO_NULL || sheet_width == JO_NULL || sheet_height == JO_NULL || sprite_id == JO_NULL || punch_sprite_id == JO_NULL || damaged_sprite_id == JO_NULL || defeated_sprite_id == JO_NULL || sheet_name == JO_NULL || defeated_tile_name == JO_NULL || defeated_tile == JO_NULL)
        return false;

    if (!*loaded)
    {
        if (!*sheet_ready)
        {
            jo_img sheet = {0};
            if (character_load_sheet(&sheet, sheet_name, sprite_dir, JO_COLOR_Green))
            {
                *sheet_width = sheet.width;
                *sheet_height = sheet.height;
                if (ram_cart_store_sprite(sheet_name, sheet.data, (size_t)sheet.width * (size_t)sheet.height * sizeof(unsigned short)))
                {
                    *sheet_ready = true;
                    *use_cart_ram = true;
                    *sheet_img = sheet;
                }
                else
                {
                    *sheet_ready = true;
                    *use_cart_ram = false;
                    *sheet_img = sheet;
                }
            }
        }

        if (*sprite_id < 0)
            *sprite_id = character_create_blank_sprite();

        if (*punch_sprite_id < 0)
            *punch_sprite_id = character_create_blank_sprite_with_size(48, CHARACTER_HEIGHT);

        if (*damaged_sprite_id < 0)
            *damaged_sprite_id = character_create_blank_sprite();

        *defeated_sprite_id = jo_sprite_add_tga_tileset(sprite_dir, defeated_tile_name, JO_COLOR_Green, defeated_tile, defeated_tile_count);

        *loaded = true;
    }

    character_apply_common_load_state(chr, *sprite_id, *damaged_sprite_id, *defeated_sprite_id, profile);

    return true;
}

void character_unload_generic(character_t *chr,
                              bool *loaded,
                              bool *sheet_ready,
                              bool *use_cart_ram,
                              jo_img *sheet_img,
                              int *sheet_width,
                              int *sheet_height,
                              int *sprite_id,
                              const char *sheet_name)
{
    if (loaded == JO_NULL || sheet_ready == JO_NULL || use_cart_ram == JO_NULL || sheet_img == JO_NULL || sheet_width == JO_NULL || sheet_height == JO_NULL || sprite_id == JO_NULL)
        return;

    if (!*loaded)
        return;

    if (*sheet_ready)
    {
        if (*use_cart_ram && sheet_name != JO_NULL && sheet_name[0] != '\0')
        {
            ram_cart_delete_sprite(sheet_name);
        }
        else if (sheet_img->data != JO_NULL)
        {
            jo_free_img(sheet_img);
        }

        *sheet_ready = false;
        *use_cart_ram = false;
        *sheet_width = 0;
        *sheet_height = 0;
    }

    *sprite_id = -1;
    if (chr != JO_NULL)
        chr->wram_sprite_id = -1;

    *loaded = false;
}

void character_running_animation_handling(character_t *chr, jo_sidescroller_physics_params *physics)
{
    int speed_step;

    if (jo_physics_is_standing(physics))
    {
        jo_reset_sprite_anim(chr->running1_anim_id);
        jo_reset_sprite_anim(chr->running2_anim_id);

        if (!jo_is_sprite_anim_stopped(chr->walking_anim_id))
        {
            jo_reset_sprite_anim(chr->walking_anim_id);
            jo_reset_sprite_anim(chr->stand_sprite_id);
        }
        else
        {
            if (jo_is_sprite_anim_stopped(chr->stand_sprite_id))
                jo_start_sprite_anim_loop(chr->stand_sprite_id);

            if (jo_get_sprite_anim_frame(chr->stand_sprite_id) == 0)
                jo_set_sprite_anim_frame_rate(chr->stand_sprite_id, 70);
            else
                jo_set_sprite_anim_frame_rate(chr->stand_sprite_id, 2);
        }
    }
    else
    {
        if (chr->run == 0)
        {
            jo_reset_sprite_anim(chr->running1_anim_id);
            jo_reset_sprite_anim(chr->running2_anim_id);

            if (jo_is_sprite_anim_stopped(chr->walking_anim_id))
                jo_start_sprite_anim_loop(chr->walking_anim_id);
        }
        else if (chr->run == 1)
        {
            jo_reset_sprite_anim(chr->walking_anim_id);
            jo_reset_sprite_anim(chr->running2_anim_id);

            if (jo_is_sprite_anim_stopped(chr->running1_anim_id))
                jo_start_sprite_anim_loop(chr->running1_anim_id);
        }
        else if (chr->run == 2)
        {
            jo_reset_sprite_anim(chr->walking_anim_id);
            jo_reset_sprite_anim(chr->running1_anim_id);

            if (jo_is_sprite_anim_stopped(chr->running2_anim_id))
                jo_start_sprite_anim_loop(chr->running2_anim_id);
        }

        speed_step = (int)JO_ABS(physics->speed);

        if (speed_step >= 6)
        {
            jo_set_sprite_anim_frame_rate(chr->running2_anim_id, 3);
            chr->run = 2;
        }
        else if (speed_step >= 5)
        {
            jo_set_sprite_anim_frame_rate(chr->running1_anim_id, 4);
            chr->run = 1;
        }
        else if (speed_step >= 4)
        {
            jo_set_sprite_anim_frame_rate(chr->running1_anim_id, 5);
            chr->run = 1;
        }
        else if (speed_step >= 3)
        {
            jo_set_sprite_anim_frame_rate(chr->running1_anim_id, 6);
            chr->run = 1;
        }
        else if (speed_step >= 2)
        {
            jo_set_sprite_anim_frame_rate(chr->running1_anim_id, 7);
            chr->run = 1;
        }
        else if (speed_step >= 1)
        {
            jo_set_sprite_anim_frame_rate(chr->walking_anim_id, 8);
            chr->run = 0;
        }
        else
        {
            jo_set_sprite_anim_frame_rate(chr->walking_anim_id, 9);
            chr->run = 0;
        }
    }

    player_update_punch_state_for_character(chr);
    player_update_kick_state_for_character(chr);
}

void character_running_animation_handling_sheet(character_t *chr,
                                                 jo_sidescroller_physics_params *physics,
                                                 int *anim_mode,
                                                 int *anim_frame,
                                                 int *anim_ticks,
                                                 int *fall_time_ms,
                                                 bool *land_pending,
                                                 int long_fall_ms,
                                                 int punch_frames,
                                                 bool *perform_punch2_or_kick2)
{
    if (chr == JO_NULL || physics == JO_NULL || anim_mode == JO_NULL || anim_frame == JO_NULL || anim_ticks == JO_NULL || fall_time_ms == JO_NULL || land_pending == JO_NULL)
        return;

    if (chr->life <= 0)
        return;

    if (chr->stun_timer > 0)
    {
        if (*anim_mode != CHARACTER_ANIM_DAMAGED)
        {
            *anim_mode = CHARACTER_ANIM_DAMAGED;
            *anim_frame = 0;
            *anim_ticks = 0;
        }
        return;
    }

    if (chr->punch || chr->punch2)
    {
        if (*anim_mode != CHARACTER_ANIM_PUNCH)
        {
            *anim_mode = CHARACTER_ANIM_PUNCH;
            *anim_frame = 0;
            *anim_ticks = 0;
        }

        if (*anim_frame >= punch_frames - 1)
        {
            chr->punch = false;
            chr->punch2 = false;
            chr->punch2_requested = false;
            if (perform_punch2_or_kick2 != JO_NULL)
                *perform_punch2_or_kick2 = false;
            chr->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
            *anim_mode = CHARACTER_ANIM_IDLE;
            *anim_frame = 0;
            *anim_ticks = 0;
        }
        return;
    }

    if (physics->is_in_air)
    {
        *land_pending = false;

        if (physics->speed_y < 0.0f)
        {
            if (*anim_mode != CHARACTER_ANIM_JUMP)
            {
                *anim_mode = CHARACTER_ANIM_JUMP;
                *anim_frame = 0;
                *anim_ticks = 0;
            }
            *fall_time_ms = 0;
            return;
        }

        if (*anim_mode != CHARACTER_ANIM_FALL)
        {
            *anim_mode = CHARACTER_ANIM_FALL;
            *anim_frame = 0;
            *anim_ticks = 0;
        }

        *fall_time_ms += GAME_FRAME_MS;
        return;
    }

    if (*land_pending)
    {
        if (character_has_movement_input(chr))
        {
            *land_pending = false;
        }
        else
        {
            if (*anim_mode != CHARACTER_ANIM_LAND)
            {
                *anim_mode = CHARACTER_ANIM_LAND;
                *anim_frame = 0;
                *anim_ticks = 0;
            }
            return;
        }
    }

    if (*fall_time_ms >= long_fall_ms)
    {
        *land_pending = true;
        *fall_time_ms = 0;
        *anim_mode = CHARACTER_ANIM_LAND;
        *anim_frame = 0;
        *anim_ticks = 0;
        return;
    }

    *fall_time_ms = 0;

    if (chr->walk && chr->run == 2)
        *anim_mode = CHARACTER_ANIM_RUN;
    else if (chr->walk)
        *anim_mode = CHARACTER_ANIM_WALK;
    else
        *anim_mode = CHARACTER_ANIM_IDLE;
}
