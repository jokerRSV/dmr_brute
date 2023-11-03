/*
 * Copyright (C) 2010 DSD Author
 * GPG Key ID: 0x3F1D7FD0 (74EF 430D F7F2 0A48 FCE6  F630 FAA2 635D 3F1D 7FD0)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>

#include "dsd.h"

static int digitize(dsd_state *state, int symbol) {
    // determine dibit state
    if ((state->synctype == 6) || (state->synctype == 14) || (state->synctype == 18) || (state->synctype == 37)) {
        //  6 +D-STAR
        // 14 +ProVoice
        // 18 +D-STAR_HD
        // 37 +EDACS
    } else if ((state->synctype == 7) || (state->synctype == 15) || (state->synctype == 19) ||
               (state->synctype == 38)) {
        //  7 -D-STAR
        // 15 -ProVoice
        // 19 -D-STAR_HD
        // 38 -EDACS
    } else if ((state->synctype == 1) || (state->synctype == 3) || (state->synctype == 5) ||
               (state->synctype == 9) || (state->synctype == 11) || (state->synctype == 13) ||
               (state->synctype == 17) || (state->synctype == 29) || (state->synctype == 36)) {
        //  1 -P25p1
        //  3 -X2-TDMA (inverted signal voice frame)
        //  5 -X2-TDMA (inverted signal data frame)
        //  9 -NXDN (inverted voice frame)
        // 11 -DMR (inverted signal voice frame)
        // 13 -DMR (inverted signal data frame)
        // 17 -NXDN (inverted data frame)
        // 29 -NXDN (inverted FSW)
        // 36 -P25p2
    } else {
        //  0 +P25p1
        //  2 +X2-TDMA (non inverted signal data frame)
        //  4 +X2-TDMA (non inverted signal voice frame)
        //  8 +NXDN (non inverted voice frame)
        // 10 +DMR (non inverted signal data frame)
        // 12 +DMR (non inverted signal voice frame)
        // 16 +NXDN (non inverted data frame)
        // 28 +NXND (FSW)
        // 35 +p25p2

        int dibit;

        if (symbol > state->center) {
            if (symbol > state->umid) {
                dibit = 1;               // +3
            } else {
                dibit = 0;               // +1
            }
        } else {
            if (symbol < state->lmid) {
                dibit = 3;               // -3
            } else {
                dibit = 2;               // -1
            }
        }

        state->last_dibit = dibit;

        *state->dibit_buf_p = dibit;
        state->dibit_buf_p++;

        //dmr buffer
        //note to self, perceived bug with initial dibit buffer appears to be caused by
        //media player, when playing back from audacity, the initial few dmr frames are
        //decoded properly, need to investigate the root cause of what audacity is doing
        //vs other audio sources...perhaps just the audio level itself?
        *state->dmr_payload_p = dibit;
        state->dmr_payload_p++;
        //dmr buffer end

        return dibit;
    }
}

int
get_dibit_and_analog_signal(dsd_opts *opts, dsd_state *state) {
    int symbol;
    int dibit;

    state->numflips = 0;
    symbol = getSymbol(opts, state, 1);
    state->sbuf[state->sidx] = symbol;
    dibit = digitize(state, symbol);
    return dibit;
}

/**
 * \brief This important method reads the last analog signal value (getSymbol call) and digitizes it.
 * Depending of the ongoing transmission it in converted into a bit (0/1) or a di-bit (00/01/10/11).
 */
int
getDibit(dsd_opts *opts, dsd_state *state) {
    return get_dibit_and_analog_signal(opts, state);
}

void
skipDibit(dsd_opts *opts, dsd_state *state, int count) {

    short sample;
    int i;

    for (i = 0; i < (count); i++) {
        sample = getDibit(opts, state);
    }
}
