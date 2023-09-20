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

//pa_sample_spec ss;
//pa_sample_spec tt;
//pa_sample_spec zz;
//pa_sample_spec cc;

void
openAudioInDevice(dsd_opts *opts) {
    char *extension;
    const char ch = '.';
    extension = strrchr(opts->audio_in_dev, ch); //return extension if this is a .wav or .bin file

    //if no extension set, give default of .wav -- bugfix for github issue #105
    // if (extension == NULL) extension = ".wav";

    // get info of device/file
    if (strncmp(opts->audio_in_dev, "-", 1) == 0) {
        opts->audio_in_type = 1;
        opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
        opts->audio_in_file_info->samplerate = opts->wav_sample_rate;
        opts->audio_in_file_info->channels = 1;
        opts->audio_in_file_info->seekable = 0;
        opts->audio_in_file_info->format = SF_FORMAT_RAW | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
        opts->audio_in_file = sf_open_fd(fileno(stdin), SFM_READ, opts->audio_in_file_info, 0);

        if (opts->audio_in_file == NULL) {
            fprintf(stderr, "Error, couldn't open stdin with libsndfile: %s\n", sf_strerror(NULL));
            exit(1);
        }
    } else if (strncmp(opts->audio_in_dev, "tcp", 3) == 0) {
        opts->audio_in_type = 8;
    } else if (strncmp(opts->audio_in_dev, "udp", 3) == 0) {
        opts->audio_in_type = 6;
    } else if (strncmp(opts->audio_in_dev, "rtl", 3) == 0) {
        opts->audio_in_type = 0;
    } else if (strncmp(opts->audio_in_dev, "pulse", 5) == 0) {
        opts->audio_in_type = 0;
    } else if ((strncmp(opts->audio_in_dev, "/dev/dsp", 8) == 0)) {
        opts->audio_in_type = 5;
    } else if (extension == NULL) {
    } else if (strncmp(extension, ".bin", 3) == 0) {
        //open as wav file as last resort, wav files subseptible to sample rate issues if not 48000
    } else {
        struct stat stat_buf;
        if (stat(opts->audio_in_dev, &stat_buf) != 0) {
            fprintf(stderr, "Error, couldn't open wav file %s\n", opts->audio_in_dev);
            exit(1);
        }
        if (S_ISREG(stat_buf.st_mode)) {
            opts->audio_in_type = 2; //two now, seperating STDIN and wav files
            opts->audio_in_file_info = calloc(1, sizeof(SF_INFO));
            opts->audio_in_file_info->samplerate = opts->wav_sample_rate;
            opts->audio_in_file_info->channels = 1;
            opts->audio_in_file_info->channels = 1;
            opts->audio_in_file_info->seekable = 0;
            opts->audio_in_file_info->format = SF_FORMAT_RAW | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
            //
            opts->audio_in_file = sf_open(opts->audio_in_dev, SFM_READ, opts->audio_in_file_info);

            if (opts->audio_in_file == NULL) {
                fprintf(stderr, "Error, couldn't open wav file %s\n", opts->audio_in_dev);
                exit(1);
            }

        }
            //open pulse audio if no bin or wav
        else //seems this condition is never met
        {
            //opts->audio_in_type = 5; //not sure if this works or needs to openPulse here
            fprintf(stderr, "Error, couldn't open input file.\n");
            exit(1);
        }
    }
    if (opts->split == 1) {
        fprintf(stderr, "Audio In Device: %s\n", opts->audio_in_dev);
    } else {
        fprintf(stderr, "Audio In/Out Device: %s\n", opts->audio_in_dev);
    }
}
