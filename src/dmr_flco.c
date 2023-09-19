/*-------------------------------------------------------------------------------
 * dmr_flco.c
 * DMR Full Link Control, Short Link Control, TACT/CACH and related funtions
 *
 * Portions of link control/voice burst/vlc/tlc from LouisErigHerve
 * Source: https://github.com/LouisErigHerve/dsd/blob/master/src/dmr_sync.c
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//combined flco handler (vlc, tlc, emb), minus the superfluous structs and strings
void dmr_flco(dsd_opts *opts, dsd_state *state, uint8_t lc_bits[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors,
              uint8_t type) {

    //force slot to 0 if using dmr mono handling
    if (opts->dmr_mono == 1) state->currentslot = 0;

    uint8_t pf = 0;
    uint8_t reserved = 0;
    uint8_t flco = 0;
    uint8_t fid = 0;
    uint8_t so = 0;
    uint32_t target = 0;
    uint32_t source = 0;
    uint8_t capsite = 0;
    int restchannel = -1;
    int is_cap_plus = 0;
    int is_alias = 0;
    int is_gps = 0;

    //XPT 'Things'
    int is_xpt = 0;
    uint8_t xpt_hand = 0;  //handshake
    uint8_t xpt_free = 0;  //free repeater
    uint8_t xpt_int = 0;  //xpt channel to interrupt (channel/repeater call should occur on?)
    uint8_t xpt_res_a = 0; //unknown values of other bits of the XPT LC
    uint8_t xpt_res_b = 0; //unknown values of other bits of the XPT LC
    uint8_t xpt_res_c = 0; //unknown values of other bits of the XPT LC
    uint8_t target_hash[24]; //for XPT (and others if desired, get the hash and compare against SLC or XPT Status CSBKs)
    uint8_t tg_hash = 0; //value of the hashed TG

    uint8_t slot = state->currentslot;
    uint8_t unk = 0; //flag for unknown FLCO + FID combo

    pf = (uint8_t) (lc_bits[0]); //Protect Flag -- Hytera XPT uses this to signify which TS the PDU is on
    reserved = (uint8_t) (lc_bits[1]); //Reserved -- Hytera XPT G/I bit; 0 - Individual; 1 - Group;
    flco = (uint8_t) ConvertBitIntoBytes(&lc_bits[2], 6); //Full Link Control Opcode
    fid = (uint8_t) ConvertBitIntoBytes(&lc_bits[8], 8); //Feature set ID (FID)
    so = (uint8_t) ConvertBitIntoBytes(&lc_bits[16], 8); //Service Options
    target = (uint32_t) ConvertBitIntoBytes(&lc_bits[24], 24); //Target or Talk Group
    source = (uint32_t) ConvertBitIntoBytes(&lc_bits[48], 24);

    //read ahead a little to get this for the xpt flag
    if (IrrecoverableErrors == 0 && flco == 0x09 && fid == 0x68) {
        sprintf(state->dmr_branding, "%s", "  Hytera");
        sprintf(state->dmr_branding_sub, "XPT ");
    }

    //look at the dmr_branding_sub for the XPT string
    //branding sub is set at CSBK(68-3A and 3B), SLCO 8, and here on 0x09
    if (strcmp(state->dmr_branding_sub, "XPT ") == 0) is_xpt = 1;

    //hytera XPT -- disable the pf flag, is used for TS value in some Hytera XPT PDUs
    if (is_xpt) pf = 0;

    //check protect flag
    if (pf == 1) {
        if (type == 1) fprintf(stderr, "%s \n", KRED);
        if (type == 2) fprintf(stderr, "%s \n", KRED);
        if (type == 3) fprintf(stderr, "%s", KRED);
        fprintf(stderr, " SLOT %d", state->currentslot + 1);
        fprintf(stderr, " Protected LC ");
        goto END_FLCO;
    }

    if (IrrecoverableErrors == 0) {

        if (slot == 0) state->dmr_flco = flco;
        else state->dmr_flcoR = flco;

        //FID 0x10 and FLCO 0x14, 0x15, 0x16 and 0x17 confirmed as Moto EMB Alias
        //Can probably assume FID 0x10 FLCO 0x18 is Moto EMB GPS -- to be tested
        if (fid == 0x10 && (flco == 0x14 || flco == 0x15 || flco == 0x16 || flco == 0x17 || flco == 0x18)) {
            flco = flco - 0x10;
            fid = 0;
        }

        //Embedded Talker Alias Header Only (format and len storage)
        if ((fid == 0 || fid == 0x68) && type == 3 && flco == 0x04) {
            dmr_embedded_alias_header(opts, state, lc_bits);
        }

        //Embedded Talker Alias Header (continuation) and Blocks
        if ((fid == 0 || fid == 0x68) && type == 3 && flco > 0x03 && flco < 0x08) {
            is_alias = 1;
            dmr_embedded_alias_blocks(opts, state, lc_bits);
        }

        //Embedded GPS
        if ((fid == 0 || fid == 0x68) && fid == 0 && type == 3 && flco == 0x08) {
            is_gps = 1;
            dmr_embedded_gps(opts, state, lc_bits);
        }

        //look for Cap+ on VLC header, then set source and/or rest channel appropriately
        if (type == 1 && fid == 0x10 && (flco == 0x04 || flco == 0x07)) //0x07 appears to be a cap+ txi private call
        {
            is_cap_plus = 1;
            capsite = (uint8_t) ConvertBitIntoBytes(&lc_bits[48], 4); //don't believe so
            restchannel = (int) ConvertBitIntoBytes(&lc_bits[52], 4); //
            source = (uint32_t) ConvertBitIntoBytes(&lc_bits[56], 16);
        }

        //Unknown CapMax/Moto Things
        if (fid == 0x10 && (flco == 0x08 || flco == 0x28)) {
            //NOTE: fid 0x10 and flco 0x08 (emb) produces a lot of 'zero' bytes
            //this has been observed to happen often on CapMax systems, so I believe it could be some CapMax 'thing'
            //Unknown Link Control - FLCO=0x08 FID=0x10 SVC=0xC1 or FLCO=0x08 FID=0x10 SVC=0xC0 <- probably no SVC bits in the lc
            //flco 0x28 has also been observed lately but the tg and src values don't match
            //another flco 0x10 does seem to match, so is probably capmax group call flco
            if (type == 1) fprintf(stderr, "%s \n", KCYN);
            if (type == 2) fprintf(stderr, "%s \n", KCYN);
            if (type == 3) fprintf(stderr, "%s", KCYN);
            fprintf(stderr, " SLOT %d", state->currentslot + 1);
            fprintf(stderr, " Motorola");
            unk = 1;
            goto END_FLCO;
        }

        //7.1.1.1 Terminator Data Link Control PDU - ETSI TS 102 361-3 V1.2.1 (2013-07)
        if (type == 2 && flco == 0x30) {
            fprintf(stderr, "%s \n", KRED);
            fprintf(stderr, " SLOT %d", state->currentslot + 1);
            fprintf(stderr, " Data Terminator (TD_LC) ");
            fprintf(stderr, "%s", KNRM);

            //reset data header format storage
            state->data_header_format[slot] = 7;
            //flag off data header validity
            state->data_header_valid[slot] = 0;
            //flag off conf data flag
            state->data_conf_data[slot] = 0;

            goto END_FLCO;
        }

        //Unknown Tait Things
        if (fid == 0x58) {
            //NOTE: fid 0x58 (tait) had a single flco 0x06 emb observed, but without the other blocks (4,5,7) for an alias
            //will need to observe this one, or just remove it from the list, going to isolate tait lc for now
            if (type == 1) fprintf(stderr, "%s \n", KCYN);
            if (type == 2) fprintf(stderr, "%s \n", KCYN);
            if (type == 3) fprintf(stderr, "%s", KCYN);
            fprintf(stderr, " SLOT %d", state->currentslot + 1);
            fprintf(stderr, " Tait");
            unk = 1;
            goto END_FLCO;
        }

        //look for any Hytera XPT system, adjust TG to 16-bit allocation
        //Groups use only 8 out of 16, but 16 always seems to be allocated
        //private calls use 16-bit target values hashed to 8-bit in the site status csbk
        //the TLC preceeds the VLC for a 'handshake' call setup in XPT

        //truncate if XPT is set
        if (is_xpt == 1) {
            target = (uint32_t) ConvertBitIntoBytes(&lc_bits[32], 16); //16-bit allocation
            source = (uint32_t) ConvertBitIntoBytes(&lc_bits[56], 16); //16-bit allocation

            //the crc8 hash is the value represented in the CSBK when dealing with private calls
            for (int i = 0; i < 16; i++) target_hash[i] = lc_bits[32 + i];
            tg_hash = crc8(target_hash, 16);
        }

        //XPT Call 'Grant/Alert' Setup Occurs in TLC with a flco 0x09
        if (fid == 0x68 && flco == 0x09) {
            //The CSBK always uses an 8-bit TG/TGT; The White Papers (user manuals) say 8-bit TG and 16-bit SRC addressing
            //private calls and indiv data calls use a hash of their 8 bit tgt values in the CSBK
            xpt_int = (uint8_t) ConvertBitIntoBytes(&lc_bits[16],
                                                    4); //This is consistent with an LCN value, not an LSN value
            xpt_free = (uint8_t) ConvertBitIntoBytes(&lc_bits[24], 4); //24 and 4 on 0x09
            xpt_hand = (uint8_t) ConvertBitIntoBytes(&lc_bits[28],
                                                     4); //handshake kind: 0 - ordinary; 1-2 Interrupts; 3-15 reserved;

            target = (uint32_t) ConvertBitIntoBytes(&lc_bits[32], 16); //16-bit allocation
            source = (uint32_t) ConvertBitIntoBytes(&lc_bits[56], 16); //16-bit allocation

            //the bits that are left behind
            xpt_res_a = (uint8_t) ConvertBitIntoBytes(&lc_bits[20],
                                                      4); //after the xpt_int channel, before the free repeater channel -- call being established = 7; call connected = 0; ??
            xpt_res_b = (uint8_t) ConvertBitIntoBytes(&lc_bits[48], 8); //where the first 8 bits of the SRC would be

            //the crc8 hash is the value represented in the CSBK when dealing with private calls
            for (int i = 0; i < 16; i++) target_hash[i] = lc_bits[32 + i];
            tg_hash = crc8(target_hash, 16);

            fprintf(stderr, "%s \n", KGRN);
            fprintf(stderr, " SLOT %d ", state->currentslot + 1);
            if (opts->payload == 1) fprintf(stderr, "FLCO=0x%02X FID=0x%02X ", flco, fid);
            fprintf(stderr, "TGT=%u SRC=%u ", target, source);

            fprintf(stderr, "Hytera XPT ");

            //Group ID ranges from 1 to 240; emergency group call ID ranges from 250 to 254; all call ID is 255.
            if (reserved == 1) {
                fprintf(stderr, "Group "); //according to observation
                if (target > 248 && target < 255) fprintf(stderr, "Emergency ");
                if (target == 255) fprintf(stderr, "All ");
            } else fprintf(stderr, "Private "); //according to observation
            fprintf(stderr, "Call Alert "); //Alert or Grant

            //reorganized all the 'extra' data to a second line and added extra verbosity
            if (opts->payload == 1) fprintf(stderr, "\n  ");
            if (opts->payload == 1) fprintf(stderr, "%s", KYEL);
            //only display the hashed tgt value if its a private call and not a group call
            if (reserved == 0 && opts->payload == 1) fprintf(stderr, "TGT Hash=%d; ", tg_hash);
            if (opts->payload == 1) fprintf(stderr, "HSK=%X; ", xpt_hand);
            //extra verbosity on handshake types found in the patent
            if (opts->payload == 1) {
                fprintf(stderr, "Handshake - ");
                if (xpt_hand == 0) fprintf(stderr, "Ordinary; ");
                else if (xpt_hand == 1) fprintf(stderr, "Callback/Alarm Interrupt; ");
                else if (xpt_hand == 2) fprintf(stderr, "Release Channel Interrupt; ");
                else fprintf(stderr, "Reserved; ");
            }

            // if (opts->payload == 1)
            // {
            //   if (xpt_res_a == 0) fprintf (stderr, "Call Connected; ");
            //   if (xpt_res_a == 1) fprintf (stderr, "Data Call Request; ");
            //   if (xpt_res_a == 7) fprintf (stderr, "Voice Call Request; ");
            //   if (xpt_res_a == 2) fprintf (stderr, "Unknown Status; ");
            // }

            //logical repeater channel, not the logical slot value in the CSBK
            if (opts->payload == 1)
                fprintf(stderr, "Call on LCN %d; ", xpt_int); //LCN channel call or 'interrupt' will occur on
            // if (opts->payload == 1) fprintf(stderr, "RS A[%01X]B[%02X]C[%02X]; ", xpt_res_a, xpt_res_b, xpt_res_c); //leftover bits
            if (opts->payload == 1) fprintf(stderr, "Free LCN %d; ", xpt_free); //current free repeater LCN channel
            fprintf(stderr, "%s ", KNRM);

            //add string for ncurses terminal display
            sprintf(state->dmr_site_parms, "Free LCN - %d ", xpt_free);

            is_xpt = 1;
            goto END_FLCO;
        }

        //Hytera XPT 'Others' -- moved the patent opcodes here as well for now
        if (fid == 0x68 && (flco == 0x13 || flco == 0x31 || flco == 0x2E || flco == 0x2F)) {
            if (type == 1) fprintf(stderr, "%s \n", KCYN);
            if (type == 2) fprintf(stderr, "%s \n", KCYN);
            if (type == 3) fprintf(stderr, "%s", KCYN);
            fprintf(stderr, " SLOT %d", state->currentslot + 1);
            fprintf(stderr, " Hytera ");
            unk = 1;
            goto END_FLCO;
        }

        //unknown other manufacturer or OTA ENC, etc.
        //removed tait from the list, added hytera 0x08
        if (fid != 0 && fid != 0x68 && fid != 0x10 && fid != 0x08) {
            if (type == 1) fprintf(stderr, "%s \n", KYEL);
            if (type == 2) fprintf(stderr, "%s \n", KYEL);
            if (type == 3) fprintf(stderr, "%s", KYEL);
            fprintf(stderr, " SLOT %d", state->currentslot + 1);
            fprintf(stderr, " Unknown LC ");
            unk = 1;
            goto END_FLCO;
        }

    }

    //will want to continue to observe for different flco and fid combinations to find out their meaning
    if (IrrecoverableErrors == 0 && is_alias == 0 && is_gps == 0) {
        //set overarching manufacturer in use when non-standard feature id set is up
        if (fid != 0) state->dmr_mfid = fid;

        if (type != 2) //VLC and EMB, set our target, source, so, and fid per channel
        {
            if (state->currentslot == 0) {
                state->dmr_fid = fid;
                state->dmr_so = so;
                state->lasttg = target;
                state->lastsrc = source;
            }
            if (state->currentslot == 1) {
                state->dmr_fidR = fid;
                state->dmr_soR = so;
                state->lasttgR = target;
                state->lastsrcR = source;
            }

            //update cc amd vc sync time for trunking purposes (particularly Con+)
            if (opts->p25_is_tuned == 1) {
                state->last_vc_sync_time = time(NULL);
                state->last_cc_sync_time = time(NULL);
            }

        }

        if (type == 2) //TLC, zero out target, source, so, and fid per channel, and other odd and ends
        {
            //I wonder which of these we truly want to zero out, possibly none of them
            if (state->currentslot == 0) {
                state->dmr_fid = 0;
                state->dmr_so = 0;
                // state->lasttg = 0;
                // state->lastsrc = 0;
                state->payload_algid = 0;
                state->payload_mi = 0;
                state->payload_keyid = 0;
            }
            if (state->currentslot == 1) {
                state->dmr_fidR = 0;
                state->dmr_soR = 0;
                // state->lasttgR = 0;
                // state->lastsrcR = 0;
                state->payload_algidR = 0;
                state->payload_miR = 0;
                state->payload_keyidR = 0;
            }

        }


        if (restchannel != state->dmr_rest_channel && restchannel != -1) {
            state->dmr_rest_channel = restchannel;
            //assign to cc freq
            if (state->trunk_chan_map[restchannel] != 0) {
                state->p25_cc_freq = state->trunk_chan_map[restchannel];
            }
        }

        if (type == 1) fprintf(stderr, "%s \n", KGRN);
        if (type == 2) fprintf(stderr, "%s \n", KRED);
        if (type == 3) fprintf(stderr, "%s", KGRN);

        fprintf(stderr, " SLOT %d ", state->currentslot + 1);
        fprintf(stderr, "TGT=%u SRC=%u ", target, source);
        if (opts->payload == 1 && is_xpt == 1 && flco == 0x3) fprintf(stderr, "HASH=%d ", tg_hash);
        if (opts->payload == 1) fprintf(stderr, "FLCO=0x%02X FID=0x%02X SVC=0x%02X ", flco, fid, so);

        //0x04 and 0x05 on a TLC seem to indicate a Cap + Private Call Terminator (perhaps one for each MS)
        //0x07 on a VLC seems to indicate a Cap+ Private Call Header
        //0x23 on the Embedded Voice Burst Sync seems to indicate a Cap+ or Cap+ TXI Private Call in progress
        //0x20 on the Embedded Voice Burst Sync seems to indicate a Moto (non-specific) Group Call in progress
        //its possible that both EMB FID 0x10 FLCO 0x20 and 0x23 are just Moto but non-specific (observed 0x20 on Tier 2)

        if (fid == 0x68) sprintf(state->call_string[slot], " Hytera  ");

        else if (flco == 0x4 || flco == 0x5 || flco == 0x7 || flco == 0x23) //Cap+ Things
        {
            sprintf(state->call_string[slot], " Cap+");
            fprintf(stderr, "Cap+ ");
            if (flco == 0x4) {
                strcat(state->call_string[slot], " Grp");
                fprintf(stderr, "Group ");
            } else {
                strcat(state->call_string[slot], " Pri");
                fprintf(stderr, "Private ");
            }
        } else if (flco == 0x3) //UU_V_Ch_Usr
        {
            sprintf(state->call_string[slot], " Private ");
            fprintf(stderr, "Private ");
        } else //Grp_V_Ch_Usr -- still valid on hytera VLC
        {
            sprintf(state->call_string[slot], "   Group ");
            fprintf(stderr, "Group ");
        }

        if (so & 0x80) {
            strcat(state->call_string[slot], " Emergency  ");
            fprintf(stderr, "%s", KRED);
            fprintf(stderr, "Emergency ");
        } else strcat(state->call_string[slot], "            ");

        if (so & 0x40) {
            //REMUS! Uncomment Line Below if desired
            // strcat (state->call_string[slot], " Encrypted");
            fprintf(stderr, "%s", KRED);
            fprintf(stderr, "Encrypted ");
        }
        //REMUS! Uncomment Line Below if desired
        // else strcat (state->call_string[slot], "          ");

        //Motorola FID 0x10 Only
        if (fid == 0x10) {
            /* Check the "Service Option" bits */
            if (so & 0x20) {
                //REMUS! Uncomment Line Below if desired
                // strcat (state->call_string[slot], " TXI");
                fprintf(stderr, "TXI ");
            }
            if (so & 0x10) {
                //REMUS! Uncomment Line Below if desired
                // strcat (state->call_string[slot], " RPT");
                fprintf(stderr,
                        "RPT "); //Short way of saying the next SF's VC6 will be pre-empted/repeat frames for the TXI backwards channel
            }
            if (so & 0x08) {
                //REMUS! Uncomment Line Below if desired
                // strcat (state->call_string[slot], "-BC   ");
                fprintf(stderr, "Broadcast ");
            }
            if (so & 0x04) {
                //REMUS! Uncomment Line Below if desired
                // strcat (state->call_string[slot], "-OVCM ");
                fprintf(stderr, "OVCM ");
            }
            if (so & 0x03) {
                if ((so & 0x03) == 0x01) {
                    //REMUS! Uncomment Line Below if desired
                    // strcat (state->call_string[slot], "-P1");
                    fprintf(stderr, "Priority 1 ");
                } else if ((so & 0x03) == 0x02) {
                    //REMUS! Uncomment Line Below if desired
                    // strcat (state->call_string[slot], "-P2");
                    fprintf(stderr, "Priority 2 ");
                } else if ((so & 0x03) == 0x03) {
                    //REMUS! Uncomment Line Below if desired
                    // strcat (state->call_string[slot], "-P3");
                    fprintf(stderr, "Priority 3 ");
                } else /* We should never go here */
                {
                    //REMUS! Uncomment Line Below if desired
                    // strcat (state->call_string[slot], "  ");
                    fprintf(stderr, "No Priority ");
                }
            }
        }

        //should rework this back into the upper portion
        if (fid == 0x68) fprintf(stderr, "Hytera ");
        if (is_xpt) fprintf(stderr, "XPT ");
        if (fid == 0x68 && flco == 0x00) fprintf(stderr, "Group ");
        if (fid == 0x68 && flco == 0x03) fprintf(stderr, "Private ");

        fprintf(stderr, "Call ");


        //check Cap+ rest channel info if available and good fec
        if (is_cap_plus == 1) {
            if (restchannel != -1) {
                fprintf(stderr, "%s ", KYEL);
                fprintf(stderr, "Rest LSN: %d", restchannel);
            }
        }

        fprintf(stderr, "%s ", KNRM);

        //group labels
        for (int i = 0; i < state->group_tally; i++) {
            //Remus! Change target to source if you prefer
            if (state->group_array[i].groupNumber == target) {
                fprintf(stderr, "%s", KCYN);
                fprintf(stderr, "[%s] ", state->group_array[i].groupName);
                fprintf(stderr, "%s", KNRM);
            }
        }

        //BUGFIX: Include slot and algid so we don't accidentally print more than one loaded key
        //this can happen on Missing PI header and LE when the keyloader has loaded a TG/Hash key and an RC4 key simultandeously
        //subsequennt EMB would print two key values until call cleared out

        if (state->K != 0 && fid == 0x10 && so & 0x40 && slot == 0 && state->payload_algid == 0) {
            fprintf(stderr, "%s", KYEL);
            fprintf(stderr, "Key %lld ", state->K);
            fprintf(stderr, "%s ", KNRM);
        }

        if (state->K != 0 && fid == 0x10 && so & 0x40 && slot == 1 && state->payload_algidR == 0) {
            fprintf(stderr, "%s", KYEL);
            fprintf(stderr, "Key %lld ", state->K);
            fprintf(stderr, "%s ", KNRM);
        }

        if (state->K1 != 0 && fid == 0x68 && so & 0x40 && slot == 0 && state->payload_algid == 0) {
            if (state->K2 != 0) fprintf(stderr, "\n ");
            fprintf(stderr, "%s", KYEL);
            fprintf(stderr, "Key %010llX ", state->K1);
            if (state->K2 != 0) fprintf(stderr, "%016llX ", state->K2);
            if (state->K4 != 0) fprintf(stderr, "%016llX %016llX", state->K3, state->K4);
            fprintf(stderr, "%s ", KNRM);
        }

        if (state->K1 != 0 && fid == 0x68 && so & 0x40 && slot == 1 && state->payload_algidR == 0) {
            if (state->K2 != 0) fprintf(stderr, "\n ");
            fprintf(stderr, "%s", KYEL);
            fprintf(stderr, "Key %010llX ", state->K1);
            if (state->K2 != 0) fprintf(stderr, "%016llX ", state->K2);
            if (state->K4 != 0) fprintf(stderr, "%016llX %016llX", state->K3, state->K4);
            fprintf(stderr, "%s ", KNRM);
        }

        if (slot == 0 && state->payload_algid == 0x21 && state->R != 0) {
            fprintf(stderr, "%s", KYEL);
            fprintf(stderr, "Key %010llX ", state->R);
            fprintf(stderr, "%s ", KNRM);
        }

        if (slot == 1 && state->payload_algidR == 0x21 && state->RR != 0) {
            fprintf(stderr, "%s", KYEL);
            fprintf(stderr, "Key %010llX ", state->RR);
            fprintf(stderr, "%s ", KNRM);
        }

    }

    END_FLCO:

    //blank the call string here if its a TLC
    if (type == 2) sprintf(state->call_string[slot], "%s", "                     "); //21 spaces

    if (unk == 1 || pf == 1) {
        fprintf(stderr, " FLCO=0x%02X FID=0x%02X ", flco, fid);
        fprintf(stderr, "%s", KNRM);
    }

    if (IrrecoverableErrors != 0) {
        if (type != 3) fprintf(stderr, "\n");
        fprintf(stderr, "%s", KRED);
        fprintf(stderr, " SLOT %d", state->currentslot + 1);
        fprintf(stderr, " FLCO FEC ERR ");
        fprintf(stderr, "%s", KNRM);
    }


}

//externalize embedded alias to keep the flco function relatively clean
void dmr_embedded_alias_header(dsd_opts *opts, dsd_state *state, uint8_t lc_bits[]) {

    uint8_t slot = state->currentslot;
    uint8_t pf;
    uint8_t res;
    uint8_t format = (uint8_t) ConvertBitIntoBytes(&lc_bits[16], 2);
    uint8_t len;

    //this len seems to pertain to number of blocks? not bit len.
    //len = (uint8_t)ConvertBitIntoBytes(&lc_bits[18], 5);

    if (format == 0) len = 7;
    else if (format == 1 || format == 2) len = 8;
    else len = 16;

    state->dmr_alias_format[slot] = format;
    state->dmr_alias_len[slot] = len;

    //fprintf (stderr, "F: %d L: %d - ", format, len);

}

void dmr_embedded_alias_blocks(dsd_opts *opts, dsd_state *state, uint8_t lc_bits[]) {
    fprintf(stderr, "%s", KYEL);
    fprintf(stderr, " Embedded Alias: ");
    uint8_t slot = state->currentslot;
    uint8_t block = (uint8_t) ConvertBitIntoBytes(&lc_bits[2], 6); //FLCO equals block number
    uint8_t format = state->dmr_alias_format[slot]; //0=7-bit; 1=ISO8; 2=UTF-8; 3=UTF16BE
    uint8_t len = state->dmr_alias_len[slot];
    uint8_t start; //starting position depends on context of block and format

    //Cap Max Variation
    uint8_t fid = (uint8_t) ConvertBitIntoBytes(&lc_bits[8], 8);
    if (fid == 0x10) block = block - 0x10; //CapMax adds 0x10 to its FLCO (block num) in its Embedded Aliasing

    //there is some issue with the next three lines of code that prevent proper assignments, not sure what.
    //if (block > 4) start = 16;
    //else if (block == 4 && format > 0) start = 23; //8-bit and 16-bit chars
    //else start = 24;

    //forcing start to 16 make it work on 8-bit alias, len seems okay when set off of format
    start = 16;
    len = 8;

    // fprintf (stderr, "block: %d start: %d len: %d ", block, start, len);

    //all may not be used depending on format, len, start.
    uint16_t A0, A1, A2, A3, A4, A5, A6;

    //sanity check
    if (block > 7) block = 4; //prevent oob array (although we should never get here)

    if (len > 6) //if not greater than zero, then the header hasn't arrived yet
    {
        A0 = 0;
        A1 = 0;
        A2 = 0;
        A3 = 0;
        A4 = 0;
        A5 = 0;
        A6 = 0; //NULL ASCII Characters
        A0 = (uint16_t) ConvertBitIntoBytes(&lc_bits[start + len * 0], len);
        A1 = (uint16_t) ConvertBitIntoBytes(&lc_bits[start + len * 1], len);
        A2 = (uint16_t) ConvertBitIntoBytes(&lc_bits[start + len * 2], len);
        A3 = (uint16_t) ConvertBitIntoBytes(&lc_bits[start + len * 3], len);
        A4 = (uint16_t) ConvertBitIntoBytes(&lc_bits[start + len * 4], len);
        A5 = (uint16_t) ConvertBitIntoBytes(&lc_bits[start + len * 5], len);
        A6 = (uint16_t) ConvertBitIntoBytes(&lc_bits[start + len * 6], len);

        //just going to assign the usual ascii set here to prevent 'naughty' characters, sans diacriticals
        if (A0 > 0x19 && A0 < 0x7F) sprintf(state->dmr_alias_block_segment[slot][block - 4][0], "%c", A0);
        if (A1 > 0x19 && A1 < 0x7F) sprintf(state->dmr_alias_block_segment[slot][block - 4][1], "%c", A1);
        if (A2 > 0x19 && A2 < 0x7F) sprintf(state->dmr_alias_block_segment[slot][block - 4][2], "%c", A2);
        if (A3 > 0x19 && A3 < 0x7F) sprintf(state->dmr_alias_block_segment[slot][block - 4][3], "%c", A3);
        if (A4 > 0x19 && A4 < 0x7F) sprintf(state->dmr_alias_block_segment[slot][block - 4][4], "%c", A4);
        if (A5 > 0x19 && A5 < 0x7F) sprintf(state->dmr_alias_block_segment[slot][block - 4][5], "%c", A5);
        if (A6 > 0x19 && A6 < 0x7F) sprintf(state->dmr_alias_block_segment[slot][block - 4][6], "%c", A6);

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 7; j++) {
                fprintf(stderr, "%s", state->dmr_alias_block_segment[slot][i][j]);
            }
        }


    } else fprintf(stderr, "Missing Header Block Format and Len Data");
    fprintf(stderr, "%s", KNRM);

}

//externalize embedded GPS - Needs samples to test/fix function
void dmr_embedded_gps(dsd_opts *opts, dsd_state *state, uint8_t lc_bits[]) {
    fprintf(stderr, "%s", KYEL);
    fprintf(stderr, " Embedded GPS:");
    uint8_t slot = state->currentslot;
    uint8_t pf = lc_bits[0];
    uint8_t res_a = lc_bits[1];
    uint8_t res_b = (uint8_t) ConvertBitIntoBytes(&lc_bits[16], 4);
    uint8_t pos_err = (uint8_t) ConvertBitIntoBytes(&lc_bits[20], 3);

    uint32_t lon_sign = lc_bits[23];
    uint32_t lon = (uint32_t) ConvertBitIntoBytes(&lc_bits[24], 24);
    uint32_t lat_sign = lc_bits[48];
    uint32_t lat = (uint32_t) ConvertBitIntoBytes(&lc_bits[49], 23);

    double lat_unit = (double) 180 / pow(2.0, 24); //180 divided by 2^24
    double lon_unit = (double) 360 / pow(2.0, 25); //360 divided by 2^25

    char latstr[3];
    char lonstr[3];
    sprintf(latstr, "%s", "N");
    sprintf(lonstr, "%s", "E");

    //run calculations and print (still cannot verify as accurate)
    //7.2.16 and 7.2.17 (two's compliment)

    double latitude = 0;
    double longitude = 0;

    if (pf) fprintf(stderr, " Protected");
    else {
        if (lat_sign) {
            lat = 0x800001 - lat;
            sprintf(latstr, "%s", "S");
        }
        latitude = ((double) lat * lat_unit);

        if (lon_sign) {
            lon = 0x1000001 - lon;
            sprintf(lonstr, "%s", "W");
        }
        longitude = ((double) lon * lon_unit);

        //sanity check
        if (abs(latitude) < 90 && abs(longitude) < 180) {
            fprintf(stderr, " Lat: %.5lf %s Lon: %.5lf %s", latitude, latstr, longitude, lonstr);

            //7.2.15 Position Error
            uint16_t position_error = 2 * pow(10, pos_err); //2 * 10^pos_err
            if (pos_err == 0x7) fprintf(stderr, " Position Error: Unknown or Invalid");
            else fprintf(stderr, " Position Error: Less than %dm", position_error);

            //save to array for ncurses
            if (pos_err != 0x7) {
                sprintf(state->dmr_embedded_gps[slot], "E-GPS (%lf %s %lf %s) Err: %dm", latitude, latstr, longitude,
                        lonstr, position_error);
            }

        }

    }

    fprintf(stderr, "%s", KNRM);
}
