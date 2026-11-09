/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays)
 *   PLAY           the garden grows / rests        ARP   the next chord, now
 *   REC            FREEZE: the chord holds, the reverb rings on    OCT- / OCT+   the bells' octave
 *   ENV            the pad's colour: Rhodes / Voices (lit: Voices), kept with the piece
 *   SELECT tempo   ALGORITHM mode (loads a pool)   PRESETS the piece (Root is on the Setup page)
 *   HOME SEQ FX SEL GLO   the pages: LAYERS, MOTION, SPACE, TONE, SETUP (EDIT steps through them)
 *   KNOB 1-4       the page's four values
 *   SAVE           saves (it also saves by itself a few seconds after a change, while resting);
 *                  held 1.5 s: the piece back to its recipe */
#include "fastmath.h"

/* the pages, one a layer (a button each); SELECT steps through them in this order */
/* StoryBox's pages, in the order SELECT steps through them; then the punch-ins' (while one is held, or a latched one
 * tapped). HOME: Home; SEL: Story (the keys' map); EDIT: Lead; FX: Effects; GLO: Setup */
enum { V_MIX, V_GARDEN, V_STORY, V_LEAD, V_SPACE, V_PAD, V_BELLS, V_BASS, V_PULSE, V_SETUP, V_SYSTEM,
       V_REV, V_STUT, V_FILT, V_SCAT, NVIEWS };
#define NPAGES V_REV
#define NOBTN 0xFF
static const char *const VIEW_NAME[NVIEWS] = {"Home", "Feel", "Story", "Lead", "Effects", "Pad", "Melody", "Bass", "Drums",
                                              "Setup", "System", "Reverse", "Stutter", "Filter", "Scatter"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, NOBTN, B_SEL, B_EDIT, B_FX, NOBTN, NOBTN, NOBTN, NOBTN, B_GLO, NOBTN,
                                         B_ENV, B_LFO, B_ARP, B_SEQ};
/* the punch-ins: ENV Reverse, LFO Stutter, ARP Filter, SEQ Phaser (held; a double tap latches one on) */
static const uint8_t FX_BTN[EN_NFX] = {B_ENV, B_LFO, B_ARP, B_SEQ};
/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum { K_LEDS = 100, K_CPU, K_NONE, K_PLAYMODE, K_WANDER, K_FADE, K_ACT, K_TIMER };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_PAD, P_BELLS, P_BASS, P_PULSE},
    {P_EVOLVE, P_FREEDOM, P_DRIFT, P_MOTION},
    {K_ACT, P_GROOVE, P_TENSION, K_WANDER},
    {P_LSOUND, P_LLEVEL, P_LTONE, P_LGLIDE},
    {P_REVERB, P_ECHO, P_ETIME, P_SURF},
    {P_PAD, P_SWELL, P_BRIGHT, P_PADTONE},
    {P_BELLS, P_DENSITY, P_MSOUND, P_BOCT},
    {P_BASS, P_BASSOCT, P_GLIDE, P_FIFTH},
    {P_PULSE, P_HITS, P_SWING, P_KIT},
    {P_ROOT, P_TEMPO, K_TIMER, K_FADE},
    {P_TUNE, K_LEDS, P_MIDI, K_CPU},
    {P_RVLEN, P_RVMIX, K_NONE, K_NONE},
    {P_STRATE, P_STMIX, K_NONE, K_NONE},
    {P_FLCUT, P_FLRES, K_NONE, K_NONE},
    {P_SCSIZE, P_SCMIX, K_NONE, K_NONE},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Pad", "Melody", "Bass", "Drums"},
    {"Evolve", "Freedom", "Drift", "Motion"},
    {"Act", "Structure", "Tension", "Wander"},
    {"Sound", "Level", "Tone", "Glide"},
    {"Reverb", "Echo", "Echo time", "Surf"},
    {"Level", "Swell", "Bright", "Colour"},
    {"Level", "Density", "Sound", "Octave"},
    {"Level", "Octave", "Glide", "Fifth"},
    {"Level", "Hits", "Swing", "Kit"},
    {"Root", "Tempo", "Sleep", "Fade"},
    {"Tune", "Lights", "MIDI out", "CPU"},
    {"Length", "Mix", "", ""},
    {"Rate", "Mix", "", ""},
    {"Cutoff", "Resonance", "", ""},
    {"Rate", "Depth", "", ""},
};

/* the screen's colours: the cover of Ambient 1: Music for Airports (1978), as the web app and the site.
 * The map's paper, its ink; the land's greens for the pad (the chord) and the scale, its rivers for the
 * bells; the bass and the pulse in darker inks. Each layer has a pale tint for the shapes at rest. */
/* StoryBox: a night palette, high contrast but not bright. Near-black ground, warm white text; yellow for what is
 * selected or turning, green for what plays, fuchsia for alerts (a warning, a low battery, a lot of Tension) */
#define K_BG RGB(10, 11, 16)
#define K_PANEL RGB(37, 40, 52)
#define K_LINE RGB(71, 78, 97)
#define K_DIM RGB(177, 179, 193)
#define K_TEXT RGB(250, 246, 234)
#define K_ACC RGB(255, 224, 94)          /* yellow: the page, the knob being turned, a message's word, where it is */
#define K_ALERT RGB(253, 92, 193)        /* fuchsia: alerts */
#define K_BELL RGB(132, 220, 231)
#define K_BELL_T RGB(32, 63, 69)
#define K_PAD RGB(150, 228, 121)         /* green: what plays */
#define K_PAD_T RGB(41, 69, 39)
#define K_PULSE RGB(220, 186, 249)
#define K_PULSE_T RGB(58, 48, 71)
#define K_BASS RGB(249, 168, 99)
#define K_BASS_T RGB(71, 50, 32)
#define K_FROST RGB(164, 197, 255)
#define K_FROST_T RGB(43, 54, 89)

#define KEY_HOLD 450u                          /* ACTS: a key held this long fades slowly instead */
static struct {
    uint8_t acts;                      /* ACTS (SEL): the keys pick acts and stories, not notes */
    uint8_t lead;                      /* LEAD (EDIT): the keys play the lead */
    uint8_t edit_done;                 /* EDIT held: the next chord went this hold */
    uint8_t mutes, solos;              /* the layers muted, soloed (bit 0 pad .. 3 drums): the player's, not saved */
    uint8_t bd_asked;                  /* the breakdown asked for (or on) */
    uint8_t timer;                     /* the sleep timer: 0 off, 1..4 a quarter hour each */
    uint8_t dark;                      /* the screen dark: the timer on and nothing touched for 30 s */
    uint32_t input_t;                  /* the last touch */
    uint32_t timer_end;
    uint8_t fx_held, fx_latch;         /* the punch-ins held, latched (bit FX_*) */
    uint8_t fx_back;                   /* the page to go back to when the last punch-in is let go (+1; 0 none) */
    uint32_t fx_up_t[EN_NFX];          /* when each was let go (a double tap latches) */
    uint32_t key_t[NKEYS], key_held;
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    uint8_t msg_alert;                 /* the message is a warning: its word in fuchsia */
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[4];                   /* what each band shows: redrawn when it changes */
} ui;

/* the screen reader (announce.c; speak() below): a message went to it this frame, so speak() leaves what it
 * changed be; and the transport as last said (PLAY says its own: the engine takes it a block later) */
static uint8_t sp_said;
static struct {
    uint8_t playing;
    uint32_t play_t;                   /* when PLAY (or the start) set it: the engine catches up within ms */
} sp_tp;
static void sp_piece(void);
static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader gets it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
/* a message for a moment in the header; flags: how a screen reader gets it (0: the caller tells it) */
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.msg_alert = (uint8_t)((flags & ANN_KEEP) != 0);
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)proj.par[P_ROOT];
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)             /* (StoryBox says major or minor, not the mode's name) */
{
    uint32_t ap = pool_abs();
    int r = proj.par[P_ROOT];
    return (ap >> ((r + 3) % 12) & 1u) && !(ap >> ((r + 4) % 12) & 1u) ? "minor" : "major";
}

/* -------------------------------------------------------- transitions --- */
/* StoryBox: moving between acts (and stories). Every setting glides from where it is to the new act's;
 * the switches among them (the mode, the pad's colour, the echo time, the octaves) change halfway.
 *   queued (a tap): starts on the next phrase (PHRASE sixteenths), gliding over one phrase
 *   slow (a hold): starts now, over Fade (5 s .. 2 min)
 * A knob turned meanwhile stays where it is put. Vibe moves on to another act of the story by itself every
 * few phrases (Wander), slowly. */
#define PHRASE_BARS 2u                         /* a phrase: two bars (of 4/4 or 3/4) */
static struct {
    uint8_t on, mid;                           /* gliding; the switches done */
    int8_t pending;                            /* a piece waiting for the next phrase, or -1 */
    uint8_t piece;                             /* the piece gliding to */
    int16_t from[P_NPARAMS], to[P_NPARAMS];
    uint16_t to_pool;
    float t, rate;                             /* 0..1; per ms */
    uint32_t last_ms, last_phrase, phrases;    /* phrases since the act last changed (Vibe) */
    uint32_t rng;
} tr;

static int global_p(int p)                     /* the player's: Root and the lead only within a story (a new story brings its own) */
{
    return p == P_TUNE || p == P_MIDI || p == P_ENERGY || (p >= P_RVLEN && p <= P_SCMIX) ||
           ((p == P_ROOT || (p >= P_LSOUND && p <= P_LOCT)) && tr.piece / EN_NACT == proj.piece / EN_NACT);
}
static int switch_p(int p)
{
    return p == P_MODE || p == P_PADTONE || p == P_ETIME || p == P_BOCT || p == P_BASSOCT || p == P_ROOT || p == P_METER ||
           p == P_LSOUND || p == P_LOCT;
}
static uint32_t fade_ms(void)                  /* Fade 0..100: 5 s .. 2 min, as a curve */
{
    float f = (float)proj.fade / 100.0f;
    return 5000u + (uint32_t)(115000.0f * f * f);
}
static uint32_t wander_phrases(void) { return 2u + (uint32_t)(100 - proj.wander) * 14u / 99u; }   /* 16 .. 2 */
static uint32_t phrase_ms(void) { return PHRASE_BARS * en_bar_len * 60000u / ((uint32_t)proj.par[P_TEMPO] * 4u); }
static int tr_goal(void) { return tr.pending >= 0 ? tr.pending : tr.on ? tr.piece : proj.piece; }   /* where it's going */
static int tr_shown_act(void)                  /* the act the screen shows: where it is going, else where it is */
{
    return (tr.pending >= 0 ? tr.pending : tr.on ? tr.piece : proj.piece) % EN_NACT;
}

static void tr_start(int piece, uint32_t ms)
{
    int p;
    project_keep();                            /* the act we leave, as it is (unless a glide is already under way) */
    proj_hold_keep = 1;
    for (p = 0; p < P_NPARAMS; p++)
        tr.from[p] = proj.par[p];
    project_target(piece, tr.to, &tr.to_pool);
    tr.piece = (uint8_t)piece;
    tr.t = 0.0f;
    tr.rate = 1.0f / (float)(ms ? ms : 1u);
    tr.on = 1;
    tr.mid = 0;
    tr.last_ms = plat_ms();
    tr.phrases = 0;
}

/* to piece i: on the next phrase (slow 0), or now over Fade (slow 1) */
static void tr_queue(int piece, int slow)
{
    piece = (piece % NPIECES + NPIECES) % NPIECES;
    if (slow || !en_playing) {
        tr.pending = -1;
        tr_start(piece, slow ? fade_ms() : 600u);
        say_as(slow ? "FADING TO" : "ACT", en_act_name(piece), 0);
    } else {
        tr.pending = (int8_t)piece;
        say_as("NEXT", en_act_name(piece), 0);
    }
    mark_dirty();
}

static void tr_hands_on(int p, int v)
{
    if (tr.on) {
        tr.from[p] = tr.to[p] = (int16_t)v;
    }
}

static void timer_tick(void)                   /* the sleep timer: the last three minutes fade out, then it rests */
{
    int32_t left;
    float f;
    if (!ui.timer) {                           /* faded out: silent until it plays again */
        if (en_fade < 1.0f && en_playing && plat_ms() - ui.timer_end > 2000u)
            en_fade = 1.0f;
        return;
    }
    left = (int32_t)(ui.timer_end - plat_ms());
    if (left <= 0) {
        ui.timer = 0;
        en_fade = 0.0f;                        /* (the tails too) */
        if (en_playing)
            en_play(0);
        say("SLEEP", "GOOD NIGHT");
        return;
    }
    f = left < 180000 ? (float)left / 180000.0f : 1.0f;
    en_fade = f * f;                           /* (heard as an even fade) */
}

static void tr_tick(void)
{
    timer_tick();
    uint32_t now = plat_ms(), phrase = en_barno / PHRASE_BARS;
    int p;
    if (phrase != tr.last_phrase) {             /* a new phrase: a queued act starts, Vibe counts */
        tr.last_phrase = phrase;
        if (tr.pending >= 0) {
            int i = tr.pending;
            tr.pending = -1;
            tr_start(i, phrase_ms());
        } else if (proj.wander && !tr.on && en_playing && ++tr.phrases >= wander_phrases()) {
            int a = proj.piece % EN_NACT, b;
            tr.rng = tr.rng * 1664525u + 1013904223u + now;
            b = (a + 1 + (int)(tr.rng >> 16) % (EN_NACT - 1)) % EN_NACT;
            tr_start(proj.piece / EN_NACT * EN_NACT + b, fade_ms());
            say_as("DRIFTING TO", en_act_name(proj.piece / EN_NACT * EN_NACT + b), 0);
        }
    }
    if (!tr.on)
        return;
    tr.t += (float)(now - tr.last_ms) * tr.rate;
    tr.last_ms = now;
    if (tr.t > 1.0f)
        tr.t = 1.0f;
    {
        float e = tr.t * tr.t * (3.0f - 2.0f * tr.t);   /* eased */
        for (p = 0; p < P_NPARAMS; p++) {
            int v;
            if (global_p(p))
                continue;
            if (switch_p(p))
                v = tr.t >= 0.5f ? tr.to[p] : tr.from[p];
            else
                v = tr.from[p] + (int)((float)(tr.to[p] - tr.from[p]) * e + (tr.to[p] >= tr.from[p] ? 0.5f : -0.5f));
            if (v != proj.par[p]) {
                proj.par[p] = (int16_t)v;
                en_set(p, v);
            }
        }
    }
    if (!tr.mid && tr.t >= 0.5f) {             /* halfway: the new act's scale, and its progression from the top */
        tr.mid = 1;
        proj.pool = tr.to_pool;
        en_pool(proj.pool);
        en_prog(tr.piece);
    }
    if (tr.t >= 1.0f) {
        int story = proj.piece / EN_NACT;
        tr.on = 0;
        proj.piece = tr.piece;
        proj_hold_keep = 0;
        if (proj.piece / EN_NACT != story)
            say_as("STORY", en_story_name(proj.piece / EN_NACT), 0);
        mark_dirty();
    }
}

/* ACTS, the keys in banks with a gap between them. White: F3 G3 A3 B3 the story's four acts; (C4 a gap); D4 .. D5 the
 * eight stories; (E5 a gap); F5 G5 the two stings. Black: F#3 G#3 A#3 C#4 the pad, bells, bass and drums; F#5 the
 * breakdown. KEY_ACT: an act 0..3; a story 10 + s; else as below; -1 nothing */
static const int8_t KEY_ACT[NKEYS] = {0, 50, 1, 51, 2, 52, 3, -1, 53, 10, -1, 11, 12, -1, 13, -1, 14, -1, 15, 16, -1, 17, -1, 18, 30, 40, 31};
static const char *const TRACK_NAME[4] = {"PAD", "MELODY", "BASS", "DRUMS"};
static void mutes_apply(void)                  /* what is heard: the solos if any, else all but the mutes */
{
    en_mute(ui.solos ? (uint32_t)(~ui.solos & 15u) : ui.mutes);
}
#define KEY_STING 30                          /* F5 G5: the story's two stings */
#define KEY_BREAK 40                          /* F#5: the breakdown */
#define KEY_TRACK 50                          /* F#3 G#3 A#3 C#4: the pad, the bells, the bass, the drums (tap mute, hold solo) */
static int key_target(int k)
{
    int v = (unsigned)k < NKEYS ? KEY_ACT[k] : -1;
    if (v < 0 || v >= KEY_STING)
        return -1;
    if (v < EN_NACT)                            /* an act of the story playing (or about to) */
        return (tr.pending >= 0 ? tr.pending : tr.on ? tr.piece : proj.piece) / EN_NACT * EN_NACT + v;
    v -= 10;                                    /* a story, from its first act */
    return v < EN_NSTORY ? v * EN_NACT : -1;
}

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    if (k == K_PLAYMODE)
        return proj.vibe;
    if (k == K_WANDER)
        return proj.wander;
    if (k == K_FADE)
        return proj.fade;
    if (k == K_ACT)
        return tr_shown_act();
    if (k == K_TIMER)
        return ui.timer;
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else {
        *lo = 0;
        *hi = k == K_LEDS ? 2 : k == K_CPU || k == K_WANDER || k == K_FADE ? 100 : k == K_PLAYMODE ? 1 :
              k == K_ACT ? EN_NACT - 1 : k == K_TIMER ? 4 : 0;
    }
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS)
        return en_param_info(k)->name;
    return k == K_LEDS ? "Lights" : k == K_CPU ? "CPU" : k == K_PLAYMODE ? "Mode" : k == K_WANDER ? "Wander" :
           k == K_FADE ? "Fade" : k == K_ACT ? "Act" : k == K_TIMER ? "Sleep" : "";
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else if (k == K_PLAYMODE || k == K_ACT) {
        const char *n = k == K_ACT ? en_act_name(tr_goal() / EN_NACT * EN_NACT + v) : v ? "Vibe" : "Perform";
        int j = 0;
        while (n[j] && j < 9) {
            b[j] = n[j];
            j++;
        }
        b[j] = 0;
    } else if (k == K_TIMER) {                    /* the sleep timer: off, or the minutes left (rounded up) */
        if (!v) {
            const char *n = "Off";
            while ((*b++ = *n++))
                ;
        } else {
            uint32_t left = ui.timer_end - plat_ms();
            itoa_u((left + 59999u) / 60000u, b);
            while (*b)
                b++;
            *b++ = 'm';
            *b = 0;
        }
    } else if (k == K_WANDER && !v) {             /* 0: Perform, the acts wait for you */
        const char *n = "Perform";
        while ((*b++ = *n++))
            ;
    } else if (k == K_WANDER) {                   /* Vibe: moves on every N phrases */
        itoa_u((uint32_t)wander_phrases(), b);
        while (*b)
            b++;
        *b++ = ' ';
        *b++ = 'p';
        *b++ = 'h';
        *b = 0;
    } else if (k == K_FADE) {
        uint32_t sec = fade_ms() / 1000u;
        if (sec < 60u) {
            itoa_u(sec, b);
            while (*b)
                b++;
            *b++ = 's';
            *b = 0;
        } else {
            itoa_u(sec / 60u, b);
            while (*b)
                b++;
            *b++ = ':';
            *b++ = (char)('0' + sec % 60u / 10u);
            *b++ = (char)('0' + sec % 10u);
            *b = 0;
        }
    } else {
        b[0] = 0;
    }
}

static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        en_set(k, v);
        tr_hands_on(k, v);                       /* a transition leaves a knob you turn where you put it */
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    } else if (k == K_PLAYMODE) {
        proj.vibe = (uint8_t)v;
        tr.phrases = 0;
        say("MODE", v ? "VIBE" : "PERFORM");
    } else if (k == K_WANDER) {
        if (!proj.wander != !v)
            say("MODE", v ? "VIBE" : "PERFORM");
        proj.wander = (uint8_t)v;
        tr.phrases = 0;
    } else if (k == K_FADE) {
        proj.fade = (uint8_t)v;
    } else if (k == K_ACT) {
        tr_queue(tr_goal() / EN_NACT * EN_NACT + v, 0);
        return;
    } else if (k == K_TIMER) {                   /* 15, 30, 45 or 60 minutes from now, the last three fading out */
        ui.timer = (uint8_t)v;
        ui.timer_end = plat_ms() + (uint32_t)v * 15u * 60000u;
        en_fade = 1.0f;
        say("SLEEP", v ? "TIMER ON" : "TIMER OFF");
        return;
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - proj.par[P_ROOT] + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        warn("THE ROOT STAYS", "(SETUP)");
        return;
    }
    if (pool_count(p) < 3) {
        warn("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    say_as("PIECE", piece_name(proj.piece), 0);
    sp_piece();                                  /* a screen reader: the piece with its key and tempo */
    mark_dirty();
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void lead_set(int on)                    /* LEAD on (ACTS off) or off (its notes let go) */
{
    int i;
    if (!on)
        for (i = 0; i < NKEYS; i++)
            if (ui.keys >> i & 1u)
                en_lead_off(53 + i);
    ui.lead = (uint8_t)on;
    ui.acts = (uint8_t)!on;                      /* the keys: the lead, or the acts */
    if (on)
        set_view(V_LEAD);
    else if (ui.view == V_LEAD)
        set_view(V_MIX);
    say(on ? "LEAD" : "ACTS", on ? EN_LEAD_NAME[proj.par[P_LSOUND]] : "ON THE KEYS");
}

static void button(int b)
{
    int v;
    if (b == B_HOME) {                           /* HOME: home */
        set_view(V_MIX);
        say("HOME", 0);
        return;
    }
    if (b == B_EDIT) {                           /* EDIT: the keys play the lead, again: back to the acts (held: the
                                                  * next chord) */
        lead_set(!ui.lead);
        return;
    }
    if (b == B_SEL) {                            /* SEL: the keys are the acts, and the Story page their map */
        if (ui.lead)
            lead_set(0);
        ui.acts = 1;
        set_view(V_STORY);
        say("ACTS", "ON THE KEYS");
        return;
    }
    for (v = 0; v < NVIEWS; v++)                 /* a page's button: its page */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_PLAY:
        sp_tp.playing = !en_playing;
        sp_tp.play_t = plat_ms();
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("GROWING", 0);
        }
        break;
    case B_REC:
        say("FREEZE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP:
        if (ui.lead) {                           /* Lead: its octave */
            char t[8];
            knob_set(P_LOCT, proj.par[P_LOCT] + (b == B_OCTUP ? 1 : -1));
            en_param_text(P_LOCT, proj.par[P_LOCT], t);
            say("LEAD OCTAVE", t);
            break;
        }
        knob_set(P_BOCT, proj.par[P_BOCT] + (b == B_OCTUP ? 1 : -1));
        {
            char t[8];
            en_param_text(P_BOCT, proj.par[P_BOCT], t);
            say("BELL OCTAVE", t);
        }
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            warn("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes add or remove their note from the scale; clock and
 * start / stop drive the grid */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt, 0)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if (ui.lead && st == 0x90u && v)     /* LEAD: MIDI in plays it */
            en_lead_on((int)n, (int)v);
        else if (ui.lead && (st == 0x80u || st == 0x90u))
            en_lead_off((int)n);
        else if (st == 0x90u && v)               /* (no scale editing: notes in play the lead) */
            en_lead_on((int)n, (int)v);
        else if (st == 0x80u || st == 0x90u)
            en_lead_off((int)n);
    }
}

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. PLAY acts on
 * its press, and SEL has no job here: neither has a card. */
#define HELP_HOLD 900u
#define RESET_HOLD 1500u
static void help_tick(uint32_t btn, uint32_t keys)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.help) {
        uint32_t m = 1u << (ui.help - 1);
        if (btn != m || keys || (int32_t)(ui.turn_t - ui.down_t[ui.help - 1]) > 0)
            ui.help = 0;
        return;
    }
    if (!btn || (btn & (btn - 1u)) || keys)
        return;
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_PLAY && !(ui.btn_used & btn) && now - ui.down_t[i] >= (i == B_HOME ? 400u : HELP_HOLD) &&
            (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;                  /* its release now does nothing */
        }
}

static int32_t enc_in(int n)                    /* an encoder's turn (any: input, for the dark screen) */
{
    int32_t e = plat_enc(n);
    if (e)
        ui.input_t = plat_ms();
    return e;
}

static void wake(void)                           /* the screen back on: every band drawn again */
{
    int k;
    ui.dark = 0;
    for (k = 0; k < 4; k++)
        ui.sig[k] = 0;
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    if (btn != ui.btn || keys != ui.keys)
        ui.input_t = plat_ms();
    if (ui.dark) {                               /* dark (the sleep timer's): the first touch only wakes it */
        int n;
        for (n = 0; n < NE; n++)
            if (plat_enc(n))
                ui.input_t = plat_ms();
        if (plat_ms() - ui.input_t < 100u) {
            wake();
            ui.btn_used |= btn;                  /* (what is down now does nothing when let go) */
            ui.key_held |= keys;
        }
        ui.btn = btn;
        ui.keys = keys;
        return;
    }
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < EN_NFX; i++) {             /* the punch-ins: on while held; a quick second press latches */
        uint32_t m = 1u << FX_BTN[i];
        if (!(ch & m))
            continue;
        ui.btn_used |= m;
        ui.help = 0;
        if (btn & m) {
            if ((ui.fx_latch >> i & 1u) && plat_ms() - ui.fx_up_t[i] < 300u) {   /* a latched one, double-tapped: off */
                ui.fx_latch &= (uint8_t)~(1u << i);
                ui.fx_up_t[i] = 0;
                say(VIEW_NAME[V_REV + i], "OFF");
            } else if (ui.fx_latch >> i & 1u) {  /* tapped: its page, to turn its knobs */
                set_view(V_REV + (int)i);
                say(VIEW_NAME[V_REV + i], "LATCHED");
            } else {
                ui.fx_held |= (uint8_t)(1u << i);
                if (!ui.fx_back)
                    ui.fx_back = (uint8_t)(ui.view + 1);
                set_view(V_REV + (int)i);
                if (plat_ms() - ui.fx_up_t[i] < 300u) {
                    ui.fx_latch |= (uint8_t)(1u << i);
                    say(VIEW_NAME[V_REV + i], "LATCHED");
                } else {
                    say(VIEW_NAME[V_REV + i], "ON");
                }
            }
        } else {
            ui.fx_held &= (uint8_t)~(1u << i);
            ui.fx_up_t[i] = plat_ms();
            if (!ui.fx_held && ui.fx_back) {     /* the last let go: back to the page */
                if (ui.view >= V_REV)
                    set_view(ui.fx_back - 1);
                ui.fx_back = 0;
            }
        }
        en_fx((uint32_t)(ui.fx_held | ui.fx_latch));
    }
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m) || i == B_ENV || i == B_LFO || i == B_ARP || i == B_SEQ)   /* (the punch-ins: above) */
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS && ui.lead; i++)      /* LEAD: the keys play (F3 is MIDI 53) */
        if (ch >> i & 1u) {
            if (keys >> i & 1u)
                en_lead_on(53 + (int)i, 100);
            else
                en_lead_off(53 + (int)i);
        }
    for (i = 0; i < NKEYS && !ui.lead; i++) {
        int t = ui.acts ? key_target((int)i) : -2;
        if (t == -2) {                           /* (neither mode: nothing; StoryBox has no scale editing) */
        } else if (KEY_ACT[i] >= KEY_TRACK) {   /* ACTS: a track, a tap mutes it, a hold solos it */
            int l = KEY_ACT[i] - KEY_TRACK;
            if ((ch & keys) >> i & 1u) {
                ui.key_t[i] = plat_ms();
                ui.key_held &= ~(1u << i);
            } else if ((keys >> i & 1u) && !(ui.key_held >> i & 1u) && plat_ms() - ui.key_t[i] >= KEY_HOLD) {
                ui.key_held |= 1u << i;
                ui.solos ^= (uint8_t)(1u << l);
                mutes_apply();
                say(ui.solos >> l & 1u ? "SOLO" : "SOLO OFF", TRACK_NAME[l]);
            } else if ((ch >> i & 1u) && !(keys >> i & 1u) && !(ui.key_held >> i & 1u)) {
                if (ui.solos) {                  /* a tap while something is soloed: the solos off */
                    ui.solos = 0;
                    say("SOLOS", "OFF");
                } else {
                    ui.mutes ^= (uint8_t)(1u << l);
                    say(ui.mutes >> l & 1u ? "MUTE" : "UNMUTE", TRACK_NAME[l]);
                }
                mutes_apply();
            }
        } else if (KEY_ACT[i] == KEY_BREAK) {    /* ACTS: the breakdown, on the next bar; again: back, with a fill */
            if ((ch & keys) >> i & 1u) {
                int on = !(en_bd == 1 || ui.bd_asked);
                ui.bd_asked = (uint8_t)on;
                en_breakdown(on);
                say("BREAKDOWN", on ? "NEXT BAR" : "COMING BACK");
            }
        } else if (KEY_ACT[i] >= KEY_STING) {    /* ACTS: a sting, on the next beat */
            if ((ch & keys) >> i & 1u) {
                int cur = tr.pending >= 0 ? tr.pending : tr.on ? tr.piece : proj.piece;
                if (en_sting_len(cur, KEY_ACT[i] - KEY_STING)) {
                    en_sting(cur, KEY_ACT[i] - KEY_STING);
                    say("STING", en_sting_name(cur, KEY_ACT[i] - KEY_STING));
                }
            }
        } else if (t >= 0) {                     /* ACTS: a tap queues, a hold fades now */
            if ((ch & keys) >> i & 1u) {
                ui.key_t[i] = plat_ms();
                ui.key_held &= ~(1u << i);
            } else if ((keys >> i & 1u) && !(ui.key_held >> i & 1u) && plat_ms() - ui.key_t[i] >= KEY_HOLD) {
                ui.key_held |= 1u << i;
                tr_queue(t, 1);
            } else if ((ch >> i & 1u) && !(keys >> i & 1u) && !(ui.key_held >> i & 1u)) {
                tr_queue(t, 0);
            }
        }
    }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = enc_in(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_SETUP) {            /* Tempo is Setup's second knob */
                ui.touched = 1;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else if (ui.view >= V_REV) {           /* from an effect's page: back to Effects */
            set_view(V_SPACE);
        } else {                                 /* round the bend: past the last page to the first, and back */
            set_view(((int)ui.view + (e > 0 ? 1 : -1) + NPAGES) % NPAGES);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = enc_in(EN_ALGO)) != 0) {         /* ALGORITHM: Energy, from any page */
        char t[8];
        turn(EN_ALGO, P_ENERGY, e);
        itoa_u((uint32_t)proj.par[P_ENERGY], t);
        say("ENERGY", t);
        ui.turn_t = plat_ms();
    }
    if ((e = enc_in(EN_PRESET)) != 0) {       /* PRESETS: the next / previous story, from its first act */
        int st = (tr.pending >= 0 ? tr.pending : tr.on ? tr.piece : proj.piece) / EN_NACT;
        st = ((st + (e > 0 ? 1 : -1)) % EN_NSTORY + EN_NSTORY) % EN_NSTORY;
        tr_queue(st * EN_NACT, 0);
        say("STORY", en_story_name(st));
        ui.turn_t = plat_ms();
    }
    if (!(btn >> B_EDIT & 1u))
        ui.edit_done = 0;
    else if (!ui.edit_done && plat_ms() - ui.down_t[B_EDIT] > 600u) {   /* EDIT held: the next chord */
        ui.edit_done = 1;
        ui.btn_used |= 1u << B_EDIT;
        ui.help = 0;
        en_next();
        say("NEXT CHORD", en_playing ? "ON THE BAR" : 0);
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
        en_regrow();
        say("RESET", piece_name(proj.piece));
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = enc_in(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* four bands: the page (16 px), the knobs (60), the main view (144), the bar (20) */
#define HDR_Y 0
#define HDR_H 16
#define KNB_Y 16
#define KNB_H 60
#define MAIN_Y 76
#define MAIN_H 144                             /* (the canvas: 240 x 144 at most) */
#define BAR_Y 220
#define BAR_H 20

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6) {                                 /* larger: each row's half width, by the square root */
        for (j = -r + 1; j < r; j++) {
            int32_t w = (int32_t)(fm_sqrtf((float)(r * r - j * j)) - 0.3f);
            cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
        }
        return;
    }
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void tag(int32_t *x, const char *s, uint16_t bg, uint16_t fg)
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(*x, 6, w, 16, 6, bg);
    cv_text(*x + 5, 7, &FONT_XS, s, fg);
    *x += w + 4;
}

/* the battery (plat_battery): 0 (low) .. 3 bars, 4 a USB host powers the FM-1, -1 not read yet. The
 * simulator's "battery N" (host.c) stands in for it */
#ifdef PLAT_HOST
static int8_t batt_host = -2;                  /* -2: plat_battery's */
static int batt(void) { return batt_host > -2 ? batt_host : plat_battery(); }
#else
#define batt() plat_battery()
#endif
#define K_LOW K_ALERT                          /* the battery at one bar or none */
#define BATT_W 20                              /* the body 18 wide and its nub */
/* lying down at the header's right, its middle on the beat light's: three bars, rust at one or none; a
 * bolt instead on USB power; nothing until it is read */
static void draw_batt(int32_t x, int b)
{
    static const int8_t BOLT[7][2] = {{10, 2}, {9, 2}, {8, 2}, {7, 5}, {9, 2}, {8, 2}, {7, 2}};
    uint16_t c = b == 0 ? K_LOW : K_DIM;           /* neutral; an alert only when nearly empty */
    int i;
    cv_round(x, 3, 18, 10, 2, c);
    cv_rect(x + 1, 4, 16, 8, K_BG);
    cv_rect(x + 18, 6, 2, 4, c);
    if (b == 4) {
        for (i = 0; i < 7; i++)
            cv_rect(x + BOLT[i][0], 4 + i, BOLT[i][1], 1, K_TEXT);
        return;
    }
    for (i = 0; i < b && i < 3; i++)
        cv_rect(x + 2 + i * 5, 5, 4, 6, K_DIM);
}
static const char *batt_words(int b)
{
    static const char *const W[5] = {"Battery low", "Battery one third", "Battery two thirds", "Battery full",
                                     "On USB power"};
    return b >= 0 && b <= 4 ? W[b] : 0;
}

/* the bar along the bottom: the page and its dots, under it the story and act (where it is going, blinking while it
 * waits, a line under it as it glides) or the page's hint; a message takes the two lines for a moment. On the right:
 * the keys' mode (ACTS yellow, LEAD green) and the battery, neutral */
static const char *view_hint(void)
{
    if (ui.help == B_HOME + 1)
        return "let go of HOME to close";
    if (ui.view == V_STORY)
        return "tap a key: next phrase. hold: fade";
    if (ui.view == V_SPACE || ui.view >= V_REV)
        return ui.view >= V_REV && (ui.fx_latch >> (ui.view - V_REV) & 1u) ? "double-tap its button: off" : "double-tap: latch";
    return 0;
}
static void draw_head(void)                     /* along the very top: the page and its dots; the battery */
{
    char t[24];
    int k;
    int32_t x, pg = ui.help == B_HOME + 1 ? -1 : ui.view < NPAGES ? ui.view : -1;
    uint32_t h = hash(hash(2166136261u, (uint32_t)ui.view | (uint32_t)ui.help << 8), (uint32_t)(batt() + 1));
    if (h == ui.sig[3])
        return;
    ui.sig[3] = h;
    cv_begin(240, HDR_H, K_BG);
    caps(t, ui.help == B_HOME + 1 ? "Buttons" : VIEW_NAME[ui.view], sizeof t);
    x = cv_text(8, 1, &FONT_XS, t, K_ACC) + 6;
    for (k = 0; k < NPAGES; k++)
        cv_round(x + k * 6, 6, 4, 4, 2, k == pg ? K_ACC : K_LINE);
    if (batt() >= 0)
        draw_batt(240 - 8 - BATT_W, batt());
    cv_blit(0, HDR_Y);
}

/* the bar along the bottom: the story and act (where it is going, blinking while it waits, a line under it as it
 * glides), or the page's hint; a message takes it for a moment. At the right, the keys' mode (ACTS yellow, LEAD
 * green) */
static void draw_bar(void)
{
    int msg = plat_ms() < ui.msg_until;
    const char *hint = view_hint();
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)ui.view << 8 | (uint32_t)ui.lead << 16 | (uint32_t)ui.help << 17),
                           (uint32_t)msg),
                      (uint32_t)(tr.pending >= 0 ? plat_ms() / 400u & 1u : 0u) << 30 | (uint32_t)(tr.pending + 1) |
                          (uint32_t)tr.on << 8 | (uint32_t)tr.piece << 9 | (uint32_t)(tr.t * 20.0f) << 16);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, BAR_H, K_BG);
    cv_rect(0, 0, 240, 1, K_LINE);
    {   /* the mode, boxed, at the right */
        const char *m = ui.lead ? "LEAD" : "ACTS";
        uint16_t c = ui.lead ? K_PAD : K_ACC;
        cv_round(192, 3, 42, 16, 4, c);
        cv_round(193, 4, 40, 14, 3, K_BG);
        text_c(213, 4, &FONT_XS, m, c);
    }
    if (msg) {
        int32_t x = cv_text(8, 4, &FONT_XS, ui.msg[0], K_TEXT);
        cv_text(x + 6, 4, &FONT_XS, ui.msg[1], ui.msg_alert ? K_ALERT : K_ACC);
    } else if (hint) {
        cv_text(8, 4, &FONT_XS, hint, K_DIM);
    } else {   /* the story and act; where it is going */
        int32_t x = cv_text(8, 4, &FONT_XS, en_story_name(proj.piece / EN_NACT), K_DIM);
        x = cv_text(x + 3, 4, &FONT_XS, ".", K_LINE);
        x = cv_text(x + 3, 4, &FONT_XS, en_act_name(proj.piece), K_TEXT);
        if (tr.pending >= 0 || tr.on) {
            int goal = tr.pending >= 0 ? tr.pending : tr.piece;
            const char *g = goal / EN_NACT != proj.piece / EN_NACT ? en_story_name(goal / EN_NACT) : en_act_name(goal);
            int32_t gx = x + 5, gw = text_w(&FONT_XS, g);
            if (gx + 10 + gw <= 186) {
                cv_rect(gx, 11, 6, 1, K_DIM);            /* an arrow */
                cv_rect(gx + 4, 9, 1, 5, K_DIM);
                cv_rect(gx + 5, 10, 1, 3, K_DIM);
                if (tr.on) {                             /* the glide's progress, under the name */
                    cv_rect(gx + 10, 18, gw, 1, K_LINE);
                    cv_rect(gx + 10, 18, (int32_t)((float)gw * tr.t), 1, K_ACC);
                }
                cv_text(gx + 10, 4, &FONT_XS, g, tr.pending >= 0 && (plat_ms() / 400u & 1u) ? K_LINE : K_ACC);
            }
        }
    }
    cv_blit(0, BAR_Y);
}

/* HOME: rings, from the outside in: the drums' steps in the bar (hits larger, the step now yellow); the bass, an arc
 * as loud as it is; the bells' notes, fading behind each loop's playhead; the pad, an arc; the chord in the middle */
#define CX 120
#define CY 72
static void polar(int32_t cx, int32_t cy, float r, int i, int n, int32_t *x, int32_t *y)
{
    float a = (float)i * (FM_TWO_PI / (float)n);  /* step i of n, clockwise from the top */
    float sx = fm_sinf(a) * r, sy = fm_cosf(a) * r;
    *x = cx + (int32_t)(sx + (sx < 0.0f ? -0.5f : 0.5f));
    *y = cy - (int32_t)(sy + (sy < 0.0f ? -0.5f : 0.5f));
}

static const char *chord_now(char *nm)          /* the chord's name ("Em7") */
{
    int pcs[EN_NPAD + 1], k;
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    return en_chord_name(pcs, EN_NPAD + 1, nm);
}

#define RING_N 72
static void ring_arc(float r, float v, uint16_t track, uint16_t fill, int bold)   /* a ring of dots, v of it lit
                                                                                  * from the top; bold: in focus */
{
    int i, n = (int)(fm_clampf(v, 0.0f, 1.0f) * (float)RING_N + 0.5f);
    for (i = 0; i < RING_N; i++) {
        int32_t x, y;
        polar(CX, CY, r, i, RING_N, &x, &y);
        if (i < n)
            cv_disc(x, y, bold ? 3 : 2, fill);
        else if (bold)
            cv_disc(x, y, 1, track);
        else
            cv_rect(x, y, 1, 1, track);
    }
}

static uint32_t drum_steps(void)                 /* the hits in the bar: the thump's, and the backbeat's snare */
{
    uint32_t p = en_drum_pat[0];
    if (proj.par[P_KIT] && en_bar_len == 16)
        p |= 1u << 4 | 1u << 12;
    return p;
}

/* the rings, one of them in focus (an instrument's page: the others dimmed; -1 none) */
static uint16_t focus_c(int focus, int ring, uint16_t c) { return focus < 0 || focus == ring ? c : mix(K_BG, c, 5); }
static float ring_sc = 1.0f;                     /* (the rings' size: smaller behind Lead's notes) */
static int ring_chord = 1;                       /* (the chord's name in the middle) */
static void draw_rings(int focus)
{
    int i, l, n = en_bar_len, pos = en_drum_pos[0];
    uint32_t p = drum_steps();
    for (i = 0; i < n; i++) {                    /* the drums */
        int32_t x, y;
        int cur = en_playing && i == pos, hit = (int)(p >> i & 1u) && proj.par[P_PULSE] > 0;
        polar(CX, CY, 66.0f * ring_sc, i, n, &x, &y);
        const int b = focus == 3;                /* (in focus: larger) */
        if (cur)
            cv_disc(x, y, (hit ? 4 : 2) + b * 2, focus_c(focus, 3, K_ACC));
        else if (hit)
            cv_disc(x, y, 3 + b * 2, focus_c(focus, 3, en_pulse_rest ? K_LINE : mix(K_PULSE_T, K_PULSE, 12)));
        else
            cv_disc(x, y, 1 + b, focus_c(focus, 3, K_LINE));
    }
    ring_arc(55.0f * ring_sc, en_bass_level * (float)proj.par[P_BASS] * 0.012f, focus_c(focus, 2, K_LINE), focus_c(focus, 2, K_BASS), focus == 2);
    for (l = 0; l < EN_NLOOP; l++) {             /* the bells: each loop's last few notes, fading */
        int len = en_loop_len[l], k;
        if (!en_playing || len <= 0 || !proj.par[P_BELLS])
            continue;
        for (k = 5; k >= 0; k--) {
            int s = ((int)en_loop_pos[l] - k + len) % len;
            int32_t x, y;
            if (en_loop_note[l][s] < 0)
                continue;
            polar(CX, CY, (44.0f - (float)l * 3.0f) * ring_sc, s, len, &x, &y);
            cv_disc(x, y, (k ? 2 : 3) + (focus == 1) * 2, focus_c(focus, 1, mix(K_BG, K_BELL, k ? 12 - k * 2 : 16)));
        }
    }
    ring_arc(30.0f * ring_sc, en_pad_level * (float)proj.par[P_PAD] * 0.012f, focus_c(focus, 0, K_LINE), focus_c(focus, 0, K_PAD), focus == 0);
    if (ring_chord) {   /* the chord's name, in the middle; "held" under it while frozen */
        char nm[16];
        chord_now(nm);
        text_c(CX, CY - 9, text_w(&FONT_B, nm) <= 44 ? &FONT_B : &FONT_XS, nm,
               en_frozen ? K_FROST : focus < 0 || focus == 0 ? K_TEXT : K_DIM);
        if (en_frozen)
            text_c(CX, CY + 7, &FONT_XS, "held", K_FROST);
    }
}

/* Feel: the music's character, not its instruments. Energy (ALGORITHM) and Tension as meters; the tide (the arc,
 * minutes long, sparse to full: Motion) as a line of the last four minutes; how near the next chord is */
#define TIDE_N 120
static int8_t tide_buf[TIDE_N];                  /* the arc every 2 s, -100..100: a ring, tide_head the oldest */
static int tide_head;
static uint32_t tide_t;
#define TIDE(k) tide_buf[(tide_head + (k)) % TIDE_N]   /* k: 0 the oldest .. TIDE_N - 1 the newest */
static void tide_tick(void)
{
    if (plat_ms() - tide_t < 2000u)
        return;
    tide_t = plat_ms();
    tide_buf[tide_head] = (int8_t)fm_clampf(en_arc * 100.0f, -100.0f, 100.0f);
    tide_head = (tide_head + 1) % TIDE_N;
}
static void meter(int32_t y, int32_t h, int v, uint16_t lo, uint16_t hi)   /* 20 segments, v of 100 lit */
{
    int k, lit = (v + 2) / 5;
    for (k = 0; k < 20; k++) {
        uint16_t c = k < lit ? mix(lo, hi, (uint32_t)(k * 16 / 19)) : K_PANEL;
        cv_round(10 + k * 11, y, 9, h, 2, c);
    }
}
static void draw_feel(void)
{
    char t[8];
    int k, e = proj.par[P_ENERGY], tn = proj.par[P_TENSION];
    int32_t px = 10, py = 0;
    cv_text(10, 2, &FONT_XS, "ENERGY", K_DIM);
    cv_text(66, 2, &FONT_XS, "ALGORITHM", K_LINE);
    itoa_u((uint32_t)e, t);
    cv_text(230 - text_w(&FONT_B, t), 0, &FONT_B, t, K_TEXT);
    meter(18, 12, e, K_PAD, e > 75 ? K_ALERT : K_ACC);
    cv_text(10, 36, &FONT_XS, "TENSION", K_DIM);
    itoa_u((uint32_t)tn, t);
    cv_text(230 - text_w(&FONT_XS, t), 36, &FONT_XS, t, K_TEXT);
    meter(51, 5, tn, K_DIM, K_ALERT);
    cv_text(10, 64, &FONT_XS, "THE TIDE", K_DIM);
    cv_text(230 - text_w(&FONT_XS, "full"), 64, &FONT_XS, "full", K_LINE);
    cv_rect(10, 100, 220, 1, K_PANEL);            /* the middle: the act as it is */
    for (k = 0; k < TIDE_N; k++) {               /* sparse below, full above */
        int32_t x = 10 + k * 220 / TIDE_N, y = 100 - TIDE(k) * 18 / 100;
        if (k)
            cv_line(px, py, x, y, k == TIDE_N - 1 ? K_ACC : K_PAD);
        px = x, py = y;
    }
    cv_disc(px, py, 3, K_ACC);
    cv_text(230 - text_w(&FONT_XS, "sparse"), 112, &FONT_XS, "sparse", K_LINE);
    cv_text(10, 128, &FONT_XS, "NEXT CHORD", K_DIM);
    cv_rect(92, 134, 138, 3, K_PANEL);
    cv_rect(92, 134, (int32_t)(138.0f * fm_clampf(en_drift, 0.0f, 1.0f)), 3, K_PAD);
}

/* a button's card (help_tick): its name, then what it does. A line starting with a space goes on from the one
 * before (a screen reader joins them: sp_line) */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static const char *const BTN_DOES[12] = {"Effects", "Acts", "Reverse", "Stutter", "Lead", "Setup",
                                         "Home", "Save", "Filter", "Scatter", "Play", "Freeze"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_SEL: L("ACTS: THE KEYS"); L("ARE THE ACTS AND STORIES"); L("TAP: NEXT PHRASE. HOLD: FADE"); break;
    case B_FX: L("EFFECTS"); L("REVERB, ECHO, ECHO TIME,"); L("SURF"); break;
    case B_GLO: L("SETUP"); L("ROOT, TEMPO, SLEEP, FADE"); break;
    case B_EDIT: L("LEAD: THE KEYS PLAY A LEAD"); L("IN THE SCALE. OCT: ITS OCTAVE"); L("AGAIN: BACK TO THE ACTS"); break;
    case B_REC: L("FREEZE: THE CHORD HOLDS"); L(" AND THE REVERB RINGS ON"); L("AGAIN: THAW"); break;
    case B_OCTDN: L("THE BELLS AN OCTAVE DOWN"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("THE BELLS AN OCTAVE UP"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("SAVES EVERY ACT"); L("KEEP HOLDING: THE ACT"); L(" BACK TO ITS RECIPE"); break;
    case B_HOME: L("FX EFFECTS, SEL ACTS, ENV REVERSE,"); L(" LFO STUTTER, EDIT LEAD, GLO SETUP,");
        L(" SAVE, ARP FILTER, SEQ PHASER,"); L(" PLAY, REC FREEZE"); break;
    default: break;
    }
#undef L
    return n;
}

static void draw_buttons(void)                   /* HOME held: what each button does, laid out as they are */
{
    int r, c;
    text_c(120, 4, &FONT_XS, "WHAT EACH BUTTON DOES", K_DIM);
    for (r = 0; r < 2; r++)
        for (c = 0; c < 6; c++) {
            int i = r * 6 + c;
            int32_t x = 1 + c * 40, y = 20 + r * 48, sh = c == 2 ? -2 : c == 3 ? 2 : 0;   /* (Reverse, Stutter: apart) */
            cv_round(x, y, 38, 44, 5, K_PANEL);
            text_c(x + 19, y + 5, &FONT_XS, BTN_LABEL[i], K_ACC);
            text_c(x + 19 + sh, y + 23, &FONT_XS, BTN_DOES[i], K_TEXT);
        }
    text_c(120, 116, &FONT_XS, "SELECT: pages   PRESETS: story", K_DIM);
    text_c(120, 130, &FONT_XS, "ALGORITHM: energy", K_DIM);
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n, k;
    if (ui.help == B_HOME + 1) {
        draw_buttons();
        return;
    }
    n = help_lines(ui.help - 1, l);
    cv_round(16, 18, 208, 116, 6, K_PANEL);
    cv_round(16, 18, 208, 26, 6, K_TEXT);
    cv_rect(16, 38, 208, 6, K_TEXT);
    text_c(120, 22, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 60 + k * 20, &FONT_S, l[k] + (l[k][0] == ' '), K_TEXT);
}

/* SEL, the story's path: its name, large; its four acts as stations on a line (the act it is in filled yellow, the
 * one it is going to ringed, the line filling toward it as it glides), each with its key; under them, quietly, the
 * stories and stings and their keys; then the tracks (the black keys) in their rings' colours and the breakdown */
static void draw_map(void)
{
    static const char *const AK[EN_NACT] = {"F", "G", "A", "B"};
    static const char *const SK[EN_NSTING] = {"F5", "G5"};
    static const char *const MK[9] = {"D", "E", "F", "G", "A", "B", "C", "D", "E"};
    static const char *const TK[4] = {"F#", "G#", "A#", "C#"};
    static const char *const TN[4] = {"Pad", "Bells", "Bass", "Drums"};
    static const uint16_t TC[4] = {K_PAD, K_BELL, K_BASS, K_PULSE};
    int cur = proj.piece, goal = tr.pending >= 0 ? tr.pending : tr.on ? tr.piece : -1, blink = (plat_ms() / 300u) & 1u, i;
    int shown = goal >= 0 && goal / EN_NACT != cur / EN_NACT ? goal : cur;   /* the story it is in, or going to */
    int st = shown / EN_NACT, ca = shown == cur ? cur % EN_NACT : -1, ga = goal >= 0 && goal / EN_NACT == st ? goal % EN_NACT : -1;
    int bd_on = en_bd == 1 && ui.bd_asked, bd_wait = en_bd == 2 || (en_bd == 1 && !ui.bd_asked) || (ui.bd_asked && !en_bd);
    char t[24];
    int32_t x;
    caps(t, en_story_name(st), sizeof t);        /* the story: its key, its name */
    cv_text(10, 7, &FONT_B, MK[st], K_FROST);
    x = cv_text(26, 2, &FONT_M, t, st != cur / EN_NACT && blink ? K_LINE : K_TEXT);
    {
        char n[16] = "story ";
        itoa_u((uint32_t)st + 1u, n + 6);
        if (x + 8 + text_w(&FONT_XS, n) < 234 - 62)   /* (if the breakdown leaves room) */
            cv_text(x + 8, 11, &FONT_XS, n, K_DIM);
    }
    {   /* the path */
        const int32_t cy = 46;
        cv_rect(30, cy - 1, 180, 2, K_PANEL);
        if (ca > 0)
            cv_rect(30, cy - 1, ca * 60, 2, K_ACC);
        if (ca >= 0 && ga >= 0 && ga != ca && tr.on) {   /* gliding: the line fills toward where it is going */
            int32_t a = 30 + ca * 60, b = 30 + ga * 60, w = (int32_t)((float)(b - a) * tr.t);
            if (w > 0)
                cv_rect(a, cy - 1, w, 2, K_ACC);
            else if (w < 0)
                cv_rect(a + w, cy - 1, -w, 2, K_ACC);
        }
        for (i = 0; i < EN_NACT; i++) {
            int32_t cx = 30 + i * 60;
            const char *nm = en_act_name(st * EN_NACT + i);
            if (i == ca) {
                cv_disc(cx, cy, 12, K_ACC);
                text_c(cx, cy - 8, &FONT_B, AK[i], K_BG);
            } else if (i == ga) {
                cv_disc(cx, cy, 12, blink && tr.pending >= 0 ? K_LINE : K_ACC);
                cv_disc(cx, cy, 10, K_BG);
                text_c(cx, cy - 8, &FONT_B, AK[i], K_ACC);
            } else {
                cv_disc(cx, cy, 12, K_PANEL);
                text_c(cx, cy - 8, &FONT_B, AK[i], K_DIM);
            }
            text_c(cx, cy + 15, &FONT_XS, nm, i == ca ? K_TEXT : i == ga ? K_ACC : K_DIM);
        }
    }
    for (i = 0; i < EN_NSTORY + EN_NSTING; i++) {   /* the stories and stings, three a row */
        static const int32_t COL[3] = {4, 86, 166};
        int32_t cx = COL[i % 3], cy = 76 + (i / 3) * 13;
        int sting = i >= EN_NSTORY, here = !sting && i == cur / EN_NACT, going = !sting && goal >= 0 && i == goal / EN_NACT && !here;
        const char *k = sting ? SK[i - EN_NSTORY] : MK[i], *nm;
        if (sting) {
            int sn = i - EN_NSTORY;
            nm = en_sting_len(shown, sn) ? en_sting_name(shown, sn) : "-";
        } else {
            nm = en_story_name(i);
        }
        if (here)
            cv_round(cx - 3, cy, 80, 13, 3, K_PANEL);
        x = cv_text(cx, cy, &FONT_XS, k, sting ? K_ALERT : K_FROST);
        cv_text(x + 4, cy, &FONT_XS, nm, here ? K_TEXT : going ? (blink ? K_ACC : K_LINE) : K_DIM);
    }
    for (i = 0; i < 4; i++) {                    /* the tracks: heard in their colours, muted dim, soloed yellow */
        int solo = ui.solos >> i & 1u, heard = ui.solos ? solo : !(ui.mutes >> i & 1u);
        x = cv_text(4 + i * 60, 131, &FONT_XS, TK[i], K_LINE);
        cv_text(x + 3, 131, &FONT_XS, TN[i], solo ? K_ACC : heard ? TC[i] : K_LINE);
    }
    {   /* the breakdown, at the top right */
        const char *bn = bd_wait ? "Break..." : "Break";
        int32_t w = text_w(&FONT_XS, bn);
        cv_text(234 - w, 11, &FONT_XS, bn, bd_on ? K_ALERT : bd_wait ? (blink ? K_ALERT : K_LINE) : K_DIM);
        cv_text(234 - w - 4 - text_w(&FONT_XS, "F#5"), 11, &FONT_XS, "F#5", K_LINE);
    }
}

/* EDIT, the lead: its sound, its octave, the key it plays in */
/* EDIT, the lead: the rings, smaller and dimmed, its sound's name in the middle; around them a ring of the twelve
 * notes (the root at the top): the scale's notes marked, the root larger; a note played blooms yellow and fades */
static float lead_glow[12];
static void draw_lead(void)
{
    char t[24];
    int r = proj.par[P_ROOT], k;
    uint32_t ap = pool_abs();
    ring_sc = 0.62f;
    ring_chord = 0;
    draw_rings(9);
    ring_sc = 1.0f;
    ring_chord = 1;
    caps(t, EN_LEAD_NAME[proj.par[P_LSOUND]], sizeof t);
    text_c(CX, CY - 7, &FONT_XS, t, K_PAD);
    for (k = 0; k < 12; k++) {                   /* the notes: k semitones above the root, clockwise */
        int pc = (r + k) % 12;
        int32_t x, y;
        float g = lead_glow[pc];
        polar(CX, CY, 62.0f, k, 12, &x, &y);
        if (g > 0.05f)
            cv_disc(x, y, 4 + (int32_t)(4.0f * g), mix(K_PAD, K_ACC, (uint32_t)(g * 16.0f)));
        else if (ap >> pc & 1u)
            cv_disc(x, y, k ? 3 : 5, k ? mix(K_BG, K_PAD, 10) : K_PAD);
        else
            cv_disc(x, y, 1, K_LINE);
        if (!k || g > 0.05f)                     /* the root's name, and a note's as it blooms */
            text_c(x, y + (y < CY ? 8 : -21), &FONT_XS, EN_NOTE_NAME[pc], g > 0.05f ? K_ACC : K_PAD);   /* (inward) */
    }
}
static void lead_glow_tick(void)                 /* the notes held glow full; let go, they fade in about a second */
{
    int k;
    for (k = 0; k < 12; k++)
        lead_glow[k] *= 0.93f;
    if (!ui.lead)
        return;
    for (k = 0; k < NKEYS; k++)
        if (ui.keys >> k & 1u)
            lead_glow[en_lead_snap(53 + k) % 12] = 1.0f;
}

/* Setup: the rings dimmed behind; the sleep timer, when it runs, as a ring of frost draining around the minutes left;
 * else the key and the tempo in the middle */
static void draw_setup(void)
{
    char t[24];
    int n = 0;
    const char *s;
    ring_chord = 0;
    draw_rings(9);
    ring_chord = 1;
    cv_disc(CX, CY, 26, K_BG);                   /* (a clearing in the middle) */
    if (ui.timer) {
        uint32_t left = ui.timer_end - plat_ms(), total = (uint32_t)ui.timer * 15u * 60000u;
        ring_arc(68.0f, fm_clampf((float)left / (float)total, 0.0f, 1.0f), K_PANEL, K_FROST, 1);
        itoa_u((left + 59999u) / 60000u, t);
        text_c(CX, CY - 14, &FONT_M, t, K_FROST);
        text_c(CX, CY + 9, &FONT_XS, left < 180000u ? "fading" : "min", K_FROST);
    } else {
        for (s = EN_NOTE_NAME[proj.par[P_ROOT]]; *s;)
            t[n++] = *s++;
        if (mode_name()[1] == 'i')
            t[n++] = 'm';
        t[n] = 0;
        text_c(CX, CY - 16, &FONT_B, t, K_TEXT);
        itoa_u((uint32_t)proj.par[P_TEMPO], t);
        text_c(CX, CY - 1, &FONT_B, t, K_TEXT);
        text_c(CX, CY + 13, &FONT_XS, "bpm", K_DIM);
    }
}

/* System: a little StoryBox, blinking now and then, in front of the dimmed rings; under it, in words, how it is */
static void draw_system(void)
{
    char t[16];
    int b = batt(), blink = (plat_ms() % 4200u) < 160u;
    ring_chord = 0;
    draw_rings(9);
    ring_chord = 1;
    cv_round(CX - 34, 16, 68, 50, 10, K_PANEL);  /* the box */
    cv_round(CX - 30, 20, 60, 30, 7, K_BG);      /* its screen, a face on it */
    if (blink) {
        cv_rect(CX - 14, 34, 8, 2, K_ACC);
        cv_rect(CX + 6, 34, 8, 2, K_ACC);
    } else {
        cv_disc(CX - 10, 33, 3, K_ACC);
        cv_disc(CX + 10, 33, 3, K_ACC);
    }
    cv_rect(CX - 5, 42, 10, 2, K_ALERT);         /* a smile */
    cv_rect(CX - 7, 40, 2, 2, K_ALERT);
    cv_rect(CX + 5, 40, 2, 2, K_ALERT);
    {
        int k;                                   /* its keys */
        for (k = 0; k < 6; k++)
            cv_rect(CX - 26 + k * 9, 55, 7, 6, k == (int)(plat_ms() / 700u % 6u) ? K_PAD : K_LINE);
    }
    text_c(CX, 70, &FONT_B, "StoryBox", K_TEXT);
    text_c(CX, 86, &FONT_XS, EN_VERSION, K_DIM);
    {   /* how it is: four pills */
        static const char *const L[4] = {"Power", "Lights", "MIDI", "Load"};
        int k;
        for (k = 0; k < 4; k++) {
            int32_t x = 12 + (k % 2) * 112, yy = 104 + (k / 2) * 19;
            uint16_t c = k == 0 ? (b == 0 ? K_ALERT : K_PAD) : k == 1 ? (proj.leds ? K_ACC : K_LINE) :
                         k == 2 ? (proj.par[P_MIDI] ? K_BELL : K_LINE) : K_PULSE;
            cv_round(x, yy, 104, 16, 8, K_PANEL);
            cv_disc(x + 9, yy + 8, 3, c);
            cv_text(x + 17, yy + 1, &FONT_XS, L[k], K_DIM);
            if (k == 0)
            {
                const char *w = b == 4 ? "USB" : b == 3 ? "Full" : b == 2 ? "2/3" : b == 1 ? "1/3" : b == 0 ? "Low" : "-";
                int q = 0;
                while ((t[q] = w[q]) != 0)
                    q++;
            }
            else
                knob_text(k == 1 ? K_LEDS : k == 2 ? P_MIDI : K_CPU, t);
            cv_text(x + 98 - text_w(&FONT_XS, t), yy + 1, &FONT_XS, t, K_TEXT);
        }
    }
}

/* a picture of what an effect does, in the box (x, y, w, h): Reverse a sound swelling backwards; Stutter one moment
 * again and again; Filter a low-pass curve (its corner at Cutoff, a bump of Resonance); Phaser a wave, its notches
 * sweeping */
static void fx_picture(int i, int32_t x0, int32_t y0, int32_t w, int32_t h, uint16_t c)
{
    int k;
    int32_t my = y0 + h / 2;
    switch (i) {
    case 0: {
        int nb = w / 7;
        for (k = 0; k < nb; k++) {
            int32_t bh = 2 + k * (h - 10) / nb;
            cv_rect(x0 + k * 7, my - bh / 2 - 3, 4, bh, c);
        }
        cv_rect(x0, y0 + h - 3, nb * 7, 2, c);   /* an arrow, back */
        cv_rect(x0, y0 + h - 6, 2, 8, c);
        cv_rect(x0 + 2, y0 + h - 5, 2, 6, c);
        break;
    }
    case 1: {
        int32_t bw = w / 4;
        for (k = 0; k < 4; k++) {
            int j;
            for (j = 0; j < 5 && j * 6 < bw - 6; j++) {
                int32_t bh = (h - 6) - j * (h - 6) / 6;
                cv_rect(x0 + k * bw + j * 6, my - bh / 2, 4, bh, c);
            }
        }
        break;
    }
    case 2: {
        int32_t px = x0, py = y0 + 6, fc = x0 + 10 + proj.par[P_FLCUT] * (w - 30) / 100, q = proj.par[P_FLRES] * (h / 3) / 100;
        for (k = x0; k <= x0 + w; k += 3) {
            int32_t y = k < fc - 10 ? y0 + 6 + q : k < fc ? y0 + 6 + q - q * (k - (fc - 10)) / 10 : y0 + 6 + (k - fc) * 2;
            if (y > y0 + h - 2)
                y = y0 + h - 2;
            cv_line(px, py, k, y, c);
            px = k, py = y;
        }
        break;
    }
    default: {
        int32_t px = x0, py = my;
        float sh = (float)(plat_ms() % 4000u) * (1.0f / 4000.0f);
        for (k = x0; k <= x0 + w; k += 2) {
            float u = (float)(k - x0) / (float)w, a = 1.0f - 0.85f * fm_fabsf(fm_sinf(FM_TWO_PI * (u * 1.5f + sh)));
            int32_t y = my - (int32_t)((float)(h / 2 - 2) * a * fm_sinf(FM_TWO_PI * u * 6.0f));
            cv_line(px, py, k, y, c);
            px = k, py = y;
        }
        break;
    }
    }
}

/* FX, the effects: the four punch-ins as their buttons sit on the FM-1 (ENV LFO above ARP SEQ), each its picture, its
 * name, its button and state (held or latched: fuchsia) */
static void draw_fx_list(void)
{
    int i;
    for (i = 0; i < EN_NFX; i++) {
        int on = ((ui.fx_held | ui.fx_latch) >> i) & 1u, lat = ui.fx_latch >> i & 1u;
        int32_t x = 6 + (i % 2) * 116, y = 4 + (i / 2) * 70;
        const char *st = lat ? "LATCHED" : on ? "ON" : "hold";
        cv_round(x, y, 112, 66, 8, on ? mix(K_BG, K_ALERT, 5) : K_PANEL);
        if (on)
            cv_round(x, y, 112, 2, 1, K_ALERT);
        fx_picture(i, x + 10, y + 6, 92, 26, on ? K_ALERT : K_DIM);
        cv_text(x + 8, y + 34, &FONT_B, VIEW_NAME[V_REV + i], on ? K_ALERT : K_TEXT);
        cv_text(x + 8, y + 50, &FONT_XS, BTN_LABEL[FX_BTN[i]], K_ACC);
        cv_text(x + 104 - text_w(&FONT_XS, st), y + 50, &FONT_XS, st, on ? K_ALERT : K_DIM);
    }
}

static void draw_fx_page(void)                   /* one effect: its picture, large; its name, its state */
{
    int i = ui.view - V_REV, on = ((ui.fx_held | ui.fx_latch) >> i) & 1u, lat = ui.fx_latch >> i & 1u;
    char t[24] = "hold ";
    fx_picture(i, 30, 14, 180, 64, on ? K_ALERT : K_DIM);
    text_c(120, 90, &FONT_B, VIEW_NAME[ui.view], on ? K_ALERT : K_TEXT);
    caps(t + 5, BTN_LABEL[FX_BTN[i]], sizeof t - 5);
    text_c(120, 110, &FONT_XS, lat ? "LATCHED" : on ? "ON" : t, lat || on ? K_ALERT : K_DIM);
    if (lat)
        text_c(120, 126, &FONT_XS, "double-tap its button: off", K_DIM);
}

static int ring_focus(void)                      /* an instrument's page: its ring */
{
    return ui.view == V_PAD ? 0 : ui.view == V_BELLS ? 1 : ui.view == V_BASS ? 2 : ui.view == V_PULSE ? 3 : -1;
}
static void draw_main(void)
{
    uint32_t h, i;
    int what = ui.help ? 1 : ui.view == V_STORY ? 2 : ui.view == V_LEAD ? 3 : ui.view == V_SPACE ? 4 : ui.view >= V_REV ? 5 :
               ui.view == V_GARDEN ? 6 : ui.view == V_SETUP ? 7 : ui.view == V_SYSTEM ? 8 : 0;
    tide_tick();
    lead_glow_tick();
    if (ui.frame % 3u)                           /* 20 frames a second is plenty */
        return;
    h = hash(hash(2166136261u, (uint32_t)what | (uint32_t)ui.view << 4 | (uint32_t)ui.help << 8), proj.piece);
    switch (what) {
    case 1:
        break;
    case 2:
        h = hash(hash(hash(h, (uint32_t)(tr.pending + 1) << 8 | (uint32_t)tr.on << 16 | (uint32_t)tr.piece << 17),
                      ((plat_ms() / 300u) & 1u) | (uint32_t)(tr.t * 60.0f) << 1),
                 (uint32_t)ui.mutes | (uint32_t)ui.solos << 4 | (uint32_t)en_bd << 8 | (uint32_t)ui.bd_asked << 10);
        break;
    case 3:
        h = hash(hash(h, (uint32_t)proj.par[P_LSOUND] | (uint32_t)proj.par[P_LOCT] << 8 | (uint32_t)proj.par[P_ROOT] << 16 | pool_abs() << 20),
                 ui.keys | (uint32_t)ui.lead << 31);
        for (i = 0; i < 12; i++)
            h = hash(h, (uint32_t)(lead_glow[i] * 16.0f));
        goto rings_too;                          /* (the rings behind move too) */
        break;
    case 4:
    case 5:
        h = hash(hash(h, (uint32_t)ui.fx_held | (uint32_t)ui.fx_latch << 8),
                 (uint32_t)proj.par[P_FLCUT] | (uint32_t)proj.par[P_FLRES] << 8 | (ui.view != V_REV && ui.view != V_STUT && ui.view != V_FILT ? plat_ms() / 100u : 0u) << 16);
        break;
    case 7:
        h = hash(hash(h, (uint32_t)ui.timer | (uint32_t)proj.par[P_ROOT] << 4 | (uint32_t)proj.par[P_TEMPO] << 8 | pool_abs() << 16),
                 ui.timer ? (ui.timer_end - plat_ms()) / 2000u : 0u);
        goto rings_too;
    case 8:
        h = hash(hash(h, (uint32_t)(batt() + 1) | (uint32_t)proj.leds << 4 | (uint32_t)proj.par[P_MIDI] << 8),
                 (plat_ms() % 4200u < 160u) | (plat_ms() / 700u % 6u) << 1 | (plat_ms() / 1000u) << 4);
        goto rings_too;
    case 6:
        h = hash(hash(h, (uint32_t)proj.par[P_ENERGY] | (uint32_t)proj.par[P_TENSION] << 8 | (uint32_t)(en_drift * 40.0f) << 16),
                 tide_t);
        break;
    default:
    rings_too:
        for (i = 0; i < EN_NPAD; i++)
            h = hash(h, en_chord[i]);
        for (i = 0; i < EN_NLOOP; i++)
            h = hash(h, en_loop_pos[i] | (uint32_t)en_loop_len[i] << 8);
        h = hash(h, en_drum_pos[0] | drum_steps() << 8 | (uint32_t)en_bar_len << 24 | (uint32_t)en_pulse_rest << 31);
        h = hash(h, (uint32_t)(en_bass_level * 40.0f) | (uint32_t)(en_pad_level * 40.0f) << 8 | (uint32_t)en_frozen << 16 |
                        (uint32_t)en_playing << 17);
        for (i = 0; i < 4; i++)
            h = hash(h, (uint32_t)proj.par[i]);
        break;
    }
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    switch (what) {
    case 1: draw_help(); break;
    case 2: draw_map(); break;
    case 3: draw_lead(); break;
    case 4: draw_fx_list(); break;
    case 5: draw_fx_page(); break;
    case 6: draw_feel(); break;
    case 7: draw_setup(); break;
    case 8: draw_system(); break;
    default: draw_rings(ring_focus()); break;
    }
    cv_blit(0, MAIN_Y);
}

/* the knobs, along the top as they are on the FM-1: a cell a knob, its ring (or pips, for a switch of a few
 * positions), its value, its name. The knob being turned is drawn on a tint. Only redrawn when what it shows
 * changes (the rings cost a square root and an arctangent per pixel). */
static void draw_knobs(void)
{
    uint32_t h = hash(hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), proj.piece), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(hash(h, (uint32_t)now_touch), ui.timer ? (ui.timer_end - plat_ms()) / 60000u : 0u);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    cv_rect(0, KNB_H - 1, 240, 1, K_LINE);
    for (i = 0; i < 4; i++) {
        int k = VIEW_KNOB[ui.view][i], lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_PAD;
        if (k == K_NONE)
            continue;
        if (hot)
            cv_round(x + 3, 0, 54, 58, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        if (k == P_TENSION && v > 60)            /* a lot of Tension: an alert */
            fill = K_ALERT;
        if (k != K_CPU && hi - lo < 6) {         /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 11, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 15, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, 27, &FONT_B, t, hot ? K_ACC : K_TEXT);
        text_c(cx, 42, &FONT_XS, KNOB_LABEL[ui.view][i], hot ? K_TEXT : K_DIM);
    }
    cv_blit(0, KNB_Y);
}

/* lights at three levels: lit, mid (~12 %) and dim. In ACTS: every loaded slot lit; where it is breathes (fading down
 * to dark and back, 2.4 s); where it is going blinks quickly; empty slots and gaps dark. Tracks: heard lit, soloed
 * breathing, muted dark. The breakdown: lit; breathing while down; blinking while it waits or comes back */
#define L_OFF 0
#define L_DIM 1
#define L_MID 2
#define L_LIT 3
static int breath(void)                          /* lit, then fading down to dark and back up: 2.4 s */
{
    uint32_t t = plat_ms() % 2400u;
    return t < 900u ? L_LIT : t < 1150u ? L_MID : t < 1400u ? L_DIM : t < 1700u ? L_OFF : t < 1950u ? L_DIM :
           t < 2200u ? L_MID : L_LIT;
}
static void leds(void)
{
    uint32_t b = 0, k = 0, km = 0, kd = 0, i, ap = pool_abs(), db, play = 1u << B_PLAY;
    if (VIEW_BTN[ui.view] != NOBTN)
        b |= 1u << VIEW_BTN[ui.view];
    for (i = 0; i < EN_NFX; i++)                 /* the punch-ins on: lit */
        if ((ui.fx_held | ui.fx_latch) >> i & 1u)
            b |= 1u << FX_BTN[i];
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (ui.lead)
        b |= 1u << B_EDIT;
    if (ui.acts) {
        int goal = tr.pending >= 0 ? tr.pending : tr.on ? tr.piece : -1;
        int quick = (plat_ms() / 150u) & 1u, br = breath();
        b |= 1u << B_SEL;
        for (i = 0; i < NKEYS; i++) {
            int t = key_target((int)i), act = KEY_ACT[i] < EN_NACT, lv = L_OFF;
            if (KEY_ACT[i] >= KEY_TRACK) {
                int l = KEY_ACT[i] - KEY_TRACK;
                lv = ui.solos ? (ui.solos >> l & 1u ? breath() : L_OFF) : ui.mutes >> l & 1u ? L_OFF : L_LIT;
            } else if (KEY_ACT[i] == KEY_BREAK) {
                int down = en_bd == 1 && ui.bd_asked;
                int moving = (en_bd == 1 && !ui.bd_asked) || en_bd == 2 || (ui.bd_asked && en_bd == 0);
                lv = down ? breath() : moving ? (quick ? L_LIT : L_OFF) : L_LIT;
            } else if (KEY_ACT[i] >= KEY_STING) {
                lv = en_sting_len(proj.piece, KEY_ACT[i] - KEY_STING) ? L_LIT : L_OFF;
            } else if (t >= 0) {
                lv = L_LIT;
                if (goal >= 0 && (act ? t == goal : t / EN_NACT == goal / EN_NACT && goal / EN_NACT != proj.piece / EN_NACT))
                    lv = quick ? L_LIT : L_OFF;
                else if (act ? t == proj.piece : t / EN_NACT == proj.piece / EN_NACT)
                    lv = br;
            }
            if (lv == L_LIT)
                k |= 1u << i;
            else if (lv == L_MID)
                km |= 1u << i;
            else if (lv == L_DIM)
                kd |= 1u << i;
        }
    } else if (proj.leds)
        for (i = 0; i < NKEYS; i++)
            if (ap >> ((5u + i) % 12u) & 1u)
                k |= 1u << i;
    /* as plat_leds: PLAY green (its second LED), the unlit buttons glowing with Lights On */
    db = proj.leds == 1 ? ((1u << NB) - 1u) & ~b : 0u;
    b = (b & ~play) | (b & play ? LED_PLAY_GREEN : 0u);
    db = (db & ~play) | (db & play ? LED_PLAY_GREEN : 0u);
    plat_lights(b, k, db, kd, proj.leds == 1 ? 2 : kd ? 1 : 0);
    plat_lights_mid(0, km);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the garden grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns; a help card as it comes up; the chord as it moves (queued: the
 * garden moves it by itself, every 20 s at the quickest); growing and resting from MIDI, and a MIDI clock
 * arriving or going; what a button did (say). WHERE (the reader asks, or it has just started listening) is
 * the whole screen in two: the page, its knobs and the header, then the garden (the chord, the scale).
 * Nothing is built while no reader listens. */
static struct {
    uint8_t view, help, ext, started;
    int16_t val[4];
    uint32_t chord;                            /* the chord's name, hashed */
} sp;
static char sp_buf[ANN_MAX + 1];
static uint32_t sp_n;
/* the chord names (en_chord_name's, after the root) in words: "m" and "M7" differ only in capitals, which
 * the words file ignores */
static const char *const CHORD_Q[][2] = {
    {"", "major"}, {"m", "minor"}, {"7", "7"}, {"M7", "major 7"}, {"m7", "minor 7"}, {"6", "6"},
    {"m6", "minor 6"}, {"add9", "add 9"}, {"madd9", "minor add 9"}, {"aug", "augmented"}, {"dim", "diminished"},
    {"dim7", "diminished 7"}, {"m7b5", "half diminished"}, {"mM7", "minor major 7"}, {"sus2", "suspended 2"},
    {"sus4", "suspended 4"}, {"7sus4", "7 suspended 4"}, {"5", "5"},
};
#define NCHORD_Q (sizeof CHORD_Q / sizeof CHORD_Q[0])

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_int(uint32_t v)
{
    char t[12];
    itoa_u(v, t);
    sp_cat(t);
}
static int sp_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a value as it is said: Drift in minutes and seconds (the screen's "1:05", "45s"), the rest as shown */
static void sp_value(int k)
{
    char t[16];
    knob_text(k, t);
    if (k == P_DRIFT) {
        uint32_t m = 0, s = 0, i;
        for (i = 0; t[i] >= '0' && t[i] <= '9'; i++)
            s = s * 10u + (uint32_t)(t[i] - '0');
        if (t[i] == ':') {
            m = s;
            s = 0;
            for (i++; t[i] >= '0' && t[i] <= '9'; i++)
                s = s * 10u + (uint32_t)(t[i] - '0');
        }
        if (m) {
            sp_int(m);
            sp_cat(m == 1u ? " minute" : " minutes");
            if (!s)
                return;
            sp_cat(" ");
        }
        sp_int(s);
        sp_cat(s == 1u ? " second" : " seconds");
        return;
    }
    sp_cat(t);
}
static void sp_knob(int view, int i)           /* "Swell 60": the name the page gives it, then its value */
{
    sp_cat(KNOB_LABEL[view][i]);
    sp_cat(" ");
    sp_value(VIEW_KNOB[view][i]);
}

static void sp_chord_q(int root, const char *q)   /* "E minor 7" */
{
    uint32_t i;
    sp_cat(EN_NOTE_NAME[root % 12]);
    sp_cat(" ");
    for (i = 0; i < NCHORD_Q; i++)
        if (sp_same(q, CHORD_Q[i][0])) {
            sp_cat(CHORD_Q[i][1]);
            return;
        }
    sp_cat(q);
}
static void sp_chord(void)                     /* the chord in the middle of the garden */
{
    char nm[16];
    const char *q = chord_now(nm), *r = EN_NOTE_NAME[en_chord_root % 12u];
    while (*r && *q == *r)
        q++, r++;
    sp_chord_q(en_chord_root % 12u, q);
}

/* the page: its name and its knobs; all: the header too (the piece, its key, the tempo, growing or resting) */
static void sp_page(int all)
{
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(VIEW_NAME[ui.view]);
    sp_cat(" page");
    for (i = 0; i < 4; i++) {
        sp_cat(i ? ", " : ": ");
        sp_knob(ui.view, i);
    }
    if (!all)
        return;
    sp_cat(". Piece ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    if (en_ext) {
        sp_cat(", tempo EXT");
    } else if (ui.view != V_SETUP) {           /* (Setup's Tempo knob says it) */
        sp_cat(", tempo ");
        sp_int((uint32_t)proj.par[P_TEMPO]);
    }
    sp_cat(sp_tp.playing ? ", growing" : ", resting");
    if (ui.dirty)
        sp_cat(", not saved yet");
    if (ui.view == V_SETUP) {
        sp_cat(". CPU ");
        sp_int(plat_cpu_pct());
        sp_cat("%");
    }
    if (batt_words(batt())) {
        sp_cat(". ");
        sp_cat(batt_words(batt()));
    }
}

/* the garden: the chord and how far to the next, the scale from the root up ("F and F sharp": one letter's
 * two notes joined, so they are heard as two), the pulse sitting out */
static void sp_garden(void)
{
    uint32_t ap = pool_abs();
    int i, last = -1;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("Chord ");
    sp_chord();
    if (en_frozen) {
        sp_cat(", frozen");
    } else {
        sp_cat(", next chord ");
        sp_int((uint32_t)(fm_clampf(en_drift, 0.0f, 1.0f) * 100.0f + 0.5f));
        sp_cat("% of the way");
    }
    sp_cat(". Scale");
    for (i = 0; i < 12; i++) {
        int pc = (proj.par[P_ROOT] + i) % 12;
        if (ap >> pc & 1u) {
            sp_cat(last < 0 ? " " : EN_NOTE_NAME[last][0] == EN_NOTE_NAME[pc][0] ? " and " : ", ");
            sp_cat(EN_NOTE_NAME[pc]);
            last = pc;
        }
    }
    if (en_pulse_rest && proj.par[P_PULSE])
        sp_cat(". Pulse resting");
}

/* a help card: its button, then its lines ("ENV button. THE PAD. LEVEL, SWELL, BRIGHT, COLOUR") */
static void sp_line(const char *s)
{
    char p = sp_n ? sp_buf[sp_n - 1] : 0;
    if (*s == ' ')                             /* it goes on from the line before */
        s++;
    else if (p != ',' && p != ':')
        sp_cat(".");
    sp_cat(" ");
    sp_cat(s);
}
static void sp_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(BTN_LABEL[ui.help - 1]);
    sp_cat(" button");
    for (k = 0; k < n; k++)
        sp_line(l[k]);
}

static void sp_piece_text(void)                /* "PIECE Departures, C# Ionian, tempo 60" */
{
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("PIECE ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    sp_cat(", tempo ");
    sp_int((uint32_t)proj.par[P_TEMPO]);
}
static void sp_piece(void)
{
    if (!ann_on())
        return;
    sp_piece_text();
    ann_say(sp_buf, 0, ANN_NOW);
    sp_said = 1;
}

static uint32_t sp_chord_sig(void)
{
    char nm[16];
    const char *s = chord_now(nm);
    uint32_t h = 2166136261u;
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}
static void sp_snap(void)
{
    int i;
    for (i = 0; i < 4; i++)
        sp.val[i] = (int16_t)knob_get(VIEW_KNOB[ui.view][i]);
    sp.view = ui.view;
    sp.help = ui.help;
    sp.ext = en_ext;
    sp.chord = sp_chord_sig();
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {                       /* everything on the screen, in two */
        sp.started = 1;
        sp_snap();
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            sp_page(1);
        } else {
            sp_page(1);
            ann_say(sp_buf, 0, ANN_NOW);
            sp_garden();
        }
        ann_say(sp_buf, 0, 0);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        sp_snap();
        return;
    }
    if (en_playing != sp_tp.playing && now - sp_tp.play_t > 150u) {   /* MIDI start / stop */
        sp_tp.playing = en_playing;
        ann_say(en_playing ? "GROWING" : "RESTING", 0, 0);
    }
    if (en_ext != sp.ext) {                           /* a MIDI clock arrived, or stopped */
        sp.ext = en_ext;
        sp_n = 0;
        sp_cat("Tempo ");
        if (en_ext)
            sp_cat("EXT");
        else
            sp_int((uint32_t)proj.par[P_TEMPO]);
        ann_say(sp_buf, 0, 0);
    }
    if (ui.help != sp.help) {                         /* a help card: at once */
        sp.help = ui.help;
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            return;
        }
    }
    if (ui.view != sp.view) {                         /* a new page: said when it settles */
        sp_snap();
        sp_page(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    for (i = 0; i < 4; i++) {                         /* a knob turning */
        int v = knob_get(VIEW_KNOB[ui.view][i]);
        if (v != sp.val[i] && VIEW_KNOB[ui.view][i] != K_CPU) {   /* (the load moves by itself: not said) */
            sp.val[i] = (int16_t)v;
            if (changed < 0)
                changed = i;
        }
    }
    if (changed >= 0) {
        sp_n = 0;
        sp_knob(ui.view, changed);
        ann_later(sp_buf, ANN_NOW, 120u, now);
    }
    c = sp_chord_sig();
    if (c != sp.chord) {                              /* the chord moved: queued, after what is being said */
        sp.chord = c;
        sp_n = 0;
        sp_cat("Chord ");
        sp_chord();
        ann_say(sp_buf, 0, 0);
    }
}

#ifdef PLAT_HOST
/* every page, knob value, help card, chord and message as a screen reader gets them (the "speakdump"
 * command): KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"},
                                         {"NEXT CHORD", "ON THE BAR"}, {"NEXT CHORD", 0}, {"RESTING", 0},
                                         {"GROWING", 0}, {"FREEZE", "ON"}, {"FREEZE", "OFF"}, {"SAVED", 0}};
    static const char *const WARN[][2] = {{"THE ROOT STAYS", "(SETUP)"}, {"3 NOTES", "AT LEAST"},
                                          {"NOT SAVED", "FLASH ERROR"}};
    project_t keep = proj;
    uint8_t keep_view = ui.view, keep_help = ui.help, keep_dirty = ui.dirty, keep_play = sp_tp.playing;
    uint8_t keep_ext = en_ext, keep_frozen = en_frozen, keep_rest = en_pulse_rest;
    int v, i, x, lo, hi;
    char t[48];
    for (v = 0; v < NVIEWS; v++) {
        ui.view = (uint8_t)v;
        sp_page(0);
        sp_print("SCREEN", sp_buf);
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[v][i];
            knob_range(k, &lo, &hi);
            for (x = lo; x <= hi; x++) {
                if (k < P_NPARAMS)
                    proj.par[k] = (int16_t)x;
                else
                    proj.leds = (uint8_t)x;
                sp_n = 0;
                sp_knob(v, i);
                sp_print("KNOB", sp_buf);
            }
            proj = keep;
        }
    }
    for (x = 0; x < 8; x++) {                         /* WHERE: growing, resting, unsaved, a clock, frozen, custom */
        ui.view = (uint8_t)(x * 3 % NVIEWS);
        sp_tp.playing = (uint8_t)(x & 1);
        ui.dirty = (uint8_t)(x >> 1 & 1);
        en_ext = (uint8_t)(x == 3);
        en_frozen = (uint8_t)(x == 5);
        en_pulse_rest = (uint8_t)(x == 2);
        proj.pool = x == 6 ? 0xFFFu : x == 7 ? 0x91u : keep.pool;
        proj.par[P_ROOT] = (int16_t)(x * 5 % 12);
        sp_page(1);
        sp_print("WHERE", sp_buf);
        sp_garden();
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    ui.dirty = keep_dirty;
    sp_tp.playing = keep_play;
    en_ext = keep_ext;
    en_frozen = keep_frozen;
    en_pulse_rest = keep_rest;
    ui.view = keep_view;
    for (i = 0; i < NB; i++) {
        const char *l[HELP_LINES];
        if (!help_lines(i, l))
            continue;
        ui.help = (uint8_t)(i + 1);
        sp_help();
        sp_print("HELP", sp_buf);
    }
    ui.help = keep_help;
    for (x = 0; x < 12; x++)
        for (i = 0; i < (int)NCHORD_Q; i++) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord_q(x, CHORD_Q[i][0]);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], t);
        sp_print("MSG", t);
    }
    for (i = 0; i < (int)(sizeof WARN / sizeof WARN[0]); i++) {
        msg_text(WARN[i][0], WARN[i][1], t);
        sp_print("WARN", t);
    }
    for (x = 0; x < 12; x++) {
        msg_text("ADDED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
        msg_text("REMOVED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < EN_NMODES; x++) {
        msg_text("MODE", EN_MODE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = -2; x <= 2; x++) {
        char o[8];
        en_param_text(P_BOCT, x, o);
        msg_text("BELL OCTAVE", o, t);
        sp_print("MSG", t);
    }
    for (x = 30; x <= 120; x += 45) {
        char o[8];
        itoa_u((uint32_t)x, o);
        msg_text("TEMPO", o, t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NPIECES; x++) {
        project_piece(x);
        sp_piece_text();
        sp_print("MSG", sp_buf);
        msg_text("RESET", piece_name(x), t);
        sp_print("MSG", t);
    }
    proj = keep;
    sp_print("MSG", "Tempo EXT");
    sp_print("MSG", "Tempo 72");
    for (x = 0; x <= 4; x++)                          /* the battery, at the end of WHERE's first part */
        sp_print("WHERE", batt_words(x));
}
#endif

/* the loading screen: the home screen's rings, still, around the name; in two bands (the canvas holds 144 rows) */
void ui_splash(void)
{
    static const uint16_t C[4] = {K_PULSE, K_BASS, K_BELL, K_PAD};
    static const float R[4] = {66.0f, 55.0f, 44.0f, 30.0f};
    int band, r, i;
    for (band = 0; band < 2; band++) {
        int32_t y0 = band ? 144 : 0, h = band ? 96 : 144;
        cv_begin(240, (uint32_t)h, K_BG);
        for (r = 0; r < 4; r++)
            for (i = 0; i < 48; i++) {
                int32_t x, y;
                polar(120, 120 - y0, R[r], i, 48, &x, &y);
                if (i % (r + 2) == 0)
                    cv_disc(x, y, r == 0 ? 2 : 1, C[r]);
                else
                    cv_rect(x, y, 1, 1, K_LINE);
            }
        text_c(120, 111 - y0, &FONT_B, APP_NAME, K_ACC);
        text_c(120, 214 - y0, &FONT_XS, "stories, music box, sleep", K_DIM);
        cv_blit(0, y0);
    }
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    ui.acts = 1;                                 /* the keys: the acts, from the start */
    memset(&tr, 0, sizeof tr);
    tr.pending = -1;
    lcd_fill(0, 0, 240, 240, K_BG);
    if (!en_playing)
        en_play(1);                              /* it grows from the start */
    sp_tp.playing = 1;
    sp_tp.play_t = plat_ms();
}

#define DARK_AFTER 30000u
void ui_frame(void)
{
    input();
    tr_tick();
    autosave();
    if (!ui.dark && ui.timer && plat_ms() - ui.input_t > DARK_AFTER) {   /* the sleep timer on, untouched: dark */
        ui.dark = 1;
        cv_begin(240, 144, 0);
        cv_blit(0, 0);
        cv_begin(240, 96, 0);
        cv_blit(0, 144);
        plat_lights(0, 0, 0, 0, 0);
    }
    if (ui.dark) {
        ui.frame++;
        return;
    }
    draw_head();
    draw_bar();
    draw_main();
    draw_knobs();
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void)
{
    input();
    tr_tick();
}
