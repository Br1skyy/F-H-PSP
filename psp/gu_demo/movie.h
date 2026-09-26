#ifndef FH_MOVIE_H
#define FH_MOVIE_H

/* Blocking fullscreen movie player (PSMF video, no audio track).
 * 0 finished, 1 skipped by the player, <0 on error. Audio rides the
 * music engine when it lands; pass the video path only. */
int movie_play(const char *path);

#endif
