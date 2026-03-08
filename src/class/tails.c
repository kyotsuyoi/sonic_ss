#include <jo/jo.h>
#include "sonic.h"
#include "tails.h"

#define character_ref player
extern jo_sidescroller_physics_params physics;
#define SPRITE_DIR "SPT"
#define DEFEATED_SPRITE_WIDTH 40
#define DEFEATED_SPRITE_HEIGHT 32
#define TAILS_HIT_RANGE_PUNCH1 10
#define TAILS_HIT_RANGE_PUNCH2 11
#define TAILS_HIT_RANGE_KICK1 11
#define TAILS_HIT_RANGE_KICK2 12
#define TAILS_ATTACK_FORWARD_IMPULSE_LIGHT 0.60f
#define TAILS_ATTACK_FORWARD_IMPULSE_HEAVY 1.10f
#define TAILS_KNOCKBACK_PUNCH1 1.8f
#define TAILS_KNOCKBACK_PUNCH2 2.3f
#define TAILS_KNOCKBACK_KICK1 1.8f
#define TAILS_KNOCKBACK_KICK2 2.6f
#define TAILS_KICK1_ROTATION_TIME 16
#define TAILS_KICK2_ROTATION_TIME 32
#define TAILS_TAIL_FRAME_COUNT 4
#define TAILS_TAIL_FRAME_DURATION 5
#define TAILS_TAIL_OFFSET_X 14
#define TAILS_TAIL_Z CHARACTER_SPRITE_Z

static bool tails_loaded = false;
static int tails_walking_base_id;
static int tails_running1_base_id;
static int tails_running2_base_id;
static int tails_stand_base_id;
static int tails_punch_base_id;
static int tails_kick_base_id;
static int tails_walking_anim_id;
static int tails_running1_anim_id;
static int tails_running2_anim_id;
static int tails_stand_anim_id;
static int tails_punch_anim_id;
static int tails_kick_anim_id;
static int tails_spin_sprite_id;
static int tails_jump_sprite_id;
static int tails_kick_sprite_id;
static int tails_damage_sprite_id;
static int tails_defeated_sprite_id;
static int tails_tail_base_id;
static int tails_kick_timer = 0;
static int tails_kick_duration = 0;
static int tails_kick_total_degrees = 0;
static bool tails_kick_rotation_active = false;
static int tails_tail_frame = 0;
static int tails_tail_timer = 0;

static const jo_tile TailsWalkingTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
//     {CHARACTER_WIDTH * 5, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
//     {CHARACTER_WIDTH * 6, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
//     {CHARACTER_WIDTH * 7, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
 };

static const jo_tile TailsRunning1Tiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 5, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 6, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 7, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

// static const jo_tile TailsRunning2Tiles[] =
// {
//     {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
//     {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
//     {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
//     {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
//     {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
//     {CHARACTER_WIDTH * 5, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
//     {CHARACTER_WIDTH * 6, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
//     {CHARACTER_WIDTH * 7, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
// };

static const jo_tile TailsStandTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile TailsPunchTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile TailsKickTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 4, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 5, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 6, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 7, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 8, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 9, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 10, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 11, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    // {CHARACTER_WIDTH * 12, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
};

static const jo_tile TailsTailTiles[] =
{
    {CHARACTER_WIDTH * 0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 1, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 2, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT},
    {CHARACTER_WIDTH * 3, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT}
};

static const jo_tile TailsDefeatedTile[] =
{
    {0, 0, DEFEATED_SPRITE_WIDTH, DEFEATED_SPRITE_HEIGHT},
};

static void tails_update_tail_loop(void)
{
    ++tails_tail_timer;
    if (tails_tail_timer >= TAILS_TAIL_FRAME_DURATION)
    {
        tails_tail_timer = 0;
        tails_tail_frame = (tails_tail_frame + 1) % TAILS_TAIL_FRAME_COUNT;
    }
}

static bool tails_should_draw_tail(void)
{
    if (tails_tail_base_id < 0)
        return false;
    if (character_ref.spin)
        return false;
    if (character_ref.life <= 0)
        return false;
    if (character_ref.stun_timer > 0)
        return false;
    if (character_ref.kick2)
        return false;
    if (tails_kick_rotation_active)
        return false;

    return true;
}

static void tails_draw_tail(void)
{
    int tail_x;

    if (!tails_should_draw_tail())
        return;

    tail_x = character_ref.x + (character_ref.flip ? TAILS_TAIL_OFFSET_X : -TAILS_TAIL_OFFSET_X);
    jo_sprite_draw3D2(tails_tail_base_id + tails_tail_frame, tail_x, character_ref.y, TAILS_TAIL_Z);
}

static void tails_reset_animation_lists_except(int active_anim_id)
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

void tails_running_animation_handling(void)
{
    int speed_step;

    tails_update_tail_loop();

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

    {
        const int punch_frame_count = JO_TILE_COUNT(TailsPunchTiles);
        const bool has_punch2_stage = (punch_frame_count >= 5);
        const int punch1_last_frame = (punch_frame_count >= 5) ? 4 : (punch_frame_count - 1);
        const int punch2_start_frame = has_punch2_stage ? 0 : -1;
        const int punch2_last_frame = punch1_last_frame;

        if (character_ref.punch) {
            int anim_frame = jo_get_sprite_anim_frame(character_ref.punch_anim_id);
            bool anim_stopped = jo_is_sprite_anim_stopped(character_ref.punch_anim_id);

            if (anim_frame > punch1_last_frame || anim_stopped) {
                jo_set_sprite_anim_frame(character_ref.punch_anim_id, 0);
                jo_start_sprite_anim(character_ref.punch_anim_id);
                anim_frame = jo_get_sprite_anim_frame(character_ref.punch_anim_id);
                anim_stopped = false;
            }

            if (anim_frame >= punch1_last_frame) {
                if (character_ref.punch2_requested && has_punch2_stage) {
                    character_ref.punch = false;
                    character_ref.punch2 = true;
                    character_ref.punch2_requested = false;
                    character_ref.perform_punch2 = true;
                    jo_set_sprite_anim_frame(character_ref.punch_anim_id, punch2_start_frame);
                    jo_start_sprite_anim(character_ref.punch_anim_id);
                } else if (anim_stopped) {
                    character_ref.punch = false;
                    character_ref.punch2_requested = false;
                    character_ref.attack_cooldown = ATTACK_COOLDOWN_FRAMES;
                    jo_reset_sprite_anim(character_ref.punch_anim_id);
                }
            }
        } else if (character_ref.punch2) {
            int anim_frame = jo_get_sprite_anim_frame(character_ref.punch_anim_id);

            if (!has_punch2_stage) {
                character_ref.punch2 = false;
                character_ref.punch2_requested = false;
                character_ref.attack_cooldown = ATTACK_COOLDOWN_PUNCH2_FRAMES;
                jo_reset_sprite_anim(character_ref.punch_anim_id);
            } else {
                if (anim_frame < punch2_start_frame) {
                    jo_set_sprite_anim_frame(character_ref.punch_anim_id, punch2_start_frame);
                    jo_start_sprite_anim(character_ref.punch_anim_id);
                }
                if (anim_frame >= punch2_last_frame && jo_is_sprite_anim_stopped(character_ref.punch_anim_id)) {
                    character_ref.punch2 = false;
                    character_ref.attack_cooldown = ATTACK_COOLDOWN_PUNCH2_FRAMES;
                    jo_reset_sprite_anim(character_ref.punch_anim_id);
                }
            }
        }
    }

    if (character_ref.kick)
    {
        if (!tails_kick_rotation_active || tails_kick_total_degrees != 180)
        {
            tails_kick_timer = 0;
            tails_kick_duration = TAILS_KICK1_ROTATION_TIME;
            tails_kick_total_degrees = 180;
            tails_kick_rotation_active = true;
        }

        if (tails_kick_timer < tails_kick_duration)
            ++tails_kick_timer;

        if (tails_kick_timer >= tails_kick_duration)
        {
            if (character_ref.kick2_requested)
            {
                character_ref.kick = false;
                character_ref.kick2 = true;
                character_ref.kick2_requested = false;
                character_ref.perform_kick2 = true;

                tails_kick_timer = 0;
                tails_kick_duration = TAILS_KICK2_ROTATION_TIME;
                tails_kick_total_degrees = 360;
                tails_kick_rotation_active = true;
            }
            else
            {
                character_ref.kick = false;
                character_ref.attack_cooldown = ATTACK_COOLDOWN_FRAMES;
                tails_kick_rotation_active = false;
            }
        }
    }
    else if (character_ref.kick2)
    {
        if (!tails_kick_rotation_active || tails_kick_total_degrees != 360)
        {
            tails_kick_timer = 0;
            tails_kick_duration = TAILS_KICK2_ROTATION_TIME;
            tails_kick_total_degrees = 360;
            tails_kick_rotation_active = true;
        }

        if (tails_kick_timer < tails_kick_duration)
            ++tails_kick_timer;

        if (tails_kick_timer >= tails_kick_duration)
        {
            character_ref.kick2 = false;
            character_ref.attack_cooldown = ATTACK_COOLDOWN_KICK2_FRAMES;
            tails_kick_rotation_active = false;
        }
    }
    else if (tails_kick_rotation_active)
    {
        if (tails_kick_timer < tails_kick_duration)
            ++tails_kick_timer;
        if (tails_kick_timer >= tails_kick_duration)
            tails_kick_rotation_active = false;
    }

    if (!tails_kick_rotation_active && !character_ref.kick && !character_ref.kick2)
    {
        tails_kick_timer = 0;
        tails_kick_duration = 0;
        tails_kick_total_degrees = 0;
    }
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

    tails_draw_tail();

    if (character_ref.spin)
    {
        tails_reset_animation_lists_except(-1);
        jo_sprite_draw3D_and_rotate2(character_ref.spin_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z, character_ref.angle);
        if (character_ref.flip)
            character_ref.angle -= CHARACTER_SPIN_SPEED;
        else
            character_ref.angle += CHARACTER_SPIN_SPEED;
    } else if (character_ref.life <= 0 && tails_defeated_sprite_id >= 0) {
        tails_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(tails_defeated_sprite_id, character_ref.x, character_ref.y + (CHARACTER_HEIGHT - DEFEATED_SPRITE_HEIGHT), CHARACTER_SPRITE_Z);
    } else if (character_ref.stun_timer > 0 && tails_damage_sprite_id >= 0) {
        tails_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(tails_damage_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else if (character_ref.punch || character_ref.punch2){
        tails_reset_animation_lists_except(character_ref.punch_anim_id);
        jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.punch_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else if (character_ref.kick || character_ref.kick2 || tails_kick_rotation_active){
        int kick_angle = 0;
        tails_reset_animation_lists_except(-1);
        if (tails_kick_duration > 0)
            kick_angle = (tails_kick_total_degrees * tails_kick_timer) / tails_kick_duration;
        if (character_ref.flip)
            kick_angle = -kick_angle;
        jo_sprite_draw3D_and_rotate2(tails_kick_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z, kick_angle);
    } else if (character_ref.jump){
        tails_reset_animation_lists_except(-1);
        jo_sprite_draw3D2(character_ref.jump_sprite_id, character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
    } else {
        if (character_ref.walk && character_ref.run == 0)
        {
            tails_reset_animation_lists_except(character_ref.walking_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.walking_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 1)
        {
            tails_reset_animation_lists_except(character_ref.running1_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.running1_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else if (character_ref.walk && character_ref.run == 2)
        {
            tails_reset_animation_lists_except(character_ref.running2_anim_id);
            jo_sprite_draw3D2(jo_get_anim_sprite(character_ref.running2_anim_id), character_ref.x, character_ref.y, CHARACTER_SPRITE_Z);
        }
        else
        {
            tails_reset_animation_lists_except(character_ref.stand_sprite_id);
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

void load_tails(void)
{
    if (!tails_loaded)
    {
        tails_walking_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "TLS_WLK.TGA", JO_COLOR_Green, TailsWalkingTiles, JO_TILE_COUNT(TailsWalkingTiles));
        tails_walking_anim_id = jo_create_sprite_anim(tails_walking_base_id, JO_TILE_COUNT(TailsWalkingTiles), 4);

        tails_running1_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "TLS_RUN1.TGA", JO_COLOR_Green, TailsRunning1Tiles, JO_TILE_COUNT(TailsRunning1Tiles));
        tails_running1_anim_id = jo_create_sprite_anim(tails_running1_base_id, JO_TILE_COUNT(TailsRunning1Tiles), 4);

        // We don't have TLS_RUN2.TGA, reuse TLS_RUN1.TGA for the second running level
        tails_running2_base_id = tails_running1_base_id;
        tails_running2_anim_id = jo_create_sprite_anim(tails_running2_base_id, JO_TILE_COUNT(TailsRunning1Tiles), 4);

        tails_stand_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "TLS_STD.TGA", JO_COLOR_Green, TailsStandTiles, JO_TILE_COUNT(TailsStandTiles));
        tails_stand_anim_id = jo_create_sprite_anim(tails_stand_base_id, JO_TILE_COUNT(TailsStandTiles), 4);

        tails_spin_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "TLS_SPN.TGA", JO_COLOR_Green);
        tails_jump_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "TLS_JMP.TGA", JO_COLOR_Green);
        tails_kick_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "TLS_KCK.TGA", JO_COLOR_Green);
        tails_damage_sprite_id = jo_sprite_add_tga(SPRITE_DIR, "TLS_DMG.TGA", JO_COLOR_Green);
        tails_defeated_sprite_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "TLS_DFT.TGA", JO_COLOR_Green, TailsDefeatedTile, JO_TILE_COUNT(TailsDefeatedTile));
        tails_tail_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "TLS_TLS.TGA", JO_COLOR_Green, TailsTailTiles, JO_TILE_COUNT(TailsTailTiles));

        tails_punch_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "TLS_PNC.TGA", JO_COLOR_Green, TailsPunchTiles, JO_TILE_COUNT(TailsPunchTiles));
        tails_punch_anim_id = jo_create_sprite_anim(tails_punch_base_id, JO_TILE_COUNT(TailsPunchTiles), 4);

        tails_kick_base_id = jo_sprite_add_tga_tileset(SPRITE_DIR, "TLS_KCK.TGA", JO_COLOR_Green, TailsKickTiles, JO_TILE_COUNT(TailsKickTiles));
        tails_kick_anim_id = jo_create_sprite_anim(tails_kick_base_id, JO_TILE_COUNT(TailsKickTiles), 4);

        tails_loaded = true;
    }

    character_ref.walking_anim_id = tails_walking_anim_id;
    character_ref.running1_anim_id = tails_running1_anim_id;
    character_ref.running2_anim_id = tails_running2_anim_id;
    character_ref.stand_sprite_id = tails_stand_anim_id;
    character_ref.spin_sprite_id = tails_spin_sprite_id;
    character_ref.jump_sprite_id = tails_jump_sprite_id;
    character_ref.damage_sprite_id = tails_damage_sprite_id;
    character_ref.defeated_sprite_id = tails_defeated_sprite_id;
    character_ref.punch_anim_id = tails_punch_anim_id;
    character_ref.kick_anim_id = tails_kick_anim_id;
    character_ref.hit_range_punch1 = TAILS_HIT_RANGE_PUNCH1;
    character_ref.hit_range_punch2 = TAILS_HIT_RANGE_PUNCH2;
    character_ref.hit_range_kick1 = TAILS_HIT_RANGE_KICK1;
    character_ref.hit_range_kick2 = TAILS_HIT_RANGE_KICK2;
    character_ref.attack_forward_impulse_light = TAILS_ATTACK_FORWARD_IMPULSE_LIGHT;
    character_ref.attack_forward_impulse_heavy = TAILS_ATTACK_FORWARD_IMPULSE_HEAVY;
    character_ref.knockback_punch1 = TAILS_KNOCKBACK_PUNCH1;
    character_ref.knockback_punch2 = TAILS_KNOCKBACK_PUNCH2;
    character_ref.knockback_kick1 = TAILS_KNOCKBACK_KICK1;
    character_ref.knockback_kick2 = TAILS_KNOCKBACK_KICK2;
    character_ref.attack_cooldown = 0;
    tails_tail_frame = 0;
    tails_tail_timer = 0;

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

    tails_walking_base_id = -1;
    tails_running1_base_id = -1;
    tails_running2_base_id = -1;
    tails_stand_base_id = -1;
    tails_punch_base_id = -1;
    tails_kick_base_id = -1;
    tails_walking_anim_id = -1;
    tails_running1_anim_id = -1;
    tails_running2_anim_id = -1;
    tails_stand_anim_id = -1;
    tails_punch_anim_id = -1;
    tails_kick_anim_id = -1;
    tails_spin_sprite_id = -1;
    tails_jump_sprite_id = -1;
    tails_kick_sprite_id = -1;
    tails_damage_sprite_id = -1;
    tails_defeated_sprite_id = -1;
    tails_tail_base_id = -1;
    tails_tail_frame = 0;
    tails_tail_timer = 0;
    tails_loaded = false;
}

int tails_get_tail_base_id(void)
{
    return tails_tail_base_id;
}

int tails_get_kick_sprite_id(void)
{
    return tails_kick_sprite_id;
}
