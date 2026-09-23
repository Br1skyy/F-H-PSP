

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../runtime/interp.h"
#include "../runtime/text.h"
#include "test_vec.h"

#define N(x) (sizeof(x) / sizeof((x)[0]))

static int fails = 0;


static void dec_put_esc(char *out, int *n, const char *p, int len) {
    for (int i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)p[i];
        if (isalnum(ch) || strchr(" ., '\"!?()-_:;/", ch)) {
            out[(*n)++] = (char)ch;
        } else {
            *n += sprintf(out + *n, "\\x%02x", ch);
        }
    }
}

static char decbuf[32768];
static int decn;
static void dec_text(const char *ptr, int len, void *ud) {
    (void)ud;
    decbuf[decn++] = 'T';
    dec_put_esc(decbuf, &decn, ptr, len);
    decbuf[decn++] = ';';
}
static void dec_code(const char *code, int param, void *ud) {
    (void)ud;
    decn += sprintf(decbuf + decn, "C%s,%d;", code, param);
}

#define CHECK(t, cond, fmt, ...) do { \
    if (!(cond)) { printf("FAIL %s: " fmt "\n", t, ##__VA_ARGS__); fails++; } \
} while (0)

static void run_one(const char *name, const FhCmd *list, int len,
                    const int *xtrace, int xn, const int (*xsw)[2], int xswn,
                    const int (*xvar)[2], int xvarn, const char *xtext,
                    int xwaits, int xunknown, int branch, int sel,
                    const int (*isw)[2], int iswn,
                    const int *ce_ids, const FhCmd **ce_lists, const int *ce_lens, int ce_n,
                    const int *xparty, int xpartyn, const int *xtint,
                    const char *xse, int xsen,
                    const int (*xroutes)[3], int xrouten,
                    const int (*xastate)[3], int xastaten,
                    const int (*xinv)[3], int xinvn,
                    const int (*xhp)[2], int xhpn,
                    const int (*xmp)[2], int xmpn,
                    const int (*xanims)[3], int xanimsn,
                    const int (*xchpos)[4], int xchposn,
                    const int (*ihp)[2], int ihpn,
                    int xrefresh, const char *xchname,
                    const char *xplug, int xprogn, const char *xdec,
                    int xev, int xonmap, int xtransp, int xfoll,
                    const int *xtransfer,
                    const int (*xballoons)[2], int xballoonn,
                    const int *xerased, int xerasedn,
                    const int (*xscrolls)[3], int xscrolln,
                    const int (*xselfsw)[2], int xselfswn,
                    const int *xtimer, int xgold, const char *xbbgm,
                    int xmenu, int xfade, const int *xflash, const int *xshake,
                    const char *xpics, const int *xweather,
                    int xtroop, const int (*xehp0)[2], int xehpn0,
                    const int (*xxp)[2], int xxpn,
                    const int (*xlev)[2], int xlevn,
                    const int (*xapram)[3], int xapramn,
                    const int (*xskills)[3], int xskillsn,
                    const int (*xequip)[3], int xequipn,
                    const char *xanames, const int (*xaclass)[2], int xaclassn,
                    const char *xanick, const char *xaprof,
                    const int (*xehp)[2], int xehpn,
                    const int (*xemp)[2], int xempn,
                    const int (*xetp)[2], int xetpn,
                    const int (*xestate)[3], int xestaten,
                    const int *xeappear, int xeappearn,
                    const int (*xetransform)[2], int xetransformn,
                    const int *xbattle,
                    const int (*xshop)[3], int xshopn, int xshop_only,
                    const int *xnameinput, int xscene,
                    const int *xnuminput, const int *xitemchoice,
                    const char *xaudio, const int *xaflag,
                    const char *xsysnames, const int *xsysflag,
                    const int *xwtone, const char *xvehbgm,
                    const char *xbattlebacks, const char *xparallax, const int *xparallaxn,
                    const int *xlocinfo, const int *xvehloc,
                    int xvehin, int xgather, const char *xmovie,
                    const int *xforce) {
    FhInterp it;
    fh_interp_init(&it, list, len);
    it.branch = branch;
    it.choice_sel = sel;
    it.ev_id = xev;
    it.on_map = xonmap;
    it.troop_n = xtroop;
    it.ce_ids = ce_ids; it.ce_lists = ce_lists; it.ce_lens = ce_lens; it.ce_n = ce_n;
    for (int i = 0; i < iswn; i++) it.sw[isw[i][0]] = (unsigned char)isw[i][1];
    for (int i = 0; i < ihpn; i++) it.hp[ihp[i][0]] = ihp[i][1];
    for (int i = 0; i < xehpn0; i++) it.ehp[xehp0[i][0]] = xehp0[i][1];
    int r = fh_interp_run(&it, 100000);
    CHECK(name, r == FH_RUN_END, "run did not end (r=%d pc=%d)", r, it.pc);
    CHECK(name, it.await_choice == 0, "await_choice left set");    CHECK(name, it.trace_len == xn, "trace len %d != expected %d", it.trace_len, xn);
    int n = it.trace_len < xn ? it.trace_len : xn;
    for (int i = 0; i < n; i++)
        if (it.trace[i] != xtrace[i]) {
            printf("FAIL %s: trace[%d] = %d, expected %d\n", name, i, it.trace[i], xtrace[i]);
            fails++;
            break;
        }
    for (int i = 0; i < xswn; i++)
        CHECK(name, it.sw[xsw[i][0]] == xsw[i][1], "sw[%d] = %d, expected %d",
              xsw[i][0], it.sw[xsw[i][0]], xsw[i][1]);
    for (int i = 0; i < xvarn; i++)
        CHECK(name, it.var[xvar[i][0]] == xvar[i][1], "var[%d] = %d, expected %d",
              xvar[i][0], it.var[xvar[i][0]], xvar[i][1]);
    CHECK(name, strcmp(it.text, xtext) == 0, "text mismatch:\n--- got ---\n%s\n--- want ---\n%s",
          it.text, xtext);
    CHECK(name, it.waits == xwaits, "waits %d != %d", it.waits, xwaits);
    CHECK(name, it.unknown == xunknown, "unknown %d != %d", it.unknown, xunknown);
    CHECK(name, it.party_n == xpartyn, "party_n %d != %d", it.party_n, xpartyn);
    for (int i = 0; i < xpartyn && i < it.party_n; i++)
        CHECK(name, it.party[i] == xparty[i], "party[%d] = %d, expected %d",
              i, it.party[i], xparty[i]);
    for (int i = 0; i < 4; i++)
        CHECK(name, it.tint[i] == xtint[i], "tint[%d] = %d, expected %d", i, it.tint[i], xtint[i]);
    CHECK(name, it.tint_frames == xtint[4], "tint_frames %d != %d", it.tint_frames, xtint[4]);
    CHECK(name, strcmp(it.last_se, xse) == 0, "last_se '%s' != '%s'", it.last_se, xse);
    CHECK(name, it.se_count == xsen, "se_count %d != %d", it.se_count, xsen);
    CHECK(name, it.route_n == xrouten, "route_n %d != %d", it.route_n, xrouten);
    for (int i = 0; i < xrouten && i < it.route_n; i++) {
        CHECK(name, it.routes[i].ch == xroutes[i][0] && it.routes[i].steps == xroutes[i][1],
              "route[%d] = (%d,%d), expected (%d,%d)", i,
              it.routes[i].ch, it.routes[i].steps, xroutes[i][0], xroutes[i][1]);
    }
    for (int i = 0; i < xastaten; i++)
        CHECK(name, it.astate[xastate[i][0]][xastate[i][1]] == xastate[i][2],
              "astate[%d][%d] = %d, expected %d", xastate[i][0], xastate[i][1],
              it.astate[xastate[i][0]][xastate[i][1]], xastate[i][2]);
    for (int i = 0; i < xinvn; i++) {
        int got = -1;
        if (xinv[i][0] == 126 && xinv[i][1] < FH_MAX_ITEMS) got = it.inv_item[xinv[i][1]];
        if (xinv[i][0] == 127 && xinv[i][1] < FH_MAX_WEAPONS) got = it.inv_weap[xinv[i][1]];
        if (xinv[i][0] == 128 && xinv[i][1] < FH_MAX_ARMORS) got = it.inv_arm[xinv[i][1]];
        CHECK(name, got == xinv[i][2], "inv(%d,%d) = %d, expected %d",
              xinv[i][0], xinv[i][1], got, xinv[i][2]);
    }
    for (int i = 0; i < xhpn; i++)
        CHECK(name, it.hp[xhp[i][0]] == xhp[i][1], "hp[%d] = %d, expected %d",
              xhp[i][0], it.hp[xhp[i][0]], xhp[i][1]);
    for (int i = 0; i < xmpn; i++)
        CHECK(name, it.mp[xmp[i][0]] == xmp[i][1], "mp[%d] = %d, expected %d",
              xmp[i][0], it.mp[xmp[i][0]], xmp[i][1]);
    CHECK(name, it.anim_n == xanimsn, "anim_n %d != %d", it.anim_n, xanimsn);
    for (int i = 0; i < xanimsn && i < it.anim_n; i++)
        CHECK(name, it.anims[i].ch == xanims[i][0] && it.anims[i].anim == xanims[i][1] &&
                     it.anims[i].mirror == xanims[i][2], "anims[%d] = (%d,%d,%d), expected (%d,%d,%d)",
              i,
              it.anims[i].ch, it.anims[i].anim, it.anims[i].mirror, xanims[i][0], xanims[i][1], xanims[i][2]);
    for (int i = 0; i < xchposn; i++) {
        int c = xchpos[i][0];
        CHECK(name, it.chx[c] == xchpos[i][1] && it.chy[c] == xchpos[i][2] && it.chd[c] == xchpos[i][3],
              "chpos[%d] = (%d,%d,%d), expected (%d,%d,%d)", c,
              it.chx[c], it.chy[c], it.chd[c], xchpos[i][1], xchpos[i][2], xchpos[i][3]);
    }
    CHECK(name, it.refresh_n == xrefresh, "refresh_n %d != %d", it.refresh_n, xrefresh);
    {
        char tmp[4096];
        strncpy(tmp, xchname, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        int seen = 0;
        for (char *line = strtok(tmp, "\n"); line; line = strtok(NULL, "\n")) {
            int a = -1, ix = -1;
            char kind[4] = "", nm[FH_NAME_CAP] = "";
            if (sscanf(line, "%d:%3[^=]=%31[^,],%d", &a, kind, nm, &ix) != 4) {
                printf("FAIL %s: bad appearance line '%s'\n", name, line);
                fails++;
                continue;
            }
            seen++;
            if (!strcmp(kind, "ch"))
                CHECK(name, !strcmp(it.chname[a], nm) && it.chidx[a] == ix,
                      "chname[%d] = '%s',%d, expected '%s',%d", a, it.chname[a], it.chidx[a], nm, ix);
            else if (!strcmp(kind, "bt"))
                CHECK(name, !strcmp(it.btname[a], nm), "btname[%d] = '%s', expected '%s'",
                      a, it.btname[a], nm);
            else if (!strcmp(kind, "fc"))
                CHECK(name, !strcmp(it.fcname[a], nm) && it.fcidx[a] == ix,
                      "fcname[%d] = '%s',%d, expected '%s',%d", a, it.fcname[a], it.fcidx[a], nm, ix);
        }
        (void)seen;
    }
    {
        char tmp[8192];
        strncpy(tmp, xplug, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        int i = 0;
        CHECK(name, it.plug_n == xprogn, "plug_n %d != %d", it.plug_n, xprogn);
        for (char *line = strtok(tmp, "\n"); line; line = strtok(NULL, "\n"), i++) {
            char nm[32] = "", ag[96] = "";
            sscanf(line, "%31s %95[^\n]", nm, ag);
            if (i < it.plug_n)
                CHECK(name, !strcmp(it.plug[i].name, nm) && !strcmp(it.plug[i].args, ag),
                      "plug[%d] = '%s %s', expected '%s %s'", i,
                      it.plug[i].name, it.plug[i].args, nm, ag);
        }
    }
    {
        FhEscCtx ctx;
        ctx.vars = it.var; ctx.nvars = FH_MAX_VARS;
        ctx.actor_names = fh_actnames; ctx.nactors = fh_nactors;
        ctx.party = it.party; ctx.party_n = it.party_n;
        ctx.currency = fh_currency;
        FhEscCb cb;
        cb.on_text = dec_text; cb.on_code = dec_code; cb.ud = NULL;
        decn = 0;
        decbuf[0] = '\0';
        fh_decode_escapes(it.text, &ctx, &cb);
        decbuf[decn] = '\0';
        CHECK(name, !strcmp(decbuf, xdec), "decode mismatch:\n--- got ---\n%s\n--- want ---\n%s",
              decbuf, xdec);
    }
    CHECK(name, it.transparent == xtransp, "transparent %d != %d", it.transparent, xtransp);
    CHECK(name, it.followers_on == xfoll, "followers %d != %d", it.followers_on, xfoll);
    CHECK(name, it.transfer.map == xtransfer[0] && it.transfer.x == xtransfer[1] &&
               it.transfer.y == xtransfer[2] && it.transfer.dir == xtransfer[3] &&
               it.transfer.fade == xtransfer[4] && it.transfer.pending == xtransfer[5],
          "transfer (%d,%d,%d,%d,%d,%d)", it.transfer.map, it.transfer.x, it.transfer.y,
          it.transfer.dir, it.transfer.fade, it.transfer.pending);
    CHECK(name, it.balloon_n == xballoonn, "balloon_n %d != %d", it.balloon_n, xballoonn);
    for (int i = 0; i < xballoonn && i < it.balloon_n; i++)
        CHECK(name, it.balloons[i].ch == xballoons[i][0] && it.balloons[i].icon == xballoons[i][1],
              "balloon[%d] = (%d,%d)", i, it.balloons[i].ch, it.balloons[i].icon);
    CHECK(name, it.erased_n == xerasedn, "erased_n %d != %d", it.erased_n, xerasedn);
    for (int i = 0; i < xerasedn && i < it.erased_n; i++)
        CHECK(name, it.erased[i] == xerased[i], "erased[%d] = %d, expected %d",
              i, it.erased[i], xerased[i]);
    CHECK(name, it.scroll_n == xscrolln, "scroll_n %d != %d", it.scroll_n, xscrolln);
    for (int i = 0; i < xscrolln && i < it.scroll_n; i++)
        CHECK(name, it.scrolls[i].dir == xscrolls[i][0] && it.scrolls[i].dist == xscrolls[i][1],
              "scroll[%d] = (%d,%d)", i, it.scrolls[i].dir, it.scrolls[i].dist);
    for (int i = 0; i < xselfswn; i++) {
        int ev = xselfsw[i][0] / 4, L = xselfsw[i][0] % 4;
        CHECK(name, it.selfsw[ev][L] == xselfsw[i][1], "selfsw[%d][%d] = %d, expected %d",
              ev, L, it.selfsw[ev][L], xselfsw[i][1]);
    }
    CHECK(name, it.timer_on == xtimer[0] && it.timer_frames == xtimer[1],
          "timer (%d,%d) != (%d,%d)", it.timer_on, it.timer_frames, xtimer[0], xtimer[1]);
    CHECK(name, it.gold == xgold, "gold %d != %d", it.gold, xgold);
    CHECK(name, !strcmp(it.bbgm, xbbgm), "bbgm '%s' != '%s'", it.bbgm, xbbgm);
    CHECK(name, it.menu_disabled == xmenu, "menu %d != %d", it.menu_disabled, xmenu);
    CHECK(name, it.fade == xfade, "fade %d != %d", it.fade, xfade);
    for (int i = 0; i < 4; i++)
        CHECK(name, it.flash[i] == xflash[i], "flash[%d] = %d, expected %d", i, it.flash[i], xflash[i]);
    CHECK(name, it.flash_frames == xflash[4], "flash_frames %d != %d", it.flash_frames, xflash[4]);
    for (int i = 0; i < 3; i++)
        CHECK(name, it.shake[i] == xshake[i], "shake[%d] = %d, expected %d", i, it.shake[i], xshake[i]);
    {
        char tmp[8192];
        strncpy(tmp, xpics, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        for (char *line = strtok(tmp, "\n"); line; line = strtok(NULL, "\n")) {
            int pid, used, o, x, y, sx, sy, op, bl, rot, t0, t1, t2, t3;
            char nm[FH_NAME_CAP] = "";
            if (sscanf(line, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d,%d,%d,%d:%31[^\n]",
                       &pid, &used, &o, &x, &y, &sx, &sy, &op, &bl, &rot,
                       &t0, &t1, &t2, &t3, nm) < 14) {
                printf("FAIL %s: bad pic line '%s'\n", name, line);
                fails++;
                continue;
            }
            if (pid < 1 || pid > FH_MAX_PICS) { printf("FAIL %s: pic id %d\n", name, pid); fails++; continue; }
            CHECK(name, it.pic[pid - 1].used == used, "pic%d.used %d != %d", pid, it.pic[pid - 1].used, used);
            CHECK(name, it.pic[pid - 1].x == x && it.pic[pid - 1].y == y, "pic%d.xy (%d,%d) != (%d,%d)",
                  pid, it.pic[pid - 1].x, it.pic[pid - 1].y, x, y);
            CHECK(name, it.pic[pid - 1].op == op && it.pic[pid - 1].blend == bl, "pic%d.op/blend", pid);
            CHECK(name, it.pic[pid - 1].rot == rot, "pic%d.rot %d != %d", pid, it.pic[pid - 1].rot, rot);
            CHECK(name, it.pic[pid - 1].tone[0] == t0 && it.pic[pid - 1].tone[3] == t3, "pic%d.tone", pid);
            CHECK(name, !strcmp(it.pic[pid - 1].name, nm), "pic%d.name '%s' != '%s'",
                  pid, it.pic[pid - 1].name, nm);
        }
    }
    for (int i = 0; i < 3; i++)
        CHECK(name, it.weather[i] == xweather[i], "weather[%d] = %d, expected %d",
              i, it.weather[i], xweather[i]);
    for (int i = 0; i < xxpn; i++)
        CHECK(name, it.exp_[xxp[i][0]] == xxp[i][1], "exp[%d] = %d, expected %d",
              xxp[i][0], it.exp_[xxp[i][0]], xxp[i][1]);
    for (int i = 0; i < xlevn; i++)
        CHECK(name, it.level[xlev[i][0]] == xlev[i][1], "level[%d] = %d, expected %d",
              xlev[i][0], it.level[xlev[i][0]], xlev[i][1]);
    for (int i = 0; i < xapramn; i++)
        CHECK(name, it.apram[xapram[i][0]][xapram[i][1]] == xapram[i][2], "apram");
    for (int i = 0; i < xskillsn; i++)
        CHECK(name, it.askill[xskills[i][0]][xskills[i][1]] == xskills[i][2], "skill");
    for (int i = 0; i < xequipn; i++)
        CHECK(name, it.equip[xequip[i][0]][xequip[i][1]] == xequip[i][2], "equip");
    {
        char tmp[8192], *line;
        strncpy(tmp, xanames, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = '\0';
        for (line = strtok(tmp, "\n"); line; line = strtok(NULL, "\n")) {
            int a = atoi(line);
            const char *eq = strchr(line, '=');
            CHECK(name, eq && a >= 0 && a < FH_MAX_ACTORS && !strcmp(it.aname[a], eq + 1),
                  "aname[%d] '%s'", a, eq ? eq + 1 : "?");
        }
        strncpy(tmp, xanick, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = '\0';
        for (line = strtok(tmp, "\n"); line; line = strtok(NULL, "\n")) {
            int a = atoi(line);
            const char *eq = strchr(line, '=');
            CHECK(name, eq && a >= 0 && a < FH_MAX_ACTORS && !strcmp(it.anick[a], eq + 1),
                  "anick[%d]", a);
        }
        strncpy(tmp, xaprof, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = '\0';
        for (line = strtok(tmp, "\n"); line; line = strtok(NULL, "\n")) {
            int a = atoi(line);
            const char *eq = strchr(line, '=');
            CHECK(name, eq && a >= 0 && a < FH_MAX_ACTORS && !strcmp(it.aprof[a], eq + 1),
                  "aprof[%d]", a);
        }
    }
    for (int i = 0; i < xaclassn; i++)
        CHECK(name, it.aclass[xaclass[i][0]] == xaclass[i][1], "class");
    for (int i = 0; i < xehpn; i++)
        CHECK(name, it.ehp[xehp[i][0]] == xehp[i][1], "ehp[%d] = %d, expected %d",
              xehp[i][0], it.ehp[xehp[i][0]], xehp[i][1]);
    for (int i = 0; i < xempn; i++)
        CHECK(name, it.emp[xemp[i][0]] == xemp[i][1], "emp");
    for (int i = 0; i < xetpn; i++)
        CHECK(name, it.etp[xetp[i][0]] == xetp[i][1], "etp");
    for (int i = 0; i < xestaten; i++)
        CHECK(name, it.estate[xestate[i][0]][xestate[i][1]] == xestate[i][2], "estate");
    for (int i = 0; i < xeappearn; i++)
        CHECK(name, it.eappear[xeappear[i]] == 1, "eappear[%d]", xeappear[i]);
    for (int i = 0; i < xetransformn; i++)
        CHECK(name, it.etransform[xetransform[i][0]] == xetransform[i][1], "etransform");
    CHECK(name, it.battle.troop == xbattle[0] && it.battle.esc == xbattle[1] &&
               it.battle.lose == xbattle[2] && it.battle.pending == xbattle[3],
          "battle got (%d,%d,%d,%d) want (%d,%d,%d,%d)",
          it.battle.troop, it.battle.esc, it.battle.lose, it.battle.pending,
          xbattle[0], xbattle[1], xbattle[2], xbattle[3]);
    CHECK(name, it.shop_n == xshopn, "shop_n %d != %d", it.shop_n, xshopn);
    for (int i = 0; i < xshopn && i < it.shop_n; i++)
        CHECK(name, it.shop[i].type == xshop[i][0] && it.shop[i].id == xshop[i][1] &&
                     it.shop[i].price == xshop[i][2], "shop[%d]", i);
    CHECK(name, it.shop_only == xshop_only, "shop_only");
    CHECK(name, it.nameinput[0] == xnameinput[0] && it.nameinput[1] == xnameinput[1], "nameinput");
    CHECK(name, it.scene_req == xscene, "scene %d != %d", it.scene_req, xscene);
    CHECK(name, it.numinput[0] == xnuminput[0], "numinput");
    CHECK(name, it.itemchoice[0] == xitemchoice[0], "itemchoice");
    {
        char tmp[2048], *line;
        strncpy(tmp, xaudio, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = '\0';
        for (line = strtok(tmp, "\n"); line; line = strtok(NULL, "\n")) {
            int code = atoi(line);
            const char *eq = strchr(line, '=');
            const char *want = eq ? eq + 1 : "";
            const char *got = "";
            if (code == 241) got = it.last_bgm;
            else if (code == 245) got = it.last_bgs;
            else if (code == 249) got = it.last_me;
            CHECK(name, !strcmp(got, want), "audio %d '%s' != '%s'", code, got, want);
        }
    }
    CHECK(name, it.bgm_fade == xaflag[0] && it.bgs_fade == xaflag[1] && it.se_stop == xaflag[2] &&
               it.bgm_saved == xaflag[3] && it.bgm_replayed == xaflag[4], "aflag");
    {
        char tmp[512], *line;
        strncpy(tmp, xsysnames, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = '\0';
        for (line = strtok(tmp, "\n"); line; line = strtok(NULL, "\n")) {
            const char *eq = strchr(line, '=');
            if (!eq) continue;
            if (!strncmp(line, "victory", 7))
                CHECK(name, !strcmp(it.victory_me, eq + 1), "victory_me");
            else CHECK(name, !strcmp(it.defeat_me, eq + 1), "defeat_me");
        }
    }
    CHECK(name, it.save_on == xsysflag[0] && it.encounter_on == xsysflag[1] &&
               it.formation_on == xsysflag[2] && it.namedisp == xsysflag[3], "sysflag");
    CHECK(name, it.tileset == xsysflag[4], "tileset %d != %d", it.tileset, xsysflag[4]);
    for (int i = 0; i < 4; i++)
        CHECK(name, it.wtone[i] == xwtone[i], "wtone");
    {
        int v = atoi(xvehbgm);
        const char *eq = strchr(xvehbgm, '=');
        CHECK(name, it.vehbgm.veh == v && eq && !strcmp(it.vehbgm.name, eq + 1), "vehbgm");
    }
    {
        char tmp[256], *line;
        strncpy(tmp, xbattlebacks, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = '\0';
        line = strtok(tmp, "\n");
        if (line) CHECK(name, !strcmp(it.battleback1, line), "battleback1 '%s'", line);
        line = line ? strtok(NULL, "\n") : NULL;
        if (line) CHECK(name, !strcmp(it.battleback2, line), "battleback2 '%s'", line);
    }
    CHECK(name, !strcmp(it.parallax.name, xparallax) &&
               it.parallax.loopx == xparallaxn[0] && it.parallax.loopy == xparallaxn[1] &&
               it.parallax.sx == xparallaxn[2] && it.parallax.sy == xparallaxn[3], "parallax");
    CHECK(name, it.locinfo.var == xlocinfo[0] && it.locinfo.type == xlocinfo[1], "locinfo");
    CHECK(name, it.vehloc.veh == xvehloc[0] && it.vehloc.map == xvehloc[1], "vehloc");
    CHECK(name, it.vehicle_in == xvehin, "vehicle_in");
    CHECK(name, it.gather == xgather, "gather");
    CHECK(name, !strcmp(it.movie, xmovie), "movie '%s' != '%s'", it.movie, xmovie);
    CHECK(name, it.force.side == xforce[0] && it.force.idx == xforce[1] &&
               it.force.skill == xforce[2] && it.force.target == xforce[3] &&
               it.force.pending == xforce[4],
          "force got (%d,%d,%d,%d,%d) want (%d,%d,%d,%d,%d)",
          it.force.side, it.force.idx, it.force.skill, it.force.target, it.force.pending,
          xforce[0], xforce[1], xforce[2], xforce[3], xforce[4]);
    if (!fails) printf("ok %s (steps=%d)\n", name, xn);
}

#define T(t) run_one(#t, t##_list, N(t##_list), t##_trace, N(t##_trace), \
                     t##_sw, t##_swn, t##_var, t##_varn, t##_text, \
                     t##_waits, t##_unknown, t##_init_branch, t##_init_sel, \
                     t##_init_sw, t##_init_swn, \
                     t##_ce_ids, t##_ce_lists, t##_ce_lens, t##_ce_n, \
                     t##_party, t##_partyn, t##_tint, t##_se, t##_sen, \
                     t##_routes, t##_routen, t##_astate, t##_astaten, \
                     t##_inv, t##_invn, t##_hp, t##_hpn, t##_mp, t##_mpn, \
                     t##_anims, t##_animsn, t##_chpos, t##_chposn, \
                     t##_init_hp, t##_init_hpn, \
                     t##_refresh, t##_chname, t##_plug, t##_progn, t##_dec, \
                     t##_ev, t##_onmap, t##_transp, t##_foll, t##_transfer, \
                     t##_balloons, t##_balloonn, t##_erased, t##_erasedn, \
                     t##_scrolls, t##_scrolln, \
                     t##_selfsw, t##_selfswn, t##_timer, t##_gold, t##_bbgm, \
                     t##_menu, t##_fade, t##_flash, t##_shake, t##_pics, t##_weather, \
                     t##_troop, t##_init_ehp, t##_init_ehpn, \
                     t##_exp, t##_expn, t##_level, t##_leveln, \
                     t##_apram, t##_apramn, t##_skills, t##_skillsn, \
                     t##_equip, t##_equipn, t##_anames, t##_aclass, t##_aclassn, \
                     t##_anick, t##_aprof, \
                     t##_ehp, t##_ehpn, t##_emp, t##_empn, t##_etp, t##_etpn, \
                     t##_estate, t##_estaten, t##_eappear, t##_eappearn, \
                     t##_etransform, t##_etransformn, t##_battle, \
                     t##_shop, t##_shopn, t##_shop_only, \
                     t##_nameinput, t##_scene, t##_numinput, t##_itemchoice, \
                     t##_audio, t##_aflag, t##_sysnames, t##_sysflag, \
                     t##_wtone, t##_vehbgm, t##_battlebacks, \
                     t##_parallax, t##_parallaxn, t##_locinfo, t##_vehloc, \
                     t##_vehin, t##_gather, t##_movie, t##_force)

int main(void) {
    T(T1_switches);
    T(T2_conditional);
    T(T2b_conditional_true);
    T(T3_choices);
    T(T3b_choice_cancel);
    T(T4_battle_win);
    T(T4b_battle_lose);
    T(T5_common);
    T(T6_items_hp);
    T(T7_locate_anim);
    T(T8_escapes);
    T(T9_costume);
    T(T10_plugin);
    T(T11_presence);
    T(T12_graphic);
    T(T13_transfer);
    T(T14_scroll);
    T(T15_flow);
    T(T16_flash);
    T(T16b_shake);
    T(T16c_fade);
    T(T17_pictures);
    T(T17b_weather);
    T(T18_economy);
    T(T18b_timer);
    T(T19_equip);
    T(T20_battle);
    T(T21_enemies);
    T(T21b_enemyanim);
    T(T22_actoradmin);
    T(T23_gameover);
    T(T24_me);
    T(T24b_stopse);
    T(T24c_bgs);
    T(T25_battleback);
    T(T25b_tileset);
    T(T25c_appear);
    T(T26_rottenmeat);
    T(T26b_leaveit);
    T(T27_golemrite);
    if (fails) { printf("%d FAILURES\n", fails); return 1; }
    printf("ALL PASS\n");
    return 0;
}
