#ifndef DSD_H
#define DSD_H
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

//defined by CMakeLists.txt -- Disable by using cmake -DCOLORS=OFF ..
#ifdef PRETTY_COLORS
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#else
#define KNRM  ""
#define KRED  ""
#define KGRN  ""
#define KYEL  ""
#define KBLU  ""
#define KMAG  ""
#define KCYN  ""
#define KWHT  ""
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <math.h>
#include <sndfile.h>
#include <omp.h>
#include <locale.h>
#include <stdbool.h>
#define TRUE    1

#define SIZE_OF_BUFFER 1000000
extern volatile uint8_t exitflag; //fix for issue #136

//group csv import struct
typedef struct {
    unsigned long int groupNumber;
    char groupMode[8]; //char *?
    char groupName[50];
} groupinfo;


typedef struct {
    uint8_t F1;
    uint8_t F2;
    uint8_t MessageType;

    /****************************/
    /***** VCALL parameters *****/
    /****************************/
    uint8_t CCOption;
    uint8_t CallType;
    uint8_t VoiceCallOption;
    uint16_t SourceUnitID;
    uint16_t DestinationID;  /* May be a Group ID or a Unit ID */
    uint8_t CipherType;
    uint8_t KeyID;
    uint8_t VCallCrcIsGood;

    /*******************************/
    /***** VCALL_IV parameters *****/
    /*******************************/
    uint8_t IV[8];
    uint8_t VCallIvCrcIsGood;

    /*****************************/
    /***** Custom parameters *****/
    /*****************************/

    /* Specifies if the "CipherType" and the "KeyID" parameter are valid
     * 1 = Valid ; 0 = CRC error */
    uint8_t CipherParameterValidity;

    /* Used on DES and AES encrypted frames */
    uint8_t PartOfCurrentEncryptedFrame;  /* Could be 1 or 2 */
    uint8_t PartOfNextEncryptedFrame;     /* Could be 1 or 2 */
    uint8_t CurrentIVComputed[8];
    uint8_t NextIVComputed[8];
} NxdnElementsContent_t;

//Reed Solomon (12,9) constant
#define RS_12_9_DATASIZE        9
#define RS_12_9_CHECKSUMSIZE    3

//Reed Solomon (12,9) struct
typedef struct {
    uint8_t data[RS_12_9_DATASIZE + RS_12_9_CHECKSUMSIZE];
} rs_12_9_codeword_t;

// Maximum degree of various polynomials.
#define RS_12_9_POLY_MAXDEG (RS_12_9_CHECKSUMSIZE*2)

typedef struct {
    uint8_t data[RS_12_9_POLY_MAXDEG];
} rs_12_9_poly_t;

#define RS_12_9_CORRECT_ERRORS_RESULT_NO_ERRORS_FOUND           0
#define RS_12_9_CORRECT_ERRORS_RESULT_ERRORS_CORRECTED          1
#define RS_12_9_CORRECT_ERRORS_RESULT_ERRORS_CANT_BE_CORRECTED  2
typedef uint8_t rs_12_9_correct_errors_result_t;

typedef struct {
    uint8_t bytes[3];
} rs_12_9_checksum_t;

//dPMR
/* Could only be 2 or 4 */
#define NB_OF_DPMR_VOICE_FRAME_TO_DECODE 2

typedef struct {
    unsigned char RawVoiceBit[NB_OF_DPMR_VOICE_FRAME_TO_DECODE * 4][72];
    unsigned int errs1[NB_OF_DPMR_VOICE_FRAME_TO_DECODE *
                       4];  /* 8 x errors #1 computed when demodulate the AMBE voice bit of the frame */
    unsigned int errs2[NB_OF_DPMR_VOICE_FRAME_TO_DECODE *
                       4];  /* 8 x errors #2 computed when demodulate the AMBE voice bit of the frame */
    unsigned char AmbeBit[NB_OF_DPMR_VOICE_FRAME_TO_DECODE * 4][49];  /* 8 x 49 bit of AMBE voice of the frame */
    unsigned char CCHData[NB_OF_DPMR_VOICE_FRAME_TO_DECODE][48];
    unsigned int CCHDataHammingOk[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
    unsigned char CCHDataCRC[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
    unsigned int CCHDataCrcOk[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
    unsigned char CalledID[8];
    unsigned int CalledIDOk;
    unsigned char CallingID[8];
    unsigned int CallingIDOk;
    unsigned int FrameNumbering[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
    unsigned int CommunicationMode[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
    unsigned int Version[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
    unsigned int CommsFormat[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
    unsigned int EmergencyPriority[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
    unsigned int Reserved[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
    unsigned char SlowData[NB_OF_DPMR_VOICE_FRAME_TO_DECODE];
    unsigned int ColorCode[NB_OF_DPMR_VOICE_FRAME_TO_DECODE / 2];
} dPMRVoiceFS2Frame_t;
//
typedef struct {
    char wav_out_file[1024];
    int onesymbol;
    char mbe_in_file[1024];
    FILE *mbe_in_f;
    int errorbars;
    int datascope;
    int symboltiming;
    int verbose;
    int p25enc;
    int p25lc;
    int p25status;
    int p25tg;
    int scoperate;
    char audio_in_dev[2048]; //increase size for super long directory/file names
    int audio_in_fd;
    SNDFILE *audio_in_file;
    SF_INFO *audio_in_file_info;

    uint32_t rtlsdr_center_freq;
    int rtlsdr_ppm_error;
    int audio_in_type;
    char audio_out_dev[1024];
    int audio_out_fd;
    int audio_out_fdR; //right channel audio for OSS hack
    SNDFILE *audio_out_file;
    SF_INFO *audio_out_file_info;

    int audio_out_type; // 0 for device, 1 for file,
    int split;
//    int playoffset;
//    int playoffsetR;
    char mbe_out_dir[1024];
    char mbe_out_file[1024];
    char mbe_out_fileR[1024]; //second slot on a TDMA system
    char mbe_out_path[2048]; //1024
    FILE *mbe_out_f;
    FILE *mbe_out_fR; //second slot on a TDMA system
    FILE *symbol_out_f;
    float audio_gain;
    float audio_gainR;
    int audio_out;
    int dmr_stereo_wav;
    char wav_out_fileR[1024];
    char wav_out_file_raw[1024];
    char symbol_out_file[1024];
    char lrrp_out_file[1024];
    char szNumbers[1024]; //**tera 10/32/64 char str
    short int symbol_out;
    short int mbe_out; //flag for mbe out, don't attempt fclose more than once
    short int mbe_outR; //flag for mbe out, don't attempt fclose more than once
    SNDFILE *wav_out_f;
    SNDFILE *wav_out_fR;
    SNDFILE *wav_out_raw;
    //int wav_out_fd;
    int serial_baud;
    char serial_dev[1024];
    int serial_fd;
    int frame_dstar;
    int frame_x2tdma;
    int frame_p25p1;
    int frame_p25p2;
    int inverted_p2;
    int p2counter;
    int frame_nxdn48;
    int frame_nxdn96;
    int frame_dmr;
    int frame_provoice;
    int mod_c4fm;
    int mod_qpsk;
    int mod_gfsk;
    int uvquality;
    int inverted_x2tdma;
    int inverted_dmr;
    int mod_threshold;
    int ssize;
    int mod_div;
    int lastKeys;
    int startFrame;
    int frameSize;
    int msize;
    int delay;
    int use_cosine_filter;
    int unmute_encrypted_p25;
    int rtl_dev_index;
    int rtl_gain_value;
    int rtl_squelch_level;
    int rtl_volume_multiplier;
    int rtl_udp_port;
    int rtl_bandwidth;
    int rtl_started;
    int rtl_rms;
    int monitor_input_audio;
    int pulse_raw_rate_in;
    int pulse_raw_rate_out;
    int pulse_digi_rate_in;
    int pulse_digi_rate_out;
    int pulse_raw_in_channels;
    int pulse_raw_out_channels;
    int pulse_digi_in_channels;
    int pulse_digi_out_channels;
    int pulse_flush;
    int use_ncurses_terminal;
    int ncurses_compact;
    int ncurses_history;
    int reset_state;
    int payload;
    char output_name[1024];

    unsigned int dPMR_curr_frame_is_encrypted;
    int dPMR_next_part_of_superframe;
    int inverted_dpmr;
    int frame_dpmr;

    short int dmr_mono;
    short int dmr_stereo;
    short int lrrp_file_output;


    int frame_ysf;
    int inverted_ysf;
    short int aggressive_framesync;

    FILE *symbolfile;
    int call_alert;

    //rigctl opt
    int rigctl_sockfd;
    int use_rigctl;
    char rigctlhostname[1024];

    //udp socket for GQRX, SDR++, etc
    int udp_sockfd;
    int udp_portno;
    char *udp_hostname;
    SNDFILE *udp_file_in;

    //tcp socket for SDR++, etc
    int tcp_sockfd;
    int tcp_portno;
    char tcp_hostname[1024];
    SNDFILE *tcp_file_in;

    //wav file sample rate, interpolator and decimator
    int wav_sample_rate;
    int wav_interpolator;
    int wav_decimator;

    int p25_trunk; //experimental P25 trunking with RIGCTL (or RTLFM)
    int p25_is_tuned; //set to 1 if currently on VC, set back to 0 if on CC
    float trunk_hangtime; //hangtime in seconds before tuning back to CC

    int scanner_mode; //experimental -- use the channel map as a conventional scanner, quicker tuning, but no CC

    //csv import filenames
    char group_in_file[1024];
    char lcn_in_file[1024];
    char chan_in_file[1024];
    char key_in_file[1024];
    //end import filenames

    //reverse mute
    uint8_t reverse_mute;

    //setmod bandwidth
    int setmod_bw;

    //DMR Location Area - DMRLA B***S***
    uint8_t dmr_dmrla_is_set; //flag to tell us dmrla is set by the user
    uint8_t dmr_dmrla_n; //n value for dmrla

    //DMR Late Entry
    uint8_t dmr_le; //user option to turn on or turn off late entry for enc identifiers

    //Trunking - Use Group List as Allow List
    uint8_t trunk_use_allow_list;

    //Trunking - Tune Group Calls
    uint8_t trunk_tune_group_calls;

    //Trunking - Tune Private Calls
    uint8_t trunk_tune_private_calls;

    //Trunking - Tune Data Calls
    uint8_t trunk_tune_data_calls;

    //Trunking - Tune Enc Calls (P25 only on applicable grants with svc opts)
    uint8_t trunk_tune_enc_calls;

    //OSS audio - slot preference
    int slot_preference;

    //'DSP' Format Output
    uint8_t use_dsp_output;
    char dsp_out_file[2048];

    //Use P25p1 heuristics
    uint8_t use_heuristics;

} dsd_opts;

typedef struct {
    unsigned long long int key;
    int zero_num;
} key_state;

struct mbe_parameters
{
    float w0;
    int L;
    int K;
    int Vl[57];
    float Ml[57];
    float log2Ml[57];
    float PHIl[57];
    float PSIl[57];
    float gamma;
    int un;
    int repeat;
};

typedef struct mbe_parameters mbe_parms;

/*
 * Prototypes from ecc.c
 */
void mbe_checkGolayBlock (long int *block);
int mbe_golay2312 (char *in, char *out);
int mbe_hamming1511 (char *in, char *out);
int mbe_7100x4400hamming1511 (char *in, char *out);

/*
 * Prototypes from ambe3600x2450.c
 */
int mbe_eccAmbe3600x2450C0 (char ambe_fr[4][24]);
int mbe_eccAmbe3600x2450Data (char ambe_fr[4][24], char *ambe_d);
int mbe_decodeAmbe2450Parms (char *ambe_d, mbe_parms * cur_mp, mbe_parms * prev_mp);
void mbe_demodulateAmbe3600x2450Data (char ambe_fr[4][24]);
void mbe_processAmbe2450Dataf (float *aout_buf, int *errs, int *errs2, char *err_str, char ambe_d[49], mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, int uvquality);
void mbe_processAmbe2450Data (short *aout_buf, int *errs, int *errs2, char *err_str, char ambe_d[49], mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, int uvquality);
void mbe_processAmbe3600x2450Framef (float *aout_buf, int *errs, int *errs2, char *err_str, char ambe_fr[4][24], char ambe_d[49], mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, int uvquality);
void mbe_processAmbe3600x2450Frame (short *aout_buf, int *errs, int *errs2, char *err_str, char ambe_fr[4][24], char ambe_d[49], mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, int uvquality);

/*
 * Prototypes from mbelib.c
 */
void mbe_moveMbeParms (mbe_parms * cur_mp, mbe_parms * prev_mp);
void mbe_useLastMbeParms (mbe_parms * cur_mp, mbe_parms * prev_mp);
void mbe_initMbeParms (mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced);
void mbe_spectralAmpEnhance (mbe_parms * cur_mp);
void mbe_synthesizeSilencef (float *aout_buf);
void mbe_synthesizeSilence (short *aout_buf);
void mbe_synthesizeSpeechf (float *aout_buf, mbe_parms * cur_mp, mbe_parms * prev_mp, int uvquality);
void mbe_synthesizeSpeech (short *aout_buf, mbe_parms * cur_mp, mbe_parms * prev_mp, int uvquality);
void mbe_floattoshort (float *float_buf, short *aout_buf);

#define SIZE_OF_STORE 1000

typedef struct {
    short audio_out[960];
    int DMRvcL;
    int DMRvcL_p[SIZE_OF_STORE];
    int dropL_p[SIZE_OF_STORE];
//    key_state key_buff_struct[SIZE_OF_BUFFER];
    short voice_buff[SIZE_OF_BUFFER];
    int voice_buff_counter;
    mbe_parms *cur_mp;
    mbe_parms *prev_mp;
    mbe_parms *prev_mp_enhanced;
    mbe_parms *cur_mp2;
    mbe_parms *cur_mp2_store[SIZE_OF_STORE];
    mbe_parms *cur_mp_store[SIZE_OF_STORE];
    mbe_parms *prev_mp2;
    mbe_parms *prev_mp2_store[SIZE_OF_STORE];
    mbe_parms *prev_mp_store[SIZE_OF_STORE];
    mbe_parms *prev_mp_enhanced2;
    unsigned char ambe_d[SIZE_OF_STORE][49];
    unsigned long key_buff[5000];
    int key_buff_count;
    unsigned char b0_arr_count;

    int audio_count;
    int ambe_count;
    int errs;
    int errs2;
    int sample_count;
    int pos;
    int *dibit_buf;
    int *dibit_buf_p;
    int *dmr_payload_buf;
    int *dmr_payload_p;
    int repeat;
    short *audio_out_buf;
    short *audio_out_buf_p;
    short *audio_out_bufR;
    short *audio_out_buf_pR;
    float *audio_out_float_buf;
    float *audio_out_float_buf_p;
    float *audio_out_float_bufR;
    float *audio_out_float_buf_pR;
    float audio_out_temp_buf[160];
    float *audio_out_temp_buf_p;
    float audio_out_temp_bufR[160];
    float *audio_out_temp_buf_pR;
    int audio_out_idx;
    int audio_out_idx2;
    int audio_out_idxR;
    int audio_out_idx2R;
    int center;
    int jitter;
    int synctype;
    int min;
    int max;
    int lmid;
    int umid;
    int minref;
    int maxref;
    int lastsample;
    int sbuf[128];
    int sidx;
    int maxbuf[1024];
    int minbuf[1024];
    int midx;
    char err_str[64];
    char err_buf[64];
    char err_strR[64];
    char err_bufR[64];
    char fsubtype[16];
    char ftype[16];
    int symbolcnt;
    int symbolc;

    int rf_mod;
    int numflips;
    int lastsynctype;
    int lastp25type;
    int offset;
    int carrier;
    char tg[25][16];
    int tgcount;
    int lasttg;
    int lasttgR;
    int lastsrc;
    int lastsrcR;
    int nac;
    int errsR;
    int errs2R;
    int mbe_file_type;
    int optind;
    int numtdulc;
    int firstframe;
    char slot0light[8];
    float aout_gain;
    float aout_gainR;
    float aout_max_buf[200];
    float aout_max_bufR[200];
    float *aout_max_buf_p;
    float *aout_max_buf_pR;
    int aout_max_buf_idx;
    int aout_max_buf_idxR;
    int samplesPerSymbol;
    int symbolCenter;
    char algid[9];
    char keyid[17];
    int currentslot;
    int hardslot;
    int p25kid;
    int payload_algid;
    int payload_algidR;
    int payload_keyid;
    int payload_keyidR;
    int payload_mfid;
    int payload_mfidR;
    int payload_mi;
    int payload_mi_p[SIZE_OF_STORE];
    int payload_miR;
    unsigned long long int payload_miN;
    unsigned long long int payload_miP;
    int p25vc;
    unsigned long long int K;
    unsigned long long int K1;
    unsigned long long int K2;
    unsigned long long int K3;
    unsigned long long int K4;
    int M;
    int menuopen;

    unsigned int debug_audio_errors;
    unsigned int debug_audio_errorsR;
    unsigned int debug_header_errors;
    unsigned int debug_header_critical_errors;

    // Last dibit read
    int last_dibit;

    //input sample buffer for monitoring Input
    short input_sample_buffer; //HERE HERE
    short pulse_raw_out_buffer; //HERE HERE

    unsigned int dmr_color_code;
    unsigned int nxdn_last_ran;
    unsigned int nxdn_last_rid;
    unsigned int nxdn_last_tg;
    unsigned int nxdn_cipher_type;
    unsigned int nxdn_key;
    char nxdn_call_type[1024];

    unsigned long long int dmr_lrrp_source[2];

    NxdnElementsContent_t NxdnElementsContent;

    char ambe_ciphered[49];
    char ambe_deciphered[49];

    unsigned int color_code;
    unsigned int color_code_ok;
    unsigned int PI;
    unsigned int PI_ok;
    unsigned int LCSS;
    unsigned int LCSS_ok;

    unsigned int dmr_fid;
    unsigned int dmr_so;
    unsigned int dmr_flco;

    unsigned int dmr_fidR;
    unsigned int dmr_soR;
    unsigned int dmr_flcoR;

    char slot1light[8];
    char slot2light[8];
    int directmode;

    int dmr_stereo_payload[144];    //load up 144 dibit buffer for every single DMR TDMA frame
    int data_header_blocks[2];      //collect number of blocks to follow from data header per slot
    int data_block_counter[2];      //counter for number of data blocks collected
    uint8_t data_header_valid[2];   //flag for verifying the data header if still valid (in case of tact/burst fec errs)
    uint8_t data_header_padding[2]; //collect number of padding octets in last block per slot
    uint8_t data_header_format[2];  //collect format of data header (conf or unconf) per slot
    uint8_t data_header_sap[2];     //collect sap info per slot
    uint8_t data_p_head[2];         //flag for dmr proprietary header to follow

    //new stuff below here
    uint8_t data_conf_data[2];            //flag for confirmed data blocks per slot
    uint8_t dmr_pdu_sf[2][288];          //unified pdu 'superframe' //[slot][byte]
    uint8_t cap_plus_csbk_bits[2][12 * 8 * 8]; //CSBK Cap+ FL initial and appended block bit storage, by slot
    uint8_t cap_plus_block_num[2];         //received block number storage -- per timeslot
    uint8_t data_block_crc_valid[2][25]; //flag each individual block as good crc on confirmed data
    char dmr_embedded_signalling[2][7][48]; //embedded signalling 2 slots by 6 vc by 48 bits (replacing TS1SuperFrame.TimeSlotRawVoiceFrame.Sync structure)

    char dmr_cach_fragment[4][17]; //unsure of size, will need to check/verify
    int dmr_cach_counter; //counter for dmr_cach_fragments 0-3; not sure if needed yet.

    //dmr talker alias new/fixed stuff
    uint8_t dmr_alias_format[2]; //per slot
    uint8_t dmr_alias_len[2]; //per slot
    char dmr_alias_block_segment[2][4][7][16]; //2 slots, by 4 blocks, by up to 7 alias bytes that are up to 16-bit chars
    char dmr_embedded_gps[2][200]; //2 slots by 99 char string for string embedded gps
    char dmr_lrrp_gps[2][200]; //2 slots by 99 char string for string lrrp gps
    char dmr_site_parms[200]; //string for site/net info depending on type of DMR system (TIII or Con+)
    char call_string[2][200]; //string for call information
    char active_channel[31][200]; //string for storing and displaying active trunking channels


    dPMRVoiceFS2Frame_t dPMRVoiceFS2Frame;

    char dpmr_caller_id[20];
    char dpmr_target_id[20];

    int dpmr_color_code;

    short int dmr_stereo; //need state variable for upsample function
    short int dmr_ms_rc;
    short int dmr_ms_mode;
    unsigned int dmrburstL;
    unsigned int dmrburstR;
    int dropL;
    int dropR;
    unsigned long long int R;
    unsigned long long int RR;
    unsigned long long int H;
    unsigned long long int HYTL;
    unsigned long long int HYTR;
    int DMRvcR;

    short int dmr_encL;
    short int dmr_encR;

    //P2 variables
    unsigned long long int p2_wacn;
    unsigned long long int p2_sysid;
    unsigned long long int p2_cc;    //p1 NAC
    unsigned long long int p2_siteid;
    unsigned long long int p2_rfssid;
    int p2_hardset; //flag for checking whether or not P2 wacn and sysid are hard set by user
    int p2_scramble_offset; //offset counter for scrambling application
    int p2_vch_chan_num; //vch channel number (0 or 1, not the 0-11 TS)
    int ess_b[2][96]; //external storage for ESS_B fragments
    int fourv_counter[2]; //external reference counter for ESS_B fragment collection
    int p2_is_lcch; //flag to tell us when a frame is lcch and not sacch

    //iden freq storage for frequency calculations
    int p25_chan_tdma[16]; //set from iden_up vs iden_up_tdma
    int p25_chan_iden;
    int p25_chan_type[16];
    int p25_trans_off[16];
    int p25_chan_spac[16];
    long int p25_base_freq[16];

    //p25 frequency storage for trunking and display in ncurses
    long int p25_cc_freq;     //cc freq from net_stat
    long int p25_vc_freq[2]; //vc freq from voice grant updates, etc
    int p25_cc_is_tdma; //flag to tell us that the P25 control channel is TDMA so we can change symbol rate when required


    //experimental symbol file capture read throttle
    int symbol_throttle; //throttle speed
    int use_throttle; //only use throttle if set to 1

    //dstar header for ncurses
    unsigned char dstarradioheader[41];

    //dmr trunking stuff
    int dmr_rest_channel;
    int dmr_mfid; //just when 'fid' is used as a manufacturer ID and not a feature set id
    int dmr_vc_lcn;
    int dmr_vc_lsn;
    int dmr_tuned_lcn;

    //edacs
    int ea_mode;
    int esk_mode;
    unsigned short esk_mask;
    unsigned long long int edacs_site_id;
    int edacs_lcn_count; //running tally of lcn's observed on edacs system
    int edacs_cc_lcn; //current lcn for the edacs control channel
    int edacs_vc_lcn; //current lcn for any active vc (not the one we are tuned/tuning to)
    int edacs_tuned_lcn; //the vc we are currently tuned to...above variable is for updating all in the matrix

    //trunking group and lcn freq list
    long int trunk_lcn_freq[26]; //max number on an EDACS system, should be enough on DMR too hopefully
    long int trunk_chan_map[0xFFFF]; //NXDN - 10 bit; P25 - 16 bit; DMR up to 12 bit (standard TIII)
    groupinfo group_array[0x3FF]; //max supported by Cygwin is 3FFF, I hope nobody actually tries to import this many groups
    unsigned int group_tally; //tally number of groups imported from CSV file for referencing later
    int lcn_freq_count;
    int lcn_freq_roll; //number we have 'rolled' to in search of the CC
    time_t last_cc_sync_time; //use this to start hunting for CC after signal lost
    time_t last_vc_sync_time; //flag for voice activity bursts, tune back on con+ after more than x seconds no voice
    time_t last_active_time; //time the a 'call grant' was received, used to clear the active_channel strings after x seconds
    int is_con_plus; //con_plus flag for knowing its safe to skip payload channel after x seconds of no voice sync

    //new nxdn stuff
    int nxdn_part_of_frame;
    int nxdn_ran;
    int nxdn_sf;
    bool nxdn_sacch_non_superframe; //flag to indicate whether or not a sacch is a part of a superframe, or an individual piece
    uint8_t nxdn_sacch_frame_segment[4][18]; //part of frame by 18 bits
    uint8_t nxdn_sacch_frame_segcrc[4];
    uint8_t nxdn_alias_block_number;
    char nxdn_alias_block_segment[4][4][8];

    //site/srv/cch info
    char nxdn_location_category[14];
    uint32_t nxdn_location_sys_code;
    uint16_t nxdn_location_site_code;

    //channel access information
    uint8_t nxdn_rcn;
    uint8_t nxdn_base_freq;
    uint8_t nxdn_step;
    uint8_t nxdn_bw;

    //multi-key array
    unsigned long long int rkey_array[0xFFFF];
    int keyloader; //let us know the keyloader is active

    //dmr late entry mi
    uint64_t late_entry_mi_fragment[2][7][3];

    //dmr manufacturer branding and sub_branding (i.e., Motorola and Con+)
    char dmr_branding[20];
    char dmr_branding_sub[80];

    //Remus DMR End Call Alert Beep
    int dmr_end_alert[2]; //dmr TLC end call alert beep has already played once flag

    //Upsampled Audio Smoothing
    uint8_t audio_smoothing;

} dsd_state;

/*
 * Frame sync patterns
 */

#define FUSION_SYNC     "31111311313113131131" //HA!
#define INV_FUSION_SYNC "13333133131331313313" //HA!

#define INV_P25P1_SYNC "333331331133111131311111"
#define P25P1_SYNC     "111113113311333313133333"

#define P25P2_SYNC     "11131131111333133333"
#define INV_P25P2_SYNC "33313313333111311111"

#define X2TDMA_BS_VOICE_SYNC "113131333331313331113311"
#define X2TDMA_BS_DATA_SYNC  "331313111113131113331133"
#define X2TDMA_MS_DATA_SYNC  "313113333111111133333313"
#define X2TDMA_MS_VOICE_SYNC "131331111333333311111131"

#define DSTAR_HD       "131313131333133113131111"
#define INV_DSTAR_HD   "313131313111311331313333"
#define DSTAR_SYNC     "313131313133131113313111"
#define INV_DSTAR_SYNC "131313131311313331131333"

#define NXDN_MS_DATA_SYNC      "313133113131111333"
#define INV_NXDN_MS_DATA_SYNC  "131311331313333111"
#define INV_NXDN_BS_DATA_SYNC  "131311331313333131"
#define NXDN_BS_DATA_SYNC      "313133113131111313"
#define NXDN_MS_VOICE_SYNC     "313133113131113133"
#define INV_NXDN_MS_VOICE_SYNC "131311331313331311"
#define INV_NXDN_BS_VOICE_SYNC "131311331313331331"
#define NXDN_BS_VOICE_SYNC     "313133113131113113"

#define DMR_BS_DATA_SYNC  "313333111331131131331131"
#define DMR_BS_VOICE_SYNC "131111333113313313113313"
#define DMR_MS_DATA_SYNC  "311131133313133331131113"
#define DMR_MS_VOICE_SYNC "133313311131311113313331"

//Part 1-A CAI 4.4.4 (FSW only - Late Entry - Marginal Signal)
#define NXDN_FSW      "3131331131"
#define INV_NXDN_FSW  "1313113313"
//Part 1-A CAI 4.4.3 Preamble Last 9 plus FSW (start of RDCH)
#define NXDN_PANDFSW      "3131133313131331131" //19 symbols
#define INV_NXDN_PANDFSW  "1313311131313113313" //19 symbols

#define DMR_RC_DATA_SYNC  "131331111133133133311313"

#define DMR_DIRECT_MODE_TS1_DATA_SYNC  "331333313111313133311111"
#define DMR_DIRECT_MODE_TS1_VOICE_SYNC "113111131333131311133333"
#define DMR_DIRECT_MODE_TS2_DATA_SYNC  "311311111333113333133311"
#define DMR_DIRECT_MODE_TS2_VOICE_SYNC "133133333111331111311133"

#define INV_PROVOICE_SYNC    "31313111333133133311331133113311"
#define PROVOICE_SYNC        "13131333111311311133113311331133"
#define INV_PROVOICE_EA_SYNC "13313133113113333311313133133311"
#define PROVOICE_EA_SYNC     "31131311331331111133131311311133"

//define the provoice conventional string pattern to default 85/85 if not enabled, else mute it so we won't double sync on accident in frame_sync
#ifdef PVCONVENTIONAL
#define PROVOICE_CONV        "00000000000000000000000000000000" //all zeroes should be unobtainable string in the frame_sync synctests
#define INV_PROVOICE_CONV    "00000000000000000000000000000000" //all zeroes should be unobtainable string in the frame_sync synctests
#else
#define PROVOICE_CONV        "13131333111311311313131313131313" //TX 85 RX 85 (default programming value)
#define INV_PROVOICE_CONV    "31313111333133133131313131313131" //TX 85 RX 85 (default programming value)
#endif
//we use the short sync instead of the default 85/85 wnen PVCONVENTIONAL is defined by cmake
#define PROVOICE_CONV_SHORT                 "1313133311131131" //16-bit short pattern, last 16-bits change based on TX an RX values
#define INV_PROVOICE_CONV_SHORT             "3131311133313313"
//In this pattern (inverted polarity, the norm for PV) 3 is bit 0, and 1 is bit 1 (2 level GFSK)
//same pattern   //TX     //RX
// Sync Pattern = 3131311133313313 31331131 31331131 TX/RX 77  -- 31331131 symbol = 01001101 binary = 77 decimal
// Sync Pattern = 3131311133313313 33333333 33333333 TX/RX 0   -- 33333333 symbol = 00000000 binary = 0 decimal
// Sync Pattern = 3131311133313313 33333331 33333331 TX/RX 1   -- 33333331 symbol = 00000001 binary = 1 decimal
// Sync Pattern = 3131311133313313 13131133 13131133 TX/RX 172 -- 13131133 symbol = 10101100 binary = 172 decimal
// Sync Pattern = 3131311133313313 11333111 11333111 TX/RX 199 -- 11333111 symbol = 11000111 binary = 199 decimal
// Sync Pattern = 3131311133313313 31313131 31313131 TX/RX 85  -- 31313131 symbol = 01010101 binary = 85 decimal

#define EDACS_SYNC      "313131313131313131313111333133133131313131313131"
#define INV_EDACS_SYNC  "131313131313131313131333111311311313131313131313"

#define DPMR_FRAME_SYNC_1     "111333331133131131111313"
#define DPMR_FRAME_SYNC_2     "113333131331"
#define DPMR_FRAME_SYNC_3     "133131333311"
#define DPMR_FRAME_SYNC_4     "333111113311313313333131"

/* dPMR Frame Sync 1 to 4 - Inverted */
#define INV_DPMR_FRAME_SYNC_1 "333111113311313313333131"
#define INV_DPMR_FRAME_SYNC_2 "331111313113"
#define INV_DPMR_FRAME_SYNC_3 "311313111133"
#define INV_DPMR_FRAME_SYNC_4 "111333331133131131111313"

/*
 * function prototypes
 */

void openAudioInDevice(dsd_opts *opts);

int getDibit(dsd_opts *opts, dsd_state *state);

int get_dibit_and_analog_signal(dsd_opts *opts, dsd_state *state);

void skipDibit(dsd_opts *opts, dsd_state *state, int count);

void processFrame(dsd_opts *opts, dsd_state *state);

void printFrameSync(dsd_opts *opts, dsd_state *state, char *frametype, int offset, char *modulation);

int getFrameSync(dsd_opts *opts, dsd_state *state);

int comp(const void *a, const void *b);

void noCarrier(dsd_opts *opts, dsd_state *state);

void initOpts(dsd_opts *opts);

void initState(dsd_state *state);

void usage();

void liveScanner(dsd_opts *opts, dsd_state *state);

void cleanupAndExit(dsd_opts *opts, dsd_state *state);

// void sigfun (int sig); //not necesary
int main(int argc, char **argv);

void processMbeFrame(dsd_state *state, char ambe_fr[4][24], dsd_opts *opts);

int getSymbol(dsd_opts *opts, dsd_state *state, int have_sync);

short dmr_filter(short sample);

uint64_t ConvertBitIntoBytes(uint8_t *BufferIn, uint32_t BitLength);

void BPTCDeInterleaveDMRData(uint8_t *Input, uint8_t *Output);

uint32_t BPTC_196x96_Extract_Data(uint8_t InputDeInteleavedData[196], uint8_t DMRDataExtracted[96], uint8_t R[3]);

uint32_t BPTC_128x77_Extract_Data(uint8_t InputDataMatrix[8][16], uint8_t DMRDataExtracted[77]);

uint32_t
BPTC_16x2_Extract_Data(uint8_t InputInterleavedData[32], uint8_t DMRDataExtracted[32], uint32_t ParityCheckTypeOdd);

//Reed Solomon (12,9) functions
void rs_12_9_calc_syndrome(rs_12_9_codeword_t *codeword, rs_12_9_poly_t *syndrome);

uint8_t rs_12_9_check_syndrome(rs_12_9_poly_t *syndrome);

rs_12_9_correct_errors_result_t
rs_12_9_correct_errors(rs_12_9_codeword_t *codeword, rs_12_9_poly_t *syndrome, uint8_t *errors_found);

//DMR CRC Functions
uint16_t ComputeCrcCCITT(uint8_t *DMRData);

uint16_t ComputeCrcCCITT16d(const uint8_t buf[], uint8_t len);

uint32_t
ComputeAndCorrectFullLinkControlCrc(uint8_t *FullLinkControlDataBytes, uint32_t *CRCComputed, uint32_t CRCMask);

uint8_t ComputeCrc5Bit(uint8_t *DMRData);

uint16_t ComputeCrc9Bit(uint8_t *DMRData, uint32_t NbData);

uint32_t ComputeCrc32Bit(uint8_t *DMRData, uint32_t NbData);

//new simplified dmr functions
void dmr_data_burst_handler(dsd_opts *opts, dsd_state *state, uint8_t info[196], uint8_t databurst);

void dmr_data_sync(dsd_opts *opts, dsd_state *state);

void dmr_pi(dsd_opts *opts, dsd_state *state, uint8_t PI_BYTE[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors);

void dmr_flco(dsd_opts *opts, dsd_state *state, uint8_t lc_bits[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors,
              uint8_t type);

void dmr_cspdu(dsd_opts *opts, dsd_state *state, uint8_t cs_pdu_bits[], uint8_t cs_pdu[], uint32_t CRCCorrect,
               uint32_t IrrecoverableErrors);

uint32_t dmr_34(uint8_t *input, uint8_t treturn[18]); //simplier trellis decoder

//Embedded Alias and GPS
void dmr_embedded_alias_header(dsd_opts *opts, dsd_state *state, uint8_t lc_bits[]);

void dmr_embedded_alias_blocks(dsd_opts *opts, dsd_state *state, uint8_t lc_bits[]);

void dmr_embedded_gps(dsd_opts *opts, dsd_state *state, uint8_t lc_bits[]);

//"DMR STEREO"
void dmrBSBootstrap(dsd_opts *opts, dsd_state *state);

void dmrBS(dsd_opts *opts, dsd_state *state);

void dmrMS(dsd_opts *opts, dsd_state *state);

void dmrMSData(dsd_opts *opts, dsd_state *state);

void dmrMSBootstrap(dsd_opts *opts, dsd_state *state);

//dmr data header and multi block types (header, 1/2, 3/4, 1, Unified)
void dmr_dheader(dsd_opts *opts, dsd_state *state, uint8_t dheader[], uint8_t dheader_bits[], uint32_t CRCCorrect,
                 uint32_t IrrecoverableErrors);

void dmr_block_assembler(dsd_opts *opts, dsd_state *state, uint8_t block_bytes[], uint8_t block_len, uint8_t databurst,
                         uint8_t type);

void dmr_pdu(dsd_opts *opts, dsd_state *state, uint8_t block_len, uint8_t DMR_PDU[]);

void dmr_reset_blocks(dsd_opts *opts, dsd_state *state);

void dmr_lrrp(dsd_opts *opts, dsd_state *state, uint8_t block_len, uint8_t DMR_PDU[]);

void dmr_locn(dsd_opts *opts, dsd_state *state, uint8_t block_len, uint8_t DMR_PDU[]);

//dmr alg stuff
void dmr_alg_reset(dsd_opts *opts, dsd_state *state);

void dmr_alg_refresh(dsd_opts *opts, dsd_state *state);

void dmr_late_entry_mi_fragment(dsd_opts *opts, dsd_state *state, uint8_t vc, char ambe_fr[4][24], char ambe_fr2[4][24],
                                char ambe_fr3[4][24]);

void dmr_late_entry_mi(dsd_opts *opts, dsd_state *state);

//handle Single Burst (Voice Burst F) or Reverse Channel Signalling
void dmr_sbrc(dsd_opts *opts, dsd_state *state, uint8_t power);

//DMR FEC/CRC from Boatbod - OP25
bool Hamming17123(uint8_t *d);

uint8_t crc8(uint8_t bits[], unsigned int len);

bool crc8_ok(uint8_t bits[], unsigned int len);

//modified CRC functions for SB/RC
uint8_t crc7(uint8_t bits[], unsigned int len);

uint8_t crc3(uint8_t bits[], unsigned int len);

uint8_t crc4(uint8_t bits[], unsigned int len);

//LFSR and LFSRP code courtesy of https://github.com/mattames/LFSR/
void LFSR(dsd_state *state);

void LFSR64(dsd_state *state);

void Hamming_7_4_init();

bool Hamming_7_4_decode(unsigned char *rxBits);

void Hamming_12_8_init();

void Hamming_13_9_init();

bool Hamming_13_9_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords);

void Hamming_15_11_init();

bool Hamming_15_11_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords);

void Hamming_16_11_4_init();

bool Hamming_16_11_4_decode(unsigned char *rxBits, unsigned char *decodedBits, int nbCodewords);

void Golay_20_8_init();

bool Golay_20_8_decode(unsigned char *rxBits);

void Golay_23_12_init();

void Golay_24_12_init();

bool Golay_24_12_decode(unsigned char *rxBits);

void QR_16_7_6_init();

bool QR_16_7_6_decode(unsigned char *rxBits);

void InitAllFecFunction(void);

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif // DSD_H
