#ifndef FH_BATTLE_BLOB_H
#define FH_BATTLE_BLOB_H

#include <stdint.h>
#include "battle.h"
#include "interp.h"

#define TBLOB_MAGIC 0x52544846u
#define TBLOB_VERSION 1u
#define TBLOB_MAX_PAGES 24
#define TBLOB_MAX_CES 12
#define TBLOB_MAX_ACTORS 48
#define TBLOB_MAX_CMDS 19000
#define TBLOB_RAW_SIZE (1024 * 1024)
#define TBLOB_CE_SIZE (512 * 1024)
#define TBLOB_NAME_SIZE 4096

#define TBLOB_MAX_INDEX 256

typedef struct {
    int id, cls, eq[8];
} TblobActor;

int tblob_open(const char *path);
int tblob_count(void);
int tblob_id(int idx);
const char *tblob_name(int idx);
int tblob_load(int id);
int tblob_npages(void);
const BtPageCond *tblob_conds(void);
const FhCmd **tblob_lists(void);
const int *tblob_lens(void);
int tblob_nces(void);
int tblob_ce_id(int k);
const FhCmd *tblob_ce_list(int k);
int tblob_ce_len(int k);
int tblob_nactors(void);
const TblobActor *tblob_actors(void);

int skillce_open(const char *path);
int skillce_count(void);
int skill_union_build(const int *seed_ids, int nseed);
int skill_union_count(void);
int skill_union_id(int k);
const FhCmd *skill_union_list(int k);
int skill_union_len(int k);

#endif
