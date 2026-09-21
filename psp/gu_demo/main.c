/* F&H PSP Port - Main Game Loop
 * 
 * Clean, modular implementation with rendering and input separated.
 * Controls: D-pad=move, L/R=change character, START=exit, SELECT=message demo
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <psprtc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "render.h"
#include "input.h"
#include "map_runtime.h"
#include "interp_rt.h"
#include "text_rt.h"
#include "../../runtime/player.h"
#include "event_demo.h"

PSP_MODULE_INFO("F&H port", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(1024);  /* nothing mallocs at runtime (all static); keep the
                           footprint small so real hardware boots (24MB user
                           partition: .data 2.4MB + .bss 1.7MB + heap) */

/* --- Message Demo (temporary) --- */
static FhInterp mit;
static int msg_mode = 0, msg_ended = 0, msg_cursor = 0;
static const FhCmd *msg_list = NULL;
static int msg_len = 0;
static char msg_title[64] = "";

static void msg_open(const FhCmd *list, int len, const char *title) {
    msg_list = list;
    msg_len = len;
    strncpy(msg_title, title, sizeof(msg_title) - 1);
    msg_title[sizeof(msg_title) - 1] = 0;
    fh_interp_init(&mit, msg_list, msg_len);
    msg_mode = 1;
    msg_ended = 0;
    msg_cursor = 0;
}

static void msg_print_text(const char *ptr, int len, void *ud) {
    (void)ud;
    pspDebugScreenPrintf("%.*s", len, ptr);
}

static void msg_print_code(const char *code, int param, void *ud) {
    (void)ud; (void)code; (void)param;
}

static void msg_show(void) {
    FhEscCtx ctx = {NULL, 0, NULL, 0, NULL, 0, NULL};
    FhEscCb cb = {&msg_print_text, &msg_print_code, NULL};
    pspDebugScreenSetXY(0, 3);
    fh_decode_escapes(mit.text, &ctx, &cb);
    pspDebugScreenPrintf("\n");
    if (mit.await_choice && mit.choice_text) {
        char tmp[512];
        strncpy(tmp, mit.choice_text, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        char *line;
        int i = 0;
        for (line = strtok(tmp, "\n"); line; line = strtok(NULL, "\n"), i++)
            pspDebugScreenPrintf("  %c %s\n", i == msg_cursor ? '>' : ' ', line);
        pspDebugScreenPrintf("UP/DN+Cross (Circle=cancel)\n");
    } else if (msg_ended) {
        pspDebugScreenPrintf("-- END (item21=%d SELECT=map) --\n", mit.inv_item[21]);
    }
}

/* --- Exit Callback --- */
static int exit_request = 0;

static int exitCallback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    exit_request = 1;
    return 0;
}

static int callbackThread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setupExitCallback(void) {
    int thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

/* --- Game State --- */
#define MAP_W 145
#define MAP_H 105

static unsigned int gu_list[262144] __attribute__((aligned(16)));
static uint16_t map_layers[4][MAP_H][MAP_W];
static uint8_t map_passability[MAP_H][MAP_W];
static uint8_t map_higher[1664];  /* higher-tile mask (flags[tid] & 0x10) */
static Player player;
static int current_character = 0;

typedef struct {
    const char *name;
    unsigned char *sprite_data;
    unsigned char *clut_data;
} CharacterDef;

static CharacterDef characters[4];

/* Embedded data */
extern unsigned char d_a1_start[], d_b_start[], d_e_start[], d_ib_start[], d_d_start[];
extern unsigned char d_a1c_start[], d_bc_start[], d_ec_start[], d_ibc_start[], d_dc_start[];
extern unsigned char d_layers_start[], d_pass_start[], d_high_start[];
extern unsigned char d_merc_start[], d_mercc_start[];
extern unsigned char d_outl_start[], d_outlc_start[];
extern unsigned char d_priest_start[], d_priestc_start[];
extern unsigned char d_knight_start[], d_knightc_start[];
extern unsigned char d_flame_start[], d_flamec_start[];
extern unsigned char d_creat_start[], d_creatc_start[];
extern unsigned char d_mobj_start[], d_mobjc_start[];
extern unsigned char d_ghost_start[], d_ghostc_start[];

/* NPC sheets: t8/clut pointers, img_w/h active px, tex/stride upload dims.
 * Dims from converted meta.json; see docs/engine-notes.md ($ = single). */
static struct { unsigned char *t8, *clut; int iw, ih, tw, th, stride, big; } npc_sheets[4];

/* NPCs baked from Map030.json pg0 (id, sheet, tile, index, pattern,
 * dir_mv, trigger, priority, dirfix, talk list or NULL).
 * trigger 0 = action button; prio 0 = underfoot (Here), 1 = faced (There)
 * per Game_Player.triggerButtonAction (rpg_objects.js). */
typedef struct {
    int ev_id, sheet, tx, ty, index, pattern, dir_mv, trig, prio, dirfix;
    const FhCmd *talk;
    int talk_len;
} NpcDef;

static NpcDef npcs[15];
static NpcSprite npc_draw[15];

static void load_map030(void) {
    /* Load tileset textures */
    sheet_px[4] = d_a1_start; sheet_px[5] = d_b_start; sheet_px[6] = d_e_start;
    sheet_px[7] = d_ib_start; sheet_px[8] = d_d_start;
    memcpy(sheet_cl[4], d_a1c_start, sizeof(sheet_cl[4]));
    memcpy(sheet_cl[5], d_bc_start, sizeof(sheet_cl[5]));
    memcpy(sheet_cl[6], d_ec_start, sizeof(sheet_cl[6]));
    memcpy(sheet_cl[7], d_ibc_start, sizeof(sheet_cl[7]));
    memcpy(sheet_cl[8], d_dc_start, sizeof(sheet_cl[8]));
    
    /* Load map data */
    memcpy(map_layers, d_layers_start, sizeof(map_layers));
    memcpy(map_passability, d_pass_start, sizeof(map_passability));
    memcpy(map_higher, d_high_start, sizeof(map_higher));
    
    /* Setup character data */
    characters[0].name = "Mercenary";
    characters[0].sprite_data = d_merc_start;
    characters[0].clut_data = d_mercc_start;
    
    characters[1].name = "Outlander";
    characters[1].sprite_data = d_outl_start;
    characters[1].clut_data = d_outlc_start;
    
    characters[2].name = "Dark Priest";
    characters[2].sprite_data = d_priest_start;
    characters[2].clut_data = d_priestc_start;
    
    characters[3].name = "Knight";
    characters[3].sprite_data = d_knight_start;
    characters[3].clut_data = d_knightc_start;

    /* NPC sheets (!creature/!map_objects2/!Flame: 288×192 non-$, 24px cells;
     * $minerghost2: 120×220 single, 40×55 cells) */
    npc_sheets[0].t8 = d_creat_start; npc_sheets[0].clut = d_creatc_start;
    npc_sheets[0].iw = 288; npc_sheets[0].ih = 192;
    npc_sheets[0].tw = 512; npc_sheets[0].th = 256;
    npc_sheets[0].stride = 512; npc_sheets[0].big = 0;
    npc_sheets[1].t8 = d_ghost_start; npc_sheets[1].clut = d_ghostc_start;
    npc_sheets[1].iw = 120; npc_sheets[1].ih = 220;
    npc_sheets[1].tw = 128; npc_sheets[1].th = 256;
    npc_sheets[1].stride = 128; npc_sheets[1].big = 1;
    npc_sheets[2].t8 = d_mobj_start; npc_sheets[2].clut = d_mobjc_start;
    npc_sheets[2].iw = 288; npc_sheets[2].ih = 192;
    npc_sheets[2].tw = 512; npc_sheets[2].th = 256;
    npc_sheets[2].stride = 512; npc_sheets[2].big = 0;
    npc_sheets[3].t8 = d_flame_start; npc_sheets[3].clut = d_flamec_start;
    npc_sheets[3].iw = 288; npc_sheets[3].ih = 192;
    npc_sheets[3].tw = 512; npc_sheets[3].th = 256;
    npc_sheets[3].stride = 512; npc_sheets[3].big = 0;

    /* NPC roster (Map030 pg0_trigger/priority/image as shipped) */
    static const NpcDef baked[] = {
        /* ev, sheet, tx,ty, idx,pat,dir, trig,prio,fix, talk */
        {208, 0, 56,11, 1,2,8, 0,1,1, NULL, 0},
        {209, 0, 57,11, 2,0,8, 0,1,1, NULL, 0},
        {213, 0, 58,11, 2,1,8, 0,0,1, M30_EV213, M30_EV213_LEN},
        {210, 0, 56,12, 5,2,2, 0,0,1, NULL, 0},
        {211, 0, 57,12, 6,0,2, 0,0,1, NULL, 0},
        {212, 0, 58,12, 6,1,2, 0,0,1, NULL, 0},  /* empty list: examine = no-op */
        {185, 1, 82,11, 0,2,8, 4,1,0, NULL, 0},  /* parallel: static pose */
        {187, 1, 71,21, 0,2,8, 4,1,0, NULL, 0},
        {266, 2,104, 9, 4,0,8, 0,1,1, NULL, 0},
        {267, 2,105, 9, 4,1,8, 0,1,1, NULL, 0},
        {268, 2,105, 8, 4,1,6, 0,1,1, NULL, 0},
        {269, 2,104, 8, 4,0,6, 0,1,1, NULL, 0},
        { 20, 3, 84,20, 5,1,2, 0,1,1, M30_EV020, M30_EV020_LEN},
        { 21, 3, 84,19, 1,1,8, 0,1,1, NULL, 0},
    };
    for (int i = 0; i < 14; i++) {
        npcs[i] = baked[i];
        npc_draw[i].t8 = npc_sheets[baked[i].sheet].t8;
        npc_draw[i].clut = (unsigned int *)npc_sheets[baked[i].sheet].clut;
        npc_draw[i].img_w = npc_sheets[baked[i].sheet].iw;
        npc_draw[i].img_h = npc_sheets[baked[i].sheet].ih;
        npc_draw[i].tex_w = npc_sheets[baked[i].sheet].tw;
        npc_draw[i].tex_h = npc_sheets[baked[i].sheet].th;
        npc_draw[i].stride = npc_sheets[baked[i].sheet].stride;
        npc_draw[i].is_big = npc_sheets[baked[i].sheet].big;
        npc_draw[i].tile_x = baked[i].tx;
        npc_draw[i].tile_y = baked[i].ty;
        npc_draw[i].char_index = baked[i].index;
        npc_draw[i].pattern = baked[i].pattern;
        npc_draw[i].dir_mv = baked[i].dir_mv;
    }
    
    /* Initialize player */
    player_init(&player, 70, 8);
    player_set_sprite(&player, characters[0].sprite_data,
                     (unsigned int*)characters[0].clut_data, 480, 440, 0);
    
    sceKernelDcacheWritebackAll();
}

/* --- Main --- */
#ifdef DIAG_STAGES
/* Hardware boot diagnostic: writes stage markers to ms0:/fh_stage.txt and
 * paints a solid color per stage. Absence/truncation of the file pinpoints
 * the crash on real hardware (PPSSPP runs fine, so this is HW-only). */
#include <pspiofilemgr.h>
static SceUID diag_fd = -1;
static void diag_mark(const char *s) {
    if (diag_fd >= 0) sceIoWrite(diag_fd, s, strlen(s));
}
static void diag_color(unsigned int c, const char *label, void *fbp0, void *fbp1) {
    (void)fbp1;
    diag_mark(label);
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(c);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    sceKernelDelayThread(400000);
}
#endif

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
#ifdef DIAG_STAGES
    diag_fd = sceIoOpen("ms0:/fh_stage.txt",
                        PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    diag_mark("entry\n");
#endif

    pspDebugScreenInit();
    setupExitCallback();
    
    /* Initialize subsystems */
    input_init();
    
    /* Allocate framebuffers */
    void *fbp0 = guGetStaticVramBuffer(BUF_W, SCR_H, GU_PSM_8888);
    void *fbp1 = guGetStaticVramBuffer(BUF_W, SCR_H, GU_PSM_8888);
    void *zbp = guGetStaticVramBuffer(BUF_W, SCR_H, GU_PSM_4444);
    int total_frames = 0;
    
    render_init(fbp0, fbp1, zbp, gu_list);
#ifdef DIAG_STAGES
    diag_color(0xffff0000, "gu-init\n", fbp0, fbp1);  /* red = GU up */
#endif
    load_map030();
#ifdef DIAG_STAGES
    diag_color(0xff00ff00, "data-loaded\n", fbp0, fbp1);  /* green = data ok */
#endif
    
    /* Game state */
    InputState input = {0};
    int cam_x = player.x - SCR_W / 2;
    int cam_y = player.y - SCR_H / 2;
    
    /* Performance tracking */
    u64 tick_last;
    u32 tick_freq = sceRtcGetTickResolution();
    sceRtcGetCurrentTick(&tick_last);
    int frames = 0, fps = 0;
    
    /* Debug text buffer (updated periodically, not every frame) */
    static char debug_text[128] = "";
    int last_debug_update = 0;
    
    /* Main loop */
    while (!exit_request) {
        input_update(&input);
        
        /* Character selection */
        if (input_pressed(&input, PSP_CTRL_LTRIGGER)) {
            current_character = (current_character + 3) % 4;
            player_set_sprite(&player, characters[current_character].sprite_data,
                            (unsigned int*)characters[current_character].clut_data,
                            480, 440, 0);
        }
        if (input_pressed(&input, PSP_CTRL_RTRIGGER)) {
            current_character = (current_character + 1) % 4;
            player_set_sprite(&player, characters[current_character].sprite_data,
                            (unsigned int*)characters[current_character].clut_data,
                            480, 440, 0);
        }
        
        /* Exit */
        if (input_held(&input, PSP_CTRL_START)) break;
        
        /* Message demo toggle */
        if (input_pressed(&input, PSP_CTRL_SELECT)) {
            msg_mode = !msg_mode;
            if (msg_mode)
                msg_open(DEMO_EV, DEMO_EV_LEN, "Map001 ev37 (rotten meat)");
        }

        /* Talk: OK button. OG triggerButtonAction (rpg_objects.js): action
         * trigger here ([0], below-priority tiles) then facing tile
         * ([0,1,2], normal priority). Only standing starts events. */
        if (input_pressed(&input, PSP_CTRL_CROSS) && !player.moving) {
            int ptx = player.x / TILE, pty = player.y / TILE;
            int dx = 0, dy = 0;
            switch (player.dir) {
                case 0: dy = 1; break;
                case 1: dx = -1; break;
                case 2: dx = 1; break;
                case 3: dy = -1; break;
            }
            int found = -1;
            for (int pass = 0; pass < 2 && found < 0; pass++) {
                int tx = pass == 0 ? ptx : ptx + dx;
                int ty = pass == 0 ? pty : pty + dy;
                for (int i = 0; i < 14; i++) {
                    if (npcs[i].tx != tx || npcs[i].ty != ty) continue;
                    if (pass == 0) {
                        if (npcs[i].trig == 0 && npcs[i].prio == 0) found = i;
                    } else {
                        if (npcs[i].trig >= 0 && npcs[i].trig <= 2 &&
                            npcs[i].prio == 1) found = i;
                    }
                    if (found >= 0) break;
                }
            }
            if (found >= 0) {
                /* Face the player unless direction-fixed (page flag). */
                if (!npcs[found].dirfix) {
                    int ax = ptx - npcs[found].tx, ay = pty - npcs[found].ty;
                    int mv = (ay > 0) ? 2 : (ay < 0) ? 8 :
                             (ax > 0) ? 6 : 4;
                    npc_draw[found].dir_mv = mv;
                }
                if (npcs[found].talk && npcs[found].talk_len > 1) {
                    char title[64];
                    snprintf(title, sizeof(title), "Map030 ev%d (%s)",
                             npcs[found].ev_id,
                             npcs[found].ev_id == 20 ? "torch" : "saw corpse");
                    msg_open(npcs[found].talk, npcs[found].talk_len, title);
                }
            }
        }
        
        /* Message mode (temporary demo) */
        if (msg_mode) {
            if (!msg_ended) {
                if (mit.await_choice) {
                    if (input_pressed(&input, PSP_CTRL_UP) && msg_cursor > 0) msg_cursor--;
                    if (input_pressed(&input, PSP_CTRL_DOWN) && msg_cursor < mit.choice_n - 1) msg_cursor++;
                    if (input_pressed(&input, PSP_CTRL_CROSS)) {
                        mit.choice_sel = msg_cursor;
                        mit.await_choice = 0;
                    }
                    if (input_pressed(&input, PSP_CTRL_CIRCLE)) {
                        mit.choice_sel = -1;
                        mit.await_choice = 0;
                    }
                } else {
                    int r = fh_interp_step(&mit);
                    if (r == FH_RUN_END) msg_ended = 1;
                    msg_cursor = 0;
                }
            }
            
            /* Render message screen */
            sceGuStart(GU_DIRECT, gu_list);
            sceGuClearColor(0xff101018);
            sceGuClear(GU_COLOR_BUFFER_BIT);
            sceGuFinish();
            sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
            pspDebugScreenSetXY(0, 0);
            pspDebugScreenPrintf("MSG: %s", msg_title);
            msg_show();
            sceDisplayWaitVblankStart();
            fbp0 = sceGuSwapBuffers();
            frames++;
            
            /* Update FPS counter */
            u64 now;
            sceRtcGetCurrentTick(&now);
            if (now - tick_last >= tick_freq) {
                float span = (float)(now - tick_last) / (float)tick_freq;
                fps = (int)(frames / span);
                frames = 0;
                tick_last = now;
            }
            continue;
        }
        
        /* Update player */
        player_update(&player, input.buttons, (uint16_t*)map_passability, MAP_W, MAP_H);
        
        /* Center camera on player with smooth following */
        int target_x = player.x - SCR_W / 2;
        int target_y = player.y - SCR_H / 2;
        
        /* Apply bounds checking to target */
        if (target_x < 0) target_x = 0;
        if (target_y < 0) target_y = 0;
        if (target_x > MAP_W * TILE - SCR_W) target_x = MAP_W * TILE - SCR_W;
        if (target_y > MAP_H * TILE - SCR_H) target_y = MAP_H * TILE - SCR_H;
        
        /* Smooth camera: interpolate towards target (reduces jitter during movement) */
        cam_x = cam_x + (target_x - cam_x) / 2;
        cam_y = cam_y + (target_y - cam_y) / 2;
        
        /* Render game world */
        unsigned char *char_sprites[4] = {
            characters[0].sprite_data, characters[1].sprite_data,
            characters[2].sprite_data, characters[3].sprite_data
        };
        unsigned int *char_cluts[4] = {
            (unsigned int*)characters[0].clut_data, (unsigned int*)characters[1].clut_data,
            (unsigned int*)characters[2].clut_data, (unsigned int*)characters[3].clut_data
        };
        
        render_frame(cam_x, cam_y, &map_layers[0][0][0], MAP_W, MAP_H,
                    &player, current_character, char_sprites, char_cluts,
                    map_higher, sizeof(map_higher), total_frames,
                    npc_draw, 14);
        
        /* Update debug text periodically (not every frame) */
        if (frames == 0 || frames - last_debug_update >= 30) {
            snprintf(debug_text, sizeof(debug_text), "F&H: %s | L/R:switch | fps:%d",
                    characters[current_character].name, fps);
            last_debug_update = frames;
        }
        render_debug_text(debug_text);
        
        sceDisplayWaitVblankStart();
        fbp0 = sceGuSwapBuffers();
        frames++;
        total_frames++;
#ifdef DIAG_STAGES
        if (frames == 1) diag_mark("frame1\n");
        else if (frames == 60) diag_mark("frame60\n");
        else if (frames == 600) { diag_mark("frame600\n"); sceIoClose(diag_fd); diag_fd = -1; }
#endif
        
        /* Update FPS counter */
        u64 now;
        sceRtcGetCurrentTick(&now);
        if (now - tick_last >= tick_freq) {
            float span = (float)(now - tick_last) / (float)tick_freq;
            fps = (int)(frames / span);
            frames = 0;
            tick_last = now;
        }
    }
    
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
