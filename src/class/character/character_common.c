#include <stddef.h>
#include <string.h>
#include <jo/jo.h>
#include "character_common.h"
#include "game_constants.h"
#include "player.h"

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
    if (chr->wram_sprite_id < 0)
    {
        /* Prefer reusing the global sprite only for the primary player instance */
        extern character_t player;
        if (chr == &player && global_sprite_id != JO_NULL && *global_sprite_id >= 0)
            chr->wram_sprite_id = *global_sprite_id;

        if (chr->wram_sprite_id < 0)
            chr->wram_sprite_id = character_create_blank_sprite();

        runtime_log("character: wram sprite %d for char=%d", chr->wram_sprite_id, chr->character_id);
    }

    if (chr->wram_sprite_id < 0)
        runtime_log("character: failed to create wram sprite for char=%d", chr->character_id);

    return chr->wram_sprite_id;
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
                      draw_x,
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
