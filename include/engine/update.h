#ifndef ENGINE_UPDATE_H_
#define ENGINE_UPDATE_H_

#include <libdragon.h>

typedef struct {
	float dt;
	joypad_buttons_t held, pressed;
	joypad_inputs_t sticks;
} update_parms_t;

#endif /* ENGINE_UPDATE_H_ */
