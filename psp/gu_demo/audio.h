#ifndef FH_AUDIO_H
#define FH_AUDIO_H

#include "../../runtime/interp.h"

void audio_init(void);
void audio_poll(FhInterp *it);
void audio_play_se(const char *name, int vol, int pitch, int pan);
void audio_play_bgm(const char *name, int vol, int pitch);
void audio_play_bgs(const char *name, int vol, int pitch, int pan);
void audio_play_me(const char *name, int vol, int pitch, int pan);
void audio_fade_bgm(int frames);
void audio_fade_bgs(int frames);
void audio_stop_se(void);
void audio_stop_bgs(void);
void audio_stop_me(void);
void audio_push_bgm(void);
void audio_pop_bgm(void);
void audio_push_bgs(void);
void audio_pop_bgs(void);
void audio_forget(FhInterp *it);
void audio_set_master(int vol);
void audio_set_bus(int bus, int vol);
void audio_get_vol(int out[5]);
void audio_status(char *out, int cap);

#endif
