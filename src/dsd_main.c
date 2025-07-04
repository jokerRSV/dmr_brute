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

#define _MAIN

#include "dsd.h"
#include "dmr_const.h"

int comp(const void *a, const void *b) {
    if (*((const int *) a) == *((const int *) b))
        return 0;
    else if (*((const int *) a) < *((const int *) b))
        return -1;
    else
        return 1;
}

//struct for checking existence of directory to write to
struct stat st = {0};
char wav_file_directory[1024] = {0};
char dsp_filename[1024] = {0};
unsigned long long int p2vars = 0;

char *pEnd; //bugfix

void
noCarrier(dsd_opts *opts, dsd_state *state) {

    state->dibit_buf_p = state->dibit_buf + 200;
    memset(state->dibit_buf, 0, sizeof(int) * 200);
    //dmr buffer
    state->dmr_payload_p = state->dibit_buf + 200;
    memset(state->dmr_payload_buf, 0, sizeof(int) * 200);
    //dmr buffer end
    state->jitter = -1;
    state->lastsynctype = -1;
    state->carrier = 0;
    state->max = 15000;
    state->min = -15000;
    state->center = 0;
    state->err_str[0] = 0;
    state->err_strR[0] = 0;
    sprintf(state->fsubtype, "              ");
    sprintf(state->ftype, "             ");
    state->errs = 0;
//    state->audio_count = 0;
    state->ambe_count = 0;
    state->errs2 = 0;

    //zero out right away if not trunking
    if (opts->p25_trunk == 0) {
        state->lasttg = 0;
        state->lastsrc = 0;
        state->lasttgR = 0;
        state->lastsrcR = 0;

        //zero out vc frequencies?
        state->p25_vc_freq[0] = 0;
        state->p25_vc_freq[1] = 0;

        //only reset cap+ rest channel if not trunking
        state->dmr_rest_channel = -1;

        //zero out nxdn site/srv/cch info if not trunking
        state->nxdn_location_site_code = 0;
        state->nxdn_location_sys_code = 0;

        //channel access information
        state->nxdn_rcn = 0;
        state->nxdn_base_freq = 0;
        state->nxdn_step = 0;
        state->nxdn_bw = 0;
    }

    state->lastp25type = 0;
    state->repeat = 0;
    state->nac = 0;
    state->numtdulc = 0;
    sprintf(state->slot1light, "%s", "");
    sprintf(state->slot2light, "%s", "");
    state->firstframe = 0;
    if (opts->audio_gain == (float) 0) {
        if (state->audio_smoothing == 0) state->aout_gain = 15; //15
        else state->aout_gain = 25; //25
    }

    if (opts->audio_gainR == (float) 0) {
        if (state->audio_smoothing == 0) state->aout_gainR = 15; //15
        else state->aout_gainR = 25; //25
    }
    memset(state->aout_max_buf, 0, sizeof(float) * 200);
    state->aout_max_buf_p = state->aout_max_buf;
    state->aout_max_buf_idx = 0;

    memset(state->aout_max_bufR, 0, sizeof(float) * 200);
    state->aout_max_buf_pR = state->aout_max_bufR;
    state->aout_max_buf_idxR = 0;

    sprintf(state->algid, "________");
    sprintf(state->keyid, "________________");
    mbe_initMbeParms(state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
    mbe_initMbeParms(state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2);

    state->dmr_ms_mode = 0;

    //not sure if desirable here or not just yet, may need to disable a few of these
    state->payload_mi = 0;
    state->payload_miR = 0;
    state->payload_mfid = 0;
    state->payload_mfidR = 0;
    state->payload_algid = 0;
    state->payload_algidR = 0;
    state->payload_keyid = 0;
    state->payload_keyidR = 0;

    state->HYTL = 0;
    state->HYTR = 0;
    state->DMRvcL = 0;
//    state->count = 0;

    state->DMRvcR = 0;
    state->dropL = 256;
    state->dropR = 256;

    state->payload_miN = 0;
    state->p25vc = 0;
    state->payload_miP = 0;

    //initialize dmr data header source
    state->dmr_lrrp_source[0] = 0;
    state->dmr_lrrp_source[1] = 0;


    //initialize data header bits
    state->data_header_blocks[0] = 1;  //initialize with 1, otherwise we may end up segfaulting when no/bad data header
    state->data_header_blocks[1] = 1; //when trying to fill the superframe and 0-1 blocks give us an overflow
    state->data_header_padding[0] = 0;
    state->data_header_padding[1] = 0;
    state->data_header_format[0] = 7;
    state->data_header_format[1] = 7;
    state->data_header_sap[0] = 0;
    state->data_header_sap[1] = 0;
    state->data_block_counter[0] = 1;
    state->data_block_counter[1] = 1;
    state->data_p_head[0] = 0;
    state->data_p_head[1] = 0;

    state->dmr_encL = 0;
    state->dmr_encR = 0;

    state->dmrburstL = 17;
    state->dmrburstR = 17;

    //reset P2 ESS_B fragments and 4V counter
    for (short i = 0; i < 4; i++) {
        state->ess_b[0][i] = 0;
        state->ess_b[1][i] = 0;
    }
    state->fourv_counter[0] = 0;
    state->fourv_counter[1] = 0;

    //values displayed in ncurses terminal
    // state->p25_vc_freq[0] = 0;
    // state->p25_vc_freq[1] = 0;

    //new nxdn stuff
    state->nxdn_part_of_frame = 0;
    state->nxdn_ran = 0;
    state->nxdn_sf = 0;
    memset(state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc)); //init on 1, bad CRC all
    state->nxdn_sacch_non_superframe = TRUE;
    memset(state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
    state->nxdn_alias_block_number = 0;
    memset(state->nxdn_alias_block_segment, 0, sizeof(state->nxdn_alias_block_segment));
    sprintf(state->nxdn_call_type, "%s", "");

    //unload keys when using keylaoder
    if (state->keyloader == 1) {
        state->R = 0; //NXDN, or RC4 (slot 1)
        state->RR = 0; //RC4 (slot 2)
        state->K = 0; //BP
        state->K1 = 0; //tera 10 char BP
        state->H = 0; //shim for above
    }

    //forcing key application will re-enable this at the time of voice tx
    state->nxdn_cipher_type = 0;

    //dmr overaching manufacturer in use for a particular system or radio
    // state->dmr_mfid = -1;

    //dmr slco stuff
    memset(state->dmr_cach_fragment, 1, sizeof(state->dmr_cach_fragment));
    state->dmr_cach_counter = 0;

    //initialize unified dmr pdu 'superframe'
    memset(state->dmr_pdu_sf, 0, sizeof(state->dmr_pdu_sf));
    memset(state->data_header_valid, 0, sizeof(state->data_header_valid));

    //initialize cap+ bits and block num storage
    memset(state->cap_plus_csbk_bits, 0, sizeof(state->cap_plus_csbk_bits));
    memset(state->cap_plus_block_num, 0, sizeof(state->cap_plus_block_num));

    //init confirmed data individual block crc as invalid
    memset(state->data_block_crc_valid, 0, sizeof(state->data_block_crc_valid));

    //embedded signalling
    memset(state->dmr_embedded_signalling, 0, sizeof(state->dmr_embedded_signalling));

    //late entry mi fragments
    memset(state->late_entry_mi_fragment, 0, sizeof(state->late_entry_mi_fragment));

    //dmr talker alias new/fixed stuff
    memset(state->dmr_alias_format, 0, sizeof(state->dmr_alias_format));
    memset(state->dmr_alias_len, 0, sizeof(state->dmr_alias_len));
    memset(state->dmr_alias_block_segment, 0, sizeof(state->dmr_alias_block_segment));
    memset(state->dmr_embedded_gps, 0, sizeof(state->dmr_embedded_gps));
    memset(state->dmr_lrrp_gps, 0, sizeof(state->dmr_lrrp_gps));

    // memset(state->active_channel, 0, sizeof(state->active_channel));

    if (time(NULL) - state->last_cc_sync_time > 10) //ten seconds of no carrier
    {
        state->dmr_rest_channel = -1;
        state->p25_vc_freq[0] = 0;
        state->p25_vc_freq[1] = 0;
        state->dmr_mfid = -1;
        opts->p25_is_tuned = 0;
        memset(state->active_channel, 0, sizeof(state->active_channel));
    }

    opts->dPMR_next_part_of_superframe = 0;

    state->dPMRVoiceFS2Frame.CalledIDOk = 0;
    state->dPMRVoiceFS2Frame.CallingIDOk = 0;
    memset(state->dPMRVoiceFS2Frame.CalledID, 0, 8);
    memset(state->dPMRVoiceFS2Frame.CallingID, 0, 8);
    memset(state->dPMRVoiceFS2Frame.Version, 0, 8);
} //nocarrier

void
initOpts(dsd_opts *opts) {

    opts->onesymbol = 10;
    opts->mbe_in_file[0] = 0;
    opts->mbe_in_f = NULL;
    opts->errorbars = 1;
    opts->datascope = 0;
    opts->symboltiming = 0;
    opts->verbose = 2;
    opts->p25enc = 0;
    opts->p25lc = 0;
    opts->p25status = 0;
    opts->p25tg = 0;
    opts->scoperate = 15;
    sprintf(opts->audio_in_dev, "pulse");
    sprintf(opts->audio_out_dev, "pulse");
    opts->audio_in_fd = -1;
    opts->audio_out_fd = -1;
    opts->audio_out_fdR = -1;

    opts->split = 0;
//    opts->playoffset = 0;
//    opts->playoffsetR = 0;
    opts->mbe_out_dir[0] = 0;
    opts->mbe_out_file[0] = 0;
    opts->mbe_out_fileR[0] = 0; //second slot on a TDMA system
    opts->mbe_out_path[0] = 0;
    opts->mbe_out_f = NULL;
    opts->mbe_out_fR = NULL; //second slot on a TDMA system
    opts->audio_gain = 0; //0
    opts->audio_gainR = 0; //0
    opts->audio_out = 1;
    opts->wav_out_file[0] = 0;
    opts->wav_out_fileR[0] = 0;
    opts->wav_out_file_raw[0] = 0;
    opts->symbol_out_file[0] = 0;
    opts->lrrp_out_file[0] = 0;
    //csv import filenames
    opts->group_in_file[0] = 0;
    opts->lcn_in_file[0] = 0;
    opts->chan_in_file[0] = 0;
    opts->key_in_file[0] = 0;
    //end import filenames
    opts->szNumbers[0] = 0;
    opts->symbol_out_f = NULL;
    opts->symbol_out = 0;
    opts->mbe_out = 0;
    opts->mbe_outR = 0; //second slot on a TDMA system
    opts->wav_out_f = NULL;
    opts->wav_out_fR = NULL;
    opts->wav_out_raw = NULL;

    opts->dmr_stereo_wav = 0; //flag for per call dmr stereo wav recordings
    //opts->wav_out_fd = -1;
    opts->serial_baud = 115200;
    sprintf(opts->serial_dev, "/dev/ttyUSB0");
    opts->frame_dstar = 0;
    opts->frame_x2tdma = 0;
    opts->frame_p25p1 = 1;
    opts->frame_p25p2 = 1;
    opts->frame_nxdn48 = 0;
    opts->frame_nxdn96 = 0;
    opts->frame_dmr = 1;
    opts->frame_dpmr = 0;
    opts->frame_provoice = 0;
    opts->frame_ysf = 0; //forgot to init this, and Cygwin treated as it was turned on.
    opts->mod_c4fm = 1;
    opts->mod_qpsk = 0;
    opts->mod_gfsk = 0;
    opts->uvquality = 3;
    opts->inverted_x2tdma = 1;    // most transmitter + scanner + sound card combinations show inverted signals for this
    opts->inverted_dmr = 0;       // most transmitter + scanner + sound card combinations show non-inverted signals for this
    opts->mod_threshold = 26;
    opts->ssize = 128; //36 default, max is 128, much cleaner data decodes on Phase 2 cqpsk at max
    opts->startFrame = 100;
    opts->frameSize = 150;
    opts->mod_div = 1;
    opts->lastKeys = 20;
    opts->msize = 1024; //15 default, max is 1024, much cleaner data decodes on Phase 2 cqpsk at max
    opts->delay = 0;
    opts->use_cosine_filter = 1;
    opts->unmute_encrypted_p25 = 0;
    //all RTL user options -- enabled AGC by default due to weak signal related issues
    opts->rtl_dev_index = 0;        //choose which device we want by index number
    opts->rtl_gain_value = 0;     //mid value, 0 - AGC - 0 to 49 acceptable values
    opts->rtl_squelch_level = 100; //100 by default, but only affects NXDN and dPMR during framesync test, compared to RMS value
    opts->rtl_volume_multiplier = 1; //sample multiplier; This multiplies the sample value to produce a higher 'inlvl' (probably best left unused)
    opts->rtl_udp_port = 0; //set UDP port for RTL remote -- 0 by default, will be making this optional for some external/legacy use cases (edacs-fm, etc)
    opts->rtl_bandwidth = 12; //default is 12, reverted back to normal on this (no inherent benefit)
    opts->rtlsdr_ppm_error = 0; //initialize ppm with 0 value;
    opts->rtlsdr_center_freq = 850000000; //set to an initial value (if user is using a channel map, then they won't need to specify anything other than -i rtl if desired)
    opts->rtl_started = 0;
    opts->rtl_rms = 0; //root means square power level on rtl input signal
    //end RTL user options
    opts->pulse_raw_rate_in = 48000;
    opts->pulse_raw_rate_out = 48000; //
    opts->pulse_digi_rate_in = 48000;
    opts->pulse_digi_rate_out = 24000; //new default for XDMA
    opts->pulse_raw_in_channels = 1;
    opts->pulse_raw_out_channels = 1;
    opts->pulse_digi_in_channels = 1; //2
    opts->pulse_digi_out_channels = 2; //new default for XDMA

    opts->wav_sample_rate = 48000; //default value (DSDPlus uses 96000 on raw signal wav files)
    opts->wav_interpolator = 1; //default factor of 1 on 48000; 2 on 96000; sample rate / decimator
    opts->wav_decimator = 48000; //maybe for future use?

    sprintf(opts->output_name, "XDMA");
    opts->pulse_flush = 1; //set 0 to flush, 1 for flushed
    opts->use_ncurses_terminal = 0;
    opts->ncurses_compact = 0;
    opts->ncurses_history = 1;
    opts->payload = 0;
    opts->inverted_dpmr = 0;
    opts->dmr_mono = 0;
    opts->dmr_stereo = 1;
    opts->aggressive_framesync = 1;

    //this may not matter so much, since its already checked later on
    //but better safe than sorry I guess
    opts->audio_in_type = 0;
    opts->audio_out_type = 0;

    opts->lrrp_file_output = 0;

    opts->monitor_input_audio = 0;

    opts->inverted_p2 = 0;
    opts->p2counter = 0;

    opts->call_alert = 0; //call alert beeper for ncurses

    //rigctl options
    opts->use_rigctl = 0;
    opts->rigctl_sockfd = 0;
    sprintf(opts->rigctlhostname, "%s", "localhost");

    //udp input options
    opts->udp_sockfd = 0;
    opts->udp_portno = 7355; //default favored by GQRX and SDR++
    opts->udp_hostname = "localhost";

    //tcp input options
    opts->tcp_sockfd = 0;
    opts->tcp_portno = 7355; //default favored by SDR++
    sprintf(opts->tcp_hostname, "%s", "localhost");

    opts->p25_trunk = 0; //0 disabled, 1 is enabled
    opts->p25_is_tuned = 0; //set to 1 if currently on VC, set back to 0 on carrier drop
    opts->trunk_hangtime = 1; //1 second hangtime by default before tuning back to CC, going sub 1 sec causes issues with cc slip

    opts->scanner_mode = 0; //0 disabled, 1 is enabled

    //reverse mute
    opts->reverse_mute = 0;

    //setmod bandwidth
    opts->setmod_bw = 0; //default to 0 - off

    //DMR Location Area - DMRLA B***S***
    opts->dmr_dmrla_is_set = 0;
    opts->dmr_dmrla_n = 0;

    //DMR Late Entry
    opts->dmr_le = 1; //re-enabled again

    //Trunking - Use Group List as Allow List
    opts->trunk_use_allow_list = 0; //disabled by default

    //Trunking - Tune Group Calls
    opts->trunk_tune_group_calls = 1; //enabled by default

    //Trunking - Tune Private Calls
    opts->trunk_tune_private_calls = 1; //enabled by default

    //Trunking - Tune Data Calls
    opts->trunk_tune_data_calls = 0; //disabled by default

    //Trunking - Tune Encrypted Calls (P25 only on applicable grants with svc opts)
    opts->trunk_tune_enc_calls = 1; //enabled by default

    opts->dPMR_next_part_of_superframe = 0;

    //OSS audio - Slot Preference
    //slot preference is used during OSS audio playback to
    //prefer one tdma voice slot over another when both are playing back
    //this is a fix to cygwin stuttering when both slots have voice
    opts->slot_preference = 0; //default prefer slot 1 -- state->currentslot = 0;

    //dsp structured file
    opts->dsp_out_file[0] = 0;
    opts->use_dsp_output = 0;

    //Use P25p1 heuristics
    opts->use_heuristics = 0;

} //initopts

void
initState(dsd_state *state) {

    int i, j;
    state->sample_count = 0;
    state->last_dibit = 0;
    state->dibit_buf = malloc(sizeof(int) * 1000000);
    state->dibit_buf_p = state->dibit_buf + 200;
    memset(state->dibit_buf, 0, sizeof(int) * 200);
    //dmr buffer -- double check this set up
    state->dmr_payload_buf = malloc(sizeof(int) * 1000000);
    state->dmr_payload_p = state->dmr_payload_buf + 200;
    memset(state->dmr_payload_buf, 0, sizeof(int) * 200);
    //dmr buffer end
    state->repeat = 0;

    //Upsampled Audio Smoothing
    state->audio_smoothing = 0;

    state->audio_out_buf = malloc(sizeof(short) * 1000000);
    state->audio_out_bufR = malloc(sizeof(short) * 1000000);
    memset(state->audio_out_buf, 0, 100 * sizeof(short));
    memset(state->audio_out_bufR, 0, 100 * sizeof(short));
    state->audio_out_buf_p = state->audio_out_buf + 100;
    state->audio_out_buf_pR = state->audio_out_bufR + 100;
    state->audio_out_float_buf = malloc(sizeof(float) * 1000000);
    state->audio_out_float_bufR = malloc(sizeof(float) * 1000000);
    memset(state->audio_out_float_buf, 0, 100 * sizeof(float));
    memset(state->audio_out_float_bufR, 0, 100 * sizeof(float));
    state->audio_out_float_buf_p = state->audio_out_float_buf + 100;
    state->audio_out_float_buf_pR = state->audio_out_float_bufR + 100;
    state->audio_out_idx = 0;
    state->audio_out_idx2 = 0;
    state->audio_out_idxR = 0;
    state->audio_out_idx2R = 0;
    state->audio_out_temp_buf_p = state->audio_out_temp_buf;
    for (int k = 0; k < 960; ++k) {
        state->audio_out[k] = 0;
    }
    state->audio_out_temp_buf_pR = state->audio_out_temp_bufR;
    //state->wav_out_bytes = 0;
    state->center = 0;
    state->jitter = -1;
    state->synctype = -1;
    state->min = -15000;
    state->max = 15000;
    state->lmid = 0;
    state->umid = 0;
    state->minref = -12000;
    state->maxref = 12000;
    state->lastsample = 0;
    for (i = 0; i < 128; i++) {
        state->sbuf[i] = 0;
    }
    state->sidx = 0;
    for (i = 0; i < 1024; i++) {
        state->maxbuf[i] = 15000;
    }
    for (i = 0; i < 1024; i++) {
        state->minbuf[i] = -15000;
    }
    state->midx = 0;
    state->err_str[0] = 0;
    state->err_strR[0] = 0;
    state->symbolcnt = 0;
    state->symbolc = 0; //
    state->rf_mod = 0;
    state->numflips = 0;
    state->lastsynctype = -1;
    state->lastp25type = 0;
    state->offset = 0;
    state->carrier = 0;
    for (i = 0; i < SIZE_OF_STORE; i++) {
        for (j = 0; j < 49; j++) {
            state->ambe_d[i][j] = 0;
        }
    }
    for (int k = 0; k < 5000; ++k) {
        state->key_buff[k] = 0;
    }
    state->key_buff_count = 0;
    state->b0_arr_count = 0;
//    for (i = 0; i < SIZE_OF_BUFFER; i++) {
//            state->key_buff_struct[i].zero_num = 0;
//            state->key_buff_struct[i].key = 0;
//    }
    for (int k = 0; k < SIZE_OF_BUFFER; ++k) {
        state->voice_buff[k] = 0;
    }
    state->voice_buff_counter = 0;
    for (i = 0; i < 25; i++) {
        for (j = 0; j < 16; j++) {
            state->tg[i][j] = 48;
        }
    }
    state->tgcount = 0;
    state->lasttg = 0;
    state->lastsrc = 0;
    state->lasttgR = 0;
    state->lastsrcR = 0;
    state->nac = 0;
    state->errs = 0;
    state->errs2 = 0;
    state->mbe_file_type = -1;
    state->optind = 0;
    state->numtdulc = 0;
    state->firstframe = 0;
    state->aout_gain = 0;
    state->aout_gainR = 0;
    memset(state->aout_max_buf, 0, sizeof(float) * 200);
    state->aout_max_buf_p = state->aout_max_buf;
    state->aout_max_buf_idx = 0;

    memset(state->aout_max_bufR, 0, sizeof(float) * 200);
    state->aout_max_buf_pR = state->aout_max_bufR;
    state->aout_max_buf_idxR = 0;

    state->samplesPerSymbol = 10;
    state->symbolCenter = 4;
    sprintf(state->algid, "________");
    sprintf(state->keyid, "________________");
    state->currentslot = 0;
    state->cur_mp = malloc(sizeof(mbe_parms));
    for (int k = 0; k < SIZE_OF_STORE; ++k) {
        state->cur_mp_store[k] = malloc(sizeof(mbe_parms));
    }
    state->prev_mp = malloc(sizeof(mbe_parms));
    for (int k = 0; k < SIZE_OF_STORE; ++k) {
        state->prev_mp_store[k] = malloc(sizeof(mbe_parms));
    }
    state->prev_mp_enhanced = malloc(sizeof(mbe_parms));
    state->cur_mp2 = malloc(sizeof(mbe_parms));
    for (int k = 0; k < SIZE_OF_STORE; ++k) {
        state->cur_mp2_store[k] = malloc(sizeof(mbe_parms));
    }
    for (int k = 0; k < SIZE_OF_STORE; ++k) {
        state->DMRvcL_p[k] = 0;
    }
    for (int k = 0; k < SIZE_OF_STORE; ++k) {
        state->payload_mi_p[k] = 0;
    }
    for (int k = 0; k < SIZE_OF_STORE; ++k) {
        state->dropL_p[k] = 0;
    }
    state->prev_mp2 = malloc(sizeof(mbe_parms));
    for (int k = 0; k < SIZE_OF_STORE; ++k) {
        state->prev_mp2_store[k] = malloc(sizeof(mbe_parms));
    }
    state->prev_mp_enhanced2 = malloc(sizeof(mbe_parms));

    mbe_initMbeParms(state->cur_mp, state->prev_mp, state->prev_mp_enhanced);
    mbe_initMbeParms(state->cur_mp2, state->prev_mp2, state->prev_mp_enhanced2);
    state->p25kid = 0;

    state->debug_audio_errors = 0;
    state->debug_audio_errorsR = 0;
    state->debug_header_errors = 0;
    state->debug_header_critical_errors = 0;

    state->nxdn_last_ran = -1;
    state->nxdn_last_rid = 0;
    state->nxdn_last_tg = 0;
    state->nxdn_cipher_type = 0;
    state->nxdn_key = 0;
    state->payload_miN = 0;

    state->dpmr_color_code = -1;

    state->payload_mi = 0;
    state->payload_miR = 0;
    state->payload_mfid = 0;
    state->payload_mfidR = 0;
    state->payload_algid = 0;
    state->payload_algidR = 0;
    state->payload_keyid = 0;
    state->payload_keyidR = 0;

    //init P2 ESS_B fragments and 4V counter
    for (short i = 0; i < 4; i++) {
        state->ess_b[0][i] = 0;
        state->ess_b[1][i] = 0;
    }
    state->fourv_counter[0] = 0;
    state->fourv_counter[1] = 0;

    state->K = 0;
    state->R = 0;
    state->RR = 0;
    state->H = 0;
    state->K1 = 0;
    state->K2 = 0;
    state->K3 = 0;
    state->K4 = 0;
    state->M = 0; //force key priority over settings from fid/so

    state->dmr_stereo = 0; //1, or 0?
    state->dmrburstL = 17; //initialize at higher value than possible
    state->dmrburstR = 17; //17 in char array is set for ERR
    state->dmr_so = 0;
    state->dmr_soR = 0;
    state->dmr_fid = 0;
    state->dmr_fidR = 0;
    state->dmr_flco = 0;
    state->dmr_flcoR = 0;
    state->dmr_ms_mode = 0;

    state->HYTL = 0;
    state->HYTR = 0;
    state->DMRvcL = 0;
    state->DMRvcR = 0;
    state->dropL = 256;
    state->dropR = 256;

    state->p25vc = 0;

    state->payload_miP = 0;

    memset(state->dstarradioheader, 0, 41);

    //initialize dmr data header source
    state->dmr_lrrp_source[0] = 0;
    state->dmr_lrrp_source[1] = 0;


    //initialize data header bits
    state->data_header_blocks[0] = 1;  //initialize with 1, otherwise we may end up segfaulting when no/bad data header
    state->data_header_blocks[1] = 1; //when trying to fill the superframe and 0-1 blocks give us an overflow
    state->data_header_padding[0] = 0;
    state->data_header_padding[1] = 0;
    state->data_header_format[0] = 7;
    state->data_header_format[1] = 7;
    state->data_header_sap[0] = 0;
    state->data_header_sap[1] = 0;
    state->data_block_counter[0] = 1;
    state->data_block_counter[1] = 1;
    state->data_p_head[0] = 0;
    state->data_p_head[1] = 0;

    state->menuopen = 0; //is the ncurses menu open, if so, don't process frame sync

    state->dmr_encL = 0;
    state->dmr_encR = 0;

    //P2 variables
    state->p2_wacn = 0;
    state->p2_sysid = 0;
    state->p2_cc = 0;
    state->p2_siteid = 0;
    state->p2_rfssid = 0;
    state->p2_hardset = 0;
    state->p2_is_lcch = 0;
    state->p25_cc_is_tdma = 2; //init on 2, TSBK NET_STS will set 0, TDMA NET_STS will set 1. //used to determine if we need to change symbol rate when cc hunting

    //experimental symbol file capture read throttle
    state->symbol_throttle = 100; //throttle speed
    state->use_throttle = 0; //only use throttle if set to 1

    state->p2_scramble_offset = 0;
    state->p2_vch_chan_num = 0;

    //p25 iden_up values
    state->p25_chan_iden = 0;
    for (int i = 0; i < 16; i++) {
        state->p25_chan_type[i] = 0;
        state->p25_trans_off[i] = 0;
        state->p25_chan_spac[i] = 0;
        state->p25_base_freq[i] = 0;
    }

    //values displayed in ncurses terminal
    state->p25_cc_freq = 0;
    state->p25_vc_freq[0] = 0;
    state->p25_vc_freq[1] = 0;

    //edacs - may need to make these user configurable instead for stability on non-ea systems
    state->ea_mode = -1; //init on -1, 0 is standard, 1 is ea
    state->esk_mode = -1; //same as above, but with esk or not
    state->esk_mask = 0x0; //toggles from 0x0 to 0xA0 if esk mode enabled
    state->edacs_site_id = 0;
    state->edacs_lcn_count = 0;
    state->edacs_cc_lcn = 0;
    state->edacs_vc_lcn = 0;
    state->edacs_tuned_lcn = -1;

    //trunking
    memset(state->trunk_lcn_freq, 0, sizeof(state->trunk_lcn_freq));
    memset(state->trunk_chan_map, 0, sizeof(state->trunk_chan_map));
    state->group_tally = 0;
    state->lcn_freq_count = 0; //number of frequncies imported as an enumerated lcn list
    state->lcn_freq_roll = 0; //needs reset if sync is found?
    state->last_cc_sync_time = time(NULL);
    state->last_vc_sync_time = time(NULL);
    state->last_active_time = time(NULL);
    state->is_con_plus = 0;

    //dmr trunking/ncurses stuff
    state->dmr_rest_channel = -1; //init on -1
    state->dmr_mfid = -1; //

    //new nxdn stuff
    state->nxdn_part_of_frame = 0;
    state->nxdn_ran = 0;
    state->nxdn_sf = 0;
    memset(state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc)); //init on 1, bad CRC all
    state->nxdn_sacch_non_superframe = TRUE;
    memset(state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
    state->nxdn_alias_block_number = 0;
    memset(state->nxdn_alias_block_segment, 0, sizeof(state->nxdn_alias_block_segment));

    //site/srv/cch info
    state->nxdn_location_site_code = 0;
    state->nxdn_location_sys_code = 0;

    //channel access information
    state->nxdn_rcn = 0;
    state->nxdn_base_freq = 0;
    state->nxdn_step = 0;
    state->nxdn_bw = 0;

    //multi-key array
    memset(state->rkey_array, 0, sizeof(state->rkey_array));
    state->keyloader = 0; //keyloader off

    //Remus DMR End Call Alert Beep
    state->dmr_end_alert[0] = 0;
    state->dmr_end_alert[1] = 0;

    //initialize unified dmr pdu 'superframe'
    memset(state->dmr_pdu_sf, 0, sizeof(state->dmr_pdu_sf));
    memset(state->data_header_valid, 0, sizeof(state->data_header_valid));

    //initialize cap+ bits and block num storage
    memset(state->cap_plus_csbk_bits, 0, sizeof(state->cap_plus_csbk_bits));
    memset(state->cap_plus_block_num, 0, sizeof(state->cap_plus_block_num));

    //init confirmed data individual block crc as invalid
    memset(state->data_block_crc_valid, 0, sizeof(state->data_block_crc_valid));

    //dmr slco stuff
    memset(state->dmr_cach_fragment, 1, sizeof(state->dmr_cach_fragment));
    state->dmr_cach_counter = 0;

    //embedded signalling
    memset(state->dmr_embedded_signalling, 0, sizeof(state->dmr_embedded_signalling));

    //dmr talker alias new/fixed stuff
    memset(state->dmr_alias_format, 0, sizeof(state->dmr_alias_format));
    memset(state->dmr_alias_len, 0, sizeof(state->dmr_alias_len));
    memset(state->dmr_alias_block_segment, 0, sizeof(state->dmr_alias_block_segment));
    memset(state->dmr_embedded_gps, 0, sizeof(state->dmr_embedded_gps));
    memset(state->dmr_lrrp_gps, 0, sizeof(state->dmr_lrrp_gps));
    memset(state->active_channel, 0, sizeof(state->active_channel));

    memset(state->late_entry_mi_fragment, 0, sizeof(state->late_entry_mi_fragment));

    state->dPMRVoiceFS2Frame.CalledIDOk = 0;
    state->dPMRVoiceFS2Frame.CallingIDOk = 0;
    memset(state->dPMRVoiceFS2Frame.CalledID, 0, 8);
    memset(state->dPMRVoiceFS2Frame.CallingID, 0, 8);
    memset(state->dPMRVoiceFS2Frame.Version, 0, 8);
} //init_state

void
usage() {

    printf("\n");
    printf("Input/Output options:\n");
    printf("  -i <device>   Audio input device (default is pulse)\n");
    printf("                filename.wav\n");
    printf("                (Use single quotes '/directory/audio file.wav' when directories/spaces are present)\n");
    printf("Decoder options:\n");
    printf("  -fs           DMR Stereo BS and MS Simplex\n");
    printf("\n");
    printf("  -mg           Use only GFSK modulation optimizations\n");
    printf("\n");
    printf("  U <number>   Division number, default = 1\n");
    printf("  t <number>   Last keys number to print, default = 20\n");
    printf("  s <number>   From what frame to start, default = 100\n");
    printf("  A <number>   Frames number to process, default = 150\n");
    printf("\n");
    exit(0);
}

void
liveScanner(dsd_opts *opts, dsd_state *state) {

//    char *file_name = "coef_file.txt";
//    opts->coef_file = fopen(file_name, "a");

//    if (opts->audio_out_type == 0) {
    // openPulseInput(opts); //test to see if we still randomly hang up in ncurses and tcp input if we open this and leave it opened
//        openPulseOutput(opts);
//    }

    while (1) {

        noCarrier(opts, state);
        if (state->menuopen == 0) {
            state->synctype = getFrameSync(opts, state);
            // recalibrate center/umid/lmid
            state->center = ((state->max) + (state->min)) / 2;
            state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
            state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
        }


        while (state->synctype != -1) {
            processFrame(opts, state);

            // state->synctype = getFrameSync (opts, state);

            // // recalibrate center/umid/lmid
            // state->center = ((state->max) + (state->min)) / 2;
            // state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
            // state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
            if (state->menuopen == 0) {
                state->synctype = getFrameSync(opts, state);
                // recalibrate center/umid/lmid
                state->center = ((state->max) + (state->min)) / 2;
                state->umid = (((state->max) - state->center) * 5 / 8) + state->center;
                state->lmid = (((state->min) - state->center) * 5 / 8) + state->center;
            }
        }
    }
}

void cleanupAndExit(dsd_opts *opts, dsd_state *state) {
    noCarrier(opts, state);
    //cause of crash on exit, need to check if NULL first, may need to set NULL when turning off in nterm

//    fprintf(stderr, "\n");
//    fprintf(stderr, "Total audio errors: %i\n", state->debug_audio_errors);
//    fprintf(stderr, "Total header errors: %i\n", state->debug_header_errors);
//    fprintf(stderr, "Total irrecoverable header errors: %i\n", state->debug_header_critical_errors);
//    fprintf(stderr, "Exiting.\n");
    exit(0);
}

double atofs(char *s) {
    char last;
    int len;
    double suff = 1.0;
    len = strlen(s);
    last = s[len - 1];
    s[len - 1] = '\0';
    switch (last) {
        case 'g':
        case 'G':
            suff *= 1e3;
        case 'm':
        case 'M':
            suff *= 1e3;
        case 'k':
        case 'K':
            suff *= 1e3;
            suff *= atof(s);
            s[len - 1] = last;
            return suff;
    }
    s[len - 1] = last;
    return atof(s);
}

int
main(int argc, char **argv) {
    int c;
    extern char *optarg;
    extern int optind, opterr, optopt;
    dsd_opts opts;
    dsd_state state;
    char versionstr[25];
    sprintf(versionstr, "%s", "0.10 ");

    struct stat st = {0};
    if (stat("iii", &st) == -1) {
        mkdir("iii", 0700);
    }

    fprintf(stderr, "Version: %s\n", versionstr);

    initOpts(&opts);
    initState(&state);

    InitAllFecFunction();

    while ((c = getopt(argc, argv,
                       "haepPqs:t:v:z:i:o:d:c:g:nw:B:C:R:f:m:u:x:A:S:M:G:D:L:VU:YK:b:H:X:NQ:WrlZTF01:2:345:6:7:89Ek:")) !=
           -1) {
        opterr = 0;
        switch (c) {
            case 'h':
                usage();
                exit(0);

            case 'a':
                opts.call_alert = 1;
                break;

                //Free'd up switches include: I, J, j, n, O, v, y
                //all numerals have reclaimed, except for 4 and 0,1 (rc4 enforcement and single key)

                //make sure to put a colon : after each if they need an argument
                //or remove colon if no argument required

                //NOTE: The 'K' option for single BP key has been swapped to 'b'
                //'K' is now used for hexidecimal key.csv imports

            case '9': //This is a temporary fix for RR issue until a permanent fix can be found
                state.ea_mode = 0;
                fprintf(stderr, "Force Enabling EDACS Standard/Networked Mode Mode\n");
                break;

                //rc4 enforcement on DMR (due to missing the PI header)
            case '0':
                state.M = 0x21;
                fprintf(stderr, "Force RC4 Key over Missing PI header/LE Encryption Identifiers (DMR)\n");
                break;

                //load single rc4 key
            case '1':
                sscanf(optarg, "%llX", &state.R);
                state.RR = state.R; //put key on both sides
                fprintf(stderr, "RC4 Encryption Key Value set to 0x%llX \n", state.R);
                opts.unmute_encrypted_p25 = 0;
                state.keyloader = 0; //turn off keyloader
                break;

            case '3':
                opts.dmr_le = 0;
                fprintf(stderr, "DMR Late Entry Encryption Identifiers Disabled (VC6 Single Burst)\n");
                break;

            case 'Y': //conventional scanner mode
                opts.scanner_mode = 1; //enable scanner
                opts.p25_trunk = 0; //turn off trunking mode if user enabled it
                break;

            case 'k': //multi-key loader (dec)
                break;

            case 'K': //multi-key loader (hex)
                break;

            case 'Q': //'DSP' Structured Output file for OKDMRlib
                sprintf(wav_file_directory, "./DSP");
                wav_file_directory[1023] = '\0';
                if (stat(wav_file_directory, &st) == -1) {
                    fprintf(stderr, "-Q %s DSP file directory does not exist\n", wav_file_directory);
                    fprintf(stderr, "Creating directory %s to save DSP Structured files\n", wav_file_directory);
                    mkdir(wav_file_directory,
                          0700); //user read write execute, needs execute for some reason or segfault
                }
                //read in filename
                //sprintf (opts.wav_out_file, "./WAV/DSD-FME-X1.wav");
                strncpy(dsp_filename, optarg, 1023);
                sprintf(opts.dsp_out_file, "%s/%s", wav_file_directory, dsp_filename);
                fprintf(stderr, "Saving DSP Structured files to %s\n", opts.dsp_out_file);
                opts.use_dsp_output = 1;
                break;

            case 'z':
                sscanf(optarg, "%d", &opts.slot_preference);
                if (opts.slot_preference > 0) opts.slot_preference--; //user inputs 1 or 2, internally we want 0 and 1
                if (opts.slot_preference > 1) opts.slot_preference = 1;
                fprintf(stderr, "TDMA (DMR and P2) Slot Voice Preference is Slot %d. \n", opts.slot_preference + 1);
                break;

                //Enable Audio Smoothing for Upsampled Audio
            case 'V':
                state.audio_smoothing = 1;
                break;

                //Trunking - Use Group List as Allow List
            case 'W':
                opts.trunk_use_allow_list = 1;
                fprintf(stderr, "Using Group List as Allow/White List. \n");
                break;

                //Trunking - Tune Group Calls
            case 'E':
                opts.trunk_tune_group_calls = 0; //disable
                fprintf(stderr, "Disable Tuning to Group Calls. \n");
                break;

                //Trunking - Tune Private Calls
            case 'p':
                opts.trunk_tune_private_calls = 0; //disable
                fprintf(stderr, "Disable Tuning to Private Calls. \n");
                break;

                //Trunking - Tune Data Calls
            case 'e':
                opts.trunk_tune_data_calls = 1; //enable
                fprintf(stderr, "Enable Tuning to Data Calls. \n");
                break;

            case 'D': //user set DMRLA n value
                sscanf(optarg, "%c", &opts.dmr_dmrla_n);
                if (opts.dmr_dmrla_n > 10) opts.dmr_dmrla_n = 10; //max out at 10;
                // if (opts.dmr_dmrla_n != 0) opts.dmr_dmrla_is_set = 1; //zero will fix a capmax site id value...I think
                opts.dmr_dmrla_is_set = 1;
                fprintf(stderr, "DMRLA n value set to %d. \n", opts.dmr_dmrla_n);
                break;

            case 'G': //new letter assignment for group import, flow down to allow temp numbers
                break;

            case 'q': //New letter assignment for Reverse Mute, flow down to allow temp numbers
                opts.reverse_mute = 1;
                fprintf(stderr, "Reverse Mute\n");
                break;

            case 'B': //New letter assignment for RIGCTL SetMod BW, flow down to allow temp numbers
                sscanf(optarg, "%d", &opts.setmod_bw);
                if (opts.setmod_bw > 25000) opts.setmod_bw = 25000; //not too high
                break;

//            case 's':
//                sscanf(optarg, "%d", &opts.wav_sample_rate);
//                opts.wav_interpolator = opts.wav_sample_rate / opts.wav_decimator;
//                state.samplesPerSymbol = state.samplesPerSymbol * opts.wav_interpolator;
//                state.symbolCenter = state.symbolCenter * opts.wav_interpolator;
//                break;

                // case 'v':
                //   sscanf (optarg, "%d", &opts.verbose);
                //   break;

                // case 'n': //disable or reclaim?
                //   state.use_throttle = 1;
                //   break;

            case 'b': //formerly Capital 'K'
                sscanf(optarg, "%lld", &state.K);
                if (state.K > 256) {
                    state.K = 256;
                }
                break;

            case 'R':
                sscanf(optarg, "%lld", &state.R);
                if (state.R > 0x7FFF) state.R = 0x7FFF;
                //disable keyloader in case user tries to use this and it at the same time
                state.keyloader = 0;
                break;

            case 'H':
                //new handling for 10/32/64 Char Key

                strncpy(opts.szNumbers, optarg, 1023);
                opts.szNumbers[1023] = '\0';
                state.K1 = strtoull(opts.szNumbers, &pEnd, 16);
                state.K2 = strtoull(pEnd, &pEnd, 16);
                state.K3 = strtoull(pEnd, &pEnd, 16);
                state.K4 = strtoull(pEnd, &pEnd, 16);
                fprintf(stderr, "**tera Key = %016llX\n", state.K1);
                state.H = state.K1; //shim still required?
                break;

            case '4':
                state.M = 1;
                fprintf(stderr, "Force Privacy Key over Encryption Identifiers (DMR BP and NXDN Scrambler) \n");
                break;

                //manually set Phase 2 TDMA WACN/SYSID/CC
            case 'X':
                sscanf(optarg, "%llX", &p2vars);
                if (p2vars > 0) {
                    state.p2_wacn = p2vars >> 24;
                    state.p2_sysid = (p2vars >> 12) & 0xFFF;
                    state.p2_cc = p2vars & 0xFFF;
                }
                if (state.p2_wacn != 0 && state.p2_sysid != 0 && state.p2_cc != 0) {
                    state.p2_hardset = 1;
                }
                break;

            case 'N':
                opts.use_ncurses_terminal = 1;
                fprintf(stderr, "Enabling NCurses Terminal.\n");
                break;

            case 'Z':
                opts.payload = 1;
                fprintf(stderr, "Logging Frame Payload to console\n");
                break;

            case 'L': //LRRP output to file
                strncpy(opts.lrrp_out_file, optarg, 1023);
                opts.lrrp_out_file[1023] = '\0';
                opts.lrrp_file_output = 1;
                fprintf(stderr, "Writing + Appending LRRP data to file %s\n", opts.lrrp_out_file);
                break;

            case 'P': //TDMA/NXDN Per Call - was T, now is P
                sprintf(wav_file_directory, "./WAV");
                wav_file_directory[1023] = '\0';
                if (stat(wav_file_directory, &st) == -1) {
                    fprintf(stderr, "-P %s WAV file directory does not exist\n", wav_file_directory);
                    fprintf(stderr, "Creating directory %s to save decoded wav files\n", wav_file_directory);
                    mkdir(wav_file_directory,
                          0700); //user read write execute, needs execute for some reason or segfault
                }
                fprintf(stderr, "XDMA and NXDN Per Call Wav File Saving Enabled. (NCurses Terminal Only)\n");
                sprintf(opts.wav_out_file, "./WAV/DSD-FME-X1.wav");
                sprintf(opts.wav_out_fileR, "./WAV/DSD-FME-X2.wav");
                opts.dmr_stereo_wav = 1;
                break;

            case 'F':
                opts.aggressive_framesync = 0;
                fprintf(stderr, "%s", KYEL);
                //fprintf (stderr,"DMR Stereo Aggressive Resync Disabled!\n");
                fprintf(stderr, "Relax P25 Phase 2 MAC_SIGNAL CRC Checksum Pass/Fail\n");
                fprintf(stderr, "Relax DMR RAS/CRC CSBK/DATA Pass/Fail\n");
                fprintf(stderr, "Relax NXDN SACCH/FACCH/CAC/F2U CRC Pass/Fail\n");
                fprintf(stderr, "%s", KNRM);
                break;

            case 'i':
                strncpy(opts.audio_in_dev, optarg, 2047);
                opts.audio_in_dev[2047] = '\0';
                break;
            case 'o':
                strncpy(opts.audio_out_dev, optarg, 1023);
                opts.audio_out_dev[1023] = '\0';
                break;
            case 'd':
                strncpy(opts.mbe_out_dir, optarg, 1023);
                opts.mbe_out_dir[1023] = '\0';
                if (stat(opts.mbe_out_dir, &st) == -1) {
                    fprintf(stderr, "-d %s directory does not exist\n", opts.mbe_out_dir);
                    fprintf(stderr, "Creating directory %s to save MBE files\n", opts.mbe_out_dir);
                    mkdir(opts.mbe_out_dir, 0700); //user read write execute, needs execute for some reason or segfault
                } else fprintf(stderr, "Writing mbe data files to directory %s\n", opts.mbe_out_dir);
                // fprintf (stderr,"Writing mbe data temporarily disabled!\n");
                break;

            case 'c':
                strncpy(opts.symbol_out_file, optarg, 1023);
                opts.symbol_out_file[1023] = '\0';
                fprintf(stderr, "Writing + Appending symbol capture to file %s\n", opts.symbol_out_file);
                opts.symbol_out = 1;
                break;

            case 'g':
                sscanf(optarg, "%f", &opts.audio_gain);
                if (opts.audio_gain < (float) 0) {
                    fprintf(stderr, "Disabling audio out gain setting\n");
                    opts.audio_gainR = opts.audio_gain;
                } else if (opts.audio_gain == (float) 0) {
                    opts.audio_gain = (float) 0;
                    opts.audio_gainR = opts.audio_gain;
                    fprintf(stderr, "Enabling audio out auto-gain\n");
                } else {
                    fprintf(stderr, "Setting audio out gain to %f\n", opts.audio_gain);
                    opts.audio_gainR = opts.audio_gain;
                    state.aout_gain = opts.audio_gain;
                    state.aout_gainR = opts.audio_gain;
                }
                break;

            case 'w':
                strncpy(opts.wav_out_file, optarg, 1023);
                opts.wav_out_file[1023] = '\0';
                fprintf(stderr, "Writing + Appending decoded audio to file %s\n", opts.wav_out_file);
                opts.dmr_stereo_wav = 0;
                if (access(opts.wav_out_file, F_OK) == 0) {
                    remove(opts.wav_out_file);
                }

                //openWavOutFile(&opts, &state);
                break;

            case 'f':
                if (optarg[0] == 'a') {
                    opts.frame_dstar = 1;
                    opts.frame_x2tdma = 1;
                    opts.frame_p25p1 = 1;
                    opts.frame_p25p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 1;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 0;
                    opts.pulse_digi_rate_out = 48000; // /dev/dsp does need this set to 48000 to properly process the upsampling
                    opts.pulse_digi_out_channels = 1;
                    opts.dmr_stereo = 1; //switching in 'stereo' for 'mono'
                    opts.dmr_mono = 0;
                    state.dmr_stereo = 0;
                    //opts.setmod_bw = 7000;
                    sprintf(opts.output_name, "Legacy Auto");
                } else if (optarg[0] == 'd') {
                    opts.frame_dstar = 1;
                    opts.frame_x2tdma = 0;
                    opts.frame_p25p1 = 0;
                    opts.frame_p25p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 0;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 0;
                    opts.pulse_digi_rate_out = 48000;
                    opts.pulse_digi_out_channels = 1;
                    opts.dmr_stereo = 0;
                    state.dmr_stereo = 0;
                    opts.dmr_mono = 0;
                    state.rf_mod = 0;
                    sprintf(opts.output_name, "D-STAR");
                    fprintf(stderr, "Decoding only D-STAR frames.\n");
                } else if (optarg[0] == 'x') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 1;
                    opts.frame_p25p1 = 0;
                    opts.frame_p25p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 0;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 0;
                    opts.pulse_digi_rate_out = 48000;
                    opts.pulse_digi_out_channels = 1;
                    opts.dmr_stereo = 0;
                    opts.dmr_mono = 0;
                    state.dmr_stereo = 0;
                    sprintf(opts.output_name, "X2-TDMA");
                    fprintf(stderr, "Decoding only X2-TDMA frames.\n");
                } else if (optarg[0] == 'p') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 0;
                    opts.frame_p25p1 = 0;
                    opts.frame_p25p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 0;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 1;
                    opts.frame_ysf = 0;
                    state.samplesPerSymbol = 5;
                    state.symbolCenter = 2;
                    opts.mod_c4fm = 0;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 1;
                    state.rf_mod = 2;
                    opts.pulse_digi_rate_out = 48000;
                    opts.pulse_digi_out_channels = 1;
                    opts.dmr_stereo = 0;
                    opts.dmr_mono = 0;
                    state.dmr_stereo = 0;
                    // opts.setmod_bw = 12500;
                    sprintf(opts.output_name, "EDACS/PV");
                    fprintf(stderr, "Setting symbol rate to 9600 / second\n");
                    fprintf(stderr, "Decoding only ProVoice frames.\n");
                    //rtl specific tweaks
                    opts.rtl_bandwidth = 24;
                    // opts.rtl_gain_value = 36;
                } else if (optarg[0] == '1') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 0;
                    opts.frame_p25p1 = 1;
                    opts.frame_p25p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 0;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 0;
                    opts.dmr_stereo = 0;
                    state.dmr_stereo = 0;
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 0; //
                    opts.dmr_stereo = 0;
                    opts.dmr_mono = 0;
                    opts.pulse_digi_rate_out = 48000;
                    opts.pulse_digi_out_channels = 1;
                    // opts.setmod_bw = 12000;
                    opts.ssize = 36; //128 current default, fall back to old default on P1 only systems
                    opts.msize = 15; //1024 current default, fall back to old default on P1 only systems
                    opts.use_heuristics = 1; //enable for Phase 1 only
                    sprintf(opts.output_name, "P25p1");
                    fprintf(stderr, "Decoding only P25 Phase 1 frames.\n");
                } else if (optarg[0] == 'i') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 0;
                    opts.frame_p25p1 = 0;
                    opts.frame_p25p2 = 0;
                    opts.frame_nxdn48 = 1;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 0;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 0;
                    state.samplesPerSymbol = 20;
                    state.symbolCenter = 10;
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 0;
                    opts.pulse_digi_rate_out = 48000;
                    opts.pulse_digi_out_channels = 1;
                    opts.dmr_stereo = 0;
                    state.dmr_stereo = 0;
                    opts.dmr_mono = 0;
                    // opts.setmod_bw = 4000; //causing issues
                    sprintf(opts.output_name, "NXDN48");
                    fprintf(stderr, "Setting symbol rate to 2400 / second\n");
                    fprintf(stderr, "Decoding only NXDN 4800 baud frames.\n");
                } else if (optarg[0] == 'y') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 0;
                    opts.frame_p25p1 = 0;
                    opts.frame_p25p2 = 0;
                    opts.frame_p25p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 0;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 1;
                    state.samplesPerSymbol = 20; //10
                    state.symbolCenter = 10;
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 0;
                    opts.pulse_digi_rate_out = 48000;
                    opts.pulse_digi_out_channels = 1;
                    opts.dmr_stereo = 0;
                    state.dmr_stereo = 0;
                    opts.dmr_mono = 0;
                    sprintf(opts.output_name, "YSF");
                    fprintf(stderr, "Setting symbol rate to 2400 / second\n");
                    fprintf(stderr, "Decoding only YSF frames.\nNot working yet!\n");
                } else if (optarg[0] == '2') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 0;
                    opts.frame_p25p1 = 0;
                    opts.frame_p25p2 = 1;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 0;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 0;
                    state.samplesPerSymbol = 8;
                    state.symbolCenter = 3;
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 0;
                    opts.dmr_stereo = 1;
                    state.dmr_stereo = 0;
                    opts.dmr_mono = 0;
                    // opts.setmod_bw = 12000;
                    sprintf(opts.output_name, "P25p2");
                    fprintf(stderr, "Decoding 6000 sps P25p2 frames only!\n");
                } else if (optarg[0] == 's') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 0;
                    opts.frame_p25p1 = 0;
                    opts.frame_p25p2 = 0;
                    opts.inverted_p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 1;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 0;
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 0;
                    opts.dmr_stereo = 1;
                    opts.dmr_mono = 0;
                    // opts.setmod_bw = 7000;
                    opts.pulse_digi_rate_out = 24000;
                    opts.pulse_digi_out_channels = 2;
                    sprintf(opts.output_name, "DMR Stereo");

                    fprintf(stderr, "Decoding DMR Stereo BS/MS Simplex\n");
                } else if (optarg[0] == 't') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 1;
                    opts.frame_p25p1 = 1;
                    opts.frame_p25p2 = 1;
                    opts.inverted_p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 1;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 0;
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0;
                    //Need a new demodulator is needed to handle p2 cqpsk
                    //or consider using an external modulator (GNURadio?)
                    state.rf_mod = 0;
                    opts.dmr_stereo = 1;
                    opts.dmr_mono = 0;
                    // opts.setmod_bw = 12000; //safe default on both DMR and P25
                    opts.pulse_digi_rate_out = 24000;
                    opts.pulse_digi_out_channels = 2;
                    sprintf(opts.output_name, "XDMA");
                    fprintf(stderr, "Decoding XDMA P25 and DMR\n");
                } else if (optarg[0] == 'n') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 0;
                    opts.frame_p25p1 = 0;
                    opts.frame_p25p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 1;
                    opts.frame_dmr = 0;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 0;
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 0;
                    opts.pulse_digi_rate_out = 48000;
                    opts.pulse_digi_out_channels = 1;
                    opts.dmr_stereo = 0;
                    opts.dmr_mono = 0;
                    state.dmr_stereo = 0;
                    // opts.setmod_bw = 7000; //causing issues
                    sprintf(opts.output_name, "NXDN96");
                    fprintf(stderr, "Decoding only NXDN 9600 baud frames.\n");
                } else if (optarg[0] == 'r') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 0;
                    opts.frame_p25p1 = 0;
                    opts.frame_p25p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 1;
                    opts.frame_dpmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_ysf = 0;
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0; //
                    state.rf_mod = 0;  //
                    opts.pulse_digi_rate_out = 24000;
                    opts.pulse_digi_out_channels = 2;
                    opts.dmr_mono = 0;
                    opts.dmr_stereo = 1;
                    state.dmr_stereo = 0; //0
                    // opts.setmod_bw = 7000;
                    sprintf(opts.output_name, "DMR Stereo");
                    fprintf(stderr, "-fr / DMR Mono switch has been deprecated.\n");
                    fprintf(stderr, "Decoding DMR Stereo BS/MS Simplex\n");

                } else if (optarg[0] == 'm') {
                    opts.frame_dstar = 0;
                    opts.frame_x2tdma = 0;
                    opts.frame_p25p1 = 0;
                    opts.frame_p25p2 = 0;
                    opts.frame_nxdn48 = 0;
                    opts.frame_nxdn96 = 0;
                    opts.frame_dmr = 0;
                    opts.frame_provoice = 0;
                    opts.frame_dpmr = 1;
                    opts.frame_ysf = 0; //testing with NXDN48 parameters
                    state.samplesPerSymbol = 20; //same as NXDN48 - 20
                    state.symbolCenter = 10; //same as NXDN48 - 10
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 0;
                    opts.pulse_digi_rate_out = 48000;
                    opts.pulse_digi_out_channels = 1;
                    opts.dmr_stereo = 0;
                    opts.dmr_mono = 0;
                    state.dmr_stereo = 0;
                    sprintf(opts.output_name, "dPMR");
                    fprintf(stderr,
                            "Notice: dPMR cannot autodetect polarity. \n Use -xd option if Inverted Signal expected.\n");
                    fprintf(stderr, "Decoding only dPMR frames.\n");
                }
                break;
                //don't mess with the modulations unless you really need to
            case 'm':
                if (optarg[0] == 'a') {
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 1;
                    opts.mod_gfsk = 1;
                    state.rf_mod = 0;
                    fprintf(stderr, "Don't use the -ma switch.\n");
                } else if (optarg[0] == 'c') {
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 0;
                    fprintf(stderr, "Enabling only C4FM modulation optimizations.\n");
                } else if (optarg[0] == 'g') {
                    opts.mod_c4fm = 0;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 1;
                    state.rf_mod = 2;
                    fprintf(stderr, "Enabling only GFSK modulation optimizations.\n");
                } else if (optarg[0] == 'q') {
                    opts.mod_c4fm = 0;
                    opts.mod_qpsk = 1;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 1;
                    // opts.setmod_bw = 12000;
                    fprintf(stderr, "Enabling only QPSK modulation optimizations.\n");
                } else if (optarg[0] == '2') {
                    opts.mod_c4fm = 0;
                    opts.mod_qpsk = 1;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 1;
                    state.samplesPerSymbol = 8;
                    state.symbolCenter = 3;
                    // opts.setmod_bw = 12000;
                    fprintf(stderr, "Enabling 6000 sps P25p2 QPSK.\n");
                }
                    //test
                else if (optarg[0] == '3') {
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 0;
                    opts.mod_gfsk = 0;
                    state.rf_mod = 0;
                    state.samplesPerSymbol = 10;
                    state.symbolCenter = 4;
                    // opts.setmod_bw = 12000;
                    fprintf(stderr, "Enabling 6000 sps P25p2 C4FM.\n");
                } else if (optarg[0] == '4') {
                    opts.mod_c4fm = 1;
                    opts.mod_qpsk = 1;
                    opts.mod_gfsk = 1;
                    state.rf_mod = 0;
                    state.samplesPerSymbol = 8;
                    state.symbolCenter = 3;
                    // opts.setmod_bw = 12000;
                    fprintf(stderr, "Enabling 6000 sps P25p2 all optimizations.\n");
                }
                break;
            case 'u':
                sscanf(optarg, "%i", &opts.uvquality);
                if (opts.uvquality < 1) {
                    opts.uvquality = 1;
                } else if (opts.uvquality > 64) {
                    opts.uvquality = 64;
                }
                fprintf(stderr, "Setting unvoice speech quality to %i waves per band.\n", opts.uvquality);
                break;
            case 'x':
                if (optarg[0] == 'x') {
                    opts.inverted_x2tdma = 0;
                    fprintf(stderr, "Expecting non-inverted X2-TDMA signals.\n");
                } else if (optarg[0] == 'r') {
                    opts.inverted_dmr = 1;
                    fprintf(stderr, "Expecting inverted DMR signals.\n");
                } else if (optarg[0] == 'd') {
                    opts.inverted_dpmr = 1;
                    fprintf(stderr, "Expecting inverted ICOM dPMR signals.\n");
                }
                break;

            case 'U':
                sscanf(optarg, "%d", &opts.mod_div);
                printf("mod division number: %d\n", opts.mod_div);
                break;
            case 't':
                sscanf(optarg, "%d", &opts.lastKeys);
                printf("last keys to print num: %d\n", opts.lastKeys);
                break;
            case 's':
                sscanf(optarg, "%d", &opts.startFrame);
                printf("start frame: %d\n", opts.startFrame);
                break;
            case 'A':
                sscanf(optarg, "%d", &opts.frameSize);
                printf("frame numbers: %d\n", opts.frameSize);
                break;

            case 'M':
                sscanf(optarg, "%i", &opts.msize);
                if (opts.msize > 1024) {
                    opts.msize = 1024;
                } else if (opts.msize < 1) {
                    opts.msize = 1;
                }
                fprintf(stderr, "Setting QPSK Min/Max buffer to %i\n", opts.msize);
                break;
            case 'r':
                break;
            case 'l':
                opts.use_cosine_filter = 0;
                break;
            default:
                usage();
                exit(0);
        }
    }


    int fmt;
    int speed = 48000;

    if ((strncmp(opts.audio_out_dev, "pulse", 5) == 0)) {
        opts.audio_out_type = 0;
    }

    opts.split = 1;
    opts.delay = 0;
    openAudioInDevice(&opts);
    liveScanner(&opts, &state);
    return (0);
}
