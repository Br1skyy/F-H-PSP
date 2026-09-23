

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../runtime/map.h"

#define TILE 24
#define VW 480
#define VH 272
#define NX 22
#define NY 13


static void deswizzle8(unsigned char *out, const unsigned char *in, int w, int h) {
    int src = 0;
    for (int by = 0; by < h; by += 8)
        for (int bx = 0; bx < w; bx += 16)
            for (int row = 0; row < 8; row++) {
                memcpy(out + (by + row) * w + bx, in + src, 16);
                src += 16;
            }
}

static unsigned char sheets[5][384 * 384 * 4];
static int sheet_w[5] = {192, 384, 384, 384, 384};
static int sheet_h[5] = {384, 384, 384, 384, 384};
static const char *sheet_names[5] = {"Mines_A1", "Mines_B", "Mines_E", "Inside_B", "Mines_D"};

static int load_sheet(const char *dir, int i) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.t8", dir, sheet_names[i]);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "no %s\n", path); return 0; }
    int w = sheet_w[i], h = sheet_h[i];
    unsigned char *idx = malloc((size_t)w * h);
    if (fread(idx, 1, (size_t)w * h, f) != (size_t)w * h) return 0;
    fclose(f);
    snprintf(path, sizeof(path), "%s/%s.clut", dir, sheet_names[i]);
    f = fopen(path, "rb");
    if (!f) return 0;
    uint32_t clut[256];
    if (fread(clut, 4, 256, f) != 256) return 0;
    fclose(f);
    unsigned char *swiz = malloc((size_t)w * h);
    deswizzle8(swiz, idx, w, h);
    free(idx);
    for (int p = 0; p < w * h; p++) {
        uint32_t c = clut[swiz[p]];
        sheets[i][p * 4 + 0] = (unsigned char)(c & 255);
        sheets[i][p * 4 + 1] = (unsigned char)((c >> 8) & 255);
        sheets[i][p * 4 + 2] = (unsigned char)((c >> 16) & 255);
        sheets[i][p * 4 + 3] = (unsigned char)((c >> 24) & 255);
    }
    free(swiz);
    return 1;
}


static void over(unsigned char *d, const unsigned char *s) {
    double sa = s[3] / 255.0, da = d[3] / 255.0;
    double oa = sa + da * (1 - sa);
    if (oa <= 0) { d[0] = d[1] = d[2] = d[3] = 0; return; }
    for (int c = 0; c < 3; c++)
        d[c] = (unsigned char)(s[c] * sa / oa + d[c] * da * (1 - sa) / oa + 0.5);
    d[3] = (unsigned char)(oa * 255 + 0.5);
}

int main(int argc, char **argv) {
    if (argc != 6) { fprintf(stderr, "usage: draw_ppm layers.bin sheetdir x0 y0 out.ppm\n"); return 2; }
    int MW = 145, MH = 105;
    uint16_t *layers = malloc((size_t)4 * MW * MH * 2);
    FILE *f = fopen(argv[1], "rb");
    if (!f || fread(layers, 2, (size_t)4 * MW * MH, f) != (size_t)4 * MW * MH) {
        fprintf(stderr, "layers read fail\n");
        return 1;
    }
    fclose(f);
    for (int i = 0; i < 5; i++)
        if (!load_sheet(argv[2], i)) return 1;
    int x0 = atoi(argv[3]), y0 = atoi(argv[4]);

    static unsigned char canvas[VW * VH * 4];
    memset(canvas, 0, sizeof(canvas));

    for (int i = 0; i < VW * VH; i++) canvas[i * 4 + 3] = 255;

    for (int z = 0; z < 4; z++) {
        for (int s = 4; s < 9; s++) {
            static FhDraw out[NX * NY];
            int n = fh_collect(x0, y0, NX, NY, TILE, 0, 0, z, s,
                               layers, MW, MH, out, NX * NY);
            for (int i = 0; i < n; i++) {
                int si = s - 4;
                int su = out[i].q.su / 2, sv = out[i].q.sv / 2;
                for (int py = 0; py < TILE; py++) {
                    for (int px = 0; px < TILE; px++) {
                        int dx = out[i].dx + px, dy = out[i].dy + py;
                        if (dx < 0 || dy < 0 || dx >= VW || dy >= VH) continue;
                        const unsigned char *sp =
                            &sheets[si][((sv + py) * sheet_w[si] + (su + px)) * 4];
                        over(&canvas[(dy * VW + dx) * 4], sp);
                    }
                }
            }
        }
    }
    f = fopen(argv[5], "wb");
    fprintf(f, "P6\n%d %d\n255\n", VW, VH);
    for (int i = 0; i < VW * VH; i++) fputc(canvas[i * 4], f), fputc(canvas[i * 4 + 1], f), fputc(canvas[i * 4 + 2], f);
    fclose(f);
    printf("wrote %s\n", argv[5]);
    return 0;
}
