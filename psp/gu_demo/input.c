
#include "input.h"
#include <string.h>

void input_init(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

void input_update(InputState *state) {
    state->buttons_old = state->buttons;
    sceCtrlReadBufferPositive(&state->pad, 1);
    state->buttons = state->pad.Buttons;
    state->pressed = state->buttons & ~state->buttons_old;
}
