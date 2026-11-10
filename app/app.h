/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 app: the project (everything that is saved), the app's entry points and its half of the contract with
 * fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "amb.h"

#define APP_NAME "StoryBox"
#define APP_SPLASH                       /* the loading screen in StoryBox's colours (ui_splash) */
void ui_splash(void);
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#ifdef APP_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION APP_VERSION
#else
#define EN_VERSION "0.6.7"               /* StoryBox's version, shown on the System page (the install page's too) */
#endif

/* the store: the project at 0xFC000 and the panel calibration at 0xDC000, where AMB-1 0.9 kept them (an update keeps
 * the pieces) */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_BLE, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u}   /* (0.6: OBJ_BLE, the radio's address and the speakers known) */
#define APP_BLE_AT_BOOT (proj.bt_off == 0)  /* Bluetooth at power-on unless the Bluetooth page turned it off */
#define PROJ_MAGIC 0x31425453u           /* StoryBox: "STB1" (AMB-1's "ENO1" at the same place is not read: defaults) */
#define PROJ_FORMAT 8u                   /* 2: Freedom; 3: Tension, Meter, Energy; 4: the lead; 5: Groove; 6: punch-ins;
                                          * 7: Fly, Intermission, Surf, Melody; 8: the slots in bytes, Post Show, the
                                          * instruments' effects (7 read: kept, Fly's acts and the new ones from their
                                          * recipes) */
#define NPIECES EN_NPIECES

typedef struct {                         /* (every value fits a byte: Tempo up to 120, Tune down to -50) */
    int8_t par[P_NPARAMS];
    uint16_t pool;
} slot_t;

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* the piece playing, as it is now (amb.h P_*) */
    uint16_t pool;                       /* its scale, relative to the root (bit 0 = the root) */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t piece;                       /* which piece: story * EN_NACT + act */
    uint8_t vibe;                        /* (unused: Wander 0 is Perform) */
    uint8_t wander;                      /* 0 Perform (acts wait for you); 1..50 the acts in order, 51..100 roaming, more and more often */
    uint8_t fade;                        /* a held act's crossfade, 0..100 (5 s .. 2 min) */
    uint8_t bt_off;                      /* 0.6: Bluetooth off (the Bluetooth page); 0 on (an older save: on) */
    uint8_t answer;                      /* 0.6: the lead's answer, 0 off .. 100 free (an older save: off) */
    uint8_t recipes;                     /* 0.7: the recipes' changes brought into saved acts (0: none yet) */
    slot_t slot[NPIECES];                /* every piece as it was left (Tune and MIDI out are global) */
    uint8_t rsv[16];                     /* room to grow */
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
void project_apply(void);                /* every value to the engine (boot, load) */
const char *piece_name(int i);
void project_piece(int i);               /* the playing piece kept, piece i taken up as it was left */
void project_piece_reset(void);          /* the playing piece back to its recipe */
void project_target(int i, int16_t *par, uint16_t *pool);   /* piece i as it was left, the globals as now */
void project_keep(void);                 /* the playing piece into its slot */
extern int proj_hold_keep;               /* set while a transition runs: the slots are left alone */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved project (or the defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

/* the host simulator's part (app/host.c): AMB-1's checks, its commands, the screen reader's dump */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
#define APP_HOST_SPEAKDUMP
void app_speak_dump(void);
#define APP_HOST_REBOOT
void app_host_reboot(void);              /* the project forgotten, as RAM is at a restart */
