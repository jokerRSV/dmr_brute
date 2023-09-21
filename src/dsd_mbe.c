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

static double calc_entropy(const unsigned char *f, int length) {
    int counts[256] = {0};
    double entropy = 0;

    for (int i = 0; i < length; ++i) {
        counts[f[i]]++;
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
    int start = 100;
    int num = 150;
    if (state->currentslot == 0 && state->audio_count >= start && state->audio_count < num + start) {
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

    if (state->audio_count == num + start) {
        state->DMRvcL = 0;

        int abort = 0;
        double entropy_acc = 1;
        unsigned long long int key;
//        for (int d = 0x10; d < 0x1a + 10; d++) {
//            char buffer[30];
//            struct timeval tv;
//            print_time(buffer, tv, d, 0, 0);
//#pragma omp parallel for ordered shared(abort, entropy_acc)
//            for (int l = 0xda; l < 0xe2 + 10; l++) {
//#pragma omp flush(abort)
//                if (!abort) {
//                    for (int j = 0xa0; j < 0xac + 10; j++) {
//                        for (int k = 0xa0; k < 0xa3 + 10; k++) {
//                            for (int m = 0x90; m < 0xa5 + 10; m++) {
        for (int d = 0x00; d < 256; d++) {
            for (int l = 0x00; l < 0x256; l++) {
                char buffer[30];
                struct timeval tv;
                print_time(buffer, tv, d, l, 0);
#pragma omp parallel for ordered shared(entropy_acc)
                for (int j = 0x00; j < 256; j++) {
                    for (int k = 0x00; k < 256; k++) {
                        for (int m = 0x00; m < 256; m++) {

//        for (int d = 0x1a; d < 0x1a + 1; d++) {
//#pragma omp parallel for
//            for (int l = 0xe2; l < 0xe2 + 1; l++) {
//                for (int j = 0xac; j < 0xac + 1; j++) {
//                    for (int k = 0xa3; k < 0xa3 + 1; k++) {
//                        for (int m = 0xa5; m < 0xa5 + 1; m++) {
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
//                            printf("\n");
//                            printf("--- %02x ", (unsigned int) (k1 >> 32));
//                            printf("%02x ", (unsigned int) (((k1 << 8) & 0xff00000000) >> 32));
//                            printf("%02x ", (unsigned int) (((k1 << 16) & 0xff00000000) >> 32));
//                            printf("%02x ", (unsigned int) (((k1 << 24) & 0xff00000000) >> 32));
//                            printf("%02x === ", (unsigned int) (((k1 << 32) & 0xff00000000) >> 32));

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

                            unsigned char ambe_d_copy[1000][49];
                            for (int i = 0; i < 1000; i++) {
                                for (int u = 0; u < 49; u++) {
                                    ambe_d_copy[i][u] = state->ambe_d[i][u];
                                }
                            }

                            unsigned char b0_arr[1000];

                            for (int w = 0; w < state->ambe_count; ++w) {
                                pos = state->DMRvcL_p[w];
                                for (int i = 0; i < 49; i++) {
                                    ambe_d_copy[w][i] ^= pN[pos];
                                    pos++;
                                }
                                unsigned char b0 = 0;
                                b0 |= ambe_d_copy[w][0] << 6;
                                b0 |= ambe_d_copy[w][1] << 5;
                                b0 |= ambe_d_copy[w][2] << 4;
                                b0 |= ambe_d_copy[w][3] << 3;
                                b0 |= ambe_d_copy[w][37] << 2;
                                b0 |= ambe_d_copy[w][38] << 1;
                                b0 |= ambe_d_copy[w][39];
                                b0_arr[w] = b0;
                            }

                            double entropy = calc_entropy(b0_arr, state->ambe_count);
                            if (entropy < entropy_acc) {
                                entropy_acc = entropy;
                                key = k1;
                            }
                        }
                    }
                }
            }
        }
        printf("\n");
        printf("--- %f ===\n", entropy_acc);
        printf("--- %02lx ===\n", (unsigned long) (key >> 24));

    }

    exit:
    state->audio_count++;
}
