/*-------------------------------------------------------------------------------
 * dmr_bs.c
 * DMR BS Voice Handling and Data Gathering Routines - "DMR STEREO"
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include "dmr_const.h"

//A subroutine for processing each TDMA frame individually to allow for
//processing voice and/or data on both BS slots (channels) simultaneously
void dmrBS(dsd_opts *opts, dsd_state *state) {
    int i, dibit;

    char ambe_fr[4][24];
    char ambe_fr2[4][24];
    char ambe_fr3[4][24];

    //redundancy check (carrier signal loss event)
    char redundancyA[36];

    const int *w, *x, *y, *z;
    char sync[25];
    char syncdata[48];
    char EmbeddedSignalling[16];

    uint8_t emb_ok = 0;
    uint8_t tact_okay = 0;
    uint8_t cach_err = 0;

    uint8_t internalslot;
    uint8_t vc1;
    uint8_t vc2;

    //assign as nonsensical numbers
    uint8_t cc = 25;
    uint8_t power = 9; //power and pre-emption indicator
    uint8_t lcss = 9;

    //would be ideal to grab all dibits and break them into bits to pass to new data handler?
    uint8_t dummy_bits[196];
    memset(dummy_bits, 0, sizeof(dummy_bits));

    //Init slot lights
    sprintf(state->slot1light, " slot1 ");
    sprintf(state->slot2light, " slot2 ");

    //Init the color code status
    state->color_code_ok = 0;

    vc1 = 2;
    vc2 = 2;

    short int loop = 1;
    short int skipcount = 0;

    //cach
    char cachdata[25];
    int cachInterleave[24] =
            {0, 7, 8, 9, 1, 10,
             11, 12, 2, 13, 14,
             15, 3, 16, 4, 17, 18,
             19, 5, 20, 21, 22, 6, 23
            };

    //cach tact bits
    uint8_t tact_bits[7];

    //Run Loop while the getting is good
    while (loop == 1) {

        memset(ambe_fr, 0, sizeof(ambe_fr));
        memset(ambe_fr2, 0, sizeof(ambe_fr2));
        memset(ambe_fr3, 0, sizeof(ambe_fr3));

        internalslot = -1; //reset here so we know if this value is being set properly
        for (i = 0; i < 12; i++) {
            dibit = getDibit(opts, state);
            state->dmr_stereo_payload[i] = dibit;

            cachdata[cachInterleave[(i * 2)]] = (1 & (dibit >> 1)); // bit 1
            cachdata[cachInterleave[(i * 2) + 1]] = (1 & dibit);       // bit 0
        }

        for (i = 0; i < 7; i++) {
            tact_bits[i] = cachdata[i];
        }

        //disabled for dmr_cach testing
        tact_okay = 0;
        if (Hamming_7_4_decode(tact_bits)) tact_okay = 1;
        if (tact_okay != 1) goto END;


        internalslot = state->currentslot = tact_bits[1];

        //Setup for first AMBE Frame
        //Interleave Schedule
        w = rW;
        x = rX;
        y = rY;
        z = rZ;

        //First AMBE Frame, Full 36
        for (i = 0; i < 36; i++) {
            dibit = getDibit(opts, state);
            state->dmr_stereo_payload[i + 12] = dibit;
            redundancyA[i] = dibit;

            ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
            ambe_fr[*y][*z] = (1 & dibit);        // bit 0

            w++;
            x++;
            y++;
            z++;

        }

        //Setup for Second AMBE Frame
        //Interleave Schedule
        w = rW;
        x = rX;
        y = rY;
        z = rZ;

        //Second AMBE Frame, First Half 18 dibits just before Sync or EmbeddedSignalling
        for (i = 0; i < 18; i++) {
            dibit = getDibit(opts, state);
            state->dmr_stereo_payload[i + 48] = dibit;
            ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
            ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

            w++;
            x++;
            y++;
            z++;

        }

        //signaling data or sync
        for (i = 0; i < 24; i++) {
            dibit = getDibit(opts, state);
            state->dmr_stereo_payload[i + 66] = dibit;
            sync[i] = (dibit | 1) + 48; //how does this work again?

            syncdata[(2 * i)] = (1 & (dibit >> 1));  // bit 1
            syncdata[(2 * i) + 1] = (1 & dibit);         // bit 0

            //embedded link control
            //grab on vc1 values 2-5 B C D and E
            if (internalslot == 0 && vc1 > 1 && vc1 < 7) {
                state->dmr_embedded_signalling[internalslot][vc1 - 1][i * 2] = (1 & (dibit >> 1)); // bit 1
                state->dmr_embedded_signalling[internalslot][vc1 - 1][i * 2 + 1] = (1 & dibit); // bit 0
            }

            //grab on vc2 values 2-5 B C D and E
            if (internalslot == 1 && vc2 > 1 && vc2 < 7) {
                state->dmr_embedded_signalling[internalslot][vc2 - 1][i * 2] = (1 & (dibit >> 1)); // bit 1
                state->dmr_embedded_signalling[internalslot][vc2 - 1][i * 2 + 1] = (1 & dibit); // bit 0
            }

        }
        sync[24] = 0;

        for (i = 0; i < 8; i++) EmbeddedSignalling[i] = syncdata[i];
        for (i = 0; i < 8; i++) EmbeddedSignalling[i + 8] = syncdata[i + 40];

        //Continue Second AMBE Frame, 18 after Sync or EmbeddedSignalling
        for (i = 0; i < 18; i++) {
            dibit = getDibit(opts, state);
            state->dmr_stereo_payload[i + 90] = dibit;
            ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
            ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

            w++;
            x++;
            y++;
            z++;

        }

        //Setup for Third AMBE Frame
        //Interleave Schedule
        w = rW;
        x = rX;
        y = rY;
        z = rZ;

        //Third AMBE Frame, Full 36
        for (i = 0; i < 36; i++) {
            dibit = getDibit(opts, state);
            state->dmr_stereo_payload[i + 108] = dibit;
            ambe_fr3[*w][*x] = (1 & (dibit >> 1)); // bit 1
            ambe_fr3[*y][*z] = (1 & dibit);        // bit 0

            w++;
            x++;
            y++;
            z++;

        }

        //reset vc counters to 1 if new voice sync frame on each slot
        if (strcmp(sync, DMR_BS_VOICE_SYNC) == 0) {
            if (internalslot == 0) vc1 = 1;
            if (internalslot == 1) vc2 = 1;
        }

        //only play voice on no data sync
        //we already have a tact ecc check, so we won't get here without that, see if there is any other eccs we can run just to make sure
        if (strcmp(sync, DMR_BS_DATA_SYNC) != 0) {

            //check the embedded signalling, if bad at this point, we probably aren't quite in sync
            if (QR_16_7_6_decode(EmbeddedSignalling)) emb_ok = 1;
            else emb_ok = 0;

            //disable the goto END; if this causes more problems than fixing on late entry dual voices i.e., lots of forced resyncs
            if ((strcmp(sync, DMR_BS_VOICE_SYNC) != 0) && emb_ok == 0) goto END; //fprintf (stderr, "EMB BAD? ");
            else if (emb_ok == 1) {
                cc = ((EmbeddedSignalling[0] << 3) + (EmbeddedSignalling[1] << 2) + (EmbeddedSignalling[2] << 1) +
                      EmbeddedSignalling[3]);
                power = EmbeddedSignalling[4];
                lcss = ((EmbeddedSignalling[5] << 1) + EmbeddedSignalling[6]);
            }


            skipcount = 0; //reset skip count if processing voice frames

            //simplifying things
            char polarity[3];
            char light[18];
            uint8_t vc;

            processMbeFrame(state, ambe_fr);
            processMbeFrame(state, ambe_fr2);
            processMbeFrame(state, ambe_fr3);

            //run sbrc here to look for the late entry key and alg after we observe potential errors in VC6
            if (internalslot == 0 && vc1 == 6) dmr_sbrc(opts, state, power);
            if (internalslot == 1 && vc2 == 6) dmr_sbrc(opts, state, power);

            // run alg refresh after vc6 ambe processing
            if (internalslot == 0 && vc1 == 6)
                dmr_alg_refresh(opts, state);
            if (internalslot == 1 && vc2 == 6)
                dmr_alg_refresh(opts, state);

            //increment the vc counters
            if (internalslot == 0) vc1++;
            if (internalslot == 1) vc2++;

            //reset err checks
            cach_err = 1;
            tact_okay = 0;
            emb_ok = 0;

            //reset emb components
            cc = 25;
            power = 9;
            lcss = 9;

            //Extra safeguards to break loop
            // if ( (vc1 > 7 && vc2 > 7) ) goto END;
            if ((vc1 > 14 || vc2 > 14)) goto END;

        }

        //after 2 consecutive data frames, drop back to getFrameSync and process with dmr_data_sync
        SKIP:
        if (skipcount > 2) {
            //set tests to all good so we don't get a bogus/redundant voice error
            cach_err = 0;
            tact_okay = 1;
            emb_ok = 1;
            goto END;
        }

    } // while loop

    END:
    state->dmr_stereo = 0;
    state->errs = 0;
    state->errs2 = 0;
    state->errs2R = 0;
    state->errs2 = 0;

}

//Process buffered half frame and 2nd half and then jump to full BS decoding
void dmrBSBootstrap(dsd_opts *opts, dsd_state *state) {
    int i, j, k, l, dibit;
    int *dibit_p;
    char ambe_fr[4][24];
    char ambe_fr2[4][24];
    char ambe_fr3[4][24];

    memset(ambe_fr, 0, sizeof(ambe_fr));
    memset(ambe_fr2, 0, sizeof(ambe_fr2));
    memset(ambe_fr3, 0, sizeof(ambe_fr3));

    //memcpy of ambe_fr for late entry
    char m1[4][24];
    char m2[4][24];
    char m3[4][24];

    const int *w, *x, *y, *z;
    char sync[25];
    uint8_t tact_okay = 0;
    uint8_t cach_err = 0;
    uint8_t sync_okay = 1;

    uint8_t internalslot;

    char cachdata[25];
    int cachInterleave[24] =
            {0, 7, 8, 9, 1, 10,
             11, 12, 2, 13, 14,
             15, 3, 16, 4, 17, 18,
             19, 5, 20, 21, 22, 6, 23
            };
    //add time to mirror printFrameSync
    //payload buffer
    //CACH + First Half Payload + Sync = 12 + 54 + 24
    dibit_p = state->dmr_payload_p - 90;
    for (i = 0; i < 90; i++) //90
    {
        dibit = *dibit_p;
        dibit_p++;
        if (opts->inverted_dmr == 1) dibit = (dibit ^ 2) & 3;
        state->dmr_stereo_payload[i] = dibit;
    }

    for (i = 0; i < 12; i++) {
        dibit = state->dmr_stereo_payload[i];
        cachdata[cachInterleave[(i * 2)]] = (1 & (dibit >> 1)); // bit 1
        cachdata[cachInterleave[(i * 2) + 1]] = (1 & dibit);       // bit 0
    }

    //cach tact bits
    uint8_t tact_bits[7];
    for (i = 0; i < 7; i++) {
        tact_bits[i] = cachdata[i];
    }

    //decode and correct tact and compare
    if (Hamming_7_4_decode(tact_bits)) tact_okay = 1;
    if (tact_okay != 1)
        goto END;

    internalslot = state->currentslot = tact_bits[1];

    //Setup for first AMBE Frame

    //Interleave Schedule
    w = rW;
    x = rX;
    y = rY;
    z = rZ;

    //First AMBE Frame, Full 36
    for (i = 0; i < 36; i++) {
        dibit = state->dmr_stereo_payload[i + 12];
        ambe_fr[*w][*x] = (1 & (dibit >> 1)); // bit 1
        ambe_fr[*y][*z] = (1 & dibit);        // bit 0

        w++;
        x++;
        y++;
        z++;

    }

    //Setup for Second AMBE Frame

    //Interleave Schedule
    w = rW;
    x = rX;
    y = rY;
    z = rZ;

    //Second AMBE Frame, First Half 18 dibits just before Sync or EmbeddedSignalling
    for (i = 0; i < 18; i++) {
        dibit = state->dmr_stereo_payload[i + 48];
        ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
        ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

        w++;
        x++;
        y++;
        z++;

    }

    // signaling data or sync, just redo it
//    for (i = 0; i < 24; i++) {
//        dibit = state->dmr_stereo_payload[i + 66];
//        sync[i] = (dibit | 1) + 48;
//    }
//    sync[24] = 0;

//    if (strcmp(sync, DMR_BS_VOICE_SYNC) != 0) {
//        sync_okay = 0;
//        goto END;
//    }

    //Continue Second AMBE Frame, 18 after Sync or EmbeddedSignalling
    for (i = 0; i < 18; i++) {
        dibit = getDibit(opts, state);
        state->dmr_stereo_payload[i + 90] = dibit;
        ambe_fr2[*w][*x] = (1 & (dibit >> 1)); // bit 1
        ambe_fr2[*y][*z] = (1 & dibit);        // bit 0

        w++;
        x++;
        y++;
        z++;

    }

    //Setup for Third AMBE Frame

    //Interleave Schedule
    w = rW;
    x = rX;
    y = rY;
    z = rZ;

    //Third AMBE Frame, Full 36
    for (i = 0; i < 36; i++) {
        dibit = getDibit(opts, state);
        state->dmr_stereo_payload[i + 108] = dibit;
        ambe_fr3[*w][*x] = (1 & (dibit >> 1)); // bit 1
        ambe_fr3[*y][*z] = (1 & dibit);        // bit 0

        w++;
        x++;
        y++;
        z++;

    }

    char polarity[3];
    char light[18];

    if (state->currentslot == 0) {
        sprintf(light, "%s", " [SLOT1]  slot2  ");
    } else {
        sprintf(light, "%s", "  slot1  [SLOT2] ");
    }
    if (opts->inverted_dmr == 0) sprintf(polarity, "%s", "+");
    else sprintf(polarity, "%s", "-");

    fprintf(stderr, "Sync: %sDMR %s| Color Code=%02d | VC1*", polarity, light, state->dmr_color_code);

    dmr_alg_reset(opts, state);

    if (opts->payload == 1) fprintf(stderr, "\n"); //extra line break necessary here
    processMbeFrame(state, ambe_fr);
    processMbeFrame(state, ambe_fr2);
    processMbeFrame(state, ambe_fr3);

    if (opts->payload == 0) fprintf(stderr, "\n");

    //update voice sync time for trunking purposes (particularly Con+)
    if (opts->p25_is_tuned == 1) state->last_vc_sync_time = time(NULL);

    dmrBS(opts, state); //bootstrap into full TDMA frame for BS mode
    END:
    //if we have a tact err, then produce sync pattern/err message
    if (tact_okay != 1 || sync_okay != 1) {
        fprintf(stderr, "Sync:  DMR                  ");
        fprintf(stderr, "%s", KRED);
        fprintf(stderr, "| VOICE CACH/SYNC ERR");
        fprintf(stderr, "%s", KNRM);
        fprintf(stderr, "\n");
        //run refresh if either slot had an active MI in it.
        if (state->payload_algid >= 0x21) {
            state->currentslot = 0;
            dmr_alg_refresh(opts, state);
        }
        if (state->payload_algidR >= 0x21) {
            state->currentslot = 1;
            dmr_alg_refresh(opts, state);
        }

        //failsafe to reset all data header and blocks when bad tact or emb
        dmr_reset_blocks(opts, state);
    }


}
