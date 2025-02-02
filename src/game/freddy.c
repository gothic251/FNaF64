#include <stdlib.h>

#include "engine/util.h"
#include "engine/object.h"
#include "engine/sfx.h"

#include "game/camera.h"
#include "game/buttons.h"
#include "game/bonnie.h"
#include "game/chica.h"
#include "game/freddy.h"

#define MOVE_TIMER 3.02f

/* public vars */
int freddy_ai_level = 0;
int freddy_cam_last;
int freddy_cam;
bool freddy_is_jumpscaring;
float freddy_scare_timer;

/* private vars */
static int move_state;
static float opportunity_timer;
static float move_timer;
static bool ready_to_scare;
static float ready_scare_timer;

static const float vol_lut[CAM_COUNT][2] = {
	{ 0.15f, 0.30f }, // 1A
	{ 0.20f, 0.35f }, // 1B
	{ 0.0f, 0.0f }, // 1C
	{ 0.0f, 0.0f }, // 2A
	{ 0.0f, 0.0f }, // 2B
	{ 0.0f, 0.0f }, // 3
	{ 0.60f, 0.75f }, // 4A
	{ 0.80f, 1.00f }, // 4B
	{ 0.0f, 0.0f }, // 5
	{ 0.40f, 0.60f }, // 6
	{ 0.30f, 0.40f } // 7
};

static const int new_cam_lut[CAM_COUNT + 1] = {
	CAM_1B, // 1A
	CAM_7, // 1B
	-1, // 1C
	-1, // 2A
	-1, // 2B
	-1, // 3
	CAM_4B, // 4A
	AT_DOOR, // 4B
	-1, // 5
	CAM_4A, // 6
	CAM_6, // 7
	AT_DOOR, // At Door
};

void freddy_init(void)
{
	freddy_cam_last = CAM_1A;
	freddy_cam = CAM_1A;
	freddy_is_jumpscaring = false;
	freddy_scare_timer = 0.0f;

	move_state = 0;
	opportunity_timer = 0.0f;
	move_timer = 0.0f;
	ready_to_scare = false;
	ready_scare_timer = 0.0f;
}

void freddy_draw_debug(void)
{
	int w = vcon(35);
	int h = w;
	int x = vcon(cam_button_pos[freddy_cam][0] - 317);
	int y = vcon(cam_button_pos[freddy_cam][1]);
	int x0 = x;
	int y0 = y - (h >> 1);
	int x1 = x + w;
	int y1 = y + (h >> 1);

	rdpq_set_mode_fill(RGBA32(0xB7, 0x67, 0x43, 0xFF));
	rdpq_fill_rectangle(x0, y0, x1, y1);
}

static void _freddy_handle_music_box(void)
{
	mixer_ch_set_vol(SFXC_MUSICBOX, 0, 0);
	if (freddy_cam != CAM_6)
		return;

	float vol = (camera_is_visible && (cam_selected == CAM_6)) ? 0.5f :
								     0.05f;
	mixer_ch_set_vol(SFXC_MUSICBOX, vol, vol);
}

void freddy_update(double dt)
{
	_freddy_handle_music_box();

	if (ready_to_scare) {
		ready_scare_timer += dt;
		bool try_scare;
		ready_scare_timer = wrapf(ready_scare_timer, 1, &try_scare);
		if (try_scare && ((rand() % 4) == 0)) {
			freddy_is_jumpscaring = true;
			ready_to_scare = false;
			wav64_play(&jumpscare_sfx, SFXC_JUMPSCARE);
			return;
		}
		return;
	}

	if (freddy_is_jumpscaring) {
		freddy_scare_timer += dt * speed_fps(25);
		freddy_scare_timer =
			clampf(freddy_scare_timer, 0, FREDDY_SCARE_FRAMES);
		return;
	}

	if (freddy_cam == AT_DOOR) {
		ready_to_scare = true;
		return;
	}

	if (move_state == 1)
		move_timer += dt * 60;

	opportunity_timer += dt;
	bool try_move;
	opportunity_timer = wrapf(opportunity_timer, MOVE_TIMER, &try_move);
	if (camera_is_visible && freddy_cam != CAM_4B) {
		if (cam_selected == freddy_cam) {
			move_timer = 0;
			return;
		}

		return;
	}

	if (move_timer >= 1000 - (freddy_ai_level * 100) && move_state == 1) {
		move_timer = 0;
		move_state = 2;
	}

	if (try_move && (1 + (rand() % 20)) <= freddy_ai_level) {
		move_state = 1;
		return;
	}

	if (move_state != 2)
		return;

	if (freddy_cam == CAM_4B) {
		if (!camera_is_visible) {
			move_timer = 0;
			return;
		} else {
			if (freddy_cam == cam_selected) {
				move_timer = 0;
				return;
			}
		}
	}

	/* Don't move Freddy while Bonnie and Chica are on stage */
	if (((camera_states[CAM_1A] & BONNIE_BIT) ||
	     (camera_states[CAM_1A] & CHICA_BIT)) &&
	    freddy_cam == CAM_1A) {
		return;
	}

	freddy_cam_last = freddy_cam;
	int cam_next = new_cam_lut[freddy_cam];
	bool right_door_closed = button_state & BUTTON_RIGHT_DOOR;
	if (freddy_cam == CAM_4B && camera_is_visible)
		if (right_door_closed)
			cam_next = CAM_4A;

	move_state = 0;

	if (cam_next < AT_DOOR) {
		float laugh_vol = vol_lut[cam_next][0];
		float foot_vol = vol_lut[cam_next][1];
		mixer_ch_set_vol(SFXC_FREDDYLAUGH, laugh_vol, laugh_vol);
		mixer_ch_set_vol(SFXC_FREDDYRUN, foot_vol, foot_vol);
	}

	switch (rand() % 3) {
	case 0:
		wav64_play(&freddylaugh1, SFXC_FREDDYLAUGH);
		break;

	case 1:
		wav64_play(&freddylaugh2, SFXC_FREDDYLAUGH);
		break;

	case 2:
		wav64_play(&freddylaugh3, SFXC_FREDDYLAUGH);
		break;
	}

	wav64_play(&freddyrun_sfx, SFXC_FREDDYRUN);

	if (cam_next == CAM_6)
		wav64_play(&musicbox_sfx, SFXC_MUSICBOX);
	else
		mixer_ch_stop(SFXC_MUSICBOX);

	freddy_cam = cam_next;

	/* I have no fucking clue why I have to do this */
	if (freddy_cam_last < AT_DOOR) {
		camera_states[freddy_cam_last] &= ~FREDDY_BIT;
		if (freddy_cam < AT_DOOR)
			camera_states[freddy_cam] |= FREDDY_BIT;
	}
}
