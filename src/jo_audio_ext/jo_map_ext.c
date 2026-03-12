#include "jo_map_ext.h"
#include "jo/colors.h"
#include "jo/sprite_animator.h"
#include "jo/sprites.h"
#include "jo/malloc.h"

extern jo_texture_definition __jo_sprite_def[JO_MAX_SPRITE];
extern jo_picture_definition __jo_sprite_pic[JO_MAX_SPRITE];

typedef struct
{
    short x;
    short y;
    short real_x;
    short real_y;
    short width;
    short height;
#if JO_MAX_SPRITE > 255
    unsigned short
#else
    unsigned char
#endif
    sprite_or_anim_id;
    unsigned char attribute;
    bool is_animated;
    bool is_visible_on_screen;
    jo_pos3D pos;
} jo_map_ext_tile_t;

static jo_map_ext_tile_t *g_map_ext[JO_MAP_MAX_LAYER];
static unsigned short g_map_ext_tile_count[JO_MAP_MAX_LAYER];
static int g_map_ext_depth_fixed[JO_MAP_MAX_LAYER];
static bool g_map_ext_created[JO_MAP_MAX_LAYER];

static void jo_map_ext_store_tile(const unsigned int layer,
                                  const short x,
                                  const short y,
#if JO_MAX_SPRITE > 255
                                  const unsigned short sprite_or_anim_id,
#else
                                  const unsigned char sprite_or_anim_id,
#endif
                                  const bool is_animated,
                                  const unsigned char attribute)
{
    int sprite_id;
    jo_map_ext_tile_t *new_tile;

    new_tile = &g_map_ext[layer][g_map_ext_tile_count[layer]];
    new_tile->pos.z = g_map_ext_depth_fixed[layer];
    new_tile->is_visible_on_screen = false;
    new_tile->sprite_or_anim_id = sprite_or_anim_id;
    new_tile->real_x = x;
    new_tile->real_y = y;
    if (is_animated)
        sprite_id = jo_get_anim_sprite(sprite_or_anim_id);
    else
        sprite_id = sprite_or_anim_id;
    new_tile->x = x - JO_TV_WIDTH_2 + JO_DIV_BY_2(__jo_sprite_def[sprite_id].width);
    new_tile->y = y - JO_TV_HEIGHT_2 + JO_DIV_BY_2(__jo_sprite_def[sprite_id].height);
    new_tile->width = __jo_sprite_def[sprite_id].width;
    new_tile->height = __jo_sprite_def[sprite_id].height;
    new_tile->is_animated = is_animated;
    new_tile->attribute = attribute;
    ++g_map_ext_tile_count[layer];
}

bool jo_map_create_ext(const unsigned int layer, const unsigned short max_tile_count, const short depth)
{
    if (layer >= JO_MAP_MAX_LAYER)
        return false;

    if (g_map_ext[layer] != JO_NULL)
        jo_free(g_map_ext[layer]);

    g_map_ext_depth_fixed[layer] = depth;
    g_map_ext_tile_count[layer] = 0;
    g_map_ext_created[layer] = false;
    g_map_ext[layer] = (jo_map_ext_tile_t *)jo_malloc(max_tile_count * sizeof(*g_map_ext[layer]));
    if (g_map_ext[layer] == JO_NULL)
        return false;

    if (!jo_map_create(layer, max_tile_count, depth))
    {
        jo_free(g_map_ext[layer]);
        g_map_ext[layer] = JO_NULL;
        return false;
    }

    g_map_ext_created[layer] = true;
    return true;
}

void jo_map_add_tile_ext(const unsigned int layer, const short x, const short y,
#if JO_MAX_SPRITE > 255
                         const unsigned short sprite_id,
#else
                         const unsigned char sprite_id,
#endif
                         const unsigned char attribute)
{
    jo_map_add_tile(layer, x, y, sprite_id, attribute);
    jo_map_ext_store_tile(layer, x, y, sprite_id, false, attribute);
}

void jo_map_add_animated_tile_ext(const unsigned int layer, const short x, const short y, const unsigned char anim_id, const unsigned char attribute)
{
    jo_map_add_animated_tile(layer, x, y, anim_id, attribute);
    jo_map_ext_store_tile(layer, x, y, anim_id, true, attribute);
}

bool jo_map_set_tile_sprite_ext(const unsigned int layer, const unsigned short tile_index,
#if JO_MAX_SPRITE > 255
                                const unsigned short sprite_id)
#else
                                const unsigned char sprite_id)
#endif
{
    jo_map_ext_tile_t *tile;

    if (layer >= JO_MAP_MAX_LAYER)
        return false;
    if (g_map_ext[layer] == JO_NULL || tile_index >= g_map_ext_tile_count[layer])
        return false;

    tile = &g_map_ext[layer][tile_index];
    if (tile->is_animated)
        return false;

    tile->sprite_or_anim_id = sprite_id;
    tile->width = __jo_sprite_def[sprite_id].width;
    tile->height = __jo_sprite_def[sprite_id].height;
    tile->x = tile->real_x - JO_TV_WIDTH_2 + JO_DIV_BY_2(tile->width);
    tile->y = tile->real_y - JO_TV_HEIGHT_2 + JO_DIV_BY_2(tile->height);
    return true;
}

int jo_map_per_pixel_vertical_collision_ext(const unsigned int layer, int x, int y, unsigned char *attribute)
{
    jo_map_ext_tile_t *current_tile;
    int i;
    int distance;
    unsigned short *img_data;
    unsigned char *img_data_8bits;
    jo_picture_definition *current_pic;

    if (layer >= JO_MAP_MAX_LAYER || g_map_ext[layer] == JO_NULL)
        return JO_MAP_NO_COLLISION;

    for (i = 0; i < g_map_ext_tile_count[layer]; ++i)
    {
        current_tile = &g_map_ext[layer][i];
        if (!jo_square_intersect(current_tile->real_x, current_tile->real_y, current_tile->width, current_tile->height, x, y, 1, 1))
            continue;

        if (attribute != JO_NULL)
            *attribute = current_tile->attribute;
        current_pic = &__jo_sprite_pic[current_tile->is_animated ? jo_get_anim_sprite(current_tile->sprite_or_anim_id) : current_tile->sprite_or_anim_id];
        x -= current_tile->real_x;
        y -= current_tile->real_y;
        if (current_pic->color_mode == COL_256)
        {
            img_data_8bits = (unsigned char *)current_pic->data;
            if (jo_sprite_get_pixel_palette_index(img_data_8bits, x, y, current_tile->width) == 0)
            {
                for (distance = 1; distance < current_tile->height; ++distance)
                    if (jo_sprite_get_pixel_palette_index(img_data_8bits, x, y + distance, current_tile->width) != 0)
                        return distance;
                return JO_MAP_NO_COLLISION;
            }

            distance = y;
            do
            {
                --y;
                if (jo_sprite_get_pixel_palette_index(img_data_8bits, x, y, current_tile->width) == 0)
                    break;
            }
            while (y > 0);
            return y - distance + 1;
        }

        img_data = (unsigned short *)current_pic->data;
        if (jo_sprite_is_pixel_transparent(img_data, x, y, current_tile->width))
        {
            for (distance = 1; distance < current_tile->height; ++distance)
                if (!jo_sprite_is_pixel_transparent(img_data, x, y + distance, current_tile->width))
                    return distance;
            return JO_MAP_NO_COLLISION;
        }

        distance = y;
        do
        {
            --y;
            if (jo_sprite_is_pixel_transparent(img_data, x, y, current_tile->width))
                break;
        }
        while (y > 0);
        return y - distance + 1;
    }

    return JO_MAP_NO_COLLISION;
}

int jo_map_hitbox_detection_custom_boundaries_ext(const unsigned int layer, const int x, const int y, const int w, const int h)
{
    jo_map_ext_tile_t *current_tile;
    int i;

    if (layer >= JO_MAP_MAX_LAYER || g_map_ext[layer] == JO_NULL)
        return JO_MAP_NO_COLLISION;

    for (i = 0; i < g_map_ext_tile_count[layer]; ++i)
    {
        current_tile = &g_map_ext[layer][i];
        if (jo_square_intersect(current_tile->real_x, current_tile->real_y, current_tile->width, current_tile->height, x, y, w, h))
            return current_tile->attribute;
    }
    return JO_MAP_NO_COLLISION;
}

void jo_map_draw_ext(const unsigned int layer, const short screen_x, const short screen_y)
{
    int i;
    jo_map_ext_tile_t *current_tile;

    if (layer >= JO_MAP_MAX_LAYER || g_map_ext[layer] == JO_NULL)
        return;

    for (i = 0; i < g_map_ext_tile_count[layer]; ++i)
    {
        current_tile = &g_map_ext[layer][i];
        current_tile->is_visible_on_screen = jo_square_intersect(current_tile->real_x,
                                                                 current_tile->real_y,
                                                                 current_tile->width,
                                                                 current_tile->height,
                                                                 screen_x,
                                                                 screen_y,
                                                                 JO_TV_WIDTH,
                                                                 JO_TV_HEIGHT);
        if (!current_tile->is_visible_on_screen)
            continue;

        current_tile->pos.x = current_tile->x - screen_x;
        current_tile->pos.y = current_tile->y - screen_y;
        if (current_tile->is_animated)
            jo_sprite_draw(jo_get_anim_sprite(current_tile->sprite_or_anim_id), &current_tile->pos, true, false);
        else
            jo_sprite_draw(current_tile->sprite_or_anim_id, &current_tile->pos, true, false);
    }
}

void jo_map_free_ext(const unsigned int layer)
{
    if (layer >= JO_MAP_MAX_LAYER)
        return;

    if (g_map_ext[layer] != JO_NULL)
    {
        jo_free(g_map_ext[layer]);
        g_map_ext[layer] = JO_NULL;
    }
    g_map_ext_tile_count[layer] = 0;
    if (g_map_ext_created[layer])
    {
        jo_map_free(layer);
        g_map_ext_created[layer] = false;
    }
}