#include <stdlib.h>

#include "config.h"

#include "engine/object.h"
#include "engine/util.h"
#include "engine/sfx.h"
#include "engine/perspective.h"

#include "game/game.h"
#include "game/office.h"
#include "game/settings.h"
#include "game/texture_index.h"
#include "game/powerdown.h"

#define FREDDY_SCARE_FRAMES 18
static object_t room_views[2];
static const char *room_view_paths[2] = {
	TX_ROOM_POWERDOWN0,
	TX_ROOM_POWERDOWN1,
};

static object_t freddy_scare[FREDDY_SCARE_FRAMES];
static const char *freddy_scare_paths[FREDDY_SCARE_FRAMES] = {
	TX_FREDDY_SCARE_DARK00, TX_FREDDY_SCARE_DARK01, TX_FREDDY_SCARE_DARK02,
	TX_FREDDY_SCARE_DARK03, TX_FREDDY_SCARE_DARK04, TX_FREDDY_SCARE_DARK05,
	TX_FREDDY_SCARE_DARK06, TX_FREDDY_SCARE_DARK07, TX_FREDDY_SCARE_DARK08,
	TX_FREDDY_SCARE_DARK09, TX_FREDDY_SCARE_DARK10, TX_FREDDY_SCARE_DARK11,
	TX_FREDDY_SCARE_DARK12, TX_FREDDY_SCARE_DARK13, TX_FREDDY_SCARE_DARK14,
	TX_FREDDY_SCARE_DARK15, TX_FREDDY_SCARE_DARK16, TX_FREDDY_SCARE_DARK17,
};

static float room_turn;
static bool is_loaded = false;
static float show_up_timer;
static float total_timer;
static int freddy_state;
static int freddy_flicker_val;
static float freddy_flicker_timer;
static float freddy_music_timer;
static float freddy_music_timer_full;
static float shut_down_flicker_timer;
static float shut_down_timer;
static int shut_down_flicker;
static float freddy_scare_rand_timer_loop;
static float freddy_scare_rand_timer;
static float freddy_scare_anim_timer;

static void _power_down_load(void)
{
	if (is_loaded)
		return;

	show_up_timer = 0.0f;
	total_timer = 0.0f;
	room_turn = office_turn;
	freddy_state = 0;
	freddy_flicker_val = 0;
	freddy_flicker_timer = 0.0f;
	freddy_music_timer = 0.0f;
	freddy_music_timer_full = 0.0f;
	freddy_scare_rand_timer_loop = 0.0f;
	freddy_scare_rand_timer = 0.0f;
	freddy_scare_anim_timer = 0.0f;
	shut_down_flicker_timer = 0.0f;
	shut_down_timer = 0.0f;
	shut_down_flicker = 0;
	objects_load(room_views, 2, room_view_paths);
	mixer_ch_set_vol(SFXC_AMBIENCE, 0.6f, 0.6f);
	wav64_play(&powerdown_sfx, SFXC_AMBIENCE);

	is_loaded = true;
}

static void _power_down_unload(void)
{
	if (!is_loaded)
		return;

	objects_unload(room_views, 2);
	objects_unload(freddy_scare, FREDDY_SCARE_FRAMES);

	is_loaded = false;
}

void power_down_draw(void)
{
	_power_down_load();

	perspective_begin();
	rdpq_set_mode_copy(false);

	int state = (freddy_state == 1) * (freddy_flicker_val < 2);
	if ((!shut_down_flicker && freddy_state == 2) || freddy_state >= 3) {
		rdpq_set_mode_fill(RGBA32(0, 0, 0, 0xFF));
		rdpq_fill_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
		perspective_end();

		if (freddy_state == 4) {
			rdpq_set_mode_copy(false);
			int frame = (freddy_scare_anim_timer / 40.0f) *
				    FREDDY_SCARE_FRAMES;
			object_draw(freddy_scare[frame], 0, 0, 0, 0);
		}
		return;
	}

	object_draw(room_views[state], room_turn, 0, 0, 0);
	perspective_end();
}

enum scene power_down_update(update_parms_t uparms)
{
	night_timer += uparms.dt;
	if (night_timer >= 6 * HOUR_LEN_SECONDS) {
		sfx_stop_all();
		rdpq_call_deferred((void (*)(void *))_power_down_unload, NULL);
		return SCENE_NIGHT_END;
	}

	freddy_flicker_timer += uparms.dt;
	bool tick_flicker;
	freddy_flicker_timer =
		wrapf(freddy_flicker_timer, 0.05f, &tick_flicker);
	if (tick_flicker)
		freddy_flicker_val = (rand() % 4) + 1;

	total_timer += uparms.dt;

	switch (freddy_state) {
	case 0:
		show_up_timer += uparms.dt;
		bool trigger_freddy;
		show_up_timer = wrapf(show_up_timer, 5.0f, &trigger_freddy);
		if ((trigger_freddy && (rand() % 5) == 0) ||
		    total_timer >= 20) {
			freddy_state = 1;
			mixer_ch_set_vol(SFXC_MUSICBOX, 0.8f, 0.8f);
			wav64_play(&musicbox_sfx, SFXC_MUSICBOX);
		}
		break;

	case 1:
		freddy_music_timer_full += uparms.dt;
		freddy_music_timer += uparms.dt;
		bool shut_down;
		freddy_music_timer =
			wrapf(freddy_music_timer, 5.0f, &shut_down);
		if ((shut_down && ((rand() % 5) == 0)) ||
		    freddy_music_timer_full >= 20) {
			freddy_state = 2;
			sfx_stop_all();
			wav64_play(&fan_sfx, SFXC_FAN);
			shut_down_timer = 20;
		}
		break;

	case 2:
		shut_down_flicker_timer += uparms.dt * 60;
		bool shut_down_toggle_flicker;
		shut_down_flicker_timer = wrapf(shut_down_flicker_timer, 1.0f,
						&shut_down_toggle_flicker);
		if (shut_down_toggle_flicker)
			shut_down_flicker = rand() & 1;

		float vol = (float)shut_down_flicker * 0.5f;
		mixer_ch_set_vol(SFXC_FAN, vol, vol);

		shut_down_timer -= uparms.dt * 60;
		if (shut_down_timer <= 0) {
			sfx_stop_all();
			freddy_state = 3;
			shut_down_flicker = 0;
			debugf("Load freddy frames\n");
			objects_load(freddy_scare, FREDDY_SCARE_FRAMES,
				     freddy_scare_paths);
		}

		break;

	case 3:
		freddy_scare_rand_timer += uparms.dt;
		freddy_scare_rand_timer_loop += uparms.dt;
		bool try_scare;
		freddy_scare_rand_timer_loop =
			wrapf(freddy_scare_rand_timer_loop, 2.0f, &try_scare);
		if ((try_scare && (rand() & 1)) ||
		    freddy_scare_rand_timer >= 20) {
			freddy_state = 4;
			wav64_play(&jumpscare_sfx, SFXC_JUMPSCARE);
		}
		break;

	case 4:
		freddy_scare_anim_timer += 60 * uparms.dt;
		if (freddy_scare_anim_timer >= 40) {
			sfx_stop_all();
			rdpq_call_deferred((void (*)(void *))_power_down_unload,
					   NULL);
			return SCENE_GAME_OVER;
		}
		break;
	}

	room_turn -= uparms.dt * uparms.sticks.stick_x * ROOM_TURN_SPEED;
	room_turn = clampf(room_turn, ROOM_TURN_MIN, 0);

	if (fabsf(room_turn + 193) < 32 &&
	    (uparms.pressed.a || uparms.pressed.b))
		wav64_play(&boop_sfx, SFXC_BLIP);

	return SCENE_POWER_DOWN;
}
