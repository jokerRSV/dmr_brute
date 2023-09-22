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

#if !defined(NULL)
#define NULL 0
#endif


void
processFrame(dsd_opts *opts, dsd_state *state) {

    char duid[3];
    char nac[13];

    nac[12] = 0;
    duid[2] = 0;

    if (state->rf_mod == 1) {
        state->maxref = (int) (state->max * 0.80F);
        state->minref = (int) (state->min * 0.80F);
    } else {
        state->maxref = state->max;
        state->minref = state->min;
    }

    //NXDN FSW
    if ((state->synctype == 28) || (state->synctype == 29)) {
        return;
        //D-Star
    } else if ((state->synctype == 6) || (state->synctype == 7)) {
        return;
        //D-Star-HD
    } else if ((state->synctype == 18) || (state->synctype == 19)) {
        return;
    }
        //Start DMR Types
    else if ((state->synctype >= 10) && (state->synctype <= 13) || (state->synctype == 32) || (state->synctype == 33) ||
             (state->synctype == 34)) //32-34 DMR MS and RC
    {

        //print manufacturer strings to branding, disabled 0x10 moto other systems can use that fid set
        //0x06 is trident, but when searching, apparently, they developed con+, but was bought by moto?
        if (state->dmr_mfid == 0x10)
            sprintf(state->dmr_branding, "%s", "Motorola");
        else if (state->dmr_mfid == 0x68)
            sprintf(state->dmr_branding, "%s", "  Hytera");
        else if (state->dmr_mfid == 0x58)
            sprintf(state->dmr_branding, "%s", "    Tait");

            //disabling these due to random data decodes setting an odd mfid, could be legit, but only for that one packet?
            //or, its just a decode error somewhere
        else if (state->dmr_mfid == 0x20) sprintf(state->dmr_branding, "%s", "JVC Kenwood");
        else if (state->dmr_mfid == 0x04) sprintf(state->dmr_branding, "%s", "Flyde Micro");
        else if (state->dmr_mfid == 0x05) sprintf(state->dmr_branding, "%s", "PROD-EL SPA");
        else if (state->dmr_mfid == 0x06) sprintf(state->dmr_branding, "%s", "Motorola"); //trident/moto con+
        else if (state->dmr_mfid == 0x07) sprintf(state->dmr_branding, "%s", "RADIODATA");
        else if (state->dmr_mfid == 0x08) sprintf(state->dmr_branding, "%s", "Hytera");
        else if (state->dmr_mfid == 0x09) sprintf(state->dmr_branding, "%s", "ASELSAN");
        else if (state->dmr_mfid == 0x0A) sprintf(state->dmr_branding, "%s", "Kirisun");
        else if (state->dmr_mfid == 0x0B) sprintf(state->dmr_branding, "%s", "DMR Association");
        else if (state->dmr_mfid == 0x13) sprintf(state->dmr_branding, "%s", "EMC S.P.A.");
        else if (state->dmr_mfid == 0x1C) sprintf(state->dmr_branding, "%s", "EMC S.P.A.");
        else if (state->dmr_mfid == 0x33) sprintf(state->dmr_branding, "%s", "Radio Activity");
        else if (state->dmr_mfid == 0x3C) sprintf(state->dmr_branding, "%s", "Radio Activity");
        else if (state->dmr_mfid == 0x77) sprintf(state->dmr_branding, "%s", "Vertex Standard");

        //disable so radio id doesn't blink in and out during ncurses and aggressive_framesync
        state->nac = 0;

        //DMR Voice Modes
        if ((state->synctype == 11) || (state->synctype == 12) || (state->synctype == 32)) {

            sprintf(state->fsubtype, " VOICE        ");
            if (opts->dmr_stereo == 0 && state->synctype < 32) {
//                sprintf(state->slot1light, " slot1 ");
//                sprintf(state->slot2light, " slot2 ");
                //we can safely open MBE on any MS or mono handling
                if (opts->p25_trunk == 0) dmrMSBootstrap(opts, state);
            }
            if (opts->dmr_mono == 1 && state->synctype == 32) {
                //we can safely open MBE on any MS or mono handling
                if (opts->p25_trunk == 0) dmrMSBootstrap(opts, state);
            }
            //opts->dmr_stereo == 1
            if (opts->dmr_stereo == 1) {
                state->dmr_stereo = 1; //set the state to 1 when handling pure voice frames
                if (state->synctype <= 31)
                    dmrBSBootstrap(opts, state); //bootstrap into BS Bootstrap
            }
            //MS Data and RC data
        } else if ((state->synctype == 33) || (state->synctype == 34)) {
            if (opts->p25_trunk == 0) dmrMSData(opts, state);
        } else {
            if (opts->dmr_stereo == 0) {
                state->err_str[0] = 0;
//                sprintf(state->slot1light, " slot1 ");
//                sprintf(state->slot2light, " slot2 ");
                dmr_data_sync(opts, state);
            }
            //switch dmr_stereo to 0 when handling BS data frame syncs with processDMRdata
            if (opts->dmr_stereo == 1) {
                state->dmr_stereo = 0; //set the state to zero for handling pure data frames
//                sprintf(state->slot1light, " slot1 ");
//                sprintf(state->slot2light, " slot2 ");
                dmr_data_sync(opts, state);
            }
        }
        return;
    }
        //X2-TDMA
    else if ((state->synctype >= 2) && (state->synctype <= 5)) {
        return;
    } else if ((state->synctype == 14) || (state->synctype == 15)) {
        return;
    }
        //edacs
    else if ((state->synctype == 37) || (state->synctype == 38)) {
        return;
    }
        //ysf
    else if ((state->synctype == 30) || (state->synctype == 31)) {
        return;
    }
        //P25 P2
    else if ((state->synctype == 35) || (state->synctype == 36)) {
        return;
    }
        //dPMR
    else if ((state->synctype == 20) || (state->synctype == 24)) {
    } else if ((state->synctype == 21) || (state->synctype == 25)) {
        return;

    } else if ((state->synctype == 22) || (state->synctype == 26)) {
    } else if ((state->synctype == 23) || (state->synctype == 27)) {
    }
        //dPMR
        //P25
    else {
    }

}
