/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument (amb.h). One compilation unit, -O2. */
#include "amb.h"
#include "fastmath.h"

#if defined EN_HOST || defined PLAT_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_MODE_NAME[EN_NMODES] = {"Ionian", "Dorian", "Phrygian", "Lydian", "Mixolyd.", "Aeolian",
                                             "Pent maj", "Pent min", "Mel. min"};
#define B(a, b, c, d, e, f, g) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e) | 1u << (f) | 1u << (g))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    B(0, 2, 4, 5, 7, 9, 11), B(0, 2, 3, 5, 7, 9, 10), B(0, 1, 3, 5, 7, 8, 10), B(0, 2, 4, 6, 7, 9, 11),
    B(0, 2, 4, 5, 7, 9, 10), B(0, 2, 3, 5, 7, 8, 10), B(0, 2, 4, 7, 9, 9, 9),  B(0, 3, 5, 7, 10, 10, 10),
    B(0, 2, 3, 5, 7, 9, 11)};
#undef B
/* the bells' loops, in beats: StoryBox's Structure chooses them (loops_for): drifting primes (13 17 23 29), loops that
 * agree every few bars, or loops a bar, two or four long on the downbeat */
volatile uint8_t en_loop_len[EN_NLOOP] = {13, 17, 23, 29};
const uint8_t EN_DRUM_N[EN_NDRUM] = {16, 12, 7};
volatile uint8_t en_drum_n[EN_NDRUM] = {16, 12, 7};
volatile uint8_t en_bar_len = 16;
volatile uint32_t en_barno;

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Pad", 0, 100, 60},      {"Bells", 0, 100, 65},    {"Bass", 0, 100, 45},     {"Pulse", 0, 100, 40},
    {"Density", 0, 100, 40},  {"Evolve", 0, 100, 35},   {"Drift", 0, 100, 25},    {"Density", 0, 100, 35},
    {"Echo", 0, 100, 45},     {"Echo time", 0, 5, 3},   {"Reverb", 0, 100, 65},   {"Size", 0, 100, 80},
    {"Bright", 0, 100, 40},   {"Swell", 0, 100, 60},    {"Bell tone", 0, 100, 50}, {"Motion", 0, 100, 50},
    {"Tune", -50, 50, 0},     {"MIDI out", 0, 1, 1},    {"Tempo", 30, 120, 72},   {"Root", 0, 11, 2},
    {"Mode", 0, EN_NMODES - 1, 1}, {"Bell oct", -2, 2, 0}, {"Pad tone", 0, 3, 0},
    {"Bass sound", 0, 3, 0},   {"Glide", 0, 100, 20},    {"Fifth", 0, 100, 35},
    {"Swing", 0, 100, 0},     {"Hats", 0, 100, 50},     {"Freedom", 0, 100, 100},
    {"Tension", 0, 100, 0},   {"Meter", 0, 1, 0},       {"Energy", 0, 100, 50},
    {"Lead", 0, EN_NLEAD - 1, 0}, {"Lead level", 0, 100, 65}, {"Lead tone", 0, 100, 50}, {"Glide", 0, 100, 20},
    {"Lead oct", -2, 2, 0},   {"Structure", 0, 100, 0}, {"Kit", 0, 4, 0},
    {"Interval", 0, 2, 0},    {"Mix", 0, 100, 60},     {"Rate", 0, 3, 1},        {"Mix", 0, 100, 100},
    {"Cutoff", 0, 100, 25},   {"Resonance", 0, 100, 40}, {"Size", 0, 100, 40},     {"Mix", 0, 100, 100},
    {"Surf", 0, 100, 0},      {"Melody", 0, EN_NMEL - 1, 0},
    {"Shimmer", 0, 100, 0},   {"Echo", 0, 100, 50},     {"Drive", 0, 100, 0},     {"Punch", 0, 100, 0},
    {"Shimmer", 0, 100, 0},   {"Width", 0, 100, 50},    {"Length", 0, 100, 100},   {"Pitch", -50, 50, 0},
    {"Sweep", 0, 100, 0},     {"Type", 0, 2, 0},        {"Pitch", 0, 100, 100},   {"Spread", 0, 100, 50},
    {"Level", 0, 100, 70},    {"Space", 0, 100, 40},    {"Echo", 0, 100, 20},     {"Tone", 0, 100, 50},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

/* StoryBox: the stories. Each is four acts, and each act a recipe for every parameter (Tune, MIDI out and Root
 * are the player's, not the act's). A piece, below, is one act of one story: piece = story * EN_NACT + act.
 *                     Pad Bel Bas Pul  Den Evo Dri Hit  Ech ET Rev Siz  Bri Swe Ton Mot  Tun MI Tem Rt Md Oc  PT  BO Gli Fif  Swi Hat
 * (Md: 0 Ionian 1 Dorian 2 Phrygian 3 Lydian 4 Mixolydian 5 Aeolian 6 pent. major 7 pent. minor; PT: the pad's colour,
 * 0 Rhodes, 1 Voices, 2 Strings, 3 Organ; Oc: the bells' octave; BO: the bass's octave) */
/* Each act may have a progression of up to PROG_MAX chords, each "CHORD:bars" (bars 1 if left out), 0 or "" none
 * (free). A CHORD is either a degree of the act's scale (1 its first chord: "1:2 4:2 7:2 3:2"), or a name in the
 * story's key (key: G for Inside), moved with the player's Root: a root (C D E F G A B, # or b), a quality
 * (none major, m, 7, m7, maj7, dim, aug, sus2, sus4, add9, 6, m6, magic: root, fifth, sixth and major seventh) and
 * a bass (/F#): "G:2 Bm/F#:2 Em:2 D:2". Named chords need not be in the scale: the bells bend to them.
 * Freedom blends it: 0 as written, more and more varied (a chord a third away, a chord held, a bar longer or
 * shorter) toward 100, where it is ignored.
 * Stings: notes with octaves ("G6", "G3 G3 D4 E4 F#4 G4"), in the story's key, moved with Root. */
#define PROG_MAX 12
#define STING_MAX 6
static const struct {
    const char *name;
    const char *act[EN_NACT];
    int8_t v[EN_NACT][P_NPARAMS];
    const char *prog[EN_NACT];
    int8_t key;
    const char *sting[EN_NSTING];
    const char *sting_name[EN_NSTING];
} STORIES[EN_NSTORY] = {
    /* Pre-show (0.7: its drums the Lounge kit, a ride's swing and a cross-stick on four; Lights Down brushed) */
    {"Pre-show", {"Doors", "Warm Up", "Lounge", "Lights Down"}, {
        {50, 40, 50, 45, 30, 50, 30, 45, 40, 2, 50, 65, 45, 45, 55, 40, 0, 1, 86, 2, 1, 0, 0, 0, 15, 30, 70, 55, 50, 0, 0, 50, 0, 60, 45, 15, 0, 70, 3},
        {55, 55, 60, 62, 45, 60, 25, 62, 40, 2, 45, 60, 55, 40, 60, 50, 0, 1, 88, 2, 1, 0, 0, 0, 10, 40, 70, 65, 35, 0, 0, 50, 0, 60, 45, 15, 0, 70, 3},
        {55, 70, 55, 58, 55, 70, 25, 58, 50, 1, 50, 65, 60, 45, 70, 55, 0, 1, 88, 2, 4, 0, 0, 0, 15, 40, 65, 70, 60, 0, 0, 50, 0, 60, 45, 15, 0, 70, 3},
        {50, 35, 40, 40, 25, 50, 35, 40, 50, 3, 60, 75, 35, 55, 45, 40, 0, 1, 80, 2, 1, 0, 0, 0, 20, 30, 70, 45, 100, 0, 0, 50, 0, 60, 45, 15, 0, 70, 2}},
        {"1:2 4:2", "1:2 4:2 7:2 3:2", "1:2 7:2 4:2 1:2", 0}, 2, {0}, {0}},
    {"Sleep", {"Dusk", "Drifting", "Deep", "Stillness"}, {
        {50, 45, 30, 0, 30, 30, 55, 0, 40, 4, 55, 85, 35, 70, 30, 45, 0, 1, 56, 2, 6, 0, 0, 0, 40, 25, 0, 40, 100, 0, 0, 50, 2, 50, 30, 40, 0, 0, 0},
        {50, 35, 25, 0, 25, 25, 65, 0, 35, 5, 58, 86, 28, 75, 20, 50, 0, 1, 52, 2, 6, -1, 1, -1, 45, 20, 0, 40, 100, 0, 0, 50, 2, 50, 30, 40, 0, 0, 0},
        {50, 20, 20, 0, 18, 20, 75, 0, 25, 5, 60, 88, 20, 80, 10, 45, 0, 1, 50, 2, 6, -1, 1, -1, 50, 15, 0, 40, 100, 0, 0, 50, 2, 50, 30, 40, 0, 0, 0},
        {45, 10, 10, 0, 12, 15, 85, 0, 20, 5, 60, 88, 15, 85, 5, 35, 0, 1, 48, 2, 6, -1, 1, -1, 50, 10, 0, 40, 100, 0, 0, 50, 2, 50, 30, 40, 0, 0, 0}}, {0}, 2, {0}, {0}},
    {"Lighthouse", {"Fog", "Signal", "Storm", "Dawn"}, {
        {68, 30, 38, 0, 25, 30, 45, 0, 55, 4, 62, 86, 30, 70, 20, 60, 0, 1, 66, 2, 5, -1, 1, 0, 35, 25, 0, 40, 100, 10, 0, 50, 1, 60, 50, 10, 0, 20, 0},
        {60, 62, 42, 20, 40, 40, 35, 25, 50, 3, 65, 85, 42, 55, 80, 55, 0, 1, 66, 2, 5, 1, 1, 0, 25, 30, 0, 60, 100, 15, 0, 50, 1, 60, 50, 10, 0, 20, 0},
        {66, 60, 60, 60, 60, 70, 15, 70, 60, 1, 60, 80, 50, 35, 70, 90, 0, 1, 76, 2, 2, 0, 1, 0, 15, 45, 20, 80, 100, 25, 0, 50, 1, 60, 50, 10, 0, 20, 0},
        {62, 55, 38, 15, 40, 35, 40, 15, 40, 3, 70, 85, 65, 60, 40, 50, 0, 1, 66, 2, 3, 0, 1, 0, 30, 30, 0, 40, 100, 10, 0, 50, 1, 60, 50, 10, 0, 20, 0}}, {0}, 2, {0}, {0}},
    {"Market Day", {"Morning", "Crowds", "Siesta", "Lanterns"}, {
        {50, 58, 38, 35, 45, 40, 30, 40, 35, 2, 50, 65, 70, 45, 65, 50, 0, 1, 100, 2, 0, 1, 0, 0, 15, 35, 25, 65, 100, 0, 0, 50, 0, 65, 60, 10, 0, 15, 0},
        {45, 72, 55, 62, 70, 60, 20, 80, 40, 1, 45, 60, 78, 35, 75, 65, 0, 1, 104, 2, 4, 1, 0, 0, 10, 45, 30, 80, 100, 0, 0, 50, 0, 65, 60, 10, 0, 10, 0},
        {55, 45, 32, 22, 30, 25, 45, 25, 50, 3, 60, 75, 52, 65, 45, 40, 0, 1, 90, 2, 6, 0, 1, 0, 30, 25, 25, 45, 100, 0, 0, 50, 0, 65, 60, 10, 0, 15, 0},
        {55, 62, 50, 48, 50, 45, 30, 55, 55, 2, 60, 75, 62, 50, 60, 55, 0, 1, 96, 2, 4, 1, 0, 0, 20, 40, 30, 60, 100, 0, 0, 50, 0, 65, 60, 10, 0, 15, 0}}, {0}, 2, {0}, {0}},
    /* Fast and Slow (Zach's, 0.7): two brothers who try to walk across the world, lose each other and find each
     * other again; happy and sad. A waltz in G, slower, the kit's oom-pah-pah (Structure up). Together (G Em Cmaj7 G,
     * Em Cmaj7 Cmaj7 G: where it ends too), Dream (D6, Cmaj7), Flower (Fmaj7, Cmaj7: brighter, G with its F), The
     * Dark (Em, Cmaj7). Its one-shots are the brothers' themes as Zach played them (0.7: from his MIDI): Fast (a flute,
     * leaping) and Slow (a cello, stepping down; 0.7.3: an octave up, to stand out), sixteen bars each; the third is the act's own ('#' between the
     * acts): Together (both), Dream and Flower (Zach's melodies over those chords), Searching (Slow, Fast four bars
     * behind). */
    {"Fast and Slow", {"Together", "Dream", "Flower", "The Dark"}, {
        {55, 45, 52, 48, 35, 30, 30, 45, 40, 2, 60, 78, 55, 45, 45, 40, 0, 1, 95, 7, 0, 0, 2, 0, 10, 30, 0, 35, 10, 0, 1, 50, 2, 62, 50, 25, 0, 80, 2},
        {62, 40, 40, 25, 30, 35, 40, 25, 48, 3, 70, 88, 66, 62, 56, 45, 0, 1, 95, 7, 0, 0, 1, 0, 15, 25, 0, 25, 10, 0, 1, 45, 2, 60, 50, 25, 0, 75, 2},
        {55, 60, 50, 50, 45, 40, 25, 50, 40, 2, 60, 78, 72, 40, 60, 50, 0, 1, 95, 7, 4, 0, 2, 0, 10, 30, 0, 40, 10, 0, 1, 58, 2, 62, 55, 25, 0, 80, 2},
        {60, 30, 58, 35, 25, 30, 45, 30, 45, 3, 68, 86, 35, 55, 35, 50, 0, 1, 95, 7, 0, 0, 2, 0, 20, 20, 0, 25, 10, 15, 1, 45, 2, 58, 45, 25, 0, 75, 2}},
        {"G:2 Em:2 Cmaj7:2 G:2 Em:2 Cmaj7:2 Cmaj7:2 G:2", "D6:4 Cmaj7:4", "Fmaj7:4 Cmaj7:4", "Em:4 Cmaj7:4"},
        7, {"@2;G4:1 B4:1 D5:1 G5:1 G5:1 G5:1 G5:1.5 E5:.5 D5:1 D5:2 D5:1 E5:1 E5:1 E5:1 E5:1 D5:1 B4:1 E5:1 D5:.5 B4:1.5 B4:2 A4:1 G4:1.5 B4:.5 D5:1 G5:1 E5:1 D5:1 G5:1 E5:1 D5:1 E5:1 D5:.5 B4:1.5 D5:1 C5:1 B4:1 D5:.5 C5:.5 B4:2 D5:1 C5:1 G4:4", "@6;B5:3 A5:2 G5:1 E5:1 G5:1 A5:1 B5:3 E5:1 G5:1 A5:1 B5:1.5 A5:.5 G5:1 B5:6 B5:1 A5:1 G5:1 B5:2 B5:1 B5:1.5 A5:.5 G5:1 G5:3 E5:1 G5:1 A5:1 B5:1 A5:1 G5:1 G5:6",
            "@2;G4:1 B4:1 D5:1 G5:1 G5:1 G5:1 G5:1.5 E5:.5 D5:1 D5:2 D5:1 E5:1 E5:1 E5:1 E5:1 D5:1 B4:1 E5:1 D5:.5 B4:1.5 B4:2 A4:1 G4:1.5 B4:.5 D5:1 G5:1 E5:1 D5:1 G5:1 E5:1 D5:1 E5:1 D5:.5 B4:1.5 D5:1 C5:1 B4:1 D5:.5 C5:.5 B4:2 D5:1 C5:1 G4:4|@6;B5:3 A5:2 G5:1 E5:1 G5:1 A5:1 B5:3 E5:1 G5:1 A5:1 B5:1.5 A5:.5 G5:1 B5:6 B5:1 A5:1 G5:1 B5:2 B5:1 B5:1.5 A5:.5 G5:1 G5:3 E5:1 G5:1 A5:1 B5:1 A5:1 G5:1 G5:6#@6;r:2 D5:.5 D5:.5 D5:1 B4:1 A4:1 A4:3 r:1 D5:1 B4:1 B4:3 B4:1 G4:1 E4:1 r:1 B4:1 B4:1 B4:1.5 A4:.5 G4:1#@2;r:.5 E5:2 F5:.5 E5:.5 F5:.5 E5:.5 C5:.5 A4:1 A4:3 r:3 r:1 B4:2 C5:.5 B4:.5 C5:.5 B4:.5 G4:.5 E4:.5 E4:3 r:3 F4:1 A4:1 C5:1 E5:1 F5:1 E5:.5 F5:.5 E5:.5 C5:.5 A4:2 r:3 G4:1 B4:2 C5:.5 B4:2 C5:.5 B4:.5 G4:.5 E4:1 E4:1 E4:3#@6;B5:3 A5:2 G5:1 E5:1 G5:1 A5:1 B5:3 E5:1 G5:1 A5:1 B5:1.5 A5:.5 G5:1 B5:6 B5:1 A5:1 G5:1 B5:2 B5:1 B5:1.5 A5:.5 G5:1 G5:3 E5:1 G5:1 A5:1 B5:1 A5:1 G5:1 G5:6|@2;r:12 G4:1 B4:1 D5:1 G5:1 G5:1 G5:1 G5:1.5 E5:.5 D5:1 D5:2 D5:1 E5:1 E5:1 E5:1 E5:1 D5:1 B4:1 E5:1 D5:.5 B4:1.5 B4:2 A4:1 G4:1.5 B4:.5 D5:1 G5:1 E5:1 D5:1 G5:1 E5:1 D5:1 E5:1 D5:.5 B4:1.5 D5:1 C5:1 B4:1 D5:.5 C5:.5 B4:2 D5:1 C5:1 G4:4"},
        {"Fast", "Slow", "Together#Dream#Flower#Searching"}},
    /* Inside (The Story of the Inside): a waltz in G. A magician who makes the inside, his apprentice, her years
     * outside (G with a B flat everywhere: melodic minor; a slide down by half steps, Tension for the storm), her
     * return, and the magic chord (G, D, E, F#: no third) */
    {"Inside", {"Magician", "Apprentice", "Outside", "Return"}, {
        {50, 60, 45, 35, 45, 30, 40, 40, 35, 2, 55, 70, 55, 40, 35, 45, 0, 1, 96, 7, 0, 0, 0, 0, 10, 30, 0, 40, 10, 0, 1, 50, 1, 60, 55, 10, 1, 60, 0},
        {55, 45, 40, 25, 35, 30, 40, 30, 45, 3, 65, 80, 40, 55, 30, 45, 0, 1, 92, 7, 0, 0, 1, 0, 15, 25, 0, 35, 15, 0, 1, 50, 1, 60, 55, 10, 1, 60, 0},
        {50, 45, 55, 40, 30, 25, 30, 45, 55, 2, 62, 85, 35, 40, 70, 40, 0, 1, 96, 7, 8, 0, 1, -1, 20, 20, 0, 70, 5, 15, 1, 50, 1, 60, 55, 10, 1, 72, 0},
        {55, 65, 45, 35, 50, 35, 40, 40, 40, 2, 60, 75, 60, 45, 40, 50, 0, 1, 96, 7, 0, 0, 0, 0, 10, 30, 0, 40, 10, 0, 1, 50, 1, 60, 55, 10, 1, 60, 0}},
        {"G:2 Bm/F#:2 Em:2 D:2", "G:2 Em:2 D:2 Em:2", "Gm:2 F#:2 F:2 Em:2 F:2 F#:2",
         "G:2 Bm/F#:2 Em:2 D:2 G:2 Em:2 D:2 Gmagic:4"},
        7, {"G6", "G2 G3 D4 E4 F#4 G4"}, {"High G", "Magic"}},
    /* Fly: a fly who helps the spider undo him. A slow, spooky waltz in A minor (with E minor, and an E7 bending the
     * scale), plucked bells, a slide guitar's lead. Entry: classy and spare. Approach: fuller, the voices in.
     * Devour: heavier, the backbeat, Tension. Good-bye: no drums, the bass and the pad heavy, an ocean rushing in */
    {"Fly", {"Entry", "Approach", "Devour", "Good-bye"}, {
        {50, 50, 48, 40, 25, 20, 30, 35, 45, 2, 62, 82, 45, 45, 35, 30, 0, 1, 80, 9, 5, 0, 0, 0, 10, 25, 0, 30, 10, 0, 1, 50, 7, 60, 45, 55, 0, 90, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {50, 55, 50, 45, 45, 40, 30, 40, 50, 2, 65, 85, 45, 45, 40, 50, 0, 1, 84, 9, 5, 0, 2, 0, 20, 30, 0, 40, 15, 12, 1, 50, 7, 65, 50, 60, 0, 85, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {60, 55, 65, 60, 50, 50, 25, 60, 45, 2, 60, 80, 50, 35, 55, 70, 0, 1, 88, 9, 5, 0, 2, -1, 15, 40, 0, 60, 15, 32, 1, 50, 7, 70, 60, 50, 0, 80, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {75, 20, 70, 0, 15, 20, 70, 0, 35, 4, 72, 92, 20, 85, 20, 40, 0, 1, 60, 9, 5, -1, 1, -1, 40, 20, 0, 0, 30, 40, 1, 50, 7, 50, 35, 70, 0, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 70}},
        {"Am:2 C:2 Am:2 Em:2", "Am:2 Em:2 F:2 E7:2", "Am:2 F:2 Dm:2 E7:2", "Am:4 Em:4 Fmaj7:4 Em:4"},
        9, {"~A3", "A2 E3 A3 C4 B3 A3"}, {"Buzz", "The web"}},
    /* Intermission: a head-nodding boom-bap groove in C minor (Dorian), 90 bpm, swung; the backbeat kit, Structure
     * up (the beat locked in), the kick on the off-beats, ghost snares and fills; a Rhodes, vibes, a bass leaning on
     * the fifth. Lights Up, Lobby, Snack Bar (busier), Last Call (calling everyone back) */
    {"Intermission", {"Lights Up", "Lobby", "Snack Bar", "Last Call"}, {
        {45, 30, 55, 55, 30, 45, 30, 55, 35, 1, 40, 55, 45, 40, 50, 35, 0, 1, 90, 0, 1, 0, 0, 0, 10, 45, 40, 60, 30, 0, 0, 50, 4, 60, 45, 15, 0, 85, 1},
        {50, 50, 60, 62, 45, 55, 25, 65, 40, 1, 40, 55, 55, 40, 60, 45, 0, 1, 90, 0, 1, 0, 0, 0, 10, 50, 40, 65, 30, 0, 0, 50, 4, 60, 45, 15, 0, 85, 1},
        {50, 55, 65, 68, 55, 65, 20, 75, 45, 1, 40, 55, 60, 35, 65, 55, 0, 1, 92, 0, 1, 0, 0, 0, 10, 55, 45, 70, 35, 10, 0, 50, 4, 60, 50, 15, 0, 75, 1},
        {55, 45, 45, 40, 30, 50, 35, 40, 50, 2, 50, 65, 45, 50, 50, 40, 0, 1, 88, 0, 1, 0, 0, 0, 15, 40, 35, 50, 30, 0, 0, 50, 4, 55, 45, 15, 0, 85, 1}},
        {"Cm7:2 Fm7:2", "Cm7:2 Fm7:2 Abmaj7:2 G7:2", "Fm7:2 Bb7:2 Ebmaj7:2 Abmaj7:1 G7:1", "Abmaj7:2 G7:2"},
        0, {"C6 G5", "C3 C3 Eb4 G4 Bb4"}, {"Chime", "Curtain"}},
    /* Post Show: Intermission's family, jazzy and bright (0.5: major sevenths, nothing scary): F major, swung; a Rhodes,
     * an organ, strings. Each act its own drums: Bows, Encore and Stage Door Bright (0.7: four on the floor, the claps,
     * the hats between: not Intermission's boom-bap), Walk Home soft. (0.7: Bows plain and glad, F Bb F C, no
     * sevenths; its brushes were too tense) */
    {"Post Show", {"Bows", "Encore", "Stage Door", "Walk Home"}, {
        {50, 45, 55, 55, 40, 50, 25, 45, 35, 1, 40, 55, 60, 35, 60, 40, 0, 1, 90, 5, 0, 0, 0, 0, 10, 45, 30, 50, 30, 0, 0, 55, 4, 60, 50, 15, 0, 85, 4},
        {55, 55, 65, 66, 50, 60, 20, 65, 40, 1, 40, 55, 65, 35, 65, 50, 0, 1, 92, 5, 0, 0, 2, 0, 10, 55, 40, 60, 30, 0, 0, 60, 4, 60, 50, 15, 0, 85, 4},
        {55, 55, 68, 70, 55, 65, 20, 82, 45, 1, 40, 55, 65, 30, 70, 55, 0, 1, 96, 5, 0, 0, 3, 0, 10, 60, 25, 80, 35, 0, 0, 65, 4, 65, 55, 15, 0, 60, 4},
        {55, 45, 50, 40, 35, 50, 30, 35, 50, 2, 50, 65, 50, 45, 55, 40, 0, 1, 88, 5, 0, 0, 0, 0, 15, 40, 45, 40, 30, 0, 0, 50, 4, 55, 45, 15, 0, 90, 0}},
        {"F:2 Bb:2 F:2 C:2", "Fmaj7:2 Am7:2 Bbmaj7:2 C7:2", "Bbmaj7:2 Ebmaj7:2 Fmaj7:2 Dbmaj7:1 C7:1",
         "Fmaj7:2 Bbmaj7:2 Gm7:2 C7:2"},
        5, {"F5 A5 C6", "F3 C4 F4 A4 C5"}, {"Fanfare", "Bow"}},
    /* Riebeck: an original campfire-and-stars piece in the spirit of a game Zach loves (none of its music): a banjo
     * picking by the fire in D, the launch, drifting among the stars (no drums, a far wind), the last supernova */
    {"Riebeck", {"Campfire", "Liftoff", "Drifting", "Supernova"}, {
        {45, 60, 40, 20, 45, 30, 35, 25, 40, 2, 60, 75, 50, 45, 50, 40, 0, 1, 84, 2, 0, 0, 0, 0, 15, 35, 15, 40, 15, 0, 0, 50, 2, 60, 55, 10, 0, 70, 2},
        {55, 60, 50, 45, 50, 40, 25, 45, 45, 2, 62, 80, 60, 40, 55, 50, 0, 1, 88, 2, 0, 0, 2, 0, 15, 40, 15, 55, 15, 5, 0, 55, 2, 60, 55, 10, 0, 75, 2},
        {65, 40, 40, 0, 25, 35, 55, 0, 55, 4, 72, 92, 35, 75, 40, 50, 0, 1, 70, 2, 0, 1, 2, -1, 30, 25, 0, 0, 30, 10, 0, 50, 2, 60, 55, 10, 0, 50, 0},
        {65, 55, 60, 55, 50, 55, 25, 60, 50, 2, 70, 90, 60, 40, 60, 75, 0, 1, 88, 2, 0, 0, 3, 0, 15, 45, 15, 65, 15, 30, 0, 60, 2, 65, 60, 10, 0, 70, 1,
         0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {"D:2 G:2 Bm:2 A:2", "G:2 D:2 Em:2 A:2", "Bm:4 G:4 D:4 A:4", "G:2 A:2 F#m:2 Bm:2 G:2 A:2 D:4"},
        2, {"D5 F#5 A5", "D3 A3 D4 F#4 A4"}, {"Spark", "The loop"}},
};

/* the progressions and stings, parsed once. A step: a degree (prog_deg >= 0), or a named chord: its root and bass
 * as semitones above the story's key (bass -1: the root), its tones as intervals */
static int8_t prog_deg[EN_NPIECES][PROG_MAX], prog_off[EN_NPIECES][PROG_MAX], prog_bass[EN_NPIECES][PROG_MAX];
static uint8_t prog_q[EN_NPIECES][PROG_MAX], prog_bars[EN_NPIECES][PROG_MAX], prog_n[EN_NPIECES], prog_parsed;
static uint8_t sting_note[EN_NSTORY][EN_NSTING][STING_MAX], sting_n[EN_NSTORY][EN_NSTING];
static uint8_t sting_buzz[EN_NSTORY][EN_NSTING];
static uint8_t sting_theme[EN_NSTORY][EN_NSTING];   /* (0.7) "@..." a theme: a melody in time (theme_fire) */
static void buzz_start(int note);
/* the qualities: a suffix and up to four intervals (0xFF: none) */
#define NQUAL 13
static const char *const QUAL_NAME[NQUAL] = {"magic", "maj7", "sus2", "sus4", "add9", "dim", "aug", "m7", "m6", "m", "7",
                                             "6", ""};
static const uint8_t QUAL_IV[NQUAL][4] = {{0, 7, 9, 11}, {0, 4, 7, 11}, {0, 2, 7, 0xFF}, {0, 5, 7, 0xFF}, {0, 4, 7, 2},
                                          {0, 3, 6, 0xFF}, {0, 4, 8, 0xFF}, {0, 3, 7, 10}, {0, 3, 7, 9}, {0, 3, 7, 0xFF},
                                          {0, 4, 7, 10}, {0, 4, 7, 9}, {0, 4, 7, 0xFF}};

static int note_pc(const char **tp)             /* "F#", "Bb", "C": its pitch class, -1 none */
{
    static const int8_t L[7] = {9, 11, 0, 2, 4, 5, 7};   /* A .. G */
    const char *t = *tp;
    int pc;
    if (*t < 'A' || *t > 'G')
        return -1;
    pc = L[*t++ - 'A'];
    if (*t == '#')
        pc++, t++;
    else if (*t == 'b')
        pc += 11, t++;
    *tp = t;
    return pc % 12;
}

static int starts(const char *t, const char *w)
{
    while (*w)
        if (*t++ != *w++)
            return 0;
    return 1;
}

static void prog_parse(void)
{
    int i, s, k;
    for (i = 0; i < EN_NPIECES; i++) {
        const char *t = STORIES[i / EN_NACT].prog[i % EN_NACT];
        int key = STORIES[i / EN_NACT].key, n = 0;
        while (t && *t && n < PROG_MAX) {
            int b = 0, q;
            while (*t == ' ')
                t++;
            if (!*t)
                break;
            prog_bass[i][n] = -1;
            if (*t >= '0' && *t <= '9') {        /* a degree */
                int d = 0;
                while (*t >= '0' && *t <= '9')
                    d = d * 10 + (*t++ - '0');
                prog_deg[i][n] = (int8_t)(d < 1 ? 0 : d - 1);
            } else {                             /* a name */
                int pc = note_pc(&t);
                if (pc < 0)
                    break;
                prog_deg[i][n] = -1;
                prog_off[i][n] = (int8_t)((pc - key + 12) % 12);
                for (q = 0; q < NQUAL - 1 && !starts(t, QUAL_NAME[q]); q++)
                    ;
                prog_q[i][n] = (uint8_t)q;
                while (*t && *t != '/' && *t != ':' && *t != ' ')   /* past the suffix */
                    t++;
                if (*t == '/') {
                    int bp;
                    t++;
                    bp = note_pc(&t);
                    if (bp >= 0)
                        prog_bass[i][n] = (int8_t)((bp - key + 12) % 12);
                }
            }
            while (*t && *t != ':' && *t != ' ')
                t++;
            if (*t == ':')
                for (t++; *t >= '0' && *t <= '9';)
                    b = b * 10 + (*t++ - '0');
            prog_bars[i][n] = (uint8_t)(b < 1 ? 1 : b > 16 ? 16 : b);
            n++;
            while (*t && *t != ' ')
                t++;
        }
        prog_n[i] = (uint8_t)n;
    }
    for (s = 0; s < EN_NSTORY; s++)
        for (k = 0; k < EN_NSTING; k++) {
            const char *t = STORIES[s].sting[k];
            int n = 0;
            sting_theme[s][k] = t && *t == '@';
            if (sting_theme[s][k]) {             /* (a theme: read when it is played) */
                sting_n[s][k] = 1;
                continue;
            }
            sting_buzz[s][k] = t && *t == '~';   /* StoryBox: "~E4" a fly's buzz, about that note */
            if (sting_buzz[s][k])
                t++;
            while (t && *t && n < STING_MAX) {
                int pc, o = 0;
                while (*t == ' ')
                    t++;
                if ((pc = note_pc(&t)) < 0)
                    break;
                while (*t >= '0' && *t <= '9')
                    o = o * 10 + (*t++ - '0');
                sting_note[s][k][n++] = (uint8_t)(12 * (o + 1) + pc);
                while (*t && *t != ' ')
                    t++;
            }
            sting_n[s][k] = (uint8_t)n;
        }
    prog_parsed = 1;
}
int en_prog_len(int piece)
{
    if (!prog_parsed)
        prog_parse();
    return (unsigned)piece < EN_NPIECES ? prog_n[piece] : 0;
}
int en_story_key(int s) { return STORIES[(unsigned)s < EN_NSTORY ? s : 0].key; }
int en_sting_len(int piece, int k)
{
    if (!prog_parsed)
        prog_parse();
    return (unsigned)piece < EN_NPIECES && (unsigned)k < EN_NSTING ? sting_n[piece / EN_NACT][k] : 0;
}
static const char *act_part(const char *t, int a);
const char *en_sting_name(int piece, int k)     /* ("a#b#c#d": one per act) */
{
    static char nm[2][16];
    static int w;
    const char *n = (unsigned)piece < EN_NPIECES && (unsigned)k < EN_NSTING ? STORIES[piece / EN_NACT].sting_name[k] : 0;
    int i = 0;
    if (!n)
        return "Sting";
    n = act_part(n, piece % EN_NACT);
    w ^= 1;
    while (n[i] && n[i] != '#' && i < 15) {
        nm[w][i] = n[i];
        i++;
    }
    nm[w][i] = 0;
    return nm[w];
}

const char *en_story_name(int s) { return STORIES[(unsigned)s < EN_NSTORY ? s : 0].name; }
const char *en_prog_text(int i)                 /* (0.8.1, the Perform page) an act's progression as written; its key */
{
    i = (unsigned)i < EN_NPIECES ? i : 0;
    return STORIES[i / EN_NACT].prog[i % EN_NACT];
}
const char *en_act_name(int i)
{
    i = (unsigned)i < EN_NPIECES ? i : 0;
    return STORIES[i / EN_NACT].act[i % EN_NACT];
}
const char *en_piece_name(int i) { return en_story_name(((unsigned)i < EN_NPIECES ? i : 0) / EN_NACT); }
void en_piece(int i, int16_t *par, uint16_t *pool)
{
    int p;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    /* StoryBox: each story's melody instrument (P_MSOUND), for every act */
    static const uint8_t STORY_MEL[EN_NSTORY][EN_NACT] = {
        {MEL_VIBES, MEL_VIBES, MEL_VIBES, MEL_VIBES},          {MEL_MUSICBOX, MEL_MUSICBOX, MEL_MUSICBOX, MEL_MUSICBOX},
        {MEL_GLASS, MEL_GLASS, MEL_GLASS, MEL_GLASS},          {MEL_MARIMBA, MEL_MARIMBA, MEL_MARIMBA, MEL_MARIMBA},
        {MEL_MUSICBOX, MEL_GLASS, MEL_VIBES, MEL_GLASS},       {MEL_BELLS, MEL_BELLS, MEL_BELLS, MEL_BELLS},
        {MEL_MUSICBOX, MEL_SLIDE, MEL_SLIDE, MEL_GLASS},       /* Fly: the slide running through Approach, Devour */
        {MEL_VIBES, MEL_VIBES, MEL_VIBES, MEL_VIBES},          {MEL_BELLS, MEL_BELLS, MEL_KALIMBA, MEL_BELLS},
        {MEL_BANJO, MEL_BANJO, MEL_GLASS, MEL_BANJO},          /* Riebeck: the banjo by the fire, glass among stars */
    };
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = STORIES[i / EN_NACT].v[i % EN_NACT][p];
    for (p = P_HVSHIM; p < P_NPARAMS; p++)       /* (0.8's: not in the recipes, their defaults) */
        par[p] = PARAMS[p].def;
    par[P_MSOUND] = STORY_MEL[i / EN_NACT][i % EN_NACT];
    {   /* each story's bass sound (0.5: the bass octave's place): Round, Upright, Synth, Sub */
        static const uint8_t SBASS[EN_NSTORY] = {1, 3, 0, 1, 0, 0, 1, 1, 1, 0};   /* (Fast and Slow, 4: Round) */
        par[P_BSOUND] = SBASS[i / EN_NACT];
    }
    if (i / EN_NACT == 2) {                      /* Lighthouse: the sea, under every act (the storm's the loudest) */
        static const int8_t SEA[EN_NACT] = {45, 30, 55, 25};
        par[P_SURF] = SEA[i % EN_NACT];
    }
    if (i == 9 * EN_NACT + 2)                    /* Riebeck's Drifting: a far wind */
        par[P_SURF] = 18;
    {   /* each story's instrument effects: Shimmer, the melody's Echo, Drive, Punch (0.7, Crush's place) */
        static const int8_t SFX[EN_NSTORY][4] = {
            {0, 50, 0, 25}, {35, 55, 0, 0}, {25, 55, 0, 0}, {0, 50, 0, 0},
            {20, 50, 0, 20}, {20, 50, 0, 0}, {20, 50, 10, 0}, {0, 45, 15, 40}, {10, 45, 15, 35}, {30, 55, 0, 15},
        };
        par[P_SHIMMER] = SFX[i / EN_NACT][0];
        par[P_MECHO] = SFX[i / EN_NACT][1];
        par[P_DRIVE] = SFX[i / EN_NACT][2];
        par[P_CRUSH] = SFX[i / EN_NACT][3];
    }
    *pool = EN_MODE_POOL[par[P_MODE]];
}

static const char *const ETIME_NAME[6] = {"1/4.", "1/2", "1/2.", "1 bar", "2 bars", "4 bars"};
static const float ETIME_BEATS[6] = {1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f};

static char *itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

static float drift_secs(int v) { return 20.0f * fm_exp2f((float)v * 0.039069f); }   /* 20 s .. 5 min */

void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    if (i == P_ETIME)
        s = ETIME_NAME[(unsigned)v < 6u ? v : 0];
    else if (i == P_MIDI)
        s = ONOFF[v ? 1 : 0];
    else if (i == P_METER)
        s = v ? "3/4" : "4/4";
    else if (i == P_RVLEN)                       /* (0.7: Harmony's Interval) */
        s = v >= 2 ? "Both" : v ? "Octave" : "Fifth";
    else if (i == P_FLTYPE)                      /* (0.8) */
        s = v >= 2 ? "Band" : v ? "Low cut" : "High cut";
    else if (i == P_KIT)
        s = v == 4 ? "Bright" : v == 3 ? "Lounge" : v == 2 ? "Brushes" : v ? "Backbeat" : "Soft";
    else if (i == P_LSOUND)
        s = EN_LEAD_NAME[(unsigned)v < EN_NLEAD ? v : 0];
    else if (i == P_PADTONE)
        s = v == 3 ? "Organ" : v == 2 ? "Strings" : v ? "Voices" : "Rhodes";
    else if (i == P_MSOUND)
        s = EN_MEL_NAME[(unsigned)v < EN_NMEL ? v : 0];
    else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    if (s) {
        int k = 0;
        while (s[k] && k < 9) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i == P_DRIFT) {
        int sec = (int)(drift_secs(v) + 0.5f);
        if (sec < 60) {
            b = itoa_s(sec, b);
            *b++ = 's';
            *b = 0;
        } else {
            b = itoa_s(sec / 60, b);
            *b++ = ':';
            *b++ = (char)('0' + sec % 60 / 10);
            *b++ = (char)('0' + sec % 10);
            *b = 0;
        }
        return;
    }
    if (i == P_SWING) {                          /* as a swing percentage: 50% (straight) .. 75% */
        b = itoa_s(50 + v / 4, b);
        *b++ = '%';
        *b = 0;
        return;
    }
    if (i == P_BSOUND) {
        static const char *const BS[4] = {"Round", "Upright", "Synth", "Sub"};
        const char *q = BS[(unsigned)v < 4 ? v : 0];
        while ((*b++ = *q++))
            ;
        return;
    }
    if ((i == P_TUNE || i == P_BOCT) && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------- the pure parts --- */
uint32_t en_euclid(int k, int n, int rot)
{
    /* Bjorklund's algorithm (as Toussaint's "The Euclidean Algorithm Generates Traditional Musical
     * Rhythms"): k groups "x" and n - k groups ".", the remainders folded onto the heads until at most
     * one remains. Groups are bit strings, LSB first: (bits, length). */
    uint32_t gb[32], m = 0;
    uint8_t gl[32];
    int ng, heads, tails, i, pos = 0;
    if (n <= 0 || n > 32 || k <= 0)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++) {
        gb[i] = i < k ? 1u : 0u;
        gl[i] = 1;
    }
    ng = n;
    heads = k;
    tails = n - k;
    while (tails > 1 && heads > 0) {
        int pairs = heads < tails ? heads : tails;
        for (i = 0; i < pairs; i++) {           /* a tail onto each head */
            gb[i] |= gb[ng - pairs + i] << gl[i];
            gl[i] = (uint8_t)(gl[i] + gl[ng - pairs + i]);
        }
        if (heads > tails) {                    /* the unpaired heads become the new tails */
            tails = heads - pairs;
            heads = pairs;
        } else {
            tails -= pairs;
            heads = pairs;
        }
        ng = heads + tails;
    }
    for (i = 0; i < ng; i++) {
        uint32_t j;
        for (j = 0; j < gl[i]; j++)
            if (gb[i] >> j & 1u)
                m |= 1u << (((pos + (int)j + rot) % n + n) % n);
        pos += gl[i];
    }
    return m;
}

int en_scale(uint32_t pool, int *steps)
{
    int i, m = 0;
    pool |= 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[m++] = i;
    return m;
}

int en_chord_tones(uint32_t pool, int root, int deg, int *pcs)
{
    static const uint8_t OFF[7] = {0, 2, 4, 6, 1, 3, 5};   /* stacked thirds, then what's left */
    int st[12], m = en_scale(pool, st), want, n = 0, i, j;
    want = m >= 6 ? 4 : 3;
    deg = ((deg % m) + m) % m;
    for (i = 0; i < 7 && n < want; i++) {
        int pc = (root + st[(deg + OFF[i]) % m]) % 12, dup = 0;
        for (j = 0; j < n; j++)
            dup |= pcs[j] == pc;
        if (!dup)
            pcs[n++] = pc;
    }
    return n;
}

static int nearest(int from, int pc, int lo, int hi)    /* the note of class pc nearest from, in lo..hi */
{
    int n = from + ((pc - from) % 12 + 12) % 12;          /* at or above */
    if (n - from > 6)
        n -= 12;
    while (n < lo)
        n += 12;
    while (n > hi)
        n -= 12;
    return n;
}

void en_voice_lead(const int *from, const int *pcs, int nt, int *to, int lo, int hi)
{
    static const uint8_t PERM[24][4] = {
        {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
        {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
        {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
        {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
    int t[4], p, v, best = 1 << 30;
    for (v = 0; v < 4; v++)
        t[v] = pcs[v < nt ? v : 0];                        /* three tones: the root doubled */
    for (p = 0; p < 24; p++) {
        int c = 0, n[4], a, b;
        for (v = 0; v < 4; v++) {
            n[v] = nearest(from[v], t[PERM[p][v]], lo, hi);
            c += n[v] > from[v] ? n[v] - from[v] : from[v] - n[v];
        }
        for (a = 0; a < 4; a++)                            /* no two voices on one note */
            for (b = a + 1; b < 4; b++)
                if (n[a] == n[b])
                    c += 40;
        if (c < best) {
            best = c;
            for (v = 0; v < 4; v++)
                to[v] = n[v];
        }
    }
}

const char *en_chord_name(const int *pcs, int n, char *buf)
{
    uint32_t iv = 0;
    int i, k = 0, third, fifth;
    const char *q = "", *r = EN_NOTE_NAME[pcs[0] % 12];
    for (i = 1; i < n; i++)
        iv |= 1u << ((pcs[i] - pcs[0] + 12) % 12);
    third = iv >> 4 & 1u ? 4 : iv >> 3 & 1u ? 3 : 0;
    fifth = iv >> 7 & 1u ? 7 : iv >> 6 & 1u ? 6 : iv >> 8 & 1u ? 8 : 0;
    if (third == 4) {
        q = fifth == 8 ? "aug" : iv >> 11 & 1u ? "M7" : iv >> 10 & 1u ? "7" : iv >> 9 & 1u ? "6"
            : iv >> 2 & 1u ? "add9" : "";
    } else if (third == 3) {
        if (fifth == 6)
            q = iv >> 10 & 1u ? "m7b5" : iv >> 9 & 1u ? "dim7" : "dim";
        else
            q = iv >> 10 & 1u ? "m7" : iv >> 11 & 1u ? "mM7" : iv >> 9 & 1u ? "m6" : iv >> 2 & 1u ? "madd9" : "m";
    } else {
        q = iv >> 5 & 1u ? (iv >> 10 & 1u ? "7sus4" : "sus4") : iv >> 2 & 1u ? "sus2" : "5";
    }
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_PROG, C_STING, C_MUTE, C_BREAK,
       C_LON, C_LOFF, C_FX, C_ANS };
#define QN 256u                                   /* (0.5: from 64: a story's change posts some 50 values at once) */
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_pool(uint32_t pool) { post(C_POOL, 0, (int)(pool & 0xFFFu)); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_mute(uint32_t mask) { post(C_MUTE, 0, (int)(mask & 15u)); }
void en_fx(uint32_t mask) { post(C_FX, 0, (int)(mask & 15u)); }
void en_lead_on(int note, int vel) { post(C_LON, note & 127, vel & 127); }
void en_lead_off(int note) { post(C_LOFF, note & 127, 0); }
void en_answer(int amount) { post(C_ANS, 0, amount < 0 ? 0 : amount > 100 ? 100 : amount); }
const char *const EN_MEL_NAME[EN_NMEL] = {"Bells", "Kalimba", "Marimba", "Vibes", "Slide", "Glass", "Music box", "Banjo"};
const char *const EN_LEAD_NAME[EN_NLEAD] = {"Kalimba", "Bell", "Flute", "Guitar", "Rhodes", "Violin", "Cello", "Slide"};
void en_breakdown(int on) { post(C_BREAK, on ? 1 : 0, 0); }
void en_sting(int piece, int k)
{
    if (!prog_parsed)
        prog_parse();
    post(C_STING, (unsigned)(k & 7) < EN_NSTING ? k : 0, piece);   /* (k + 16: stop it, 0.8.2) */   /* (k + 8: join the loop now, 0.8.1) */
}
void en_prog(int piece)
{
    if (!prog_parsed)
        prog_parse();
    post(C_PROG, 0, piece);
}

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen, en_ext;
volatile uint8_t en_chord[EN_NPAD], en_chord_root, en_chord_n;
volatile float en_drift, en_pad_level, en_bass_level;
volatile uint8_t en_loop_pos[EN_NLOOP];
volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];
volatile float en_loop_glow[EN_NLOOP];
volatile uint8_t en_drum_pos[EN_NDRUM];
volatile uint32_t en_drum_pat[EN_NDRUM];
volatile float en_drum_glow[EN_NDRUM];
volatile uint32_t en_bars;
volatile float en_arc;
volatile uint8_t en_pulse_rest;
volatile uint8_t en_pad_note[EN_NPAD];
volatile float en_pad_amp[EN_NPAD];

static int16_t par[P_NPARAMS];
static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale */
static float energy;                            /* Energy, -1 (calm) .. 0 (as the act has it) .. +1 (full) */
static float lead_vol, lead_tone = 0.5f, lead_glide_t;   /* the lead's level, tone, glide (s; 0 none) */
static float lv_rs = 0.3f, lv_es = 0.33f;     /* (0.8) a lead voice's reverb and echo sends: the player's lead's;
                                                 * the one-shots', their Space and Echo while they render */
static void lead_note_on(int key, int vel);
static void lead_note_off(int key);
static void call_on(int key, int vel);
static void theme_fire(const char *t, int mv);
static int theme_busy(void);
static uint8_t th_join;                          /* (0.8.1) a theme: 0 waits for the loop's start (the next bar
                                                  * without a progression); 1 joins now, where the loop is */
static void theme_stop(void);
static void theme_bar(void);
static void call_off(int key);
static void ans_stop(void);
static void ans_beat(void);
static float ans_amt;                            /* the answer: 0 off .. 1 free (StoryBox 0.6) */
#define NLEAD 3
#define KS_N 1024                               /* the guitar's string: up to 1024 samples (down to 43 Hz) */
typedef struct {
    float cph, cph2, mph, mph2, inc, inc_t, amp, amp2, att, idx, idx2, rel;   /* 2: the twin, or the piano's tine */
    float blp, bhp, chiff, vt;                  /* the flute's breath filter, its chiff, the time since the note began (s) */
    float ksl, kslp;                            /* the guitar: its string's length (samples), the loop's low-pass state */
    int ksw;                                    /* and where it writes */
    uint8_t on, held, key, note;                /* key: the note asked for (its note off), note: the one played */
} lead_t;
static lead_t lead[NLEAD];
/* StoryBox performance: the layers' mutes, each layer's gain gliding to 0 or 1 (mg: pad, bells, bass, pulse); the
 * breakdown (bd_req: asked for, from the next bar; bd_ret: 1 asked to come back, 2 this bar is the fill) */
static uint32_t mute_mask, fx_mask;
static int rb_held, rb_rec, rb_rec_n, rb_regrab;   /* the punch-ins' ring held; rb_rec: recording again while the old
                                                    * moment plays (a refresh); rb_regrab: the new one ready, at the seam */
static float mg[4] = {1.0f, 1.0f, 1.0f, 1.0f};
static uint8_t bd, bd_req, bd_ret;
volatile uint8_t en_bd;
/* StoryBox: a named chord in play (chord_x): its tones and its bass (-1: the root), absolute pitch classes */
static int chord_x, x_pcs[4], x_n, x_bass = -1;
static float tension;                           /* Tension, 0..1 */
static int deg;                                 /* the chord's scale degree */
static int chord_pcs[4], chord_nt;
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u;

static uint32_t rnd(void)                       /* Marsaglia's xorshift (Xorshift RNGs, 2003): shifts 13, 17, 5 */
{
    uint32_t x = rng;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    rng = x ^ (x << 5);
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (par[P_MIDI])
        en_midi_out(st, d1, d2);
}

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)              /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return sint[i] + (sint[i + 1] - sint[i]) * f;
}

/* ------------------------------------------------- slow random motion --- */
#define NMOD 9                                  /* 0..5 the pads', 6..8 the echo's */
#define NMOD_PAD 6
static struct {
    float v, tgt, rate;
    uint32_t left;                              /* samples until a new target */
} mod[NMOD];

static void mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD; i++) {
        if (mod[i].left <= n) {
            float secs = 7.0f + 33.0f * frand();
            mod[i].tgt = frand() * 2.0f - 1.0f;
            mod[i].left = (uint32_t)(secs * EN_SR);
            mod[i].rate = (float)n / (secs * EN_SR * 0.5f);
        } else {
            mod[i].left -= n;
        }
        mod[i].v += (mod[i].tgt - mod[i].v) * fm_minf(mod[i].rate, 1.0f);
    }
}
static float motion;                            /* 0..1: Motion */

/* ---------------------------------------------------------------- pad --- */
/* Breathing voices, as the sung tape loops of Music for Airports' "2/1": each voice swells in, hangs
 * and fades on its own cycle (unequal lengths, set by Swell), taking its note of the chord as it
 * starts, so the chord forms and dissolves rather than sitting there. The timbre: two saws a few
 * cents apart through two formant band-passes, a vowel that Bright moves from "oo" through "ah" to
 * "eh", with a little of the first formant's low-pass for body. */
typedef struct {
    float ph[2], inc[2];
    float note, target;                         /* MIDI: the note of this breath, the next one's */
    float t, rate, peak, amp;                   /* t 0..1 through the breath, per 32 samples */
    float ic[4];                                /* the two filters' states */
    float c1[4], c2[4];                         /* their coefficients: a1, a2, a3, k */
    float vib, pan;
    float hf;                                   /* a handoff's fade: 1 normal */
    uint8_t midi_note, hand;                    /* hand: 1 fading out of the old key, 2 into the new */
    uint8_t voices;                             /* this breath's colour: 0 Rhodes, 1 Voices */
} pad_t;
static pad_t pad[EN_NPAD];
static float pad_gate, pad_gain, pad_vol, bright, swell;
static const float PAD_PAN[EN_NPAD] = {-0.55f, -0.18f, 0.18f, 0.55f};
static const float BREATH[EN_NPAD] = {1.0f, 1.21f, 1.43f, 1.67f};    /* unequal cycles, as 2/1's 12..20 s loops */
static const float START_T[EN_NPAD] = {0.0f, 0.9f, 0.82f, 0.95f};    /* the first breaths overlap */
static const float VOWEL[4][2] = {{320.0f, 800.0f}, {480.0f, 900.0f}, {730.0f, 1090.0f}, {530.0f, 1840.0f}};
static float arcm;                              /* the slow arc, -1 sparse .. +1 full, times Motion */

static float breath(float t)                    /* rise, hang, fall, a short rest */
{
    if (t < 0.4f)
        return 0.5f - 0.5f * fm_cosf(FM_PI * t * 2.5f);
    if (t < 0.55f)
        return 1.0f;
    if (t < 0.9f)
        return 0.5f + 0.5f * fm_cosf(FM_PI * (t - 0.55f) * (1.0f / 0.35f));
    return 0.0f;
}

static void svf_coefs(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 40.0f, 16000.0f) * (1.0f / EN_SR));
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}

static void midi(uint32_t st, uint32_t d1, uint32_t d2);
static void pad_breathe(pad_t *v, int i)        /* a new breath: its length, its strength, its note */
{
    float secs = (4.0f + 0.12f * swell) * BREATH[i] * (0.85f + 0.3f * frand());
    v->rate = 32.0f / (secs * EN_SR);
    v->peak = 0.6f + 0.4f * frand();
    if (en_playing && v->midi_note)
        midi(0x80, v->midi_note, 0);
    v->note = v->target;
    v->voices = (uint8_t)par[P_PADTONE];
    v->midi_note = (uint8_t)v->target;
    if (en_playing)
        midi(0x90, v->midi_note, (uint32_t)(50.0f + 40.0f * v->peak));
}

/* a new key (piece, root, scale): the voices still singing the old one fade out in about a second and
 * breathe again on the new chord; a voice at rest just takes its new note. Chord drift doesn't do
 * this: there the old notes are meant to hang over */
#define HAND_STEP (32.0f / (1.0f * EN_SR))
static void pad_handoff(void)
{
    int i;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        if (v->amp > 0.002f && ((float)v->midi_note != v->target || v->voices != par[P_PADTONE]))
            v->hand = 1;
        else if (!v->hand)
            v->note = v->target;
    }
}

/* a chord change: a voice singing a note the new chord doesn't have fades out in about a second and
 * breathes in on its new note; a voice on a shared note holds it to the end of its breath */
static void pad_leave(void)
{
    int i, k;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        int pc = v->midi_note % 12, shared = 0;
        for (k = 0; k < chord_nt; k++)
            shared |= chord_pcs[k] == pc;
        if (v->amp > 0.002f && !shared && !v->hand)
            v->hand = 1;
        else if (v->amp <= 0.002f && !v->hand)
            v->note = v->target;
    }
}

static void pad_coefs(pad_t *v, int i)          /* every 32 samples: the breath, pitch, the vowel */
{
    float f, vw, f1, f2;
    int a;
    if (v->hand == 1) {                         /* out of the old key */
        v->hf -= HAND_STEP;
        if (v->hf <= 0.0f) {
            v->hf = 0.0f;
            v->hand = 2;
            v->t = 0.15f;                       /* a breath already rising, on the new chord */
            pad_breathe(v, i);
        }
    } else if (v->hand == 2) {                  /* into the new */
        v->hf += HAND_STEP * 0.6f;
        if (v->hf >= 1.0f) {
            v->hf = 1.0f;
            v->hand = 0;
        }
    }
    v->t += v->rate;
    if (v->t >= 1.0f) {
        v->t -= 1.0f;
        pad_breathe(v, i);
    }
    v->vib += (4.6f + 0.4f * (float)i) * 32.0f / EN_SR;
    if (v->vib >= 1.0f)
        v->vib -= 1.0f;
    f = mtof(v->note + 0.08f * motion * mod[i].v);                          /* a slow drift, as tape */
    v->inc[0] = f * 0.99856f * (1.0f / EN_SR);  /* -2.5 and +2.5 cents */
    v->inc[1] = f * 1.00144f * (1.0f / EN_SR);
    vw = fm_clampf((float)bright * 0.01f + 0.25f * arcm + 0.2f * motion * mod[(i + 2) % NMOD_PAD].v, 0.0f, 1.0f) * 2.999f;
    a = (int)vw;
    vw -= (float)a;
    f1 = VOWEL[a][0] + (VOWEL[a + 1][0] - VOWEL[a][0]) * vw;
    f2 = VOWEL[a][1] + (VOWEL[a + 1][1] - VOWEL[a][1]) * vw;
    svf_coefs(v->c1, f1 * (1.0f + 0.03f * (float)i), 0.45f);
    svf_coefs(v->c2, f2 * (1.0f - 0.02f * (float)i), 0.3f);
}

static inline float blep_saw(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}

/* ------------------------------------------------------------------ bass --- */
static struct {
    float ph, inc, note, target, amp;
    float pluck, lp;                             /* StoryBox: the upright's pluck (1, fading), the synth's filter */
    uint8_t midi_note, rest;                    /* rest: sitting this chord out */
} bass;
static float bass_vol, glide_k = 1.0f / (0.25f * EN_SR);      /* glide_k: 1 / the glide's time in samples */

/* ----------------------------------------------------------------- bells --- */
#define NBELL 6
typedef struct {
    float cph, mph, cinc, minc, amp, adec, idx, idec, att, gl, gr;
    float sl, sv;                                /* StoryBox: the slide's glide (x pitch, toward 1) and time */
    int32_t wait;
    uint8_t on, note, loop, sting;               /* sting: StoryBox's, at its own level (not the bells'), ringing long */
    uint8_t mel;                                 /* StoryBox: its instrument (MEL) */
} bell_t;
static bell_t bell[NBELL];
/* StoryBox: the melody's instruments, all two-operator FM: the modulator's ratio (0: the bells' own, by Bell tone), its
 * index and how fast it fades (s), the ring (x the bells'), a soft attack (0: the bells'), an octave up, a tremolo */
static const struct {
    float ratio, idx, idx_t, dec, atk;
    int8_t oct;
    uint8_t trem;
} MEL[EN_NMEL] = {
    {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0, 0},        /* bells */
    {5.4f, 1.4f, 0.07f, 0.55f, 0.0f, 0, 0},      /* kalimba: a tick, a round body */
    {4.0f, 1.1f, 0.025f, 0.28f, 0.0f, 0, 0},     /* marimba: short and woody */
    {4.0f, 0.22f, 0.05f, 1.7f, 0.0f, 0, 1},      /* vibes: pure and long, a mallet's tick, the motor's shimmer */
    {1.0f, 1.5f, 0.45f, 0.9f, 0.0f, 0, 0},       /* slide: a guitar's, each note sliding from the one before */
    {1.0f, 0.25f, 0.6f, 1.4f, 0.0003f, 0, 0},    /* glass: pure, swelling in */
    {3.5f, 2.2f, 0.6f, 0.55f, 0.0f, 12, 0},      /* music box: high and small */
    {3.0f, 1.0f, 0.035f, 0.2f, 0.0f, 0, 0},      /* banjo: a plunk (the head's quick ring), a short nasal core */
};
static const float MEL_GAIN[EN_NMEL] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f};   /* (the banjo softer) */
static float mel_trem, mel_vib, mel_last[EN_NLOOP + 1];   /* (the slide: each loop's last note's pitch) */
static int bell_sting_now;                       /* (sting_fire: its notes are always bells) */
static float bell_vol, btone;
static int8_t loop_note[EN_NLOOP][EN_LOOP_MAX];
static uint8_t loop_vel[EN_NLOOP][EN_LOOP_MAX], loop_pos[EN_NLOOP];
static const uint8_t LOOP_BASE[EN_NLOOP] = {72, 67, 79, 62};     /* each loop's register */
static const float LOOP_PAN[EN_NLOOP] = {-0.6f, 0.35f, 0.7f, -0.25f};
static const float RATIO[5] = {1.0f, 2.0f, 3.0f, 3.5f, 4.77f};

/* ----------------------------------------------------------------- pulse --- */
typedef struct {
    float ph, inc, inc_end, amp, adec, idx, x1, x2, att, mph[4], pend;
    int32_t wait;                               /* swing: samples until a hit that waits (pend: its velocity) */
    uint8_t on, note;
} drum_t;
static drum_t drum[EN_NDRUM];
static uint8_t drum_pos[EN_NDRUM];
static uint32_t drum_pat[EN_NDRUM];
static uint8_t drum_n[EN_NDRUM] = {16, 12, 7};  /* StoryBox: each pulse's cycle (the thump's is the bar) */
static void snare_fire(float vel);             /* the backbeat kit's snare (below) */
static float pulse_vol, swing, hat_f = 0.9f, hat_open = 0.42f;
static float step_len, pulse_left;               /* a sixteenth, in samples; to the next MIDI clock pulse */
static uint32_t noise = 22222u;
static const uint8_t DRUM_KMAX[EN_NDRUM] = {5, 7, 3};
static const uint8_t DRUM_GM[EN_NDRUM] = {36, 42, 46};          /* kick, closed hat, open hat */
/* the hats' metal: four square waves at the 808's inharmonic ratios (Hz / EN_SR) */
static const float HAT_INC[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};

/* ------------------------------------------------------------------ space --- */
static float echo_send, rev_send, echo_len, echo_len_t, echo_fb = 0.55f;
static float rev_in_gain = 1.0f;                /* falls to 0 while frozen */

/* -------------------------------------------------------------- harmony --- */
static void scale_set(void)
{
    nsteps = en_scale(pool, steps);
}

static int bell_note(int l, int off)            /* a loop slot's note, from the chord's degree */
{
    int d = deg + off, oct = d / nsteps, s = steps[d % nsteps], base, root = par[P_ROOT], n;
    base = LOOP_BASE[l] - ((LOOP_BASE[l] - root) % 12 + 12) % 12;   /* the root at or below the register */
    n = base + s + 12 * oct + 12 * par[P_BOCT];
    while (n > 103)
        n -= 12;
    while (n < 36)
        n += 12;
    if (chord_x) {                              /* a named chord: a scale note a semitone from one of its tones bends to it */
        int pc = n % 12, i, in = 0;
        for (i = 0; i < chord_nt; i++)
            in |= chord_pcs[i] == pc;
        for (i = 0; i < chord_nt && !in; i++)
            if ((chord_pcs[i] - pc + 12) % 12 == 1 || (pc - chord_pcs[i] + 12) % 12 == 1) {
                n += (chord_pcs[i] - pc + 12) % 12 == 1 ? 1 : -1;
                in = 1;
            }
    }
    if (tension > 0.0f && frand() < tension * 0.45f)   /* Tension: a semitone off */
        n += frand() < 0.5f ? 1 : -1;
    return n;
}

static void pad_retarget(void)                  /* the chord's tones to the four voices */
{
    int from[4], to[4], i;
    if (chord_x) {
        for (i = 0; i < x_n; i++)
            chord_pcs[i] = x_pcs[i];
        chord_nt = x_n;
    } else {
        chord_nt = en_chord_tones(pool, par[P_ROOT], deg, chord_pcs);
    }
    if (tension > 0.45f) {                      /* Tension: a flat ninth in the chord, then a tritone too */
        int b9 = (chord_pcs[0] + 1) % 12, tt = (chord_pcs[0] + 6) % 12;
        if (chord_nt < 4)
            chord_pcs[chord_nt++] = b9;
        else
            chord_pcs[3] = b9;
        if (tension > 0.75f)
            chord_pcs[2] = tt;
    }
    for (i = 0; i < EN_NPAD; i++)
        from[i] = (int)(pad[i].target + 0.5f);
    en_voice_lead(from, chord_pcs, chord_nt, to, 48, 77);
    for (i = 0; i < EN_NPAD; i++) {             /* each voice takes its note at its next breath */
        pad[i].target = (float)to[i];
        en_chord[i] = (uint8_t)to[i];
    }
    en_chord_root = (uint8_t)chord_pcs[0];
    en_chord_n = (uint8_t)chord_nt;
}

static int bass_pc(void)                        /* the chord's bass: a slash chord's, else its root; Tension leans on the b2 */
{
    int pc = chord_x && x_bass >= 0 ? x_bass : chord_pcs[0];
    if (tension > 0.6f && frand() < (tension - 0.6f) * 1.5f)
        pc = (pc + 1) % 12;
    return pc;
}

static void bass_to(int pc)
{
    int n = 36 + pc % 12;                        /* C2..B2 (the Sub an octave down, in render_bass) */
    if (n != bass.midi_note)
        bass.pluck = 1.0f;                       /* (the upright: plucked anew) */
    bass.target = (float)n;
    if (en_playing && bass.midi_note != n) {
        midi(0x82, bass.midi_note, 0);
        midi(0x92, (uint32_t)n, 80);
    }
    bass.midi_note = (uint8_t)n;
}

static float drift_left, drift_total, evolve;   /* evolve: Evolve, 0..1 */
static uint8_t change_pending, fifth_done;

static void drift_reset(void)
{
    drift_total = drift_secs(par[P_DRIFT]) * (0.7f + 0.6f * frand()) * EN_SR;
    drift_left = drift_total;
    fifth_done = 0;
}

/* StoryBox: the progression the act follows (prog_cur, -1 none), its step, and the bars left on it */
static int prog_cur = -1, prog_i, prog_left, prog_total = 1;   /* (prog_total: the step's bars, for its progress) */
static float freedom = 1.0f;                    /* Freedom, 0..1 */
volatile int8_t en_prog_step = -1;
static void chord_to(int i);
static int prog_on(void) { return prog_cur >= 0 && prog_n[prog_cur] && freedom < 0.995f; }
static float freedom_eff(void) { return freedom * (1.0f - 0.008f * (float)par[P_GROOVE]); }   /* Structure: as written */

static void chord_named(int off, int q, int bass)   /* a named chord, off semitones above the key (Root) */
{
    int i, r = (par[P_ROOT] + off) % 12, best = 0, bd = 99;
    x_n = 0;
    for (i = 0; i < 4 && QUAL_IV[q][i] != 0xFF; i++)
        x_pcs[x_n++] = (r + QUAL_IV[q][i]) % 12;
    x_bass = bass >= 0 ? (par[P_ROOT] + bass) % 12 : -1;
    for (i = 0; i < nsteps; i++) {              /* the bells' degree: the scale's note nearest the root */
        int d = ((par[P_ROOT] + steps[i]) % 12 - r + 12) % 12;
        d = d > 6 ? 12 - d : d;
        if (d < bd)
            bd = d, best = i;
    }
    chord_x = 1;
    chord_to(best);
}

/* (0.8) a refrain on the bells over a chord of the progression: Fast and Slow's Dream, over D6, D D D B A as Zach
 * played it (a beat and a half in, twice in the chord's four bars), soft and far */
static void bell_voice(int l, int n, float vel, int32_t wait);
/* (0.8.1) where the progression's loop is: sixteenths since its first chord (-1: no progression), and its length in
 * bars; the Perform page shows it, a one-shot joins it */
static int32_t loop16 = -1;
volatile int16_t en_loop_bar = -1, en_loop_bars;
static int16_t rf_t = -1;                        /* sixteenths since it began (-1: none) */
static const struct { uint8_t piece, step, n; int8_t ev[12][2]; } REFRAIN = {
    4 * EN_NACT + 1, 0, 10,
    {{74, 8}, {74, 10}, {74, 12}, {71, 16}, {69, 20}, {74, 32}, {74, 34}, {74, 36}, {71, 40}, {69, 44}},
};
static void refrain_tick(void)                   /* each sixteenth (tick) */
{
    int i;
    if (rf_t < 0)
        return;
    for (i = 0; i < REFRAIN.n; i++)
        if (REFRAIN.ev[i][1] == rf_t)
            bell_voice(0, REFRAIN.ev[i][0] + 12 * par[P_BOCT], i % 5 == 4 ? 0.62f : 0.5f, 0);
    if (++rf_t > 60)
        rf_t = -1;
}

static void prog_advance(void)                  /* the next step, on a bar: as written, or (with Freedom) a variation */
{
    int d, b;
    prog_i = prog_i + 1 >= prog_n[prog_cur] || prog_i < 0 ? 0 : prog_i + 1;
    if (prog_cur == REFRAIN.piece && prog_i == REFRAIN.step && !bd)
        rf_t = 0;                                /* (0.8: the refrain, from this bar) */
    if (prog_i == 0)
        loop16 = 0;                              /* (0.8.1: the loop from its start) */
    d = prog_deg[prog_cur][prog_i];
    b = prog_bars[prog_cur][prog_i];
    en_prog_step = (int8_t)prog_i;
    if (d < 0) {                                /* a named chord: as written, or (with Freedom) the last one held */
        prog_left = b + (!theme_busy() && frand() < freedom_eff() * 0.25f ? (b > 1 && frand() < 0.5f ? -1 : 1) : 0);
        prog_total = prog_left;
        if (!(frand() < freedom_eff() * 0.3f))
            chord_named(prog_off[prog_cur][prog_i], prog_q[prog_cur][prog_i], prog_bass[prog_cur][prog_i]);
        return;
    }
    if (frand() < freedom_eff() * 0.55f) {            /* a variation: a chord a third away, or the last one held */
        float r = frand();
        if (r < 0.45f)
            d += nsteps == 7 ? 5 : nsteps - 2;  /* a third below: the relative */
        else if (r < 0.8f)
            d += 2;                             /* a third above */
        else
            d = deg;                            /* held */
    }
    if (frand() < freedom_eff() * 0.25f)              /* and now and then a bar longer or shorter */
        b += b > 1 && frand() < 0.5f ? -1 : 1;
    prog_left = b;
    prog_total = b;
    d = ((d % nsteps) + nsteps) % nsteps;
    if (d != deg || chord_x) {
        chord_x = 0;
        chord_to(d);
    }
}

static void chord_change(void)                  /* to another degree: the near, strong moves first */
{
    static const float W7[7] = {4.0f, 2.0f, 1.0f, 3.0f, 3.0f, 3.0f, 0.4f};
    float w[12], sum = 0.0f, r;
    int i;
    if (prog_on()) {                            /* following a progression: its next step */
        prog_advance();
        return;
    }
    for (i = 0; i < nsteps; i++) {
        w[i] = i == deg ? 0.0f : nsteps == 7 ? W7[i] : i == 0 ? 3.0f : 1.5f;
        sum += w[i];
    }
    r = frand() * sum;
    for (i = 0; i < nsteps; i++) {
        r -= w[i];
        if (r <= 0.0f && w[i] > 0.0f)
            break;
    }
    if (i >= nsteps)
        i = deg == 0 ? (nsteps > 1 ? 1 : 0) : 0;
    chord_x = 0;
    chord_to(i);
}

static void chord_to(int i)
{
    deg = i;
    pad_retarget();
    pad_leave();
    bass_to(bass_pc());
    /* Evolve: the bass sometimes sits a chord out, more often when the arc is sparse */
    bass.rest = frand() < evolve * 0.3f + 0.25f * fm_maxf(-arcm, 0.0f);
    drift_reset();
}

/* ------------------------------------------------------------- the loops --- */
static int rand_offset(void)                    /* scale steps above the chord's degree */
{
    if (frand() < 0.72f) {                      /* a chord tone, sometimes an octave up */
        int o = 2 * irand(chord_nt);
        return o + (frand() < 0.3f ? nsteps : 0);
    }
    return 1 + 2 * irand(3);                    /* a passing note: the 2nd, 4th or 6th */
}

static int loop_count(int l)
{
    int i, c = 0;
    for (i = 0; i < en_loop_len[l]; i++)
        c += loop_note[l][i] >= 0;
    return c;
}

static int loop_target(void)                    /* 0..5 notes: Density, swelled and thinned by the arc */
{
    float d = (float)par[P_DENSITY] * 0.05f * (1.0f + 0.8f * arcm) * (1.0f + 0.7f * energy);
    return (int)(fm_clampf(d, 0.0f, 5.0f) + frand());
}
static uint8_t loop_rest[EN_NLOOP];             /* this pass: silent (Evolve) */

static void loop_add(int l)
{
    int len = en_loop_len[l], i, s;
    for (i = 0; i < 8; i++) {
        s = irand(len);
        if (loop_note[l][s] < 0) {
            loop_note[l][s] = (int8_t)rand_offset();
            loop_vel[l][s] = (uint8_t)(50 + irand(50));
            return;
        }
    }
}

static int loop_pick(int l)                     /* a random sounding slot, -1 none */
{
    int len = en_loop_len[l], i, s = irand(len);
    for (i = 0; i < len; i++, s = (s + 1) % len)
        if (loop_note[l][s] >= 0)
            return s;
    return -1;
}

static void loop_publish(int l)
{
    int i;
    for (i = 0; i < EN_LOOP_MAX; i++)
        en_loop_note[l][i] = i < en_loop_len[l] ? loop_note[l][i] : -1;
}

static void loop_regrow(int l)
{
    int i, n = loop_target();
    for (i = 0; i < EN_LOOP_MAX; i++)
        loop_note[l][i] = -1;
    for (i = 0; i < n; i++)
        loop_add(l);
    loop_publish(l);
}

static void loop_pass(int l)                    /* the end of a pass: grow, prune, evolve */
{
    int c = loop_count(l), t = loop_target(), s;
    for (; c < t; c++)                          /* Density: to its count by the end of a pass */
        loop_add(l);
    for (; c > t && (s = loop_pick(l)) >= 0; c--)
        loop_note[l][s] = -1;
    loop_rest[l] = frand() < (evolve * 0.15f + 0.15f * fm_maxf(-arcm, 0.0f)) * (1.0f - 0.008f * (float)par[P_GROOVE]);
    if (frand() < evolve * 0.7f * (1.0f - 0.007f * (float)par[P_GROOVE]) && (s = loop_pick(l)) >= 0) {   /* Structure: steadier */
        if (frand() < 0.5f) {
            loop_note[l][s] = (int8_t)rand_offset();             /* another note */
        } else {
            int d = irand(en_loop_len[l]);                       /* the same note, elsewhere */
            if (loop_note[l][d] < 0) {
                loop_note[l][d] = loop_note[l][s];
                loop_vel[l][d] = loop_vel[l][s];
                loop_note[l][s] = -1;
            }
        }
    }
    loop_publish(l);
}

/* each loop its own bell: index, decay and ratio a little apart, so the four can be told apart */
static const float LOOP_IDX[EN_NLOOP] = {1.0f, 0.55f, 1.5f, 0.8f}, LOOP_DEC[EN_NLOOP] = {1.0f, 1.35f, 0.75f, 1.6f};
static const int8_t LOOP_RAT[EN_NLOOP] = {0, 0, 1, -1};

static void bell_voice(int l, int n, float vel, int32_t wait)
{
    int i, best = 0, ri;
    float lo = 9.0f, t60, p;
    bell_t *v;
    for (i = 0; i < NBELL; i++) {               /* a free voice, or the quietest */
        float a = bell[i].on ? bell[i].amp : -1.0f;
        if (a < lo) {
            lo = a;
            best = i;
        }
    }
    v = &bell[best];
    if (v->on)
        midi(0x81, v->note, 0);
    v->on = 1;
    v->note = (uint8_t)n;
    v->loop = (uint8_t)l;
    v->cph = v->mph = 0.0f;
    v->mel = (uint8_t)(bell_sting_now ? 0 : par[P_MSOUND]);
    if (MEL[v->mel].oct && n + MEL[v->mel].oct <= 108)
        n += MEL[v->mel].oct;
    v->cinc = mtof((float)n) * (1.0f / EN_SR);
    v->sl = 1.0f;
    v->sv = 0.0f;
    if (v->mel == MEL_SLIDE) {                   /* from this loop's last note (near enough), or up from below */
        float lp = mel_last[l < EN_NLOOP ? l : EN_NLOOP];
        v->sl = lp > 0.0f ? fm_clampf(lp / v->cinc, 0.75f, 1.33f) : 0.944f;
        mel_last[l < EN_NLOOP ? l : EN_NLOOP] = v->cinc;
    }
    ri = par[P_BTONE] * 5 / 101 + LOOP_RAT[l];
    v->minc = v->cinc * (MEL[v->mel].ratio > 0.0f ? MEL[v->mel].ratio : RATIO[ri < 0 ? 0 : ri > 4 ? 4 : ri]);
    v->amp = vel;
    t60 = 4.2f - (float)(n - 60) * 0.06f;       /* the low bells ring longer */
    t60 = fm_clampf(t60, 1.6f, 5.0f) * LOOP_DEC[l];
    v->adec = fm_expf(-6.9f / (t60 * EN_SR));
    /* Bell tone: 0 a round, nearly pure tone; 100 a long metallic clang (index in radians, as turns) */
    v->idx = (0.15f + 6.5f * btone * btone) * LOOP_IDX[l] * (0.6f + 0.4f * vel) * (1.0f / FM_TWO_PI);
    v->idec = fm_expf(-6.9f / ((0.35f + 2.6f * btone) * EN_SR));
    if (v->mel) {                                /* StoryBox: another instrument */
        v->idx = MEL[v->mel].idx * (0.6f + 0.4f * vel) * (1.0f / FM_TWO_PI);
        v->idec = fm_expf(-6.9f / (MEL[v->mel].idx_t * EN_SR));
        v->adec = fm_expf(-6.9f / (t60 * MEL[v->mel].dec * EN_SR));
    }
    v->att = 0.0f;
    v->sting = 0;
    p = (LOOP_PAN[l] + 1.0f) * (FM_PI / 4.0f);
    v->gl = fm_cosf(p);
    v->gr = fm_sinf(p);
    v->wait = wait;
    en_loop_glow[l] = 1.0f;
    midi(0x91, (uint32_t)n, (uint32_t)(vel * 110.0f));
}

static void bell_ring(int l, int slot)
{
    int n = bell_note(l, loop_note[l][slot]);
    float vel = (float)loop_vel[l][slot] * 0.01f;
    int32_t wait = irand(2600);                 /* a human hand: up to 60 ms late */
    bell_voice(l, n, vel, wait);
    if (n + 12 <= 103 && frand() < 0.08f + 0.12f * fm_maxf(arcm, 0.0f))
        bell_voice(l, n + 12, vel * 0.4f, wait + 300 + irand(2000));   /* now and then, its octave */
}

/* ---------------------------------------------------------------- pulse --- */
static const uint8_t DRUM_ROT[EN_NDRUM] = {0, 2, 3};
static int drum_k[EN_NDRUM], drum_kb[EN_NDRUM], drum_rot[EN_NDRUM], pulse_rest;   /* rest: bars left */

static void drum_publish(int d)
{
    drum_pat[d] = en_euclid(drum_k[d], drum_n[d], drum_rot[d]);
    if (par[P_KIT] == 1 && en_bar_len == 16 && d == 0)   /* backbeat: boom-bap (one, the and of two, three's and), more with Hits */
        drum_pat[0] = drum_k[0] >= 3 ? 0x0485u : 0x0481u;
    else if (par[P_KIT] == 3 && en_bar_len == 16 && d == 0)   /* 0.7, Lounge: the kick soft on one and three's and */
        drum_pat[0] = drum_k[0] >= 3 ? 0x0601u : 0x0401u;
    else if (par[P_KIT] == 3 && en_bar_len == 16 && d == 1)   /* the ride's swing: ding, ding-a ding, ding-a */
        drum_pat[1] = drum_k[1] >= 6 ? 0x5555u : 0x5151u;
    else if (par[P_KIT] == 3 && en_bar_len == 16 && d == 2)   /* (no open hat: the ride is enough) */
        drum_pat[2] = drum_k[2] >= 4 ? 0x4000u : 0u;
    else if (par[P_KIT] == 4 && en_bar_len == 16 && d == 0)   /* 0.7, Bright: four on the floor */
        drum_pat[0] = drum_k[0] >= 5 ? 0x1111u | 1u << 14 : 0x1111u;
    else if (par[P_KIT] == 4 && en_bar_len == 16 && d == 1)   /* the hats on the off-beats, sixteenths when busier */
        drum_pat[1] = drum_k[1] >= 7 ? 0xEEEEu : 0x4444u;
    else if (par[P_KIT] == 4 && en_bar_len == 16 && d == 2)   /* an open hat lifting into each half */
        drum_pat[2] = drum_k[2] >= 2 ? 0x4040u : 0x4000u;
    else if (par[P_KIT] && en_bar_len == 16 && d == 2)   /* and the open hat steps back for the snare */
        drum_pat[2] &= ~0x1010u;
    if (en_bar_len == 12 && d == 0)              /* 3/4: the thump always on the one */
        drum_pat[0] = par[P_KIT] && par[P_GROOVE] >= 60 ? 1u | (drum_k[0] >= 4 ? 1u << 10 : 0u)   /* a kit's waltz: oom */
                                                         : en_euclid(drum_k[0] < 1 ? 1 : drum_k[0], 12, 0) | 1u;
    else if (en_bar_len == 12 && d == 1)         /* and the closed hat on two and three (oom-pah-pah), more with Hits */
        drum_pat[1] = par[P_KIT] && par[P_GROOVE] >= 60 ? 0x110u   /* (a kit's, Structure up: just pah, pah) */
                                                         : (en_euclid(drum_k[1], 12, drum_rot[1]) | 0x110u) & ~1u;
    en_drum_pat[d] = drum_pat[d];
}

static int energy_hits(void)                    /* Hits, through Energy */
{
    int h = par[P_HITS] + (int)(35.0f * energy);
    return par[P_HITS] == 0 ? 0 : h < 0 ? 0 : h > 100 ? 100 : h;   /* a story with no pulse keeps none */
}

static void drums_pattern(void)                 /* Hits: the patterns' home */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++) {
        drum_kb[d] = drum_k[d] = (energy_hits() * DRUM_KMAX[d] + 50) / 100;
        drum_rot[d] = DRUM_ROT[d];
        drum_publish(d);
    }
}

static void drums_evolve(void)                  /* a bar: a pattern turns, or gains or loses a hit */
{
    int d = irand(EN_NDRUM), n = drum_n[d];
    if (!drum_kb[d])
        return;
    if (frand() < 0.5f) {
        drum_rot[d] = (drum_rot[d] + (frand() < 0.5f ? 1 : n - 1)) % n;
    } else {
        int k = drum_k[d] + (frand() < 0.5f ? 1 : -1);
        if (k >= drum_kb[d] - 1 && k <= drum_kb[d] + 1 && k >= 1 && k <= n)
            drum_k[d] = k;
    }
    drum_publish(d);
}

static void drum_fire(int d, float vel)
{
    drum_t *v = &drum[d];
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    switch (d) {
    case 0: {                                   /* thump: the chord's root, low, falling into place */
        float f = mtof((float)(36 + chord_pcs[0]));
        v->note = DRUM_GM[0];
        v->inc = f * (par[P_KIT] == 2 ? 1.4f : 2.2f) * (1.0f / EN_SR);   /* (brushes: a soft low drum, shorter) */
        v->inc_end = f * (1.0f / EN_SR);
        v->adec = fm_expf(-6.9f / ((par[P_KIT] == 2 ? 0.3f : 0.45f) * EN_SR));
        break;
    }
    case 1:                                     /* closed hat: short; it chokes the open one */
        v->note = DRUM_GM[1];
        v->adec = fm_expf(-6.9f / ((par[P_KIT] == 2 ? 0.1f : par[P_KIT] == 3 ? 0.24f : 0.06f) * EN_SR));   /* (brushes: a shaker;
                                                                                          * Lounge: a ride's ring) */
        drum[2].adec = fm_expf(-6.9f / (0.02f * EN_SR));
        break;
    default:                                    /* open hat: a long, soft "tss" */
        v->note = DRUM_GM[2];
        v->adec = fm_expf(-6.9f / ((par[P_KIT] == 2 ? 0.38f : hat_open) * EN_SR));   /* (brushes: a sweep) */
        break;
    }
    en_drum_glow[d] = 1.0f;
    midi(0x99, v->note, (uint32_t)(vel * 100.0f));
    midi(0x89, v->note, 0);
}

/* (0.8.5) the feel: each kit's hits a little off the grid, as players are. Hip-hop and lo-fi lay the snare back
 * behind the beat and let the hats drag (J Dilla's "drunk" time); jazz brushes and the ride breathe; the dance kit stays
 * tight. And every hit a few milliseconds loose (Magenta's GrooVAE, learned from drummers, shows the timing and the
 * loudness of each hit are much of a groove), less so with Structure (Groove). In ms: kick, hat, open hat, snare (late);
 * then the looseness either way */
static const int8_t FEEL_MS[5][5] = {
    {0, 2, 2, 0, 4},                             /* 0, the thump: barely */
    {0, 7, 5, 22, 6},                            /* 1, backbeat (boom-bap, lo-fi): the snare well back, the hats dragging */
    {0, 6, 8, 12, 8},                            /* 2, brushes: loose */
    {0, 4, 4, 14, 5},                            /* 3, Lounge: the ride a hair late, the cross-stick back */
    {0, 0, 0, 3, 2},                             /* 4, Bright: four on the floor, tight */
};
static int32_t feel_wait(int d)                  /* d 0..2 the drums, 3 the snare: samples late */
{
    const int k = par[P_KIT] >= 0 && par[P_KIT] <= 4 ? par[P_KIT] : 0;
    float ms = (float)FEEL_MS[k][d] + (frand() - 0.5f) * 2.0f * (float)FEEL_MS[k][4] * (1.0f - (float)par[P_GROOVE] / 150.0f);
    return ms > 0.0f ? (int32_t)(ms * (EN_SR / 1000.0f)) : 0;
}
static int32_t sn_wait;                          /* (0.8.5) the snare's own wait, and its velocity */
static float sn_pend;
static void snare_fire(float vel);
static void snare_hit(float vel)                 /* the snare, with its feel */
{
    int32_t w = feel_wait(3);
    if (w > 0) {
        sn_wait = w;
        sn_pend = vel;
    } else
        snare_fire(vel);
}

static void drum_hit(int d, float vel, int off)  /* off: an off sixteenth, which Swing delays */
{
    int32_t w = (off ? (int32_t)(swing * step_len) : 0) + (d ? feel_wait(d) : 0);   /* (0.8.5: and the feel; the kick
                                                  * stays on the grid, the anchor) */
    if (w > 0) {
        drum[d].wait = w;
        drum[d].pend = vel;
    } else {
        drum_fire(d, vel);
    }
}

static int32_t drums_wait(void)                 /* samples to the next hit that waits; 0: none */
{
    int32_t m = 0;
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (!m || drum[d].wait < m))
            m = drum[d].wait;
    if (sn_wait > 0 && (!m || sn_wait < m))
        m = sn_wait;
    return m;
}

static void drums_wait_step(int32_t k)          /* k samples went by: hits whose wait is over */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (drum[d].wait -= k) <= 0) {
            drum[d].wait = 0;
            drum_fire(d, drum[d].pend);
        }
    if (sn_wait > 0 && (sn_wait -= k) <= 0) {
        sn_wait = 0;
        snare_fire(sn_pend);
    }
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static uint32_t sixteenth;
static uint8_t bar_pos, meter_next;             /* StoryBox: the sixteenth in the bar; 3/4 asked for (from the next bar) */
static uint8_t fill_bar;                        /* StoryBox: this bar ends its phrase with a fill */
static void loops_for(uint8_t *len)             /* the loops' lengths for Structure and the meter */
{
    static const uint8_t L[3][2][EN_NLOOP] = {
        {{13, 17, 23, 29}, {13, 17, 23, 29}},   /* loose: primes, drifting, never agreeing */
        {{12, 16, 20, 24}, {12, 15, 18, 24}},   /* in between: agreeing every few bars (4/4, 3/4) */
        {{4, 8, 8, 16}, {3, 6, 6, 12}},         /* steady: a bar, two and four, on the downbeat */
    };
    int s = par[P_GROOVE] < 34 ? 0 : par[P_GROOVE] < 67 ? 1 : 2, l;
    for (l = 0; l < EN_NLOOP; l++)
        len[l] = L[s][en_bar_len == 12][l];
}

static void meter_bar(void)                      /* a bar starts: the meter and the Groove asked for take over */
{
    uint8_t len = meter_next ? 12 : 16, steady = par[P_GROOVE] >= 50;
    uint8_t n1 = steady ? len : 12, n2 = steady ? (uint8_t)(len * 2) : 7;   /* steady: the hats a bar and two bars long */
    if (len != en_bar_len || n1 != drum_n[1] || n2 != drum_n[2]) {
        int d;
        en_bar_len = len;
        en_drum_n[0] = drum_n[0] = len;         /* the thump's cycle is the bar */
        en_drum_n[1] = drum_n[1] = n1;
        en_drum_n[2] = drum_n[2] = n2;
        for (d = 0; d < EN_NDRUM; d++)
            drum_pos[d] = 0;
        drums_pattern();
    }
    {                                           /* the bells' loops: new lengths start together, on the one */
        uint8_t want[EN_NLOOP];
        int l, i, chg = 0;
        loops_for(want);
        for (l = 0; l < EN_NLOOP; l++)
            chg |= want[l] != en_loop_len[l];
        if (chg) {
            for (l = 0; l < EN_NLOOP; l++) {
                en_loop_len[l] = want[l];
                for (i = want[l]; i < EN_LOOP_MAX; i++)   /* the notes past the new end: gone */
                    loop_note[l][i] = -1;
                loop_pos[l] = 0;
                if (!loop_count(l))
                    loop_regrow(l);
                loop_publish(l);
            }
        } else if (par[P_GROOVE] >= 67) {       /* steady: the loops kept on the downbeat */
            for (l = 0; l < EN_NLOOP; l++)
                if (loop_pos[l] % (en_bar_len / 4) != 0)
                    loop_pos[l] = 0;
        }
    }
}
static void sting_mark(void)                    /* the voice just struck (the loudest, newest) is a sting's */
{
    int i, b = -1;
    for (i = 0; i < NBELL; i++)
        if (bell[i].on && bell[i].att == 0.0f && bell[i].amp >= 0.94f)
            b = i;
    if (b >= 0) {
        bell[b].sting = 1;
        bell[b].adec = fm_expf(fm_logf(bell[b].adec) * 0.55f);   /* rings nearly twice as long */
    }
}
static int sting_pend = -1;                     /* a sting waiting for the beat: story * EN_NSTING + which */
static int sting_act;                           /* (0.7: its act, for a theme with one per act) */
static const char *act_part(const char *t, int a)   /* "x#y#z": the act's part (the first when it has none) */
{
    const char *u = t;
    for (; a > 0 && u && *u; u++)
        if (*u == '#' && --a == 0)
            return u + 1;
    return t;
}
static void sting_fire(void)                     /* its notes as bells, moved with Root, struck together (a hand's spread) */
{
    int st = sting_pend / EN_NSTING, k = sting_pend % EN_NSTING, i, mv = (par[P_ROOT] - STORIES[st].key + 12) % 12;
    if (mv > 6)
        mv -= 12;
    if (sting_theme[st][k]) {                   /* a theme: from the next bar, in time */
        theme_fire(act_part(STORIES[st].sting[k], sting_act), mv);
        sting_pend = -1;
        return;
    }
    if (sting_buzz[st][k] && sting_n[st][k]) {  /* a fly, flying past */
        buzz_start(sting_note[st][k][0] + mv);
        sting_pend = -1;
        return;
    }
    for (i = 0; i < sting_n[st][k]; i++) {
        int n = sting_note[st][k][i] + mv;
        while (n > 103)
            n -= 12;
        while (n < 36)
            n += 12;
        bell_sting_now = 1;
        bell_voice(i % 2 ? 1 : 2, n, 0.95f, (int32_t)i * 300);
        bell_sting_now = 0;
        sting_mark();
    }
    sting_pend = -1;
}
static void tick(void)
{
    int d, l;
    const float pa = fm_clampf(1.0f + 1.3f * arcm, 0.3f, 1.15f);   /* the arc: the pulse fades (never out) */
    const int bar = bar_pos == 0;
    if (bar) {
        meter_bar();
        en_barno++;
        if (rb_held && !rb_rec && !rb_regrab && !(en_barno & 1u)) {   /* held a while (latched): a fresh moment */
            rb_rec = 1;
            rb_rec_n = 0;
        }
        if (bd_req) {                            /* the breakdown, on the bar */
            bd_req = 0;
            bd = 1;
            bd_ret = 0;
        } else if (bd_ret == 1) {                /* coming back: this bar is the fill */
            bd_ret = 2;
        } else if (bd_ret == 2) {                /* and back */
            bd = 0;
            bd_ret = 0;
            drum_hit(0, 1.0f, 0);                /* landing on the one */
        }
        en_bd = (uint8_t)(bd ? (bd_ret ? 2 : 1) : 0);
    }
    if (bd_ret == 2 && bar_pos >= en_bar_len - 4) /* the fill: the last beat's sixteenths, rising */
        drum_hit(1, 0.45f + 0.15f * (float)(bar_pos - (en_bar_len - 4)), (int)(sixteenth & 1u));
    if (bar) {                                  /* a bar: Evolve's rests and turns */
        if (pulse_rest > 0)
            pulse_rest--;
        else if (frand() < evolve * 0.015f * (1.0f - 0.01f * (float)par[P_GROOVE]))
            pulse_rest = 2 + irand(3);           /* a short breath: 2 to 4 bars (less with Groove) */
        en_pulse_rest = (uint8_t)(pulse_rest > 0);
        if (frand() < evolve * 0.25f * (1.0f - 0.01f * (float)par[P_GROOVE]))
            drums_evolve();                      /* (Groove steadies them: at 100 they stay put) */
    }
    if (bar)                                    /* StoryBox: a phrase's last bar (of four) may end in a fill: more
                                                 * surely with Structure, more often with Energy */
        fill_bar = (en_barno & 3u) == 0u && !pulse_rest && !bd && drum_kb[0] > 0 &&
                   frand() < (0.2f + 0.45f * (float)par[P_GROOVE] * 0.01f) * (0.7f + 0.3f * energy);
    if (par[P_KIT] == 3 && en_bar_len == 16 && !pulse_rest && !bd && par[P_PULSE] > 0) {   /* Lounge: the cross-stick */
        int b = bar_pos;
        if (b == 12)
            snare_hit((0.55f + 0.15f * frand()) * pa);
        else if ((b == 6 || b == 14) && frand() < 0.08f + 0.15f * (energy + 1.0f) * 0.5f)
            snare_hit(0.2f * pa);
    } else if (par[P_KIT] && en_bar_len == 16 && !pulse_rest && !bd && par[P_PULSE] > 0) {   /* the backbeat kit (and
                                                                                     * Bright's: a clap on two, four) */
        int b = bar_pos;
        if (b == 4 || b == 12)
            snare_hit((0.75f + 0.2f * frand()) * pa);
        else if ((b == 7 || b == 15) && frand() < 0.15f + 0.3f * (energy + 1.0f) * 0.5f)
            snare_hit((0.13f + 0.08f * frand()) * pa);   /* a ghost (0.8.5: each its own weight) */
        else if (par[P_KIT] == 1 && (b == 3 || b == 10) && frand() < 0.06f + 0.2f * (energy + 1.0f) * 0.5f)
            snare_hit((0.1f + 0.06f * frand()) * pa);    /* (0.8.5) hip-hop's ghosts: the "a" before the backbeat, the
                                                          * "and" of three */
    } else if (par[P_KIT] && en_bar_len == 12 && !pulse_rest && !bd && par[P_PULSE] > 0) {   /* a waltz's: pah, pah */
        int b = bar_pos;
        if (b == 4 || b == 8)
            snare_hit((0.38f + 0.12f * frand() + (b == 8 ? 0.08f : 0.0f)) * pa);
        else if (b == 11 && frand() < 0.1f + 0.2f * (energy + 1.0f) * 0.5f)
            snare_hit(0.15f * pa);
    }
    if (!pulse_rest && !bd && drum_kb[0] > 0 && !fill_bar && bar_pos == en_bar_len - 2 &&
        !(drum_pat[0] >> drum_pos[0] & 1u) && frand() < 0.08f + 0.1f * (energy + 1.0f))
        drum_hit(0, (0.5f + 0.15f * frand()) * pa, 0);   /* now and then a kick leading into the bar */
    if (fill_bar && bar_pos >= en_bar_len - 4 && bd_ret != 2) {   /* the fill: the last beat's sixteenths, rising */
        int f = bar_pos - (en_bar_len - 4);
        drum_hit(1, (0.4f + 0.15f * (float)f) * pa, (int)(sixteenth & 1u));
        if (par[P_KIT] && f >= 2)
            snare_fire((0.4f + 0.2f * (float)(f - 2)) * pa);
        else if (!par[P_KIT] && f == 2)
            drum_hit(0, 0.6f * pa, 0);
    }
    for (d = 0; d < EN_NDRUM; d++) {
        int p = drum_pos[d];
        if (drum_pat[d] >> p & 1u && !pulse_rest && !bd && !(fill_bar && d == 1 && bar_pos >= en_bar_len - 4))
            drum_hit(d, ((p == 0 ? 0.95f : (p & 3) == 0 ? 0.75f : 0.55f) + 0.25f * frand()) * pa,   /* accents */
                     (int)(sixteenth & 1u));
        en_drum_pos[d] = drum_pos[d];
        if (++drum_pos[d] >= drum_n[d])
            drum_pos[d] = 0;
    }
    if ((sixteenth & 3u) == 0u) {               /* the loops step in quarter notes */
        for (l = 0; l < EN_NLOOP; l++) {
            int s = loop_pos[l];
            if (loop_note[l][s] >= 0 && !loop_rest[l])
                bell_ring(l, s);
            en_loop_pos[l] = (uint8_t)s;
            if (++loop_pos[l] >= en_loop_len[l]) {
                loop_pos[l] = 0;
                loop_pass(l);
            }
        }
    }
    if (bar && change_pending && !en_frozen) {  /* chords move on the bar */
        change_pending = 0;
        chord_change();
    } else if (bar && prog_on() && !en_frozen && --prog_left <= 0) {
        prog_advance();                          /* a progression: its chords on their bars */
    }
    refrain_tick();                              /* (0.8: Dream's refrain on the bells) */
    if (!prog_on())
        loop16 = -1;
    else if (loop16 >= 0) {                      /* (0.8.1) the loop's place, for the screen */
        int b, tot = 0;
        for (b = 0; b < prog_n[prog_cur]; b++)
            tot += prog_bars[prog_cur][b];
        en_loop_bars = (int16_t)tot;
        en_loop_bar = (int16_t)(loop16 / en_bar_len);
    }
    theme_bar();                                 /* (0.7: a theme's sixteenth; 0.8.1: after the loop's step) */
    if (loop16 >= 0)
        loop16++;
    if (sting_pend >= 0 && (bar_pos & 3u) == 0u)
        sting_fire();                            /* a sting: on the beat */
    if ((bar_pos & 3u) == 0u)
        ans_beat();                              /* an answer waiting: it starts on the beat */
    if (++bar_pos >= en_bar_len)
        bar_pos = 0;
    sixteenth++;
    en_bars = sixteenth;
}

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in FoMni): a predelay, four input diffusers,
 * then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping lowpass, an
 * allpass and a delay, feeding the other half. Stereo out from seven taps a side. The paper's lengths
 * are for 29761 Hz; DS() scales them to 44.1 kHz. */
#define DS(n) (((n) * 14818 + 5000) / 10000)          /* 44100 / 29761 */
#define PRE_N 1323                                   /* 30 ms */
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_dk = 0.45f;                       /* (the tank's damping: less while frozen) */
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_decay_t = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}

static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += 0.6f * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0001f * lfo_c, c = lfo_c - 0.0001f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += pl_dk * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += pl_dk * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}

static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------- echo: tape and Echoplex --- */
/* After Discreet Music's chain: an Echoplex (a short, smearing echo, ~160 ms, fed back) and a tape loop
 * (long: up to ~5.6 s, synced to the tempo), each repeat a little darker and thinner, with a little
 * wow. The tape runs at a quarter rate (11 kHz, 16-bit, mono): it loses its highs anyway. Two heads:
 * the first plays the loop's length, the second a part of it (3/4 at first). With Motion the echo
 * evolves: each head wanders across the field, the feedback leans now on one head, now on the other
 * (the repeats fall in changing patterns, left and right), and now and then the second head slides to
 * another part of the loop, as a head moved by hand (the repeats bend a little in pitch on the way).
 * The Echoplex has a second tap, for the right. */
#define TAPE_N 44000                             /* 4 s at 11.025 kHz (StoryBox: from 5.6 s, room for the punch-ins) */
#define TAPE_SR 11025.0f
static int16_t tape[TAPE_N] EN_POOL;
static int tape_w, tape_ph;
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_p2, tape_c1, tape_c2;
static const float HEAD2[5] = {0.75f, 0.6667f, 0.625f, 0.5f, 0.375f};   /* the second head's parts */
static float head2 = 0.75f, head2_t = 0.75f, head2_v;   /* its place (of the loop), target, speed */
static float fb_lean;                            /* 0..1: how much of the feedback is the second head's */
static float hg[4] = {1.0f, 0.0f, 0.0f, 1.0f};   /* the heads' pan gains: 1 left, 1 right, 2 left, 2 right */
static uint32_t head_left;                       /* samples until the second head may move */
#define PLEX_N 7100                              /* 160 ms at 44.1 kHz */
#define PLEX_TAP 2700                            /* the right's tap: ~100 ms */
static int16_t plex[PLEX_N];
static int plex_w;
static float plex_lp, plex_lp2;

static inline float tape_read(float d)           /* d quarter-rate samples ago */
{
    int k = (int)d, i0, i1;
    float f = d - (float)k, a, c;
    i0 = tape_w - k;
    i1 = i0 - 1;
    if (i0 < 0)
        i0 += TAPE_N;
    if (i1 < 0)
        i1 += TAPE_N;
    a = (float)tape[i0];
    c = (float)tape[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void echo_time(void)
{
    int i = par[P_ETIME];
    float beat, t;
    if (par[P_TEMPO] <= 0)
        return;                                  /* not yet (en_init applies in order) */
    beat = 60.0f / (float)par[P_TEMPO];
    t = ETIME_BEATS[i] * beat;
    while (t * TAPE_SR > (float)(TAPE_N - 8) && i > 0)
        t = ETIME_BEATS[--i] * beat;             /* too long for the tape: the next shorter */
    if (t * TAPE_SR > (float)(TAPE_N - 8))
        t = (float)(TAPE_N - 8) / TAPE_SR;
    echo_len_t = t * TAPE_SR;
    if (echo_len <= 0.0f)
        echo_len = echo_len_t;
}

/* once a chunk: where the heads sit in the field, how the feedback leans, the second head's slide */
static void heads_step(uint32_t n)
{
    float p, want, vmax;
    int i;
    for (i = 0; i < 2; i++) {
        p = fm_clampf((i ? 0.7f : -0.7f) + 0.9f * motion * mod[6 + i].v, -1.0f, 1.0f);
        p = (p + 1.0f) * (FM_PI / 4.0f);
        hg[2 * i] = fm_cosf(p);
        hg[2 * i + 1] = fm_sinf(p);
    }
    fb_lean = 0.25f * motion * (1.0f + mod[8].v);
    if (head_left <= n) {
        head_left = (uint32_t)((20.0f + 40.0f * frand()) * EN_SR);
        if (en_playing && !en_frozen && frand() < motion)
            head2_t = HEAD2[(int)(frand() * 4.999f)];
    } else {
        head_left -= n;
    }
    /* its speed (a part of the loop per tape sample): at most a bend of ~14 cents, easing in and out */
    vmax = 0.008f / fm_maxf(echo_len, 100.0f);
    want = fm_clampf((head2_t - head2) * (1.0f / (2.0f * TAPE_SR)), -vmax, vmax);
    head2_v += (want - head2_v) * fm_minf((float)n * (1.0f / EN_SR), 1.0f);
}

/* one sample of both: the input (a mono send) to the tape's heads (el, er) and the Echoplex (xl, xr) */
static inline void echoes(float in, float *el, float *er, float *xl, float *xr)
{
    float y = (float)plex[plex_w] * (1.0f / 12000.0f);         /* the Echoplex: 160 ms, fed back, darker */
    int t = plex_w - PLEX_TAP;
    plex_lp += 0.35f * (y - plex_lp);
    plex_lp2 += 0.35f * ((float)plex[t < 0 ? t + PLEX_N : t] * (1.0f / 12000.0f) - plex_lp2);
    plex[plex_w] = s16(in + 0.55f * plex_lp);
    if (++plex_w >= PLEX_N)
        plex_w = 0;
    *xl = plex_lp;
    *xr = plex_lp2;
    tape_acc += in + 0.3f * plex_lp;
    if (++tape_ph >= 4) {                        /* the tape, a quarter of the time */
        float d, h1, h2, fb;
        tape_ph = 0;
        echo_len += (echo_len_t - echo_len) * 0.001f;
        head2 += head2_v;
        d = echo_len * (1.0f + 0.003f * motion * mod[5].v);   /* wow */
        if (d > (float)(TAPE_N - 4))
            d = (float)(TAPE_N - 4);
        h1 = tape_read(d);
        h2 = tape_read(d * head2);
        tape_lp += 0.55f * (h1 + fb_lean * (h2 - h1) - tape_lp);   /* each time round: darker, thinner */
        tape_hp += 0.03f * (tape_lp - tape_hp);
        fb = tape_lp - tape_hp;
        if (++tape_w >= TAPE_N)
            tape_w = 0;
        tape[tape_w] = s16(tape_acc * 0.25f + echo_fb * fb);
        tape_acc = 0.0f;
        tape_p1 = tape_c1;
        tape_p2 = tape_c2;
        tape_c1 = h1;
        tape_c2 = h2;
    }
    {   /* between the tape's samples: a line from the last to the next; then to the field */
        float f = (float)tape_ph * 0.25f;
        float h1 = tape_p1 + (tape_c1 - tape_p1) * f, h2 = tape_p2 + (tape_c2 - tape_p2) * f;
        *el = hg[0] * h1 + hg[2] * h2;
        *er = hg[1] * h1 + hg[3] * h2;
    }
}

/* ------------------------------------------------------------ parameters --- */
static int pulse;
static uint32_t ext_age;

static void tempo_set(void) { step_len = EN_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f); }

static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    switch (p) {
    case P_PAD: pad_vol = (float)(v * v) * 0.0001f; break;
    case P_BELLS: bell_vol = (float)(v * v) * 0.0001f * (1.0f + 0.3f * energy); break;
    case P_BASS: bass_vol = (float)(v * v) * 0.0001f; break;
    case P_PULSE: pulse_vol = (float)(v * v) * 0.0001f * (1.0f + 0.45f * energy); break;
    case P_ENERGY:                               /* Energy: the levels, the hits and the brightness again, through it */
        energy = (float)(v - 50) * 0.02f;
        apply(P_BELLS, par[P_BELLS]);
        apply(P_PULSE, par[P_PULSE]);
        apply(P_BRIGHT, par[P_BRIGHT]);
        drums_pattern();
        break;
    case P_HITS: drums_pattern(); break;
    case P_BSOUND: break;
    case P_GLIDE: glide_k = 1.0f / ((0.02f + 0.012f * (float)v) * EN_SR); break;   /* 20 ms .. 1.2 s */
    case P_SWING: swing = (float)v * 0.005f; break;      /* the off sixteenths up to half a step late */
    case P_FREEDOM: freedom = (float)v * 0.01f; break;
    case P_LLEVEL: lead_vol = (float)(v * v) * 0.0001f; break;
    case P_LTONE: lead_tone = (float)v * 0.01f; break;
    case P_LGLIDE: lead_glide_t = v ? 0.01f + 0.004f * (float)v : 0.0f; break;   /* to 0.41 s */
    case P_TENSION: tension = (float)v * 0.01f; break;
    case P_METER: meter_next = (uint8_t)(v != 0); break;
    case P_KIT: drums_pattern(); break;
    case P_HATS:
        hat_f = 0.55f + 0.0070f * (float)v;               /* the high-pass: ~4 kHz dark .. ~10 kHz bright */
        hat_open = 0.6f - 0.0035f * (float)v;             /* the open hat: long and soft .. short and crisp */
        break;
    case P_DRIFT:
        if (drift_total > 0.0f) {                /* keep the progress, at the new pace */
            float frac = drift_left / drift_total;
            drift_total = drift_secs(v) * EN_SR;
            drift_left = frac * drift_total;
        }
        break;
    case P_ECHO: echo_send = (float)(v * v) * 0.0001f; break;
    case P_ETIME: echo_time(); break;
    case P_REVERB: rev_send = (float)(v * v) * 0.0001f; break;
    case P_SIZE:
        pl_decay_t = 0.6f + 0.375f * (float)v * 0.01f;     /* to 0.975: a long hall */
        echo_fb = 0.5f + 0.32f * (float)v * 0.01f;
        break;
    case P_BRIGHT: bright = fm_clampf((float)v + 25.0f * energy, 0.0f, 100.0f); break;
    case P_SWELL: swell = (float)v; break;
    case P_EVOLVE: evolve = (float)v * 0.01f; break;
    case P_BTONE: btone = (float)v * 0.01f; break;
    case P_PADTONE: pad_handoff(); break;        /* the voices cross over as they did for a new key */
    case P_MOTION: motion = (float)v * 0.01f; break;
    case P_BOCT:                                 /* let it be heard now: a bell from the first loop */
        if (en_playing && nsteps) {
            int l, k;
            for (l = 0; l < EN_NLOOP; l++)
                for (k = 0; k < en_loop_len[l]; k++)
                    if (loop_note[l][k] >= 0) {
                        bell_ring(l, k);
                        return;
                    }
        }
        break;
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_TEMPO:
        tempo_set();
        echo_time();
        break;
    case P_ROOT:
        if (nsteps) {
            pad_retarget();
            pad_handoff();
            bass_to(chord_pcs[0]);
        }
        break;
    case P_MIDI:
        if (!v)
            en_midi_out(0xB0, 123, 0);
        break;
    default: break;
    }
}

static void pool_set(uint32_t p)
{
    pool = (p | 1u) & 0xFFFu;
    scale_set();
    if (deg >= nsteps)
        deg = 0;
    pad_retarget();
    pad_handoff();
    bass_to(chord_pcs[0]);
}

static void regrow(void)                        /* a new piece: fresh loops, home chord, patterns home */
{
    int l;
    for (l = 0; l < EN_NLOOP; l++) {
        loop_regrow(l);
        loop_rest[l] = 0;
    }
    deg = 0;
    chord_x = 0;
    pad_retarget();
    pad_handoff();
    bass_to(chord_pcs[0]);
    bass.rest = 0;
    pulse_rest = 0;
    en_pulse_rest = 0;
    drums_pattern();
    drift_reset();
    change_pending = 0;
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < 5; i++)
        en_midi_out(0xB0 | (uint32_t)i, 123, 0);
    en_midi_out(0xB9, 123, 0);
}

static void quiet(void)
{
    int i;
    for (i = 0; i < NBELL; i++)
        bell[i].on = 0;
    for (i = 0; i < NLEAD; i++)
        lead[i].on = 0;
    for (i = 0; i < EN_NDRUM; i++)
        drum[i].on = 0, drum[i].wait = 0;
    for (i = 0; i < EN_NPAD; i++)
        pad[i].amp = 0.0f;
    bass.amp = 0.0f;
    notes_off();
}

void en_init(void)
{
    int i;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    plate_init();
    for (i = 0; i < TAPE_N; i++)
        tape[i] = 0;
    for (i = 0; i < PLEX_N; i++)
        plex[i] = 0;
    tape_w = plex_w = tape_ph = 0;
    head2 = head2_t = 0.75f;
    head2_v = 0.0f;
    echo_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].note = pad[i].target = (float)(50 + 7 * i);
        pad[i].pan = PAD_PAN[i];
        pad[i].amp = 0.0f;
        pad[i].ph[0] = 0.0f;
        pad[i].ph[1] = 0.37f;
        pad[i].vib = 0.3f * (float)i;
        pad[i].midi_note = 0;
        pad[i].hf = 1.0f;
        pad[i].hand = 0;
    }
    arcm = 0.0f;
    pad_gain = 0.0f;
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    for (i = 0; i < EN_NPAD; i++)
        pad[i].note = pad[i].target;
    bass.note = bass.target;
    for (i = 0; i < EN_NLOOP; i++) {
        loop_pos[i] = 0;
        loop_regrow(i);
    }
    drums_pattern();
    drift_reset();
    pl_decay = pl_decay_t;
}

/* ------------------------------------------------------------ transport --- */
static void clock_pulse(void)
{
    if (pulse == 0)
        tick();
    if (!en_ext)
        midi(0xF8, 0, 0);
    if (++pulse >= 6)
        pulse = 0;
}

static void play_start(int from_top)
{
    int i;
    if (from_top) {
        pulse = 0;
        sixteenth = 0;
        bar_pos = 0;
        for (i = 0; i < EN_NDRUM; i++)
            drum_pos[i] = 0;
        for (i = 0; i < EN_NLOOP; i++)
            loop_pos[i] = 0;
    }
    pulse_left = 0.0f;
    en_playing = 1;
    pad_gate = 1.0f;
    for (i = 0; i < EN_NPAD; i++)               /* from silence: the breaths start over, overlapping */
        if (pad[i].amp < 1e-4f) {
            pad[i].t = START_T[i];
            pad[i].midi_note = 0;
            pad_breathe(&pad[i], i);
        }
    if (bass.amp < 1e-4f)
        bass.note = bass.target;
    midi(0x92, bass.midi_note, 80);
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        drum[d].wait = 0;                        /* a swung hit still waiting stays unplayed */
    en_playing = 0;
    pad_gate = 0.0f;
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_SET:
            if (a < P_NPARAMS)
                apply(a, b);
            break;
        case C_POOL: pool_set((uint32_t)b & 0xFFFu); break;
        case C_PLAY:
            if (a && !en_playing)
                play_start(1);
            else if (!a && en_playing)
                play_stop();
            break;
        case C_NEXT:
            if (en_playing)
                change_pending = 1;
            else
                chord_change();
            break;
        case C_FREEZE: en_frozen = (uint8_t)(a != 0); break;
        case C_MUTE: mute_mask = (uint32_t)b & 15u; break;
        case C_FX: fx_mask = (uint32_t)b & 15u; break;
        case C_LON: call_on(a, b ? b : 100); break;
        case C_LOFF: call_off(a); break;
        case C_ANS:
            ans_amt = (float)b * 0.01f;
            if (!b)
                ans_stop();
            break;
        case C_BREAK:
            if (a && !bd)
                bd_req = 1;
            else if (!a && bd_req)
                bd_req = 0;
            else if (!a && bd && !bd_ret)
                bd_ret = 1;
            break;
        case C_STING:                            /* on the next beat (now while resting); (0.8.1) a theme: a + 8,
                                                  * joining the loop now, else from the loop's start */
            {
                int join = (a & 8) != 0;
                if (a & 16) {                    /* (0.8.2) k + 16: the theme stops, now */
                    theme_stop();
                    sting_pend = -1;
                    break;
                }
                a &= 7;
                if ((unsigned)b < EN_NPIECES && (unsigned)a < EN_NSTING && sting_n[b / EN_NACT][a]) {
                    sting_pend = b / EN_NACT * EN_NSTING + a;
                    sting_act = b % EN_NACT;
                    if (sting_theme[b / EN_NACT][a]) {
                        sting_fire();            /* (read now: it waits for the loop, or joins it) */
                        th_join = (uint8_t)(join || !en_playing);
                    } else if (!en_playing)
                        sting_fire();
                }
            }
            break;
        case C_PROG:                             /* from the progression's start, on the next bar */
            prog_cur = (unsigned)b < EN_NPIECES && prog_n[b] ? b : -1;
            prog_i = -1;
            prog_left = 1;
            en_prog_step = -1;
            break;
        case C_SEED:
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_CLOCK:
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                if (en_playing)
                    clock_pulse();
            } else if (a == EN_CLK_START) {
                play_start(1);
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start(0);
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC:
            quiet();
            theme_stop();
            break;
        case C_REGROW: regrow(); break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK];

/* the pad's bus: a 12 dB low-pass, darker than it sounds on paper (Brian Eno darkened everything: the
 * Synthi's filter below half, then tape). No chorus: a modulated delay against pure tones beats (on
 * the FM-1's speaker above all), and that read as pulsing. The width comes from the voices' places,
 * the echo's two heads and the plate. */
static float padl[BLK], padr[BLK];
static float plp[4];

static void pad_bus(uint32_t n)
{
    const float fc = fm_clampf(700.0f * fm_exp2f(bright * 0.03f + 0.6f * arcm), 300.0f, 9000.0f);
    const float c = 1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR));
    uint32_t j;
    for (j = 0; j < n; j++) {
        float l, r;
        plp[0] += c * (padl[j] - plp[0]);
        plp[1] += c * (plp[0] - plp[1]);
        plp[2] += c * (padr[j] - plp[2]);
        plp[3] += c * (plp[2] - plp[3]);
        l = plp[1];
        r = plp[3];
        mixl[j] += l * 0.8f;
        mixr[j] += r * 0.8f;
        rsend[j] += (l + r) * 0.6f;
        esend[j] += (l + r) * 0.2f;
    }
    for (j = 0; j < 4; j++)
        plp[j] = fm_flush(plp[j]);
}

static void render_pad(uint32_t n)
{
    int i;
    float lvl = 0.0f;
    uint32_t j0;
    for (j0 = 0; j0 < n; j0++)
        padl[j0] = padr[j0] = 0.0f;
    const float g = pad_vol * 0.5f * mg[0];
    pad_gain += (pad_gate - pad_gain) * fm_minf((float)n / ((pad_gate > 0.0f ? 2.0f : 5.0f) * EN_SR), 1.0f);
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        float p0 = v->ph[0], p1 = v->ph[1], amp = v->amp, s0 = v->ic[0], s1 = v->ic[1], s2 = v->ic[2], s3 = v->ic[3],
              gl, gr, pn, tgt = 0.0f, idx = 0.0f;
        uint32_t j;
        if (amp < 1e-5f && pad_gain < 1e-4f) {
            v->amp = 0.0f;
            continue;
        }
        pn = fm_clampf(v->pan + 0.35f * motion * mod[(i + 3) % NMOD_PAD].v, -1.0f, 1.0f);
        gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        for (j = 0; j < n; j++) {
            float x, v1, v2, v3, b1, l1, b2;
            if (!(j & 31u)) {
                float br;
                pad_coefs(v, i);
                br = breath(v->t);
                tgt = br * v->peak * pad_gain * v->hf;
                /* the Rhodes blooms: more overtones as the breath swells (index in turns) */
                idx = (0.04f + 0.1f * bright * 0.01f + 0.1f * br) * (1.0f + 0.4f * arcm);
            }
            if (!v->voices) {                    /* FM, the modulator at the carrier's pitch: a Rhodes */
                float m = sine(p0) * idx;
                x = (sine(p0 + m + 1.0f) + 0.12f * sine(2.0f * p0)) * 1.2f;   /* one carrier: no beating */
                p0 += v->inc[0];
                if (p0 >= 1.0f)
                    p0 -= 1.0f;
                amp += (tgt - amp) * 0.01f;
                x *= amp;
                padl[j] += x * gl;
                padr[j] += x * gr;
                continue;
            }
            if (v->voices >= 2) {                /* StoryBox: a string section (two saws a little apart, softened by
                                                  * Bright: the pad bus's chorus makes the section) or an organ */
                if (v->voices == 2) {
                    x = blep_saw(p0, v->inc[0]) + blep_saw(p1, v->inc[0] * 1.0045f);
                    s0 += (x - s0) * (0.05f + 0.0014f * bright);
                    s1 += (s0 - s1) * (0.08f + 0.0014f * bright);
                    x = s1 * 1.5f;
                    p1 += v->inc[0] * 1.0045f;
                    if (p1 >= 1.0f)
                        p1 -= 1.0f;
                } else {                         /* drawbars: 8', 4', 2 2/3', 2' */
                    x = (sine(p0) + 0.55f * sine(2.0f * p0) + 0.3f * sine(3.0f * p0) + 0.16f * sine(4.0f * p0)) * 0.95f;
                }
                p0 += v->inc[0];
                if (p0 >= 1.0f)
                    p0 -= 1.0f;
                amp += (tgt - amp) * 0.01f;
                x *= amp;
                padl[j] += x * gl;
                padr[j] += x * gr;
                continue;
            }
            /* as Discreet Music's Synthi: a mellow triangle (a sine's neighbour), a little saw in it */
            x = (p0 < 0.5f ? 4.0f * p0 - 1.0f : 3.0f - 4.0f * p0) * 1.4f + 0.45f * blep_saw(p0, v->inc[0]) + 0.6f * sine(p0);
            p0 += v->inc[0];
            if (p0 >= 1.0f)
                p0 -= 1.0f;
            v3 = x - s1;                         /* the first formant: TPT SVF (Zavalishin), bp and lp */
            v1 = v->c1[0] * s0 + v->c1[1] * v3;
            v2 = s1 + v->c1[1] * s0 + v->c1[2] * v3;
            s0 = 2.0f * v1 - s0;
            s1 = 2.0f * v2 - s1;
            b1 = v1 * v->c1[3];
            l1 = v2;
            v3 = x - s3;                         /* the second */
            v1 = v->c2[0] * s2 + v->c2[1] * v3;
            v2 = s3 + v->c2[1] * s2 + v->c2[2] * v3;
            s2 = 2.0f * v1 - s2;
            s3 = 2.0f * v2 - s3;
            b2 = v1 * v->c2[3];
            amp += (tgt - amp) * 0.01f;
            x = (b1 + 0.5f * b2 + 0.45f * l1) * amp;
            padl[j] += x * gl;                   /* to the pad's bus: smoothed and chorused below */
            padr[j] += x * gr;
        }
        v->ic[0] = fm_flush(s0);
        v->ic[1] = fm_flush(s1);
        v->ic[2] = fm_flush(s2);
        v->ic[3] = fm_flush(s3);
        v->amp = amp;
        v->ph[0] = p0;
        v->ph[1] = p1;
        en_pad_note[i] = (uint8_t)v->note;
        en_pad_amp[i] = amp;
        lvl = fm_maxf(lvl, amp);
    }
    en_pad_level = lvl;
    pad_bus(n);
}

static void render_bass(uint32_t n)
{
    uint32_t j;
    const float g = bass_vol * 0.42f * mg[2], tgt = en_playing && !bass.rest ? 1.0f : 0.0f,
                rate = tgt > 0.0f ? 1.0f / (2.5f * EN_SR) : 1.0f / (4.0f * EN_SR);
    float ph = bass.ph, amp = bass.amp, inc;
    if (amp < 1e-5f && !en_playing) {
        bass.amp = 0.0f;
        en_bass_level = 0.0f;
        return;
    }
    bass.note += (bass.target - bass.note) * fm_minf((float)n * glide_k, 1.0f);   /* Glide */
    inc = mtof(bass.note) * (1.0f / EN_SR);
    {   /* StoryBox 0.5: its sound. Round: the fundamental, its sub-octave, a touch of the octave; Upright: plucked
         * (each new note), woody, settling to a quarter; Synth: a soft saw (FM), filtered; Sub: an octave
         * below, warmed so it is heard */
        const int bs = par[P_BSOUND];
        const float pdec = fm_expf(-(float)n / (0.7f * EN_SR));
        float pl = bass.pluck;
        bass.pluck *= pdec;
        for (j = 0; j < n; j++) {
            float x;
            switch (bs) {
            case 1:
                x = (sine(ph) * 0.8f + sine(ph * 2.0f) * 0.3f * pl + sine(ph * 3.0f) * 0.08f * pl) * (0.35f + 1.1f * pl);
                break;
            case 2: {
                float m = sine(ph) * 0.3f;       /* (an index of ~1.9 rad: near a saw) */
                x = sine(ph + m + 4.0f) * 0.7f + sine(ph * 0.5f + 0.25f) * 0.35f;
                bass.lp += (x - bass.lp) * 0.3f;
                x = bass.lp * 1.15f;
                break;
            }
            case 3: {                            /* (0.7: the sine an octave down, rounded over into its overtones so
                                                  * small speakers and headphones carry it, and the note itself) */
                float u = sine(ph * 0.5f + 0.25f) * 5.0f;
                x = u / (1.0f + (u < 0.0f ? -u : u)) * 0.75f + sine(ph) * 0.45f + sine(ph * 2.0f) * 0.08f;
                break;
            }
            default:
                x = sine(ph) * 0.75f + sine(ph * 0.5f + 0.25f) * 0.45f + sine(ph * 2.0f) * 0.12f;
                break;
            }
            ph += inc;
        if (ph >= 2.0f)                          /* two turns: the sub-octave's period */
            ph -= 2.0f;
        amp += (tgt - amp) * rate;
            x *= amp * g;
            mixl[j] += x;
            mixr[j] += x;
            rsend[j] += x * 0.08f;
        }
    }
    bass.ph = ph;
    bass.amp = amp;
    en_bass_level = amp;
}

static void render_bells(uint32_t n)
{
    int i;
    /* (StoryBox 0.3: the melody a quarter quieter) */
    const float gb = bell_vol * 0.34f * mg[1], gs = 0.15f, arate = 0.0004f + 0.0056f * fm_minf(btone * 2.0f, 1.0f);
    float trem;
    mel_trem += (float)n * (4.2f / EN_SR);       /* the vibes' motor */
    mel_vib += (float)n * (5.0f / EN_SR);        /* the slide's vibrato */
    if (mel_vib >= 1.0f)
        mel_vib -= 1.0f;
    if (mel_trem >= 1.0f)
        mel_trem -= 1.0f;
    trem = 0.5f + 0.5f * sine(mel_trem);
    for (i = 0; i < NBELL; i++) {
        bell_t *v = &bell[i];
        float cph = v->cph, mph = v->mph, amp = v->amp, idx = v->idx, att = v->att;
        uint32_t j = 0;
        float g = v->sting ? gs : gb, ar = MEL[v->mel].atk > 0.0f ? MEL[v->mel].atk : arate, sm = 1.0f,
              mecho = v->sting ? 0.85f : (float)par[P_MECHO] * 0.017f;   /* (Echo 50: as before) */
        if (MEL[v->mel].trem)
            g *= 1.0f - 0.5f * trem;
        if (!v->sting)
            g *= MEL_GAIN[v->mel];
        if (!v->on)
            continue;
        if (v->wait > 0) {
            if ((uint32_t)v->wait >= n) {
                v->wait -= (int32_t)n;
                continue;
            }
            j = (uint32_t)v->wait;
            v->wait = 0;
        }
        if (v->mel == MEL_SLIDE) {              /* the slide: gliding in (~0.15 s), then a vibrato */
            v->sl += (1.0f - v->sl) * fm_minf((float)n * (1.0f / (0.15f * EN_SR)), 1.0f);
            v->sv += (float)n * (1.0f / EN_SR);
            sm = v->sl * (1.0f + 0.006f * fm_clampf((v->sv - 0.2f) * 2.0f, 0.0f, 1.0f) * sine(mel_vib));
        }
        for (; j < n; j++) {
            float m = sine(mph) * idx, x;
            float p = cph + m + 4.0f;            /* + 4 turns: positive for the table */
            x = sine(p) * amp * att * g;
            mixl[j] += x * v->gl;
            mixr[j] += x * v->gr;
            rsend[j] += x * 0.7f;
            esend[j] += x * mecho;
            cph += v->cinc * sm;
            mph += v->minc * sm;
            if (cph >= 1.0f)
                cph -= 1.0f;
            if (mph >= 1.0f)
                mph -= 1.0f;
            amp *= v->adec;
            idx *= v->idec;
            att += (1.0f - att) * ar;            /* the strike: ~4 ms, or soft as a slowed piano (~50 ms) */
        }
        v->cph = cph;
        v->mph = mph;
        v->amp = amp;
        v->idx = idx;
        v->att = att;
        if (amp < 0.0008f) {
            v->on = 0;
            midi(0x81, v->note, 0);
        }
    }
}

/* a hat: the metal and noise, high-passed (Chamberlin SVF, ~4 to 10 kHz by Hats); no pitch, little echo */
static void render_hat(drum_t *v, uint32_t n, float g, float pl, float pr)
{
    uint32_t j;
    int k;
    float x1, x2, amp, att;
    if (!v->on)
        return;
    x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att;
    if (par[P_KIT] == 2) {                       /* StoryBox's brushes: noise only, lower, swishing in */
        const float f = hat_f * 0.5f;
        for (j = 0; j < n; j++) {
            float hp, x;
            noise = noise * 1664525u + 1013904223u;
            hp = (float)(int32_t)noise * (1.0f / 2147483648.0f) * 0.8f - x2 - 0.9f * x1;
            x1 += f * hp;
            x2 += f * x1;
            x = x1 * 0.7f * amp * att * g;       /* (its band) */
            mixl[j] += x * pl;
            mixr[j] += x * pr;
            rsend[j] += x * 0.3f;
            amp *= v->adec;
            att += (1.0f - att) * 0.012f;
        }
        v->x1 = fm_flush(x1);
        v->x2 = fm_flush(x2);
        v->amp = amp;
        v->att = att;
        v->on = amp > 0.0005f;
        return;
    }
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp, x;
        for (k = 0; k < 4; k++) {
            v->mph[k] += HAT_INC[k];
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            in += v->mph[k] < 0.5f ? 0.25f : -0.25f;
        }
        noise = noise * 1664525u + 1013904223u;
        in = 0.5f * in + 0.6f * (float)(int32_t)noise * (1.0f / 2147483648.0f);
        hp = in - x2 - 0.8f * x1;
        x1 += hat_f * hp;
        x2 += hat_f * x1;
        x = hp * amp * att * g;
        mixl[j] += x * pl;
        mixr[j] += x * pr;
        rsend[j] += x * 0.2f;
        esend[j] += x * 0.08f;
        amp *= v->adec;
        att += (1.0f - att) * 0.3f;
    }
    v->x1 = fm_flush(x1);
    v->x2 = fm_flush(x2);
    v->amp = amp;
    v->att = att;
    v->on = amp > 0.0005f;
}

/* StoryBox: the backbeat kit's snare: a clap's three quick bursts of band-passed noise over a short body (190 Hz) */
static struct {
    float amp, ph, bamp, lp, hp;
    int32_t t;
    uint8_t on;
} sn;
static void snare_fire(float vel)
{
    sn.amp = vel;
    sn.bamp = vel;
    sn.ph = 0.0f;
    sn.t = 0;
    sn.on = 1;
    midi(0x99, 38, (uint32_t)(vel * 127.0f));
    midi(0x89, 38, 0);
}
static void render_snare(uint32_t n, float g)
{
    uint32_t j;
    const int rim = par[P_KIT] == 2 || par[P_KIT] == 3;   /* brushes, Lounge: a rim's click (a short knock, ~1.2 kHz) */
    const float dec = fm_expf(-6.9f / ((rim ? 0.01f : 0.2f) * EN_SR)), bdec = fm_expf(-6.9f / ((rim ? 0.03f : 0.08f) * EN_SR)),
                binc = (rim ? 1250.0f : 190.0f) / EN_SR;
    if (!sn.on)
        return;
    for (j = 0; j < n; j++) {
        float nz, x, burst = 1.0f;
        int32_t t = sn.t++;
        noise = noise * 1664525u + 1013904223u;
        nz = (float)(int32_t)noise * (1.0f / 2147483648.0f);
        sn.lp += (nz - sn.lp) * 0.35f;          /* band-passed: ~1 .. 5 kHz */
        sn.hp += (sn.lp - sn.hp) * 0.12f;
        if (t < 1100 && !rim)                   /* the clap: three quick re-attacks */
            burst = 1.0f + 0.9f * fm_expf(-(float)(t % 370) * (1.0f / 70.0f));
        x = (sn.lp - sn.hp) * 1.6f * sn.amp * burst + sine(sn.ph) * 0.55f * sn.bamp;
        x *= g;
        mixl[j] += x * 0.9f;
        mixr[j] += x;
        rsend[j] += x * 0.3f;
        sn.ph += binc;
        if (sn.ph >= 1.0f)
            sn.ph -= 1.0f;
        sn.amp *= dec;
        sn.bamp *= bdec;
    }
    sn.on = sn.amp > 0.0005f || sn.bamp > 0.0005f;
}

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = pulse_vol * 1.45f * mg[3];   /* (StoryBox 0.3: louder) */
    drum_t *v;
    v = &drum[0];                                /* thump: a sine falling to its note */
    if (v->on) {
        float ph = v->ph, inc = v->inc, amp = v->amp, att = v->att;
        for (j = 0; j < n; j++) {
            float x = sine(ph) * amp * att * g;
            mixl[j] += x;
            mixr[j] += x;
            rsend[j] += x * 0.1f;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc += (v->inc_end - inc) * 0.0012f;
            amp *= v->adec;
            att += (1.0f - att) * 0.02f;
        }
        v->ph = ph;
        v->inc = inc;
        v->amp = amp;
        v->att = att;
        v->on = amp > 0.0005f;
    }
    render_hat(&drum[1], n, g * 0.2f, 0.6f, 1.0f);    /* closed: a little right */
    render_hat(&drum[2], n, g * 0.14f, 1.0f, 0.7f);   /* open: a little left */
    render_snare(n, g * 0.5f);
}

/* ------------------------------------------------------------------ lead --- */
/* StoryBox's lead, three voices. Five ways to make a sound (StoryBox 0.3: Rhodes for the piano, a bowed solo voice
 * through a body's resonances for Violin and Cello, a slide guitar):
 *   FM      two operators: a bright strike (the index fading) over a body that rings (kalimba, bell)
 *   FLUTE   a warm tone (a little self-modulation and an octave), a breath band-passed from noise, a chiff as the note
 *           starts, vibrato easing in after a moment
 *   STRING  a plucked string (Karplus-Strong): a burst of filtered noise in a delay line the length of the note, fed
 *           back through a gentle low-pass; a guitar
 *           (Slide: one voice, the string's length gliding from note to note: a bottleneck's slide, sliding up into
 *           a fresh note)
 *   PIANO   a Rhodes: two pairs, a body (FM 1:1, its index fading to a floor, ringing long) and a tine (14:1, a
 *           short bright clang); the body's twin a little sharp; a slow stereo tremolo
 *   BOWED   a bowed string: a near-saw, brighter as the bow presses (the swell), a slow vibrato easing in, the bow's
 *           hiss, through the body's resonances (a violin's: its air, its wood and the bridge's hill; a cello's,
 *           an octave and more lower)
 * ring: how long a struck note rings (s; 0 held: it lasts while the key is down, then fades over rel) */
enum { LK_FM, LK_FLUTE, LK_STRING, LK_PIANO, LK_BOWED };
static float lead_vib, lead_trem, lead_last;   /* (lead_last: the last note's pitch, for Glide) */
static float body_s[2][3][2];                    /* the bowed body's three resonators a side: band, low */
static int16_t ks[NLEAD][KS_N] EN_POOL;          /* the strings, as 16-bit samples (x 16384) */
static const struct {
    uint8_t kind, legato, body;                  /* legato: one voice, gliding (with Glide); body: 1 violin, 2 cello */
    int8_t oct;
    float ratio, idx, idx_t, ring, atk, rel, vib, breath, detune, gain;
} LEADS[EN_NLEAD] = {
    {LK_FM, 0, 0, 0, 5.4f, 1.4f, 0.07f, 2.4f, 0.002f, 0.4f, 0.0f, 0.0f, 0.0f, 1.0f},        /* kalimba: a tick, a round body */
    {LK_FM, 0, 0, 0, 3.5f, 2.2f, 0.9f, 3.5f, 0.003f, 0.6f, 0.0f, 0.0f, 0.0f, 1.0f},         /* bell: a music box's */
    {LK_FLUTE, 1, 0, 0, 1.0f, 0.0f, 0.0f, 0.0f, 0.09f, 0.8f, 0.0045f, 0.06f, 0.0f, 1.0f},   /* flute (0.4: a slower release) */
    {LK_STRING, 0, 0, 0, 1.0f, 0.0f, 0.0f, 4.0f, 0.001f, 0.15f, 0.0f, 0.0f, 0.0f, 0.9f},    /* guitar (0.4: quieter) */
    {LK_PIANO, 0, 0, 0, 1.0f, 0.9f, 1.1f, 4.5f, 0.002f, 0.9f, 0.0f, 0.0f, 0.0008f, 1.05f},  /* Rhodes: warm, a bell-like tine;
                                                                                              * sustained while held */
    {LK_BOWED, 0, 1, 0, 1.0f, 1.9f, 60.0f, 0.0f, 0.16f, 1.2f, 0.0065f, 0.03f, 0.0018f, 1.25f},  /* violin (a slower release) */
    {LK_BOWED, 0, 2, -1, 1.0f, 1.8f, 60.0f, 0.0f, 0.24f, 1.2f, 0.0055f, 0.025f, 0.0015f, 1.35f}, /* cello */
    {LK_STRING, 1, 0, 0, 1.0f, 0.0f, 0.0f, 10.0f, 0.001f, 0.25f, 0.0075f, 0.0f, 0.0f, 1.0f},  /* slide guitar: long (quieter) */
};
/* the bodies' resonances: Hz, Q, gain; and how much of the string goes straight through */
static const float BODY[2][3][3] = {
    {{290.0f, 3.0f, 0.9f}, {470.0f, 4.0f, 1.1f}, {2900.0f, 1.4f, 0.8f}},     /* violin: its air, its wood, the bridge */
    {{110.0f, 3.0f, 1.0f}, {210.0f, 3.5f, 1.1f}, {1400.0f, 1.4f, 0.7f}},     /* cello */
};

int en_lead_snap(int n)                          /* down to the scale (the root kept) */
{
    int i, r = par[P_ROOT];
    for (i = 0; i < 12; i++, n--)
        if (pool >> (((n - r) % 12 + 12) % 12) & 1u)
            return n;
    return n + 12;
}

static void ks_pluck(lead_t *v, int i, float f, float vel)   /* the string's first shape: a pick's burst of noise */
{
    float lp = 0.0f, k = 0.07f + 0.3f * lead_tone * (0.5f + 0.5f * vel), len = EN_SR / f - 0.5f;
    int j, n, pick;
    if (len > (float)(KS_N - 4))
        len = (float)(KS_N - 4);
    n = (int)len + 2;
    for (j = 0; j < n; j++) {                    /* noise, low-passed: a softer pick is darker */
        noise = noise * 1664525u + 1013904223u;
        lp += ((float)(int32_t)noise * (1.0f / 2147483648.0f) - lp) * k;
        ks[i][j] = (int16_t)(fm_clampf(lp * 3.0f, -1.99f, 1.99f) * 16384.0f);
    }
    pick = (int)(len * 0.13f);                   /* plucked near the bridge: the comb that leaves out some overtones */
    for (j = n - 1; j >= pick && pick > 0; j--)
        ks[i][j] = (int16_t)((float)ks[i][j] - 0.6f * (float)ks[i][j - pick]);
    for (j = n; j < KS_N; j++)
        ks[i][j] = 0;
    v->ksl = len;
    v->ksw = n;
    v->kslp = 0.0f;
}

static void lead_note_on(int key, int vel)
{
    int sd = par[P_LSOUND], n = key + 12 * (par[P_LOCT] + LEADS[sd].oct), i, best = 0, kind = LEADS[sd].kind;   /* (0.5:
                                                                                     * every key its own note) */
    float lo = 9.0f, fv = (float)vel / 127.0f, from = 0.0f;
    lead_t *v;
    while (n > 100)
        n -= 12;
    while (n < 28)
        n += 12;
    if (LEADS[sd].legato && lead_glide_t > 0.0f) {   /* one voice, gliding legato (the flute, the slide) */
        v = &lead[0];
        for (i = 1; i < NLEAD; i++)
            lead[i].on = 0;
        if (v->on && v->held) {
            v->inc_t = mtof((float)n) * (1.0f / EN_SR);
            lead_last = v->inc_t;
            midi(0x84, v->note, 0);
            v->key = (uint8_t)key;
            v->note = (uint8_t)n;
            midi(0x94, (uint32_t)n, (uint32_t)vel);
            return;
        }
        if (v->on && kind == LK_STRING)          /* the string still ringing: picked again, sliding from its note */
            from = v->inc;
        best = 0;
    } else {
        for (i = 0; i < NLEAD; i++) {            /* a free voice, or the quietest */
            float a = lead[i].on ? lead[i].amp * lead[i].rel : -1.0f;
            if (a < lo)
                lo = a, best = i;
        }
    }
    v = &lead[best];
    if (v->on)
        midi(0x84, v->note, 0);
    v->on = 1;
    v->held = 1;
    v->key = (uint8_t)key;
    v->note = (uint8_t)n;
    v->cph = v->mph = v->mph2 = 0.0f;
    v->cph2 = 0.37f;
    v->blp = v->bhp = 0.0f;
    v->chiff = 1.0f;
    v->vt = 0.0f;
    v->inc = v->inc_t = mtof((float)n) * (1.0f / EN_SR);
    if (LEADS[sd].legato && kind == LK_STRING)   /* the slide: from the last note, or up into this one */
        v->inc = from > 0.0f ? from : v->inc_t * 0.9439f;
    else if (lead_glide_t > 0.0f && lead_last > 0.0f)   /* Glide (0.5: every sound): from the note before */
        v->inc = lead_last;
    lead_last = v->inc_t;
    v->amp = 0.35f + 0.65f * fv;
    v->amp2 = 0.5f * fv + 0.15f;
    v->att = 0.0f;
    v->idx = LEADS[sd].idx * (0.3f + 1.4f * lead_tone) * (0.6f + 0.6f * fv) * (1.0f / FM_TWO_PI);
    v->idx2 = (0.4f + 1.2f * lead_tone) * fv * (1.0f / FM_TWO_PI);
    v->rel = 1.0f;
    if (kind == LK_STRING)
        ks_pluck(v, best, v->inc * EN_SR, fv);
    midi(0x94, (uint32_t)n, (uint32_t)vel);
}

static void lead_note_off(int key)
{
    int i;
    for (i = 0; i < NLEAD; i++)
        if (lead[i].on && lead[i].held && lead[i].key == key) {
            lead[i].held = 0;
            midi(0x84, lead[i].note, 0);
        }
}

/* StoryBox 0.6, the answer (call and response, as NACHFAHRT's): a phrase played on the lead, then a beat's rest, and
 * StoryBox plays it back, changed: its rhythm kept (now and then a note left out), its notes moved along the scale,
 * more the higher Answer (a step here and there, the shape turned over, the whole moved up or down, an octave), and
 * its last note on a note of the chord. Kept as events, not sound. A key played while it answers stops it: it
 * listens again. */
#define ANS_N 16
static struct {
    uint8_t key, vel, state, firm;               /* (resp: 0 waiting, 1 sounding, 2 done; firm (0.8.3): on a beat or
                                                  * long, so a note of the chord when it sounds) */
    uint32_t on, off;
} call[ANS_N], resp[ANS_N];
static int call_n, call_held, resp_n, ans_state;   /* ans_state: 0 listening, 1 waiting for the beat, 2 answering */
static int imp_rest;                             /* (0.7, improvising) beats to its next phrase */
static uint32_t smp, call_last, ans_t0;
volatile uint8_t en_ans_ring[16];                /* the answer's notes, for the screen: the key, | 0x80 let go */
volatile uint8_t en_ans_w;

static void ans_note(int key, int on)
{
    if (on)
        lead_note_on(key, resp[0].vel);
    else
        lead_note_off(key);
    en_ans_ring[en_ans_w & 15u] = (uint8_t)((key & 0x7F) | (on ? 0 : 0x80));
    BARRIER();
    en_ans_w++;
}

static void ans_stop(void)
{
    int i;
    for (i = 0; i < resp_n; i++)
        if (resp[i].state == 1) {
            ans_note(resp[i].key, 0);
            resp[i].state = 2;
        }
    ans_state = 0;
    resp_n = 0;
}

static void call_on(int key, int vel)
{
    int i;
    if (ans_state)
        ans_stop();                              /* the player again: the answer gives way */
    imp_rest = 4;                                /* (and the improvising waits) */
    if (ans_amt > 0.0f) {
        if (call_n && !call_held && smp - call_last > (uint32_t)(EN_SR * 4.0f))
            call_n = 0;                          /* a long rest: a new phrase */
        if (call_n == ANS_N) {                   /* (the last sixteen kept) */
            for (i = 1; i < ANS_N; i++)
                call[i - 1] = call[i];
            call_n--;
        }
        call[call_n].key = (uint8_t)key;
        call[call_n].vel = (uint8_t)vel;
        call[call_n].on = smp;
        call[call_n].off = 0;
        call_n++;
        call_held++;
    }
    lead_note_on(key, vel);
}

static void call_off(int key)
{
    int i;
    for (i = call_n - 1; i >= 0; i--)
        if (call[i].key == key && !call[i].off) {
            call[i].off = smp ? smp : 1u;
            if (call_held > 0)
                call_held--;
            call_last = smp;
            break;
        }
    lead_note_off(key);
}

static int scale_note(int d, const int *sc, int m)   /* degree d of the scale (from the root's C below 28) */
{
    int r = par[P_ROOT] % 12, o = d >= 0 ? d / m : -((-d + m - 1) / m);
    return r + 12 * o + sc[d - o * m];
}
static int scale_deg(int key, const int *sc, int m)  /* the degree at or just below key */
{
    int r = par[P_ROOT] % 12, o = (key - r) >= 0 ? (key - r) / 12 : -((r - key + 11) / 12), i, rel = key - r - 12 * o;
    for (i = m - 1; i > 0 && sc[i] > rel; i--)
        ;
    return o * m + i;
}

static void ans_build(void)                      /* the call, changed, into resp */
{
    int sc[12], m = en_scale(pool, sc), i, j, d0, inv, shift, oct, n = 0;
    const float a = fm_minf(ans_amt * 2.0f, 1.0f);   /* (0.7: the knob's first half; past it, all the way) */
    const int i0 = ans_amt <= 0.3f && call_n > 4 ? call_n - 4 : 0;   /* (0.8: a short answer, to the call's last four) */
    uint32_t t0 = call[i0].on;
    if (m < 3)
        return;
    d0 = scale_deg(call[i0].key, sc, m);
    inv = frand() < a * 0.5f;
    shift = frand() < 0.2f + 0.5f * a ? (frand() < 0.5f ? -1 : 1) * (1 + (frand() < a)) : 0;
    oct = frand() < a * 0.2f ? (call[0].key < 72 ? 12 : -12) : 0;
    for (i = i0; i < call_n; i++) {
        int d = scale_deg(call[i].key, sc, m) - d0, k;
        if (i > i0 && i < call_n - 1 && call_n - i0 > 3 && frand() < a * 0.25f) {   /* a note left out: the one before held on */
            if (n)
                resp[n - 1].off = (call[i].off ? call[i].off : call[i].on + (uint32_t)(step_len * 2.0f)) - t0;
            continue;
        }
        if (inv)
            d = -d;
        d += d0 + shift;
        if (i > i0 && frand() < a * 0.5f)
            d += frand() < 0.5f ? -1 : 1;
        k = scale_note(d, sc, m) + oct;
        if (i == call_n - 1) {                   /* the last: onto the chord */
            for (j = 0; j < 3; j++) {
                int c, pc[2] = {scale_note(d - j, sc, m) + oct, scale_note(d + j, sc, m) + oct}, h, f = 0;
                for (h = 0; h < 2 && !f; h++) {
                    int p = ((pc[h] % 12) + 12) % 12;
                    if (p == en_chord_root % 12)
                        f = 1;
                    for (c = 0; c < EN_NPAD && !f; c++)
                        if (en_chord[c] % 12 == p)
                            f = 1;
                    if (f)
                        k = pc[h];
                }
                if (f)
                    break;
            }
        }
        while (k < 36)
            k += 12;
        while (k > 96)
            k -= 12;
        resp[n].key = (uint8_t)k;
        resp[n].vel = (uint8_t)(call[i].vel * 0.85f);
        resp[n].on = call[i].on - t0;
        resp[n].off = (call[i].off > call[i].on ? call[i].off : call[i].on + (uint32_t)(step_len * 2.0f)) - t0;
        resp[n].state = 0;
        resp[n].firm = 0;
        n++;
    }
    resp_n = n;
}

static void ans_go(void)
{
    ans_build();
    call_n = 0;
    ans_t0 = smp;
    ans_state = resp_n ? 2 : 0;
}

/* (0.7) Improvise: Answer past its middle, StoryBox plays leads of its own between the player's phrases (and still
 * answers them). A phrase of one or two bars on the scale, a walk mostly by steps from a note of the chord, now
 * and then a leap, ending on the chord; how many notes, and how short the rests between phrases, with Energy
 * (and the knob). A key played: it gives way at once, and waits a little after */
static int chord_has(int pc)
{
    int c;
    pc = ((pc % 12) + 12) % 12;
    if (pc == en_chord_root % 12)
        return 1;
    for (c = 0; c < EN_NPAD; c++)
        if (en_chord[c] % 12 == pc)
            return 1;
    return 0;
}
/* (0.8.3) the improviser, after the way jazz players and the programs that learn from them build a line (Bob Keller's
 * Impro-Visor sorts a solo's notes as chord tones, colour, approach and scale tones; Al Biles's GenJam keeps and varies
 * its own phrases): on the beats and the long notes, a note of the chord; between, steps through the scale, passing; a
 * leap lands on the chord and steps back the other way; now and then the last phrase's rhythm again, new notes on it;
 * and at the end, onto the chord, held. When a note sounds (ans_tick), one that should be the chord's and isn't (the
 * chord moved on since) slides to the nearest that is */
static uint8_t imp_rh[ANS_N], imp_rd[ANS_N], imp_rn;   /* the last phrase's rhythm: its notes' sixteenths and lengths */
static int chord_near(int d, const int *sc, int m, int dir)   /* the scale degree nearest d that is the chord's (dir
                                                  * first: 1 up, -1 down, 0 either) */
{
    int j;
    for (j = 0; j <= 3; j++) {
        int a = dir < 0 ? d - j : d + j, b = dir < 0 ? d + j : d - j;
        if (chord_has(scale_note(a, sc, m)))
            return a;
        if (chord_has(scale_note(b, sc, m)))
            return b;
    }
    return d;
}
static void imp_build(void)                      /* a phrase of its own, into resp */
{
    int sc[12], m = en_scale(pool, sc), n = 0, d, k, bars, len16, slot, last = 0, back = 0, again;
    const float e = fm_clampf((energy + 1.0f) * 0.5f, 0.0f, 1.0f), wild = fm_clampf((ans_amt - 0.5f) * 2.0f, 0.0f, 1.0f);
    const float fill = (0.22f + 0.45f * e + 0.15f * wild) * (0.55f + 0.45f * wild);   /* how many of the eighths sound
                                                  * (0.8: Sparse improv, fewer) */
    uint8_t rh[ANS_N], rd[ANS_N];
    int rn = 0;
    if (m < 3) {
        resp_n = 0;
        return;
    }
    bars = 1 + (frand() < 0.4f + 0.3f * e);
    len16 = bars * (int)en_bar_len;
    again = imp_rn >= 3 && frand() < 0.35f;      /* the last rhythm again (a motif), new notes on it */
    if (again) {
        for (k = 0; k < imp_rn && imp_rh[k] < len16; k++) {
            rh[rn] = imp_rh[k];
            rd[rn] = (uint8_t)(imp_rh[k] + imp_rd[k] > len16 ? len16 - imp_rh[k] : imp_rd[k]);
            rn++;
        }
    } else {
        for (slot = 0; slot < len16 && rn < ANS_N; slot += 2) {
            int dur = 2;
            if (slot && frand() > fill)
                continue;
            while (slot + dur < len16 && dur < 8 && frand() > fill + 0.2f)   /* an eighth, or longer */
                dur += 2;
            rh[rn] = (uint8_t)slot;
            rd[rn++] = (uint8_t)dur;
            slot += dur - 2;
        }
    }
    d = chord_near(scale_deg(64 + irand(8), sc, m), sc, m, 0);   /* from about E4, a note of the chord */
    for (k = 0; k < rn && n < ANS_N; k++) {
        int note, firm = (rh[k] % 4) == 0 || rd[k] >= 4;   /* a beat, or a long note: the chord's */
        if (k) {
            if (back) {                          /* after a leap: a step back */
                d -= back;
                back = 0;
            } else if (frand() < 0.1f + 0.1f * wild) {   /* a leap, a third to a fifth, onto the chord */
                int dir = frand() < 0.5f ? -1 : 1;
                d = chord_near(d + dir * (2 + irand(3)), sc, m, dir);
                back = dir;
                firm = 1;
            } else
                d += frand() < 0.55f ? -1 : 1;   /* a step (a little more often down: lines fall) */
        }
        if (firm)
            d = chord_near(d, sc, m, 0);
        note = scale_note(d, sc, m);
        while (note < 55)
            note += 12, d += m;
        while (note > 84)
            note -= 12, d -= m;
        resp[n].key = (uint8_t)note;
        resp[n].vel = (uint8_t)(firm ? 82 + irand(20) : 66 + irand(18));   /* (the beats a little stronger) */
        resp[n].on = (uint32_t)((float)rh[k] * step_len);
        resp[n].off = (uint32_t)((float)(rh[k] + rd[k]) * step_len * 0.92f);
        resp[n].state = 0;
        resp[n].firm = (uint8_t)firm;
        last = n++;
    }
    if (n) {                                     /* its last note: onto the chord, a little longer */
        resp[last].key = (uint8_t)scale_note(chord_near(scale_deg(resp[last].key, sc, m), sc, m, -1), sc, m);
        resp[last].firm = 1;
        resp[last].off += (uint32_t)(step_len * 2.0f);
    }
    for (k = 0; k < rn; k++)                     /* (its rhythm, for next time) */
        imp_rh[k] = rh[k], imp_rd[k] = rd[k];
    imp_rn = (uint8_t)rn;
    resp_n = n;
}

static void ans_beat(void)                       /* a beat: an answer waiting starts; improvising, a phrase of its own */
{
    if (ans_state == 1) {
        ans_go();
        imp_rest = 2 + irand(3);
        return;
    }
    if (ans_amt <= 0.5f || ans_state || call_held || (call_n && smp - call_last < (uint32_t)(step_len * 8.0f)))
        return;
    if (call_n)                                  /* (a few notes left from the player, unanswered: let go) */
        call_n = 0;
    if (--imp_rest > 0)
        return;
    imp_build();
    if (resp_n) {
        ans_t0 = smp;
        ans_state = 2;
    }
    {
        const float e = fm_clampf((energy + 1.0f) * 0.5f, 0.0f, 1.0f);   /* the rest after it, in beats */
        imp_rest = (int)(en_bar_len / 4u) * (1 + (frand() < 0.5f)) + (int)((1.0f - e) * 6.0f * frand());
    }
}

static void ans_tick(uint32_t n)                 /* each chunk: is the call over? the answer's notes, on time */
{
    int i, left = 0;
    smp += n;
    if (ans_amt <= 0.0f) {
        call_n = 0;
        return;
    }
    if (!ans_state && call_n >= 2 && !call_held && smp - call_last > (uint32_t)(step_len * 4.0f)) {
        ans_state = 1;                           /* a beat's rest: the call is over */
        if (!en_playing)
            ans_go();
    }
    if (ans_state != 2)
        return;
    for (i = 0; i < resp_n; i++) {
        if (resp[i].state == 0 && smp - ans_t0 >= resp[i].on) {
            for (left = 0; left < i; left++)     /* (one key once: an earlier one on it let go first) */
                if (resp[left].state == 1 && resp[left].key == resp[i].key) {
                    ans_note(resp[left].key, 0);
                    resp[left].state = 2;
                }
            if (!chord_has(resp[i].key)) {      /* (0.8.3) the chord moved under it: a firm note, or a long one a
                                                  * semitone from the chord's, onto the nearest of the chord's */
                int j, near = 0;
                for (j = 1; j <= 2 && !near; j++)
                    near = chord_has(resp[i].key - j) ? -j : chord_has(resp[i].key + j) ? j : 0;
                if (near && (resp[i].firm || ((near == 1 || near == -1) && resp[i].off - resp[i].on > (uint32_t)(step_len * 3.0f)))) {
                    resp[i].key = (uint8_t)(resp[i].key + near);
                }
            }
            ans_note(resp[i].key, 1);
            resp[i].state = 1;
        }
        if (resp[i].state == 1 && smp - ans_t0 >= resp[i].off) {
            ans_note(resp[i].key, 0);
            resp[i].state = 2;
        }
    }
    for (i = 0, left = 0; i < resp_n; i++)
        left += resp[i].state != 2;
    if (!left)
        ans_state = 0;
}

/* (0.7) one set of voices, one sound: the player's lead (lead[], Lead's sound), or a theme's voice (its own sound,
 * never a plucked string: those keep ks[] per player voice). vol: its level; glide_t: its glide (s); bs: its bowed
 * body's resonators; the vibrato's and tremolo's phases, moved once a chunk by render_lead */
static void render_lead_v(uint32_t n, lead_t *vs, int nv, int sd, float vol, float glide_t, float (*bs)[3][2])
{
    int i, kind = LEADS[sd].kind;
    const float g = vol * 0.4f * LEADS[sd].gain, dt = 1.0f + LEADS[sd].detune;
    const int body = kind == LK_BOWED ? LEADS[sd].body : 0;
    static float bodl[BLK], bodr[BLK];
    float trem;
    uint32_t j0;
    const float idec = LEADS[sd].idx_t > 0.0f ? fm_expf(-6.9f / (LEADS[sd].idx_t * EN_SR)) : 1.0f,
                adec = LEADS[sd].ring > 0.0f ? fm_expf(-6.9f / (LEADS[sd].ring * EN_SR)) : 1.0f,
                rdec = fm_expf(-6.9f / (LEADS[sd].rel * EN_SR)), arate = 1.0f / (LEADS[sd].atk * EN_SR * 0.25f + 1.0f),
                ratio = LEADS[sd].ratio, held_sound = LEADS[sd].ring <= 0.0f,
                hdec = kind == LK_PIANO ? fm_expf(-6.9f / (LEADS[sd].ring * 4.0f * EN_SR)) : adec;   /* (a Rhodes held:
                                                                                                    * four times as long) */
    const float tdec = fm_expf(-6.9f / (0.05f * EN_SR)), cdec = fm_expf(-6.9f / (0.045f * EN_SR)),
                a2dec = fm_expf(-6.9f / (0.7f * EN_SR));
    float vib;
    vib = LEADS[sd].vib * sine(lead_vib);
    trem = 0.2f * sine(lead_trem);
    if (body)
        for (j0 = 0; j0 < n; j0++)
            bodl[j0] = bodr[j0] = 0.0f;
    for (i = 0; i < nv; i++) {
        lead_t *v = &vs[i];
        float cph = v->cph, cph2 = v->cph2, mph = v->mph, mph2 = v->mph2, amp = v->amp, amp2 = v->amp2, att = v->att,
              idx = v->idx, idx2 = v->idx2, rel = v->rel, inc = v->inc, blp = v->blp, bhp = v->bhp, chiff = v->chiff;
        float vd, pi, fl = 0.0f, ksg = 0.0f;
        uint32_t j;
        if (!v->on)
            continue;
        inc += (v->inc_t - inc) * (glide_t > 0.0f ? fm_minf((float)n / (glide_t * EN_SR), 1.0f) : 1.0f);
        v->vt += (float)n * (1.0f / EN_SR);      /* the vibrato eases in: nothing for 0.25 s, all of it by 0.85 s */
        vd = 1.0f + vib * fm_clampf((v->vt - 0.25f) * 1.67f, 0.0f, 1.0f);
        pi = inc * vd;
        if (kind == LK_FLUTE)
            fl = 0.25f + 0.4f * lead_tone;       /* the flute's warmth: its self-modulation */
        if (kind == LK_STRING) {                 /* the string's length: the note's (gliding, wavering: the slide) */
            v->ksl = fm_clampf(1.0f / pi - 0.5f, 8.0f, (float)(KS_N - 4));
            ksg = fm_expf(-6.9f / (LEADS[sd].ring * EN_SR * (1.0f / v->ksl))) * 0.5f;   /* the loss a pass: it rings
                                                                                         * LEADS.ring s */
        }
        for (j = 0; j < n; j++) {
            float x, xr, m, l = 0.5f, r = 0.5f;
            switch (kind) {
            case LK_FLUTE: {
                float nz, br;
                x = sine(cph + 4.0f + fl * sine(cph)) + 0.12f * sine(2.0f * cph) + 0.05f * sine(3.0f * cph);
                noise = noise * 1664525u + 1013904223u;
                nz = (float)(int32_t)noise * (1.0f / 2147483648.0f);
                blp += (nz - blp) * 0.28f;       /* the breath: noise, band-passed (~1.5 .. 6 kHz) */
                bhp += (blp - bhp) * 0.06f;
                br = (blp - bhp) * LEADS[sd].breath * (1.0f + 6.0f * chiff);
                x = (x * (1.0f - 0.25f * chiff) + br * (0.6f + 0.4f * x)) * 0.7f;   /* the chiff, a breath riding the tone */
                chiff *= cdec;
                break;
            }
            case LK_STRING: {
                float rp = (float)v->ksw - v->ksl, fr, a, b2;
                int r0;
                if (rp < 0.0f)
                    rp += (float)KS_N;
                r0 = (int)rp;
                fr = rp - (float)r0;
                a = (float)ks[i][r0 & (KS_N - 1)] * (1.0f / 16384.0f);
                b2 = (float)ks[i][(r0 + 1) & (KS_N - 1)] * (1.0f / 16384.0f);
                x = a + (b2 - a) * fr;           /* the string, a period ago (between two samples) */
                v->kslp += (x - v->kslp) * fm_minf(0.6f + 0.35f * lead_tone + LEADS[sd].breath, 0.98f);   /* a gentle
                                                                                         * low-pass in the loop */
                ks[i][v->ksw & (KS_N - 1)] = (int16_t)(fm_clampf((v->kslp + x) * ksg, -1.99f, 1.99f) * 16384.0f);
                v->ksw = (v->ksw + 1) & (KS_N - 1);
                bhp += (x - bhp) * 0.004f;       /* (its DC, out: ~28 Hz) */
                x = (x - bhp) * 3.9f;
                break;
            }
            case LK_PIANO:
                m = sine(mph) * (idx * 0.25f + idx * 0.75f * amp2);   /* the body's index falls to a quarter */
                x = sine(cph + m + 4.0f);
                xr = sine(cph2 + m + 4.0f);
                l = 0.5f + trem, r = 0.5f - trem;
                {                                /* the tine: a short bright clang, an octave and a half of overtones */
                    float t = sine(cph + sine(mph2) * idx2 + 4.0f) * amp2 * 0.35f;
                    x += t;
                    xr += t;
                }
                mph2 += pi * 14.0f;
                if (mph2 >= 1.0f)
                    mph2 -= (float)(int)mph2;
                idx2 *= tdec;
                amp2 *= a2dec;
                cph2 += pi * dt;
                if (cph2 >= 1.0f)
                    cph2 -= 1.0f;
                x = x * l * 3.6f;
                xr = xr * r * 3.6f;
                break;
            case LK_BOWED:
                m = sine(mph) * idx * (0.45f + 0.55f * att);   /* brighter as the bow presses in */
                x = sine(cph + m + 4.0f) * 1.24f;
                xr = sine(cph2 + m + 4.0f) * 1.24f;
                cph2 += pi * dt;
                if (cph2 >= 1.0f)
                    cph2 -= 1.0f;
                break;
            default:
                m = sine(mph) * idx;
                x = sine(cph + m + 4.0f);
                break;
            }
            if (kind != LK_PIANO && kind != LK_BOWED)
                xr = x;
            if ((kind == LK_PIANO || kind == LK_BOWED) && LEADS[sd].breath > 0.0f) {   /* the hammer's knock, the bow's hiss */
                float nz, h;
                noise = noise * 1664525u + 1013904223u;
                nz = (float)(int32_t)noise * (1.0f / 2147483648.0f);
                blp += (nz - blp) * (kind == LK_PIANO ? 0.12f : 0.3f);
                bhp += (blp - bhp) * 0.04f;
                h = (blp - bhp) * LEADS[sd].breath * (kind == LK_PIANO ? 9.0f * chiff : 1.0f + 2.5f * chiff);
                x += h;
                xr += h;
                chiff *= cdec;
            }
            {
                float k = amp * att * rel * g;
                float ol = x * k, orr = xr * k;
                if (body) {                      /* bowed: into the body first (below) */
                    bodl[j] += ol;
                    bodr[j] += orr;
                } else {
                    mixl[j] += ol;
                    mixr[j] += orr;
                    rsend[j] += (ol + orr) * lv_rs;
                    esend[j] += (ol + orr) * lv_es;
                }
            }
            cph += pi;
            mph += pi * ratio;
            if (cph >= 1.0f)
                cph -= 1.0f;
            if (mph >= 1.0f)
                mph -= 1.0f;
            if (kind != LK_STRING)
                amp *= v->held ? hdec : adec;
            idx *= idec;
            att += (1.0f - att) * arate;
            if (!v->held && (held_sound || kind == LK_PIANO))   /* struck sounds ring on; held ones (and a Rhodes,
                                                                 * its damper) fade once let go */
                rel *= rdec;
        }
        v->cph = cph, v->cph2 = cph2, v->mph = mph, v->mph2 = mph2, v->amp = amp, v->amp2 = amp2, v->att = att;
        v->idx = idx, v->idx2 = idx2, v->rel = rel, v->inc = inc, v->blp = blp, v->bhp = bhp, v->chiff = chiff;
        if (kind == LK_STRING) {                 /* the string: done once it has died away */
            int e = ks[i][(v->ksw - 1) & (KS_N - 1)], e2 = ks[i][(v->ksw - 7) & (KS_N - 1)];
            if (v->vt > 0.2f && (e < 0 ? -e : e) + (e2 < 0 ? -e2 : e2) < 7)
                amp = 0.0f;
        }
        if (amp * rel < 0.0005f) {
            v->on = 0;
            if (v->held)
                midi(0x84, v->note, 0);
        }
    }
    if (body) {                                  /* the body: three resonators a side, and a little of the string */
        const float (*bp)[3] = BODY[body - 1];
        int c, k;
        for (c = 0; c < 2; c++) {
            float *b = c ? bodr : bodl, *o = c ? mixr : mixl;
            for (k = 0; k < 3; k++) {
                const float f = FM_TWO_PI * bp[k][0] * (1.0f / EN_SR), q = 1.0f / bp[k][1], gk = bp[k][2] * q * 2.2f;
                float band = bs[c][k][0], low = bs[c][k][1];
                for (j0 = 0; j0 < n; j0++) {     /* Chamberlin's state variable filter: its band */
                    float hi = b[j0] - low - q * band;
                    band += f * hi;
                    low += f * band;
                    o[j0] += band * gk;
                    rsend[j0] += band * gk * lv_rs;
                    esend[j0] += band * gk * lv_es;
                }
                bs[c][k][0] = band;
                bs[c][k][1] = low;
            }
            for (j0 = 0; j0 < n; j0++) {
                o[j0] += b[j0] * 0.3f;
                rsend[j0] += b[j0] * 0.3f * lv_rs;
            }
        }
    }
}


/* (0.7) Themes: a story's melodies as one-shots (Fast and Slow's brothers): "@S;B4:1 A4:.5 r:1 ..." a part, its
 * lead sound S (not a plucked string), each note and how many beats it lasts ("r" a rest); parts joined by '|'
 * play together. Fired: from the next bar, each part its own voice (a lead voice of its own sound, beside the
 * player's), legato within a part (a note into the next, a short glide), moved with Root as stings are. */
#define TH_PARTS 2
#define TH_EV 64
static struct { uint8_t note; uint8_t len; uint16_t at; } th_ev[TH_PARTS][TH_EV];   /* (sixteenths) */
static uint8_t th_n[TH_PARTS], th_i[TH_PARTS], th_snd[TH_PARTS], th_on[TH_PARTS], th_wait, th_play;
static uint16_t th_pos, th_off[TH_PARTS], th_end;
static lead_t thv[TH_PARTS];
static float th_body[TH_PARTS][2][3][2];

static void theme_note(int p, int note, int vel)   /* a part's voice: a new note (legato after the last), or off */
{
    lead_t *v = &thv[p];
    int sd = th_snd[p];
    float fv = (float)vel / 127.0f;
    if (note < 0) {
        if (v->on && v->held) {
            v->held = 0;
            midi(0x86 + (uint32_t)p, v->note, 0);
        }
        return;
    }
    note += 12 * LEADS[sd].oct;
    while (note > 100)
        note -= 12;
    while (note < 28)
        note += 12;
    if (v->on && v->held) {                      /* legato: the bow (the breath) carries on to the next note */
        midi(0x86 + (uint32_t)p, v->note, 0);
        v->inc_t = mtof((float)note) * (1.0f / EN_SR);
        v->note = (uint8_t)note;
        midi(0x96 + (uint32_t)p, (uint32_t)note, (uint32_t)vel);
        return;
    }
    v->on = 1;
    v->held = 1;
    v->key = (uint8_t)note;
    v->note = (uint8_t)note;
    v->cph = v->mph = v->mph2 = 0.0f;
    v->cph2 = 0.37f;
    v->blp = v->bhp = 0.0f;
    v->chiff = 1.0f;
    v->vt = 0.0f;
    v->inc = v->inc_t = mtof((float)note) * (1.0f / EN_SR);
    v->amp = 0.35f + 0.65f * fv;
    v->amp2 = 0.5f * fv + 0.15f;
    v->att = 0.0f;
    {                                            /* (0.8) the one-shots' Tone, not the lead's */
        const float tn = (float)par[P_OSTONE] * 0.01f;
        v->idx = LEADS[sd].idx * (0.3f + 1.4f * tn) * (0.6f + 0.6f * fv) * (1.0f / FM_TWO_PI);
        v->idx2 = (0.4f + 1.2f * tn) * fv * (1.0f / FM_TWO_PI);
    }
    v->rel = 1.0f;
    midi(0x96 + (uint32_t)p, (uint32_t)note, (uint32_t)vel);
}

static void theme_stop(void)
{
    int p;
    for (p = 0; p < TH_PARTS; p++)
        theme_note(p, -1, 0);
    th_play = th_wait = 0;
}

static void theme_fire(const char *t, int mv)    /* read the theme; it starts on the next bar */
{
    int p = 0;
    theme_stop();
    for (p = 0; p < TH_PARTS; p++)
        th_n[p] = 0;
    p = 0;
    th_end = 0;
    while (t && *t && *t != '#' && p < TH_PARTS) {   /* ('#': the next act's) */
        uint16_t at = 0;
        int n = 0, sd = 2;
        if (*t == '@')
            t++;
        if (*t >= '0' && *t <= '9')
            sd = *t++ - '0';
        if (sd >= EN_NLEAD || LEADS[sd].kind == LK_STRING)
            sd = 2;                              /* (the flute: never a plucked string here) */
        th_snd[p] = (uint8_t)sd;
        if (*t == ';')
            t++;
        while (*t && *t != '|' && *t != '#') {
            int pc = -1, o = 0, rest = 0;
            float b = 0.0f, frac = 0.0f, dv = 1.0f;
            while (*t == ' ')
                t++;
            if (!*t || *t == '|' || *t == '#')
                break;
            if (*t == 'r') {
                rest = 1;
                t++;
            } else {
                if ((pc = note_pc(&t)) < 0)
                    break;
                while (*t >= '0' && *t <= '9')
                    o = o * 10 + (*t++ - '0');
            }
            if (*t == ':') {                     /* its beats: 1, 2.5, .5 */
                t++;
                while (*t >= '0' && *t <= '9')
                    b = b * 10.0f + (float)(*t++ - '0');
                if (*t == '.') {
                    t++;
                    while (*t >= '0' && *t <= '9') {
                        dv *= 0.1f;
                        frac += (float)(*t++ - '0') * dv;
                    }
                }
                b += frac;
            } else
                b = 1.0f;
            {
                int len = (int)(b * 4.0f + 0.5f);   /* (in sixteenths) */
                if (len < 1)
                    len = 1;
                if (!rest && n < TH_EV) {
                    int note = 12 * (o + 1) + pc + mv;
                    th_ev[p][n].note = (uint8_t)(note < 24 ? 24 : note > 108 ? 108 : note);
                    th_ev[p][n].len = (uint8_t)(len > 255 ? 255 : len);
                    th_ev[p][n].at = at;
                    n++;
                }
                at = (uint16_t)(at + len);
            }
            while (*t && *t != ' ' && *t != '|' && *t != '#')
                t++;
        }
        th_n[p] = (uint8_t)n;
        if (at > th_end)
            th_end = at;
        if (*t == '|')
            t++;
        p++;
    }
    for (p = 0; p < TH_PARTS; p++)
        th_i[p] = th_on[p] = 0;
    th_wait = th_end > 0;                        /* (0.8.1: the loop no longer restarted for it: it waits, or joins) */
}
static int theme_busy(void) { return th_wait || th_play; }
int en_theme_busy(void) { return th_play ? 2 : th_wait ? 1 : 0; }   /* (0.8.1: 1 waiting, 2 playing) */   /* (0.8: the One-shots page lights the one playing) */

static void theme_bar(void)                      /* every sixteenth: the theme's notes on time */
{
    int p;
    if (th_wait && (th_join || (loop16 >= 0 ? loop16 == 0 : bar_pos == 0))) {
        th_wait = 0;                             /* (the loop's start; or now, where the loop is: its notes from there) */
        th_play = 1;
        th_pos = 0;
        if (th_join && loop16 > 0 && th_end) {
            th_pos = (uint16_t)(loop16 % th_end);
            for (p = 0; p < TH_PARTS; p++)
                while (th_i[p] < th_n[p] && th_ev[p][th_i[p]].at < th_pos)
                    th_i[p]++;
        }
        th_join = 0;
    }
    if (!th_play)
        return;
    for (p = 0; p < TH_PARTS; p++) {
        if (th_on[p] && th_pos >= th_off[p]) {   /* a note's end: off, unless the next starts now (legato) */
            th_on[p] = 0;
            if (!(th_i[p] < th_n[p] && th_ev[p][th_i[p]].at == th_pos))
                theme_note(p, -1, 0);
        }
        if (th_i[p] < th_n[p] && th_ev[p][th_i[p]].at == th_pos) {
            theme_note(p, th_ev[p][th_i[p]].note, 100);
            th_off[p] = (uint16_t)(th_pos + th_ev[p][th_i[p]].len);
            th_on[p] = 1;
            th_i[p]++;
        }
    }
    if (++th_pos > th_end)
        theme_stop();
}

static void theme_render(uint32_t n)
{
    int p;
    const float tone = lead_tone;
    lead_tone = (float)par[P_OSTONE] * 0.01f;    /* (0.8) the one-shots' own: Level, Space (reverb), Echo, Tone */
    lv_rs = (float)par[P_OSSPACE] * 0.0075f;
    lv_es = (float)par[P_OSECHO] * 0.01f;
    for (p = 0; p < TH_PARTS; p++)
        if (thv[p].on)
            render_lead_v(n, &thv[p], 1, th_snd[p], (float)par[P_OSLEVEL] * 0.0119f, 0.035f, th_body[p]);
    lead_tone = tone;
    lv_rs = 0.3f;
    lv_es = 0.33f;
}

static void render_lead(uint32_t n)
{
    lead_vib += (float)n * (5.2f / EN_SR);       /* one slow vibrato for the chunk */
    if (lead_vib >= 1.0f)
        lead_vib -= 1.0f;
    lead_trem += (float)n * (4.6f / EN_SR);      /* the Rhodes' tremolo, side to side */
    if (lead_trem >= 1.0f)
        lead_trem -= 1.0f;
    render_lead_v(n, lead, NLEAD, par[P_LSOUND], lead_vol, lead_glide_t, body_s);
    theme_render(n);
}

/* ------------------------------------------------------------- punch-in --- */
/* The whole sound, before MASTER. A ring of the last moment (mono, 22 kHz, 0.74 s) for Reverse and Stutter: it stops
 * recording while either plays, so what they play stays put. Filter: a resonant low-pass (Zavalishin's TPT SVF) swept
 * from wide open down to its cutoff. Phaser: four all-passes a side, swept by a slow LFO */
#define RB_N 16384
static int16_t rb[RB_N] EN_POOL;
static int rb_w;
static float rb_acc, rb_pos, rb_start, rb_len, fxw[EN_NFX];
static float fl_s[2][2];
volatile uint8_t en_fx_on;
volatile float en_fade = 1.0f;

static float rb_read(float p)                    /* the ring at p (22 kHz samples, any value), between two samples */
{
    int i;
    float f;
    while (p < 0.0f)
        p += (float)RB_N;
    i = (int)p;
    f = p - (float)i;
    i %= RB_N;
    return ((float)rb[i] + ((float)rb[(i + 1) % RB_N] - (float)rb[i]) * f) * (1.0f / 32768.0f);
}

static void fx_chunk(uint32_t n)                 /* once a chunk: the fades, the grab, the filter's and phaser's settings */
{
    int i, grab = (fx_mask & 2u) != 0;            /* (0.7: Stutter alone holds the moment; the Harmony plays on) */
    const float k = fm_minf((float)n / (0.03f * EN_SR), 1.0f);
    uint8_t on = 0;
    for (i = 0; i < EN_NFX; i++) {
        fxw[i] += ((fx_mask >> i & 1u ? 1.0f : 0.0f) - fxw[i]) * k;
        if (fxw[i] > 0.01f)
            on |= (uint8_t)(1u << i);
    }
    en_fx_on = on;
    if (grab && !rb_held) {                      /* Reverse or Stutter: the moment just gone, held */
        static const float SLICE[4] = {1.0f, 0.5f, 0.25f, 0.125f};
        float beat = 60.0f / (float)par[P_TEMPO] * 22050.0f;
        rb_held = 1;
        rb_start = (float)rb_w;
        rb_len = beat * SLICE[par[P_STRATE] & 3];
        rb_pos = rb_start - rb_len;
        if (rb_len > (float)(RB_N / 2 - 64))      /* at most half the ring: room to record the next while it plays */
            rb_len = (float)(RB_N / 2 - 64);
    } else if (!grab && fxw[FX_STUTTER] < 0.01f) {
        rb_held = 0;                             /* both faded out: recording again */
        rb_rec = rb_regrab = 0;
    }
}

static int rb_odd;
static float hv_last;                            /* (0.8) Harmony's last voices: Shimmer feeds them back into the ring */
static float st_rate = 1.0f;                     /* (0.8) Stutter's playback rate: Pitch moves it each repeat */
static void fx_sample(float *l, float *r)        /* one frame of the whole sound through the punch-ins */
{
    float x = *l, y = *r;
    if (!rb_held)
        st_rate = 1.0f;                          /* (a new moment held: at its own pitch first) */
    if (!rb_held || rb_rec) {                    /* the ring: mono, every other frame (a pair's average) */
        rb_acc += 0.5f * (x + y) + hv_last * (float)par[P_HVSHIM] * 0.0065f;
        if (rb_odd ^= 1) {
        } else {
            float v = fm_clampf(rb_acc * 0.5f, -1.0f, 1.0f);
            rb[rb_w] = (int16_t)(v * 32767.0f);
            rb_w = rb_w + 1 >= RB_N ? 0 : rb_w + 1;
            rb_acc = 0.0f;
            if (rb_rec && ++rb_rec_n >= (int)rb_len + 8) {   /* a fresh moment recorded: take it at the next seam */
                rb_rec = 0;
                rb_regrab = 1;
            }
        }
    }
    if (fxw[FX_REVERSE] > 0.0005f) {             /* (0.7: Harmony, in Reverse's place) the whole sound again, a fifth
                                                  * or an octave up (Interval: fifth, octave, both), over it: two
                                                  * read heads in the ring, a window apart, faded in turn */
        static const float RATIO[3][2] = {{1.4983f, 0.0f}, {2.0f, 0.0f}, {1.4983f, 2.0f}};
        static float hd[2];
        const int iv = par[P_RVLEN] < 0 ? 0 : par[P_RVLEN] > 2 ? 2 : par[P_RVLEN];
        const float W = 1100.0f, w = fxw[FX_REVERSE] * (float)par[P_RVMIX] * 0.01f;
        float wet = 0.0f;
        int h;
        for (h = 0; h < 2; h++) {
            float r = RATIO[iv][h], d, a, b2, ga, gb;
            if (r <= 0.0f)
                continue;
            hd[h] -= 0.5f * (r - 1.0f);          /* (the ring at half rate: a head at 0.5 r a frame, the writing at 0.5) */
            if (hd[h] < 1.0f)
                hd[h] += W;
            d = hd[h];
            a = rb_read((float)rb_w - d);
            d += W * 0.5f;
            if (d >= W)
                d -= W;
            b2 = rb_read((float)rb_w - (d < 1.0f ? d + W : d));
            ga = 1.0f - fm_fabsf(2.0f * hd[h] / W - 1.0f);
            gb = 1.0f - ga;
            wet += (a * ga + b2 * gb) * (h ? 0.6f : 0.8f);
        }
        hv_last = fm_flush(wet * fxw[FX_REVERSE]);
        {                                        /* (0.8) Width: the right side's voices up to 15 ms behind the left's */
            static float hw[1024];
            static int hw_i;
            int dl = (int)((float)par[P_HVWIDTH] * 6.6f), j;
            hw[hw_i] = wet;
            j = hw_i - dl;
            if (j < 0)
                j += 1024;
            hw_i = (hw_i + 1) & 1023;
            x += wet * w;
            y += hw[j] * w;
        }
    } else {
        hv_last = 0.0f;
    }
    if (fxw[FX_STUTTER] > 0.0005f) {             /* the held moment, again */
        float wet, w;
        {
            rb_pos += 0.5f * st_rate;
            if (rb_pos >= rb_start) {            /* a repeat: (0.8) Pitch moves the next one, a tape slowing or rising */
                rb_pos -= rb_len;
                st_rate = fm_clampf(st_rate * fm_exp2f((float)par[P_STPITCH] * 0.0025f), 0.35f, 2.8f);
                if (rb_regrab)
                    rb_regrab = 0, rb_start = (float)rb_w, rb_pos = rb_start - rb_len, st_rate = 1.0f;
            }
            w = fxw[FX_STUTTER] * (float)par[P_STMIX] * 0.01f;
        }
        {                                        /* a short fade at the seam, so the loop does not click */
            float d = rb_start - rb_pos, e = d < 0.0f ? d + rb_len : d, fade = fm_minf(fm_minf(e, rb_len - e) / 220.0f, 1.0f);
            float on = rb_len * (0.15f + 0.0085f * (float)par[P_STLEN]), el = rb_len - e;   /* (0.8) Length: the
                                                  * slice's share that sounds, the rest silent */
            if (on < rb_len - 1.0f)
                fade = fm_minf(fade, fm_clampf((on - el) / 220.0f, 0.0f, 1.0f));
            wet = rb_read(rb_pos) * fade;
        }
        x = x * (1.0f - w) + wet * w;
        y = y * (1.0f - w) + wet * w;
    }
    if (fxw[FX_FILTER] > 0.0005f) {              /* the filter: wide open, down to Cutoff as it comes in */
        static float g_, k_, h_;
        static uint32_t cnt;
        int c;
        float in[2] = {x, y};
        static float sw_ph;
        const int ty = par[P_FLTYPE];
        if (!(cnt++ & 31u)) {                    /* (0.8) Sweep: the cutoff moves by itself, 0.05 .. 2 times a second;
                                                  * Type: high cut (low-pass), low cut (high-pass), a band */
            float cut = 0.01f * (float)par[P_FLCUT], amt, fc, t;
            if (par[P_FLSWEEP] > 0) {
                sw_ph += 32.0f / EN_SR * 0.05f * fm_exp2f((float)par[P_FLSWEEP] * 0.0532f);
                if (sw_ph >= 1.0f)
                    sw_ph -= 1.0f;
                cut = fm_clampf(cut + 0.3f * fm_sinf(FM_TWO_PI * sw_ph), 0.0f, 1.0f);
            }
            amt = fxw[FX_FILTER] * (1.0f - cut);
            fc = ty == 1 ? 25.0f * fm_exp2f(amt * 8.5f) : 18000.0f * fm_exp2f(-amt * (ty == 2 ? 5.0f : 7.0f));
            t = FM_PI * fc / EN_SR;
            g_ = t * (1.0f + t * t * 0.3333f + t * t * t * t * 0.1333f);    /* tan(t) */
            k_ = 2.0f - 1.85f * 0.01f * (float)par[P_FLRES];
            h_ = 1.0f / (1.0f + g_ * (g_ + k_));
        }
        for (c = 0; c < 2; c++) {
            float hp = (in[c] - (k_ + g_) * fl_s[c][0] - fl_s[c][1]) * h_, bp = g_ * hp + fl_s[c][0], lp;
            fl_s[c][0] = fm_flush(g_ * hp + bp);
            lp = g_ * bp + fl_s[c][1];
            fl_s[c][1] = fm_flush(g_ * bp + lp);
            in[c] = ty == 1 ? hp : ty == 2 ? in[c] + (bp * 1.6f - in[c]) * fxw[FX_FILTER] : lp;
        }
        x = in[0];
        y = in[1];
    } else {
        fl_s[0][0] = fl_s[0][1] = fl_s[1][0] = fl_s[1][1] = 0.0f;
    }
    if (fxw[FX_SCATTER] > 0.0005f) {             /* StoryBox 0.5's Scatter, a sparkle: three grains of the last moment,
                                                  * an octave up (now and then two), short (Size: 25 .. 125 ms), each
                                                  * somewhere in the stereo field, high-passed, over the sound */
        static float g_pos[3], g_step[3], g_pan[3], g_hp[3];
        static int32_t g_left[3], g_len[3], g_wait[3];
        float wl = 0.0f, wr = 0.0f, w;
        int g;
        for (g = 0; g < 3; g++) {
            float e, t, v;
            if (g_wait[g] > 0) {
                g_wait[g]--;
                continue;
            }
            if (g_left[g] <= 0) {                /* a new grain, after a little gap */
                g_len[g] = (int32_t)((0.025f + 0.001f * (float)par[P_SCSIZE]) * EN_SR);
                g_left[g] = g_len[g];
                g_step[g] = 0.5f * fm_exp2f(0.01f * (float)par[P_SCPITCH]) * (frand() < 0.75f ? 1.0f : 2.0f);
                                                  /* (the ring is at half rate: 1 an octave up; 0.8: Pitch, 0.5 .. 1) */
                g_pos[g] = (float)rb_w - (float)g_len[g] * g_step[g] - 200.0f - frand() * 6000.0f;
                g_pan[g] = frand();
                g_wait[g] = (int32_t)(frand() * 0.3f * fm_exp2f(-0.05f * (float)par[P_SCSPREAD]) * EN_SR);
                                                  /* (0.8: Spread, a few .. dense) */
                continue;
            }
            t = (float)(g_len[g] - g_left[g]) / (float)g_len[g];
            e = 0.5f - 0.5f * fm_cosf(FM_TWO_PI * t);    /* a soft window */
            v = rb_read(g_pos[g]);
            g_hp[g] += (v - g_hp[g]) * 0.08f;    /* (the low end out: a sparkle) */
            v = (v - g_hp[g]) * e;
            wl += v * (1.0f - g_pan[g]);
            wr += v * g_pan[g];
            g_pos[g] += g_step[g];
            g_left[g]--;
        }
        w = fxw[FX_SCATTER] * 0.012f * (float)par[P_SCMIX];
        x += wl * w;
        y += wr * w;
    }
    *l = x;
    *r = y;
}

/* Surf: an ocean's rush, waves of noise (each side its own) through two low-passes that open as a wave rises; a wave
 * every 9 s or so, the right side a little behind the left */
static float surf_lp[2][2], surf_ph, surf_lvl;
/* a fly, flying past (Fly's Buzz sting): a buzzing tone (FM, near a saw) about a note, its wings fluttering it,
 * wandering, a little higher coming than going; swelling and fading over 2.6 s, from the left to the right */
static float buzz_t = -1.0f, buzz_f, buzz_ph, buzz_mph;
static void buzz_start(int note)
{
    buzz_t = 0.0f;
    buzz_f = mtof((float)note);
}
static void render_buzz(uint32_t n)
{
    const float T = 2.6f;
    uint32_t j;
    float t, env, f, pan, w;
    if (buzz_t < 0.0f)
        return;
    t = buzz_t / T;
    env = fm_sinf(FM_TWO_PI * 0.5f * fm_clampf(t, 0.0f, 1.0f));
    env *= env * 0.22f;
    w = 0.03f * fm_sinf(FM_TWO_PI * 1.3f * buzz_t) + 0.02f * fm_sinf(FM_TWO_PI * 0.7f * buzz_t + 1.0f);   /* wandering */
    pan = fm_clampf(t, 0.0f, 1.0f);
    for (j = 0; j < n; j++) {
        float fl = 1.0f + 0.035f * sine(buzz_t * 23.0f + (float)j * (23.0f / EN_SR));   /* the wings */
        float x;
        f = buzz_f * (1.0f + w + 0.05f * fm_cosf(FM_PI * t)) * fl;
        buzz_mph += f * (1.0f / EN_SR);
        if (buzz_mph >= 1.0f)
            buzz_mph -= 1.0f;
        buzz_ph += f * (1.0f / EN_SR);
        if (buzz_ph >= 1.0f)
            buzz_ph -= 1.0f;
        x = sine(buzz_ph + 4.0f + 0.45f * sine(buzz_mph)) * env;   /* (index ~2.8 rad: buzzy) */
        mixl[j] += x * (1.0f - pan);
        mixr[j] += x * pan;
        rsend[j] += x * 0.2f;
    }
    buzz_t += (float)n * (1.0f / EN_SR);
    if (buzz_t > T)
        buzz_t = -1.0f;
}

static void render_surf(uint32_t n)
{
    uint32_t j;
    int c;
    const float want = (float)par[P_SURF] * 0.01f;
    surf_lvl += (want - surf_lvl) * fm_minf((float)n / (3.0f * EN_SR), 1.0f);   /* (in and out over seconds) */
    if (surf_lvl < 0.0005f)
        return;
    surf_ph += (float)n * (1.0f / (9.3f * EN_SR));
    if (surf_ph >= 1.0f)
        surf_ph -= 1.0f;
    for (c = 0; c < 2; c++) {
        float sp = surf_ph + (c ? 0.91f : 0.0f), w, a, k, l1 = surf_lp[c][0], l2 = surf_lp[c][1];
        if (sp >= 1.0f)                          /* (the right a little behind: sine() takes 0 .. 1) */
            sp -= 1.0f;
        w = 0.5f + 0.5f * sine(sp);
        float *o = c ? mixr : mixl;
        w = w * w;                               /* a wave: rising slowly, falling back */
        k = 0.012f + 0.09f * w;                  /* (~90 Hz .. 750 Hz) */
        a = surf_lvl * (0.15f + 0.85f * w) * 0.9f;
        for (j = 0; j < n; j++) {
            float nz;
            noise = noise * 1664525u + 1013904223u;
            nz = (float)(int32_t)noise * (1.0f / 2147483648.0f);
            l1 += (nz - l1) * k;
            l2 += (l1 - l2) * k;
            o[j] += l2 * a;
            rsend[j] += l2 * a * 0.25f;
        }
        surf_lp[c][0] = l1;
        surf_lp[c][1] = l2;
    }
}

/* StoryBox 0.4: each instrument's own effect. A track is rendered alone (the mix so far set aside, put back after)
 * so its effect touches only it. Shimmer: the pad an octave up (rectified: its frequency doubled), into the reverb.
 * Drive: the bass through tanh, as loud at its middle. Punch (0.7, Crush's place): the drums' hits forward */
static float aside_l[BLK], aside_r[BLK], sh_dc, sh_lp;
static void alone_begin(uint32_t n)
{
    uint32_t j;
    for (j = 0; j < n; j++) {
        aside_l[j] = mixl[j], aside_r[j] = mixr[j];
        mixl[j] = mixr[j] = 0.0f;
    }
}
static void alone_end(uint32_t n)
{
    uint32_t j;
    for (j = 0; j < n; j++)
        mixl[j] += aside_l[j], mixr[j] += aside_r[j];
}
static void shimmer(uint32_t n)
{
    uint32_t j;
    const float a = (float)par[P_SHIMMER] * 0.018f;
    if (a <= 0.0f)
        return;
    for (j = 0; j < n; j++) {
        float x = fm_fabsf(mixl[j] + mixr[j]);
        sh_dc += (x - sh_dc) * 0.002f;
        sh_lp += (x - sh_dc - sh_lp) * 0.3f;
        rsend[j] += sh_lp * a;
    }
}
static void drive(uint32_t n)
{
    uint32_t j;
    const float d = (float)par[P_DRIVE] * 0.01f, k = 1.0f + 7.0f * d, norm = 0.4f / fm_tanhf(0.4f * k);
    if (d <= 0.0f)
        return;
    for (j = 0; j < n; j++) {
        mixl[j] = fm_tanhf(mixl[j] * k) * norm;
        mixr[j] = fm_tanhf(mixr[j] * k) * norm;
    }
}
static float pu_fast, pu_slow;
static void crush(uint32_t n)                    /* (0.7: Punch, in Crush's place) the drums' hits pushed forward: a
                                                  * fast follower against a slow one finds each attack and lifts it,
                                                  * the tail held back a little, a soft clip rounding the peaks */
{
    uint32_t j;
    const float c = (float)par[P_CRUSH] * 0.01f, fa = 0.25f, fr = 0.0012f, sa = 0.0035f, sr = 0.00015f;
    if (c <= 0.0f)
        return;
    for (j = 0; j < n; j++) {
        float x = fm_fabsf(mixl[j]) + fm_fabsf(mixr[j]), tr, g;
        pu_fast += (x - pu_fast) * (x > pu_fast ? fa : fr);
        pu_slow += (x - pu_slow) * (x > pu_slow ? sa : sr);
        tr = fm_clampf((pu_fast - pu_slow) / (pu_slow + 0.02f), 0.0f, 3.0f) * (1.0f / 3.0f);   /* 0..1: an attack */
        g = (1.0f + 1.6f * c * tr) * (1.0f - 0.3f * c * (1.0f - tr)) * (1.0f + 0.5f * c);
        mixl[j] = fm_tanhf(mixl[j] * g * 0.9f) * 1.12f;
        mixr[j] = fm_tanhf(mixr[j] * g * 0.9f) * 1.12f;
    }
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    const float rg = rev_send * rev_in_gain * (1.0f - 0.25f * arcm), eg = echo_send;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
    fx_chunk(n);
    render_pad(n);
    shimmer(n);                                  /* (the mix is the pad's, so far) */
    alone_begin(n);
    render_bass(n);
    drive(n);
    alone_end(n);
    render_bells(n);
    alone_begin(n);
    render_drums(n);
    crush(n);
    alone_end(n);
    render_lead(n);
    render_surf(n);
    render_buzz(n);
    for (j = 0; j < n; j++) {
        float wl, wr, l, r, el, er, xl, xr;
        echoes(esend[j] * eg, &el, &er, &xl, &xr);
        reverb((rsend[j] * rg + (el + er + xl + xr) * 0.35f * rev_send) * 0.8f, &wl, &wr);
        l = (mixl[j] + 0.75f * el + 0.3f * xl + 1.3f * wl) * 0.36f;
        r = (mixr[j] + 0.75f * er + 0.3f * xr + 1.3f * wr) * 0.36f;
        fx_sample(&l, &r);                       /* the punch-ins */
        l *= gain * en_fade;
        r *= gain * en_fade;
        out[2 * j] = (int32_t)(fm_tanhf(l) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r) * 8300000.0f);
    }
}

/* the arc: a slow tide, minutes long, between sparse (-1) and full (+1); Motion sets how far it goes */
static float arc, arc_tgt;
static uint32_t arc_left;

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    {                                            /* the mutes and the breakdown: each layer's gain, quickly (40 ms) */
        int l;
        const float k = fm_minf((float)n / (0.04f * EN_SR), 1.0f);
        for (l = 0; l < 4; l++) {
            float t = mute_mask >> l & 1u ? 0.0f : 1.0f;
            if (bd && (l == 2 || (l == 3 && bd_ret != 2)))
                t = 0.0f;
            mg[l] += (t - mg[l]) * k;
        }
    }
    mods_step(n);
    heads_step(n);
    if (arc_left <= n) {
        float r = frand() * 2.0f - 1.0f;
        arc_tgt = r * (0.6f + 0.4f * frand()) + (r > 0.0f ? 0.25f : -0.25f);   /* away from the middle */
        arc_left = (uint32_t)((50.0f + 100.0f * frand()) * EN_SR);
    } else {
        arc_left -= n;
    }
    if (en_playing)
        arc += (fm_clampf(arc_tgt, -1.0f, 1.0f) - arc) * (float)n * (1.0f / (35.0f * EN_SR));
    arcm = fm_clampf(arc * motion + 0.45f * energy, -1.0f, 1.0f);   /* Energy leans the arc toward sparse or full */
    en_arc = arcm;
    /* Freeze (StoryBox 0.5): the tail held long (not forever), darkening less; a third of what plays still reaches the
     * reverb (it was shut: the notes over it went dry, the held tail dull: muffled) */
    pl_decay += ((en_frozen ? 0.995f : pl_decay_t) - pl_decay) * 0.002f;
    pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
    rev_in_gain += ((en_frozen ? 0.35f : 1.0f) - rev_in_gain) * 0.01f;
    pl_dk += ((en_frozen ? 0.8f : 0.45f) - pl_dk) * 0.01f;
    if (en_playing && !en_frozen && !change_pending && !prog_on()) {
        drift_left -= (float)n;
        if (!fifth_done && drift_left < 0.5f * drift_total) {
            fifth_done = 1;                      /* halfway: the bass may lean on the fifth */
            if (frand() < (float)par[P_FIFTH] * 0.01f && chord_nt >= 3)
                bass_to(chord_pcs[2]);
        }
        if (drift_left <= 0.0f)
            change_pending = 1;
    }
    en_drift = drift_total > 0.0f ? 1.0f - fm_clampf(drift_left / drift_total, 0.0f, 1.0f) : 0.0f;
    if (prog_on() && prog_total > 0)             /* a progression: how far through this step's bars */
        en_drift = fm_clampf(((float)(prog_total - prog_left) + (float)bar_pos / (float)en_bar_len) / (float)prog_total,
                             0.0f, 1.0f);
    for (i = 0; i < EN_NLOOP; i++)
        en_loop_glow[i] *= 0.985f;
    for (i = 0; i < EN_NDRUM; i++)
        en_drum_glow[i] *= 0.95f;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    drain();
    if (en_ext && ++ext_age > 100u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
        }
        if (k > n)
            k = n;
        {
            int32_t w = drums_wait();            /* a swung hit lands on its sample */
            if (w > 0 && (uint32_t)w < k)
                k = (uint32_t)w;
        }
        if (timed)
            pulse_left -= (float)k;
        control(k);
        ans_tick(k);
        render_chunk(out, k, gain);
        drums_wait_step((int32_t)k);
        out += 2 * k;
        n -= k;
    }
}
