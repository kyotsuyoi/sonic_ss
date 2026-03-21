#include <jo/jo.h>
#include "sonic.h"
#include "knuckles.h"
#include "character_registry.h"

static character_t *knuckles_ref = &player;
static jo_sidescroller_physics_params *knuckles_physics = &physics;

#define character_ref (*knuckles_ref)
#define physics (*knuckles_physics)

void knuckles_set_current(character_t *chr, jo_sidescroller_physics_params *phy)
{
    if (chr != NULL)
        knuckles_ref = chr;
    if (phy != NULL)
        knuckles_physics = phy;
}

#define SPRITE_DIR "SPT"
#define DEFEATED_SPRITE_WIDTH 46
#define DEFEATED_SPRITE_HEIGHT 32
#define KNUCKLES_FRAME_COUNT 4
#define KNUCKLES_COMBO2_START_FRAME 2
#define KNUCKLES_CHARGED_KICK_PHASE1_FRAMES 6
#define KNUCKLES_CHARGED_KICK_PHASE2_FRAMES 10
#define KNUCKLES_KICK_PART3_INDEX 3
#define KNUCKLES_KICK_PART4_INDEX 2
#define KNUCKLES_KICK_PART3_WIDTH_PIXELS CHARACTER_WIDTH

static bool knuckles_loaded = false;
static int knuckles_walking_base_id;
static int knuckles_running1_base_id;
static int knuckles_running2_base_id;
static int knuckles_stand_base_id;
static int knuckles_punch_base_id;
static int knuckles_kick_base_id;
static int knuckles_walking_anim_id;
static int knuckles_running1_anim_id;
static int knuckles_running2_anim_id;
static int knuckles_stand_anim_id;
static int knuckles_punch_anim_id;
static int knuckles_kick_anim_id;
static int knuckles_spin_sprite_id;
static int knuckles_jump_sprite_id;
static int knuckles_damage_sprite_id;
static int knuckles_defeated_sprite_id;

static const jo_tile KnucklesWalkingTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesRunning1Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesRunning2Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesStandTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesPunchTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesKickTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile KnucklesDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_TILE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void knuckles_reset_animation_lists_except(character_t *chr, int active_anim_id)
{
    if (chr == JO_NULL)
        return;

    if (chr->walking_anim_id >= 0 && chr->walking_anim_id != active_anim_id)
        jo_reset_sprite_anim(chr->walking_anim_id);
    if (chr->running1_anim_id >= 0 && chr->running1_anim_id != active_anim_id)
        jo_reset_sprite_anim(chr->running1_anim_id);
    if (chr->running2_anim_id >= 0 && chr->running2_anim_id != active_anim_id)
        jo_reset_sprite_anim(chr->running2_anim_id);
    if (chr->stand_sprite_id >= 0 && chr->stand_sprite_id != active_anim_id)
        jo_reset_sprite_anim(chr->stand_sprite_id);
    if (chr->punch_anim_id >= 0 && chr->punch_anim_id != active_anim_id)
        jo_reset_sprite_anim(chr->punch_anim_id);
    if (chr->kick_anim_id >= 0 && chr->kick_anim_id != active_anim_id)
        jo_reset_sprite_anim(chr->kick_anim_id);
}

static void knuckles_update_for(character_t *chr, jo_sidescroller_physics_params *phy)
{
    int speed_step;

    if (chr == JO_NULL || phy == JO_NULL)
        return;

    if (jo_physics_is_standing(phy))
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

        speed_step = (int)JO_ABS(phy->speed);

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

void knuckles_update_animation_for(character_t *character, jo_sidescroller_physics_params *physics)
{
    knuckles_update_for(character, physics);
}

inline void knuckles_running_animation_handling(void)
{
    knuckles_update_for(&player, &physics);
}

void knuckles_display_for(character_t *chr, jo_sidescroller_physics_params *phy)
{
    if (chr == JO_NULL || phy == JO_NULL)
        return;

    if (!phy->is_in_air)
    {
        chr->spin = false;
        chr->jump = false;
        chr->angle = 0;
    }

    if (chr->flip)
        jo_sprite_enable_horizontal_flip();

    if (chr->spin)
    {
        knuckles_reset_animation_lists_except(chr, -1);
        jo_sprite_draw3D_and_rotate2(chr->spin_sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z, chr->angle);
        if (chr->flip)
            chr->angle -= CHARACTER_SPIN_SPEED;
        else
            chr->angle += CHARACTER_SPIN_SPEED;
    } else if (chr->life <= 0 && knuckles_defeated_sprite_id >= 0) {
        knuckles_reset_animation_lists_except(chr, -1);
        jo_sprite_draw3D2(knuckles_defeated_sprite_id, chr->x, chr->y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
    } else if (chr->stun_timer > 0 && knuckles_damage_sprite_id >= 0) {
        knuckles_reset_animation_lists_except(chr, -1);
        jo_sprite_draw3D2(knuckles_damage_sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
    } else if (chr->punch || chr->punch2){
        knuckles_reset_animation_lists_except(chr, chr->punch_anim_id);
        jo_sprite_draw3D2(jo_get_anim_sprite(chr->punch_anim_id), chr->x, chr->y, CHARACTER_SPRITE_Z);
    } else if (chr->kick || chr->kick2){
        knuckles_reset_animation_lists_except(chr, chr->kick_anim_id);
        if (chr->charged_kick_enabled && chr->kick && !chr->kick2)
        {
            jo_sprite_draw3D2(knuckles_kick_base_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
        }
        else if (chr->charged_kick_enabled && chr->kick2)
        {
            int front_offset = chr->flip ? -KNUCKLES_KICK_PART3_WIDTH_PIXELS : KNUCKLES_KICK_PART3_WIDTH_PIXELS;

            if (chr->charged_kick_active && chr->charged_kick_phase <= 1)
                jo_sprite_draw3D2(knuckles_kick_base_id + 1, chr->x, chr->y, CHARACTER_SPRITE_Z);
            else
            {
                jo_sprite_draw3D2(knuckles_kick_base_id + KNUCKLES_KICK_PART4_INDEX, chr->x, chr->y, CHARACTER_SPRITE_Z);
                jo_sprite_draw3D2(knuckles_kick_base_id + KNUCKLES_KICK_PART3_INDEX,
                                  chr->x + front_offset,
                                  chr->y,
                                  CHARACTER_SPRITE_Z);
            }
        }
        else
        {
            jo_sprite_draw3D2(jo_get_anim_sprite(chr->kick_anim_id), chr->x, chr->y, CHARACTER_SPRITE_Z);
        }
    } else if (chr->jump){
        knuckles_reset_animation_lists_except(chr, -1);
        jo_sprite_draw3D2(chr->jump_sprite_id, chr->x, chr->y, CHARACTER_SPRITE_Z);
    } else {
        if (chr->walk && chr->run == 0)
        {
            knuckles_reset_animation_lists_except(chr, chr->walking_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(chr->walking_anim_id), chr->x, chr->y, CHARACTER_SPRITE_Z);
        }
        else if (chr->walk && chr->run == 1)
        {
            knuckles_reset_animation_lists_except(chr, chr->running1_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(chr->running1_anim_id), chr->x, chr->y, CHARACTER_SPRITE_Z);
        }
        else if (chr->walk && chr->run == 2)
        {
            knuckles_reset_animation_lists_except(chr, chr->running2_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(chr->running2_anim_id), chr->x, chr->y, CHARACTER_SPRITE_Z);
        }
        else
        {
            knuckles_reset_animation_lists_except(chr, chr->stand_sprite_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(chr->stand_sprite_id), chr->x, chr->y, CHARACTER_SPRITE_Z);
        }
    }

    if (chr->flip)
        jo_sprite_disable_horizontal_flip();
}

inline void display_knuckles(void)
{
    knuckles_display_for(&player, &physics);
}

void load_knuckles(void)
{
    if (!knuckles_loaded)
    {
        knuckles_walking_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_WLK.TGA", JO_COLOR_Green, KnucklesWalkingTiles, JO_TILE_COUNT(KnucklesWalkingTiles));
            knuckles_walking_anim_id = jo_create_sprite_anim(knuckles_walking_base_id, JO_TILE_COUNT(KnucklesWalkingTiles), DEFAULT_SPRITE_FRAME_DURATION);

        knuckles_running1_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_RUN1.TGA", JO_COLOR_Green, KnucklesRunning1Tiles, JO_TILE_COUNT(KnucklesRunning1Tiles));
            knuckles_running1_anim_id = jo_create_sprite_anim(knuckles_running1_base_id, JO_TILE_COUNT(KnucklesRunning1Tiles), DEFAULT_SPRITE_FRAME_DURATION);

        knuckles_running2_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_RUN1.TGA", JO_COLOR_Green, KnucklesRunning2Tiles, JO_TILE_COUNT(KnucklesRunning2Tiles));
            knuckles_running2_anim_id = jo_create_sprite_anim(knuckles_running2_base_id, JO_TILE_COUNT(KnucklesRunning2Tiles), DEFAULT_SPRITE_FRAME_DURATION);

        knuckles_stand_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_STD.TGA", JO_COLOR_Green, KnucklesStandTiles, JO_TILE_COUNT(KnucklesStandTiles));
            knuckles_stand_anim_id = jo_create_sprite_anim(knuckles_stand_base_id, JO_TILE_COUNT(KnucklesStandTiles), DEFAULT_SPRITE_FRAME_DURATION);

        knuckles_spin_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "KNK_SPN.TGA", JO_COLOR_Green);
        knuckles_jump_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "KNK_JMP.TGA", JO_COLOR_Green);
        knuckles_damage_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "KNK_DMG.TGA", JO_COLOR_Green);
        knuckles_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_DFT.TGA", JO_COLOR_Green, KnucklesDefeatedTile, JO_TILE_COUNT(KnucklesDefeatedTile));

        knuckles_punch_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_PNC.TGA", JO_COLOR_Green, KnucklesPunchTiles, JO_TILE_COUNT(KnucklesPunchTiles));
            knuckles_punch_anim_id = jo_create_sprite_anim(knuckles_punch_base_id, JO_TILE_COUNT(KnucklesPunchTiles), DEFAULT_SPRITE_FRAME_DURATION);

        knuckles_kick_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "KNK_KCK.TGA", JO_COLOR_Green, KnucklesKickTiles, JO_TILE_COUNT(KnucklesKickTiles));
            knuckles_kick_anim_id = jo_create_sprite_anim(knuckles_kick_base_id, JO_TILE_COUNT(KnucklesKickTiles), DEFAULT_SPRITE_FRAME_DURATION);

        knuckles_loaded = true;
    }

    character_ref.walking_anim_id = knuckles_walking_anim_id;
    character_ref.running1_anim_id = knuckles_running1_anim_id;
    character_ref.running2_anim_id = knuckles_running2_anim_id;
    character_ref.stand_sprite_id = knuckles_stand_anim_id;
    character_ref.spin_sprite_id = knuckles_spin_sprite_id;
    character_ref.jump_sprite_id = knuckles_jump_sprite_id;
    character_ref.damage_sprite_id = knuckles_damage_sprite_id;
    character_ref.defeated_sprite_id = knuckles_defeated_sprite_id;
    character_ref.punch_anim_id = knuckles_punch_anim_id;
    character_ref.kick_anim_id = knuckles_kick_anim_id;
    character_registry_apply_combat_profile(&character_ref, UiCharacterKnuckles);
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

void unload_knuckles(void)
{
    if (!knuckles_loaded)
        return;

    if (knuckles_walking_anim_id >= 0) jo_remove_sprite_anim(knuckles_walking_anim_id);
    if (knuckles_running1_anim_id >= 0) jo_remove_sprite_anim(knuckles_running1_anim_id);
    if (knuckles_running2_anim_id >= 0) jo_remove_sprite_anim(knuckles_running2_anim_id);
    if (knuckles_stand_anim_id >= 0) jo_remove_sprite_anim(knuckles_stand_anim_id);
    if (knuckles_punch_anim_id >= 0) jo_remove_sprite_anim(knuckles_punch_anim_id);
    if (knuckles_kick_anim_id >= 0) jo_remove_sprite_anim(knuckles_kick_anim_id);

    knuckles_walking_base_id = -1;
    knuckles_running1_base_id = -1;
    knuckles_running2_base_id = -1;
    knuckles_stand_base_id = -1;
    knuckles_punch_base_id = -1;
    knuckles_kick_base_id = -1;
    knuckles_walking_anim_id = -1;
    knuckles_running1_anim_id = -1;
    knuckles_running2_anim_id = -1;
    knuckles_stand_anim_id = -1;
    knuckles_punch_anim_id = -1;
    knuckles_kick_anim_id = -1;
    knuckles_spin_sprite_id = -1;
    knuckles_jump_sprite_id = -1;
    knuckles_damage_sprite_id = -1;
    knuckles_defeated_sprite_id = -1;
    knuckles_loaded = false;
}
