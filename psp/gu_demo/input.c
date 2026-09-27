
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


#define IN_REP_DELAY 18
#define IN_REP_RATE 4

static int rep_frames[32];

int input_repeat(InputState *state, unsigned int btn) {
    unsigned int held = state->buttons & btn;
    unsigned int edge = state->pressed & btn;
    unsigned int b = btn;
    int bit = 0;
    while (((b & 1u) == 0u) && bit < 31) {
        b >>= 1;
        bit++;
    }
    if (!held) {
        rep_frames[bit] = 0;
        return 0;
    }
    if (edge) {
        rep_frames[bit] = 0;
        return 1;
    }
    rep_frames[bit]++;
    if (rep_frames[bit] >= IN_REP_DELAY &&
        ((rep_frames[bit] - IN_REP_DELAY) % IN_REP_RATE) == 0)
        return 1;
    return 0;
}
