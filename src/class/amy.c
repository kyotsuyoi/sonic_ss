#include <jo/jo.h>
#include "sonic.h"
#include "amy.h"
#include "player.h"
#include "game_constants.h"
#include "character_registry.h"
#include "sprite_safe.h"

#define character_ref player
extern jo_sidescroller_physics_params physics;
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

static const jo_tile AmyWalkingTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyRunning1Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyRunning2Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyStandTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyPunchTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyKickTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile AmyDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

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

    if (character_ref.spin)
    {
        amy_reset_animation_lists_except(-1);
        jo_sprite_draw3D_and_rotate2(character_ref.spin_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z, character_ref.angle);
        if (character_ref.flip)
            character_ref.angle -= CHARACTER_SPIN_SPEED;
        else
            character_ref.angle += CHARACTER_SPIN_SPEED;
    } else if (character_ref.life <= 0 && amy_defeated_sprite_id >= 0) {
        amy_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(amy_defeated_sprite_id, character_ref.x, character_ref.y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
    } else if (character_ref.stun_timer > 0 && amy_damage_sprite_id >= 0) {
        amy_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(amy_damage_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else if (character_ref.punch || character_ref.punch2){
        amy_reset_animation_lists_except(character_ref.punch_anim_id);
        jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.punch_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else if (character_ref.kick || character_ref.kick2){
        amy_reset_animation_lists_except(character_ref.kick_anim_id);
        jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.kick_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else if (character_ref.jump){
        amy_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(character_ref.jump_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else {
        if (character_ref.walk && character_ref.run == 0)
        {
            amy_reset_animation_lists_except(character_ref.walking_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.walking_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 1)
        {
            amy_reset_animation_lists_except(character_ref.running1_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.running1_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 2)
        {
            amy_reset_animation_lists_except(character_ref.running2_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.running2_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else
        {
            amy_reset_animation_lists_except(character_ref.stand_sprite_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.stand_sprite_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
    }

    if (character_ref.flip)
        jo_sprite_disable_horizontal_flip();

    int life_percent = (character_ref.life * 100) / 50;
    int bar_max_width = 20;
    int bar_width = (life_percent * bar_max_width) / 100;
    char bar[bar_max_width + 1];
    for (int i = 0; i < bar_max_width; ++i)
        bar[i] = (i < bar_width) ? '#' : '-';
    bar[bar_max_width] = '\0';
    jo_printf(1, 26, "P1 : [%s] %d%%", bar, life_percent);
}

void load_amy(void)
{
    if (!amy_loaded)
    {
amy_walking_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "AMY_WLK.TGA", JO_COLOR_Green, AmyWalkingTiles, JO_TILE_COUNT(AmyWalkingTiles));
        amy_walking_anim_id = jo_create_sprite_anim(amy_walking_base_id, JO_TILE_COUNT(AmyWalkingTiles), DEFAULT_SPRITE_FRAME_DURATION);

        amy_running1_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "AMY_RUN1.TGA", JO_COLOR_Green, AmyRunning1Tiles, JO_TILE_COUNT(AmyRunning1Tiles));
        amy_running1_anim_id = jo_create_sprite_anim(amy_running1_base_id, JO_TILE_COUNT(AmyRunning1Tiles), DEFAULT_SPRITE_FRAME_DURATION);

        amy_running2_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "AMY_RUN2.TGA", JO_COLOR_Green, AmyRunning2Tiles, JO_TILE_COUNT(AmyRunning2Tiles));
        amy_running2_anim_id = jo_create_sprite_anim(amy_running2_base_id, JO_TILE_COUNT(AmyRunning2Tiles), DEFAULT_SPRITE_FRAME_DURATION);

        amy_stand_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "AMY_STD.TGA", JO_COLOR_Green, AmyStandTiles, JO_TILE_COUNT(AmyStandTiles));
        amy_stand_anim_id = jo_create_sprite_anim(amy_stand_base_id, JO_TILE_COUNT(AmyStandTiles), DEFAULT_SPRITE_FRAME_DURATION);

        amy_spin_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "AMY_SPN.TGA", JO_COLOR_Green);
        amy_jump_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "AMY_JMP.TGA", JO_COLOR_Green);
        amy_damage_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "AMY_DMG.TGA", JO_COLOR_Green);
        amy_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "AMY_DFT.TGA", JO_COLOR_Green, AmyDefeatedTile, JO_TILE_COUNT(AmyDefeatedTile));

        amy_punch_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "AMY_PNC.TGA", JO_COLOR_Green, AmyPunchTiles, JO_TILE_COUNT(AmyPunchTiles));
        amy_punch_anim_id = jo_create_sprite_anim(amy_punch_base_id, JO_TILE_COUNT(AmyPunchTiles), DEFAULT_SPRITE_FRAME_DURATION);

        amy_kick_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "AMY_KCK.TGA", JO_COLOR_Green, AmyKickTiles, JO_TILE_COUNT(AmyKickTiles));
        amy_kick_anim_id = jo_create_sprite_anim(amy_kick_base_id, JO_TILE_COUNT(AmyKickTiles), DEFAULT_SPRITE_FRAME_DURATION);

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

    amy_walking_base_id = -1;
    amy_running1_base_id = -1;
    amy_running2_base_id = -1;
    amy_stand_base_id = -1;
    amy_punch_base_id = -1;
    amy_kick_base_id = -1;
    amy_walking_anim_id = -1;
    amy_running1_anim_id = -1;
    amy_running2_anim_id = -1;
    amy_stand_anim_id = -1;
    amy_punch_anim_id = -1;
    amy_kick_anim_id = -1;
    amy_spin_sprite_id = -1;
    amy_jump_sprite_id = -1;
    amy_damage_sprite_id = -1;
    amy_defeated_sprite_id = -1;
    amy_loaded = false;
}
