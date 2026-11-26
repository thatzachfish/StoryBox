/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 on fm1-base: the app's unity build (compiled into fm1-base's, after the platform) and the glue between
 * fm1-base's contract (app.h) and AMB-1's parts: the project (project.c), the UI (ui.c) and the engine (dsp/amb.c,
 * its own -O2 unit). */
#include "project.c"
#include "ui.c"

/* the engine's MIDI out (clock, notes when MIDI out is on): to USB */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

/* ---- the contract */
void app_init(void)
{
    en_init();
    project_load();                      /* defaults when nothing (or another format) is saved */
    project_apply();
    plat_stats_enable(1);                /* (0.7.1-bt: the debug stats on, for the Bluetooth hunt) */
}
static int32_t bt_copy[2u * APP_BLOCK];          /* (0.8.24) the render as made, for Bluetooth */
static uint8_t bt_copied;
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12)
{
    const int quiet = app_spk_off || app_bt_quiet;
    uint32_t i;
    if (quiet) {                                 /* (0.8.21) the FM-1's own output off: the USB recording (and
                                                  * Bluetooth) at MASTER's level, the render's own gain after it 0 */
        const uint32_t m = plat_master();
        gain_q12 = m * m >> 12;
    }
    en_render(lr, n, gain_q12);
    bt_copied = 0;
    if (quiet && n <= APP_BLOCK) {               /* kept for Bluetooth before the gain after it mutes it */
        for (i = 0; i < 2u * n; i++)
            bt_copy[i] = lr[i];
        bt_copied = 1;
    }
}
const int32_t *app_bt_frames(const int32_t *lr, uint32_t n)
{
    (void)n;
    return bt_copied ? bt_copy : lr;
}
void app_overload(void) { en_overload(); }
void app_panic(void)
{
    en_play(0);
    en_panic();
}
void app_ui_init(void) { ui_init(); }
void app_splash(void) { ui_splash(); }
void app_frame(void) { ui_frame(); }
void app_input_only(void) { ui_input_only(); }
void app_say(const char *a, const char *b) { ui_say(a, b); }

#ifdef PLAT_HOST
#include "host.c"                        /* the host simulator's part: checks and commands */
#endif
