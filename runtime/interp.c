

#include "interp.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void fh_interp_init(FhInterp *it, const FhCmd *list, int len) {
    memset(it, 0, sizeof(*it));
    it->list = list;
    it->len = len;
    it->rng = 1;

    it->save_on = 1;
    it->encounter_on = 1;
    it->formation_on = 1;
    it->namedisp = 1;
    it->followers_on = 1;
}

static void emit_text(FhInterp *it, const char *s) {
    if (!s) return;
    int n = (int)strlen(s);
    if (it->text_len + n + 1 >= FH_TEXT_CAP) return;
    memcpy(it->text + it->text_len, s, (size_t)n);
    it->text_len += n;
    it->text[it->text_len++] = '\n';
    it->text[it->text_len] = '\0';
}

static int eval_111(const FhInterp *it, const FhCmd *c) {


    if (c->op == 8) {
        int id = c->p[0];
        if (id < 0 || id >= FH_MAX_ITEMS) return 0;
        return it->inv_item[id] > 0;
    }
    if (c->op == 4) {
        int a = c->p[0], sub = c->p[1], n = c->p[2];
        if (a < 0 || a >= FH_MAX_ACTORS) return 0;
        if (sub == 0) {
            for (int i = 0; i < it->party_n; i++)
                if (it->party[i] == a) return 1;
            return 0;
        }
        if (sub == 1) {
            if (!c->s) return 0;
            return strcmp(it->aname[a], c->s) == 0;
        }
        if (sub == 2) return it->aclass[a] == n;
        if (sub == 3)
            return n >= 0 && n < FH_MAX_SKILLS && it->askill[a][n];
        if (sub == 4 || sub == 5) {
            for (int s = 0; s < 8; s++)
                if (it->equip[a][s] == n) return 1;
            return 0;
        }
        if (sub == 6)
            return n >= 0 && n < FH_MAX_STATES && it->astate[a][n];
        return 0;
    }
    if (c->op == 5) {
        int e = c->p[0], sub = c->p[1], n = c->p[2];
        if (e < 0 || e >= it->troop_n || e >= FH_MAX_ENEMIES) return 0;
        if (sub == 0) return it->ehp[e] > 0;
        if (sub == 1)
            return n >= 0 && n < FH_MAX_STATES && it->estate[e][n];
        return 0;
    }
    if (c->op == 8) {
        int id = c->p[0];
        if (id < 0 || id >= FH_MAX_ITEMS) return 0;
        return it->inv_item[id] > 0;
    }
    if (c->op == 0) {
        int id = c->p[0], want = c->p[1];
        if (id < 0 || id >= FH_MAX_SWITCHES) return 0;
        return it->sw[id] == (want == 0);
    }
    if (c->op == 1) {
        int id = c->p[0];
        if (id < 0 || id >= FH_MAX_VARS) return 0;
        int v1 = it->var[id], v2 = c->p[2];
        if (c->p[1] == 1) {
            if (v2 < 0 || v2 >= FH_MAX_VARS) return 0;
            v2 = it->var[v2];
        } else if (c->p[1] != 0) {
            return 0;
        }
        switch (c->p[3]) {
            case 0: return v1 == v2;
            case 1: return v1 >= v2;
            case 2: return v1 <= v2;
            case 3: return v1 > v2;
            case 4: return v1 < v2;
            case 5: return v1 != v2;
        }
        return 0;
    }
    return 0;
}

int fh_interp_step(FhInterp *it) {
    int guard = 1 << 20;
    for (;;) {
        while (it->pc >= it->len && it->depth > 0) {
            it->depth--;
            it->list = it->callst[it->depth].list;
            it->len = it->callst[it->depth].len;
            it->pc = it->callst[it->depth].pc;
        }
        if (guard-- <= 0) break;
        if (it->pc >= it->len) {
            if (it->page_open == 1 && it->text_len > 0) {

                it->page_open = 2;
                return FH_RUN_PAGE;
            }
            break;
        }
        if (it->wait > 0) {
            it->wait--;
            it->waits++;
            return FH_RUN_WAIT;
        }
        const FhCmd *c = &it->list[it->pc];


        if (!it->trace_skip && it->trace_len < FH_TRACE_CAP)
            it->trace[it->trace_len++] = it->pc | (it->depth << 24);
        it->trace_skip = 0;
        switch (c->code) {
            case 0:
            case 108:
            case 112:
            case 404:
            case 412:
            case 604:
            case 505:
            case 118:
                it->pc++;
                break;
            case 115:
                it->pc = it->len;
                break;
            case 119:
                it->pc = c->jump;
                break;
            case 123: {
                int ev = it->ev_id, L = c->p[0];
                if (it->ev_id > 0 && ev >= 0 && ev < 512 && L >= 0 && L < 4)
                    it->selfsw[ev][L] = (unsigned char)(c->p[1] == 0 ? 1 : 0);
                it->pc++;
                break;
            }
            case 124:
                if (c->p[0] == 0) { it->timer_on = 1; it->timer_frames = c->p[1] * 60; }
                else it->timer_on = 0;
                it->pc++;
                break;
            case 125: {
                int v = (c->p[1] == 0) ? c->p[2] : 0;
                if (c->p[1] == 1 && c->p[2] >= 0 && c->p[2] < FH_MAX_VARS)
                    v = it->var[c->p[2]];
                else if (c->p[1] != 0 && c->p[1] != 1) { it->unknown++; it->pc++; break; }
                if (c->p[0] == 1) v = -v;
                else if (c->p[0] != 0) { it->unknown++; it->pc++; break; }
                it->gold += v;
                it->pc++;
                break;
            }
            case 132:
                if (c->s) snprintf(it->bbgm, FH_NAME_CAP, "%s", c->s);
                it->pc++;
                break;
            case 135:
                it->menu_disabled = (c->p[0] == 0);
                it->pc++;
                break;
            case 221:
                it->fade = -1;
                it->wait += 24;
                it->pc++;
                break;
            case 222:
                it->fade = 1;
                it->wait += 24;
                it->pc++;
                break;
            case 224:
                it->flash[0] = c->p[0]; it->flash[1] = c->p[1];
                it->flash[2] = c->p[2]; it->flash[3] = c->p[3];
                it->flash_frames = c->p[4];
                if (c->op) it->wait += c->p[4];
                it->pc++;
                break;
            case 225:
                it->shake[0] = c->p[0]; it->shake[1] = c->p[1]; it->shake[2] = c->p[2];
                if (c->op) it->wait += c->p[2];
                it->pc++;
                break;
            case 231: {
                int id = c->p[0];
                int x = (c->p[2] == 0) ? c->p[3] : 0;
                int y = (c->p[2] == 0) ? c->p[4] : 0;
                if (c->p[2] == 1) {
                    x = (c->p[3] >= 0 && c->p[3] < FH_MAX_VARS) ? it->var[c->p[3]] : 0;
                    y = (c->p[4] >= 0 && c->p[4] < FH_MAX_VARS) ? it->var[c->p[4]] : 0;
                }
                if (id >= 1 && id <= FH_MAX_PICS) {
                    it->pic[id - 1].used = 1;
                    it->pic[id - 1].origin = c->p[1];
                    it->pic[id - 1].x = x; it->pic[id - 1].y = y;
                    it->pic[id - 1].sx = c->p[5]; it->pic[id - 1].sy = c->p[6];
                    it->pic[id - 1].op = c->p[7]; it->pic[id - 1].blend = c->p[8];
                    if (c->s) snprintf(it->pic[id - 1].name, FH_NAME_CAP, "%s", c->s);
                }
                it->pc++;
                break;
            }
            case 232: {
                int id = c->p[0];
                int x = (c->p[2] == 0) ? c->p[3] : 0;
                int y = (c->p[2] == 0) ? c->p[4] : 0;
                if (c->p[2] == 1) {
                    x = (c->p[3] >= 0 && c->p[3] < FH_MAX_VARS) ? it->var[c->p[3]] : 0;
                    y = (c->p[4] >= 0 && c->p[4] < FH_MAX_VARS) ? it->var[c->p[4]] : 0;
                }
                if (id >= 1 && id <= FH_MAX_PICS && it->pic[id - 1].used) {
                    it->pic[id - 1].origin = c->p[1];
                    it->pic[id - 1].x = x; it->pic[id - 1].y = y;
                    it->pic[id - 1].sx = c->p[5]; it->pic[id - 1].sy = c->p[6];
                    it->pic[id - 1].op = c->p[7]; it->pic[id - 1].blend = c->p[8];
                }
                if (c->op) it->wait += c->p[9];
                it->pc++;
                break;
            }
            case 233: {
                int id = c->p[0];
                if (id >= 1 && id <= FH_MAX_PICS) it->pic[id - 1].rot = c->p[1];
                it->pc++;
                break;
            }
            case 234: {
                int id = c->p[0];
                if (id >= 1 && id <= FH_MAX_PICS && it->pic[id - 1].used) {
                    it->pic[id - 1].tone[0] = c->p[1];
                    it->pic[id - 1].tone[1] = c->p[2];
                    it->pic[id - 1].tone[2] = c->p[3];
                    it->pic[id - 1].tone[3] = c->p[4];
                }
                if (c->op) it->wait += c->p[5];
                it->pc++;
                break;
            }
            case 235: {
                int id = c->p[0];
                if (id >= 1 && id <= FH_MAX_PICS) it->pic[id - 1].used = 0;
                it->pc++;
                break;
            }
            case 236:
                it->weather[0] = c->p[0]; it->weather[1] = c->p[1]; it->weather[2] = c->p[2];
                if (c->op) it->wait += c->p[2];
                it->pc++;
                break;

            case 314: {
                int sel = c->p[0], idv = c->p[1], i;
                int ids[FH_MAX_PARTY + 1], nids = 0;
                if (sel == 0) {
                    if (idv == 0) {
                        for (i = 0; i < it->party_n; i++) ids[nids++] = it->party[i];
                    } else ids[nids++] = idv;
                } else if (idv >= 0 && idv < FH_MAX_VARS && it->var[idv] > 0) {
                    ids[nids++] = it->var[idv];
                }
                for (i = 0; i < nids; i++)
                    if (ids[i] >= 0 && ids[i] < FH_MAX_ACTORS) {
                        memset(it->astate[ids[i]], 0, FH_MAX_STATES);
                        it->tp[ids[i]] = 0;
                    }
                it->pc++;
                break;
            }
            case 315:
            case 316:
            case 317:
            case 326: {
                int sel = c->p[0], idv = c->p[1];
                int v = (c->p[3] == 0) ? c->p[4] : 0;
                if (c->p[3] == 1 && c->p[4] >= 0 && c->p[4] < FH_MAX_VARS)
                    v = it->var[c->p[4]];
                else if (c->p[3] != 0 && c->p[3] != 1) { it->unknown++; it->pc++; break; }
                if (c->p[2] == 1) v = -v;
                else if (c->p[2] != 0) { it->unknown++; it->pc++; break; }
                int ids[FH_MAX_PARTY + 1], nids = 0, i;
                if (sel == 0) {
                    if (idv == 0) {
                        for (i = 0; i < it->party_n; i++) ids[nids++] = it->party[i];
                    } else ids[nids++] = idv;
                } else if (idv >= 0 && idv < FH_MAX_VARS && it->var[idv] > 0) {
                    ids[nids++] = it->var[idv];
                }
                for (i = 0; i < nids; i++) {
                    int a = ids[i];
                    if (a < 0 || a >= FH_MAX_ACTORS) continue;
                    if (c->code == 315) it->exp_[a] += v;
                    else if (c->code == 316) it->level[a] += v;
                    else if (c->code == 326) it->tp[a] += v;
                    else if (c->p[5] >= 0 && c->p[5] < 8) it->apram[a][c->p[5]] += v;
                }
                it->pc++;
                break;
            }
            case 318: {
                int sel = c->p[0], idv = c->p[1], sk = c->p[3], i;
                int ids[FH_MAX_PARTY + 1], nids = 0;
                if (sel == 0) {
                    if (idv == 0) {
                        for (i = 0; i < it->party_n; i++) ids[nids++] = it->party[i];
                    } else ids[nids++] = idv;
                } else if (idv >= 0 && idv < FH_MAX_VARS && it->var[idv] > 0) {
                    ids[nids++] = it->var[idv];
                }
                for (i = 0; i < nids; i++)
                    if (ids[i] >= 0 && ids[i] < FH_MAX_ACTORS && sk >= 0 && sk < FH_MAX_SKILLS)
                        it->askill[ids[i]][sk] = (unsigned char)(c->p[2] == 0 ? 1 : 0);
                it->pc++;
                break;
            }
            case 319: {
                int a = c->p[0], slot = c->p[1];
                if (a >= 0 && a < FH_MAX_ACTORS && slot >= 0 && slot < 8)
                    it->equip[a][slot] = c->p[2];
                it->pc++;
                break;
            }
            case 320: {
                int a = c->p[0];
                if (a >= 0 && a < FH_MAX_ACTORS && c->s)
                    snprintf(it->aname[a], FH_NAME_CAP, "%s", c->s);
                it->pc++;
                break;
            }
            case 321: {
                int a = c->p[0];
                if (a >= 0 && a < FH_MAX_ACTORS) it->aclass[a] = c->p[1];
                it->pc++;
                break;
            }
            case 324: {
                int a = c->p[0];
                if (a >= 0 && a < FH_MAX_ACTORS && c->s)
                    snprintf(it->anick[a], FH_NAME_CAP, "%s", c->s);
                it->pc++;
                break;
            }
            case 325: {
                int a = c->p[0];
                if (a >= 0 && a < FH_MAX_ACTORS && c->s)
                    snprintf(it->aprof[a], sizeof(it->aprof[a]), "%s", c->s);
                it->pc++;
                break;
            }

            case 331:
            case 332:
            case 342: {
                int idx = c->p[0];
                int v = (c->p[2] == 0) ? c->p[3] : 0;
                if (c->p[2] == 1 && c->p[3] >= 0 && c->p[3] < FH_MAX_VARS)
                    v = it->var[c->p[3]];
                else if (c->p[2] != 0 && c->p[2] != 1) { it->unknown++; it->pc++; break; }
                if (c->p[1] == 1) v = -v;
                else if (c->p[1] != 0) { it->unknown++; it->pc++; break; }
                for (int e = 0; e < it->troop_n && e < FH_MAX_ENEMIES; e++) {
                    if (idx >= 0 && e != idx) continue;
                    if (c->code == 331) {
                        if (it->ehp[e] > 0) {
                            if (!c->p[4] && it->ehp[e] <= -v) v = 1 - it->ehp[e];
                            it->ehp[e] += v;
                        }
                    } else if (c->code == 332) {
                        it->emp[e] += v;
                        if (it->emp[e] < 0) it->emp[e] = 0;
                    } else it->etp[e] += v;
                }
                it->pc++;
                break;
            }
            case 333: {
                int idx = c->p[0], add = (c->p[1] == 0), st = c->p[2];
                for (int e = 0; e < it->troop_n && e < FH_MAX_ENEMIES; e++) {
                    if (idx >= 0 && e != idx) continue;
                    if (st >= 0 && st < FH_MAX_STATES)
                        it->estate[e][st] = (unsigned char)(add ? 1 : 0);
                }
                it->pc++;
                break;
            }
            case 334:


                for (int e = 0; e < it->troop_n && e < FH_MAX_ENEMIES; e++) {
                    int idx = c->p[0];
                    if (idx >= 0 && e != idx) continue;
                    memset(it->estate[e], 0, FH_MAX_STATES);
                    it->ehp[e] = it->emaxhp[e];
                    it->emp[e] = it->emaxmp[e];
                    it->erecover[e] = 1;
                }
                it->pc++;
                break;
            case 335:
            case 336: {
                int idx = c->p[0];
                for (int e = 0; e < it->troop_n && e < FH_MAX_ENEMIES; e++) {
                    if (idx >= 0 && e != idx) continue;
                    if (c->code == 335) it->eappear[e] = 1;
                    else it->etransform[e] = c->p[1];
                }
                it->pc++;
                break;
            }
            case 337:
                if (it->anim_n < FH_ANIM_Q) {
                    it->anims[it->anim_n].ch = -100 - c->p[0];
                    it->anims[it->anim_n].anim = c->p[1];
                    it->anims[it->anim_n].mirror = c->p[2] ? 1 : 0;
                    it->anim_n++;
                }
                it->pc++;
                break;
            case 339:
                it->force.side = c->p[0]; it->force.idx = c->p[1];
                it->force.skill = c->p[2]; it->force.target = c->p[3];
                it->force.pending = 1;
                it->pc++;
                break;
            case 340:
                it->battle.pending = 2;
                it->pc++;
                break;

            case 301:
                if (c->p[0] == 0) it->battle.troop = c->p[1];
                else if (c->p[0] == 1 && c->p[1] >= 0 && c->p[1] < FH_MAX_VARS)
                    it->battle.troop = it->var[c->p[1]];
                else it->battle.troop = -1;
                it->battle.esc = c->p[2]; it->battle.lose = c->p[3];
                it->battle.pending = 1;
                it->pc++;
                break;
            case 302: {
                int gi = 0;
                it->shop[gi].type = c->p[0]; it->shop[gi].id = c->p[1];
                it->shop[gi].price = c->p[2]; gi++;
                while (it->pc + gi < it->len && it->list[it->pc + gi].code == 605 && gi < 16) {
                    it->shop[gi].type = it->list[it->pc + gi].p[0];
                    it->shop[gi].id = it->list[it->pc + gi].p[1];
                    it->shop[gi].price = it->list[it->pc + gi].p[2];
                    gi++;
                }
                it->shop_n = gi;
                it->shop_only = c->p[3];
                it->pc += gi;
                break;
            }
            case 605:
                it->pc++;
                break;
            case 303:
                it->nameinput[0] = c->p[0]; it->nameinput[1] = c->p[1];
                it->pc++;
                break;
            case 351: it->scene_req = 1; it->pc++; break;
            case 352: it->scene_req = 2; it->pc++; break;
            case 353: it->scene_req = 3; it->pc++; break;
            case 354: it->scene_req = 4; it->pc++; break;
            case 103:
                it->numinput[0] = c->p[0]; it->numinput[1] = c->p[1]; it->numinput[2] = c->p[2];
                it->pc++;
                break;
            case 104:
                it->itemchoice[0] = c->p[0]; it->itemchoice[1] = c->p[1];
                it->pc++;
                break;
            case 105:
                it->text_len = 0;
                it->text[0] = '\0';
                it->pc++;
                break;

            case 241:
            case 245:
            case 249:
                if (c->s) {
                    char *dst = it->last_bgm;
                    if (c->code == 245) dst = it->last_bgs;
                    if (c->code == 249) dst = it->last_me;
                    snprintf(dst, FH_NAME_CAP, "%s", c->s);
                }
                it->pc++;
                break;
            case 242: it->bgm_fade = c->p[0]; it->pc++; break;
            case 246: it->bgs_fade = c->p[0]; it->pc++; break;
            case 251: it->se_stop = 1; it->pc++; break;
            case 243: it->bgm_saved = 1; it->pc++; break;
            case 244: it->bgm_replayed = 1; it->pc++; break;
            case 133:
                if (c->s) snprintf(it->victory_me, FH_NAME_CAP, "%s", c->s);
                it->pc++;
                break;
            case 139:
                if (c->s) snprintf(it->defeat_me, FH_NAME_CAP, "%s", c->s);
                it->pc++;
                break;
            case 134: it->save_on = (c->p[0] != 0); it->pc++; break;
            case 136: it->encounter_on = (c->p[0] != 0); it->pc++; break;
            case 137: it->formation_on = (c->p[0] != 0); it->pc++; break;
            case 138:
                it->wtone[0] = c->p[0]; it->wtone[1] = c->p[1];
                it->wtone[2] = c->p[2]; it->wtone[3] = c->p[3];
                it->pc++;
                break;
            case 140:
                it->vehbgm.veh = c->p[0];
                if (c->s) snprintf(it->vehbgm.name, FH_NAME_CAP, "%s", c->s);
                it->pc++;
                break;

            case 281: it->namedisp = (c->p[0] == 0); it->pc++; break;
            case 282: it->tileset = c->p[0]; it->pc++; break;
            case 283: {
                if (c->s) {
                    char b[80];
                    strncpy(b, c->s, sizeof(b) - 1);
                    b[sizeof(b) - 1] = '\0';
                    char *a = strtok(b, "\n"), *bb = a ? strtok(NULL, "\n") : NULL;
                    if (a) snprintf(it->battleback1, FH_NAME_CAP, "%s", a);
                    if (bb) snprintf(it->battleback2, FH_NAME_CAP, "%s", bb);
                }
                it->pc++;
                break;
            }
            case 284:
                if (c->s) snprintf(it->parallax.name, FH_NAME_CAP, "%s", c->s);
                it->parallax.loopx = c->p[0]; it->parallax.loopy = c->p[1];
                it->parallax.sx = c->p[2]; it->parallax.sy = c->p[3];
                it->pc++;
                break;
            case 285:
                it->locinfo.var = c->p[0]; it->locinfo.type = c->p[1];
                it->locinfo.x = c->p[2]; it->locinfo.y = c->p[3];
                it->pc++;
                break;
            case 202:
                it->vehloc.veh = c->p[0];
                if (c->p[1] == 0) {
                    it->vehloc.map = c->p[2]; it->vehloc.x = c->p[3]; it->vehloc.y = c->p[4];
                } else {
                    it->vehloc.map = (c->p[2] >= 0 && c->p[2] < FH_MAX_VARS) ? it->var[c->p[2]] : 0;
                    it->vehloc.x = (c->p[3] >= 0 && c->p[3] < FH_MAX_VARS) ? it->var[c->p[3]] : 0;
                    it->vehloc.y = (c->p[4] >= 0 && c->p[4] < FH_MAX_VARS) ? it->var[c->p[4]] : 0;
                }
                it->pc++;
                break;
            case 206: it->vehicle_in = !it->vehicle_in; it->pc++; break;
            case 217: it->gather = 1; it->pc++; break;
            case 261:
                if (c->s) snprintf(it->movie, FH_NAME_CAP, "%s", c->s);
                it->pc++;
                break;
            case 101:


                if (it->page_open == 1) {
                    it->page_open = 2;
                    it->trace_skip = 1;
                    return FH_RUN_PAGE;
                }
                it->text_len = 0;
                it->text[0] = '\0';
                it->page_open = 1;

                if (c->s) {
                    int i = 0;
                    while (c->s[i] && i < FH_NAME_CAP - 1) {
                        it->msg_face[i] = c->s[i];
                        i++;
                    }
                    it->msg_face[i] = '\0';
                } else {
                    it->msg_face[0] = '\0';
                }
                it->msg_face_idx = c->p[0];
                it->msg_bg = c->p[1];
                it->msg_pos = c->p[2];
                it->pc++;
                break;
            case 401:
            case 405:
                emit_text(it, c->s);
                it->pc++;
                break;
            case 102: {
                it->choice_text = c->s;
                it->choice_n = 0;
                if (c->s) {
                    it->choice_n = 1;
                    for (const char *p = c->s; *p; p++)
                        if (*p == '\n') it->choice_n++;
                }


                it->page_open = 2;
                it->await_choice = 1;
                it->pc++;
                return FH_RUN_CHOICE;
            }
            case 402:
                it->pc = (it->choice_sel == c->p[0]) ? it->pc + 1 : c->jump;
                break;
            case 403:
                it->pc = (it->choice_sel < 0) ? it->pc + 1 : c->jump;
                break;
            case 111:


                if (c->op == 11) { it->unknown++; it->pc = c->jump; break; }
                if ((c->op > 1 && c->op != 4 && c->op != 5 && c->op != 8)) it->unknown++; else {
                    int r = eval_111(it, c);
                    if (c->indent < 16) it->branchv[c->indent] = r;
                    it->pc = r ? it->pc + 1 : c->jump;
                    break;
                }
                it->pc = c->jump;
                break;
            case 411:

                it->pc = (c->indent < 16 && it->branchv[c->indent]) ? c->jump : it->pc + 1;
                break;
            case 113:
            case 413:
            case 601:
            case 602:
            case 603: {
                int take = 1;
                if (c->code == 601) take = (it->branch == 0);
                if (c->code == 602) take = (it->branch == 1);
                if (c->code == 603) take = (it->branch == 2);
                it->pc = take ? it->pc + 1 : c->jump;
                break;
            }
            case 121: {


                int a = c->p[0], b = c->p[1], v = c->p[2];
                if (a < 0) a = 0;
                if (b >= FH_MAX_SWITCHES) b = FH_MAX_SWITCHES - 1;
                for (int i = a; i <= b; i++) it->sw[i] = (unsigned char)(v == 0 ? 1 : 0);
                it->pc++;
                break;
            }
            case 122: {
                int a = c->p[0], b = c->p[1];
                if (a < 0) a = 0;
                if (b >= FH_MAX_VARS) b = FH_MAX_VARS - 1;
                if (c->p[2] == 2) {

                    int lo = c->p[3], span = c->p[4] - lo + 1;
                    if (span < 1) span = 1;
                    for (int i = a; i <= b; i++) {
                        int rhs = lo + (int)fh_randn(it, (unsigned)span);
                        switch (c->op) {
                            case 0: it->var[i] = rhs; break;
                            case 1: it->var[i] += rhs; break;
                            case 2: it->var[i] -= rhs; break;
                            case 3: it->var[i] *= rhs; break;
                            case 4: if (rhs != 0) it->var[i] /= rhs; break;
                            case 5: if (rhs != 0) it->var[i] %= rhs; break;
                            default: it->unknown++; break;
                        }
                    }
                    it->pc++;
                    break;
                }
                int rhs = c->p[3];
                if (c->p[2] == 1) {
                    rhs = (rhs >= 0 && rhs < FH_MAX_VARS) ? it->var[rhs] : 0;
                } else if (c->p[2] != 0) {
                    it->unknown++;
                    it->pc++;
                    break;
                }
                for (int i = a; i <= b; i++) {
                    switch (c->op) {
                        case 0: it->var[i] = rhs; break;
                        case 1: it->var[i] += rhs; break;
                        case 2: it->var[i] -= rhs; break;
                        case 3: it->var[i] *= rhs; break;
                        case 4: if (rhs != 0) it->var[i] /= rhs; break;
                        case 5: if (rhs != 0) it->var[i] %= rhs; break;
                        default: it->unknown++; break;
                    }
                }
                it->pc++;
                break;
            }
            case 230:
                it->wait += c->p[0];
                it->pc++;
                break;
            case 355:
            case 655: {
                const char *s = c->s ? c->s : "";
                int actor = 0;
                if (!strcmp(s, "$gamePlayer.refresh();") || !strcmp(s, "$gamePlayer.refresh()")) {
                    it->refresh_n++;
                } else if (sscanf(s, "$gameActors.actor(%d).", &actor) == 1 &&
                           actor >= 0 && actor < FH_MAX_ACTORS) {
                    const char *dot = strchr(s, ')');
                    dot = dot ? dot + 2 : NULL;
                    char nm[FH_NAME_CAP];
                    int ix = 0;
                    if (dot && sscanf(dot, "setCharacterImage('%31[^']', %d)", nm, &ix) == 2) {
                        snprintf(it->chname[actor], FH_NAME_CAP, "%s", nm);
                        it->chidx[actor] = ix;
                    } else if (dot && sscanf(dot, "setBattlerImage('%31[^']')", nm) == 1) {
                        snprintf(it->btname[actor], FH_NAME_CAP, "%s", nm);
                    } else if (dot && sscanf(dot, "setFaceImage('%31[^']', %d)", nm, &ix) == 2) {
                        snprintf(it->fcname[actor], FH_NAME_CAP, "%s", nm);
                        it->fcidx[actor] = ix;
                    } else it->unknown++;
                } else it->unknown++;
                it->pc++;
                break;
            }
            case 356: {
                const char *s = c->s ? c->s : "";
                while (*s == ' ') s++;
                if (it->plug_n < FH_PLUG_Q) {
                    int ni = 0;
                    while (*s && *s != ' ' && ni < 31) it->plug[it->plug_n].name[ni++] = *s++;
                    it->plug[it->plug_n].name[ni] = '\0';
                    while (*s == ' ') s++;
                    int ai = 0;
                    while (*s && ai < 95) it->plug[it->plug_n].args[ai++] = *s++;
                    it->plug[it->plug_n].args[ai] = '\0';
                    it->plug_n++;
                }
                it->pc++;
                break;
            }
            case 117: {
                int id = c->p[0], k;
                for (k = 0; k < it->ce_n; k++)
                    if (it->ce_ids[k] == id) break;
                if (k < it->ce_n && it->depth < FH_CALL_DEPTH) {
                    it->callst[it->depth].list = it->list;
                    it->callst[it->depth].len = it->len;
                    it->callst[it->depth].pc = it->pc + 1;
                    it->depth++;
                    it->list = it->ce_lists[k];
                    it->len = it->ce_lens[k];
                    it->pc = 0;
                } else {
                    it->unknown++;
                    it->pc++;
                }
                break;
            }
            case 129: {
                int actor = c->p[0], i, at = -1;
                for (i = 0; i < it->party_n; i++)
                    if (it->party[i] == actor) { at = i; break; }
                if (c->p[1] == 0) {
                    if (at < 0 && it->party_n < FH_MAX_PARTY)
                        it->party[it->party_n++] = actor;
                } else if (at >= 0) {
                    it->party[at] = it->party[--it->party_n];
                }
                it->pc++;
                break;
            }
            case 205:
                if (it->route_n < FH_ROUTE_Q) {
                    it->routes[it->route_n].ch = c->p[0];
                    it->routes[it->route_n].steps = c->p[1];
                    it->routes[it->route_n].wait = c->p[2];
                    it->route_n++;
                }
                it->pc++;
                break;
            case 223:
                it->tint[0] = c->p[0]; it->tint[1] = c->p[1];
                it->tint[2] = c->p[2]; it->tint[3] = c->p[3];
                it->tint_frames = c->p[4];
                if (c->op) it->wait += c->p[4];
                it->pc++;
                break;
            case 250:
                if (c->s) {
                    int n = (int)strlen(c->s);
                    if (n > 63) n = 63;
                    memcpy(it->last_se, c->s, (size_t)n);
                    it->last_se[n] = '\0';
                    it->se_count++;
                }
                it->pc++;
                break;
            case 126:
            case 127:
            case 128: {
                int id = c->p[0];
                int v = (c->p[2] == 0) ? c->p[3] : 0;
                if (c->p[2] == 1 && c->p[3] >= 0 && c->p[3] < FH_MAX_VARS)
                    v = it->var[c->p[3]];
                else if (c->p[2] != 0 && c->p[2] != 1) { it->unknown++; it->pc++; break; }
                if (c->op == 1) v = -v;
                else if (c->op != 0) { it->unknown++; it->pc++; break; }
                if (c->code == 126 && id >= 0 && id < FH_MAX_ITEMS) it->inv_item[id] += v;
                else if (c->code == 127 && id >= 0 && id < FH_MAX_WEAPONS) it->inv_weap[id] += v;
                else if (c->code == 128 && id >= 0 && id < FH_MAX_ARMORS) it->inv_arm[id] += v;
                it->pc++;
                break;
            }
            case 311:
            case 312: {
                int sel = c->p[0], idv = c->p[1];
                int v = (c->p[3] == 0) ? c->p[4] : 0;
                if (c->p[3] == 1 && c->p[4] >= 0 && c->p[4] < FH_MAX_VARS)
                    v = it->var[c->p[4]];
                else if (c->p[3] != 0 && c->p[3] != 1) { it->unknown++; it->pc++; break; }
                if (c->p[2] == 1) v = -v;
                else if (c->p[2] != 0) { it->unknown++; it->pc++; break; }
                int ids[FH_MAX_PARTY + 1], nids = 0, i;
                if (sel == 0) {
                    int a = idv;
                    if (a == 0) {
                        for (i = 0; i < it->party_n; i++) ids[nids++] = it->party[i];
                    } else ids[nids++] = a;
                } else if (idv >= 0 && idv < FH_MAX_VARS && it->var[idv] > 0) {
                    ids[nids++] = it->var[idv];
                }
                for (i = 0; i < nids; i++) {
                    int a = ids[i];
                    if (a < 0 || a >= FH_MAX_ACTORS) continue;
                    if (c->code == 311) {
                        if (it->hp[a] > 0) {
                            if (!c->p[5] && it->hp[a] <= -v) v = 1 - it->hp[a];
                            it->hp[a] += v;
                        }
                    } else {
                        it->mp[a] += v;
                        if (it->mp[a] < 0) it->mp[a] = 0;
                    }
                }
                it->pc++;
                break;
            }
            case 212:
                if (it->anim_n < FH_ANIM_Q) {
                    it->anims[it->anim_n].ch = c->p[0];
                    it->anims[it->anim_n].anim = c->p[1];
                    it->anims[it->anim_n].mirror = 0;
                    it->anim_n++;
                }
                if (c->p[2]) it->wait += c->p[3] * 4 + 1;
                it->pc++;
                break;
            case 211:
                it->transparent = (c->p[0] == 0);
                it->pc++;
                break;
            case 216:
                it->followers_on = (c->p[0] == 0);
                it->refresh_n++;
                it->pc++;
                break;
            case 322: {
                int a = c->p[0];
                if (a >= 0 && a < FH_MAX_ACTORS && c->s) {

                    char b[160];
                    strncpy(b, c->s, sizeof(b) - 1);
                    b[sizeof(b) - 1] = '\0';
                    char *ln[5];
                    int n = 0;
                    for (char *t = strtok(b, "\n"); t && n < 5; t = strtok(NULL, "\n"))
                        ln[n++] = t;
                    if (n == 5) {
                        snprintf(it->chname[a], FH_NAME_CAP, "%s", ln[0]);
                        it->chidx[a] = atoi(ln[1]);
                        snprintf(it->fcname[a], FH_NAME_CAP, "%s", ln[2]);
                        it->fcidx[a] = atoi(ln[3]);
                        snprintf(it->btname[a], FH_NAME_CAP, "%s", ln[4]);
                    } else it->unknown++;
                } else it->unknown++;
                it->refresh_n++;
                it->pc++;
                break;
            }
            case 201:
                if (c->p[0] == 0) {
                    it->transfer.map = c->p[1]; it->transfer.x = c->p[2]; it->transfer.y = c->p[3];
                } else {
                    int m = (c->p[1] >= 0 && c->p[1] < FH_MAX_VARS) ? it->var[c->p[1]] : 0;
                    int x = (c->p[2] >= 0 && c->p[2] < FH_MAX_VARS) ? it->var[c->p[2]] : 0;
                    int y = (c->p[3] >= 0 && c->p[3] < FH_MAX_VARS) ? it->var[c->p[3]] : 0;
                    it->transfer.map = m; it->transfer.x = x; it->transfer.y = y;
                }
                it->transfer.dir = c->p[4]; it->transfer.fade = c->p[5];
                it->transfer.pending = 1;
                it->pc++;
                break;
            case 213:
                if (it->balloon_n < FH_ANIM_Q) {
                    it->balloons[it->balloon_n].ch = c->p[0];
                    it->balloons[it->balloon_n].icon = c->p[1];
                    it->balloon_n++;
                }
                it->pc++;
                break;
            case 214:
                if (it->on_map && it->ev_id > 0 && it->erased_n < 64)
                    it->erased[it->erased_n++] = it->ev_id;
                it->pc++;
                break;
            case 204:
                if (it->scroll_n < 16) {
                    it->scrolls[it->scroll_n].dir = c->p[0];
                    it->scrolls[it->scroll_n].dist = c->p[1];
                    it->scrolls[it->scroll_n].speed = c->p[2];
                    it->scroll_n++;
                }
                it->pc++;
                break;
            case 203: {
                int ci = c->p[0] + 2, cj = c->p[2] + 2;
                if (c->p[1] == 0) {
                    if (ci >= 0 && ci < FH_MAX_CHARS) {
                        it->chx[ci] = c->p[3]; it->chy[ci] = c->p[4];
                    }
                } else if (c->p[1] == 1) {
                    int x = (c->p[3] >= 0 && c->p[3] < FH_MAX_VARS) ? it->var[c->p[3]] : 0;
                    int y = (c->p[4] >= 0 && c->p[4] < FH_MAX_VARS) ? it->var[c->p[4]] : 0;
                    if (ci >= 0 && ci < FH_MAX_CHARS) { it->chx[ci] = x; it->chy[ci] = y; }
                } else if (c->p[1] == 2) {
                    if (ci >= 0 && ci < FH_MAX_CHARS && cj >= 0 && cj < FH_MAX_CHARS) {
                        int t;
                        t = it->chx[ci]; it->chx[ci] = it->chx[cj]; it->chx[cj] = t;
                        t = it->chy[ci]; it->chy[ci] = it->chy[cj]; it->chy[cj] = t;
                    }
                } else it->unknown++;
                if (c->p[5] > 0 && ci >= 0 && ci < FH_MAX_CHARS) it->chd[ci] = c->p[5];
                it->pc++;
                break;
            }
            case 313: {
                int sel = c->p[0], idv = c->p[1], add = (c->p[2] == 0), st = c->p[3];
                int ids[FH_MAX_PARTY + 1], nids = 0, i;
                if (sel == 0) {
                    int a = idv;
                    if (a == 0) {
                        for (i = 0; i < it->party_n; i++) ids[nids++] = it->party[i];
                    } else ids[nids++] = a;
                } else {
                    if (idv >= 0 && idv < FH_MAX_VARS && it->var[idv] > 0)
                        ids[nids++] = it->var[idv];
                }
                for (i = 0; i < nids; i++)
                    if (ids[i] >= 0 && ids[i] < FH_MAX_ACTORS && st >= 0 && st < FH_MAX_STATES)
                        it->astate[ids[i]][st] = (unsigned char)(add ? 1 : 0);
                it->pc++;
                break;
            }
            default:
                it->unknown++;
                it->pc++;
                break;
        }
    }
    return FH_RUN_END;
}

int fh_interp_run(FhInterp *it, int max_steps) {
    int steps = 0;
    for (;;) {
        int r = fh_interp_step(it);
        if (r == FH_RUN_END) return FH_RUN_END;
        if (r == FH_RUN_CHOICE) {


            it->await_choice = 0;
        }
        if (++steps >= max_steps) return FH_RUN_STEP;
    }
}

void fh_srand(FhInterp *it, unsigned seed) {
    it->rng = seed ? seed : 1u;
}

unsigned fh_randn(FhInterp *it, unsigned n) {

    it->rng = it->rng * 1664525u + 1013904223u;
    if (n == 0) return 0;
    return (unsigned)(((it->rng >> 16) * (uint64_t)n) >> 16) % n;
}
