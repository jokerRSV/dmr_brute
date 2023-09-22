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

static double calc_entropy(const unsigned char *f, const int length) {
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

void print_time(int i, int j, int k) {
    struct timeval tv;
    char buffer[30];
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    printf("%x %x %x === ", i, j, k);
    strftime(buffer, 30, "%T.", localtime(&curtime));
    printf("%s%ld", buffer, tv.tv_usec);
    fflush(stdout);
}

void processMbeFrame(dsd_state *state, char ambe_fr[4][24]) {
    unsigned char ambe_d[49];

    int start = 120;
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

        //1a == 26
        //e2 == 226
        //ac == 172
        //a3 == 163
        //a5 == 165
        volatile double entropy_acc = 1;
        volatile unsigned long key = 0;

        int d, l, j, k, m;

        d = 0;
//        for (d = 0; d < 256; d++) {
//        for (l = 0; l < 256; l++) {
//            print_time(d, l, 0);
//            printf("    --- %f ===", entropy_acc);
//            printf(" --- %02x ", (unsigned int) ((key & 0xff00000000) >> 32));
//            printf("%02x ", (unsigned int) (((key << 8) & 0xff00000000) >> 32));
//            printf("%02x ", (unsigned int) (((key << 16) & 0xff00000000) >> 32));
//            printf("%02x ", (unsigned int) (((key << 24) & 0xff00000000) >> 32));
//            printf("%02x === \n", (unsigned int) (((key << 32) & 0xff00000000) >> 32));
//#pragma omp parallel for default(none) schedule(static) private(j, k, m) shared(d, l, num, state, entropy_acc, key)
//            for (j = 0; j < 256; j++) {
//                for (k = 0; k < 256; k++) {
//                    for (m = 0; m < 256; m++) {

//        for (d = 10; d < 0x1a + 20; d++) {
        for (l = 0xe2 - 10; l < 0xe2 + 20; l++) {
            print_time(d, l, 0);
            printf("    --- %f ===", entropy_acc);
            printf(" --- %02x ", (unsigned int) ((key & 0xff00000000) >> 32));
            printf("%02x ", (unsigned int) (((key << 8) & 0xff00000000) >> 32));
            printf("%02x ", (unsigned int) (((key << 16) & 0xff00000000) >> 32));
            printf("%02x ", (unsigned int) (((key << 24) & 0xff00000000) >> 32));
            printf("%02x === \n", (unsigned int) (((key << 32) & 0xff00000000) >> 32));
#pragma omp parallel for default(none) schedule(static) private(j, k, m) shared(d, l, num, state, entropy_acc, key)
            for (j = 0xac - 10; j < 0xac + 20; j++) {
                for (k = 0xa3 - 10; k < 0xa3 + 20; k++) {
                    for (m = 0xa5 - 10; m < 0xa5 + 20; m++) {
                        unsigned long k1;
                        k1 = 0;
                        k1 |= (unsigned long long) d << 32;
                        k1 |= (unsigned long long) l << 24;
                        k1 |= (unsigned long long) j << 16;
                        k1 |= (unsigned long long) k << 8;
                        k1 |= (unsigned long long) m;

                        unsigned char T_Key[256];
                        for (int i = 0; i < 64; i++) {
                            T_Key[i] = (unsigned char) (((k1 << i) & 0x8000000000) >> 39);
                        }

                        int pos = 0;
                        unsigned char pN[882];
                        for (int i = 0; i < 882; i++) {
                            pN[i] = T_Key[pos++];
                            pos = pos % 40;
                        }

                        unsigned char ambe_d_copy[num][49];
                        for (int i = 0; i < num; i++) {
                            for (int u = 0; u < 49; u++) {
                                ambe_d_copy[i][u] = state->ambe_d[i][u];
                            }
                        }

                        unsigned char b0_arr[num];

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
                        double local_entropy;
#pragma omp flush
#pragma omp atomic read
                        local_entropy = entropy_acc;
                        if (entropy < local_entropy) {
#pragma omp atomic write
                            entropy_acc = entropy;
#pragma omp flush
#pragma omp atomic write
                            key = k1;
#pragma omp flush
//                            printf(" --- %02lx ---- ", k1);
//                            printf(" --- %f === \n", local_entropy);
                        }
                    }
                }
            }
        }
//        }
        printf("\n");
        printf("--- %f ===\n", entropy_acc);
        printf("--- %02lx ===\n", key);
    }

    state->audio_count++;
}
