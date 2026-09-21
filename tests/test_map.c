/* Unit tests for runtime/map.c. Expectations hand-computed from rpg_core.js.
 * Build: gcc -Wall -O2 -o test_map test_map.c ../runtime/map.c && ./test_map
 */
#include <stdio.h>
#include "../runtime/map.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    FhQuad q;
    /* normal: A5 base */
    CHECK(fh_normal_quad(1536, &q) && q.slot == 4 && q.su == 0 && q.sv == 0 &&
          q.sw == 48 && q.sh == 48, "A5 1536");
    /* normal: B tile 91 -> set 5, sx=(0*8+3)*48=144, sy=(11%16)*48=528 */
    CHECK(fh_normal_quad(91, &q) && q.slot == 5 && q.su == 144 && q.sv == 528, "B 91");
    /* normal: C tile 300 -> set 6, sx=(0+4)*48=192, sy=(5)*48=240 */
    CHECK(fh_normal_quad(300, &q) && q.slot == 6 && q.su == 192 && q.sv == 240, "C 300");
    /* normal: E tile 1000 -> set 8, sx=(1*8+0)*48=384, sy=(13)*48=624 */
    CHECK(fh_normal_quad(1000, &q) && q.slot == 8 && q.su == 384 && q.sv == 624, "E 1000");
    /* empty / out of range */
    CHECK(!fh_normal_quad(0, &q), "empty");
    CHECK(!fh_normal_quad(8192, &q), "max");
    CHECK(!fh_normal_quad(2048, &q), "auto-is-not-normal");
    /* autotile predicates */
    CHECK(fh_is_autotile(2048) && !fh_is_autotile(1663), "isauto");
    CHECK(fh_auto_kind(2048) == 0 && fh_auto_shape(2048) == 0, "kind/shape 0");
    CHECK(fh_auto_kind(2096) == 1 && fh_auto_shape(3591) == 7, "kind/shape");
    /* A1 kind0/shape0 -> set 0, bx 0, by 0, FLOOR */
    int set, bx, by, tab;
    CHECK(fh_auto_cell(2048, &set, &bx, &by, &tab) && set == 0 && bx == 0 &&
          by == 0 && tab == 0, "A1 kind0");
    /* A1 kind1 -> bx 0, by 3 */
    CHECK(fh_auto_cell(2096, &set, &bx, &by, &tab) && set == 0 && bx == 0 && by == 3,
          "A1 kind1");
    /* A1 kind4 (even, floor): tx=4,ty=0 -> bx=8, by=0 */
    CHECK(fh_auto_cell(2048 + 4 * 48, &set, &bx, &by, &tab) && bx == 8 && by == 0 && tab == 0,
          "A1 kind4");
    /* A1 kind5 (odd, waterfall): bx=8+6=14, table WATERFALL */
    CHECK(fh_auto_cell(2048 + 5 * 48, &set, &bx, &by, &tab) && bx == 14 && tab == 2,
          "A1 kind5");
    /* A2 tid 3591: engine kind=(3591-2048)/48=32 (A1-relative!), tx0 ty4 ->
       set 1, bx 0, by (4-2)*3=6, FLOOR */
    CHECK(fh_auto_cell(3591, &set, &bx, &by, &tab) && set == 1 &&
          bx == 0 && by == 6 && tab == 0, "A2 kind32");
    /* A3 tid 4352: engine kind=(4352-2048)/48=48, tx0 ty6 ->
       set 2, bx 0, by (6-6)*2=0, WALL */
    CHECK(fh_auto_cell(4352, &set, &bx, &by, &tab) && set == 2 &&
          bx == 0 && by == 0 && tab == 1, "A3 kind48");
    if (fails) { printf("%d FAILURES\n", fails); return 1; }
    printf("MAP UNIT ALL PASS\n");
    return 0;
}
