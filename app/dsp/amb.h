/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument: a generative ambient garden. Four layers draw on one pool of notes (a scale:
 * the root plus up to eleven more pitch classes) and one chord that drifts slowly through it:
 *
 *   PAD     four breathing voices: each swells in, hangs and fades on its own cycle (unequal lengths,
 *           as the loops of Music for Airports), taking its note of the chord as it starts, so the
 *           chord forms and dissolves. Two colours, by piece: a filtered Rhodes (FM, blooming brighter
 *           as it swells) or Voices (saws through two formants, as "2/1"); then a soft low-pass and an
 *           ensemble chorus. The chord moves every DRIFT (20 s .. 5 min): voices on shared
 *           notes hold, the others breathe in anew.
 *   BELLS   soft two-operator FM bells played by four tape loops of unequal length (13, 17, 23, 29
 *           quarter notes), each holding a short phrase that shifts against the others, each its own
 *           shade of bell. EVOLVE changes a note now and then; DENSITY sets how many notes a loop holds.
 *   BASS    a sine and its sub-octave on the chord's root (now and then its fifth), gliding.
 *   PULSE   a soft thump, a closed hat and an open hat, each on its own Euclidean pattern (16, 12 and 7
 *           sixteenths): polyrhythms. HITS sets how many; the patterns turn and change around it.
 *
 * Over it all, the ARC: a slow tide, minutes long, between sparse (fewer bells, a darker vowel, a quieter
 * pulse, more reverb) and full; MOTION sets how far it goes. And rests: with EVOLVE, the pulse
 * sits out a few bars, the bass a chord, a bell loop a pass.
 * Then, after Discreet Music's chain, an Echoplex (160 ms, fed back) and a tape loop (to ~5.6 s, two
 * heads, each repeat darker), and Dattorro's plate (from FoMni), fed generously: the space is most of
 * what is heard.
 * FREEZE holds the chord and lets the plate ring on forever.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NLOOP 4                     /* bell tape loops */
#define EN_LOOP_MAX 29
#define EN_NDRUM 3                     /* pulse: thump, closed hat, open hat */
#define EN_NPAD 4                      /* pad voices */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, MODE_MEL_MIN, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern volatile uint8_t en_loop_len[EN_NLOOP];   /* (StoryBox: Structure chooses them) */
extern const uint8_t EN_DRUM_N[EN_NDRUM];

/* parameters: value ranges in en_param_info() */
enum {
    P_PAD, P_BELLS, P_BASS, P_PULSE,                  /* layer levels */
    P_DENSITY, P_EVOLVE, P_DRIFT, P_HITS,             /* motion */
    P_ECHO, P_ETIME, P_REVERB, P_SIZE,                /* space */
    P_BRIGHT, P_SWELL, P_BTONE, P_MOTION,             /* tone */
    P_TUNE, P_MIDI, P_TEMPO, P_ROOT, P_MODE, P_BOCT,  /* setup, panel encoders, bell octave */
    P_PADTONE,                                        /* the pad's colour, set by the piece: Rhodes, Voices */
    P_BSOUND, P_GLIDE, P_FIFTH,                       /* bass: its sound (0.5, its octave's place: Round, Upright, Synth,
                                                       * Sub), its glide, how often it leans on the fifth */
    P_SWING, P_HATS,                                  /* pulse: swing, the hats' colour */
    P_FREEDOM,                                        /* StoryBox: the chords, 0 the act's progression as written ..
                                                       * 100 free (the progression ignored) */
    P_TENSION,                                        /* dissonance: bells off by a semitone, a b9 and a tritone in the
                                                       * chord, the bass on the b2 */
    P_METER,                                          /* 0 4/4, 1 3/4 (from the next bar) */
    P_ENERGY,                                         /* the player's: 50 as the act has it; lower calmer and sparser,
                                                       * higher fuller, busier and brighter (the ALGORITHM knob) */
    P_LSOUND, P_LLEVEL, P_LTONE, P_LGLIDE, P_LOCT,    /* StoryBox's lead: its sound (EN_LEAD_NAME), level, tone (dark ..
                                                       * bright), glide (legato sounds) and octave */
    P_GROOVE,                                         /* Structure: 0 loose (polyrhythmic hats, prime bell loops drifting,
                                                       * chords free to vary) .. 100 steady (patterns and bell loops a
                                                       * bar or two long on the downbeat, chords as written) */
    P_KIT,                                            /* the pulse's kit: 0 soft (thump, two hats), 1 backbeat (a lo-fi
                                                       * snare and clap on two and four, a boom-bap kick; 4/4) */
    P_RVLEN, P_RVMIX, P_STRATE, P_STMIX,              /* the punch-in effects (the player's): Reverse's length and mix, */
    P_FLCUT, P_FLRES, P_SCSIZE, P_SCMIX,            /* Stutter's rate and mix, Filter's cutoff and resonance, Phaser's */
    P_SURF,                                           /* the act's: an ocean's rush, waves of filtered noise (0 none) */
    P_MSOUND,                                         /* the act's: the melody's instrument (EN_MEL_NAME; the bells' loops) */
    P_SHIMMER, P_MECHO, P_DRIVE, P_CRUSH,             /* the act's: each instrument's own effect: the pad's shimmer (an
                                                       * octave up in the reverb), the melody's echo, the bass's drive,
                                                       * the drums' crush (fewer bits, a lower rate) */
    P_HVSHIM, P_HVWIDTH, P_STLEN, P_STPITCH,          /* 0.8: the punch-ins' knobs 3 and 4: Harmony's shimmer (its voices
                                                       * fed back, climbing) and width; Stutter's length (a slice's share
                                                       * of its rate) and pitch (-50 a tape slowing .. 50 rising); */
    P_FLSWEEP, P_FLTYPE, P_SCPITCH, P_SCSPREAD,       /* Filter's sweep (by itself, 0 off) and type (low, high, band);
                                                       * Sparkle's pitch (0 as is .. 100 an octave up) and spread */
    P_OSLEVEL, P_OSSPACE, P_OSECHO, P_OSTONE,         /* 0.8: the one-shots' (the themes'), all together, the story's:
                                                       * level, their own reverb, echo, and tone (dark .. bright) */
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_pool(uint32_t pool);                   /* the scale, relative to the root (bit 0 always set) */
void en_play(int on);                          /* the garden grows / rests (the tails ring on) */
void en_next(void);                            /* the next chord: on the next bar (now while resting) */
void en_freeze(int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new piece: fresh loops, the home chord */
void en_prog(int piece);
/* StoryBox performance: layers muted (bit 0 pad, 1 bells, 2 bass, 3 pulse; faded in and out quickly), and the
 * breakdown: the bass and the pulse drop out on the next bar (on 1); on 0 they come back a bar after a fill */
void en_mute(uint32_t mask);
/* the punch-in effects on the whole sound, each fading in and out in 30 ms: bit 0 Reverse (the last moment backwards),
 * 1 Stutter (the last slice of a beat, again and again), 2 Filter (a sweep down to its cutoff), 3 Phaser */
enum { FX_REVERSE, FX_STUTTER, FX_FILTER, FX_SCATTER, EN_NFX };
void en_fx(uint32_t mask);
extern volatile float en_fade;                 /* StoryBox: the whole sound's level (the sleep timer), 0..1 */
extern volatile uint8_t en_fx_on;              /* (UI) the effects heard now */
/* the lead: the keys in Lead mode (and MIDI in), snapped to the scale, three notes at a time. Struck sounds ring
 * on; bowed ones (violin, cello) last while held; the flute and the slide are one voice, gliding from note to note */
enum { LEAD_KALIMBA, LEAD_BELL, LEAD_FLUTE, LEAD_GUITAR, LEAD_RHODES, LEAD_VIOLIN, LEAD_CELLO, LEAD_SLIDE, EN_NLEAD };
extern const char *const EN_LEAD_NAME[EN_NLEAD];
enum { MEL_BELLS, MEL_KALIMBA, MEL_MARIMBA, MEL_VIBES, MEL_SLIDE, MEL_GLASS, MEL_MUSICBOX, MEL_BANJO, EN_NMEL };
extern const char *const EN_MEL_NAME[EN_NMEL];
int en_lead_snap(int note);                    /* the scale's note at or just below note (the main loop's view) */
void en_lead_on(int note, int vel);
void en_lead_off(int note);
void en_answer(int amount);                     /* StoryBox 0.6: the lead answered, 0 off .. 100 free */
extern volatile uint8_t en_ans_ring[16];        /* the answer's notes as they sound (key, | 0x80 let go) */
extern volatile uint8_t en_ans_w;
void en_breakdown(int on);
extern volatile uint8_t en_bd;                 /* (UI) 0 off, 1 broken down, 2 coming back (the fill) */                       /* StoryBox: follow piece's progression (none: free), from its start */
extern volatile int8_t en_prog_step;           /* (UI) the progression's step playing, -1 none */
extern volatile int16_t en_loop_bar, en_loop_bars;   /* (UI, 0.8.1) the bar in the progression's loop (-1: none), of how many */
int en_prog_len(int piece);                    /* its steps, 0 none */
/* MIDI clock in (as FoMni): ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile uint8_t en_chord[EN_NPAD];     /* the chord's notes (MIDI), pad voices' targets */
extern volatile uint8_t en_chord_root;         /* the chord's root, MIDI pitch class 0..11 */
extern volatile uint8_t en_chord_n;            /* its distinct tones: 3 or 4 */
extern volatile float en_drift;                /* 0..1: how far to the next chord */
extern volatile float en_pad_level, en_bass_level;
extern volatile uint8_t en_loop_pos[EN_NLOOP];
extern volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];   /* -1 rest, else the slot's offset */
extern volatile float en_loop_glow[EN_NLOOP];  /* 0..1: a bell just rang from this loop */
extern volatile uint8_t en_drum_pos[EN_NDRUM];
extern volatile uint32_t en_drum_pat[EN_NDRUM];/* bit i: a hit on step i */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile uint32_t en_barno;             /* bars played since start (StoryBox: phrases are two of them) */
extern volatile uint8_t en_bar_len;            /* sixteenths in a bar: 16 (4/4) or 12 (3/4) */
extern volatile uint8_t en_drum_n[EN_NDRUM];   /* each pulse's cycle now (the thump's follows the bar) */
extern volatile float en_arc;                  /* the arc, -1 sparse .. +1 full (times Motion) */
extern volatile uint8_t en_pulse_rest;         /* the pulse is sitting a few bars out */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice sings now (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* voice leading: new pad notes for the tones, each voice moving as little as possible, all in lo..hi */
void en_voice_lead(const int *from, const int *pcs, int ntones, int *to, int lo, int hi);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */

/* StoryBox: stories of EN_NACT acts; a piece is one act of one story (piece = story * EN_NACT + act), its recipe
 * for every parameter (Tune, MIDI out and Root included, but they are the player's) */
#define EN_NSTORY 10
#define EN_NACT 4
#define EN_NPIECES (EN_NSTORY * EN_NACT)
const char *en_story_name(int s);
int en_story_key(int s);                       /* the key its chords and stings are written in (pitch class) */
/* StoryBox stings: two one-shots a story (a note or a chord on bells, ringing long), on the next beat */
#define EN_NSTING 3                             /* (0.7: a third, for Fast and Slow's themes together) */
int en_sting_len(int piece, int k);            /* its notes, 0 none */
int en_theme_busy(void);                        /* a one-shot theme: 0 none, 1 waiting for the loop, 2 playing */
const char *en_prog_text(int piece);            /* an act's progression as written ("G:2 Em:2"), or 0 */
int en_prog_steps(void);                        /* (0.8.6) the progression playing: its steps */
int en_prog_label(int i, char *buf);            /* step i: its chord's name into buf (16), its bars (0: none) */
const char *en_sting_name(int piece, int k);
void en_sting(int piece, int k);
const char *en_act_name(int piece);
const char *en_piece_name(int i);              /* the piece's story */
void en_piece(int i, int16_t *par, uint16_t *pool);
