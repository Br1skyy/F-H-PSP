/* Input handling module */
#ifndef INPUT_H
#define INPUT_H

#include <pspctrl.h>

/* Input state tracking */
typedef struct {
    unsigned int buttons;
    unsigned int buttons_old;
    unsigned int pressed;   /* newly pressed this frame */
    SceCtrlData pad;
} InputState;

/* Initialize input system */
void input_init(void);

/* Update input state (call once per frame) */
void input_update(InputState *state);

/* Check if button was just pressed */
#define input_pressed(state, btn) ((state)->pressed & (btn))

/* Check if button is held */
#define input_held(state, btn) ((state)->buttons & (btn))

#endif /* INPUT_H */
