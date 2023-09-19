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

static double entropy(const char *f, int length) {
    int counts[256] = {0};
    double entropy = 0;

    for (int i = 0; i < length; ++i) {
        counts[f[i] + 128]++;
    }
    for (int i = 0; i < 256; ++i) {
        if (counts[i] != 0) {
            double freq = counts[i] / (double) length;
            entropy -= freq * log2(freq) / log2(256);
        }
    }
    return entropy;
}

void print_time(char *buffer, struct timeval tv, int i, int j, int k) {
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    printf("%x %x %x @ ", i, j, k);
    strftime(buffer, 30, "%m-%d-%Y  %T.", localtime(&curtime));
    printf("%s%ld\n", buffer, tv.tv_usec);
    fflush(stdout);
}

void processMbeFrame(dsd_opts *opts, dsd_state *state, char ambe_fr[4][24]) {
    unsigned char ambe_d[49];

    for (int i = 0; i < 49; i++) {
        ambe_d[i] = 0;
    }
    int start = 10;
    int end = 200;
    if (state->currentslot == 0 && state->audio_count >= start && state->audio_count < end) {
        mbe_demodulateAmbe3600x2450Data(ambe_fr);
        mbe_eccAmbe3600x2450Data(ambe_fr, ambe_d);
        for (int j = 0; j < 49; ++j) {
            state->ambe_d[state->ambe_count][j] = ambe_d[j];
        }
        state->cur_mp_store[state->ambe_count] = state->cur_mp;
        state->prev_mp_store[state->ambe_count] = state->prev_mp;
        state->DMRvcL_p[state->ambe_count] = state->DMRvcL * 49;
        state->ambe_count++;
        state->DMRvcL++;
    }

    if (state->audio_count == end) {
        int errs = 0;
        int errs2 = 0;
        char err_str[64] = {0};
        state->DMRvcL = 0;

        char buffer[30];
        struct timeval tv;
        unsigned char ds = 0x1a;
        unsigned char ls = 0xe2;
        unsigned char js = 0xac;
        unsigned char ks = 0xa3;
        unsigned char ms = 0xa5;
        int di[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x34, ds};
        int li[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0xff, ls};
        int ji[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0xfe, js};
        int ki[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x92, ks};
        int mi[] = {0x00, 0x11, 0x22, 0x33, 0x55, 0x66, 0x77, ms};

//        for (int d = 0; d < 1; d++) {
//            for (int l = 0; l < 1; l++) {
//                for (int j = 0; j < 1; j++) {
//                    for (int k = 0; k < 1; k++) {
//                        for (int m = 0; m < 1; m++) {
//        for (int d = 4; d < 5; d++) {
//            for (int l = 4; l < 5; l++) {
//                for (int j = 4; j < 5; j++) {
//                    for (int k = 4; k < 5; k++) {
//                        for (int m = 8; m < 9; m++) {
//        for (int d = 0; d < 8; d++) {
//            for (int l = 0; l < 8; l++) {
//                for (int j = 0; j < 8; j++) {
//                    for (int k = 0; k < 8; k++) {
//                        for (int m = 0; m < 8; m++) {
        for (int d = 0x10; d < 0x1a + 1; d++) {
            for (int l = 0xe0; l < 0xe2 + 1; l++) {
                for (int j = 0xa0; j < 0xac + 1; j++) {
                    for (int k = 0xa0; k < 0xa3 + 1; k++) {
                        for (int m = 0xa0; m < 0xa5 + 1; m++) {
                            unsigned long long int k1;
                            k1 = 0;
//                            k1 |= (unsigned long long) di[d] << 32;
//                            k1 |= (unsigned long long) li[l] << 24;
//                            k1 |= (unsigned long long) ji[j] << 16;
//                            k1 |= (unsigned long long) ki[k] << 8;
//                            k1 |= (unsigned long long) mi[m];
                            k1 |= (unsigned long long) d << 32;
                            k1 |= (unsigned long long) l << 24;
                            k1 |= (unsigned long long) j << 16;
                            k1 |= (unsigned long long) k << 8;
                            k1 |= (unsigned long long) m;
                            fprintf(stderr, "\n");
                            fprintf(stderr, "--- %02x ", (unsigned int) (k1 >> 32));
                            fprintf(stderr, "%02x ", (unsigned int) (((k1 << 8) & 0xff00000000) >> 32));
                            fprintf(stderr, "%02x ", (unsigned int) (((k1 << 16) & 0xff00000000) >> 32));
                            fprintf(stderr, "%02x ", (unsigned int) (((k1 << 24) & 0xff00000000) >> 32));
                            fprintf(stderr, "%02x === ", (unsigned int) (((k1 << 32) & 0xff00000000) >> 32));

                            k1 = k1 << 24;

                            unsigned char T_Key[256] = {0};
                            for (int i = 0; i < 64; i++) {
                                T_Key[i] = (char) (((k1 << i) & 0x8000000000000000) >> 63);
                            }

                            int pos = 0;
                            unsigned char pN[882] = {0};
                            for (int i = 0; i < 882; i++) {
                                pN[i] = T_Key[pos++];
                                pos = pos % 40;
                            }

//                            snprintf(opts->wav_out_file, 4 + 22, "iii/sample_%x%x%x%x%x.txt", di[d], ji[j], ki[k],
//                                     li[l],
//                                     mi[m]);
                            snprintf(opts->wav_out_file, 4 + 22, "iii/sample_%x%x%x%x%x.txt", d, l, j, k, m);
                            if (access(opts->wav_out_file, F_OK) == 0) {
                                remove(opts->wav_out_file);
                            }
                            FILE *pFile = fopen(opts->wav_out_file, "w");

                            //play stored voice data
                            unsigned char ambe_d_copy[1000][49];
                            for (int i = 0; i < 1000; i++) {
                                for (int u = 0; u < 49; u++) {
                                    ambe_d_copy[i][u] = state->ambe_d[i][u];
                                }
                            }

                            for (int w = 0; w < state->ambe_count; ++w) {
                                pos = state->DMRvcL_p[w];
                                for (int i = 0; i < 49; i++) {
                                    ambe_d_copy[w][i] ^= pN[pos];
                                    pos++;
                                }
//                                mbe_processAmbe2450Dataf(state->audio_out_temp_buf,
//                                                         &errs, &errs2, err_str,
//                                                         ambe_d_copy[w],
//                                                         state->cur_mp_store[w],
//                                                         state->prev_mp_store[w],
//                                                         state->prev_mp_store[w],
//                                                         1);
//                                processAudio(opts, state);
//                                writeSynthesizedVoiceToBuff(state);
//                                writeSynthesizedVoice(opts, state);
//                                playSynthesizedVoice(opts, state);
                                unsigned char b0 = 0;
                                b0 |= ambe_d_copy[w][0] << 6;
                                b0 |= ambe_d_copy[w][1] << 5;
                                b0 |= ambe_d_copy[w][2] << 4;
                                b0 |= ambe_d_copy[w][3] << 3;
                                b0 |= ambe_d_copy[w][37] << 2;
                                b0 |= ambe_d_copy[w][38] << 1;
                                b0 |= ambe_d_copy[w][39];
                                state->b0_arr[w] = b0;
                            }

                            fwrite(state->b0_arr, sizeof state->b0_arr[0], state->ambe_count, pFile);
                            fclose(pFile);

                            state->voice_buff_counter = 0;
//                            if (di[d] == 0x1a && ji[j] == 0xe2 && ki[k] == 0xac && li[l] == 0xa3 && mi[m] == 0xa5) {
                            if (d == 0x1a && l == 0xe2 && j == 0xac && k == 0xa3 && m == 0xa5) {
                                goto exit;
                            }
                        }
                    }
                }
            }
        }
    }
    exit:
    state->audio_count++;
}
