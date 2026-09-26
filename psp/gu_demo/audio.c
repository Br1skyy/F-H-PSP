#include "audio.h"
#include <pspkernel.h>
#include <pspaudio.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include "tremor/ivorbiscodec.h"
#include "tremor/ivorbisfile.h"

#define AU_RATE 44100
#define AU_CHUNK 1024
#define AU_SE_N 8
#define AU_CACHE_FRAMES 2048

static int au_mix_count;
static int au_last_err;
static char au_err_at[48];

typedef struct {
    int kind;
    FILE *f;
    long d_off, d_len;
    int rate, ch;
    OggVorbis_File vf;
    int stream_open;
    long stream_total;
    int fail_streak;
    short cache[AU_CACHE_FRAMES * 2];
    long cache_start;
    int cache_n;
    float pos, step;
    int vol, pitch, pan;
    int lg, rg;
    int active, loop;
    int fade_left, fade_total, fade_from;
    char name[64];
} AVoice;

static AVoice au_se[AU_SE_N];
static AVoice au_bgm, au_bgs, au_me;
static SceUID au_sema = -1;
static int au_ch = -1;
static char au_pushed_bgm[64];
static int au_pushed_vol, au_pushed_pitch;

static void au_gains(AVoice *v, int *lg, int *rg) {
    int g = v->vol < 0 ? 0 : v->vol > 100 ? 100 : v->vol;
    int l = v->pan <= 0 ? 256 : (100 - v->pan) * 256 / 100;
    int r = v->pan >= 0 ? 256 : (100 + v->pan) * 256 / 100;
    if (l < 0) l = 0;
    if (r < 0) r = 0;
    *lg = g * l / 100;
    *rg = g * r / 100;
}

static void au_trace(const char *msg, int v) {
    SceUID fd = sceIoOpen("ms0:/fh_audio.txt",
                          PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        char b[128];
        int n = snprintf(b, sizeof(b), "%s %d\n", msg, v);
        sceIoWrite(fd, b, n > 0 ? (SceSize)n : 0);
        sceIoClose(fd);
    }
}

static int au_wav_open(AVoice *v, const char *path) {
    unsigned char h[64];
    long off = 12, size = 0;
    int rate = 0, ch = 0, bits = 0;
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) ||
        memcmp(h + 8, "WAVE", 4)) {
        fclose(f);
        return -1;
    }
    v->d_off = -1;
    while (off + 8 <= size) {
        unsigned char c[8];
        unsigned len;
        fseek(f, off, SEEK_SET);
        if (fread(c, 1, 8, f) != 8)
            break;
        len = (unsigned)c[4] | ((unsigned)c[5] << 8) |
              ((unsigned)c[6] << 16) | ((unsigned)c[7] << 24);
        if (!memcmp(c, "fmt ", 4) && len >= 16) {
            unsigned char fm[16];
            if (fread(fm, 1, 16, f) != 16)
                break;
            if (fm[0] != 1 || fm[1] != 0)
                break;
            ch = fm[2] | (fm[3] << 8);
            rate = fm[4] | (fm[5] << 8) | (fm[6] << 16) | (fm[7] << 24);
            bits = fm[14] | (fm[15] << 8);
        } else if (!memcmp(c, "data", 4)) {
            v->d_off = off + 8;
            v->d_len = (long)len;
        }
        off += 8 + (long)((len + 1) & ~1u);
        if (v->d_off >= 0 && rate)
            break;
    }
    if (v->d_off < 0 || !rate || !ch || bits != 16) {
        fclose(f);
        return -1;
    }
    v->f = f;
    v->rate = rate;
    v->ch = ch;
    return 0;
}

static int au_cache_fill(AVoice *v, long frame) {
    long want = frame;
    if (want < v->cache_start ||
        want >= v->cache_start + v->cache_n) {
        if (v->kind == 1) {
            long byte = v->d_off + want * 2L * v->ch;
            long left = v->d_len - (byte - v->d_off);
            int nfr;
            if (left <= 0)
                return 0;
            nfr = (int)(left / (2L * v->ch));
            if (nfr > AU_CACHE_FRAMES)
                nfr = AU_CACHE_FRAMES;
            fseek(v->f, byte, SEEK_SET);
            if (fread(v->cache, 2 * v->ch, (size_t)nfr, v->f) !=
                (size_t)nfr)
                return 0;
            v->cache_start = want;
            v->cache_n = nfr;
        } else {
            int got = 0, bs = 0, step;
            vorbis_info *vi = ov_info(&v->vf, -1);
            if (want >= v->stream_total)
                return 0;
            if (want != v->cache_start + v->cache_n) {
                if (ov_pcm_seek(&v->vf, want) != 0)
                    return 0;
            }
            step = 2 * (vi->channels > 1 ? 2 : 1);
            while (got < AU_CACHE_FRAMES) {
                long r = ov_read(&v->vf, (char *)(v->cache + got * 2),
                                 (AU_CACHE_FRAMES - got) * step, &bs);
                if (r <= 0)
                    break;
                got += (int)r / step;
            }
            if (!got)
                return 0;
            v->cache_start = want;
            v->cache_n = got;
        }
    }
    return 1;
}

static int au_voice_sample(AVoice *v, int *l, int *r) {
    long frame = (long)v->pos;
    int i0, f, s0l, s0r, s1l, s1r, st;
    if (!au_cache_fill(v, frame))
        return 0;
    i0 = (int)(frame - v->cache_start);
    if (i0 < 0 || i0 >= v->cache_n)
        return 0;
    st = v->ch > 1 ? 2 : 1;
    f = (int)((v->pos - (float)frame) * 256.0f);
    s0l = v->cache[i0 * st];
    s0r = v->ch > 1 ? v->cache[i0 * st + 1] : s0l;
    if (i0 + 1 < v->cache_n) {
        s1l = v->cache[i0 * st + st];
        s1r = v->ch > 1 ? v->cache[i0 * st + st + 1] : s1l;
    } else {
        s1l = s0l;
        s1r = s0r;
    }
    *l = s0l + ((s1l - s0l) * f >> 8);
    *r = s0r + ((s1r - s0r) * f >> 8);
    v->pos += v->step;
    return 1;
}

static void au_stream_fill(AVoice *v, const char *path, int loop) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        au_last_err = -101;
        snprintf(au_err_at, sizeof(au_err_at), "open %-36s", path);
        return;
    }
    setvbuf(f, NULL, _IOFBF, 65536);
    if (ov_open(f, &v->vf, NULL, 0) < 0) {
        au_last_err = -102;
        snprintf(au_err_at, sizeof(au_err_at), "ovopen %-34s", path);
        fclose(f);
        return;
    }
    {
        vorbis_info *vi = ov_info(&v->vf, -1);
        v->rate = (int)vi->rate;
        v->ch = vi->channels;
    }
    v->f = f;
    v->kind = 2;
    v->stream_open = 1;
    v->stream_total = (long)ov_pcm_total(&v->vf, -1);
    v->loop = loop;
    v->cache_start = 0;
    v->cache_n = 0;
    v->pos = 0.0f;
    v->step = (float)v->rate * ((float)v->pitch / 100.0f) / (float)AU_RATE;
    v->fade_left = v->fade_total = 0;
    v->active = 1;
}

static void au_stream_stop(AVoice *v) {
    if (v->stream_open) {
        ov_clear(&v->vf);
        v->stream_open = 0;
    }
    if (v->f) {
        fclose(v->f);
        v->f = 0;
    }
    v->active = 0;
    v->kind = 0;
    v->name[0] = 0;
}

static void au_stream_start(AVoice *v, const char *dir, const char *name,
                            int vol, int pitch, int pan, int loop) {
    char path[96];
    if (!name || !name[0]) {
        au_stream_stop(v);
        return;
    }
    if (v->active && !strcmp(v->name, name))
        return;
    au_stream_stop(v);
    snprintf(path, sizeof(path), "data/audio/%s/%s.ogg", dir, name);
    v->vol = vol;
    v->pitch = pitch < 50 ? 50 : pitch > 150 ? 150 : pitch;
    v->pan = pan;
    snprintf(v->name, sizeof(v->name), "%s", name);
    au_stream_fill(v, path, loop);
    if (!v->active)
        v->name[0] = 0;
}

static int au_thread(SceSize args, void *argp) {
    static short out[AU_CHUNK * 2];
    static int acc[AU_CHUNK * 2];
    (void)args;
    (void)argp;
    for (;;) {
        int i, n;
        sceKernelWaitSema(au_sema, 1, 0);
        for (i = 0; i < AU_CHUNK * 2; i++) acc[i] = 0;
        for (n = 0; n < AU_SE_N; n++) {
            AVoice *v = &au_se[n];
            int s, l, r;
            if (!v->active)
                continue;
            for (s = 0; s < AU_CHUNK; s++) {
                long frames = v->d_len / (2L * v->ch);
                if ((long)v->pos >= frames) {
                    v->active = 0;
                    if (v->f) {
                        fclose(v->f);
                        v->f = 0;
                    }
                    break;
                }
                if (!au_voice_sample(v, &l, &r))
                    break;
                acc[s * 2] += l * v->lg >> 8;
                acc[s * 2 + 1] += r * v->rg >> 8;
            }
        }
        {
            AVoice *vs[3] = {&au_bgm, &au_bgs, &au_me};
            for (n = 0; n < 3; n++) {
                AVoice *v = vs[n];
                int s;
                if (!v->active)
                    continue;
                au_gains(v, &v->lg, &v->rg);
                for (s = 0; s < AU_CHUNK; s++) {
                    int l, r, g = 256;
                    if (v->fade_left > 0) {
                        g = v->fade_from * v->fade_left / v->fade_total;
                        if (--v->fade_left <= 0) {
                            au_stream_stop(v);
                            break;
                        }
                    }
                    if (!au_voice_sample(v, &l, &r)) {
                        if (v->loop && v->stream_open &&
                            ++v->fail_streak < 4) {
                            ov_raw_seek(&v->vf, 0);
                            v->cache_start = 0;
                            v->cache_n = 0;
                            v->pos = 0.0f;
                            continue;
                        }
                        au_stream_stop(v);
                        break;
                    }
                    v->fail_streak = 0;
                    acc[s * 2] += (l * v->lg >> 8) * g >> 8;
                    acc[s * 2 + 1] += (r * v->rg >> 8) * g >> 8;
                }
            }
        }
        for (i = 0; i < AU_CHUNK * 2; i++) {
            int s = acc[i];
            if (s < -32768) s = -32768;
            if (s > 32767) s = 32767;
            out[i] = (short)s;
        }
        sceKernelSignalSema(au_sema, 1);
        sceAudioOutputBlocking(au_ch, PSP_AUDIO_VOLUME_MAX, out);
        if ((++au_mix_count & 255) == 0)
            au_trace("mix", au_mix_count);
    }
    return 0;
}

void audio_init(void) {
    au_sema = sceKernelCreateSema("fh_audio", 0, 1, 1, 0);
    au_ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AU_CHUNK,
                              PSP_AUDIO_FORMAT_STEREO);
    au_trace("init sema/ch", (int)au_sema * 1000 + au_ch);
    if (au_ch >= 0) {
        SceUID th = sceKernelCreateThread("fh_audio", au_thread, 0x12,
                                          128 * 1024, 0, 0);
        au_trace("init thread", (int)th);
        if (th >= 0)
            sceKernelStartThread(th, 0, 0);
    }
}

void audio_status(char *out, int cap) {
    snprintf(out, cap, "mix=%d err=%d@%s bgm=%d:%ld/%ldv%d bgs=%d:%ld/%ld",
             au_mix_count, au_last_err, au_err_at, au_bgm.active,
             (long)au_bgm.pos, au_bgm.stream_total, au_bgm.vol,
             au_bgs.active, (long)au_bgs.pos, au_bgs.stream_total);
}

static void au_se_alloc(const char *name, int vol, int pitch, int pan) {
    char path[96];
    int n, slot = -1;
    if (!name || !name[0])
        return;
    for (n = 0; n < AU_SE_N; n++) {
        if (!au_se[n].active) {
            slot = n;
            break;
        }
    }
    if (slot < 0)
        return;
    snprintf(path, sizeof(path), "data/audio/se/%s.wav", name);
    {
        AVoice tmp;
        memset(&tmp, 0, sizeof(tmp));
        if (au_wav_open(&tmp, path) != 0) {
            au_last_err = -103;
            snprintf(au_err_at, sizeof(au_err_at), "wav %-37s", path);
            return;
        }
        au_se[slot] = tmp;
    }
    au_se[slot].kind = 1;
    au_se[slot].vol = vol;
    au_se[slot].pitch = pitch < 50 ? 50 : pitch > 150 ? 150 : pitch;
    au_se[slot].pan = pan;
    au_se[slot].step = (float)au_se[slot].rate *
                       ((float)au_se[slot].pitch / 100.0f) / (float)AU_RATE;
    au_se[slot].cache_start = 0;
    au_se[slot].cache_n = 0;
    au_se[slot].pos = 0.0f;
    au_gains(&au_se[slot], &au_se[slot].lg, &au_se[slot].rg);
    au_se[slot].active = 1;
    snprintf(au_se[slot].name, sizeof(au_se[slot].name), "%s", name);
}

void audio_play_se(const char *name, int vol, int pitch, int pan) {
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    au_se_alloc(name, vol, pitch, pan);
    sceKernelSignalSema(au_sema, 1);
}

void audio_play_bgm(const char *name, int vol, int pitch) {
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    au_stream_start(&au_bgm, "bgm", name, vol, pitch, 0, 1);
    sceKernelSignalSema(au_sema, 1);
}

void audio_play_bgs(const char *name, int vol, int pitch, int pan) {
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    au_stream_start(&au_bgs, "bgs", name, vol, pitch, pan, 1);
    sceKernelSignalSema(au_sema, 1);
}

void audio_play_me(const char *name, int vol, int pitch, int pan) {
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    au_stream_start(&au_me, "me", name, vol, pitch, pan, 0);
    sceKernelSignalSema(au_sema, 1);
}

void audio_fade_bgm(int frames) {
    if (au_sema < 0 || !au_bgm.active)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    au_bgm.fade_total = au_bgm.fade_left = frames * AU_RATE / 60;
    au_bgm.fade_from = 256;
    sceKernelSignalSema(au_sema, 1);
}

void audio_fade_bgs(int frames) {
    if (au_sema < 0 || !au_bgs.active)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    au_bgs.fade_total = au_bgs.fade_left = frames * AU_RATE / 60;
    au_bgs.fade_from = 256;
    sceKernelSignalSema(au_sema, 1);
}

void audio_stop_se(void) {
    int n;
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    for (n = 0; n < AU_SE_N; n++) {
        if (au_se[n].f) {
            fclose(au_se[n].f);
            au_se[n].f = 0;
        }
        au_se[n].active = 0;
    }
    sceKernelSignalSema(au_sema, 1);
}

void audio_stop_bgs(void) {
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    au_stream_stop(&au_bgs);
    sceKernelSignalSema(au_sema, 1);
}

void audio_stop_me(void) {
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    au_stream_stop(&au_me);
    sceKernelSignalSema(au_sema, 1);
}

static char au_pushed_bgs[64];
static int au_pushed_bgs_vol, au_pushed_bgs_pitch, au_pushed_bgs_pan;

void audio_push_bgs(void) {
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    snprintf(au_pushed_bgs, sizeof(au_pushed_bgs), "%s", au_bgs.name);
    au_pushed_bgs_vol = au_bgs.vol;
    au_pushed_bgs_pitch = au_bgs.pitch;
    au_pushed_bgs_pan = au_bgs.pan;
    sceKernelSignalSema(au_sema, 1);
}

void audio_pop_bgs(void) {
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    if (au_pushed_bgs[0])
        au_stream_start(&au_bgs, "bgs", au_pushed_bgs, au_pushed_bgs_vol,
                        au_pushed_bgs_pitch, au_pushed_bgs_pan, 1);
    au_pushed_bgs[0] = 0;
    sceKernelSignalSema(au_sema, 1);
}

void audio_push_bgm(void) {
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    snprintf(au_pushed_bgm, sizeof(au_pushed_bgm), "%s", au_bgm.name);
    au_pushed_vol = au_bgm.vol;
    au_pushed_pitch = au_bgm.pitch;
    sceKernelSignalSema(au_sema, 1);
}

void audio_pop_bgm(void) {
    if (au_sema < 0)
        return;
    sceKernelWaitSema(au_sema, 1, 0);
    if (au_pushed_bgm[0])
        au_stream_start(&au_bgm, "bgm", au_pushed_bgm, au_pushed_vol,
                        au_pushed_pitch, 0, 1);
    else
        au_stream_stop(&au_bgm);
    au_pushed_bgm[0] = 0;
    sceKernelSignalSema(au_sema, 1);
}

typedef struct {
    FhInterp *it;
    int se_count;
    char bgm[64], bgs[64], me[64];
    int bgm_fade, bgs_fade;
} AuSlot;

static AuSlot au_slots[2];

void audio_poll(FhInterp *it) {
    AuSlot *s = 0;
    int i;
    if (!it || au_sema < 0)
        return;
    for (i = 0; i < 2; i++) {
        if (au_slots[i].it == it) {
            s = &au_slots[i];
            break;
        }
        if (!au_slots[i].it) {
            au_slots[i].it = it;
            s = &au_slots[i];
            break;
        }
    }
    if (!s)
        return;
    if (it->se_count != s->se_count) {
        s->se_count = it->se_count;
        audio_play_se(it->last_se, it->se_vol, it->se_pitch, it->se_pan);
    }
    if (strcmp(it->last_bgm, s->bgm)) {
        snprintf(s->bgm, sizeof(s->bgm), "%s", it->last_bgm);
        if (it->last_bgm[0])
            audio_play_bgm(it->last_bgm, it->bgm_vol, it->bgm_pitch);
        else {
            sceKernelWaitSema(au_sema, 1, 0);
            au_stream_stop(&au_bgm);
            sceKernelSignalSema(au_sema, 1);
        }
    }
    if (it->bgm_fade) {
        audio_fade_bgm(it->bgm_fade);
        it->bgm_fade = 0;
    }
    if (strcmp(it->last_bgs, s->bgs)) {
        snprintf(s->bgs, sizeof(s->bgs), "%s", it->last_bgs);
        if (it->last_bgs[0])
            audio_play_bgs(it->last_bgs, it->bgs_vol, it->bgs_pitch,
                            it->bgs_pan);
        else {
            sceKernelWaitSema(au_sema, 1, 0);
            au_stream_stop(&au_bgs);
            sceKernelSignalSema(au_sema, 1);
        }
    }
    if (it->bgs_fade) {
        audio_fade_bgs(it->bgs_fade);
        it->bgs_fade = 0;
    }
    if (strcmp(it->last_me, s->me)) {
        snprintf(s->me, sizeof(s->me), "%s", it->last_me);
        if (it->last_me[0])
            audio_play_me(it->last_me, it->me_vol, it->me_pitch,
                           it->me_pan);
    }
    if (it->se_stop) {
        audio_stop_se();
        it->se_stop = 0;
    }
}
