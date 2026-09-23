
#ifndef INPUT_H
#define INPUT_H

#include <pspctrl.h>


typedef struct {
    unsigned int buttons;
    unsigned int buttons_old;
    unsigned int pressed;
    SceCtrlData pad;
} InputState;


void input_init(void);


void input_update(InputState *state);


#define input_pressed(state, btn) ((state)->pressed & (btn))


#define input_held(state, btn) ((state)->buttons & (btn))

#endif
