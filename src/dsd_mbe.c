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

static double calc_entropy(unsigned char *f, const int length) {
    int counts[256] = {0};
    double entropy = 0;
    for (int i = 0; i < length; ++i) {
        counts[*f]++;
        f++;
    }
    for (int i = 0; i < 256; ++i) {
        if (counts[i] != 0) {
            double freq = counts[i] / (double) length;
            entropy -= freq * log(freq) / 4;
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

void addToKeyBuff(dsd_state *state, unsigned long key) {
    state->key_buff[state->key_buff_count++] = key;
    if (state->key_buff_count >= 5000) state->key_buff_count = 0;
}

void processMbeFrame(dsd_state *state, char ambe_fr[4][24], dsd_opts *opts) {
    char ambe_d[49];

    int start = 120;
    const int num = 150;
//    char ambe_d_flat[num] = {0};
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

        //1a == 26
        //e2 == 226
        //ac == 172
        //a3 == 163
        //a5 == 165
        double entropy_acc = 1;
        unsigned long key = 0;

        int d, l, j, k, m;
        int mod_div = opts->mod_div;
        int lastKeys = opts->lastKeys;
        printf("mod division number: %d\n", mod_div);
        printf("last keys to print num: %d\n", lastKeys);

//        for (d = 0; d < 256; ++d) {
//            for (l = 0; l < 256; ++l) {
//                print_time(d, l, 0);
//                printf("    --- %f ===", entropy_acc);
//                printf(" --- %02x ", (int) (key >> 32) & 0xff);
//                printf("%02x ", (int) (key >> 24) & 0xff);
//                printf("%02x ", (int) (key >> 16) & 0xff);
//                printf("%02x ", (int) (key >> 8) & 0xff);
//                printf("%02x === \n", (int) key & 0xff);
//#pragma omp parallel for default(none) schedule(static) private(j, k, m) shared(d, l, num, state, entropy_acc, key, mod_div)
//                for (j = 0; j < 256; ++j) {
//                    for (k = 0; k < 256; ++k) {
//                        for (m = 0; m < 256; ++m) {
        for (d = 25 - 5; d < 27 + 5; ++d) {
            for (l = 0xe2 - 5; l < 0xe2 + 5; l++) {
                print_time(d, l, 0);
                printf("    --- %lf ===", entropy_acc);
                printf(" --- %02x ", (int) (key >> 32) & 0xff);
                printf("%02x ", (int) (key >> 24) & 0xff);
                printf("%02x ", (int) (key >> 16) & 0xff);
                printf("%02x ", (int) (key >> 8) & 0xff);
                printf("%02x === \n", (int) key & 0xff);
#pragma omp parallel for default(none) schedule(static) private(j, k, m) shared(d, l, num, state, entropy_acc, key, mod_div)
                for (j = 0xac - 10; j < 0xac + 20; j++) {
                    for (k = 0xa3 - 100; k < 0xa3 + 20; k++) {
                        for (m = 0xa5 - 10; m < 0xa5 + 20; m++) {

                            if (m % mod_div == 0) {
                                unsigned long k1;
                                k1 = 0;
                                k1 |= (unsigned long long) d << 32;
                                k1 |= (unsigned long long) l << 24;
                                k1 |= (unsigned long long) j << 16;
                                k1 |= (unsigned long long) k << 8;
                                k1 |= (unsigned long long) m;

//                                char T_Key[40];
//                                for (int i = 0; i < 40; i++)
//                                    T_Key[i] = (char) ((k1 >> (39 - i)) & 0x01);

//                                unsigned char ambe_d_copy[num][49];
//                                for (int i = 0; i < num; i++) {
//                                    for (int u = 0; u < 49; u++) {
//                                        ambe_d_copy[i][u] = state->ambe_d[i][u];
//                                    }
//                                }

                                unsigned char b0_arr[num];

                                int pos;
                                for (int w = 0; w < num; ++w) {
                                    pos = state->DMRvcL_p[w];
//                                    for (int i = 0; i < 49; i++) {
//                                        ambe_d_copy[w][i] ^= T_Key[pos % 40];
//                                        pos++;
//                                    }
                                    unsigned char b0 = 0;
                                    b0 |= (state->ambe_d[w][0] ^ (char) ((k1 >> (39 - (pos % 40))) & 0x01)) << 6;
                                    b0 |= (state->ambe_d[w][1] ^ (char) ((k1 >> (39 - ((pos + 1) % 40))) & 0x01)) << 5;
                                    b0 |= (state->ambe_d[w][2] ^ (char) ((k1 >> (39 - ((pos + 2) % 40))) & 0x01)) << 4;
                                    b0 |= (state->ambe_d[w][3] ^ (char) ((k1 >> (39 - ((pos + 3) % 40))) & 0x01)) << 3;
                                    b0 |= (state->ambe_d[w][37] ^ (char) ((k1 >> (39 - ((pos + 37) % 40))) & 0x01))
                                            << 2;
                                    b0 |= (state->ambe_d[w][38] ^ (char) ((k1 >> (39 - ((pos + 38) % 40))) & 0x01))
                                            << 1;
                                    b0 |= (state->ambe_d[w][39] ^ (char) ((k1 >> (39 - ((pos + 39) % 40))) & 0x01));
                                    b0_arr[w] = b0;
                                }

                                double entropy = calc_entropy(b0_arr, num);
#pragma omp flush
#pragma omp critical
                                {
                                    if (entropy < entropy_acc) {
                                        entropy_acc = entropy;
                                        key = k1;
                                        printf(" --- %02lx ---- ", k1);
                                        printf(" --- %lf === \n", entropy_acc);
                                        addToKeyBuff(state, k1);
                                    }
                                }
                            }


                        }
                    }
                }
            }
        }
        printf("\n");
        printf("result --- %lf ===\n", entropy_acc);
        printf("--- %02lx ===\n", key);
        printf("the last keys: \n");
        int lastNums;
        int diff = state->key_buff_count - lastKeys;
        if (diff > 0) lastNums = diff;
        else lastNums = 0;
        for (int i = lastNums; i < state->key_buff_count; ++i) {
            printf("%02lx\n", state->key_buff[i]);
        }
    }

    state->audio_count++;
}
