#include "battle_blob.h"
#include <stdio.h>
#include <string.h>

static uint16_t rd16(const unsigned char *p) {
    return (uint16_t)(p[0] | ((unsigned)p[1] << 8));
}

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t rd32s(const unsigned char *p) {
    return (int32_t)rd32(p);
}

static unsigned char t_raw[TBLOB_RAW_SIZE];
static BtPageCond t_conds[TBLOB_MAX_PAGES];
static FhCmd t_cmds[TBLOB_MAX_CMDS];
static const FhCmd *t_lists[TBLOB_MAX_PAGES];
static int t_lens[TBLOB_MAX_PAGES];
static int t_npages;
static int t_ce_ids[TBLOB_MAX_CES];
static const FhCmd *t_ce_lists[TBLOB_MAX_CES];
static int t_ce_lens[TBLOB_MAX_CES];
static int t_nces;
static TblobActor t_actors[TBLOB_MAX_ACTORS];
static int t_nactors;
static unsigned char t_names[TBLOB_NAME_SIZE];
static char t_path[256];
static unsigned t_idx_id[TBLOB_MAX_INDEX];
static unsigned t_idx_off[TBLOB_MAX_INDEX];
static unsigned t_idx_len[TBLOB_MAX_INDEX];
static unsigned t_idx_name[TBLOB_MAX_INDEX];
static int t_idx_n;

int tblob_open(const char *path) {
    FILE *f = fopen(path, "rb");
    unsigned char head[16];
    unsigned i;
    if (!f)
        return -1;
    strncpy(t_path, path, sizeof(t_path) - 1);
    t_path[sizeof(t_path) - 1] = 0;
    if (fread(head, 1, 16, f) != 16) {
        fclose(f);
        return -1;
    }
    if (rd32(head) != TBLOB_MAGIC || rd32(head + 4) != TBLOB_VERSION) {
        fclose(f);
        return -1;
    }
    t_idx_n = (int)rd32(head + 8);
    if (t_idx_n > TBLOB_MAX_INDEX)
        t_idx_n = TBLOB_MAX_INDEX;
    for (i = 0; i < (unsigned)t_idx_n; i++) {
        unsigned char e[16];
        if (fread(e, 1, 16, f) != 16) {
            fclose(f);
            return -1;
        }
        t_idx_id[i] = rd32(e);
        t_idx_off[i] = rd32(e + 4);
        t_idx_len[i] = rd32(e + 8);
        t_idx_name[i] = rd32(e + 12);
    }
    {
        unsigned name_len = rd32(head + 12);
        if (name_len && name_len < sizeof(t_names)) {
            if (fseek(f, 16L + (long)t_idx_n * 16L, SEEK_SET) != 0) {
                fclose(f);
                return -1;
            }
            if (fread(t_names, 1, name_len, f) != name_len) {
                fclose(f);
                return -1;
            }
            t_names[sizeof(t_names) - 1] = 0;
        } else {
            t_names[0] = 0;
        }
    }
    fclose(f);
    return 0;
}

int tblob_count(void) {
    return t_idx_n;
}

int tblob_id(int idx) {
    if (idx < 0 || idx >= t_idx_n)
        return -1;
    return (int)t_idx_id[idx];
}

const char *tblob_name(int idx) {
    if (idx < 0 || idx >= t_idx_n)
        return "";
    return (const char *)t_names + t_idx_name[idx];
}

static unsigned decode_cmds(const unsigned char *base, unsigned ncmds,
                            FhCmd *out, unsigned max) {
    const unsigned char *p = base;
    unsigned i;
    for (i = 0; i < ncmds && i < max; i++) {
        unsigned soff;
        out[i].code = (uint16_t)rd16(p);
        out[i].indent = p[2];
        out[i].jump = (uint16_t)rd16(p + 3);
        out[i].op = p[5];
        for (int k = 0; k < 10; k++)
            out[i].p[k] = rd32s(p + 6 + (unsigned)k * 4);
        soff = rd32(p + 46);
        out[i].s = soff == 0xFFFFFFFFu ? 0 :
                       (const char *)(uintptr_t)(soff + 1u);
        p += 50;
    }
    return (unsigned)(p - base);
}

int tblob_load(int id) {
    FILE *f;
    int slot = -1;
    const unsigned char *p;
    const unsigned char *pool;
    unsigned cursor = 0;
    int i, li = 0;
    unsigned pool_len;
    for (i = 0; i < t_idx_n; i++) {
        if ((int)t_idx_id[i] == id) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return -1;
    if (t_idx_len[slot] > sizeof(t_raw))
        return -1;
    f = fopen(t_path, "rb");
    if (!f)
        return -1;
    if (fseek(f, (long)t_idx_off[slot], SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    if (fread(t_raw, 1, t_idx_len[slot], f) != t_idx_len[slot]) {
        fclose(f);
        return -1;
    }
    fclose(f);
    p = t_raw;
    t_npages = t_nces = t_nactors = 0;
    {
        unsigned np = rd16(p);
        p += 2;
        if (np > TBLOB_MAX_PAGES)
            return -1;
        for (i = 0; i < (int)np; i++) {
            unsigned nc;
            if ((size_t)(p - t_raw) + 54 > sizeof(t_raw))
                return -1;
            for (int k = 0; k < 13; k++)
                ((int32_t *)&t_conds[i])[k] = rd32s(p + (unsigned)k * 4);
            p += 52;
            nc = rd16(p);
            p += 2;
            if (cursor + nc > TBLOB_MAX_CMDS)
                return -1;
            p += decode_cmds(p, nc, t_cmds + cursor,
                             TBLOB_MAX_CMDS - cursor);
            t_lists[li] = t_cmds + cursor;
            t_lens[li] = (int)nc;
            li++;
            cursor += nc;
        }
        t_npages = (int)np;
    }
    {
        unsigned nc = rd16(p);
        unsigned k;
        p += 2;
        if (nc > TBLOB_MAX_CES)
            return -1;
        for (k = 0; k < nc; k++) {
            unsigned nn;
            if ((size_t)(p - t_raw) + 4 > sizeof(t_raw))
                return -1;
            t_ce_ids[k] = (int)rd16(p);
            nn = rd16(p + 2);
            p += 4;
            if (cursor + nn > TBLOB_MAX_CMDS)
                return -1;
            p += decode_cmds(p, nn, t_cmds + cursor,
                             TBLOB_MAX_CMDS - cursor);
            t_ce_lists[k] = t_cmds + cursor;
            t_ce_lens[k] = (int)nn;
            cursor += nn;
        }
        t_nces = (int)nc;
    }
    t_nactors = (int)rd16(p);
    p += 2;
    if (t_nactors >= TBLOB_MAX_ACTORS)
        return -1;
    for (i = 0; i < t_nactors; i++) {
        if ((size_t)(p - t_raw) + 40 > sizeof(t_raw))
            return -1;
        t_actors[i].id = rd32s(p);
        t_actors[i].cls = rd32s(p + 4);
        for (int e = 0; e < 8; e++)
            t_actors[i].eq[e] = rd32s(p + 8 + (unsigned)e * 4);
        p += 40;
    }
    t_actors[t_nactors].id = 0;
    t_actors[t_nactors].cls = 0;
    for (int e = 0; e < 8; e++)
        t_actors[t_nactors].eq[e] = 0;
    pool_len = rd32(p);
    p += 4;
    pool = p;
    if ((size_t)(p - t_raw) + pool_len > sizeof(t_raw))
        return -1;
    for (i = 0; (unsigned)i < cursor; i++) {
        if (t_cmds[i].s)
            t_cmds[i].s =
                (const char *)(pool + ((uintptr_t)t_cmds[i].s - 1u));
    }
    return 0;
}

int tblob_npages(void) {
    return t_npages;
}

const BtPageCond *tblob_conds(void) {
    return t_conds;
}

const FhCmd **tblob_lists(void) {
    return (const FhCmd **)t_lists;
}

const int *tblob_lens(void) {
    return t_lens;
}

int tblob_nces(void) {
    return t_nces;
}

int tblob_ce_id(int k) {
    if (k < 0 || k >= t_nces)
        return -1;
    return t_ce_ids[k];
}

const FhCmd *tblob_ce_list(int k) {
    if (k < 0 || k >= t_nces)
        return 0;
    return t_ce_lists[k];
}

int tblob_ce_len(int k) {
    if (k < 0 || k >= t_nces)
        return 0;
    return t_ce_lens[k];
}

int tblob_nactors(void) {
    return t_nactors;
}

const TblobActor *tblob_actors(void) {
    return t_actors;
}

static char sc_path[256];
static unsigned short sc_idx_id[512];
static unsigned sc_idx_off[512];
static int sc_count;
static FhCmd sk_cmds[11000];
static const FhCmd *sk_lists[256];
static int sk_lens[256];
static int sk_ids[256];
static int sk_count;
static unsigned char sk_pool[512 * 1024];

#define SK_CMDS_MAX 11000

int skillce_open(const char *path) {
    FILE *f = fopen(path, "rb");
    unsigned char head[8];
    unsigned n, pos = 8;
    if (!f)
        return -1;
    if (fread(head, 1, 8, f) != 8) {
        fclose(f);
        return -1;
    }
    if (rd32(head) != 0x45434B53u || rd16(head + 4) != 1) {
        fclose(f);
        return -1;
    }
    n = (int)rd16(head + 6);
    if (n > 512)
        n = 512;
    sc_count = 0;
    for (unsigned i = 0; i < n && sc_count < 512; i++) {
        unsigned char ceh[4];
        unsigned nc, pool_len;
        long total;
        if (fseek(f, pos, SEEK_SET) != 0)
            break;
        if (fread(ceh, 1, 4, f) != 4)
            break;
        sc_idx_id[sc_count] = (unsigned short)rd16(ceh);
        nc = rd16(ceh + 2);
        if (fseek(f, pos + 4 + (long)nc * 50L, SEEK_SET) != 0)
            break;
        if (fread(ceh, 1, 4, f) != 4)
            break;
        pool_len = rd32(ceh);
        total = 8 + (long)nc * 50L + (long)pool_len;
        sc_idx_off[sc_count] = pos;
        pos += (unsigned)total;
        sc_count++;
    }
    fclose(f);
    strncpy(sc_path, path, sizeof(sc_path) - 1);
    sc_path[sizeof(sc_path) - 1] = 0;
    return 0;
}

int skillce_count(void) {
    return sc_count;
}

int skill_union_build(const int *seed_ids, int nseed) {
    FILE *f;
    unsigned cmd_cursor = 0, pool_cursor = 0;
    sk_count = 0;
    f = fopen(sc_path, "rb");
    if (!f)
        return -1;
    for (int s = 0; s < nseed; s++) {
        int cid = seed_ids[s], slot = -1;
        unsigned char ceh[4], *ce_chunk;
        static unsigned char chunk[256 * 1024];
        unsigned nc, pool_len;
        int dup = 0;
        for (int i = 0; i < sk_count; i++) {
            if (sk_ids[i] == cid) {
                dup = 1;
                break;
            }
        }
        if (dup)
            continue;
        for (int i = 0; i < sc_count; i++) {
            if (sc_idx_id[i] == (unsigned)cid) {
                slot = i;
                break;
            }
        }
        if (slot < 0)
            continue;
        if (fseek(f, sc_idx_off[slot], SEEK_SET) != 0) {
            fclose(f);
            return -1;
        }
        if (fread(ceh, 1, 4, f) != 4) {
            fclose(f);
            return -1;
        }
        nc = rd16(ceh + 2);
        if (4 + nc * 50u > sizeof(chunk)) {
            fclose(f);
            continue;
        }
        ce_chunk = chunk;
        if (fread(ce_chunk, 1, nc * 50u, f) != nc * 50u) {
            fclose(f);
            return -1;
        }
        if (fread(ceh, 1, 4, f) != 4) {
            fclose(f);
            return -1;
        }
        pool_len = rd32(ceh);
        if (pool_cursor + pool_len > sizeof(sk_pool)) {
            fclose(f);
            continue;
        }
        if (pool_len &&
            fread(sk_pool + pool_cursor, 1, pool_len, f) != pool_len) {
            fclose(f);
            return -1;
        }
        if (cmd_cursor + nc > SK_CMDS_MAX) {
            fclose(f);
            continue;
        }
        decode_cmds(ce_chunk, nc, sk_cmds + cmd_cursor,
                    SK_CMDS_MAX - cmd_cursor);
        for (unsigned i = 0; i < nc; i++) {
            if (sk_cmds[cmd_cursor + i].s)
                sk_cmds[cmd_cursor + i].s =
                    (const char *)(sk_pool + pool_cursor +
                                   ((uintptr_t)sk_cmds[cmd_cursor + i].s -
                                    1u));
        }
        sk_ids[sk_count] = cid;
        sk_lists[sk_count] = sk_cmds + cmd_cursor;
        sk_lens[sk_count] = (int)nc;
        sk_count++;
        cmd_cursor += nc;
        pool_cursor += pool_len;
    }
    fclose(f);
    return sk_count;
}

const FhCmd *skill_union_list(int k) {
    if (k < 0 || k >= sk_count)
        return 0;
    return sk_lists[k];
}

int skill_union_len(int k) {
    if (k < 0 || k >= sk_count)
        return 0;
    return sk_lens[k];
}

int skill_union_count(void) {
    return sk_count;
}

int skill_union_id(int k) {
    if (k < 0 || k >= sk_count)
        return -1;
    return sk_ids[k];
}
