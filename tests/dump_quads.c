

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../runtime/map.h"
#include "../runtime/autotile_tables.h"

int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: dump_quads W H layers.bin\n"); return 2; }
    int W = atoi(argv[1]), H = atoi(argv[2]);
    FILE *f = fopen(argv[3], "rb");
    if (!f) { perror("open"); return 1; }
    uint16_t *data = malloc((size_t)W * H * 4 * 2);
    if (fread(data, 2, (size_t)W * H * 4, f) != (size_t)W * H * 4) {
        fprintf(stderr, "short read\n");
        return 1;
    }
    fclose(f);
    for (int z = 0; z < 4; z++) {
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int tid = data[(z * H + y) * W + x];
                if (tid <= 0 || tid >= FH_TILE_MAX) continue;
                if (fh_is_autotile(tid)) {
                    int set, bx, by, table;
                    if (!fh_auto_cell(tid, &set, &bx, &by, &table)) continue;
                    int shape = fh_auto_shape(tid);
                    const int (*tab)[2] = NULL;


                    if (table == 0) tab = FH_AUTO_FLOOR[shape];
                    else if (table == 1 && shape < 16) tab = FH_AUTO_WALL[shape];
                    else if (table == 2 && shape < 4) tab = FH_AUTO_WATERFALL[shape];
                    else continue;
                    for (int qi = 0; qi < 4; qi++) {
                        int su = (bx * 2 + tab[qi][0]) * FH_TILE_HALF;
                        int sv = (by * 2 + tab[qi][1]) * FH_TILE_HALF;
                        printf("%d %d %d %d %d %d %d %d %d\n",
                               z, x, y, qi, set, su, sv, FH_TILE_HALF, FH_TILE_HALF);
                    }
                } else {
                    FhQuad q;
                    if (!fh_normal_quad(tid, &q)) continue;
                    printf("%d %d %d -1 %d %d %d %d %d\n", z, x, y, q.slot, q.su, q.sv, q.sw, q.sh);
                }
            }
        }
    }
    free(data);
    return 0;
}
