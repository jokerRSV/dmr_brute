/*-------------------------------------------------------------------------------
 * dmr_csbk.c
 * DMR Control Signal Data PDU (CSBK, MBC, UDT) Handler and Related Functions
 *
 * Portions of Connect+ code reworked from Boatbod OP25
 * Source: https://github.com/LouisErigHerve/dsd/blob/master/src/dmr_sync.c
 *
 * Portions of Capacity+ code reworked from Eric Cottrell
 * Source: https://github.com/LinuxSheeple-E/dsd/blob/Feature/DMRECC/dmr_csbk.c
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//function for handling Control Signalling PDUs (CSBK, MBC, UDT) messages
void dmr_cspdu(dsd_opts *opts, dsd_state *state, uint8_t cs_pdu_bits[], uint8_t cs_pdu[], uint32_t CRCCorrect,
               uint32_t IrrecoverableErrors) {

    int csbk_lb = 0;
    int csbk_pf = 0;
    int csbk_o = 0;
    int csbk_fid = 0;

    long int ccfreq = 0;

    csbk_lb = ((cs_pdu[0] & 0x80) >> 7);
    csbk_pf = ((cs_pdu[0] & 0x40) >> 6);
    csbk_o = cs_pdu[0] & 0x3F;
    csbk_fid = cs_pdu[1]; //feature set id

    //check, regardless of CRC err
    if (IrrecoverableErrors == 0) {
        //Hytera XPT CSBK Check -- if bits 0 and 1 are used as lcss, gi, ts, then this bit may be set on
        if (csbk_fid == 0x68 && (csbk_o == 0x0A || csbk_o == 0x0B)) csbk_pf = 0;
        if (csbk_pf == 1) //check the protect flag, don't run if set
        {
            fprintf(stderr, "%s", KRED);
            fprintf(stderr, "\n Protected Control Signalling Block(s)");
            fprintf(stderr, "%s", KNRM);
        }
    }

    if (IrrecoverableErrors == 0 && CRCCorrect == 1) {
        //clear stale Active Channel messages here
        if (((time(NULL) - state->last_active_time) > 3) && ((time(NULL) - state->last_vc_sync_time) > 3)) {
            memset(state->active_channel, 0, sizeof(state->active_channel));
        }

        //update time to prevent random 'Control Channel Signal Lost' hopping
        //in the middle of voice call on current Control Channel (con+ and t3)
        state->last_cc_sync_time = time(NULL);

        if (csbk_pf == 0) //okay to run
        {

            //set overarching manufacturer in use when non-standard feature id set is up
            if (csbk_fid != 0) state->dmr_mfid = csbk_fid;

            fprintf(stderr, "%s", KYEL);

            //7.1.1.1.1 Channel Grant CSBK/MBC PDU
            if (csbk_o > 47 && csbk_o < 55) {

                //maintain this to allow users to hardset the cc freq as map[0]; otherwise, set from rigctl or rtl freq at c_aloha_sys_parms
                // if (state->p25_cc_freq == 0 && state->trunk_chan_map[0] != 0) state->p25_cc_freq = state->trunk_chan_map[0];

                //initial line break
                fprintf(stderr, "\n");

                long int freq = 0;

                //all of these messages share the same format (thankfully)
                if (csbk_o == 48) fprintf(stderr, " Private Voice Channel Grant (PV_GRANT)");
                if (csbk_o == 49) fprintf(stderr, " Talkgroup Voice Channel Grant (TV_GRANT)");
                if (csbk_o == 50)
                    fprintf(stderr,
                            " Broadcast Voice Channel Grant (BTV_GRANT)");  //listed as private in the appendix (error?)
                if (csbk_o == 51) fprintf(stderr, " Private Data Channel Grant: Single Item (PD_GRANT)");
                if (csbk_o == 52) fprintf(stderr, " Talkgroup Data Channel Grant: Single Item (TD_GRANT)");
                if (csbk_o == 53) fprintf(stderr, " Duplex Private Voice Channel Grant (PV_GRANT_DX)");
                if (csbk_o == 54) fprintf(stderr, " Duplex Private Data Channel Grant (PD_GRANT_DX)");

                //Logical Physical Channel Number
                uint16_t lpchannum = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[16], 12);
                if (lpchannum == 0)
                    fprintf(stderr,
                            " - Invalid Channel"); //invalid channel, not sure why this would even be transmitted
                else if (lpchannum == 0xFFF)
                    fprintf(stderr, " - Absolute"); //This is from an MBC, signalling an absolute and not a logical
                else fprintf(stderr, " - Logical");

                //dsdplus channel values
                uint16_t pluschannum = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[16], 13); //lcn bit included
                pluschannum += 1; //add a one for good measure

                //LCN conveyed here is the tdma timeslot variety, and not the RF frequency variety
                uint8_t lcn = cs_pdu_bits[28];
                //the next three bits can have different meanings depending on which item above for context
                uint8_t st1 = cs_pdu_bits[29]; //late_entry, hi_rate, reserved(dx)
                uint8_t st2 = cs_pdu_bits[30]; //emergency
                uint8_t st3 = cs_pdu_bits[31]; //offset, call direction
                //target and source are always the same bits
                uint32_t target = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
                uint32_t source = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[56], 24);

                //move mbc variables out of if statement
                uint8_t mbc_lb = 0; //
                uint8_t mbc_pf = 0;
                uint8_t mbc_csbko = 0;
                uint8_t mbc_res = 0;
                uint8_t mbc_cc = 0;
                uint8_t mbc_cdeftype = 0;
                uint8_t mbc_res2 = 0;
                unsigned long long int mbc_cdefparms = 0;
                uint16_t mbc_lpchannum = 0;
                uint16_t mbc_abs_tx_int = 0;
                uint16_t mbc_abs_tx_step = 0;
                uint16_t mbc_abs_rx_int = 0;
                uint16_t mbc_abs_rx_step = 0;

                fprintf(stderr, "\n");
                //added my best guess as to how dsdplus arrives at a dmr channel value (seems pretty consistent) as C+
                fprintf(stderr, "  Ch [%03X] Cd [%04d] C+ [%04d] - TS [%d] - Target [%08d] - Source [%08d]", lpchannum,
                        lpchannum, pluschannum, lcn, target, source);

                if (st2) fprintf(stderr, " Emergency");

                if (lpchannum == 0xFFF) //This is from an MBC, signalling an absolute and not a logical
                {
                    //7.1.1.1.2 Channel Grant Absolute Parameters CG_AP appended MBC PDU
                    mbc_lb = cs_pdu_bits[96]; //
                    mbc_pf = cs_pdu_bits[97];
                    mbc_csbko = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[98], 6);
                    mbc_res = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[104], 4);
                    mbc_cc = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[108], 4);
                    mbc_cdeftype = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[112],
                                                                 4); //see 7.2.19.7 = 0 for channel parms, 1 through FFFF reserved
                    mbc_res2 = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[116], 2);
                    long long int mbc_cdefparms = (unsigned long long int) ConvertBitIntoBytes(&cs_pdu_bits[118],
                                                                                               58); //see 7.2.19.7.1

                    //this is how you read the 58 parm bits according to the appendix 7.2.19.7.1
                    if (mbc_cdeftype == 0) //if 0, then absolute channel parms
                    {
                        mbc_lpchannum = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[118], 12);
                        mbc_abs_tx_int = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[130], 10);
                        mbc_abs_tx_step = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[140], 13);
                        mbc_abs_rx_int = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[153], 10);
                        mbc_abs_rx_step = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[163], 13);

                        //tx_int (Mhz) + (tx_step * 125) = tx_freq
                        //rx_int (Mhz) + (rx_step * 125) = rx_freq
                        fprintf(stderr, "\n");
                        fprintf(stderr, "  ABS-CHAN [%03X][%04d] - RX INT [%d][%X] - RX STEP [%d][%X]", mbc_lpchannum,
                                mbc_lpchannum, mbc_abs_rx_int, mbc_abs_rx_int, mbc_abs_rx_step, mbc_abs_rx_step);
                        //The Frequency we want to tune is the RX Frequency
                        freq = (mbc_abs_rx_int * 1000000) + (mbc_abs_rx_step * 125);
                    } else
                        fprintf(stderr, "\n  MBC Channel Grant - Unknown Parms: %015llX",
                                mbc_cdefparms); //for any reserved values
                }

                //print frequency from absolute
                if (freq != 0 && lpchannum == 0xFFF)
                    fprintf(stderr, "\n  Frequency [%.6lf] MHz", (double) freq / 1000000);

                //run external channel map function on logical
                if (lpchannum != 0 && lpchannum != 0xFFF) {
                    freq = state->trunk_chan_map[lpchannum];
                    (stderr, "\n  Frequency [%.6lf] MHz", (double) freq / 1000000);
                }

                //add active channel string to display
                if (lpchannum != 0 && lpchannum != 0xFFF) {
                    if (csbk_o == 49 || csbk_o == 50)
                        sprintf(state->active_channel[lcn], "Active Group Ch: %d TG: %d; ", lpchannum, target);
                    else if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54)
                        sprintf(state->active_channel[lcn], "Active Data Ch: %d TG: %d; ", lpchannum, target);
                    else sprintf(state->active_channel[lcn], "Active Private Ch: %d TG: %d; ", lpchannum, target);
                } else if (lpchannum == 0xFFF) {
                    if (csbk_o == 49 || csbk_o == 50)
                        sprintf(state->active_channel[lcn], "Active Group Ch: %d TG: %d; ", mbc_lpchannum, target);
                    else if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54)
                        sprintf(state->active_channel[lcn], "Active Data Ch: %d TG: %d; ", mbc_lpchannum, target);
                    else sprintf(state->active_channel[lcn], "Active Private Ch: %d TG: %d; ", mbc_lpchannum, target);
                }
                //update last active channel time
                state->last_active_time = time(NULL);

                //Skip tuning group calls if group calls are disabled
                if (opts->trunk_tune_group_calls == 0 && csbk_o == 49) goto SKIPCALL; //TV_GRANT
                if (opts->trunk_tune_group_calls == 0 && csbk_o == 50) goto SKIPCALL; //BTV_GRANT
                if (csbk_o == 50) csbk_o = 49; //flip to normal group call for group tuning

                //Allow tuning of data calls if user wishes by flipping the csbk_o to a group voice call
                if (csbk_o == 51 || csbk_o == 52 || csbk_o == 54) {
                    if (opts->trunk_tune_data_calls == 1) csbk_o = 49;
                }

                //Skip tuning private calls if private calls are disabled
                if (opts->trunk_tune_private_calls == 0 && csbk_o != 49) goto SKIPCALL;

                //if not a data channel grant (only tuning to voice channel grants)
                if (csbk_o == 48 || csbk_o == 49 || csbk_o == 50 || csbk_o ==
                                                                    53) //48, 49, 50 are voice grants, 51 and 52 are data grants, 53 Duplex Private Voice, 54 Duplex Private Data
                {
                    //TIII tuner fix if voice assignment occurs to the control channel itself,
                    //then it may not want to resume tuning due to no framesync loss after call ends
                    if ((time(NULL) - state->last_vc_sync_time > 2)) {
                        opts->p25_is_tuned = 0;
                        //zero out vc frequencies
                        state->p25_vc_freq[0] = 0;
                        state->p25_vc_freq[1] = 0;
                    }

                    //shim in here for ncurses freq display when not trunking (playback, not live)
                    if (opts->p25_trunk == 0 && freq != 0) {
                        //just set to both for now, could go on tslot later
                        state->p25_vc_freq[0] = freq;
                        state->p25_vc_freq[1] = freq;
                    }

                    //don't tune if currently a vc on the control channel
                    if ((time(NULL) - state->last_vc_sync_time > 2)) {
                        char mode[8]; //allow, block, digital, enc, etc

                        //if we are using allow/whitelist mode, then write 'B' to mode for block
                        //comparison below will look for an 'A' to write to mode if it is allowed
                        if (opts->trunk_use_allow_list == 1) sprintf(mode, "%s", "B");

                        for (int i = 0; i < state->group_tally; i++) {
                            if (state->group_array[i].groupNumber == target) {
                                fprintf(stderr, " [%s]", state->group_array[i].groupName);
                                strcpy(mode, state->group_array[i].groupMode);
                            }
                        }

                    }

                }

                SKIPCALL:; //do nothing

            }

            //Move
            if (csbk_o == 57) {
                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, " Move (C_MOVE) ");
            }

            //Aloha
            if (csbk_o == 25) {
                //initial line break
                fprintf(stderr, "\n");
                uint8_t reserved = cs_pdu_bits[16];
                uint8_t tsccas = cs_pdu_bits[17];
                uint8_t sync = cs_pdu_bits[18];
                uint8_t version = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[19], 3); //Document Version Control
                uint8_t offset = cs_pdu_bits[22]; //0-TSCC uses aligned timing; 1-TSCC uses offset timing
                uint8_t active = cs_pdu_bits[23]; //Active_Connection
                uint8_t mask = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[24], 5);
                uint8_t sf = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[29], 2); //service function
                uint8_t nrandwait = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[31], 4);
                uint8_t regreq = cs_pdu_bits[35];
                uint8_t backoff = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[36], 4);

                uint8_t model = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[40], 2);
                uint16_t net = 0;
                uint16_t site = 0;

                //DMR Location Area - DMRLA
                uint16_t sys_area = 0;
                uint16_t sub_area = 0;
                uint16_t sub_mask = 0x1;
                //tiny n 1-3; small 1-5; large 1-8; huge 1-10
                uint16_t n = 1; //The minimum value of DMRLA is normally ≥ 1, 0 is reserved


                char model_str[8];
                char par_str[8]; //category A, B, AB, or reserved

                sprintf(model_str, "%s", " ");
                sprintf(par_str, "%s", "Res");

                if (model == 0) //Tiny
                {
                    net = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[42], 9);
                    site = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[51], 3);
                    sprintf(model_str, "%s", "Tiny");
                    n = 3;
                } else if (model == 1) //Small
                {
                    net = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[42], 7);
                    site = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[49], 5);
                    sprintf(model_str, "%s", "Small");
                    n = 5;
                } else if (model == 2) //Large
                {
                    net = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[42], 4);
                    site = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[46], 8);
                    sprintf(model_str, "%s", "Large");
                    n = 8;
                } else if (model == 3) //Huge
                {
                    net = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[42], 2);
                    site = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[44], 10);
                    sprintf(model_str, "%s", "Huge");
                    n = 10;
                }

                //honestly can't say that this is accurate, just a guess
                uint8_t is_capmax = 0; //capmax(mot) flag
                if (csbk_fid == 0x10) {
                    n = 0;
                    is_capmax = 1;
                    opts->dmr_dmrla_is_set = 1;
                    opts->dmr_dmrla_n = 0;
                    sprintf(state->dmr_branding, "%s", "Motorola");
                    sprintf(state->dmr_branding_sub, "%s", "CapMax ");
                }

                if (opts->dmr_dmrla_is_set == 1) n = opts->dmr_dmrla_n;

                if (n == 0) sub_mask = 0x0;
                if (n == 1) sub_mask = 0x1;
                if (n == 2) sub_mask = 0x3;
                if (n == 3) sub_mask = 0x7;
                if (n == 4) sub_mask = 0xF;
                if (n == 5) sub_mask = 0x1F;
                if (n == 6) sub_mask = 0x3F;
                if (n == 7) sub_mask = 0x7F;
                if (n == 8) sub_mask = 0xFF;
                if (n == 9) sub_mask = 0x1FF;
                if (n == 10) sub_mask = 0x3FF;

                uint8_t par = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[54], 2);
                if (par == 1) sprintf(par_str, "%s", "A ");
                if (par == 2) sprintf(par_str, "%s", "B ");
                if (par == 3) sprintf(par_str, "%s", "AB");

                uint32_t target = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[56], 24);

                if (n != 0)
                    fprintf(stderr,
                            " C_ALOHA_SYS_PARMS - %s - Net ID: %d Site ID: %d.%d Cat: %s\n  Reg Req: %d V: %d Mask: %02X MS: %d",
                            model_str, net + 1, (site >> n) + 1, (site & sub_mask) + 1, par_str, regreq, version, mask,
                            target);
                else
                    fprintf(stderr,
                            " C_ALOHA_SYS_PARMS - %s - Net ID: %d Site ID: %d Cat: %s\n  Reg Req: %d V: %d Mask: %02X MS: %d",
                            model_str, net, site, par_str, regreq, version, mask, target);
                if (is_capmax) fprintf(stderr, " Capacity Max ");

                //add string for ncurses terminal display
                if (n != 0)
                    sprintf(state->dmr_site_parms, "TIII - %s %d-%d.%d ", model_str, net + 1, (site >> n) + 1,
                            (site & sub_mask) + 1);
                else sprintf(state->dmr_site_parms, "TIII - %s %d-%d ", model_str, net, site);


                //debug print
                uint16_t syscode = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[40], 16);
                //fprintf (stderr, "\n  SYSCODE: %016b", syscode);
                //fprintf (stderr, "\n  SYSCODE: %02b.%b.%b", model, net, site); //won't show leading zeroes, but may not be required
                //fprintf (stderr, "\n  SYSCODE: %04X", syscode);
            }

            //P_CLEAR
            if (csbk_o == 46) {
                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, " Clear (P_CLEAR) ");
            }

            //(P_PROTECT)
            if (csbk_o == 47) {
                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, " Protect (P_PROTECT) -");
                uint16_t reserved = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[16], 12);
                uint8_t p_kind = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[28], 3);
                uint8_t gi = cs_pdu_bits[31];
                uint32_t target = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
                uint32_t source = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[56], 24);

                if (gi) fprintf(stderr, " Group");
                if (!gi) fprintf(stderr, " Private");

                if (p_kind == 0) fprintf(stderr, " Disable Target PTT (DIS_PTT)");
                if (p_kind == 1) fprintf(stderr, " Enable Target PTT (EN_PTT)");
                if (p_kind == 2) fprintf(stderr, " Call (ILLEGALLY_PARKED)");
                if (p_kind == 3) fprintf(stderr, " Enable Target MS PTT (EN_PTT_ONE_MS)");

                fprintf(stderr, "\n");
                fprintf(stderr, "  Target [%08d] - Source [%08d]", target, source);

            }

            if (csbk_o == 40) //0x28 //check to see if these are fid 0 only, or also in other manufacturer fids
            {
                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, " Announcements (C_BCAST)"); //site it, vote for etc etc
                uint8_t a_type = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[16], 5);
                if (a_type == 0) fprintf(stderr, " Announce/Withdraw TSCC (Ann_WD_TSCC)");
                if (a_type == 1) fprintf(stderr, " Specify Call Timer Parameters (CallTimer_Parms)");
                if (a_type == 2) fprintf(stderr, " Vote Now Advice (Vote_Now)");
                if (a_type == 3) fprintf(stderr, " Broadcast Local Time (Local_Time)");
                if (a_type == 4) fprintf(stderr, " Mass Registration (MassReg)");
                if (a_type == 5) fprintf(stderr, " Announce Logical Channel/Frequency Relationship (Chan_Freq)");
                if (a_type == 6) fprintf(stderr, " Adjacent Site Information (Adjacent_Site)");
                if (a_type == 7) fprintf(stderr, " General Site Parameters (Gen_Site_Params)");
                if (a_type > 7 && a_type < 0x1E) fprintf(stderr, " Reserved %02X", a_type);
                if (a_type == 0x1E || a_type == 0x1F) fprintf(stderr, " Manufacturer Specific %02X", a_type);

                //sysidcode, probably would have been easier to make this its own external function
                uint8_t model = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[40], 2);
                uint16_t net = 0;
                uint16_t site = 0;

                //DMR Location Area - DMRLA
                uint16_t sys_area = 0;
                uint16_t sub_area = 0;
                uint16_t sub_mask = 0x1;
                //tiny n 1-3; small 1-5; large 1-8; huge 1-10
                uint16_t n = 1; //The minimum value of DMRLA is normally ≥ 1, 0 is reserved

                //channel number from Broadcast Parms 2
                uint16_t lpchannum = 0;

                char model_str[8];
                char par_str[8]; //category A, B, AB, or reserved

                sprintf(model_str, "%s", " ");
                sprintf(par_str, "%s", "Res");

                if (model == 0) //Tiny
                {
                    net = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[42], 9);
                    site = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[51], 3);
                    sprintf(model_str, "%s", "Tiny");
                    n = 3;
                } else if (model == 1) //Small
                {
                    net = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[42], 7);
                    site = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[49], 5);
                    sprintf(model_str, "%s", "Small");
                    n = 5;
                } else if (model == 2) //Large
                {
                    net = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[42], 4);
                    site = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[46], 8);
                    sprintf(model_str, "%s", "Large");
                    n = 8;
                } else if (model == 3) //Huge
                {
                    net = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[42], 2);
                    site = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[44], 10);
                    sprintf(model_str, "%s", "Huge");
                    n = 10;
                }

                //disabled until this can be sorted/fleshed out by type, going to use aloha only for now

                //honestly can't say that this is accurate, just a guess
                // uint8_t is_capmax = 0; //capmax(mot) flag
                // if (csbk_fid == 0x10)
                // {
                //   n = 0;
                //   is_capmax = 1;
                //   opts->dmr_dmrla_is_set = 1;
                //   opts->dmr_dmrla_n = 0;
                //   sprintf (state->dmr_branding, "%s", "Motorola");
                //   sprintf (state->dmr_branding_sub, "%s", "CapMax ");
                // }

                if (opts->dmr_dmrla_is_set == 1) n = opts->dmr_dmrla_n;

                if (n == 0) sub_mask = 0x0;
                if (n == 1) sub_mask = 0x1;
                if (n == 2) sub_mask = 0x3;
                if (n == 3) sub_mask = 0x7;
                if (n == 4) sub_mask = 0xF;
                if (n == 5) sub_mask = 0x1F;
                if (n == 6) sub_mask = 0x3F;
                if (n == 7) sub_mask = 0x7F;
                if (n == 8) sub_mask = 0xFF;
                if (n == 9) sub_mask = 0x1FF;
                if (n == 10) sub_mask = 0x3FF;

                uint8_t par = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[54], 2);
                if (par == 1) sprintf(par_str, "%s", "A ");
                if (par == 2) sprintf(par_str, "%s", "B ");
                if (par == 3) sprintf(par_str, "%s", "AB");

                uint32_t parms2 = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
                lpchannum = parms2 & 0xFFF; //7.2.19.7, for DSDPlus, this value is (lpchannum << 1) + 1;

                //disabled until this can be sorted/fleshed out by type, going to use aloha only for now

                // if (a_type == 2 || a_type == 7)
                // {
                //   fprintf (stderr, "\n");
                //   if (n != 0) fprintf (stderr, "  C_BCAST_SYS_PARMS - %s - Net ID: %d Site ID: %d.%d Cat: %s Cd: %d C+: %d", model_str, net+1, (site>>n)+1, (site & sub_mask)+1, par_str, lpchannum, (lpchannum << 1)+1 );
                //   else fprintf (stderr, "  C_BCAST_SYS_PARMS - %s - Net ID: %d Site ID: %d Cat: %s Cd: %d C+: %d", model_str, net, site, par_str, lpchannum, (lpchannum << 1)+1 );
                //   if (is_capmax) fprintf (stderr, " Capacity Max");

                //   //add string for ncurses terminal display, if not adjacent site info
                //   if (a_type != 6 && n != 0)  sprintf (state->dmr_site_parms, "TIII - %s %d-%d.%d ", model_str, net+1, (site>>n)+1, (site & sub_mask)+1);
                //   if (a_type != 6 && n == 0)  sprintf (state->dmr_site_parms, "TIII - %s %d-%d ", model_str, net, site);
                // }

                uint16_t syscode = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[40], 16);
                //fprintf (stderr, "\n  SYSCODE: %016b", syscode);
                //fprintf (stderr, "\n  SYSCODE: %04X", syscode);

            }

            if (csbk_o == 28) {
                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, " C_AHOY ");
            }

            if (csbk_o == 42) {
                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, " P_MAINT ");
            }

            if (csbk_o == 30) {
                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, " C_ACKVIT ");
            }

            if (csbk_o == 31) {
                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, " C_RAND ");
            }

            //tier 2 csbks
            if (csbk_o == 4) {
                fprintf(stderr, "\n");
                fprintf(stderr, " Unit to Unit Voice Service Request (UU_V_Req) - ");

                uint32_t target = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
                uint32_t source = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
                fprintf(stderr, "Target [%d] - Source [%d] ", target, source);
            }

            if (csbk_o == 5) {
                fprintf(stderr, "\n");
                fprintf(stderr, " Unit to Unit Voice Service Answer Response (UU_Ans_Req) - ");

                uint32_t target = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
                uint32_t source = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
                fprintf(stderr, "Target [%d] - Source [%d] ", target, source);

            }

            if (csbk_o == 7) {
                fprintf(stderr, "\n");
                fprintf(stderr, " Channel Timing CSBK (CT_CSBK) ");
            }

            if (csbk_o == 38) {
                fprintf(stderr, "\n");
                fprintf(stderr, " Negative Acknowledgement Response (NACK_Rsp) - ");

                uint32_t target = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
                uint32_t source = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
                fprintf(stderr, "Target [%d] - Source [%d] ", target, source);

            }

            if (csbk_o == 56) {
                fprintf(stderr, "\n");
                fprintf(stderr, " BS Outbound Activation (BS_Dwn_Act) - ");

                uint32_t target = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
                uint32_t source = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[56], 24);
                fprintf(stderr, "Target [%d] - Source [%d] ", target, source);
            }

            if (csbk_o == 61) {
                fprintf(stderr, "\n");
                fprintf(stderr, " Preamble CSBK - ");
                uint8_t content = cs_pdu_bits[16];
                uint8_t gi = cs_pdu_bits[17];
                uint8_t res = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[18], 6);
                uint8_t blocks = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[24], 8);
                uint32_t target = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[32], 24);
                uint32_t source = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[56], 24);

                uint8_t target_hash[24];
                uint8_t tg_hash = 0;

                if (gi == 0) fprintf(stderr, "Individual ");
                else fprintf(stderr, "Group ");

                if (content == 0) fprintf(stderr, "CSBK - ");
                else fprintf(stderr, "Data - ");

                // check to see if this is XPT
                if (strcmp(state->dmr_branding_sub, "XPT ") == 0) {
                    //get 16-bit truncated target and source ids
                    target = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[40], 16); //40, or 32?
                    source = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[64], 16); //56, or 64?

                    //check to see if this is indiv data (private) then we will need to report the hash value as well
                    if (gi == 0) {
                        //the crc8 hash is the value represented in the Hytera XPT Site Status CSBK when dealing with indiv data
                        for (int i = 0; i < 16; i++) target_hash[i] = cs_pdu_bits[40 + i];
                        tg_hash = crc8(target_hash, 16);
                        fprintf(stderr, "Source: %d - Target: %d - Hash: %d ", source, target, tg_hash);
                    } else fprintf(stderr, "Source: %d - Target: %d ", source, target);

                }
                    // check to see if this is Cap+
                else if (strcmp(state->dmr_branding_sub, "Cap+ ") == 0) {
                    //truncate tg on group? or just on private/individual data?
                    if (gi == 0) target = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[40], 16);
                    source = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[64], 16);
                    int rest = (uint32_t) ConvertBitIntoBytes(&cs_pdu_bits[60], 4);
                    fprintf(stderr, "Source: %d - Target: %d - Rest LSN: %d", source, target, rest);
                } else fprintf(stderr, "Source: %d - Target: %d ", source, target);

            }
            //end tier 2 csbks

        }

        //Reworked Cap+ for Multi Block FL PDU and Private/Data Calls
        //experimental, but seems to be working well
        if (csbk_fid == 0x10) {

            //Cap+ Something
            if (csbk_o == 0x3A) {
                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, "%s", KYEL);

                fprintf(stderr, " Capacity Plus CSBK 0x3A ");

                // 01:15:08 Sync: +DMR   slot1  [slot2] | Color Code=03 | CSBK
                // Capacity Plus Channel Status - FL: 3 TS: 1 RS: 0 - Rest Channel 1 - Single Block
                //   Ch1: Rest Ch2: Idle Ch3: Idle Ch4: Idle
                //   Ch5: Idle Ch6: Idle Ch7: Idle Ch8: Idle
                // DMR PDU Payload [BE][10][E1][00][00][00][00][00][00][00][4E][15]
                // 01:15:08 Sync: +DMR  [slot1]  slot2  | Color Code=03 | CSBK

                //FL, TS, and Rest Channel seem to be in same location as the Channel Status CSBK
                //other values are currently unknown
                // DMR PDU Payload [BA][10][C1][3B][61][11][51][00][00][00][3D][D6]
                // 01:15:08 Sync: +DMR   slot1  [slot2] | Color Code=03 | CSBK
                // DMR PDU Payload [BA][10][E1][3B][61][11][51][00][00][00][46][BE]

            }

            //Cap+ Adjacent Sites
            if (csbk_o == 0x3B) {

                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, "%s", KYEL);

                fprintf(stderr, " Capacity Plus Adjacent Sites\n  ");

                uint8_t nl[6]; //adjacet site numerical value
                uint8_t nr[6]; //adjacent site current rest channel
                memset(nl, 0, sizeof(nl));
                memset(nr, 0, sizeof(nr));

                for (int i = 0; i < 6; i++) {
                    nl[i] = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[32 + (i * 8)], 4);
                    nr[i] = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[36 + (i * 8)], 4);
                    if (nl[i]) fprintf(stderr, "Site: %d Rest: %d; ", nl[i], nr[i]);
                }

            }

            //Cap+ Channel Status -- Expanded for up to 16 LSNs and private voice/data calls and dual slot csbk_bit storage
            if (csbk_o == 0x3E) {

                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, "%s", KYEL);

                uint8_t fl = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[16], 2);
                uint8_t ts = cs_pdu_bits[18];  //timeslot this PDU occurs in
                uint8_t res = cs_pdu_bits[19]; //unknown or unused bit value
                uint8_t rest_channel = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[20], 4);
                uint8_t group_tally = 0; //set this to the number of active channels tallied
                uint8_t bank_one = 0;
                uint8_t bank_two = 0;
                uint8_t b2_start = 0;
                uint8_t block_num = state->cap_plus_block_num[ts];
                uint8_t pdflag = 0;
                uint8_t pdflag2 = 0;
                uint16_t private_target = 0;
                uint8_t pd_b2 = 0;
                uint8_t start = 8;
                uint8_t end = 8;
                uint8_t fl_bytes = 0;
                uint8_t ch[24]; //one bit per channel
                uint8_t pch[24]; //private or data call channel bits
                uint16_t tg = 0;
                int i, j, k, x;
                //tg and channel info for trunking purposes
                uint16_t t_tg[24];
                char cap_active[20]; //local string to concantenate to active channel stuff

                //sanity check
                if (block_num > 6) {
                    state->cap_plus_block_num[ts] = 6;
                    block_num = 6;
                }

                //init some arrays
                memset(t_tg, 0, sizeof(t_tg));
                memset(ch, 0, sizeof(ch));
                memset(pch, 0, sizeof(pch));

                //treating FL as a form of LCSS
                if (fl == 2 || fl == 3) //initial or single block (fl2 or fl3)
                {
                    //NOTE: this has been changed to store per slot
                    memset(state->cap_plus_csbk_bits[ts], 0, sizeof(state->cap_plus_csbk_bits[ts]));
                    for (i = 0; i < 10 * 8; i++) state->cap_plus_csbk_bits[ts][i] = cs_pdu_bits[i];
                    state->cap_plus_block_num[ts] = 0;
                } else //appended block (fl 0) or final block (fl 1)
                {
                    for (i = 0; i < 7 * 8; i++)
                        state->cap_plus_csbk_bits[ts][i + 80 + (7 * 8 * block_num)] = cs_pdu_bits[i + 24];
                    block_num++;
                    state->cap_plus_block_num[ts]++;
                }

                if (rest_channel != state->dmr_rest_channel) {
                    state->dmr_rest_channel = rest_channel;
                }

                //assign to cc freq to follow during no sync
                if (state->trunk_chan_map[rest_channel] != 0) {
                    state->p25_cc_freq = state->trunk_chan_map[rest_channel];
                    //set to always tuned
                    opts->p25_is_tuned = 1;
                }

                fprintf(stderr, " Capacity Plus Channel Status - FL: %d TS: %d RS: %d - Rest LSN: %d", fl, ts, res,
                        rest_channel);
                if (fl == 0) fprintf(stderr, " - Appended Block"); //have not yet observed a system use this fl value
                if (fl == 1) fprintf(stderr, " - Final Block");
                if (fl == 2) fprintf(stderr, " - Initial Block");
                if (fl == 3) fprintf(stderr, " - Single Block");

                //look at each group channel bit -- run this PRIOR to checking private or data calls
                //or else we will overwrite the ch bits we set below for private calls with zeroes
                //these also seem to indicate data calls -- can't find a distinction of which is which
                bank_one = (uint8_t) ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][24], 8);
                for (int i = 0; i < 8; i++) {
                    ch[i] = state->cap_plus_csbk_bits[ts][i + 24];
                    if (ch[i] == 1)
                        group_tally++; //figure out where to start looking in the byte stream for the 0next bank of calls
                }

                //Expanded to cover larger systems up to 16 slots.
                bank_two = (uint8_t) ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][32 + (group_tally * 8)],
                                                         8); //32
                b2_start = group_tally;
                if (bank_two) {
                    for (int i = 0; i < 8; i++) {
                        ch[i + 8] = state->cap_plus_csbk_bits[ts][i + 32 + (b2_start * 8)];
                        if (ch[i + 8] == 1) group_tally++;
                    }
                }

                //check flag for appended private and/or data calls
                pdflag = (uint8_t) ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][40 + (group_tally * 8)], 8);

                //check for private activity on LSNs 1-8
                if (fl == 1 || fl == 3) {
                    if (pdflag) //== 0x80 //testing denny's 0x80 or 0x90 flag discovery
                    {
                        k = 0; //= 0
                        fprintf(stderr, "\n");
                        fprintf(stderr, " Bank One F%X Private or Data Call(s) - ", pdflag);
                        for (int i = 0; i < 8; i++) {
                            pch[i] = state->cap_plus_csbk_bits[ts][i + 48 + (group_tally * 8)];
                            if (pch[i] == 1) {
                                fprintf(stderr, " LSN %02d:", i + 1);
                                private_target = (uint16_t) ConvertBitIntoBytes(
                                        &state->cap_plus_csbk_bits[ts][56 + (k * 16) + (group_tally * 8)], 16);
                                fprintf(stderr, " TGT %d;", private_target);
                                k++;
                                if (bank_one == 0)
                                    bank_one = 0xFF; //set all bits on so we can atleast parse all of them below in listing/display
                            }
                        }
                        //save for starting point of the next private call bank
                        pd_b2 = k;
                    }
                }
                //end private/data call check on LSNs 1-8

                //check flag for appended private and/or data calls -- flag two still needs work or testing, double checking, etc, disable if issues arise
                pdflag2 = (uint8_t) ConvertBitIntoBytes(
                        &state->cap_plus_csbk_bits[ts][56 + (group_tally * 8) + (pd_b2 * 16)],
                        8);  //48 -- had wrong value here (atleast in the one sample with the false positive)

                //check for private activity on LSNs 9-16
                if (fl == 1 || fl == 3) {
                    //then check to see if this byte has a value, should be 0x80, could be other?
                    //this bytes location shifts depending on level of activity -- see banks above
                    if (pdflag2) //== 0x80 //testing denny's 0x80 or 0x90 flag discovery
                    {
                        k = 0;
                        fprintf(stderr, "\n");
                        fprintf(stderr, " Bank Two F%02X Private or Data Call(s) - ", pdflag2);
                        for (int i = 0; i < 8; i++) {
                            pch[i + 8] = state->cap_plus_csbk_bits[ts][i + 64 + (group_tally * 8) + (pd_b2 * 16)]; //56
                            if (pch[i + 8] == 1) {
                                fprintf(stderr, " LSN %02d:", i + 1);
                                private_target = (uint16_t) ConvertBitIntoBytes(
                                        &state->cap_plus_csbk_bits[ts][64 + (k * 16) + (group_tally * 8) +
                                                                       (pd_b2 * 16)],
                                        16); //56 -- had wrong value here (atleast in the one sample with the false positive)
                                fprintf(stderr, " TGT %d;", private_target);
                                k++;
                                if (bank_two == 0)
                                    bank_two = 0xFF; //set all bits on so we can atleast parse all of them below in listing/display
                            }
                        }
                    }
                }
                //end private/data call check on LSNs 9-16

                if (fl == 1 || fl == 3) {
                    fprintf(stderr, "\n  ");

                    //additive strings for active channels
                    memset(state->active_channel, 0, sizeof(state->active_channel));
                    sprintf(state->active_channel[0], "Cap+ ");
                    state->last_active_time = time(NULL);

                    k = 0;
                    x = 0;

                    //NOTE: New start and end positions will allow the status print
                    //to contract and expand as needed to prevent this from
                    //being a four liner CSBK when there is little or no activity

                    //start position;
                    start = 0;
                    if (bank_one & 0xF0) start = 0;
                    else if (rest_channel < 5) start = 0;

                    else if (bank_one & 0xF) start = 4;
                    else if (rest_channel > 4 && rest_channel < 9) start = 4;

                    else if (bank_two & 0xF0) start = 8;
                    else if (rest_channel > 8 && rest_channel < 13) start = 8;

                    else if (bank_two & 0xF) start = 12;
                    else if (rest_channel > 12) start = 12;

                    //end position;
                    end = 16;
                    if (bank_two & 0xF) end = 16;
                    else if (rest_channel > 12) end = 16;

                    else if (bank_two & 0xF0) end = 12;
                    else if (rest_channel > 9 && rest_channel < 13) end = 12;

                    else if (bank_one & 0xF) end = 8;
                    else if (rest_channel > 4 && rest_channel < 9) end = 8;

                    else if (bank_one & 0xF0) end = 4;
                    else if (rest_channel < 5) end = 4;

                    //start parsing info and listing active LSNs
                    for (i = start; i < end; i++) {
                        //skip an additional k value per bank
                        if (i == 8) k++;

                        if (start < 1 && i == 4)
                            fprintf(stderr, "\n  ");

                        if (start < 5 && i == 8)
                            fprintf(stderr, "\n  ");

                        if (start < 9 && i == 12)
                            fprintf(stderr, "\n  ");

                        fprintf(stderr, "LSN %02d: ", i + 1);
                        if (ch[i] == 1) //group voice channels
                        {

                            tg = (uint16_t) ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][(k * 8) + 32], 8);
                            if (tg != 0) fprintf(stderr, "%5d;  ", tg);
                            else fprintf(stderr, "Group;  ");
                            //flag as available for tuning if group calls enabled
                            if (opts->trunk_tune_group_calls == 1) t_tg[i] = tg;
                            if (tg != 0) k++;

                            //add active channel to display string
                            sprintf(cap_active, "LSN:%d TG:%d; ", i + 1, tg);
                            strcat(state->active_channel[i + 1], cap_active);
                        } else if (pch[i] == 1) //private or data channels
                        {
                            tg = (uint16_t) ConvertBitIntoBytes(
                                    &state->cap_plus_csbk_bits[ts][(group_tally * 8) + (x * 16) + 56],
                                    16); //don't change this AGAIN!, this is correct!
                            if (tg != 0) fprintf(stderr, "%5d;  ", tg);
                            else fprintf(stderr, " P||D;  ");
                            //flag as available for tuning if data calls enabled
                            if (opts->trunk_tune_data_calls == 1) t_tg[i] = tg; //ch[i] = 1;
                            if (tg != 0) x++;

                            //add active channel to display string

                            //NOTE: Consider only adding this if user toggled or else
                            //lots of short data bursts blink in and out
                            if (1 == 1) //opts->trunk_tune_data_calls
                            {
                                sprintf(cap_active, "LSN:%d PC:%d; ", i + 1, tg);
                                strcat(state->active_channel[i + 1], cap_active);
                            }

                        } else if (i + 1 == rest_channel) fprintf(stderr, " Rest;  ");
                        else fprintf(stderr, " Idle;  ");

                    }


                    state->dmr_mfid = 0x10;
                    sprintf(state->dmr_branding, "%s", "Motorola");
                    sprintf(state->dmr_branding_sub, "%s", "Cap+ ");

                    //nullify any previous site_parm data
                    sprintf(state->dmr_site_parms, "%s", "");

                    fprintf(stderr, "%s", KNRM);

                    //Skip tuning group calls if group calls are disabled -- moved to individual switches in the parsing phase
                    // if (opts->trunk_tune_group_calls == 0) goto SKIPCAP;

                    //Test allowing a group in the white list to preempt a call in progress and tune to a white listed call
                    if (opts->trunk_use_allow_list == 1) state->last_vc_sync_time = 0;

                    SKIPCAP:;
                    //debug print
                    if (fl == 1 && opts->payload == 1) {
                        fprintf(stderr, "%s\n", KYEL);
                        fprintf(stderr, " CAP+ Multi Block PDU \n  ");
                        fl_bytes = 0;
                        for (i = 0; i < (10 + (block_num * 7)); i++) {
                            fl_bytes = (uint8_t) ConvertBitIntoBytes(&state->cap_plus_csbk_bits[ts][i * 8], 8);
                            fprintf(stderr, "[%02X]", fl_bytes);
                            if (i == 17 || i == 35) fprintf(stderr, "\n  ");
                        }
                        fprintf(stderr, "%s", KNRM);
                    }
                    memset(state->cap_plus_csbk_bits[ts], 0, sizeof(state->cap_plus_csbk_bits[ts]));
                    state->cap_plus_block_num[ts] = 0;

                } //if (fl == 1 || fl == 3)
            } //opcode == 0x3E

        } //fid == 0x10

        //Connect+ Section
        if (csbk_fid == 0x06) {

            if (csbk_o == 0x01) {
                uint8_t nb1 = cs_pdu[2] & 0x3F;
                uint8_t nb2 = cs_pdu[3] & 0x3F;
                uint8_t nb3 = cs_pdu[4] & 0x3F;
                uint8_t nb4 = cs_pdu[5] & 0x3F;
                uint8_t nb5 = cs_pdu[6] & 0x3F;

                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, "%s", KYEL);
                fprintf(stderr, " Connect Plus Neighbors\n");
                fprintf(stderr, "  NB1(%02d), NB2(%02d), NB3(%02d), NB4(%02d), NB5(%02d)", nb1, nb2, nb3, nb4, nb5);
                state->dmr_mfid = 0x06;
                sprintf(state->dmr_branding, "%s", "Motorola");
                sprintf(state->dmr_branding_sub, "Con+ ");

            }

            if (csbk_o == 0x03) {

                //initial line break
                fprintf(stderr, "\n");
                uint32_t srcAddr = ((cs_pdu[2] << 16) + (cs_pdu[3] << 8) + cs_pdu[4]);
                uint32_t grpAddr = ((cs_pdu[5] << 16) + (cs_pdu[6] << 8) + cs_pdu[7]);
                uint8_t lcn = ((cs_pdu[8] & 0xF0) >> 4);
                uint8_t tslot = ((cs_pdu[8] & 0x08) >> 3);
                fprintf(stderr, "%s", KYEL);
                fprintf(stderr, " Connect Plus Voice Channel Grant\n");
                fprintf(stderr, "  srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)", srcAddr, grpAddr, lcn, tslot);
                state->dmr_mfid = 0x06;
                sprintf(state->dmr_branding, "%s", "Motorola");
                sprintf(state->dmr_branding_sub, "Con+ ");

                //add active channel string for display
                sprintf(state->active_channel[0], "Active Ch: %d TG: %d; ", lcn, grpAddr);
                state->last_active_time = time(NULL);

                //Skip tuning group calls if group calls are disabled
                if (opts->trunk_tune_group_calls == 0) goto SKIPCON;

                //shim in here for ncurses freq display when not trunking (playback, not live)
                if (opts->p25_trunk == 0 && state->trunk_chan_map[lcn] != 0) {
                    //just set to both for now, could go on tslot later
                    state->p25_vc_freq[0] = state->trunk_chan_map[lcn];
                    state->p25_vc_freq[1] = state->trunk_chan_map[lcn];
                }

                char mode[8]; //allow, block, digital, enc, etc

                //if we are using allow/whitelist mode, then write 'B' to mode for block
                //comparison below will look for an 'A' to write to mode if it is allowed
                if (opts->trunk_use_allow_list == 1) sprintf(mode, "%s", "B");

                for (int i = 0; i < state->group_tally; i++) {
                    if (state->group_array[i].groupNumber == grpAddr) {
                        fprintf(stderr, " [%s]", state->group_array[i].groupName);
                        strcpy(mode, state->group_array[i].groupMode);
                    }
                }

                SKIPCON:; //do nothing

            }

            fprintf(stderr, "%s", KNRM);

        } //end Connect+

        //Hytera XPT -- Experimental, but values now seem very consistent and trunking is working on systems tested with
        if (csbk_fid == 0x68) {
            //XPT Site Status
            if (csbk_o == 0x0A) {
                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, "%s", KYEL);

                uint8_t xpt_ch[6]; //one tg/call per LSN
                uint16_t tg = 0;  //8-bit TG value or TGT hash
                int i, j;

                //tg and channel info for trunking purposes
                uint8_t t_tg[6];
                memset(t_tg, 0, sizeof(t_tg));

                uint8_t xpt_seq = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[0], 2);  //this replaces the CSBK lb and pf
                uint8_t xpt_free = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[16], 4); //free repeater channel LCN
                uint8_t xpt_bank = 0;
                uint8_t xpt_lcn = 1;

                //determine starting position of LCN value
                if (xpt_seq == 1) xpt_lcn = 4;
                if (xpt_seq == 2) xpt_lcn = 7;

                //determine xpt_bank value for LSN value/tuning
                if (xpt_seq) xpt_bank = xpt_seq * 6;

                //get 2-bit status values for each 6 LSNs
                for (i = 0; i < 6; i++) xpt_ch[i] = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[20 + (i * 2)], 2);

                fprintf(stderr, " Hytera XPT Site Status - Free LCN: %d SN: %d", xpt_free, xpt_seq);

                //strings for active channels
                char xpt_active[20];
                if (xpt_seq == 0) sprintf(state->active_channel[0], "XPT "); //add initial if sequence 0
                else sprintf(state->active_channel[xpt_seq], "%s", ""); //blank current sequence to re-write to
                state->last_active_time = time(NULL);

                //Print List of LCN with LSN Activity
                for (i = 0; i < 6; i++) {
                    //add LCN value for each LSN pair to help users differentiate between the two when seeing LCN on FLC
                    if (i == 0 || i == 2 || i == 4) {
                        fprintf(stderr, "\n LCN %d - ", xpt_lcn);
                        xpt_lcn++;
                    }

                    //LSN value here is logical slot, in flco, we get the logical channel (which is the repeater, does not include the slot value)
                    fprintf(stderr, "LSN %02d: ",
                            i + xpt_bank + 1); //swapped out xpt_bank for all (xpt_seq*6) to simplify things
                    tg = (uint16_t) ConvertBitIntoBytes(&cs_pdu_bits[i * 8 + 32], 8);
                    fprintf(stderr, "ST-%X", xpt_ch[i]); //status bits value 0,1,2, or 3
                    if (tg != 0) fprintf(stderr, " %03d;  ", tg);
                    else {
                        if (xpt_ch[i] == 3)
                            fprintf(stderr,
                                    " Null; "); //NULL on ST-3 indicates repeater is not active for the LSNs that would be covered by these bits
                        else if (xpt_ch[i] == 2)
                            fprintf(stderr,
                                    " Priv; "); //This seems to be used on both private data and private voice calls, but occassionally has a 0 TGT value
                        else if (xpt_ch[i] == 1)
                            fprintf(stderr, " Unk;  "); //the 01 value has not been observed as of yet
                        else if (xpt_ch[i] == 0) fprintf(stderr, " Idle; "); //Idle

                        //if Priv/Unk occurs, add t_tg value to it for tuning purposes
                        if (xpt_ch[i] == 2) {
                            if (opts->trunk_tune_private_calls == 1) t_tg[i] = 1;
                        }

                        //could cause erroneous tuning since we don't yet know what the unk value really is (never observed)
                        if (xpt_ch[i] == 1) {
                            // if (opts->trunk_tune_data_calls == 1) t_tg[i] = 1; //disabled
                        }

                    }

                    // if (i == 2) fprintf (stderr, "\n ");

                    //add values to trunking tg/channel potentials
                    if (tg != 0) t_tg[i + xpt_bank] = tg;

                    //concantenate string to active channels for ncurses display
                    if (tg != 0) {
                        if (xpt_ch[i] == 3) sprintf(xpt_active, "LSN:%d TG:%d; ", i + xpt_bank + 1, tg);
                        if (xpt_ch[i] == 2) sprintf(xpt_active, "LSN:%d PC:%d; ", i + xpt_bank + 1, tg);
                        //last two are unknown status but have an associated target value
                        if (xpt_ch[i] == 1) sprintf(xpt_active, "LSN:%d UK:%d; ", i + xpt_bank + 1, tg);
                        if (xpt_ch[i] == 0) sprintf(xpt_active, "LSN:%d UK:%d; ", i + xpt_bank + 1, tg);
                        strcat(state->active_channel[xpt_seq], xpt_active); //add string to active channel seq
                    }

                }

                //add string for ncurses terminal display
                sprintf(state->dmr_site_parms, "Free LCN - %d ", xpt_free);

                //assign to cc freq to follow during no sync
                long int ccfreq = 0;

                //Skip tuning calls if group calls are disabled
                if (opts->trunk_tune_group_calls == 0) goto SKIPXPT;

                //Test allowing a group in the white list to preempt a call in progress and tune to a white listed call
                if (opts->trunk_use_allow_list == 1) state->last_vc_sync_time = 0;

                //don't tune if vc on the current channel
                if ((time(NULL) - state->last_vc_sync_time) > 2) //parenthesis error fixed
                {
                    for (j = 0; j < 6; j++) //go through the channels stored looking for active ones to tune to
                    {
                        char mode[8]; //allow, block, digital, enc, etc

                        //if we are using allow/whitelist mode, then write 'B' to mode for block
                        //comparison below will look for an 'A' to write to mode if it is allowed
                        if (opts->trunk_use_allow_list == 1) sprintf(mode, "%s", "B");

                        //this won't work properly on hashed TGT values
                        //unless users load a TGT hash in a csv file
                        //and hope it doesn't clash with other normal TG values
                        for (int i = 0; i < state->group_tally; i++) {
                            if (state->group_array[i].groupNumber == t_tg[j + xpt_bank]) {
                                fprintf(stderr, " [%s]", state->group_array[i].groupName);
                                strcpy(mode, state->group_array[i].groupMode);
                            }
                        }

                    }
                } //end tuning

                SKIPXPT:;

                sprintf(state->dmr_branding_sub, "XPT ");

                //Notes: I had a few issues to fix in this CSBK, but it does appear that this is trunking on a few small systems now,
                //albeit a very quiet systems that makes it difficult to know for certain if every aspect is working correctly

                //Notes: I've set XPT to set a CC frequency to whichever frequency its tuned to currently and getting
                //this particular CSBK, if the status portion does work correctly, then it shouldn't matter which frequency it is on
                //as long as this CSBK comes in and we can tune to other repeater lcns if they have activity and the frequency mapping
                //is correct in the csv file, assuming the current frequency doesn't have voice activity

            } //end 0x0A

            //XPT Adjacent Site Information -- Have yet to find a consistent indication of 'current site' identification in any CSBK/FLC/SLC payload
            if (csbk_o == 0x0B) {

                //initial line break
                fprintf(stderr, "\n");
                fprintf(stderr, "%s", KYEL);

                int i;
                uint8_t xpt_site_id[4];
                uint8_t xpt_site_rp[4];
                uint8_t xpt_site_u1[4];
                uint8_t xpt_site_u2[4];

                uint8_t xpt_sn = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[0], 2);

                for (i = 0; i < 4; i++) {
                    xpt_site_id[i] = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[16 + (i * 16)], 5);
                    xpt_site_u1[i] = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[21 + (i * 16)], 3);
                    xpt_site_rp[i] = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[24 + (i * 16)], 4);
                    xpt_site_u2[i] = (uint8_t) ConvertBitIntoBytes(&cs_pdu_bits[28 + (i * 16)], 4);
                }

                fprintf(stderr, " Hytera XPT CSBK 0x0B - SN: %d", xpt_sn);
                fprintf(stderr, "\n");
                fprintf(stderr, " XPT Adjacent ");
                for (i = 0; i < 4; i++) {
                    if (xpt_site_id[i] != 0) {
                        fprintf(stderr, "Site:%d Free:%d; ", xpt_site_id[i], xpt_site_rp[i]);
                        // fprintf (stderr, "RS1: %d RS2: %d - ", xpt_site_u1[i], xpt_site_u2[i]); //debug
                    }
                }
                sprintf(state->dmr_branding_sub, "XPT ");

            } //end 0x0B

        } //end Hytera XPT section

    }
    END:
    fprintf(stderr, "%s", KNRM);

}