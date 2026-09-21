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
            if (msg_mode) {
                fh_interp_init(&mit, DEMO_EV, DEMO_EV_LEN);
                msg_ended = 0;
                msg_cursor = 0;
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
            pspDebugScreenPrintf("MSG DEMO: Map001 ev37 (rotten meat)");
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
                    map_higher, sizeof(map_higher));
        
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
