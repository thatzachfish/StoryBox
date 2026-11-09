/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it;
 * Tune and MIDI out are global. */
project_t proj;

const char *piece_name(int i) { return en_piece_name(i); }

/* the player's, not an act's: Tune, MIDI out and Energy always; Root and the lead within a story (a story brings its own) */
static int story_par(int p) { return p == P_ROOT || (p >= P_LSOUND && p <= P_LOCT); }
static int global_par(int p)
{
    return p == P_TUNE || p == P_MIDI || p == P_ENERGY || (p >= P_RVLEN && p <= P_SCMIX) || story_par(p);
}   /* the player's */

int proj_hold_keep;                              /* a transition is under way: proj.par is between two acts */

static void recipe(int i, slot_t *s)
{
    int16_t par[P_NPARAMS];
    int p;
    en_piece(i, par, &s->pool);
    for (p = 0; p < P_NPARAMS; p++)
        s->par[p] = (int8_t)par[p];
}

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
}

static void keep(void)                           /* the piece playing, into its slot */
{
    int p;
    if (proj_hold_keep)
        return;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = (int8_t)proj.par[p];
    proj.slot[proj.piece].pool = proj.pool;
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
    proj.leds = 1;
    proj.wander = 0;
    proj.fade = 35;
}

static int pool_ok(uint32_t p)                   /* the root and at least two more notes */
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return (p & 1u) && !(p >> 12) && n >= 3;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}
static int slot_ok(const slot_t *s)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (s->par[i] < p->lo || s->par[i] > p->hi)
            return 0;
    }
    return pool_ok(s->pool);
}

static void clamp_pars(void)                    /* (0.5) a value out of range (a sound since removed: the lead's banjo)
                                                  * brought into it, not the whole project refused */
{
    int i, p;
    for (p = 0; p < P_NPARAMS; p++) {
        const en_param_t *e = en_param_info(p);
        if (proj.par[p] < e->lo)
            proj.par[p] = (int16_t)e->lo;
        if (proj.par[p] > e->hi)
            proj.par[p] = (int16_t)e->hi;
        for (i = 0; i < NPIECES; i++) {
            if (proj.slot[i].par[p] < e->lo)
                proj.slot[i].par[p] = (int8_t)e->lo;
            if (proj.slot[i].par[p] > e->hi)
                proj.slot[i].par[p] = (int8_t)e->hi;
        }
    }
}

static int project_valid(void)                  /* an older format: the recipes again */
{
    int i;
    if (proj.magic == PROJ_MAGIC && proj.format == PROJ_FORMAT) {
        clamp_pars();
        proj.bt_off = proj.bt_off != 0;          /* (0.6's settings, in bytes kept spare before) */
        if (proj.answer > 100)
            proj.answer = 100;
    }
    if (proj.magic != PROJ_MAGIC || !pool_ok(proj.pool) || !pars_ok(proj.par))
        return 0;
    if (proj.format != PROJ_FORMAT || proj.piece >= NPIECES || proj.vibe > 1 || proj.wander > 100 || proj.fade > 100)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!slot_ok(&proj.slot[i]))
            return 0;
    return 1;
}

/* 0.3's project (format 7): 49 values a piece, as int16, 32 pieces (eight stories) */
#define P7 49
#define N7 32
typedef struct {
    int16_t par[P7];
    uint16_t pool;
} slot7_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[P7];
    uint16_t pool;
    uint8_t leds, piece, vibe, wander, fade, rsv0[3];
    slot7_t slot[N7];
    uint8_t rsv[16];
} project7_t;

static int from7(const project7_t *o)            /* 0.3's: its pieces kept (Fly's acts, reworked, from their recipes),
                                                  * the new values and Post Show's from theirs */
{
    int i, p;
    project_defaults();
    for (p = 0; p < P7; p++)
        proj.par[p] = o->par[p];
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < N7 ? o->piece : 0;
    proj.wander = o->wander;
    proj.fade = o->fade;
    for (i = 0; i < N7; i++) {
        if (i / EN_NACT == 6)                    /* (Fly: 0.4 reworks it) */
            continue;
        for (p = 0; p < P7; p++)
            proj.slot[i].par[p] = (int8_t)o->slot[i].par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (p = P7; p < P_NPARAMS; p++)             /* the new values: the piece's own */
        proj.par[p] = proj.slot[proj.piece].par[p];
    return project_valid() ? 0 : -1;
}

int project_load(void)
{
    static union {
        project_t p;
        project7_t p7;
    } buf;
    int n;
    memset(&buf, 0, sizeof buf);
    n = plat_store_load(OBJ_PROJ, &buf, sizeof buf);
    if (n >= 8 && buf.p.magic == PROJ_MAGIC && buf.p.format == 7u && n >= (int)sizeof(project7_t) - 16) {
        if (from7(&buf.p7) == 0)
            return 0;
        project_defaults();
        return -1;
    }
    proj = buf.p;
    if (n >= 8 && project_valid())
        return 0;
    project_defaults();
    return -1;
}

int project_save(void)
{
    keep();
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

/* a transition's target: piece i's settings as it was left (the globals as they are now), without taking it up */
void project_target(int i, int16_t *par, uint16_t *pool)
{
    int p;
    i = (i % NPIECES + NPIECES) % NPIECES;
    for (p = 0; p < P_NPARAMS; p++)               /* Root and the lead: a new story brings its own */
        par[p] = global_par(p) && !(story_par(p) && i / EN_NACT != proj.piece / EN_NACT) ? proj.par[p] : proj.slot[i].par[p];
    *pool = proj.slot[i].pool;
}

void project_keep(void) { keep(); }

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    take(&s);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_pool(proj.pool);
    en_prog(proj.piece);
    en_answer(proj.answer);
}
