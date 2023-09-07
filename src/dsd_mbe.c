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

//NOTE: I attempted to fix the atrocious tab/space alignnment issues that happened in this file,
//it looks fine in VSCodium, but no telling how it will translate when pushed to Github or another editor

void keyring(dsd_opts *opts, dsd_state *state) {

    if (state->currentslot == 0)
        state->R = state->rkey_array[state->payload_keyid];

    if (state->currentslot == 1)
        state->RR = state->rkey_array[state->payload_keyidR];
}

void RC4(int drop, uint8_t keylength, uint8_t messagelength, uint8_t key[], uint8_t cipher[], uint8_t plain[]) {
    int i, j, count;
    uint8_t t, b;

    //init Sbox
    uint8_t S[256];
    for (int i = 0; i < 256; i++) S[i] = i;

    //Key Scheduling
    j = 0;
    for (i = 0; i < 256; i++) {
        j = (j + S[i] + key[i % keylength]) % 256;
        t = S[i];
        S[i] = S[j];
        S[j] = t;
    }

    //Drop Bytes and Cipher Byte XOR
    i = j = 0;
    for (count = 0; count < (messagelength + drop); count++) {
        i = (i + 1) % 256;
        j = (j + S[i]) % 256;
        t = S[i];
        S[i] = S[j];
        S[j] = t;
        b = S[(S[i] + S[j]) % 256];

        //return mbe payload byte here
        if (count >= drop)
            plain[count - drop] = b ^ cipher[count - drop];

    }

}

void
processMbeFrame(dsd_opts *opts, dsd_state *state, char imbe_fr[8][23], char ambe_fr[4][24], char imbe7100_fr[7][24]) {

    int i;
    char imbe_d[88];
    char ambe_d[49];
    unsigned long long int k;
    int x;

    //these conditions should ensure no clashing with the BP/HBP/Scrambler key loading machanisms already coded in
//    if (state->currentslot == 0 && state->payload_algid != 0 && state->payload_algid != 0x80 &&
//        state->payload_keyid != 0 && state->keyloader == 1)
//        keyring(opts, state);

//    if (state->currentslot == 1 && state->payload_algidR != 0 && state->payload_algidR != 0x80 &&
//        state->payload_keyidR != 0 && state->keyloader == 1)
//        keyring(opts, state);

    //24-bit TG to 16-bit hash
    uint8_t hash_bits[24];
    memset(hash_bits, 0, sizeof(hash_bits));

    for (i = 0; i < 88; i++) {
        imbe_d[i] = 0;
    }

    for (i = 0; i < 49; i++) {
        ambe_d[i] = 0;
    }

    //set playback mode for this frame
    char mode[8];

    //if we are using allow/whitelist mode, then write 'B' to mode for block
    //comparison below will look for an 'A' to write to mode if it is allowed
//    if (opts->trunk_use_allow_list == 1)
//        sprintf(mode, "%s", "B");

    int groupNumber = 0;

    if (state->currentslot == 0)
        groupNumber = state->lasttg;
//    else
//        groupNumber = state->lasttgR;

//    for (i = 0; i < state->group_tally; i++) {
//        if (state->group_array[i].groupNumber == groupNumber) {
//            strcpy(mode, state->group_array[i].groupMode);
//        }
//    }

    //set flag to not play audio this time, but won't prevent writing to wav files
//    if (strcmp(mode, "B") == 0)
//        opts->audio_out = 0;

    //end set playback mode for this frame

    if ((state->synctype == 0) || (state->synctype == 1)) {
        fprintf(stderr, "\n!!!!!!!!!!!!!!!! sync type not supported:  %d \n", state->synctype);
    } else if ((state->synctype == 14) || (state->synctype == 15)) {
        fprintf(stderr, "\n!!!!!!!!!!!!!!!! sync type not supported:  %d \n", state->synctype);
    } else if ((state->synctype == 6) || (state->synctype == 7)) {
        fprintf(stderr, "\n!!!!!!!!!!!!!!!! sync type not supported:  %d \n", state->synctype);
    } else if ((state->synctype == 28) || (state->synctype == 29)) {
        fprintf(stderr, "\n!!!!!!!!!!!!!!!! sync type not supported:  %d \n", state->synctype);
    } else {
        if (state->currentslot == 0) {

            state->errs = mbe_eccAmbe3600x2450C0(ambe_fr);
            mbe_demodulateAmbe3600x2450Data(ambe_fr);
            state->errs2 = mbe_eccAmbe3600x2450Data(ambe_fr, ambe_d);

//            fprintf(stderr, "\nerr1:  %d", state->errs);
//            fprintf(stderr, "\nerr2:  %d \n", state->errs2);


            if ((state->K1 > 0 && state->dmr_so & 0x40 && state->payload_keyid == 0 && state->dmr_fid == 0x68) ||
                (state->K1 > 0 && state->M == 1)) {

                int pos = 0;

                unsigned long long int k1 = state->K1;

                int T_Key[256] = {0};
                int pN[882] = {0};

                int len = 39;
                k1 = k1 << 24;

                for (i = 0; i < 64; i++) {
                    T_Key[i] = (((k1 << i) & 0x8000000000000000) >> 63);
                }

                for (i = 0; i < 882; i++) {
                    pN[i] = T_Key[pos];
                    pos++;
                    if (pos > len) {
                        pos = 0;
                    }
                }

                //sanity check
                if (state->DMRvcL > 17) {
                    state->DMRvcL = 17; //18
                }

                pos = state->DMRvcL * 49;
                for (i = 0; i < 49; i++) {
                    ambe_d[i] ^= pN[pos];
                    pos++;
                }
                state->DMRvcL++;
            }

            mbe_processAmbe2450Dataf(state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_d,
                                     state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

        }
        //stereo slots and slot 1 (right slot)
    }

    //quick enc check to determine whether or not to play enc traffic
    int enc_bit = 0;
    //end enc check

    //all mono traffic routed through 'left'
    if ((opts->dmr_mono == 1 || opts->dmr_stereo == 1) && state->currentslot == 0) {
        enc_bit = (state->dmr_so >> 6) & 0x1;
        if (enc_bit == 1) {
            state->dmr_encL = 1;
            //checkdown for P25 1 and 2
        }

        //check for available R key
        if (state->R != 0)
            state->dmr_encL = 0;

#ifdef AERO_BUILD //FUN FACT: OSS stutters only on Cygwin, using padsp in linux, it actually opens two virtual /dev/dsp audio streams for output
        //OSS Specific Voice Preemption if dual voices on TDMA and one slot has preference over the other
        if (opts->slot_preference == 1 && opts->audio_out_type == 5 && opts->audio_out == 1 && (state->dmrburstR == 16 || state->dmrburstR == 21) )
        {
          opts->audio_out = 0;
          preempt = 1;
          if (opts->payload == 0)
            fprintf (stderr, " *MUTED*");
        }
#endif

        if (state->dmr_encL == 0 || opts->dmr_mute_encL == 0) {
            state->debug_audio_errors += state->errs2;
            if (opts->audio_out == 1) {
                processAudio(opts, state);
            }
            if (opts->audio_out == 1) {
                playSynthesizedVoice(opts, state);
            }
        }
    }


    //reset audio out flag for next repitition
    if (strcmp(mode, "B") == 0) opts->audio_out = 1;
    //restore flag for null output type
    if (opts->audio_out_type == 9) opts->audio_out = 0;
}
