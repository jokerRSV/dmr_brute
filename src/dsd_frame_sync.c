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
#include <locale.h>

//borrowed from LEH/DSDcc for 'improved NXDN sync detection'
int strncmperr(const char *s1, const char *s2, size_t size, int MaxErr) {

    int Compare = -1;
    size_t i = 0;
    int err = 0;
    int BreakBeforeEnd = 0;

    if (s1 && s2) {
        for (i = 0; i < size; i++) {
            if (((s1[i] & 0xFF) != '\0') && ((s2[i] & 0xFF) != '\0')) {
                if ((s1[i] & 0xFF) != (s2[i] & 0xFF)) err++;
            } else {
                BreakBeforeEnd = 1;
                break;
            }
        }

        if ((err <= MaxErr) && (BreakBeforeEnd == 0)) {
            Compare = 0;
        }
    } /* End if(s1 && s2) */

    return Compare;
} /* End strncmperr() */

char *getTime(void) //get pretty hh:mm:ss timestamp
{
    time_t t = time(NULL);

    char *curr;
    char *stamp = asctime(localtime(&t));

    curr = strtok(stamp, " ");
    curr = strtok(NULL, " ");
    curr = strtok(NULL, " ");
    curr = strtok(NULL, " ");

    return curr;
}

char *getDate(void) {
    char datename[32];
    char *curr2;
    struct tm *to;
    time_t t;
    t = time(NULL);
    to = localtime(&t);
    strftime(datename, sizeof(datename), "%Y-%m-%d", to);
    curr2 = strtok(datename, " ");
    return curr2;
}

void
printFrameSync(dsd_opts *opts, dsd_state *state, char *frametype, int offset, char *modulation) {

    if (opts->verbose > 0) {
//        fprintf(stderr, "%s ", getTime());
//        fprintf(stderr, "Sync: %s ", frametype);
    }
    if (opts->verbose > 2) {
        //fprintf (stderr,"o: %4i ", offset);
    }
    if (opts->verbose > 1) {
        //fprintf (stderr,"mod: %s ", modulation);
    }
    if (opts->verbose > 2) {
        //fprintf (stderr,"g: %f ", state->aout_gain);
    }

}

int
getFrameSync(dsd_opts *opts, dsd_state *state) {
    /* detects frame sync and returns frame type
     *  0 = +P25p1
     *  1 = -P25p1
     *  2 = +X2-TDMA (non inverted signal data frame)
     *  3 = -X2-TDMA (inverted signal voice frame)
     *  4 = +X2-TDMA (non inverted signal voice frame)
     *  5 = -X2-TDMA (inverted signal data frame)
     *  6 = +D-STAR
     *  7 = -D-STAR
     *  8 = +NXDN (non inverted voice frame)
     *  9 = -NXDN (inverted voice frame)
     * 10 = +DMR (non inverted signal data frame)
     * 11 = -DMR (inverted signal voice frame)
     * 12 = +DMR (non inverted signal voice frame)
     * 13 = -DMR (inverted signal data frame)
     * 14 = +ProVoice
     * 15 = -ProVoice
     * 16 = +NXDN (non inverted data frame)
     * 17 = -NXDN (inverted data frame)
     * 18 = +D-STAR_HD
     * 19 = -D-STAR_HD
     * 20 = +dPMR Frame Sync 1
     * 21 = +dPMR Frame Sync 2
     * 22 = +dPMR Frame Sync 3
     * 23 = +dPMR Frame Sync 4
     * 24 = -dPMR Frame Sync 1
     * 25 = -dPMR Frame Sync 2
     * 26 = -dPMR Frame Sync 3
     * 27 = -dPMR Frame Sync 4
     * 28 = +NXDN (sync only)
     * 29 = -NXDN (sync only)
     * 30 = +YSF
     * 31 = -YSF
     * 32 = DMR MS Voice
     * 33 = DMR MS Data
     * 34 = DMR RC Data
     * 35 = +P25 P2
     * 36 = -P25 P2
     * 37 = +EDACS
     * 38 = -EDACS
     */


    //start control channel hunting if using trunking, time needs updating on each successful sync
    //will need to assign frequencies to a CC array for P25 since that isn't imported from CSV
    if (state->dmr_rest_channel == -1 && opts->p25_is_tuned == 0 && opts->p25_trunk == 1 &&
        ((time(NULL) - state->last_cc_sync_time) > (opts->trunk_hangtime + 0))) //was 3, go to hangtime value
    {

        //test to switch back to 10/4 P1 QPSK for P25 FDMA CC
        if (opts->mod_qpsk == 1 && state->p25_cc_is_tdma == 0) {
            state->samplesPerSymbol = 10;
            state->symbolCenter = 4;
        }

        //start going through the lcn/frequencies CC/signal hunting
        fprintf(stderr, "Control Channel Signal Lost. Searching for Control Channel.\n");
        //make sure our current roll value doesn't exceed value of frequencies imported
        if (state->lcn_freq_roll >=
            state->lcn_freq_count) //fixed this to skip the extra wait out at the end of the list
        {
            state->lcn_freq_roll = 0; //reset to zero
        }
        //roll an extra value up if the current is the same as what's already loaded -- faster hunting on Cap+, etc
        if (state->lcn_freq_roll != 0) {
            if (state->trunk_lcn_freq[state->lcn_freq_roll - 1] == state->trunk_lcn_freq[state->lcn_freq_roll]) {
                state->lcn_freq_roll++;
                //check roll again if greater than expected, then go back to zero
                if (state->lcn_freq_roll >= state->lcn_freq_count) {
                    state->lcn_freq_roll = 0; //reset to zero
                }
            }
        }
        state->lcn_freq_roll++;
        state->last_cc_sync_time = time(NULL); //set again to give another x seconds
    }

    int i, j, t, o, dibit, sync, symbol, synctest_pos, lastt;
    char synctest[25];
    char synctest12[13]; //dPMR
    char synctest10[11]; //NXDN FSW only
    char synctest19[20]; //NXDN Preamble + FSW
    char synctest18[19];
    char synctest32[33];
    char synctest20[21]; //YSF
    char synctest21[22]; //P25 S-OEMI (SACCH)
    char synctest48[49]; //EDACS
    char modulation[8];
    char *synctest_p;
    int symboltest_pos; //symbol test position, match to synctest_pos
    int symboltest_p[10240]; //make this an array instead
    char synctest_buf[10240]; //what actually is assigned to this, can't find its use anywhere?
    int lmin, lmax, lidx;

    //assign t_max value based on decoding type expected (all non-auto decodes first)
    int t_max; //maximum values allowed for t will depend on decoding type - NXDN will be 10, others will be more
    if (opts->frame_nxdn48 == 1 || opts->frame_nxdn96 == 1) {
        t_max = 10;
    }
        //else if dPMR
    else if (opts->frame_dpmr == 1) {
        t_max = 12; //based on Frame_Sync_2 pattern
    }
        //if Phase 2 (or YSF in future), then only 19
    else if (state->lastsynctype == 35 || state->lastsynctype == 36) //P2
    {
        t_max = 19; //Phase 2 S-ISCH is only 19
    } else t_max = 24; //24 for everything else

    int lbuf[48], lbuf2[48]; //if we use t_max in these arrays, and t >=  t_max in condition below, then it can overflow those checks in there if t exceeds t_max
    int lsum;
    char spectrum[64];
    //init the lbuf
    memset(lbuf, 0, sizeof(lbuf));
    memset(lbuf2, 0, sizeof(lbuf2));

    // detect frame sync
    t = 0;
    synctest10[10] = 0;
    synctest[24] = 0;
    synctest12[12] = 0;
    synctest18[18] = 0;
    synctest48[48] = 0;
    synctest32[32] = 0;
    synctest20[20] = 0;
    synctest_pos = 0;
    synctest_p = synctest_buf + 10;
    sync = 0;
    lmin = 0;
    lmax = 0;
    lidx = 0;
    lastt = 0;
    state->numflips = 0;

    if ((opts->symboltiming == 1) && (state->carrier == 1)) {
        //fprintf (stderr,"\nSymbol Timing:\n");
        //printw("\nSymbol Timing:\n");
    }
    while (sync == 0) {

        t++;

        symbol = getSymbol(opts, state, 0);

        lbuf[lidx] = symbol;
        state->sbuf[state->sidx] = symbol;
        if (lidx == (t_max - 1)) //23 //9 for NXDN
        {
            lidx = 0;
        } else {
            lidx++;
        }
        if (state->sidx == (opts->ssize - 1)) {
            state->sidx = 0;
        } else {
            state->sidx++;
        }

        if (lastt == t_max) //issue for QPSK on shorter sync pattern? //23
        {
            lastt = 0;
            if (state->numflips > opts->mod_threshold) {
                if (opts->mod_qpsk == 1) {
                    state->rf_mod = 1;
                }
            } else if (state->numflips > 18) {
                if (opts->mod_gfsk == 1) {
                    state->rf_mod = 2;
                }
            } else {
                if (opts->mod_c4fm == 1) {
                    state->rf_mod = 0;
                }
            }
            state->numflips = 0;
        } else {
            lastt++;
        }

        if (state->dibit_buf_p > state->dibit_buf + 900000) {
            state->dibit_buf_p = state->dibit_buf + 200;
        }

        //determine dibit state
        if (symbol > 0) {
            *state->dibit_buf_p = 1;
            state->dibit_buf_p++;
            dibit = 49;               // '1'
        } else {
            *state->dibit_buf_p = 3;
            state->dibit_buf_p++;
            dibit = 51;               // '3'
        }

        if (opts->symbol_out == 1 && dibit != 0) {
            int csymbol = 0;
            if (dibit == 49) {
                csymbol = 1; //1
            }
            if (dibit == 51) {
                csymbol = 3; //3
            }
            //fprintf (stderr, "%d", dibit);
            fputc(csymbol, opts->symbol_out_f);
        }

        //digitize test for storing dibits in buffer correctly for dmr recovery

        if (state->dmr_payload_p > state->dmr_payload_buf + 900000) {
            state->dmr_payload_p = state->dmr_payload_buf + 200;
        }

        if (symbol > state->center) {
            if (symbol > state->umid) {
                *state->dmr_payload_p = 1;               // +3
            } else {
                *state->dmr_payload_p = 0;               // +1
            }
        } else {
            if (symbol < state->lmid) {
                *state->dmr_payload_p = 3;               // -3
            } else {
                *state->dmr_payload_p = 2;               // -1
            }
        }

        state->dmr_payload_p++;
        // end digitize and dmr buffer testing

        *synctest_p = dibit;
        //works excelent now with short sync patterns, and no issues with large ones!
        if (t >= t_max) {
            for (i = 0; i < t_max; i++) //24
            {
                lbuf2[i] = lbuf[i];
            }
            qsort(lbuf2, t_max, sizeof(int), comp);
            lmin = (lbuf2[1] + lbuf2[2] + lbuf2[3]) / 3;
            lmax = (lbuf2[t_max - 3] + lbuf2[t_max - 2] + lbuf2[t_max - 1]) / 3;

            if (state->rf_mod == 1) {
                state->minbuf[state->midx] = lmin;
                state->maxbuf[state->midx] = lmax;
                if (state->midx == (opts->msize - 1)) //-1
                {
                    state->midx = 0;
                } else {
                    state->midx++;
                }
                lsum = 0;
                for (i = 0; i < opts->msize; i++) {
                    lsum += state->minbuf[i];
                }
                state->min = lsum / opts->msize;
                lsum = 0;
                for (i = 0; i < opts->msize; i++) {
                    lsum += state->maxbuf[i];
                }
                state->max = lsum / opts->msize;
                state->center = ((state->max) + (state->min)) / 2;
                state->maxref = (int) ((state->max) * 0.80F);
                state->minref = (int) ((state->min) * 0.80F);
            } else {
                state->maxref = state->max;
                state->minref = state->min;
            }

            strncpy(synctest, (synctest_p - 23), 24);
            //New DMR Sync
            if (opts->frame_dmr == 1) {

                if (strcmp(synctest, DMR_MS_DATA_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    //state->directmode = 0;
                    //fprintf (stderr, "DMR MS Data");
                    if (opts->inverted_dmr == 0) //opts->inverted_dmr
                    {
                        // data frame
                        sprintf(state->ftype, "DMR MS");
                        if (opts->errorbars == 1) {
                            //printFrameSync (opts, state, "+DMR MS Data", synctest_pos + 1, modulation);
                        }
                        if (state->lastsynctype != 33) //33
                        {
                            //state->firstframe = 1;
                        }
                        state->lastsynctype = 33; //33
                        return (33); //33
                    } else //inverted MS voice frame
                    {
                        sprintf(state->ftype, "DMR MS");
                        state->lastsynctype = 32;
                        return (32);
                    }
                }
                //not sure if this should be here, RC data should only be present in vc6?
                if (strcmp(synctest, DMR_RC_DATA_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    //state->directmode = 0;
                    //fprintf (stderr, "DMR RC DATA\n");
                    state->dmr_ms_rc ==
                    1; //set flag for RC data, then process accordingly and reset back to 0 afterwards
                    if (0 == 0) //opts->inverted_dmr
                    {
                        // voice frame
                        sprintf(state->ftype, "DMR RC");
                        if (opts->errorbars == 1) {
                            //printFrameSync (opts, state, "+DMR RC Data", synctest_pos + 1, modulation);
                        }
                        if (state->lastsynctype != 34) {
                            //state->firstframe = 1;
                        }
                        state->lastsynctype = 34;
                        return (34);
                    }
                }

                if (strcmp(synctest, DMR_MS_VOICE_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    //state->directmode = 0;
                    //fprintf (stderr, "DMR MS VOICE\n");
                    if (opts->inverted_dmr == 0) //opts->inverted_dmr
                    {
                        // voice frame
                        sprintf(state->ftype, "DMR MS");
                        if (opts->errorbars == 1) {
                            //printFrameSync (opts, state, "+DMR MS Voice", synctest_pos + 1, modulation);
                        }
                        if (state->lastsynctype != 32) {
                            //state->firstframe = 1;
                        }
                        state->lastsynctype = 32;
                        return (32);
                    } else //inverted MS data frame
                    {
                        sprintf(state->ftype, "DMR MS");
                        state->lastsynctype = 33;
                        return (33);
                    }

                }

                //if ((strcmp (synctest, DMR_MS_DATA_SYNC) == 0) || (strcmp (synctest, DMR_BS_DATA_SYNC) == 0))
                if (strcmp(synctest, DMR_BS_DATA_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + (lmax)) / 2;
                    state->min = ((state->min) + (lmin)) / 2;
                    state->directmode = 0;
                    if (opts->inverted_dmr == 0) {
                        // data frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1) {
                            printFrameSync(opts, state, "+DMR ", synctest_pos + 1, modulation);
                        }
                        state->lastsynctype = 10;
                        state->last_cc_sync_time = time(NULL);
                        return (10);
                    } else {
                        // inverted voice frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1 && opts->dmr_stereo == 0) {
                            //printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                        }
                        if (state->lastsynctype != 11) {
                            state->firstframe = 1;
                        }
                        state->lastsynctype = 11;
                        state->last_cc_sync_time = time(NULL);
                        return (11); //11
                    }
                }
                if (strcmp(synctest, DMR_DIRECT_MODE_TS1_DATA_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + (lmax)) / 2;
                    state->min = ((state->min) + (lmin)) / 2;
                    //state->currentslot = 0;
                    state->directmode = 1;  //Direct mode
                    if (opts->inverted_dmr == 0) {
                        // data frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1) {
                            //printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                        }
                        state->lastsynctype = 33;
                        state->last_cc_sync_time = time(NULL);
                        return (33);
                    } else {
                        // inverted voice frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1) {
                            //printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                        }
                        if (state->lastsynctype != 11) {
                            state->firstframe = 1;
                        }
                        state->lastsynctype = 32;
                        state->last_cc_sync_time = time(NULL);
                        return (32);
                    }
                } /* End if(strcmp (synctest, DMR_DIRECT_MODE_TS1_DATA_SYNC) == 0) */
                if (strcmp(synctest, DMR_DIRECT_MODE_TS2_DATA_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + (lmax)) / 2;
                    state->min = ((state->min) + (lmin)) / 2;
                    //state->currentslot = 1;
                    state->directmode = 1;  //Direct mode
                    if (opts->inverted_dmr == 0) {
                        // data frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1) {
                            //printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                        }
                        state->lastsynctype = 33;
                        state->last_cc_sync_time = time(NULL);
                        return (33);
                    } else {
                        // inverted voice frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1 && opts->dmr_stereo == 0) {
                            //printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                        }
                        if (state->lastsynctype != 11) {
                            state->firstframe = 1;
                        }
                        state->lastsynctype = 32;
                        state->last_cc_sync_time = time(NULL);
                        return (32);
                    }
                } /* End if(strcmp (synctest, DMR_DIRECT_MODE_TS2_DATA_SYNC) == 0) */
                //if((strcmp (synctest, DMR_MS_VOICE_SYNC) == 0) || (strcmp (synctest, DMR_BS_VOICE_SYNC) == 0))
                if (strcmp(synctest, DMR_BS_VOICE_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    state->directmode = 0;
                    if (opts->inverted_dmr == 0) {
                        // voice frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1 && opts->dmr_stereo == 0) {
                            //printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                        }
                        if (state->lastsynctype != 12) {
                            state->firstframe = 1;
                        }
                        state->lastsynctype = 12;
                        state->last_cc_sync_time = time(NULL);
                        return (12);
                    } else {
                        // inverted data frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1) //&& opts->dmr_stereo == 0
                        {
                            printFrameSync(opts, state, "-DMR ", synctest_pos + 1, modulation);
                        }
                        state->lastsynctype = 13;
                        state->last_cc_sync_time = time(NULL);
                        return (13);
                    }
                }
                if (strcmp(synctest, DMR_DIRECT_MODE_TS1_VOICE_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    //state->currentslot = 0;
                    state->directmode = 1;  //Direct mode
                    if (opts->inverted_dmr == 0) //&& opts->dmr_stereo == 1
                    {
                        // voice frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1 && opts->dmr_stereo == 0) {
                            //printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                        }
                        if (state->lastsynctype != 12) {
                            state->firstframe = 1;
                        }
                        state->lastsynctype = 32;
                        state->last_cc_sync_time = time(NULL);
                        return (32); //treat Direct Mode same as MS mode for now
                    } else {
                        // inverted data frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1 && opts->dmr_stereo == 0) {
                            //printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                        }
                        state->lastsynctype = 33;
                        return (33);
                    }
                } /* End if(strcmp (synctest, DMR_DIRECT_MODE_TS1_VOICE_SYNC) == 0) */
                if (strcmp(synctest, DMR_DIRECT_MODE_TS2_VOICE_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    // state->currentslot = 1;
                    state->directmode = 1;  //Direct mode
                    if (opts->inverted_dmr == 0) //&& opts->dmr_stereo == 1
                    {
                        // voice frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1 && opts->dmr_stereo == 0) {
                            //printFrameSync (opts, state, "+DMR ", synctest_pos + 1, modulation);
                        }
                        if (state->lastsynctype != 12) {
                            state->firstframe = 1;
                        }
                        state->lastsynctype = 32;
                        state->last_cc_sync_time = time(NULL);
                        return (32);
                    } else {
                        // inverted data frame
                        sprintf(state->ftype, "DMR ");
                        if (opts->errorbars == 1 && opts->dmr_stereo == 0) {
                            //printFrameSync (opts, state, "-DMR ", synctest_pos + 1, modulation);
                        }
                        state->lastsynctype = 33;
                        state->last_cc_sync_time = time(NULL);
                        return (33);
                    }
                } //End if(strcmp (synctest, DMR_DIRECT_MODE_TS2_VOICE_SYNC) == 0)
            } //End if (opts->frame_dmr == 1)

            //end DMR Sync

            //ProVoice and EDACS sync
            if (opts->frame_provoice == 1) {
                strncpy(synctest32, (synctest_p - 31), 32);
                strncpy(synctest48, (synctest_p - 47), 48);
                if ((strcmp(synctest32, PROVOICE_SYNC) == 0) || (strcmp(synctest32, PROVOICE_EA_SYNC) == 0)) {
                    state->last_cc_sync_time = time(NULL);
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    sprintf(state->ftype, "ProVoice ");
                    if (opts->errorbars == 1)
                        printFrameSync(opts, state, "+PV   ", synctest_pos + 1, modulation);
                    state->lastsynctype = 14;
                    return (14);
                } else if ((strcmp(synctest32, INV_PROVOICE_SYNC) == 0) ||
                           (strcmp(synctest32, INV_PROVOICE_EA_SYNC) == 0)) {
                    state->last_cc_sync_time = time(NULL);
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    sprintf(state->ftype, "ProVoice ");
                    printFrameSync(opts, state, "-PV   ", synctest_pos + 1, modulation);
                    state->lastsynctype = 15;
                    return (15);
                } else if (strcmp(synctest48, EDACS_SYNC) == 0) {
                    state->last_cc_sync_time = time(NULL);
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    printFrameSync(opts, state, "-EDACS", synctest_pos + 1, modulation);
                    state->lastsynctype = 38;
                    return (38);
                } else if (strcmp(synctest48, INV_EDACS_SYNC) == 0) {
                    state->last_cc_sync_time = time(NULL);
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    printFrameSync(opts, state, "+EDACS", synctest_pos + 1, modulation);
                    state->lastsynctype = 37;
                    return (37);
                }

            } else if (opts->frame_dstar == 1) {
                if (strcmp(synctest, DSTAR_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    sprintf(state->ftype, "D-STAR ");
                    if (opts->errorbars == 1) {
                        printFrameSync(opts, state, "+D-STAR ", synctest_pos + 1, modulation);
                    }
                    state->lastsynctype = 6;
                    return (6);
                }
                if (strcmp(synctest, INV_DSTAR_SYNC) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    sprintf(state->ftype, "D-STAR ");
                    if (opts->errorbars == 1) {
                        printFrameSync(opts, state, "-D-STAR ", synctest_pos + 1, modulation);
                    }
                    state->lastsynctype = 7;
                    return (7);
                }
                if (strcmp(synctest, DSTAR_HD) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    sprintf(state->ftype, "D-STAR_HD ");
                    if (opts->errorbars == 1) {
                        printFrameSync(opts, state, "+D-STAR_HD ", synctest_pos + 1, modulation);
                    }
                    state->lastsynctype = 18;
                    return (18);
                }
                if (strcmp(synctest, INV_DSTAR_HD) == 0) {
                    state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    sprintf(state->ftype, " D-STAR_HD");
                    if (opts->errorbars == 1) {
                        printFrameSync(opts, state, "-D-STAR_HD ", synctest_pos + 1, modulation);
                    }
                    state->lastsynctype = 19;
                    return (19);
                }

                //NXDN FSW sync and handling - using more exact frame sync values
            } else if ((opts->frame_nxdn96 == 1) || (opts->frame_nxdn48 == 1)) {
                strncpy(synctest10, (synctest_p - 9), 10); //FSW only
                strncpy(synctest19, (synctest_p - 18), 19); //Preamble + FSW

                if (
                        (strcmp(synctest10, "3131331131") ==
                         0) //this seems to be the most common 'correct' pattern on Type-C
                        || (strcmp(synctest10, "3331331131") == 0) //this one hits on new sync but gives a bad lich code
                        || (strcmp(synctest10, "3131331111") == 0)
                        || (strcmp(synctest10, "3331331111") == 0)
                        || (strcmp(synctest10, "3131311131") ==
                            0) //First few FSW on NXDN48 Type-C seems to hit this for some reason

                        ) {

                    // state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    // state->last_cc_sync_time = time(NULL);

                    if (state->lastsynctype == 28) {
                        state->last_cc_sync_time = time(NULL);
                        // if (opts->payload == 1)
                        //   fprintf (stderr, "\n +FSW   ");
                        // if (opts->payload == 1)
                        //   fprintf (stderr, " %s \n", synctest10);
                        return (28);
                    }
                    state->lastsynctype = 28;
                } else if (

                        (strcmp(synctest10, "1313113313") == 0)
                        || (strcmp(synctest10, "1113113313") == 0)
                        || (strcmp(synctest10, "1313113333") == 0)
                        || (strcmp(synctest10, "1113113333") == 0)
                        || (strcmp(synctest10, "1313133313") == 0) //

                        ) {

                    // state->carrier = 1;
                    state->offset = synctest_pos;
                    state->max = ((state->max) + lmax) / 2;
                    state->min = ((state->min) + lmin) / 2;
                    // state->last_cc_sync_time = time(NULL);

                    if (state->lastsynctype == 29) {
                        state->last_cc_sync_time = time(NULL);
                        // if (opts->payload == 1)
                        // fprintf (stderr, "\n -FSW   ");
                        // if (opts->payload == 1)
                        // fprintf (stderr, " %s \n", synctest10);
                        return (29);
                    }
                    state->lastsynctype = 29;
                }
            }

            SYNC_TEST_END:; //do nothing

        } // t >= 10

        if (synctest_pos < 10200) {
            synctest_pos++;
            synctest_p++;

        } else {
            // buffer reset
            synctest_pos = 0;
            synctest_p = synctest_buf;
            noCarrier(opts, state);

        }

        if (state->lastsynctype != 1) {

            if (synctest_pos >= 1800) {
                if ((opts->errorbars == 1) && (opts->verbose > 1) && (state->carrier == 1)) {
                    fprintf(stderr, "Sync: no sync\n");
                    // fprintf (stderr,"Press CTRL + C to close.\n");

                }
                noCarrier(opts, state);

                return (-1);
            }
        }

    }

    return (-1);

}

