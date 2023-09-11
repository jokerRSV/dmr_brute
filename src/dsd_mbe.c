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

    if (state->currentslot == 0) {

        int pos = 0;

        unsigned char T_Key[256] = {0};
        unsigned char pN[882] = {0};

        state->errs = mbe_eccAmbe3600x2450C0(ambe_fr);
        mbe_demodulateAmbe3600x2450Data(ambe_fr);
        state->errs2 = mbe_eccAmbe3600x2450Data(ambe_fr, ambe_d);

        char buffer[30];
        struct timeval tv;
        unsigned char is = 0x1a;
        unsigned char js = 0xe2;
        unsigned char ks = 0xac;
        unsigned char ls = 0xa3;
        unsigned char ms = 0xa5;
        unsigned long long int k1;
        for (i = is; i < 256; i++) {
            printf("%x\n", i);
            for (int j = js; j < 256; j++) {
                for (int k = ks; k < 256; k++) {
                    print_time(buffer, tv, i, j, k);
#pragma omp parallel for
                    for (int l = ls; l < 256; l++) {
                        for (int m = ms; m < 256; m++) {
                            k1 = 0;
                            k1 |= (unsigned long long) is << 32;
                            k1 |= (unsigned long long) js << 24;
                            k1 |= (unsigned long long) ks << 16;
                            k1 |= (unsigned long long) ls << 8;
                            k1 |= (unsigned long long) ms;

                            k1 = k1 << 24;

                            for (i = 0; i < 64; i++) {
                                T_Key[i] = (char) (((k1 << i) & 0x8000000000000000) >> 63);
                            }

                            for (i = 0; i < 882; i++) {
                                pN[i] = T_Key[pos++];
                                pos = pos % 40;
                            }

                            pos = (state->DMRvcL) * 49;
                            for (i = 0; i < 49; i++) {
                                ambe_d[i] ^= pN[pos];
                                pos++;
                            }
                            state->DMRvcL++;

                            int b0, b1, b2, b3, b4, b5, b6, b7, b8;

                            b0 = 0;
                            b0 |= ambe_d[0] << 6;
                            b0 |= ambe_d[1] << 5;
                            b0 |= ambe_d[2] << 4;
                            b0 |= ambe_d[3] << 3;
                            b0 |= ambe_d[37] << 2;
                            b0 |= ambe_d[38] << 1;
                            b0 |= ambe_d[39];

                            b1 = 0;
                            b1 |= ambe_d[4] << 4;
                            b1 |= ambe_d[5] << 3;
                            b1 |= ambe_d[6] << 2;
                            b1 |= ambe_d[7] << 1;
                            b1 |= ambe_d[35];

                            b2 = 0;
                            b2 |= ambe_d[8] << 4;
                            b2 |= ambe_d[9] << 3;
                            b2 |= ambe_d[10] << 2;
                            b2 |= ambe_d[11] << 1;
                            b2 |= ambe_d[36];

                            b3 = 0;
                            b3 |= ambe_d[12] << 8;
                            b3 |= ambe_d[13] << 7;
                            b3 |= ambe_d[14] << 6;
                            b3 |= ambe_d[15] << 5;
                            b3 |= ambe_d[16] << 4;
                            b3 |= ambe_d[17] << 3;
                            b3 |= ambe_d[18] << 2;
                            b3 |= ambe_d[19] << 1;
                            b3 |= ambe_d[40];

                            b4 = 0;
                            b4 |= ambe_d[20] << 6;
                            b4 |= ambe_d[21] << 5;
                            b4 |= ambe_d[22] << 4;
                            b4 |= ambe_d[23] << 3;
                            b4 |= ambe_d[41] << 2;
                            b4 |= ambe_d[42] << 1;
                            b4 |= ambe_d[43];

                            b5 = 0;
                            b5 |= ambe_d[24] << 4;
                            b5 |= ambe_d[25] << 3;
                            b5 |= ambe_d[26] << 2;
                            b5 |= ambe_d[27] << 1;
                            b5 |= ambe_d[44];

                            b6 = 0;
                            b6 |= ambe_d[28] << 3;
                            b6 |= ambe_d[29] << 2;
                            b6 |= ambe_d[30] << 1;
                            b6 |= ambe_d[45];

                            b7 = 0;
                            b7 |= ambe_d[31] << 3;
                            b7 |= ambe_d[32] << 2;
                            b7 |= ambe_d[33] << 1;
                            b7 |= ambe_d[46];

                            b8 = 0;
                            b8 |= ambe_d[34] << 2;
                            b8 |= ambe_d[47] << 1;
                            b8 |= ambe_d[48];

//    if ((b0 >= 120) && (b0 <= 123)) // if w0 bits are 1111000, 1111001, 1111010 or 1111011, frame is erasure
//    } else if ((b0 == 124) || (b0 == 125)) // if w0 bits are 1111100 or 1111101, frame is silence
//    } else if ((b0 == 126) || (b0 == 127)) // if w0 bits are 1111110 or 1111111, frame is tone

                            fprintf(stderr, "\n%d, %d, %d, %d, %d, %d, %d, %d, %d", b0, b1, b2, b3, b4, b5, b6, b7, b8);

                        }
                    }
                }
            }
        }

//        PrintAMBEData(opts, state, PrintAMBEData);

        mbe_processAmbe2450Dataf(state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str, ambe_d,
                                 state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

        state->debug_audio_errors += state->errs2;
//            if (opts->audio_out == 1 && state->audioCount >= 7 * 10 && state->audioCount <= 7 * 15) {
        if (opts->audio_out == 1) {
            processAudio(opts, state);
            writeSynthesizedVoice(opts, state);
            playSynthesizedVoice(opts, state);
        }
//        state->sample_count++;
    }

    //stereo slots and slot 1 (right slot)
    if (state->currentslot == 1) {
        state->errsR = mbe_eccAmbe3600x2450C0(ambe_fr);
        mbe_demodulateAmbe3600x2450Data(ambe_fr);
        state->errs2R = mbe_eccAmbe3600x2450Data(ambe_fr, ambe_d);

        //slot 1
        mbe_processAmbe2450Dataf(state->audio_out_temp_bufR, &state->errsR, &state->errs2R, state->err_strR,
                                 ambe_d, state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2,
                                 opts->uvquality);
        processAudioR(opts, state);
        writeSynthesizedVoiceR(opts, state);
        playSynthesizedVoiceR(opts, state);
    }
}
