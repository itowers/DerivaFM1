/* SPDX-License-Identifier: GPL-3.0-only */
/* DERIVA: a generative groovebox for electronic music on the M-VAVE FM-1.
 *
 * Six tracks of up to 64 steps (kick, snare/clap, hat, perc, bass, synth). Each of 19 styles knows where its genre
 * puts the notes (a chance per step: 100 = anchor, 0 = out of the idiom); DENS and CAOS redraw the pattern from
 * that knowledge, and every step stays editable by hand.
 *
 *   white keys       the 16 steps of the page shown, of the selected track (they switch on release). Hold a step and
 *                    turn: KNOB1 its chance, KNOB2 its repeats, KNOB3 its filter (decay on drums), KNOB4 its level,
 *                    ALGORITHM its pitch (bass, synth)
 *   black keys       OP1-OP6: the tracks (with SEL: generate again, FX: mute, EDIT: lock); then pages 1-4 of the
 *                    steps (1-16 .. 49-64) and x2 (each track twice as long, the copy after it)
 *   FX (held)        the effects on the white keys, with a guide on the screen: stutter 1/16 1/32, tape stop,
 *                    bitcrush, echo and reverb throws, gate, low and high sweeps, break, no bass, build-up, fill,
 *                    loops 1/4 1/8 1/16
 *   PRESETS style (in tempo order; while playing it comes in at the next bar, the tempo glides there)
 *   SELECT tempo     ALGORITHM rotate the track (or a held step's pitch, or a held song part's bars)
 *   ENV / LFO / GLO  knob pages: DENS CAOS FILT ECO | SOM NIVL PASS EUCL | TOM ESCL SWNG CORT | (GLO again) REVB DUCK FDBK MIDI
 *   PLAY  REC generate all  ARP vary (held: keyboard)  SEQ vary by itself (held: song)  HOME rings/grid (held: undo)
 *   SAVE (stopped; held + OCT-/OCT+: project)  OCT-/+ pattern A-D
 *
 * The screen: the rings (one per track) with the four knobs in the corners while playing, the grid to edit.
 * Integers only (no FPU on the chip). audio() does the sequencing and the sound, loop() the panel and the screen. */
#include "fm1.h"

#define NT 6
#define NS 16    /* steps a page, a bar */
#define NSMAX 64 /* steps a track at most: four pages */
#define NSTYLE 19
enum { T_KICK,
       T_SNARE,
       T_HAT,
       T_PERC,
       T_BASS,
       T_LEAD };
enum { SN_SNARE,
       SN_CLAP };
enum { PC_OHAT,
       PC_RIM,
       PC_COWBELL,
       PC_TOM };
enum { BS_SAW,
       BS_SUB,
       BS_808,
       BS_SQUARE,
       BS_REESE,
       BS_FM };
enum { LD_STAB,
       LD_PLUCK,
       LD_PAD,
       LD_BELL,
       LD_CHORD };
/* the sound of each track: 0 is the style's, then these */
static const uint8_t NVOICE[NT] = {4, 4, 4, 5, 7, 6};
static const char *const VOICE_NAME[NT][7] = {
  {"EST", "808", "909", "LOFI"},
  {"EST", "SNR", "CLP", "LOFI"},
  {"EST", "CURT", "LONG", "LOFI"},
  {"EST", "OHT", "RIM", "COW", "TOM"},
  {"EST", "SAW", "SUB", "808", "SQR", "REES", "FM"},
  {"EST", "STAB", "PLCK", "PAD", "SINO", "ACRD"},
};

/* ---- what each style knows: the chance (%) of each step of a bar sounding, per track */
typedef struct {
  const char *name;
  uint8_t bpm, swing, scale, root, snare, perc, bass, lead, kick_ms;
  uint8_t canon[NT][NS];
} style_t;

static const style_t STYLE[NSTYLE] = {
  {"HOUSE", 124, 10, 1, 9, SN_CLAP, PC_OHAT, BS_SAW, LD_STAB, 45,
   {{100, 0, 0, 0, 100, 0, 0, 6, 100, 0, 0, 0, 100, 0, 12, 0},
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 10},
    {50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50},
    {0, 0, 95, 0, 0, 0, 95, 0, 0, 0, 95, 10, 0, 0, 95, 0},
    {15, 0, 80, 40, 0, 0, 80, 40, 0, 0, 80, 50, 0, 0, 80, 30},
    {50, 0, 0, 60, 0, 0, 50, 0, 0, 0, 50, 30, 0, 0, 40, 0}}},
  {"TECHNO", 132, 0, 2, 5, SN_CLAP, PC_RIM, BS_SUB, LD_STAB, 35,
   {{100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 8},
    {0, 0, 0, 0, 60, 0, 0, 0, 0, 0, 0, 0, 80, 0, 0, 0},
    {55, 55, 100, 55, 55, 55, 100, 55, 55, 55, 100, 55, 55, 55, 100, 55},
    {0, 15, 0, 50, 0, 0, 30, 0, 0, 30, 0, 50, 0, 0, 40, 0},
    {10, 75, 75, 75, 10, 75, 75, 75, 10, 75, 75, 75, 10, 75, 75, 75},
    {30, 0, 0, 40, 0, 0, 0, 30, 0, 0, 40, 0, 0, 30, 0, 0}}},
  {"FUNK BR", 130, 0, 0, 2, SN_SNARE, PC_COWBELL, BS_808, LD_PLUCK, 45,
   {{100, 0, 0, 0, 0, 0, 25, 0, 90, 0, 0, 30, 0, 0, 15, 0},
    {0, 0, 0, 100, 0, 0, 100, 0, 0, 0, 50, 15, 100, 0, 40, 0},
    {60, 10, 70, 10, 60, 10, 50, 10, 60, 10, 70, 10, 60, 10, 70, 10},
    {0, 0, 40, 0, 0, 40, 0, 0, 0, 50, 0, 0, 0, 40, 0, 40},
    {100, 0, 0, 30, 0, 0, 40, 0, 80, 0, 0, 40, 0, 0, 30, 0},
    {50, 0, 0, 60, 0, 0, 60, 0, 30, 0, 40, 0, 60, 0, 30, 0}}},
  {"DNB", 172, 0, 0, 4, SN_SNARE, PC_RIM, BS_REESE, LD_PAD, 30,
   {{100, 0, 0, 10, 0, 0, 20, 0, 0, 0, 100, 0, 0, 15, 0, 0},
    {0, 0, 0, 0, 100, 0, 0, 20, 0, 20, 0, 0, 100, 0, 0, 25},
    {90, 30, 90, 30, 80, 30, 90, 30, 90, 30, 80, 30, 80, 30, 90, 30},
    {0, 0, 0, 30, 0, 0, 0, 30, 0, 0, 0, 30, 0, 0, 20, 0},
    {90, 0, 0, 30, 0, 0, 40, 0, 0, 0, 70, 0, 0, 30, 0, 0},
    {60, 0, 0, 0, 0, 0, 20, 0, 40, 0, 0, 0, 0, 0, 20, 0}}},
  {"TRAP", 140, 0, 0, 1, SN_CLAP, PC_OHAT, BS_808, LD_PLUCK, 45,
   {{100, 0, 0, 25, 0, 0, 0, 50, 0, 0, 60, 0, 0, 30, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 0, 10, 0, 15},
    {100, 40, 100, 40, 100, 40, 100, 40, 100, 40, 100, 40, 100, 40, 100, 40},
    {0, 0, 0, 0, 0, 0, 30, 0, 0, 0, 0, 10, 0, 0, 40, 0},
    {100, 0, 0, 25, 0, 0, 0, 50, 0, 0, 60, 0, 0, 30, 0, 0},
    {50, 0, 0, 40, 0, 0, 50, 0, 30, 0, 0, 40, 0, 0, 40, 0}}},
  {"GARAGE", 132, 24, 1, 7, SN_SNARE, PC_RIM, BS_SUB, LD_STAB, 32,
   {{100, 0, 0, 0, 0, 0, 0, 30, 0, 0, 90, 0, 0, 20, 0, 0},
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0},
    {8, 8, 100, 40, 8, 8, 100, 40, 8, 8, 100, 40, 8, 8, 100, 40},
    {0, 35, 0, 0, 0, 0, 0, 0, 0, 35, 0, 0, 0, 0, 0, 35},
    {60, 0, 0, 60, 0, 0, 50, 0, 0, 0, 70, 30, 0, 50, 0, 0},
    {0, 30, 0, 60, 0, 0, 50, 0, 0, 40, 0, 40, 0, 0, 50, 0}}},
  {"ELECTRO", 128, 0, 0, 0, SN_SNARE, PC_COWBELL, BS_SQUARE, LD_PLUCK, 40,
   {{100, 0, 0, 25, 0, 0, 80, 0, 0, 0, 70, 0, 0, 20, 0, 0},
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0},
    {70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70},
    {0, 0, 30, 0, 0, 30, 0, 0, 30, 0, 0, 40, 0, 0, 30, 0},
    {90, 0, 60, 50, 0, 0, 70, 0, 60, 0, 60, 40, 0, 0, 60, 0},
    {40, 0, 40, 0, 40, 0, 0, 50, 0, 0, 40, 0, 40, 0, 0, 30}}},
  {"DEEP", 120, 14, 1, 5, SN_CLAP, PC_RIM, BS_SUB, LD_PAD, 50,
   {{100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0},
    {0, 0, 0, 0, 90, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0},
    {25, 25, 90, 25, 25, 25, 90, 25, 25, 25, 90, 25, 25, 25, 90, 25},
    {0, 0, 0, 40, 0, 0, 0, 30, 0, 0, 20, 0, 0, 45, 0, 0},
    {30, 0, 0, 70, 0, 0, 50, 0, 20, 0, 70, 30, 0, 0, 50, 0},
    {70, 0, 0, 0, 0, 0, 40, 0, 0, 0, 50, 0, 0, 0, 0, 0}}},
  {"MINIMAL", 125, 8, 2, 7, SN_CLAP, PC_RIM, BS_SUB, LD_PLUCK, 35,
   {{100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 8, 0},
    {0, 0, 0, 0, 70, 0, 0, 0, 0, 0, 0, 0, 80, 0, 0, 10},
    {20, 20, 100, 20, 20, 20, 100, 20, 20, 20, 100, 20, 20, 20, 100, 20},
    {0, 30, 0, 0, 0, 40, 0, 0, 0, 30, 0, 50, 0, 0, 0, 30},
    {0, 0, 60, 30, 0, 0, 0, 50, 0, 0, 60, 30, 0, 0, 0, 30},
    {0, 0, 0, 30, 0, 0, 0, 0, 0, 30, 0, 0, 0, 0, 25, 0}}},
  {"ACID", 130, 0, 0, 9, SN_CLAP, PC_OHAT, BS_SAW, LD_STAB, 40,
   {{100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0},
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0},
    {60, 60, 100, 60, 60, 60, 100, 60, 60, 60, 100, 60, 60, 60, 100, 60},
    {0, 0, 90, 0, 0, 0, 90, 0, 0, 0, 90, 0, 0, 0, 90, 0},
    {90, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75},
    {20, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0}}},
  {"TRANCE", 138, 0, 0, 4, SN_CLAP, PC_OHAT, BS_SAW, LD_STAB, 40,
   {{100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0},
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0},
    {30, 30, 100, 30, 30, 30, 100, 30, 30, 30, 100, 30, 30, 30, 100, 30},
    {0, 0, 60, 0, 0, 0, 60, 0, 0, 0, 60, 0, 0, 0, 60, 0},
    {0, 90, 90, 90, 0, 90, 90, 90, 0, 90, 90, 90, 0, 90, 90, 90},
    {60, 0, 0, 50, 0, 0, 60, 0, 40, 0, 50, 0, 0, 50, 0, 0}}},
  {"PSY", 145, 0, 2, 4, SN_CLAP, PC_RIM, BS_SUB, LD_PLUCK, 30,
   {{100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0},
    {0, 0, 0, 0, 60, 0, 0, 0, 0, 0, 0, 0, 80, 0, 0, 0},
    {30, 30, 100, 30, 30, 30, 100, 30, 30, 30, 100, 30, 30, 30, 100, 30},
    {0, 0, 0, 30, 0, 0, 0, 40, 0, 0, 0, 30, 0, 0, 0, 40},
    {0, 100, 100, 100, 0, 100, 100, 100, 0, 100, 100, 100, 0, 100, 100, 100},
    {40, 0, 30, 0, 0, 40, 0, 0, 40, 0, 0, 30, 0, 40, 0, 0}}},
  {"DUBSTEP", 140, 0, 0, 5, SN_SNARE, PC_RIM, BS_REESE, LD_STAB, 45,
   {{100, 0, 0, 20, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 20, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0},
    {60, 0, 80, 0, 60, 0, 80, 0, 60, 0, 80, 0, 60, 0, 80, 0},
    {0, 0, 0, 0, 0, 0, 30, 0, 0, 0, 0, 0, 0, 30, 0, 20},
    {90, 0, 0, 50, 0, 0, 60, 0, 0, 0, 70, 0, 40, 0, 60, 0},
    {40, 0, 0, 0, 0, 0, 0, 0, 30, 0, 0, 0, 0, 0, 0, 0}}},
  {"BREAKS", 130, 6, 0, 7, SN_SNARE, PC_OHAT, BS_SAW, LD_PLUCK, 40,
   {{100, 0, 0, 0, 0, 0, 40, 0, 30, 0, 90, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 100, 0, 0, 20, 0, 0, 0, 0, 100, 0, 0, 30},
    {80, 20, 80, 20, 80, 20, 80, 20, 80, 20, 80, 20, 80, 20, 80, 20},
    {0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0, 0, 0, 50, 0},
    {90, 0, 0, 40, 0, 0, 50, 0, 0, 0, 80, 0, 0, 40, 0, 0},
    {40, 0, 0, 0, 30, 0, 0, 40, 0, 0, 30, 0, 40, 0, 0, 0}}},
  {"AMAPIANO", 112, 16, 1, 5, SN_CLAP, PC_RIM, BS_808, LD_PAD, 50,
   {{100, 0, 0, 0, 60, 0, 0, 0, 100, 0, 0, 0, 60, 0, 0, 0},
    {0, 0, 0, 0, 80, 0, 0, 0, 0, 0, 0, 0, 90, 0, 0, 0},
    {70, 70, 100, 70, 70, 70, 100, 70, 70, 70, 100, 70, 70, 70, 100, 70},
    {0, 0, 0, 50, 0, 0, 40, 0, 0, 0, 0, 50, 0, 0, 40, 0},
    {40, 0, 0, 80, 0, 0, 70, 40, 0, 0, 80, 30, 0, 60, 40, 0},
    {60, 0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0, 0, 0}}},
  {"REGGAETON", 95, 0, 0, 2, SN_SNARE, PC_RIM, BS_808, LD_PLUCK, 45,
   {{100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0},
    {0, 0, 0, 100, 0, 0, 100, 0, 0, 0, 0, 100, 0, 0, 100, 0},
    {60, 0, 60, 0, 60, 0, 60, 0, 60, 0, 60, 0, 60, 0, 60, 0},
    {0, 0, 30, 0, 0, 0, 0, 0, 0, 0, 30, 0, 0, 0, 0, 0},
    {100, 0, 0, 40, 0, 0, 0, 0, 80, 0, 0, 40, 0, 0, 0, 0},
    {50, 0, 0, 50, 0, 0, 50, 0, 30, 0, 0, 40, 0, 0, 40, 0}}},
  {"LOFI", 85, 28, 1, 10, SN_SNARE, PC_RIM, BS_SUB, LD_PAD, 50,
   {{100, 0, 0, 15, 0, 0, 0, 50, 0, 0, 70, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 15},
    {70, 20, 70, 20, 70, 20, 70, 20, 70, 20, 70, 20, 70, 20, 70, 20},
    {0, 0, 0, 0, 0, 0, 30, 0, 0, 0, 0, 0, 0, 0, 30, 0},
    {90, 0, 0, 0, 0, 0, 0, 50, 0, 0, 70, 0, 0, 0, 0, 0},
    {80, 0, 0, 0, 0, 0, 0, 0, 50, 0, 0, 0, 0, 0, 0, 0}}},
  {"DISCO", 118, 4, 3, 5, SN_SNARE, PC_OHAT, BS_SAW, LD_STAB, 40,
   {{100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0},
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0},
    {60, 60, 100, 60, 60, 60, 100, 60, 60, 60, 100, 60, 60, 60, 100, 60},
    {0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0},
    {90, 0, 80, 0, 90, 0, 80, 0, 90, 0, 80, 0, 90, 0, 80, 0},
    {0, 0, 0, 40, 0, 0, 50, 0, 0, 0, 0, 40, 0, 0, 50, 0}}},
  {"SYNTHWAVE", 100, 0, 0, 9, SN_SNARE, PC_TOM, BS_SAW, LD_PAD, 50,
   {{100, 0, 0, 0, 20, 0, 0, 0, 100, 0, 0, 0, 20, 0, 0, 0},
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0},
    {80, 0, 80, 0, 80, 0, 80, 0, 80, 0, 80, 0, 80, 0, 80, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 30, 30},
    {90, 0, 90, 0, 90, 0, 90, 0, 90, 0, 90, 0, 90, 0, 90, 0},
    {60, 0, 0, 0, 40, 0, 0, 0, 50, 0, 0, 0, 40, 0, 0, 0}}},
};


static const uint8_t SCALE[4][7] = {{0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 5, 7, 9, 11}};
static const char *const SCALE_NAME[4] = {"MEN", "DOR", "FRI", "MAI"};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/* ---- the state: four patterns and the effects, as saved to flash (bytes only: no padding) */
typedef struct {
  uint8_t st[NSMAX];   /* bits 0-1 the step (0 off, 1 ghost, 2 full), 2-3 its chance (100 75 50 25 %), 4-5 repeats */
  uint8_t note[NSMAX]; /* bass, synth: scale degree 0..NOTE_MAX */
  uint8_t lk[NSMAX];   /* locks, 0 none: low nibble the filter (bass, synth) or decay (drums), high nibble the level */
  uint32_t seed;       /* the dice: DENS and CAOS redraw from them, REC rolls new ones */
  uint8_t mute, lock, level, voice; /* voice 0: the style's sound */
  uint8_t len, eu, rot, pad;        /* steps before it repeats; Euclidean hits (0 off); how far it was turned */
} track_t;
#define STEP(t, i) ((t)->st[i] & 3)
#define PROB(t, i) (((t)->st[i] >> 2) & 3)
#define RAT(t, i) (((t)->st[i] >> 4) & 3)
static void set_st(track_t *t, int i, int step, int prob, int rat) { t->st[i] = (uint8_t)(step | prob << 2 | rat << 4); }

typedef struct {
  uint8_t used, style, bpm, swing, root, scale, dens, chaos;
  track_t tr[NT];
} pattern_t;

#define MAGIC 0x44455234u /* "DER4" */
#define NPROJ 2
#define NOTE_MAX 20
typedef struct {
  uint8_t slot, bars; /* slot 0: empty, 1-4: A-D */
} part_t;
typedef struct { /* a project: what SAVE keeps, two of them */
  uint32_t magic;
  uint8_t cur, filt, eco, rev, duck, cut, fdbk, evolve;
  uint8_t song_on, midi, pad[2]; /* midi: bit 0 follow the clock coming in, bit 1 send the notes */
  part_t song[16];
  pattern_t pat[4];
} project_t;
static project_t S;
static struct {
  uint32_t magic;
  uint8_t proj, pad[3];
  project_t p[NPROJ];
} store;
_Static_assert(sizeof store <= SETTINGS_MAX, "the projects must fit the settings");
#define P (S.pat[S.cur])

static pattern_t undo_buf;
static uint8_t undo_ok;

/* loop() <-> audio(): one writer each, except perf_cut (the drop clears it) */
static volatile uint8_t play_slot, slot_req = 0xFF; /* the pattern playing; the one asked for at the next bar */
static volatile uint8_t perf_cut, perf_loop, build_req, build_cancel, building;
static volatile uint32_t perf_la; /* where a loop started (steps since PLAY) */
static volatile uint16_t perf_fx; /* while held: bit 0 stutter 1/16, 1 tape stop, 2 bitcrush, 3 echo throw, 4 stutter 1/32,
                                   * 5 reverb throw, 6 gate, 7 low-pass sweep, 8 high-pass sweep, 9 no bass */
static volatile uint8_t perf_break; /* the drums out, while held */
static volatile uint8_t song_pos, song_bar; /* the part playing, the bar in it */
static volatile uint8_t ui_step = 0xFF, next_step, want_mutate;
static volatile uint32_t step_frames = 5335, delay_frames = 16000;
static volatile uint32_t since_fire; /* frames since the last step */

static int tlen(const track_t *t) { return t->len >= 1 && t->len <= NSMAX ? t->len : NS; }

/* =====================================================================================================
 * generating (loop() side)
 * ===================================================================================================== */
static uint32_t dice_s;
static uint8_t dice(void) { /* the track's dice, from its seed: the same every time */
  dice_s ^= dice_s << 13;
  dice_s ^= dice_s >> 17;
  dice_s ^= dice_s << 5;
  return (uint8_t)((dice_s >> 8) % 255u);
}

static void reseed(track_t *t) { t->seed = random32() | 1u; }

/* Euclidean: k hits spread as evenly as they go over n steps */
static int euclid_hit(int i, int k, int n) { return ((i * k) % n) < k; }

/* the pattern of one track from its dice, the style's knowledge, DENS and CAOS (all Q8) */
static void derive(pattern_t *p, int ti) {
  track_t *t = &p->tr[ti];
  const uint8_t *cn = STYLE[p->style].canon[ti];
  int32_t ch = p->chaos * 2, d = p->dens * 4, mel = ti >= T_BASS, any = 0, len = tlen(t);
  uint8_t nn[NSMAX];
  dice_s = t->seed ? t->seed : 0x9E3779B9u;
  for (int i = 0; i < NSMAX; i++) {
    int j = ((i - t->rot) % len + len) % len; /* the style's knowledge (or the Euclidean rhythm), turned */
    int32_t c = t->eu ? (euclid_hit(j, t->eu, len) ? 255 : 0) : cn[j & 15] * 255 / 100, pr, step, prob = 0, rat = 0;
    uint8_t r = dice(), r2 = dice();
    nn[i] = dice();
    if (len > NS && j >= len - NS && c < 128) { /* a longer pattern: its last bar a little fuller */
      c += 40;
    }
    pr = ((c * (256 - ((179 * ch * ch) >> 16))) >> 8) + (((256 - c) * ch * (mel ? 56 : 90)) >> 16);
    if (d <= 256) {
      if (!(c >= 243 && d > 77)) {
        pr = (pr * d) >> 8;
      }
    } else {
      pr += ((256 - pr) * (d - 256)) >> 9;
    }
    step = r < pr ? ((c < 128 && r2 < 140) ? 1 : 2) : 0;
    if (step && i < len) {
      any = 1;
    }
    if (step == 1 && ch > 100) { /* with chaos, the ghosts may skip */
      prob = 1 + (r2 < ch / 4);
    }
    if (ti == T_HAT && step && ch > 64 && nn[i] < ch / 3) { /* and the hats roll */
      rat = 1 + (ch > 180 && nn[i] < ch / 8);
    }
    set_st(t, i, step, prob, rat);
    t->lk[i] = 0;
  }
  if (!mel) {
    return;
  }
  if (!any) {
    set_st(t, 0, 2, 0, 0);
  }
  if (ti == T_BASS) { /* mostly the root; CAOS lets the fifth, the octave and the sevenths in */
    static const uint8_t DEG[6] = {0, 4, 7, 6, 2, 3};
    int32_t wt[6] = {16 + ((96 * (256 - ch)) >> 8), 32, 24, 16, 16, 10}, tot = 0;
    for (int k = 0; k < 6; k++) {
      tot += wt[k];
    }
    for (int i = 0; i < NSMAX; i++) {
      int32_t x = (nn[i] * tot) >> 8, deg = 0;
      for (int k = 0; k < 6; k++) {
        if (x < wt[k]) {
          deg = DEG[k];
          break;
        }
        x -= wt[k];
      }
      t->note[i] = (uint8_t)deg;
      if (ch < 154 && (i & 15) == 0) { /* each bar starts on the root */
        t->note[i] = 0;
      }
    }
  } else { /* the synth walks the scale; CAOS widens its steps */
    static const uint8_t START[4] = {0, 2, 4, 7};
    int32_t pos = START[nn[0] >> 6], span = 2 + ((ch * 5) >> 8);
    for (int i = 0; i < NSMAX; i++) {
      if (i) {
        pos += (((int32_t)nn[i] - 127) * span + (nn[i] >= 127 ? 64 : -64)) / 128;
      }
      if (pos < 0) {
        pos = -pos;
      }
      if (pos > 7) {
        pos = 14 - pos;
      }
      pos = pos < 0 ? 0 : pos > 7 ? 7 : pos;
      t->note[i] = (uint8_t)pos;
    }
  }
}

static void derive_all(pattern_t *p) {
  for (int t = 0; t < NT; t++) {
    if (!p->tr[t].lock) {
      derive(p, t);
    }
  }
}

static void gen_all(pattern_t *p) {
  for (int t = 0; t < NT; t++) {
    if (!p->tr[t].lock) {
      reseed(&p->tr[t]);
      derive(p, t);
    }
  }
}

static void set_style(pattern_t *p, int s) {
  const style_t *st = &STYLE[s];
  p->style = (uint8_t)s;
  p->bpm = st->bpm;
  p->swing = st->swing;
  p->scale = st->scale;
  p->root = st->root;
  gen_all(p);
}

static void init_pattern(pattern_t *p, int style) {
  p->used = 1;
  p->dens = 64;
  p->chaos = 26;
  for (int t = 0; t < NT; t++) {
    track_t *tr = &p->tr[t];
    tr->mute = tr->lock = tr->voice = tr->eu = tr->rot = 0;
    tr->level = 100;
    tr->len = NS;
  }
  set_style(p, style);
}

/* one or two small changes a track, inside what the style allows */
static void mutate(pattern_t *p) {
  for (int ti = 0; ti < NT; ti++) {
    track_t *t = &p->tr[ti];
    const uint8_t *cn = STYLE[p->style].canon[ti];
    int mel = ti >= T_BASS, left = 1 + ((int)(random32() % 127u) < p->chaos), len = tlen(t);
    if (t->lock) {
      continue;
    }
    for (int k = 0; k < 12 && left > 0; k++) {
      int i = (int)(random32() % (uint32_t)len), on = 0;
      for (int j = 0; j < len; j++) {
        on += STEP(t, j) != 0;
      }
      if (mel && STEP(t, i) && (random32() & 256u)) {
        int v = t->note[i] + ((random32() & 512u) ? 1 : -1);
        t->note[i] = (uint8_t)(v < 0 ? 0 : v > 7 ? 7 : v);
        left--;
      } else if (STEP(t, i)) {
        if (cn[i & 15] < 95 && on > 1) {
          set_st(t, i, 0, 0, 0);
          left--;
        }
      } else if ((int)(random32() % 100u) < cn[i & 15] + 20 + (p->chaos * 30) / 127) {
        set_st(t, i, cn[i & 15] < 50 ? 1 : 2, 0, 0);
        t->lk[i] = 0;
        if (mel) {
          int j = i;
          while (j > 0 && !STEP(t, j - 1)) {
            j--;
          }
          t->note[i] = j > 0 ? t->note[j - 1] : 0;
        }
        left--;
      }
    }
  }
}

static void rotate(track_t *t, int by) { /* within the track's length */
  track_t c = *t;
  int len = tlen(t);
  for (int i = 0; i < len; i++) {
    int j = ((i - by) % len + len) % len;
    t->st[i] = c.st[j];
    t->note[i] = c.note[j];
    t->lk[i] = c.lk[j];
  }
  t->rot = (uint8_t)(((t->rot + by) % len + len) % len);
}

static int double_len(track_t *t) { /* x2: the track twice as long, the copy after it */
  int len = tlen(t);
  if (len * 2 > NSMAX) {
    return 0;
  }
  for (int i = 0; i < len; i++) {
    t->st[len + i] = t->st[i];
    t->note[len + i] = t->note[i];
    t->lk[len + i] = t->lk[i];
  }
  t->len = (uint8_t)(len * 2);
  return 1;
}

/* =====================================================================================================
 * the sound (audio() side)
 * ===================================================================================================== */
#define BLOCK 256
#define HZ(f) ((uint32_t)(f) * 97391u) /* phase step of f Hz */
#define ENV_MAX (1u << 20)
#define RATE(ms) (1486u / (ms)) /* an exponential decay with that time constant, ms <= 1486 */

typedef struct {
  uint32_t v, rate;
} env_t;

static inline int32_t env_run(env_t *e) { /* -> Q15 */
  uint32_t d = ((e->v * e->rate) >> 16) + 1u;
  e->v = e->v > d ? e->v - d : 0;
  return (int32_t)(e->v >> 5);
}
static inline void env_hit(env_t *e, uint32_t rate) {
  e->v = ENV_MAX;
  e->rate = rate ? rate : 1u;
}

static uint32_t nz_s = 0x9E3779B9u;
static inline int32_t nz(void) {
  nz_s ^= nz_s << 13;
  nz_s ^= nz_s >> 17;
  nz_s ^= nz_s << 5;
  return (int16_t)(nz_s >> 16);
}
static inline int32_t clamp(int32_t x, int32_t m) { return x > m ? m : x < -m ? -m : x; }

/* 0..127 -> 30 Hz .. 7.4 kHz, an octave every 16 */
static uint32_t knob_hz(int v) { return ((30u * (16u + ((uint32_t)v & 15u))) >> 4) << ((uint32_t)v >> 4); }
/* the state-variable filter's frequency coefficient, Q15 (2 sin(pi f / fs)), f <= 7.5 kHz */
static int32_t svf_f(uint32_t hz) { return sine((hz > 7500u ? 7500u : hz) * 48696u) * 2; }

static int32_t bd[BLOCK], bs[BLOCK], bl[BLOCK], bx[BLOCK]; /* drums, bass, synth, reverb send */

static struct {
  env_t a, p;
  uint32_t ph, base, sw;
  int32_t vel;
  uint8_t crush;
} kick;
static struct {
  env_t n, t;
  uint32_t ph, tail;
  int32_t lp1, lp2, vel;
  uint16_t cnt;
  uint8_t clap, crush;
} snare;
static struct {
  env_t a;
  int32_t lp1, lp2, vel;
  uint8_t crush;
} hat, crash;
static struct {
  env_t a;
  uint32_t ph, ph2;
  int32_t lp1, lp2, vel;
  uint8_t type;
} perc;
static struct {
  env_t a, p, m;
  uint32_t ph, ph2, inc;
  int32_t low, band, vel;
  uint8_t type, cut;
} bass;
static struct {
  env_t a, m;
  uint32_t ph[4], inc[4], phm;
  int32_t low, band, vel, att, att_inc;
  uint8_t type, nosc, cut;
} lead;
static struct {
  int32_t lp, p; /* p: how far the build-up is, Q15; 0 = no riser */
} riser;

static void v_kick(uint32_t n) {
  uint32_t ph = kick.ph;
  for (uint32_t i = 0; i < n; i++) {
    int32_t a = env_run(&kick.a), pe = env_run(&kick.p);
    int32_t x;
    ph += kick.base + (uint32_t)((pe * (int32_t)kick.sw) >> 5);
    x = (sine(ph) * a) >> 15;
    if (kick.crush) {
      x = (x >> 11) << 11;
    }
    bd[i] += ((x * kick.vel) >> 7) * 3 / 4;
  }
  kick.ph = ph;
}

static void v_snare(uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    int32_t x = nz(), an = env_run(&snare.n), s;
    snare.lp1 += (x - snare.lp1) >> 2;
    if (snare.clap) { /* three short bursts, then the tail */
      snare.lp2 += (snare.lp1 - snare.lp2) >> 4;
      if (snare.cnt < 1500) {
        snare.cnt++;
        if (snare.cnt == 485 || snare.cnt == 1014) {
          env_hit(&snare.n, RATE(4));
        } else if (snare.cnt == 1500) {
          env_hit(&snare.n, snare.tail);
        }
      }
      s = ((snare.lp1 - snare.lp2) * an) >> 15;
    } else {
      int32_t at = env_run(&snare.t);
      snare.ph += HZ(190);
      s = (((x - snare.lp1) * an) >> 16) + ((sine(snare.ph) * at) >> 16);
    }
    if (snare.crush) {
      s = (s >> 10) << 10;
    }
    s = (s * snare.vel) >> 7;
    bd[i] += s;
    bx[i] += s >> 1;
  }
}

static void v_noise(uint32_t n, env_t *a, int32_t *lp1, int32_t *lp2, int32_t vel, int send, int crush) { /* hats, crash */
  for (uint32_t i = 0; i < n; i++) {
    int32_t x = nz(), h, s;
    *lp1 += (x - *lp1) >> 1;
    h = x - *lp1;
    *lp2 += (h - *lp2) >> 1;
    s = ((((h - *lp2) * env_run(a)) >> 16) * vel) >> 7;
    if (crush) {
      s = (s >> 10) << 10;
    }
    bd[i] += s;
    if (send) {
      bx[i] += s >> 1;
    }
  }
}

static void v_perc(uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    int32_t a = env_run(&perc.a), s;
    if (perc.type == PC_RIM) {
      perc.ph += HZ(1750);
      s = (sine(perc.ph) * a) >> 16;
    } else if (perc.type == PC_COWBELL) {
      int32_t sq;
      perc.ph += HZ(587);
      perc.ph2 += HZ(845);
      sq = ((int32_t)perc.ph < 0 ? -7000 : 7000) + ((int32_t)perc.ph2 < 0 ? -7000 : 7000);
      perc.lp1 += (sq - perc.lp1) >> 3;
      s = ((sq - perc.lp1) * a) >> 15;
    } else { /* tom */
      perc.ph += HZ(90) + (uint32_t)((a * (int32_t)(HZ(60) >> 10)) >> 5);
      s = (sine(perc.ph) * a) >> 15;
    }
    s = (s * perc.vel) >> 7;
    bd[i] += s;
    bx[i] += s >> 2;
  }
}

static void v_riser(uint32_t n) {
  int32_t a = 1500 + ((riser.p * 22000) >> 15), g = riser.p / 3;
  for (uint32_t i = 0; i < n; i++) {
    riser.lp += ((nz() - riser.lp) * a) >> 15;
    bd[i] += (riser.lp * g) >> 15;
  }
}

static void v_bass(uint32_t n) {
  uint32_t ph = bass.ph, ph2 = bass.ph2, inc = bass.inc;
  if (bass.type == BS_FM) { /* two operators: the modulator's depth falls with the note */
    for (uint32_t i = 0; i < n; i++) {
      int32_t a = env_run(&bass.a), m = env_run(&bass.m), s;
      ph2 += inc;
      ph += inc;
      s = (sine(ph + (uint32_t)sine(ph2) * (uint32_t)(m * 7)) * a) >> 15;
      bs[i] += ((s * bass.vel) >> 7) * 3 / 4;
    }
  } else if (bass.type == BS_808) {
    for (uint32_t i = 0; i < n; i++) {
      int32_t a = env_run(&bass.a), pe = env_run(&bass.p), s;
      ph += inc + (uint32_t)((pe * (int32_t)(inc >> 10)) >> 5);
      s = (sine(ph) * a) >> 15;
      s = clamp(s + (s >> 1), 30000); /* a little grit */
      bs[i] += (s * bass.vel) >> 7;
    }
  } else {
    uint32_t hz = knob_hz(bass.cut);
    int32_t low = bass.low, band = bass.band, f, q = bass.type == BS_SUB ? 30000 : 9000;
    hz = (hz * (11469u + (((bass.a.v >> 5) * 21299u) >> 15))) >> 15; /* the filter closes with the note */
    if (bass.type == BS_SUB && hz > 700u) {
      hz = 700u;
    }
    f = svf_f(hz < 40u ? 40u : hz);
    for (uint32_t i = 0; i < n; i++) {
      int32_t a = env_run(&bass.a), x, high;
      ph += inc;
      ph2 += inc + (inc >> 7);
      if (bass.type == BS_SAW) {
        x = (int32_t)ph >> 18;
      } else if (bass.type == BS_REESE) {
        x = ((int32_t)ph >> 19) + ((int32_t)ph2 >> 19);
      } else if (bass.type == BS_SQUARE) {
        x = (int32_t)ph < 0 ? -6000 : 6000;
      } else { /* sub: a triangle */
        int32_t t = (int32_t)(ph >> 16);
        x = ((t < 32768 ? t : 65535 - t) - 16384) >> 1;
      }
      low = clamp(low + ((f * band) >> 15), 40000);
      high = x - low - ((q * band) >> 15);
      band = clamp(band + ((f * high) >> 15), 40000);
      bs[i] += ((((low * a) >> 13) * bass.vel) >> 7);
    }
    bass.low = low;
    bass.band = band;
  }
  bass.ph = ph;
  bass.ph2 = ph2;
}

static void v_lead(uint32_t n) {
  uint32_t hz = knob_hz(lead.cut) * 8u / 5u;
  if (lead.type == LD_BELL) { /* FM at 3.5 : 1, a bell */
    for (uint32_t i = 0; i < n; i++) {
      int32_t a = env_run(&lead.a), m = env_run(&lead.m), x;
      lead.ph[0] += lead.inc[0];
      lead.phm += lead.inc[0] * 7u / 2u;
      x = (sine(lead.ph[0] + (uint32_t)sine(lead.phm) * (uint32_t)(m * 5)) * a) >> 16;
      bl[i] += (x * lead.vel) >> 7;
    }
    return;
  }
  int32_t low = lead.low, band = lead.band, f, q = lead.type == LD_PLUCK ? 9000 : 16000, sh = lead.type == LD_PLUCK ? 13 : 14;
  hz = (hz * (9830u + (((lead.a.v >> 5) * 22938u) >> 15))) >> 15;
  f = svf_f(hz < 150u ? 150u : hz);
  for (uint32_t i = 0; i < n; i++) {
    int32_t a = env_run(&lead.a), x = 0, high;
    for (int o = 0; o < lead.nosc; o++) {
      lead.ph[o] += lead.inc[o];
      x += (int32_t)lead.ph[o] >> 19;
    }
    if (lead.type == LD_PLUCK) {
      x += (int32_t)lead.ph[0] < 0 ? -5000 : 5000;
    }
    if (lead.att < 32767) {
      lead.att = lead.att + lead.att_inc > 32767 ? 32767 : lead.att + lead.att_inc;
    }
    low = clamp(low + ((f * band) >> 15), 40000);
    high = x - low - ((q * band) >> 15);
    band = clamp(band + ((f * high) >> 15), 40000);
    bl[i] += (((low * ((a * lead.att) >> 15)) >> sh) * lead.vel) >> 7;
  }
  lead.low = low;
  lead.band = band;
}

/* ---- starting notes (audio() only) */
static int midi_of(const pattern_t *p, int deg) {
  return 36 + p->root - (p->root > 6 ? 12 : 0) + SCALE[p->scale & 3][deg % 7] + 12 * (deg / 7);
}

static volatile int32_t duck; /* the kick pushes bass and synth down: Q15 */

/* ---- MIDI out (S.midi bit 1): drums on channel 10, bass on 1, synth on 2; one note a track */
static uint8_t mnote[NT];
static void midi_note(int ti, int note, int vel) {
  int ch = ti < T_BASS ? 9 : ti == T_BASS ? 0 : 1;
  if (!(S.midi & 2)) {
    return;
  }
  if (mnote[ti]) {
    midi_send((uint8_t)(0x80 | ch), (uint8_t)(mnote[ti] - 1), 0);
  }
  midi_send((uint8_t)(0x90 | ch), (uint8_t)(note & 127), (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel));
  mnote[ti] = (uint8_t)(note + 1);
}
static void midi_all_off(void) {
  for (int t = 0; t < NT; t++) {
    if (mnote[t]) {
      midi_send((uint8_t)(0x80 | (t < T_BASS ? 9 : t == T_BASS ? 0 : 1)), (uint8_t)(mnote[t] - 1), 0);
      mnote[t] = 0;
    }
  }
}

static uint32_t rate_ms(int ms, int lock) { /* a decay, stretched or shortened by the step's lock (64 = as is) */
  if (lock) {
    ms = ms * lock / 64;
  }
  return RATE(ms < 2 ? 2 : ms > 1400 ? 1400 : ms);
}

/* one note of track ti: scale degree deg, level vel 0..127, lock la (0: none) */
static void play(int ti, int deg, int vel, int la) {
  const pattern_t *p = &S.pat[play_slot & 3];
  const style_t *st = &STYLE[p->style];
  int vc = p->tr[ti].voice;
  switch (ti) {
    case T_KICK: {
      static const uint8_t TAU[4] = {0, 110, 34, 50};
      kick.base = vc == 1 ? HZ(42) : vc == 2 ? HZ(54) : HZ(45);
      kick.sw = (vc == 2 ? HZ(230) : vc == 1 ? HZ(80) : HZ(120)) >> 10;
      kick.crush = vc == 3;
      env_hit(&kick.a, rate_ms(vc ? TAU[vc] : st->kick_ms, la));
      env_hit(&kick.p, RATE(vc == 1 ? 24 : vc == 2 ? 10 : 16));
      kick.ph = 0;
      kick.vel = vel;
      duck = (S.duck * 230 * vel) >> 7;
      midi_note(ti, 36, vel);
      break;
    }
    case T_SNARE:
      snare.clap = vc ? vc == 2 : st->snare == SN_CLAP;
      snare.crush = vc == 3;
      snare.cnt = snare.clap ? 0 : 1500;
      snare.tail = rate_ms(45, la);
      snare.vel = vel;
      env_hit(&snare.n, snare.clap ? RATE(4) : rate_ms(40, la));
      env_hit(&snare.t, RATE(25));
      snare.ph = 0;
      midi_note(ti, snare.clap ? 39 : 38, vel);
      break;
    case T_HAT:
      env_hit(&hat.a, rate_ms(vc == 1 ? 6 : vc == 2 ? 28 : 9, la));
      hat.vel = vel;
      hat.crush = vc == 3;
      if (perc.type == PC_OHAT && perc.a.v) { /* the closed hat chokes the open one */
        perc.a.rate = RATE(6);
      }
      midi_note(ti, 42, vel);
      break;
    case T_PERC: {
      static const uint8_t NOTE[4] = {46, 37, 56, 45}, TAU[4] = {60, 6, 45, 50};
      perc.type = (uint8_t)(vc ? vc - 1 : st->perc);
      perc.vel = vel;
      perc.ph = perc.ph2 = 0;
      env_hit(&perc.a, rate_ms(TAU[perc.type & 3], la));
      midi_note(ti, NOTE[perc.type & 3], vel);
      break;
    }
    case T_BASS: {
      static const uint8_t TAU[6] = {60, 55, 120, 50, 150, 140};
      int m = midi_of(p, deg);
      bass.type = (uint8_t)(vc ? vc - 1 : st->bass);
      bass.inc = note_inc(m);
      bass.ph = bass.ph2 = 0;
      bass.vel = vel;
      bass.cut = (uint8_t)(la ? la : S.cut);
      env_hit(&bass.a, RATE(TAU[bass.type % 6]));
      env_hit(&bass.p, RATE(14));
      env_hit(&bass.m, RATE(90));
      midi_note(ti, m, vel);
      break;
    }
    default: {
      lead.type = (uint8_t)(vc ? vc - 1 : st->lead);
      lead.nosc = lead.type == LD_PLUCK || lead.type == LD_BELL ? 1 : lead.type == LD_CHORD ? 4 : 3;
      for (int o = 0; o < 4; o++) { /* the triad, or the seventh chord */
        lead.inc[o] = note_inc(midi_of(p, deg + 2 * o) + 24);
        lead.inc[o] += (lead.inc[o] >> 9) * (uint32_t)o; /* a little detune */
      }
      lead.ph[0] = lead.phm = 0;
      lead.vel = vel;
      lead.cut = (uint8_t)(la ? la : S.cut);
      lead.att = 0;
      lead.att_inc = lead.type == LD_PAD ? 18 : 400;
      env_hit(&lead.a, lead.type == LD_PAD ? RATE(200) : lead.type == LD_PLUCK ? RATE(45) : lead.type == LD_BELL ? RATE(380) : RATE(35));
      env_hit(&lead.m, RATE(160));
      midi_note(ti, midi_of(p, deg) + 24, vel);
      break;
    }
  }
}

static int trig(int ti, int step, int force) {
  const track_t *t = &S.pat[play_slot & 3].tr[ti];
  int i = step & (NSMAX - 1), v = force ? force : STEP(t, i), lv = t->lk[i] >> 4, la = t->lk[i] & 15, vel;
  if (!v || (t->mute && !force)) {
    return 0;
  }
  vel = lv ? lv * 8 + 7 : v == 2 ? 127 : 58;
  play(ti, t->note[i], (vel * t->level) >> 7, la ? la * 8 + 4 : 0);
  return 1;
}

/* ---- the master: echo, reverb, ducking, the DJ filter, the bass cut, a soft ceiling */
#define DL 32768u
static int16_t dl[DL] POOL;
static uint32_t dl_w;
static int32_t dl_lp;
#define COMB 1700
#define APL 600
static const uint16_t COMB_LEN[4] = {1557, 1617, 1491, 1422}, AP_LEN[3] = {556, 441, 579};
static int16_t cb[4][COMB] POOL, ap[3][APL] POOL;
static uint16_t cb_i[4], ap_i[3];
static int32_t cb_f[4];
static int32_t hp_acc[2][2], lp_low[2], lp_band[2];
static uint32_t hp_hz = 20;

/* the punch-in effects play from a recording of the master: 8192 frames (185 ms) */
#define CAPN 8192u
static int16_t cap[CAPN][2] POOL;
static uint32_t cw, st_base, st_len, st_k, tp_i, tp_f, crush_n;
static int32_t tp_rate, crush_l, crush_r;
static uint16_t fx_prev;
static uint32_t lp_hz = 9000;
static int32_t gate_g = 32767;

static inline int32_t allpass(int k, int32_t in) {
  int32_t bo = ap[k][ap_i[k]];
  ap[k][ap_i[k]] = (int16_t)clamp(in + (bo >> 1), 32767);
  if (++ap_i[k] >= AP_LEN[k]) {
    ap_i[k] = 0;
  }
  return bo - in;
}

static void master(int32_t *out, uint32_t n) {
  uint16_t fx = perf_fx;
  uint32_t dt = delay_frames, target = (perf_cut || (fx & 0x200)) ? 380u : 20u, lpt = 9000u, half = step_frames / 2u, sf = since_fire;
  int32_t eco = S.eco, fdbk = S.fdbk, rev = S.rev, filt = S.filt, hpf, lpf = 0, cfb = 215;
  if (filt > 68) { /* the DJ filter: right of the middle cuts lows, left of it cuts highs */
    uint32_t h = knob_hz(10 + ((filt - 68) * 100) / 59);
    target = h > target ? h : target;
  } else if (filt < 60) {
    lpt = knob_hz(40 + (filt * 87) / 59);
  }
  if (fx & 0x80) { /* low-pass sweep: down to 280 Hz while held */
    lpt = lpt < 280u ? lpt : 280u;
  }
  if (fx & 0x100) { /* high-pass sweep: up to 1.8 kHz */
    target = target > 1800u ? target : 1800u;
  }
  lp_hz = (uint32_t)((int32_t)lp_hz + ((int32_t)lpt - (int32_t)lp_hz) / ((fx & 0x80) ? 24 : 6));
  if (lp_hz < 8000u) {
    lpf = svf_f(lp_hz);
  }
  if (fx & 0x20) { /* reverb throw: all of it into a longer reverb */
    rev = 127;
    cfb = 236;
  }
  if (riser.p) {
    uint32_t h = 20u + (((uint32_t)riser.p * 700u) >> 15);
    target = h > target ? h : target;
  }
  hp_hz = building ? target : (uint32_t)((int32_t)hp_hz + ((int32_t)target - (int32_t)hp_hz) / ((fx & 0x100) ? 24 : 4));
  hpf = (int32_t)((hp_hz < 20u ? 20u : hp_hz > 4200u ? 4200u : hp_hz) * 14u / 3u); /* 2 pi f / fs, Q15 */
  if ((fx & 0x11) && !(fx_prev & 0x11)) { /* stutter: repeat the last 16th (or 32nd) */
    st_len = (fx & 1) ? step_frames : half;
    st_len = st_len > CAPN - 1u ? CAPN - 1u : st_len < 512u ? 512u : st_len;
    st_base = cw - st_len;
    st_k = 0;
  }
  if ((fx & 2) && !(fx_prev & 2)) { /* tape stop: read behind, slower and slower */
    tp_i = cw;
    tp_f = 0;
    tp_rate = 65536;
  }
  fx_prev = fx;
  if (fx & 8) {
    fdbk = 112;
  }

  for (uint32_t i = 0; i < n; i++) {
    int32_t ld = bl[i], tap, w, rl = 0, rr = 0, syn, d = duck, mix[2];
    syn = (((bs[i] + ld) >> 2) * (32767 - d)) >> 13;
    tap = dl[(dl_w - dt) & (DL - 1u)];
    dl_lp += (tap - dl_lp) >> 1;
    w = ((ld * eco) >> 7) + ((dl_lp * fdbk) >> 7);
    if (fx & 8) { /* echo throw: everything into the echo */
      w += (bd[i] + syn) >> 1;
    }
    dl[dl_w++ & (DL - 1u)] = (int16_t)clamp(w, 32767);
    if (rev) {
      int32_t in = (((bx[i] + ld + (tap >> 1)) >> 2) * rev) >> 7, acc = 0;
      if (fx & 0x20) {
        in += (bd[i] + syn) >> 3;
      }
      for (int c = 0; c < 4; c++) {
        int32_t y = cb[c][cb_i[c]];
        cb_f[c] = (y * 192 + cb_f[c] * 64) >> 8;
        cb[c][cb_i[c]] = (int16_t)clamp(in + ((cb_f[c] * cfb) >> 8), 32767);
        if (++cb_i[c] >= COMB_LEN[c]) {
          cb_i[c] = 0;
        }
        acc += y;
      }
      acc = allpass(0, clamp(acc, 60000));
      rl = allpass(1, acc) >> 1;
      rr = allpass(2, acc) >> 1;
    }
    if (d) {
      duck = d - (d >> 12) - 1;
    }
    mix[0] = bd[i] + syn + tap + rl;
    mix[1] = bd[i] + syn + tap + rr;
    if (!(fx & 0x11)) {
      cap[cw & (CAPN - 1u)][0] = (int16_t)clamp(mix[0], 32767);
      cap[cw & (CAPN - 1u)][1] = (int16_t)clamp(mix[1], 32767);
      cw++;
    }
    if (fx & 0x11) {
      uint32_t k = st_k++ % st_len, e = k < st_len - k ? k : st_len - k;
      const int16_t *c = cap[(st_base + k) & (CAPN - 1u)];
      e = e > 64u ? 64u : e; /* no clicks at the seam */
      mix[0] = (c[0] * (int32_t)e) >> 6;
      mix[1] = (c[1] * (int32_t)e) >> 6;
    } else if (fx & 2) {
      if (tp_rate > 0) {
        const int16_t *c = cap[tp_i & (CAPN - 1u)];
        mix[0] = c[0];
        mix[1] = c[1];
        tp_f += (uint32_t)tp_rate;
        tp_i += tp_f >> 16;
        tp_f &= 0xFFFFu;
        tp_rate -= 5; /* stopped in ~0.3 s */
      } else {
        mix[0] = mix[1] = 0;
      }
    }
    if (fx & 4) { /* bitcrush: 5 bits, a quarter of the rate */
      if (!(crush_n++ & 3u)) {
        crush_l = (mix[0] >> 11) << 11;
        crush_r = (mix[1] >> 11) << 11;
      }
      mix[0] = crush_l;
      mix[1] = crush_r;
    }
    if (fx & 0x40) { /* gate: each 16th open for its first half */
      int32_t tg = ((sf + i) % step_frames) < half ? 32767 : 0;
      gate_g += (tg - gate_g) >> 5;
    } else if (gate_g < 32767) {
      gate_g += ((32767 - gate_g) >> 5) + 1;
      gate_g = gate_g > 32767 ? 32767 : gate_g;
    }
    if (gate_g < 32767) {
      mix[0] = (mix[0] * (gate_g >> 3)) >> 12;
      mix[1] = (mix[1] * (gate_g >> 3)) >> 12;
    }
    for (int c = 0; c < 2; c++) {
      int32_t x = mix[c], high, u, u3;
      for (int k = 0; k < 2; k++) { /* two one-pole high-passes, their state in Q8 so silence stays silent */
        hp_acc[c][k] += (int32_t)(((int64_t)(x * 256 - hp_acc[c][k]) * hpf) >> 15);
        x -= hp_acc[c][k] >> 8;
      }
      high = x;
      x = high;
      if (lpf) {
        lp_low[c] += (int32_t)(((int64_t)lpf * lp_band[c]) >> 15);
        high = x - lp_low[c] - (int32_t)(((int64_t)23000 * lp_band[c]) >> 15);
        lp_band[c] += (int32_t)(((int64_t)lpf * high) >> 15);
        x = lp_low[c];
      }
      u = (clamp(x, 49150) * 21845) >> 15; /* a soft ceiling: u - u^3/3 */
      u3 = (((u * u) >> 15) * u) >> 15;
      out[2 * i + c] += ((u - u3 / 3) * 3) >> 1;
    }
  }
}

static void render(int32_t *out, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    bd[i] = bs[i] = bl[i] = bx[i] = 0;
  }
  if (kick.a.v) {
    v_kick(n);
  }
  if (snare.n.v || snare.t.v || (snare.clap && snare.cnt < 1500)) {
    v_snare(n);
  }
  if (hat.a.v) {
    v_noise(n, &hat.a, &hat.lp1, &hat.lp2, hat.vel, 0, hat.crush);
  }
  if (crash.a.v) {
    v_noise(n, &crash.a, &crash.lp1, &crash.lp2, crash.vel, 1, 0);
  }
  if (perc.a.v) {
    if (perc.type == PC_OHAT) {
      v_noise(n, &perc.a, &perc.lp1, &perc.lp2, perc.vel, 0, 0);
    } else {
      v_perc(n);
    }
  }
  if (riser.p) {
    v_riser(n);
  }
  if (bass.a.v) {
    v_bass(n);
  }
  if (lead.a.v) {
    v_lead(n);
  }
  master(out, n);
}

/* ---- the sequencer: 6 clock ticks a step */
/* ---- the sequencer: 6 clock ticks a step */
static uint8_t run, tickn, bu_total, bu_left, bu_silent, bu_roll32, swing_step;
static uint32_t bar, swing_wait;
static uint8_t rt_n[NT], rt_s[NT]; /* repeats still to play in this step, and which step */
static uint32_t rt_w[NT], rt_i[NT];
static uint8_t rt_fresh; /* repeats scheduled after this block's sound was made */
static const uint8_t CHANCE[4] = {255, 192, 128, 64};
static volatile uint32_t abs_step; /* steps since PLAY (or since the pattern changed): each track goes round its length */
static uint32_t swing_abs;
static volatile uint8_t ui_tpos[NT]; /* the step each track played last */
static volatile uint8_t perf_fill;
static volatile uint8_t lq_t[8], lq_d[8], lq_w, lq_r; /* notes played on the keys, for audio() */
static volatile uint8_t pv_t, pv_s, pv_req;           /* a step set by hand while stopped: hear it */
static pattern_t stage;                               /* a new style, for the next bar */
static volatile uint8_t stage_req, swapped;

static void roll_hit(int32_t p) {
  snare.clap = 0;
  snare.crush = 0;
  snare.cnt = 1500;
  snare.vel = 35 + ((p * 80) >> 15);
  env_hit(&snare.n, RATE(22));
  env_hit(&snare.t, RATE(14));
  snare.ph = 0;
}

static void fire(int cur, uint32_t ab) {
  uint32_t ea = perf_loop ? perf_la + ((ab - perf_la) % perf_loop) : ab; /* a loop: the same few steps again */
  int ps0 = (int)(ea & 15), hold = 0;
  bu_silent = 0;
  if (bu_total) {
    if (!bu_left) { /* the drop: everything comes back at once */
      bu_total = 0;
      building = 0;
      riser.p = 0;
      perf_cut = 0;
      env_hit(&crash.a, RATE(260));
      crash.vel = 110;
    } else {
      int32_t p = ((int32_t)(bu_total - bu_left) * 32767) / bu_total;
      hold = 1;
      bu_silent = bu_left <= 2;
      bu_roll32 = !bu_silent && p >= 26214;
      riser.p = bu_silent ? 0 : (p < 600 ? 600 : p);
      if (!bu_silent && cur % (p < 8192 ? 4 : p < 16384 ? 2 : 1) == 0) {
        roll_hit(p);
      }
      bu_left--;
    }
  }
  since_fire = 0;
  for (int t = 0; t < NT; t++) {
    const pattern_t *pp = &S.pat[play_slot & 3];
    const track_t *tr = &pp->tr[t];
    int sp = (int)(ea % (uint32_t)tlen(tr)); /* this track's step */
    ui_tpos[t] = (uint8_t)sp;
    if (bu_silent || (hold && (t == T_KICK || t == T_BASS)) || (perf_break && t < T_BASS)) {
      continue;
    }
    if (perf_fill && t < T_BASS && !STEP(tr, sp)) { /* FILL: the drums fill the gaps, the snare rolls in the last beat */
      int c = STYLE[pp->style].canon[t][sp & 15], r = (int)(nz() & 255);
      if (!tr->mute && ((t == T_SNARE && cur >= 12) || (c && r < 170) || r < 40)) {
        trig(t, sp, t == T_SNARE && cur >= 12 ? 2 : 1);
      }
      continue;
    }
    if (PROB(tr, sp) && (nz() & 255) >= CHANCE[PROB(tr, sp)]) {
      continue; /* this time it skips */
    }
    if (trig(t, sp, 0) && RAT(tr, sp)) {
      rt_n[t] = (uint8_t)(RAT(tr, sp) > 2 ? 2 : RAT(tr, sp));
      rt_i[t] = step_frames / (rt_n[t] + 1u);
      rt_w[t] = rt_i[t];
      rt_s[t] = (uint8_t)sp;
      rt_fresh |= (uint8_t)(1u << t);
    }
  }
  ui_step = (uint8_t)ps0;
}

static void on_step(void) {
  int cur = next_step;
  if (cur == 0) {
    uint8_t was = play_slot;
    if (stage_req) { /* a new style comes in on the bar */
      S.pat[play_slot & 3] = stage;
      stage_req = 0;
      swapped = 1;
    }
    if (slot_req < 4) {
      play_slot = slot_req;
      slot_req = 0xFF;
    } else if (S.song_on && S.song[0].slot) { /* the song: the next part when this one's bars are done */
      if (!S.song[song_pos & 15].slot) {
        song_pos = 0;
        song_bar = 0;
      } else if (bar && ++song_bar >= S.song[song_pos & 15].bars) {
        song_bar = 0;
        song_pos = (uint8_t)((song_pos + 1) & 15);
        if (!song_pos || !S.song[song_pos].slot) {
          song_pos = 0;
        }
      }
      play_slot = (uint8_t)((S.song[song_pos & 15].slot - 1) & 3);
    }
    if (play_slot != was) { /* another pattern starts from its first step */
      abs_step = 0;
    }
    bar++;
    if (S.evolve && bar > 1 && (bar & 1u)) {
      want_mutate = 1;
    }
  }
  if (build_cancel) {
    build_cancel = 0;
    bu_total = 0;
    building = 0;
    riser.p = 0;
  }
  if (build_req) {
    build_req = 0;
    if (!bu_total) {
      bu_total = bu_left = (uint8_t)(((16 - cur) & 15) + 32);
      building = 1;
    }
  }
  next_step = (uint8_t)((cur + 1) & 15);
  if ((cur & 1) && S.pat[play_slot & 3].swing) { /* swing: the off steps come late */
    swing_step = (uint8_t)cur;
    swing_abs = abs_step;
    swing_wait = (step_frames * S.pat[play_slot & 3].swing) / 100u + 1u;
  } else {
    fire(cur, abs_step);
  }
  abs_step++;
}

void audio(int32_t *out, uint32_t frames) {
  const clock_event_t *ev;
  int n = clock_events(&ev), ei = 0;
  uint32_t pos = 0;
  midi_frame(0);
  if (pv_req) {
    pv_req = 0;
    trig(pv_t % NT, pv_s, 2);
  }
  while (lq_r != lq_w) { /* notes played on the keys */
    uint8_t i = lq_r & 7, ti = lq_t[i] % NT;
    play(ti, lq_d[i], (110 * S.pat[play_slot & 3].tr[ti].level) >> 7, 0);
    lq_r++;
  }
  for (;;) {
    uint32_t next = frames;
    midi_frame(pos);
    while (ei < n && ev[ei].at <= pos) {
      if (ev[ei].what == CLOCK_START) {
        run = 1;
        tickn = 0;
        next_step = 0;
        bar = 0;
        swing_wait = 0;
        song_pos = song_bar = 0;
        abs_step = 0;
      } else if (ev[ei].what == CLOCK_STOP) {
        for (int t = 0; t < NT; t++) {
          rt_n[t] = 0;
        }
        midi_frame(ev[ei].at);
        midi_all_off();
        run = 0;
        bu_total = 0;
        building = 0;
        riser.p = 0;
        swing_wait = 0;
        ui_step = 0xFF;
      } else if (ev[ei].what == CLOCK_TICK && run) {
        if (tickn == 0) {
          on_step();
        } else if (tickn == 3 && bu_total && bu_roll32 && !bu_silent) {
          roll_hit(30000);
        }
        tickn = (uint8_t)((tickn + 1) % 6);
      }
      ei++;
    }
    if (pos >= frames) {
      break;
    }
    if (ei < n && ev[ei].at < next) {
      next = ev[ei].at;
    }
    if (swing_wait && pos + swing_wait < next) {
      next = pos + swing_wait;
    }
    for (int t = 0; t < NT; t++) {
      if (rt_n[t] && pos + rt_w[t] < next) {
        next = pos + rt_w[t];
      }
    }
    if (next - pos > BLOCK) {
      next = pos + BLOCK;
    }
    rt_fresh = 0;
    render(out + 2 * pos, next - pos);
    since_fire += next - pos;
    if (next < frames) {
      midi_frame(next);
    }
    if (swing_wait) {
      swing_wait = swing_wait > next - pos ? swing_wait - (next - pos) : 0;
      if (!swing_wait) {
        fire(swing_step, swing_abs);
      }
    }
    for (int t = 0; t < NT; t++) { /* the repeats of a step */
      if (!rt_n[t] || (rt_fresh & (1u << t))) {
        continue;
      }
      rt_w[t] = rt_w[t] > next - pos ? rt_w[t] - (next - pos) : 0;
      if (!rt_w[t]) {
        if (run && !bu_silent) {
          trig(t, rt_s[t], 0);
        }
        rt_w[t] = --rt_n[t] ? rt_i[t] : 0;
      }
    }
    pos = next;
  }
}

/* =====================================================================================================
 * the panel and the screen (loop() side)
 * ===================================================================================================== */
#define BG BLACK
#define DIM RGB(134, 160, 162)
#define OFF RGB(34, 53, 61)
#define OFF_BEAT RGB(48, 72, 82)
#define SEL_BG RGB(22, 37, 44)
#define ACCENT RGB(255, 209, 102)
#define ALERT RGB(255, 107, 94)
static const uint16_t TCOL[NT] = {RGB(255, 107, 94), RGB(255, 179, 71), RGB(242, 232, 109), RGB(127, 224, 162), RGB(92, 200, 255), RGB(197, 155, 255)};
#define HALF(c) ((uint16_t)(((c) >> 1) & 0x7BEFu))

/* the keys: the 16 white ones are the steps, the 11 black ones (OP1-OP6 on the panel) the tracks, the pages, x2 */
static const uint8_t WK[NS] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BK[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
#define K_PAGE 6 /* BK[6..9]: pages 1-4 */
#define K_X2 10

/* FX + a white key: what it does (the guide on the screen says the same) */
enum { FX_BREAK = 9,
       FX_BUILD = 11,
       FX_FILL = 12 };
static const uint16_t FXBIT[NS] = {0x1, 0x10, 0x2, 0x4, 0x8, 0x20, 0x40, 0x80, 0x100, 0, 0x200, 0, 0, 0, 0, 0};
static const char *const FX_NAME[NS] = {"STUT", "ST32", "TAPE", "CRSH", "ECHO", "VERB", "GATE", "LP", "HP", "BREAK", "CUT", "BUILD",
                                        "FILL", "L1/4", "L1/8", "L1/16"};

#define GRID_Y 62
#define ROW_H 18
#define CELL_X(i) (36 + (i) * 12 + ((i) / 4) * 2)

enum { D_HEAD = 1,
       D_KNOBS = 2,
       D_STATUS = 4 };
enum { V_GRID,
       V_RINGS,
       V_SONG,
       V_FX };
static uint8_t sel, page, playing, dirty = 7, rows_dirty = 0x3F, drawn_step = 0xFF, spage, pin_grid, drawn_pg = 0xFF;
static uint8_t turned[NS], view = 0xFF, song_screen, knobs_dirty = 0xF, center_dirty = 1, ring_pg[NT];
static uint16_t fxkeys, fx_drawn = 0xFFFF;
static uint8_t kb_mode, kb_rec, kb_cur; /* playing the keys; recording what they play; where (stopped) */
static uint8_t pend_style = 0xFF;      /* a style waiting for the next bar */
static uint16_t parts_dirty = 0xFFFF;
static int8_t held_step = -1;
static uint32_t last_edit, bpm_now, bpm_set;
static const char *msg;
static uint32_t msg_until, save_at;
static uint8_t ORDER[NSTYLE]; /* the styles in tempo order, for PRESETS */

static void snapshot(void) {
  undo_buf = P;
  undo_ok = 1;
}

static int following(void) { return (S.midi & 1) && clock_bpm(); }

static void set_tempo(void) { /* the tempo follows P.bpm in loop(), gliding while playing; stopped, at once */
  if (!playing) {
    bpm_now = 0;
  }
}

static const char *track_name(int t) {
  static const char *const PERC[4] = {"OHT", "RIM", "COW", "TOM"};
  const style_t *st = &STYLE[P.style];
  int vc = P.tr[t].voice;
  switch (t) {
    case T_KICK:
      return "KIK";
    case T_SNARE:
      return (vc ? vc == 2 : st->snare == SN_CLAP) ? "CLP" : "SNR";
    case T_HAT:
      return "HAT";
    case T_PERC:
      return PERC[vc ? (vc - 1) & 3 : st->perc];
    case T_BASS:
      return "BAS";
    default:
      return "SYN";
  }
}

static int max_len(void) {
  int m = 0;
  for (int t = 0; t < NT; t++) {
    m = tlen(&P.tr[t]) > m ? tlen(&P.tr[t]) : m;
  }
  return m;
}

static void draw_head(void) {
  char b[12];
  rect(0, 0, SCREEN_W, 38, BG);
  text(6, 4, view == V_SONG ? "MUSICA" : view == V_FX ? "EFEITOS" : STYLE[P.style].name, WHITE, FONT_BIG);
  format_int(b, (int)((bpm_now ? bpm_now : P.bpm * 100u) + 50u) / 100);
  text(SCREEN_W - 6 - text_width(b, FONT_BIG), 4, b, following() ? WHITE : ACCENT, FONT_BIG);
}

static void draw_status(void) {
  static char k[20], b[16];
  const char *s = "";
  uint16_t c = DIM;
  int i = 0;
  const char *nn = NOTE_NAME[P.root % 12], *sn = SCALE_NAME[P.scale & 3];
  k[i++] = 'P';
  k[i++] = (char)('1' + store.proj);
  k[i++] = ' ';
  k[i++] = (char)('A' + S.cur);
  k[i++] = ' ';
  k[i++] = ' ';
  while (*nn) {
    k[i++] = *nn++;
  }
  k[i++] = ' ';
  while (*sn) {
    k[i++] = *sn++;
  }
  k[i] = 0;
  rect(0, 40, SCREEN_W, 18, BG);
  if (view == V_SONG) {
    text(6, 40, "SEQ: VOLTAR", DIM, FONT_SMALL);
  } else if (view == V_FX) {
    text(6, 40, "SEGURE A TECLA", DIM, FONT_SMALL);
  } else {
    text(6, 40, k, ACCENT, FONT_SMALL);
  }
  if (msg) {
    s = msg;
    c = WHITE;
  } else if (view == V_SONG) {
    s = S.song_on ? "LIGADA" : "DESLIGADA";
    c = S.song_on ? ACCENT : DIM;
  } else if (pend_style < NSTYLE) {
    const char *n = STYLE[pend_style].name;
    int j = 0;
    b[j++] = '>';
    b[j++] = ' ';
    while (*n && j < 14) {
      b[j++] = *n++;
    }
    b[j] = 0;
    s = b;
    c = ACCENT;
  } else if (kb_mode) {
    s = kb_rec ? "GRAVANDO" : "TECLADO";
    c = kb_rec ? ALERT : ACCENT;
  } else if (building) {
    s = "BUILD-UP";
    c = ALERT;
  } else if (perf_fill || perf_loop || perf_fx || perf_break) {
    s = "EFEITO";
    c = ALERT;
  } else if (S.song_on && S.song[0].slot && playing) {
    static char m[12] = "MUSICA ";
    format_int(m + 7, song_pos + 1);
    s = m;
  } else if (S.evolve) {
    s = "VARIANDO";
  }
  text(SCREEN_W - 6 - text_width(s, FONT_SMALL), 40, s, c, FONT_SMALL);
}

/* ---- the grid: one page of 16 steps, a row a track */
static void draw_row(int t) {
  const track_t *tr = &P.tr[t];
  int y = GRID_Y + t * ROW_H, len = tlen(tr);
  uint16_t col = tr->mute ? HALF(HALF(TCOL[t])) : TCOL[t];
  rect(0, y, SCREEN_W, ROW_H, t == sel ? SEL_BG : BG);
  text(4, y + 1, track_name(t), tr->mute ? HALF(DIM) : TCOL[t], FONT_SMALL);
  if (tr->lock) {
    rect(30, y + 2, 3, 14, WHITE);
  }
  for (int c = 0; c < NS; c++) {
    int i = spage * NS + c, x = CELL_X(c), v = STEP(tr, i), h = 14 - 3 * PROB(tr, i), rt = RAT(tr, i);
    uint16_t on = v == 2 ? col : HALF(col);
    if (i >= len) { /* past the track's length */
      rect(x + 4, y + 8, 3, 2, OFF);
      continue;
    }
    rect(x, y + 2, 11, 14, c % 4 ? OFF : OFF_BEAT);
    if (!v) {
      continue;
    }
    if (t < T_BASS) { /* a shorter cell: it may skip */
      rect(x, y + 16 - h, 11, h, on);
    } else { /* bass, synth: the height is the note */
      rect(x, y + 12 - (tr->note[i] * 10) / NOTE_MAX, 11, 4, PROB(tr, i) ? HALF(on) : on);
    }
    for (int r = 1; r <= rt; r++) { /* repeats: the cell split */
      rect(x + (11 * r) / (rt + 1), y + 2, 1, 14, BG);
    }
    if (tr->lk[i]) { /* a lock: a dot in the corner */
      rect(x + 1, y + 3, 2, 2, WHITE);
    }
  }
}

static void draw_pages(void) { /* the four pages: the one shown white, the one playing in the accent */
  int y = GRID_Y + NT * ROW_H + 1, ml = max_len();
  uint8_t pp = playing && ui_tpos[sel] < NSMAX ? ui_tpos[sel] / NS : 0xFF;
  for (int g = 0; g < 4; g++) {
    uint16_t c = g == spage ? WHITE : g == pp ? ACCENT : g * NS < ml ? DIM : OFF;
    rect(4 + g * 7, y, 5, 3, c);
  }
  drawn_pg = pp;
}

static void draw_playhead(int step) {
  int y = GRID_Y + NT * ROW_H + 1;
  if (drawn_step < NS) {
    rect(CELL_X(drawn_step), y, 11, 3, BG);
  }
  if (step < NS) {
    rect(CELL_X(step), y, 11, 3, kb_rec ? ALERT : WHITE);
  }
  drawn_step = (uint8_t)step;
}

/* ---- the rings: one per track, kick inside, the 16 steps of the page each track is on, clockwise from the top */
#define RCX 120
#define RCY 149
#define RR0 24
#define RDR 9
#define RIM0 (RR0 + NT * RDR + 3)
#define RIM1 (RIM0 + 4)
static int16_t bv[NS + 1][2]; /* where each step starts: unit vectors, Q12 */

static void rings_init(void) {
  for (int k = 0; k <= NS; k++) {
    uint32_t ph = 0xC0000000u + (uint32_t)k * 0x10000000u;
    bv[k][0] = (int16_t)(sine(ph + 0x40000000u) >> 3);
    bv[k][1] = (int16_t)(sine(ph) >> 3);
  }
}

/* step k of the band rin..rout, in runs of pixels; a gap of about a pixel either side */
static void sector(int k, int rin, int rout, uint16_t col) {
  int x0 = SCREEN_W, x1 = 0, y0 = SCREEN_H, y1 = 0, r2i = rin * rin, r2o = rout * rout;
  for (int e = 0; e < 2; e++) {
    for (int q = 0; q < 2; q++) {
      int r = q ? rout : rin, px = RCX + ((bv[k + e][0] * r) >> 12), py = RCY + ((bv[k + e][1] * r) >> 12);
      x0 = px < x0 ? px : x0;
      x1 = px > x1 ? px : x1;
      y0 = py < y0 ? py : y0;
      y1 = py > y1 ? py : y1;
    }
  }
  x0 -= 1;
  y0 -= 1;
  x1 += 1;
  y1 += 1;
  for (int y = y0; y <= y1; y++) {
    int dy = y - RCY, run = -1;
    for (int x = x0; x <= x1 + 1; x++) {
      int in = 0;
      if (x <= x1) {
        int dx = x - RCX, r2 = dx * dx + dy * dy;
        in = r2 >= r2i && r2 < r2o && bv[k][0] * dy - bv[k][1] * dx >= 4600 && bv[k + 1][0] * dy - bv[k + 1][1] * dx <= -4600;
      }
      if (in && run < 0) {
        run = x;
      } else if (!in && run >= 0) {
        rect(run, y, x - run, 1, col);
        run = -1;
      }
    }
  }
}

static void draw_ring(int t) {
  const track_t *tr = &P.tr[t];
  int len = tlen(tr), pg = playing ? ui_tpos[t] / NS : spage;
  ring_pg[t] = (uint8_t)pg;
  for (int k = 0; k < NS; k++) {
    int i = pg * NS + k, v = STEP(tr, i);
    uint16_t c = k % 4 ? OFF : OFF_BEAT;
    if (i >= len) {
      c = BG;
    } else if (v) {
      c = v == 2 ? TCOL[t] : HALF(TCOL[t]);
      if (PROB(tr, i)) {
        c = HALF(c);
      }
      if (tr->mute) {
        c = HALF(HALF(TCOL[t]));
      }
    }
    sector(k, RR0 + t * RDR + 1, RR0 + (t + 1) * RDR - 1, c);
  }
}

static void draw_center(void) {
  char b[2] = {(char)('A' + S.cur), 0};
  rect(RCX - 12, RCY - 16, 24, 32, BG);
  text(RCX - text_width(b, FONT_BIG) / 2, RCY - 16, b, ACCENT, FONT_BIG);
}

static void draw_rim(int step) {
  if (drawn_step < NS) {
    sector(drawn_step, RIM0, RIM1, BG);
  }
  if (step < NS) {
    sector(step, RIM0, RIM1, WHITE);
  }
  drawn_step = (uint8_t)step;
}

/* ---- the song: 16 parts, 4 x 4 */
static void draw_part(int i) {
  const part_t *pt = &S.song[i];
  int x = 6 + (i % 4) * 58, y = 64 + (i / 4) * 44;
  char b[6];
  rect(x, y, 54, 40, pt->slot ? SEL_BG : BG);
  frame(x, y, 54, 40, (playing && S.song_on && i == song_pos) ? WHITE : OFF);
  if (pt->slot) {
    b[0] = (char)('A' + pt->slot - 1);
    b[1] = 0;
    text(x + 6, y + 4, b, ACCENT, FONT_BIG);
    b[0] = 'x';
    format_int(b + 1, pt->bars);
    text(x + 50 - text_width(b, FONT_SMALL), y + 21, b, DIM, FONT_SMALL);
  } else {
    format_int(b, i + 1);
    text(x + 6, y + 12, b, OFF_BEAT, FONT_SMALL);
  }
}

/* ---- the effects guide (FX held): what each white key does, lit while it acts */
static uint16_t fx_active(void) {
  uint16_t a = fxkeys;
  if (building) {
    a |= 1u << FX_BUILD;
  }
  return a;
}

static void draw_fx(int i) {
  int x = 6 + (i % 4) * 58, y = 64 + (i / 4) * 44, on = (fx_active() >> i) & 1;
  char b[4];
  rect(x, y, 54, 40, on ? ALERT : SEL_BG);
  format_int(b, i + 1);
  text(x + 4, y + 2, b, on ? BG : DIM, FONT_SMALL);
  text(x + (54 - text_width(FX_NAME[i], FONT_SMALL)) / 2, y + 20, FX_NAME[i], on ? BG : WHITE, FONT_SMALL);
}

/* the four knobs of each page: ENV, LFO (the selected track), GLO, and GLO again (mix and MIDI) */
static const char *const KNOB_NAME[4][4] = {
  {"DENS", "CAOS", "FILT", "ECO"}, {"SOM", "NIVL", "PASS", "EUCL"}, {"TOM", "ESCL", "SWNG", "CORT"}, {"REVB", "DUCK", "FDBK", "MIDI"}};
static const char *const MIDI_NAME[4] = {"INT", "EXT", "INT+N", "EXT+N"};

static uint8_t *knob_ptr(int k, int *min, int *max) {
  track_t *tr = &P.tr[sel];
  *min = 0;
  *max = 127;
  switch (page * 4 + k) {
    case 0:
      return &P.dens;
    case 1:
      return &P.chaos;
    case 2:
      return &S.filt;
    case 3:
      return &S.eco;
    case 4:
      *max = NVOICE[sel] - 1;
      return &tr->voice;
    case 5:
      return &tr->level;
    case 6:
      *min = 1;
      *max = NSMAX;
      return &tr->len;
    case 7:
      *max = tlen(tr);
      return &tr->eu;
    case 8:
      *max = 11;
      return &P.root;
    case 9:
      *max = 3;
      return &P.scale;
    case 10:
      *max = 40;
      return &P.swing;
    case 11:
      return &S.cut;
    case 12:
      return &S.rev;
    case 13:
      return &S.duck;
    case 14:
      *max = 110;
      return &S.fdbk;
    default:
      *max = 3;
      return &S.midi;
  }
}

/* a knob's name, value and label colour, for the knob row and the corners */
static void knob_text(int k, const char **name, const char **val, uint16_t *lc) {
  static char v[4][12];
  int min, max, x = *knob_ptr(k, &min, &max), id = page * 4 + k;
  *name = KNOB_NAME[page][k];
  *val = v[k];
  *lc = page == 1 ? TCOL[sel] : DIM;
  if (held_step >= 0) { /* a step held: its chance, repeats and locks */
    const track_t *tr = &P.tr[sel];
    int i = spage * NS + held_step;
    static const char *const CH[4] = {"100", "75", "50", "25"}, *const RP[3] = {"x1", "x2", "x3"};
    static const char *const NM[4] = {"CHNC", "REPT", "", "VOL"};
    *name = k == 2 ? (sel >= T_BASS ? "CORT" : "DEC") : NM[k];
    *lc = TCOL[sel];
    if (k == 0) {
      *val = CH[PROB(tr, i)];
    } else if (k == 1) {
      *val = RP[RAT(tr, i) % 3];
    } else {
      int l = k == 2 ? tr->lk[i] & 15 : tr->lk[i] >> 4;
      if (l) {
        format_int(v[k], l);
      } else {
        *val = "--";
      }
    }
  } else if (id == 2) {
    format_int(v[k], x - 64);
  } else if (id == 4) {
    *val = VOICE_NAME[sel][x % NVOICE[sel]];
  } else if (id == 7 && !x) {
    *val = "--";
  } else if (id == 8) {
    *val = NOTE_NAME[x % 12];
  } else if (id == 9) {
    *val = SCALE_NAME[x & 3];
  } else if (id == 15) {
    *val = MIDI_NAME[x & 3];
  } else {
    format_int(v[k], x);
  }
}

static void draw_knob(int k) { /* the knob row of the grid */
  const char *name, *s;
  uint16_t lc;
  int x = k * 60, f;
  knob_text(k, &name, &s, &lc);
  f = text_width(s, FONT_BIG) > 58 ? FONT_SMALL : FONT_BIG;
  rect(x, 180, 60, 56, BG);
  text(x + (60 - text_width(name, FONT_SMALL)) / 2, 180, name, lc, FONT_SMALL);
  text(x + (60 - text_width(s, f)) / 2, f == FONT_BIG ? 198 : 206, s, ACCENT, f);
}

static void draw_corner(int k) { /* the rings' corners: KNOB1 top left .. KNOB4 bottom right */
  const char *name, *s;
  uint16_t lc;
  int right = k & 1, y = k < 2 ? 60 : 200, x0 = right ? 190 : 0, f;
  knob_text(k, &name, &s, &lc);
  f = text_width(s, FONT_BIG) > 48 ? FONT_SMALL : FONT_BIG;
  rect(x0, y, 50, 40, BG);
  text(right ? 236 - text_width(name, FONT_SMALL) : 4, y, name, lc, FONT_SMALL);
  text(right ? 236 - text_width(s, f) : 4, y + 16, s, ACCENT, f);
}

static int accel(int e, int32_t detents) {
  static uint32_t last[ENC_COUNT];
  uint32_t now = millis(), dt = now - last[e];
  if (!detents) {
    return 0;
  }
  last[e] = now;
  return detents * (dt < 25 ? 4 : dt < 60 ? 2 : 1);
}

static void say(const char *s) {
  msg = s;
  msg_until = millis() + 1500u;
  dirty |= D_STATUS;
}

static void all_dirty(void) {
  dirty = D_HEAD | D_KNOBS | D_STATUS;
  rows_dirty = 0x3F;
  knobs_dirty = 0xF;
  center_dirty = 1;
  parts_dirty = 0xFFFF;
  fx_drawn = 0xFFFF;
  drawn_pg = 0xFE;
}

static void touch(void) { last_edit = millis(); } /* an edit: the grid comes up for a while */

static void make_slot(int s) {
  if (!S.pat[s].used) { /* a new pattern starts as a variation of this one */
    S.pat[s] = P;
    gen_all(&S.pat[s]);
  }
}

static void go_slot(int s) {
  make_slot(s);
  if (playing) {
    slot_req = (uint8_t)s;
    say(s == 0 ? "PROX: A" : s == 1 ? "PROX: B" : s == 2 ? "PROX: C" : "PROX: D");
  } else {
    S.cur = play_slot = (uint8_t)s;
    undo_ok = 0;
    set_tempo();
    all_dirty();
  }
}

/* ---- projects */
static void init_project(project_t *p) {
  for (uint32_t i = 0; i < sizeof *p; i++) {
    ((uint8_t *)p)[i] = 0;
  }
  p->magic = MAGIC;
  p->filt = 64;
  p->eco = 40;
  p->rev = 30;
  p->duck = 60;
  p->cut = 92;
  p->fdbk = 50;
  init_pattern(&p->pat[0], 0);
}

static void sanitize(project_t *pj) { /* whatever is in flash: keep it in range */
  pj->cur &= 3;
  pj->midi &= 3;
  if (!pj->pat[pj->cur].used) {
    init_pattern(&pj->pat[pj->cur], 0);
  }
  for (int s = 0; s < 4; s++) {
    pattern_t *p = &pj->pat[s];
    p->style %= NSTYLE;
    p->scale &= 3;
    p->root %= 12;
    p->bpm = p->bpm < 70 ? 70 : p->bpm > 180 ? 180 : p->bpm;
    p->swing = p->swing > 40 ? 40 : p->swing;
    for (int t = 0; t < NT; t++) {
      track_t *tr = &p->tr[t];
      tr->voice %= NVOICE[t];
      tr->len = (uint8_t)tlen(tr);
      tr->eu = tr->eu > tr->len ? tr->len : tr->eu;
      tr->rot %= NSMAX;
      for (int i = 0; i < NSMAX; i++) {
        tr->note[i] = tr->note[i] > NOTE_MAX ? NOTE_MAX : tr->note[i];
        if (STEP(tr, i) == 3 || RAT(tr, i) == 3) {
          tr->st[i] = 0;
        }
      }
    }
  }
  for (int i = 0; i < 16; i++) {
    part_t *pt = &pj->song[i];
    pt->slot = pt->slot > 4 ? 0 : pt->slot;
    pt->bars = pt->bars < 1 ? 1 : pt->bars > 64 ? 64 : pt->bars;
    if (pt->slot && !pj->pat[pt->slot - 1].used) {
      pt->slot = 0;
    }
  }
}

static void apply_project(void) {
  play_slot = S.cur;
  undo_ok = 0;
  clock_follow(S.midi & 1 ? MIDI_ANY : 0);
  set_tempo();
  all_dirty();
}

static void switch_project(int to) {
  store.p[store.proj] = S;
  store.proj = (uint8_t)to;
  if (store.p[to].magic != MAGIC) {
    init_project(&store.p[to]);
  }
  S = store.p[to];
  sanitize(&S);
  apply_project();
  say(to == 0 ? "PROJETO 1" : "PROJETO 2");
}

void setup(void) {
  if (settings_load(&store, sizeof store) || store.magic != MAGIC || store.proj >= NPROJ) {
    for (uint32_t i = 0; i < sizeof store; i++) {
      ((uint8_t *)&store)[i] = 0;
    }
    store.magic = MAGIC;
    for (uint32_t s = millis() & 15u; s; s--) {
      random32();
    }
    init_project(&store.p[0]);
  }
  if (store.p[store.proj].magic != MAGIC) {
    init_project(&store.p[store.proj]);
  }
  S = store.p[store.proj];
  sanitize(&S);
  for (int i = 0; i < NSTYLE; i++) { /* PRESETS goes through the styles slowest first */
    int j = i;
    while (j > 0 && STYLE[ORDER[j - 1]].bpm > STYLE[i].bpm) {
      ORDER[j] = ORDER[j - 1];
      j--;
    }
    ORDER[j] = (uint8_t)i;
  }
  rings_init();
  clear(BG);
  led_backlight(1);
  clock_send(1);
  apply_project();
}

static void queue_note(int t, int deg) {
  lq_t[lq_w & 7] = (uint8_t)t;
  lq_d[lq_w & 7] = (uint8_t)deg;
  lq_w++;
}

static void kb_key(int k) { /* a white key in keyboard mode: play it, and record it when armed */
  track_t *tr = &P.tr[sel];
  int deg = sel >= T_BASS ? k : 0, len = tlen(tr), at;
  queue_note(sel, deg);
  if (!kb_rec) {
    return;
  }
  if (playing) { /* to the nearer step */
    at = ui_tpos[sel] % len;
    if (since_fire * 2u > step_frames) {
      at = (at + 1) % len;
    }
  } else { /* step by step */
    at = kb_cur % len;
    kb_cur = (uint8_t)((at + 1) % len);
  }
  set_st(tr, at, 2, 0, 0);
  tr->note[at] = (uint8_t)deg;
  tr->lk[at] = 0;
  rows_dirty |= (uint8_t)(1u << sel);
  touch();
}

static void pick_style(int d) { /* PRESETS: in tempo order; while playing it waits for the next bar */
  int cur = pend_style < NSTYLE ? pend_style : P.style, at = 0, s;
  for (int i = 0; i < NSTYLE; i++) {
    if (ORDER[i] == cur) {
      at = i;
    }
  }
  at += d;
  at = at < 0 ? 0 : at >= NSTYLE ? NSTYLE - 1 : at;
  s = ORDER[at];
  if (s == cur) {
    return;
  }
  snapshot();
  if (playing) {
    stage_req = 0;
    stage = P;
    set_style(&stage, s);
    pend_style = (uint8_t)s;
    stage_req = 1;
    dirty |= D_STATUS;
  } else {
    set_style(&P, s);
    set_tempo();
    all_dirty();
  }
}

void loop(void) {
  uint32_t now = millis();
  int t;
  static uint32_t seq_t0, arp_t0, home_t0, last_glide;
  static uint8_t seq_long, arp_long, home_long, save_combo, fx_was;
  int fx_held = button_held(BTN_FX) && !song_screen;

  /* ---- what audio() reports */
  if (playing && play_slot != S.cur) { /* the pattern changed at the bar */
    S.cur = play_slot;
    undo_ok = 0;
    all_dirty();
  }
  if (swapped) { /* the new style came in */
    swapped = 0;
    pend_style = 0xFF;
    all_dirty();
  }
  if (!playing && stage_req) { /* stopped before the bar: it comes in now */
    stage_req = 0;
    P = stage;
    pend_style = 0xFF;
    all_dirty();
  }
  if (want_mutate) {
    want_mutate = 0;
    mutate(&P);
    rows_dirty = 0x3F;
  }

  /* ---- the tempo: from a clock coming in, or P.bpm, gliding there while playing */
  if (following()) {
    uint32_t b = clock_bpm();
    if (b != bpm_now) {
      bpm_now = b;
      step_frames = 66150000u / b;
      delay_frames = 3u * step_frames;
      dirty |= D_HEAD;
    }
  } else {
    uint32_t tgt = (uint32_t)(P.bpm < 70 ? 70 : P.bpm) * 100u;
    if (!playing || !bpm_now) {
      bpm_now = tgt;
    } else if (now - last_glide >= 20u && bpm_now != tgt) { /* ~1 s to most of the way, a few to all of it */
      int32_t d = (int32_t)tgt - (int32_t)bpm_now, st = d / 40;
      last_glide = now;
      bpm_now = (uint32_t)((int32_t)bpm_now + (st ? st : d > 0 ? 1 : -1));
    }
    if (bpm_now != bpm_set) {
      if (bpm_now / 100u != bpm_set / 100u) {
        dirty |= D_HEAD;
      }
      bpm_set = bpm_now;
      clock_tempo(bpm_now);
      step_frames = 66150000u / bpm_now;
      delay_frames = 3u * step_frames;
    }
  }

  /* ---- buttons */
  if (button_pressed(BTN_PLAY)) {
    playing = !playing;
    if (playing) {
      clock_start();
    } else {
      clock_stop();
      perf_loop = 0;
    }
    dirty |= D_STATUS;
    rows_dirty = 0x3F;
    parts_dirty = 0xFFFF;
  }
  if (button_pressed(BTN_REC)) {
    if (song_screen) {
      if (!S.song[0].slot) {
        say("MUSICA VAZIA");
      } else {
        S.song_on = !S.song_on;
        dirty |= D_STATUS;
        parts_dirty = 0xFFFF;
      }
    } else if (kb_mode) {
      kb_rec = !kb_rec;
      if (kb_rec) {
        snapshot();
        kb_cur = (uint8_t)(spage * NS);
      }
      dirty |= D_STATUS;
      drawn_step = 0xFF;
    } else {
      snapshot();
      gen_all(&P);
      rows_dirty = 0x3F;
    }
  }
  if (button_pressed(BTN_ARP)) { /* a tap: vary (or leave the keyboard); held: the keyboard */
    arp_t0 = now;
    arp_long = 0;
  }
  if (button_held(BTN_ARP) && !arp_long && !song_screen && now - arp_t0 >= 600u) {
    arp_long = 1;
    kb_mode = !kb_mode;
    kb_rec = 0;
    dirty |= D_STATUS;
  }
  if (button_released(BTN_ARP) && !arp_long) {
    if (kb_mode) {
      kb_mode = kb_rec = 0;
      dirty |= D_STATUS;
    } else {
      snapshot();
      mutate(&P);
      rows_dirty = 0x3F;
    }
  }
  if (button_pressed(BTN_SEQ)) { /* a tap: vary by itself; held: the song */
    seq_t0 = now;
    seq_long = 0;
  }
  if (button_held(BTN_SEQ) && !seq_long && !song_screen && now - seq_t0 >= 600u) {
    seq_long = 1;
    song_screen = 1;
    kb_mode = kb_rec = 0;
  }
  if (button_released(BTN_SEQ) && !seq_long) {
    if (song_screen) {
      song_screen = 0;
    } else {
      S.evolve = !S.evolve;
      dirty |= D_STATUS;
    }
  }
  if (button_pressed(BTN_HOME)) { /* a tap: rings or grid; held: undo (and again: redo) */
    home_t0 = now;
    home_long = 0;
  }
  if (button_held(BTN_HOME) && !home_long && now - home_t0 >= 600u) {
    home_long = 1;
    if (undo_ok) {
      pattern_t tmp = P;
      P = undo_buf;
      undo_buf = tmp;
      set_tempo();
      all_dirty();
      say("DESFEITO");
    }
  }
  if (button_released(BTN_HOME) && !home_long) {
    if (view == V_GRID && playing) {
      pin_grid = 0;
      last_edit = now - 5000u;
    } else {
      pin_grid = 1;
    }
  }
  if (button_pressed(BTN_SAVE)) { /* a tap: save; held with OCT-/OCT+: the other project */
    save_combo = 0;
  }
  if (button_released(BTN_SAVE) && !save_combo) {
    if (playing) {
      say("PARE P/ SALVAR");
    } else {
      save_at = now + 300u; /* let the tails ring out: saving stalls the sound */
      say("SALVANDO");
    }
  }
  if (save_at && now >= save_at && !playing) {
    save_at = 0;
    store.p[store.proj] = S;
    say(settings_save(&store, sizeof store) ? "ERRO AO SALVAR" : "SALVO");
  }
  if (button_pressed(BTN_ENV) && page != 0) {
    page = 0;
    knobs_dirty = 0xF;
  }
  if (button_pressed(BTN_LFO) && page != 1) {
    page = 1;
    knobs_dirty = 0xF;
  }
  if (button_pressed(BTN_GLO)) { /* GLO, and again: mix and MIDI */
    page = page == 2 ? 3 : 2;
    knobs_dirty = 0xF;
  }
  for (int dir = 0; dir < 2; dir++) {
    if (!button_pressed(dir ? BTN_OCT_UP : BTN_OCT_DOWN)) {
      continue;
    }
    if (button_held(BTN_SAVE)) {
      save_combo = 1;
      if (playing) {
        say("PARE P/ TROCAR");
      } else {
        switch_project((store.proj + 1) % NPROJ);
      }
    } else {
      go_slot(((slot_req < 4 ? slot_req : S.cur) + (dir ? 1 : 3)) & 3);
    }
  }

  /* ---- the white keys: the steps of this page, the song's parts, the notes (keyboard), or the effects (FX held) */
  for (int k = 0; k < NS; k++) {
    int key = WK[k];
    if (key_pressed(key)) {
      turned[k] = 0;
      if (fx_held) {
        fxkeys |= (uint16_t)(1u << k);
        if (k == FX_BUILD && playing) {
          if (building) {
            build_cancel = 1;
          } else {
            build_req = 1;
          }
        }
      } else if (kb_mode) {
        kb_key(k);
      }
    }
    if (!key_released(key)) {
      continue;
    }
    if (fxkeys & (1u << k)) {
      fxkeys &= (uint16_t)~(1u << k);
    } else if (turned[k] || kb_mode) {
    } else if (song_screen) { /* a part: empty, A, B, C, D */
      part_t *pt = &S.song[k];
      pt->slot = (uint8_t)((pt->slot + 1) % 5);
      if (pt->slot == 1) {
        pt->bars = 4;
      }
      if (pt->slot) {
        make_slot(pt->slot - 1);
      }
      parts_dirty |= (uint16_t)(1u << k);
    } else {
      track_t *tr = &P.tr[sel];
      int i = spage * NS + k;
      if (i >= tlen(tr)) { /* past the end: the track grows to this page */
        tr->len = (uint8_t)((spage + 1) * NS);
        rows_dirty = 0x3F;
      }
      if (STEP(tr, i) == 2) {
        set_st(tr, i, 0, 0, 0);
      } else {
        set_st(tr, i, 2, 0, 0);
        tr->lk[i] = 0;
        if (!playing) {
          pv_t = sel;
          pv_s = (uint8_t)i;
          pv_req = 1;
        }
      }
      rows_dirty |= (uint8_t)(1u << sel);
      touch();
    }
  }
  { /* what the effects keys do while held */
    uint16_t f = 0;
    uint8_t lp;
    for (int k = 0; k < NS; k++) {
      if (fxkeys & (1u << k)) {
        f |= FXBIT[k];
      }
    }
    perf_fx = f;
    perf_break = (fxkeys >> FX_BREAK) & 1u;
    perf_fill = playing && ((fxkeys >> FX_FILL) & 1u);
    lp = !playing ? 0 : (fxkeys & 0x8000u) ? 1 : (fxkeys & 0x4000u) ? 2 : (fxkeys & 0x2000u) ? 4 : 0;
    if (lp != perf_loop) {
      if (lp) {
        uint32_t a = abs_step ? abs_step - 1u : 0u;
        perf_la = a / lp * lp;
      }
      perf_loop = lp;
    }
  }
  /* ---- the black keys: OP1-OP6 the tracks, then the pages, then x2 */
  for (t = 0; t < NT; t++) {
    if (!key_pressed(BK[t]) || song_screen) {
      continue;
    }
    if (button_held(BTN_FX)) {
      P.tr[t].mute = !P.tr[t].mute;
    } else if (button_held(BTN_EDIT)) {
      P.tr[t].lock = !P.tr[t].lock;
    } else {
      if (button_held(BTN_SEL)) {
        snapshot();
        reseed(&P.tr[t]);
        derive(&P, t);
      }
      if (sel != t) {
        rows_dirty |= (uint8_t)(1u << sel);
        sel = (uint8_t)t;
        knobs_dirty = 0xF;
      }
    }
    rows_dirty |= (uint8_t)(1u << t);
    touch();
  }
  for (int g = 0; g < 4; g++) {
    if (key_pressed(BK[K_PAGE + g]) && spage != g) {
      spage = (uint8_t)g;
      rows_dirty = 0x3F;
      drawn_pg = 0xFE;
      drawn_step = 0xFF;
      touch();
    }
  }
  if (key_pressed(BK[K_X2]) && !song_screen) {
    int any = 0;
    snapshot();
    for (t = 0; t < NT; t++) {
      any |= double_len(&P.tr[t]);
    }
    say(any ? "x2: MAIS LONGO" : "MAXIMO 64");
    rows_dirty = 0x3F;
    drawn_pg = 0xFE;
    touch();
  }
  { /* a step held: the knobs edit it */
    int h = -1;
    for (int k = 0; k < NS && !song_screen && !kb_mode && !fx_held; k++) {
      if (key_held(WK[k]) && !(fxkeys & (1u << k))) {
        h = k;
        break;
      }
    }
    if (h != held_step) {
      held_step = (int8_t)h;
      knobs_dirty = 0xF;
      if (h >= 0) {
        touch();
      }
    }
  }
  {
    static uint8_t was_building, was_pos, was_fx;
    uint8_t fxs = (uint8_t)((perf_fx != 0) | perf_fill << 1 | (perf_loop != 0) << 2 | perf_break << 3);
    if (was_building != building || was_pos != song_pos || was_fx != fxs) {
      if (was_pos != song_pos) {
        parts_dirty |= (uint16_t)((1u << (was_pos & 15)) | (1u << (song_pos & 15)));
      }
      was_building = building;
      was_pos = song_pos;
      was_fx = fxs;
      dirty |= D_STATUS;
    }
  }

  /* ---- encoders */
  {
    int d = encoder(ENC_PRESETS);
    if (d) {
      pick_style(d);
    }
    d = accel(ENC_SELECT, encoder(ENC_SELECT));
    if (d) {
      if (following()) {
        say("TEMPO EXTERNO");
      } else {
        int b = P.bpm + d;
        P.bpm = (uint8_t)(b < 70 ? 70 : b > 180 ? 180 : b);
        dirty |= D_HEAD;
      }
    }
    d = encoder(ENC_ALGORITHM);
    if (d && song_screen) { /* the bars of the held parts */
      for (int k = 0; k < NS; k++) {
        if (key_held(WK[k])) {
          part_t *pt = &S.song[k];
          int b = pt->bars + d;
          if (!pt->slot) {
            pt->slot = (uint8_t)(S.cur + 1);
          }
          pt->bars = (uint8_t)(b < 1 ? 1 : b > 64 ? 64 : b);
          turned[k] = 1;
          parts_dirty |= (uint16_t)(1u << k);
        }
      }
    } else if (d) {
      track_t *tr = &P.tr[sel];
      int held = 0;
      if (sel >= T_BASS && !kb_mode) {
        for (int k = 0; k < NS; k++) {
          int i = spage * NS + k;
          if (key_held(WK[k]) && !(fxkeys & (1u << k)) && i < tlen(tr)) {
            int v = tr->note[i] + d;
            tr->note[i] = (uint8_t)(v < 0 ? 0 : v > NOTE_MAX ? NOTE_MAX : v);
            if (!STEP(tr, i)) {
              set_st(tr, i, 2, 0, 0);
            }
            turned[k] = 1;
            held = 1;
            if (!playing) {
              pv_t = sel;
              pv_s = (uint8_t)i;
              pv_req = 1;
            }
          }
        }
      }
      if (!held) {
        static uint32_t last_rot;
        if (now - last_rot > 1000u) {
          snapshot();
        }
        last_rot = now;
        rotate(tr, d);
      }
      rows_dirty |= (uint8_t)(1u << sel);
      touch();
    }
    for (int k = 0; k < 4; k++) {
      static uint32_t last_gen;
      int min, max, id = page * 4 + k, v;
      uint8_t *p;
      int32_t raw = encoder(ENC_KNOB1 + k);
      if (!raw) {
        continue;
      }
      if (held_step >= 0) { /* the held step: chance, repeats, filter or decay lock, level lock */
        track_t *tr = &P.tr[sel];
        int i = spage * NS + held_step, dd = raw > 0 ? 1 : -1, stp = STEP(tr, i) ? STEP(tr, i) : 2;
        if (i >= tlen(tr)) {
          continue;
        }
        if (k == 0) {
          v = PROB(tr, i) - dd;
          set_st(tr, i, stp, v < 0 ? 0 : v > 3 ? 3 : v, RAT(tr, i));
        } else if (k == 1) {
          v = RAT(tr, i) + dd;
          set_st(tr, i, stp, PROB(tr, i), v < 0 ? 0 : v > 2 ? 2 : v);
        } else {
          int lo = tr->lk[i] & 15, hi = tr->lk[i] >> 4;
          if (k == 2) {
            v = lo ? lo + dd : (dd > 0 ? (sel >= T_BASS ? S.cut / 8 : 8) : 0);
            lo = v < 0 ? 0 : v > 15 ? 15 : v;
          } else {
            v = hi ? hi + dd : (dd > 0 ? 12 : 0);
            hi = v < 0 ? 0 : v > 15 ? 15 : v;
          }
          tr->lk[i] = (uint8_t)(lo | hi << 4);
          set_st(tr, i, stp, PROB(tr, i), RAT(tr, i));
        }
        turned[held_step] = 1;
        rows_dirty |= (uint8_t)(1u << sel);
        knobs_dirty |= (uint8_t)(1u << k);
        touch();
        continue;
      }
      d = accel(ENC_KNOB1 + k, raw);
      p = knob_ptr(k, &min, &max);
      if (id == 4 || id == 15) { /* a sound, a MIDI mode: one a detent */
        d = raw > 0 ? 1 : -1;
      }
      v = *p + d;
      if (id == 8 || id == 9) { /* the key and the scale go round */
        v = (v % (max + 1) + max + 1) % (max + 1);
      } else {
        v = v < min ? min : v > max ? max : v;
      }
      if (v == *p) {
        continue;
      }
      if (id < 2 || id == 7) { /* DENS, CAOS, EUCL: redraw from the same dice */
        if (now - last_gen > 1000u) {
          snapshot();
        }
        last_gen = now;
        *p = (uint8_t)v;
        if (id == 7) {
          derive(&P, sel);
        } else {
          derive_all(&P);
        }
        rows_dirty = 0x3F;
      } else {
        *p = (uint8_t)v;
      }
      if (id == 4) {
        rows_dirty |= (uint8_t)(1u << sel); /* its name may change */
        if (!playing) {
          queue_note(sel, 0);
        }
      } else if (id == 6) {
        track_t *tr = &P.tr[sel];
        if (tr->eu > tr->len) {
          tr->eu = tr->len;
        }
        if (tr->eu) {
          derive(&P, sel);
        }
        rows_dirty |= (uint8_t)(1u << sel);
        drawn_pg = 0xFE;
      } else if (id == 5) {
        rows_dirty |= (uint8_t)(1u << sel);
      } else if (id == 8 || id == 9) {
        dirty |= D_STATUS;
      } else if (id == 15) {
        clock_follow(S.midi & 1 ? MIDI_ANY : 0);
        dirty |= D_HEAD;
      }
      knobs_dirty |= (uint8_t)(1u << k);
    }
  }

  /* ---- lights */
  {
    uint8_t step = ui_step, tp = playing ? ui_tpos[sel] : (kb_rec ? kb_cur : 0xFF);
    const track_t *tr = &P.tr[sel];
    for (int k = 0; k < NS; k++) {
      int on, i = spage * NS + k;
      if (fx_held) { /* the effects: lit while acting, the rest dim-blinking to show they are there */
        on = (fx_active() >> k) & 1 || (now & 512u);
      } else if (kb_mode) { /* the roots of the scale, to find your way */
        on = sel >= T_BASS ? k % 7 == 0 : 0;
      } else if (song_screen) {
        on = S.song[k].slot && !(playing && S.song_on && k == song_pos && (step & 2));
      } else {
        on = (STEP(tr, i) != 0 && i < tlen(tr)) ^ (i == tp);
      }
      led_key(WK[k], on);
    }
    for (t = 0; t < NT; t++) {
      uint8_t pt = ui_tpos[t];
      led_key(BK[t], t == sel ? 1 : (playing && pt < NSMAX && STEP(&P.tr[t], pt) && !P.tr[t].mute));
    }
    for (int g = 0; g < 4; g++) {
      int on = g == spage || (playing && tp < NSMAX && tp / NS == g && (now & 256u));
      led_key(BK[K_PAGE + g], on && g * NS < max_len() + NS);
    }
    led_key(BK[K_X2], 0);
    led_button(BTN_PLAY, playing && (step >= NS || (step & 3) < 2));
    led_button(BTN_FX, fx_held || perf_fx || perf_break || perf_fill || perf_loop);
    led_button(BTN_SEQ, S.evolve || song_screen);
    led_button(BTN_ARP, kb_mode);
    led_button(BTN_REC, (song_screen && S.song_on) || (kb_mode && kb_rec && (now & 512u)));
    led_button(BTN_ENV, page == 0);
    led_button(BTN_LFO, page == 1);
    led_button(BTN_GLO, page >= 2);
    led_button(BTN_HOME, undo_ok);
  }

  /* ---- the screen: only what changed, a little each pass */
  {
    uint8_t want = song_screen ? V_SONG : fx_held ? V_FX : (!playing || kb_mode || pin_grid || now - last_edit < 2500u) ? V_GRID : V_RINGS;
    if (want == V_RINGS && fx_was) {
      fx_was = 0;
    }
    if (want != view) {
      view = want;
      rect(0, 58, SCREEN_W, SCREEN_H - 58, BG);
      all_dirty();
      drawn_step = 0xFF;
      return; /* the clearing is enough for this pass */
    }
  }
  if (msg && now >= msg_until) {
    msg = 0;
    dirty |= D_STATUS;
  }
  if (dirty & D_HEAD) {
    draw_head();
  }
  if (dirty & D_STATUS) {
    draw_status();
  }
  if (dirty & D_KNOBS) {
    knobs_dirty = 0xF;
  }
  dirty = 0;
  if (view == V_FX) {
    uint16_t a = fx_active(), ch = a ^ fx_drawn;
    for (int i = 0; i < NS; i++) {
      if (ch & (1u << i)) {
        draw_fx(i);
        fx_drawn ^= (uint16_t)(1u << i);
        break;
      }
    }
    return;
  }
  if (view == V_SONG) {
    for (int i = 0; i < 16; i++) {
      if (parts_dirty & (1u << i)) {
        parts_dirty &= (uint16_t)~(1u << i);
        draw_part(i);
        break;
      }
    }
    return;
  }
  if (view == V_RINGS) {
    if (center_dirty) {
      center_dirty = 0;
      draw_center();
    }
    if (knobs_dirty) {
      for (int k = 0; k < 4; k++) {
        if (knobs_dirty & (1u << k)) {
          knobs_dirty &= (uint8_t)~(1u << k);
          draw_corner(k);
          return;
        }
      }
    }
    if (ui_step != drawn_step) {
      draw_rim(ui_step);
      return;
    }
    for (t = 0; t < NT; t++) { /* a track turned to its next page: its ring again */
      if (ui_tpos[t] / NS != ring_pg[t]) {
        rows_dirty |= (uint8_t)(1u << t);
      }
    }
    for (t = 0; t < NT; t++) {
      if (rows_dirty & (1u << t)) {
        rows_dirty &= (uint8_t)~(1u << t);
        draw_ring(t);
        break;
      }
    }
    return;
  }
  if (knobs_dirty) {
    for (int k = 0; k < 4; k++) {
      if (knobs_dirty & (1u << k)) {
        draw_knob(k);
      }
    }
    knobs_dirty = 0;
  }
  {
    uint8_t tp = playing ? ui_tpos[sel] : (kb_rec ? kb_cur : 0xFF), ph = 0xFF, pp = playing ? tp / NS : 0xFF;
    if (tp < NSMAX && tp / NS == spage) {
      ph = tp % NS;
    }
    if (ph != drawn_step) {
      draw_playhead(ph);
    }
    if (pp != drawn_pg) {
      draw_pages();
    }
  }
  for (t = 0; t < NT; t++) {
    if (rows_dirty & (1u << t)) {
      rows_dirty &= (uint8_t)~(1u << t);
      draw_row(t);
      break;
    }
  }
}
