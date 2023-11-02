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

#include "dsd.h"

int
getSymbol(dsd_opts *opts, dsd_state *state, int have_sync) {
    short sample, sample2;
    int i, sum, symbol, count;
    ssize_t result;

    sum = 0;
    count = 0;
    sample = 0; //init sample with a value of 0...see if this was causing issues with raw audio monitoring

    for (i = 0; i < state->samplesPerSymbol; i++) //right HERE
    {

        // timing control
        if ((i == 0) && (have_sync == 0)) {
            if (state->samplesPerSymbol == 20) {
                if ((state->jitter >= 7) && (state->jitter <= 10)) {
                    i--;
                } else if ((state->jitter >= 11) && (state->jitter <= 14)) {
                    i++;
                }
            } else if (state->rf_mod == 1) {
                if ((state->jitter >= 0) && (state->jitter < state->symbolCenter)) {
                    i++;          // fall back
                } else if ((state->jitter > state->symbolCenter) && (state->jitter < 10)) {
                    i--;          // catch up
                }
            } else if (state->rf_mod == 2) {
                if ((state->jitter >= state->symbolCenter - 1) && (state->jitter <= state->symbolCenter)) {
                    i--;
                } else if ((state->jitter >= state->symbolCenter + 1) && (state->jitter <= state->symbolCenter + 2)) {
                    i++;
                }
            } else if (state->rf_mod == 0) {
                if ((state->jitter > 0) && (state->jitter <= state->symbolCenter)) {
                    i--;          // catch up
                } else if ((state->jitter > state->symbolCenter) && (state->jitter < state->samplesPerSymbol)) {
                    i++;          // fall back
                }
            }
            state->jitter = -1;
        }


        result = sf_read_short(opts->audio_in_file, &sample, 1);
        if (result == 0) {
            sf_close(opts->audio_in_file);
//            fprintf(stderr, "\nEnd of .wav file.\n");
            cleanupAndExit(opts, state);
//            int ii = fclose(pFile);
//            fprintf (stderr, "exit11111 %d ", ii);

        }


        if (opts->use_cosine_filter) {
            if ((state->lastsynctype >= 10 && state->lastsynctype <= 13) || state->lastsynctype == 32 ||
                state->lastsynctype == 33 || state->lastsynctype == 34) {
                sample = dmr_filter(sample);
                // phase 2 C4FM disc tap input
            }
        }

        if (sample > state->center) {
            if (state->lastsample < state->center) {
                state->numflips += 1;
            }
            if (sample > (state->maxref * 1.25)) {
                if (state->lastsample < (state->maxref * 1.25)) {
                    state->numflips += 1;
                }
                if ((state->jitter < 0) && (state->rf_mod == 1)) {               // first spike out of place
                    state->jitter = i;
                }
                if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1)) {
                    fprintf(stderr, "O");
                }
            } else {
                if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1)) {
                    fprintf(stderr, "+");
                }
                if ((state->jitter < 0) && (state->lastsample < state->center) &&
                    (state->rf_mod != 1)) {               // first transition edge
                    state->jitter = i;
                }
            }
        } else {                       // sample < 0
            if (state->lastsample > state->center) {
                state->numflips += 1;
            }
            if (sample < (state->minref * 1.25)) {
                if (state->lastsample > (state->minref * 1.25)) {
                    state->numflips += 1;
                }
                if ((state->jitter < 0) && (state->rf_mod == 1)) {               // first spike out of place
                    state->jitter = i;
                }
                if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1)) {
                    fprintf(stderr, "X");
                }
            } else {
                if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1)) {
                    fprintf(stderr, "-");
                }
                if ((state->jitter < 0) && (state->lastsample > state->center) &&
                    (state->rf_mod != 1)) {               // first transition edge
                    state->jitter = i;
                }
            }
        }

        if (state->samplesPerSymbol == 20) //nxdn 4800 baud 2400 symbol rate
        {
            if ((i >= 9) && (i <= 11)) {
                sum += sample;
                count++;
            }
        }
        if (state->samplesPerSymbol == 5) //provoice or gfsk
        {
            if (i == 2) {
                sum += sample;
                count++;
            }
        } else {
            if (state->rf_mod == 0) {
                // 0: C4FM modulation

                if ((i >= state->symbolCenter - 1) && (i <= state->symbolCenter + 2)) {
                    sum += sample;
                    count++;
                }

            } else {
                // 1: QPSK modulation
                // 2: GFSK modulation
                // Note: this has been changed to use an additional symbol to the left
                // On the p25_raw_unencrypted.flac it is evident that the timing
                // comes one sample too late.
                // This change makes a significant improvement in the BER, at least for
                // this file.
                //if ((i == state->symbolCenter) || (i == state->symbolCenter + 1))
                if ((i == state->symbolCenter - 1) || (i == state->symbolCenter + 1)) {
                    sum += sample;
                    count++;
                }

            }
        }
        state->lastsample = sample;

    }

    symbol = (sum / count);

    if ((opts->symboltiming == 1) && (have_sync == 0) && (state->lastsynctype != -1)) {
        if (state->jitter >= 0) {
            fprintf(stderr, " %i\n", state->jitter);
        } else {
            fprintf(stderr, "\n");
        }
    }

    //read op25/fme symbol bin files
    state->symbolcnt++;
    return (symbol);
}
