#include "config.h"

#include "engine/sfx.h"
#include "engine/object.h"
#include "engine/util.h"

#include "game/game.h"
#include "game/freddy.h"
#include "game/bonnie.h"
#include "game/chica.h"
#include "game/foxy.h"
#include "game/settings.h"
#include "game/texture_index.h"
#include "game/night_end.h"

static object_t am, six, five;
static float timer;
static bool is_loaded = false;
static bool played_cheer;

static void _night_end_load(void)
{
	if (is_loaded)
		return;

	timer = 0.0f;
	save_data |= (NIGHT_NUM == 5) * NIGHT_5_BEATEN_BIT;
	save_data |= (NIGHT_NUM == 6) * NIGHT_6_BEATEN_BIT;

	if (freddy_ai_level == 20 && bonnie_ai_level == 20 &&
	    chica_ai_level == 20 && foxy_ai_level == 20 &&
	    !(settings_flags & SET_ROBOT_CHEAT_BIT) &&
	    !(settings_flags & SET_FAST_NIGHT_BIT))
		save_data |= (NIGHT_NUM == 7) * MODE_20_BEATEN_BIT;

	save_data++;
	mixer_ch_set_vol(SFXC_AMBIENCE, 0.8f, 0.8f);
	wav64_play(&chimes_sfx, SFXC_AMBIENCE);

	object_load(&am, TX_END_AM);
	object_load(&six, TX_END_SIX);
	object_load(&five, TX_END_FIVE);

	played_cheer = false;
	is_loaded = true;
}

static void _night_end_unload(void)
{
	if (!is_loaded)
		return;
	object_unload(&six);
	object_unload(&five);
	object_unload(&am);
	is_loaded = false;
}

void night_end_draw(void)
{
	_night_end_load();

	rdpq_set_mode_fill(RGBA32(0, 0, 0, 0xFF));
	rdpq_fill_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

	rdpq_set_mode_standard();

	int five_start = 298;
	int five_end = 298 - 110;
	int six_start = 298 + 110;
	int six_end = 298;
	float t = clampf((timer - 1) * 0.2f, 0, 1);

	object_draw(six, 390, lerpf(six_start, six_end, t), -5, 0);
	object_draw(five, 390, lerpf(five_start, five_end, t), -5, 0);
	object_draw(am, 486, 296, -5, 0);

	rdpq_set_mode_fill(RGBA32(0x0, 0x0, 0x0, 0xFF));
	rdpq_fill_rectangle(vcon(339), vcon(168), vcon(339) + vcon(159),
			    vcon(168) + vcon(120));
	rdpq_fill_rectangle(vcon(339), vcon(384), vcon(339) + vcon(159),
			    vcon(384) + vcon(120));

	float fade;
	if (timer < 9)
		fade = clampf(1.0f - timer, 0, 1);
	else
		fade = clampf(timer - 10.5f, 0, 1);

	rdpq_set_mode_standard();
	rdpq_set_prim_color(RGBA32(0x0, 0x0, 0x0, fade * 255));
	rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
	rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
	rdpq_fill_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

enum scene night_end_update(update_parms_t uparms)
{
	timer += uparms.dt;

	if (timer >= 6.2f && !played_cheer) {
		played_cheer = true;
		mixer_ch_set_vol(SFXC_FAN, 0.8f, 0.8f);
		wav64_play(&cheering_sfx, SFXC_FAN);
	}

	if (timer >= 11.5f) {
		if (!eeprom_failed)
			eepfs_write("fnaf.dat", &save_data, 1);
		debugf("Saved night %d and %d%d%d to save file.\n", NIGHT_NUM,
		       (save_data & NIGHT_5_BEATEN_BIT) > 0,
		       (save_data & NIGHT_6_BEATEN_BIT) > 0,
		       (save_data & MODE_20_BEATEN_BIT) > 0);
		rdpq_call_deferred((void (*)(void *))_night_end_unload, NULL);
		sfx_stop_all();

		if (NIGHT_NUM < 6)
			return SCENE_WHICH_NIGHT;

		return SCENE_PAYCHECK;
	}

	return SCENE_NIGHT_END;
}
