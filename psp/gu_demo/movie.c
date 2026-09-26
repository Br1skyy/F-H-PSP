#include "movie.h"
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspmpeg.h>
#include <psputility.h>
#include <psputility_avmodules.h>
#include <psprtc.h>
#include <stdio.h>
#include <string.h>

#define MV_PACKETS 256
#define MV_PACKET_SIZE 2048
#define MV_RGB_W 512
#define MV_RGB_H 512
#define MV_CHUNK (64 * 1024)

typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} MVVert;
#define MV_FMT (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | \
                GU_TRANSFORM_2D)

static SceMpeg g_mpeg;
static SceMpegRingbuffer g_ring;
static unsigned char g_ringdata[MV_PACKETS * MV_PACKET_SIZE]
    __attribute__((aligned(64)));
static unsigned char g_mpegwork[256 * 1024]
    __attribute__((aligned(64)));
static unsigned int g_rgb[MV_RGB_W * MV_RGB_H]
    __attribute__((aligned(16)));
static unsigned char g_chunk[MV_CHUNK + 4]
    __attribute__((aligned(16)));

extern unsigned int gu_list[];

static u64 gu_tick(void) {
    u64 t;
    sceRtcGetCurrentTick(&t);
    return t;
}

static int gu_skip_pressed(void) {
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);
    return pad.Buttons & (PSP_CTRL_START | PSP_CTRL_CROSS |
                          PSP_CTRL_CIRCLE | PSP_CTRL_TRIANGLE |
                          PSP_CTRL_SQUARE | PSP_CTRL_SELECT);
}

static void gu_blit(void) {
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(0xff000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, MV_RGB_W, MV_RGB_H, MV_RGB_W, g_rgb);
    sceGuTexFlush();
    sceGuTexSync();
    {
        MVVert *v = (MVVert *)sceGuGetMemory(2 * sizeof(MVVert));
        v[0].u = 0; v[0].v = 0; v[0].color = 0xffffffff;
        v[0].x = 0; v[0].y = 0; v[0].z = 0.0f;
        v[1].u = 480; v[1].v = 272; v[1].color = 0xffffffff;
        v[1].x = 480; v[1].y = 272; v[1].z = 0.0f;
        sceGuDrawArray(GU_SPRITES, MV_FMT, 2, 0, v);
    }
    sceGuDisable(GU_TEXTURE_2D);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static unsigned long long au_pts(const SceMpegAu *au) {
    unsigned long long a = ((unsigned long long)au->iPtsMSB << 32) |
                           au->iPts;
    unsigned long long b = ((unsigned long long)au->iPts << 32) |
                           au->iPtsMSB;
    if (a > 3600000ULL * 3u && b <= 3600000ULL * 3u)
        return b;
    return a;
}


int movie_play(const char *path) {
    FILE *f;
    unsigned char head[2048];
    SceInt32 offset = 0, total = 0;
    SceMpegStream *vstream = 0;
    ScePVoid esbuf = 0;
    SceMpegAu au;
    SceMpegAvcMode mode;
    int rc = -1, skipped = 0;
    long file_len = 0, file_pos = 0;
    unsigned write_pos = 0;
    unsigned long long t0 = 0;
    int have_t0 = 0;
    static unsigned gu_tick_freq;

    if (!path || !path[0])
        return -1;
    f = fopen(path, "rb");
    if (!f)
        return -2;
    fseek(f, 0, SEEK_END);
    file_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_len <= 2048) {
        fclose(f);
        return -3;
    }
    if (fread(head, 1, 2048, f) != 2048) {
        fclose(f);
        return -4;
    }
    file_pos = 2048;

    if (sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC) < 0) {
        fclose(f);
        return -10;
    }
    if (sceMpegInit() < 0) {
        fclose(f);
        return -11;
    }
    if (sceMpegQueryMemSize(4) > (int)sizeof(g_mpegwork)) {
        sceMpegFinish();
        fclose(f);
        return -12;
    }
    if (sceMpegCreate(&g_mpeg, g_mpegwork, sizeof(g_mpegwork), &g_ring,
                      512, 4, 0) < 0) {
        sceMpegFinish();
        fclose(f);
        return -13;
    }
    if (sceMpegRingbufferConstruct(&g_ring, MV_PACKETS, g_ringdata,
                                   sizeof(g_ringdata), 0, 0) < 0) {
        sceMpegDelete(&g_mpeg);
        sceMpegFinish();
        fclose(f);
        return -14;
    }
    if (sceMpegQueryStreamOffset(&g_mpeg, head, &offset) < 0 ||
        offset <= 0) {
        goto done;
    }
    if (sceMpegQueryStreamSize(head, &total) < 0 || total <= 0)
        goto done;
    mode.iUnk0 = 0;
    mode.iPixelFormat = 3;
    sceMpegAvcDecodeMode(&g_mpeg, &mode);
    vstream = sceMpegRegistStream(&g_mpeg, 0, 0);
    if (!vstream)
        goto done;
    esbuf = sceMpegMallocAvcEsBuf(&g_mpeg);
    if (!esbuf)
        goto done;
    memset(&au, 0, sizeof(au));
    if (sceMpegInitAu(&g_mpeg, esbuf, &au) < 0)
        goto done;
    if (fseek(f, offset, SEEK_SET) != 0)
        goto done;
    file_pos = offset;

    gu_tick_freq = sceRtcGetTickResolution();
    rc = 0;
    {
        u64 wall0 = 0;
        int have_wall0 = 0, stuck = 0;
        for (;;) {
            int avail, want, got;
            if (gu_skip_pressed()) {
                skipped = 1;
                break;
            }
            avail = sceMpegRingbufferAvailableSize(&g_ring);
            if (avail > 0 && file_pos < (long)offset + total) {
                long left = (long)offset + total - file_pos;
                want = avail * MV_PACKET_SIZE;
                if ((long)want > left)
                    want = (int)left;
                if ((long)want > (long)sizeof(g_chunk) - 4)
                    want = sizeof(g_chunk) - 4;
                got = (int)fread(g_chunk, 1, (size_t)want, f);
                if (got > 0) {
                    unsigned dst = write_pos % MV_PACKETS;
                    unsigned first = MV_PACKETS - dst;
                    unsigned n1 = (unsigned)got / MV_PACKET_SIZE;
                    unsigned copy1 = n1 > first ? first : n1;
                    memcpy(g_ringdata + dst * MV_PACKET_SIZE, g_chunk,
                           copy1 * MV_PACKET_SIZE);
                    if (n1 > copy1) {
                        memcpy(g_ringdata,
                               g_chunk + copy1 * MV_PACKET_SIZE,
                               (n1 - copy1) * MV_PACKET_SIZE);
                    }
                    if (got % MV_PACKET_SIZE) {
                        unsigned tail = dst + n1;
                        if (tail >= MV_PACKETS)
                            tail -= MV_PACKETS;
                        memcpy(g_ringdata + tail * MV_PACKET_SIZE,
                               g_chunk + n1 * MV_PACKET_SIZE,
                               (unsigned)got % MV_PACKET_SIZE);
                    }
                    sceKernelDcacheWritebackInvalidateRange(
                        g_ringdata, sizeof(g_ringdata));
                    sceMpegRingbufferPut(&g_ring, (int)n1, avail);
                    write_pos += n1;
                    file_pos += got;
                }
            }
            if (sceMpegGetAvcAu(&g_mpeg, vstream, &au, 0) < 0) {
                if (file_pos >= (long)offset + total)
                    break;
                sceKernelDelayThread(2000);
                if (++stuck > 2000)
                    break;
                continue;
            }
            stuck = 0;
            {
                unsigned rgbp = (unsigned)g_rgb;
                SceInt32 st = 0;
                unsigned long long pts, target;
                if (sceMpegAvcDecode(&g_mpeg, &au, 512, &rgbp, &st) < 0)
                    continue;
                pts = au_pts(&au);
                if (!have_t0) {
                    t0 = pts;
                    have_t0 = 1;
                    wall0 = gu_tick();
                    have_wall0 = 1;
                }
                gu_blit();
                target = (pts >= t0 ? pts - t0 : 0) * gu_tick_freq /
                         90000u;
                if (have_wall0) {
                    for (;;) {
                        if (gu_tick() - wall0 >= target)
                            break;
                        if (gu_skip_pressed()) {
                            skipped = 1;
                            break;
                        }
                        sceKernelDelayThread(1000);
                    }
                    if (skipped)
                        break;
                }
            }
        }
    }

done:
    fclose(f);
    sceMpegDelete(&g_mpeg);
    sceMpegFinish();
    return skipped ? 1 : rc;
}
