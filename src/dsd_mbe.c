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

int Pr[256] = {
        0x0000, 0x1F00, 0xE300, 0xFC00, 0x2503, 0x3A03, 0xC603, 0xD903,
        0x4A05, 0x5505, 0xA905, 0xB605, 0x6F06, 0x7006, 0x8C06, 0x9306,
        0x2618, 0x3918, 0xC518, 0xDA18, 0x031B, 0x1C1B, 0xE01B, 0xFF1B,
        0x6C1D, 0x731D, 0x8F1D, 0x901D, 0x491E, 0x561E, 0xAA1E, 0xB51E, //31
        0x4B28, 0x5428, 0xA828, 0xB728, 0x6E2B, 0x712B, 0x8D2B, 0x922B,
        0x012D, 0x1E2D, 0xE22D, 0xFD2D, 0x242E, 0x3B2E, 0xC72E, 0xD82E,
        0x6D30, 0x7230, 0x8E30, 0x9130, 0x4833, 0x5733, 0xAB33, 0xB433,
        0x2735, 0x3835, 0xC435, 0xDB35, 0x0236, 0x1D36, 0xE136, 0xFE36, //63
        0x2B49, 0x3449, 0xC849, 0xD749, 0x0E4A, 0x114A, 0xED4A, 0xF24A,
        0x614C, 0xAE4C, 0x824C, 0x9D4C, 0x444F, 0x5B4F, 0xA74F, 0xB84F,
        0x0D51, 0x1251, 0xEE51, 0xF151, 0x2852, 0x3752, 0xCB52, 0xD452,
        0x4754, 0x5854, 0xA454, 0xBB54, 0x6257, 0x7D57, 0x8157, 0x9E57, //95
        0x6061, 0x7F61, 0x8361, 0x9C61, 0x4562, 0x5A62, 0xA662, 0xB962,
        0x2A64, 0x3564, 0xC964, 0xD664, 0x0F67, 0x1067, 0xEC67, 0xF367,
        0x4679, 0x5979, 0xA579, 0xBA79, 0x637A, 0x7C7A, 0x807A, 0x9F7A,
        0x0C7C, 0x137C, 0xEF7C, 0xF07C, 0x297F, 0x367F, 0xCA7F, 0xD57F, //127
        0x4D89, 0x5289, 0xAE89, 0xB189, 0x688A, 0x778A, 0x8B8A, 0x948A,
        0x078C, 0x188C, 0xE48C, 0xFB8C, 0x228F, 0x3D8F, 0xC18F, 0xDE8F,
        0x6B91, 0x7491, 0x8891, 0x9791, 0x4E92, 0x5192, 0xAD92, 0xB292,
        0x2194, 0x3E94, 0xC294, 0xDD94, 0x0497, 0x1B97, 0xE797, 0xF897, //159
        0x06A1, 0x19A1, 0xE5A1, 0xFAA1, 0x23A2, 0x3CA2, 0xC0A2, 0xDFA2,
        0x4CA4, 0x53A4, 0xAFA4, 0xB0A4, 0x69A7, 0x76A7, 0x8AA7, 0x95A7,
        0x20B9, 0x3FB9, 0xC3B9, 0xDCB9, 0x05BA, 0x1ABA, 0xE6BA, 0xF9BA,
        0x6ABC, 0x75BC, 0x89BC, 0x96BC, 0x4FBF, 0x50BF, 0xACBF, 0xB3BF, //191
        0x66C0, 0x79C0, 0x85C0, 0x9AC0, 0x43C3, 0x5CC3, 0xA0C3, 0xBFC3,
        0x2CC5, 0x33C5, 0xCFC5, 0xD0C5, 0x09C6, 0x16C6, 0xEAC6, 0xF5C6,
        0x84D0, 0x85DF, 0x8AD3, 0x8BDC, 0xB6D5, 0xB7DA, 0xB8D6, 0xB9D9,
        0xD0DA, 0xD1D5, 0xDED9, 0xDFD6, 0xE2DF, 0xE3D0, 0xECDC, 0xEDD3, //223
        0x2DE8, 0x32E8, 0xCEE8, 0xD1E8, 0x08EB, 0x17EB, 0xEBEB, 0xF4EB,
        0x67ED, 0x78ED, 0x84ED, 0x9BED, 0x42EE, 0x5DEE, 0xA1EE, 0xBEEE,
        0x0BF0, 0x14F0, 0xE8F0, 0xF7F0, 0x2EF3, 0x31F3, 0xCDF3, 0xD2F3,
        0x41F5, 0x5EF5, 0xA2F5, 0xBDF5, 0x64F6, 0x7BF6, 0x87F6, 0x98F6 //255
};

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
    int i;
    unsigned char ambe_d[49];

    for (i = 0; i < 49; i++) {
        ambe_d[i] = 0;
    }
    int start = 30;
    int end = 40;
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
        char err_str[64];
        memset(err_str, 0, 64 * sizeof(char));
        state->DMRvcL = 0;

        char buffer[30];
        struct timeval tv;
        unsigned char ds = 0x1a;
        unsigned char js = 0xe2;
        unsigned char ks = 0xac;
        unsigned char ls = 0xa3;
        unsigned char ms = 0xa5;
//        unsigned char is = 0x0d;
//        unsigned char js = 0x71;
//        unsigned char ks = 0x86;
//        unsigned char ls = 0xa3;
//        unsigned char ms = 0xa5;

        unsigned long long int k1;
        unsigned char T_Key[256] = {0};
        unsigned char pN[882] = {0};

        for (int d = 0; d < 256; d += ds) {
//            printf("%x\n", i);
            for (int j = 0; j < 256; j += js) {
                for (int k = 0; k < 256; k += ks) {
//                    print_time(buffer, tv, i, j, k);
//#pragma omp parallel for
                    for (int l = 0; l < 256; l += ls) {
                        for (int m = 0; m < 256; m += ms) {
                            k1 = 0;
                            k1 |= (unsigned long long) d << 32;
                            k1 |= (unsigned long long) j << 24;
                            k1 |= (unsigned long long) k << 16;
                            k1 |= (unsigned long long) l << 8;
                            k1 |= (unsigned long long) m;
                            fprintf(stderr, "\n");
                            fprintf(stderr, "--- %02x ", (unsigned int) (k1 >> 32));
                            fprintf(stderr, "%02x ", (unsigned int) (((k1 << 8) & 0xff00000000) >> 32));
                            fprintf(stderr, "%02x ", (unsigned int) (((k1 << 16) & 0xff00000000) >> 32));
                            fprintf(stderr, "%02x ", (unsigned int) (((k1 << 24) & 0xff00000000) >> 32));
                            fprintf(stderr, "%02x === ", (unsigned int) (((k1 << 32) & 0xff00000000) >> 32));

                            k1 = k1 << 24;

                            for (int i = 0; i < 64; i++) {
                                T_Key[i] = (char) (((k1 << i) & 0x8000000000000000) >> 63);
                            }

                            int pos = 0;
                            for (int i = 0; i < 882; i++) {
                                pN[i] = T_Key[pos++];
                                pos = pos % 40;
                            }

                            snprintf(opts->wav_out_file, 20, "sample_%x%x%x%x%x.wav", d, j, k, l, m);
                            openWavOutFile(opts, state);
                            //play stored voice data
                            for (int j = 0; j < state->ambe_count; ++j) {
                                pos = state->DMRvcL_p[j];
                                for (int i = 0; i < 49; i++) {
                                    state->ambe_d[j][i] ^= pN[pos];
                                    pos++;
                                }

                                mbe_processAmbe2450Dataf(state->audio_out_temp_buf,
                                                         &errs, &errs2, err_str,
                                                         state->ambe_d[j],
                                                         state->cur_mp_store[j],
                                                         state->prev_mp_store[j],
                                                         state->prev_mp_store[j],
                                                         1);
                                processAudio(opts, state);
                                writeSynthesizedVoice(opts, state);
                                //playSynthesizedVoice(opts, state);
                            }
                            sf_close(opts->wav_out_f);

                            if (d == 0x1a && j == 0xe2 && k == 0xac && l == 0xa3 && m == 0xa5) {
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
