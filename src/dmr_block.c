/*-------------------------------------------------------------------------------
 * dmr_block.c
 * DMR Data Header and Data Block Assembly/Handling
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//hopefully a more simplified (or logical) version...once you get past all the variables
void dmr_dheader(dsd_opts *opts, dsd_state *state, uint8_t dheader[], uint8_t dheader_bits[], uint32_t CRCCorrect,
                 uint32_t IrrecoverableErrors) {

    uint32_t i, j, k;
    uint8_t slot = state->currentslot;

    //clear out unified pdu 'superframe' slot
    for (short int i = 0; i < 288; i++) state->dmr_pdu_sf[slot][i] = 0;

    //reset block counter to 1
    state->data_block_counter[slot] = 1;

    if (IrrecoverableErrors == 0 && CRCCorrect == 1) //&&CRCCorrect == 1
    {

        uint8_t gi = dheader_bits[0]; //group or individual data
        uint8_t a = dheader_bits[1]; //response requested flag
        uint8_t ab = (uint8_t) ConvertBitIntoBytes(&dheader_bits[2], 2); //appended blocks
        uint8_t dpf = (uint8_t) ConvertBitIntoBytes(&dheader_bits[4], 4); //data packet format
        uint8_t sap = (uint8_t) ConvertBitIntoBytes(&dheader_bits[8], 4); //service access point
        uint8_t poc = (uint8_t) ConvertBitIntoBytes(&dheader_bits[8], 4); //padding octets

        uint32_t target = (uint32_t) ConvertBitIntoBytes(&dheader_bits[16], 24); //destination llid
        uint32_t source = (uint32_t) ConvertBitIntoBytes(&dheader_bits[40], 24); //source llid

        //extra tgt/src handling for XPT
        uint8_t target_hash[24];
        uint8_t tg_hash = 0;
        uint8_t is_xpt = 0;
        //set flag if XPT
        if (strcmp(state->dmr_branding_sub, "XPT ") == 0) is_xpt = 1;

        //truncate tgt/src to 16-bit values if XPT
        if (is_xpt == 1) {
            target = (uint32_t) ConvertBitIntoBytes(&dheader_bits[24], 16); //destination llid
            source = (uint32_t) ConvertBitIntoBytes(&dheader_bits[48], 16); //source llid
            if (gi == 0) {
                for (int i = 0; i < 16; i++) target_hash[i] = dheader_bits[24 + i];
                tg_hash = crc8(target_hash, 16);
            }
        }

        uint8_t is_cap = 0;
        if (strcmp(state->dmr_branding_sub, "Cap+ ") == 0) is_cap = 1;

        if (is_cap) {
            //truncate tg on group? or just on private/individual data?
            if (gi == 0) target = (uint32_t) ConvertBitIntoBytes(&dheader_bits[24], 16);
            source = (uint32_t) ConvertBitIntoBytes(&dheader_bits[48], 16);
        }

        //store source for dmr pdu packet handling (lrrp) when not available in completed message
        if (dpf != 15) state->dmr_lrrp_source[slot] = source;

        //set dpf to storage for later use (UDT, SD, etc)
        state->data_header_format[slot] = dpf;

        //Type Strings
        char sap_string[20];
        char mfid_string[20];
        char udtf_string[20];
        char sddd_string[20];
        sprintf(sap_string, "%s", "");
        sprintf(mfid_string, "%s", "");
        sprintf(udtf_string, "%s", "");
        sprintf(sddd_string, "%s", "");

        //see 9.3 - ETSI TS 102 361-1 V2.5.1 (2017-10) for more info
        uint8_t f = dheader_bits[64]; //F -- Full message flag (F)
        uint8_t bf = (uint8_t) ConvertBitIntoBytes(&dheader_bits[65], 7); //Blocks to Follow (BF)

        //confirmed data header
        uint8_t s = dheader_bits[72]; //S -- Re-Synchronize Flag
        uint8_t ns = (uint8_t) ConvertBitIntoBytes(&dheader_bits[73], 3); //N(S) -- send sequence number
        uint8_t fsn = (uint8_t) ConvertBitIntoBytes(&dheader_bits[76], 4); //Fragment Sequence Number (FSN)

        //response header
        uint8_t r_class = (uint8_t) ConvertBitIntoBytes(&dheader_bits[72], 2);
        uint8_t r_type = (uint8_t) ConvertBitIntoBytes(&dheader_bits[74], 3);
        uint8_t r_status = (uint8_t) ConvertBitIntoBytes(&dheader_bits[77], 3);

        //short data - status/precoded
        uint8_t s_ab_msb = (uint8_t) ConvertBitIntoBytes(&dheader_bits[2], 2); //appended block, msb
        uint8_t s_ab_lsb = (uint8_t) ConvertBitIntoBytes(&dheader_bits[12], 4);//appended block, lsb
        uint8_t s_ab_fin = (s_ab_msb << 2) | s_ab_lsb; //appended blocks, final value
        uint8_t s_source_port = (uint8_t) ConvertBitIntoBytes(&dheader_bits[64], 3);
        uint8_t s_dest_port = (uint8_t) ConvertBitIntoBytes(&dheader_bits[67], 3);
        uint8_t s_status_precoded = (uint8_t) ConvertBitIntoBytes(&dheader_bits[70], 10);

        //short data - raw
        uint8_t sd_sarq = dheader_bits[70]; //Selective Automatic Repeat reQuest
        uint8_t sd_f = dheader_bits[71];   //full message flag
        uint8_t sd_bp = (uint8_t) ConvertBitIntoBytes(&dheader_bits[72], 8); //bit padding

        //short data - defined
        uint8_t dd_format = (uint8_t) ConvertBitIntoBytes(&dheader_bits[64], 6);

        //Unified Data Transport (UDT)
        uint8_t udt_format = (uint8_t) ConvertBitIntoBytes(&dheader_bits[12], 4);
        uint8_t udt_padnib = (uint8_t) ConvertBitIntoBytes(&dheader_bits[64], 5);
        uint8_t udt_uab = (uint8_t) ConvertBitIntoBytes(&dheader_bits[70], 2); //udt appended blocks
        uint8_t udt_sf = dheader_bits[72];
        uint8_t udt_pf = dheader_bits[73];
        uint8_t udt_op = (uint8_t) ConvertBitIntoBytes(&dheader_bits[74], 6);

        //p_head
        uint8_t p_sap = (uint8_t) ConvertBitIntoBytes(&dheader_bits[0], 4);
        uint8_t p_mfid = (uint8_t) ConvertBitIntoBytes(&dheader_bits[8], 8);
        uint8_t p_pot = (uint8_t) ConvertBitIntoBytes(&dheader_bits[3], 5); //looking for LRRP pot_report

        fprintf(stderr, "%s ", KGRN);
        fprintf(stderr, "\n");
        fprintf(stderr, " Slot %d Data Header - ", slot + 1);

        if (gi == 1 && dpf != 15) fprintf(stderr, "Group - ");
        if (gi == 0 && dpf != 15) fprintf(stderr, "Indiv - ");

        if (dpf == 0) fprintf(stderr, "Unified Data Transport (UDT) ");
        else if (dpf == 1) fprintf(stderr, "Response Packet ");
        else if (dpf == 2) fprintf(stderr, "Unconfirmed Delivery ");
        else if (dpf == 3) fprintf(stderr, "Confirmed Delivery ");
        else if (dpf == 13) fprintf(stderr, "Short Data: Defined ");
        else if (dpf == 14) fprintf(stderr, "Short Data: Raw or S/P ");
        else if (dpf == 15) fprintf(stderr, "Proprietary Packet Data");
        else fprintf(stderr, "Reserved/Unknown DPF %X ", dpf);

        if (a == 1 && dpf != 15) fprintf(stderr, "- Response Requested ");
        if (dpf != 15) fprintf(stderr, "- Source: %d Target: %d ", source, target);

        //include the hash value if this is an XPT and if its IND data
        if (dpf != 15 && is_xpt == 1 && gi == 0) fprintf(stderr, "Hash: %d ", tg_hash);

        //sap string handling
        if (dpf == 15) sap = p_sap;

        if (sap == 0) sprintf(sap_string, "%s", "UDT Data"); //apparently, both dpf and sap for UDT is 0
        else if (sap == 2) sprintf(sap_string, "%s", "TCP Comp"); //TCP/IP header compression
        else if (sap == 3) sprintf(sap_string, "%s", "UDP Comp"); //UDP/IP header compression
        else if (sap == 4) sprintf(sap_string, "%s", "IP Based"); //IP based Packet Data
        else if (sap == 5) sprintf(sap_string, "%s", "ARP Prot"); //Address Resoution Protocol (ARP)
        else if (sap == 9) sprintf(sap_string, "%s", "Prop PDU"); //Proprietary Packet Data
        else if (sap == 10) sprintf(sap_string, "%s", "Short DT"); //Short Data
        else sprintf(sap_string, "%s", "Reserved"); //reserved, or err/unk

        //mfid string handling
        if (dpf == 15) {
            if (p_mfid == 0x10) sprintf(mfid_string, "%s", "Motorola");
            else if (p_mfid == 0x58) sprintf(mfid_string, "%s", "Tait");
            else if (p_mfid == 0x68) sprintf(mfid_string, "%s", "Hytera");
            else if (p_mfid == 0x08) sprintf(mfid_string, "%s", "Hytera");
            else if (p_mfid == 0x06) sprintf(mfid_string, "%s", "Trid/Mot");
            else if (p_mfid == 0x00) sprintf(mfid_string, "%s", "Standard");
            else sprintf(mfid_string, "%s", "Other");
        }

        //udt format string handling
        if (dpf == 0) {
            if (udt_format == 0x00) sprintf(udtf_string, "%s", "Binary");
            else if (udt_format == 0x01) sprintf(udtf_string, "%s", "MS/TG Adr");
            else if (udt_format == 0x02) sprintf(udtf_string, "%s", "4-bit BCD");
            else if (udt_format == 0x03) sprintf(udtf_string, "%s", "ISO7 Char");
            else if (udt_format == 0x04) sprintf(udtf_string, "%s", "ISO8 Char");
            else if (udt_format == 0x05) sprintf(udtf_string, "%s", "NMEA LOCN");
            else if (udt_format == 0x06) sprintf(udtf_string, "%s", "IP Addr");
            else if (udt_format == 0x07) sprintf(udtf_string, "%s", "UTF-16");    //16-bit Unicode Chars
            else if (udt_format == 0x08) sprintf(udtf_string, "%s", "Manu Spec"); //Manufacturer Specific
            else if (udt_format == 0x09) sprintf(udtf_string, "%s", "Manu Spec"); //Manufacturer Specific
            else if (udt_format == 0x0A)
                sprintf(udtf_string, "%s", "Mixed UTF"); //Appended block contains addr and 16-bit UTF-16BE
            else sprintf(udtf_string, "%s", "Reserved");
        }

        //short data dd_head format string
        if (dpf == 13) {
            if (dd_format == 0x00) sprintf(sddd_string, "%s", "Binary");
            else if (dd_format == 0x01) sprintf(sddd_string, "%s", "BCD   ");
            else if (dd_format == 0x02) sprintf(sddd_string, "%s", "7-bit Char");
            else if (dd_format == 0x03) sprintf(sddd_string, "%s", "IEC 8859-1");
            else if (dd_format == 0x04) sprintf(sddd_string, "%s", "IEC 8859-2");
            else if (dd_format == 0x05) sprintf(sddd_string, "%s", "IEC 8859-3");
            else if (dd_format == 0x06) sprintf(sddd_string, "%s", "IEC 8859-4");
            else if (dd_format == 0x07) sprintf(sddd_string, "%s", "IEC 8859-5");
            else if (dd_format == 0x08) sprintf(sddd_string, "%s", "IEC 8859-6");
            else if (dd_format == 0x09) sprintf(sddd_string, "%s", "IEC 8859-7");
            else if (dd_format == 0x0A) sprintf(sddd_string, "%s", "IEC 8859-8");
            else if (dd_format == 0x0B) sprintf(sddd_string, "%s", "IEC 8859-9");
            else if (dd_format == 0x0C) sprintf(sddd_string, "%s", "IEC 8859-10");
            else if (dd_format == 0x0D) sprintf(sddd_string, "%s", "IEC 8859-11");
            else if (dd_format == 0x0E) sprintf(sddd_string, "%s", "IEC 8859-13"); //there is no 8059-12
            else if (dd_format == 0x0F) sprintf(sddd_string, "%s", "IEC 8859-14");
            else if (dd_format == 0x10) sprintf(sddd_string, "%s", "IEC 8859-15");
            else if (dd_format == 0x11) sprintf(sddd_string, "%s", "IEC 8859-16");
            else if (dd_format == 0x12) sprintf(sddd_string, "%s", "UTF-8   ");
            else if (dd_format == 0x13) sprintf(sddd_string, "%s", "UTF-16  ");
            else if (dd_format == 0x14) sprintf(sddd_string, "%s", "UTF-16BE");
            else if (dd_format == 0x15) sprintf(sddd_string, "%s", "UTF-16LE");
            else if (dd_format == 0x16) sprintf(sddd_string, "%s", "UTF-32  ");
            else if (dd_format == 0x17) sprintf(sddd_string, "%s", "UTF-32BE");
            else if (dd_format == 0x18) sprintf(sddd_string, "%s", "UTF-32LE");
            else sprintf(sddd_string, "%s", "Reserved");
        }

        if (dpf == 0) //UDT
        {
            //UDT packet info -- samples needed for testing
            //NOTE: This format's completed message has a CRC16 - like MBC - but has number of blocks (appended blocks) like R 1/2, etc.
            fprintf(stderr, "\n  SAP %02X [%s] - FMT %d [%s] - PDn %d - BLOCKS %d SF %d - PF %d OP %02X", sap,
                    sap_string, udt_format, udtf_string, udt_padnib, udt_uab, udt_sf, udt_pf, udt_op);

            //set number of blocks to follow (appended blocks) for block assembler
            state->data_header_blocks[slot] = udt_uab;

            //set data header to valid
            state->data_header_valid[slot] = 1;

            //send to assembler as type 3, rearrange into CSBK type PDU (to verify), and send to dmr_cspdu
            dmr_block_assembler(opts, state, dheader, 12, 0x0B, 3);

        }

        if (dpf == 1) //response data packet header
        {
            //mostly fleshed out response packet info
            fprintf(stderr, "\n  SAP %02X [%s] - Class %d - Type %0d - ", sap, sap_string, r_class, r_type);
            if (r_class == 0 && r_type == 1) fprintf(stderr, "ACK - Success");
            if (r_class == 1) {
                fprintf(stderr, "NACK - ");
                if (r_type == 0) fprintf(stderr, "Illegal Format");
                if (r_type == 1) fprintf(stderr, "Illegal Format");
                if (r_type == 2) fprintf(stderr, "Packet CRC ERR");
                if (r_type == 3) fprintf(stderr, "Memory Full");
                if (r_type == 4) fprintf(stderr, "FSN Out of Seq");
                if (r_type == 5) fprintf(stderr, "Undeliverable");
                if (r_type == 6) fprintf(stderr, "PKT Out of Seq");
                if (r_type == 7) fprintf(stderr, "Invalid User");
            }
            if (r_class == 2) fprintf(stderr, "SACK - Retry");
            if (r_status) fprintf(stderr, " - %d", r_status); //the object/value of the ack/nack/sack

        }

        //Confirmed or Unconfirmed Data Packets Header
        if (dpf == 2 || dpf == 3) {
            if (dpf == 2)
                fprintf(stderr, "\n  SAP %02X [%s] - FMF %d - BLOCKS %02d - PAD %02d - FSN %d", sap, sap_string, f, bf,
                        poc, fsn);
            if (dpf == 3)
                fprintf(stderr, "\n  SAP %02X [%s] - FMF %d - BLOCKS %02d - PAD %02d - S %d - NS %d - FSN %d", sap,
                        sap_string, f, bf, poc, s, ns, fsn);
            state->data_header_blocks[slot] = bf;
            if (dpf == 3)
                state->data_conf_data[slot] = 1; //set confirmed data delivery flag for additional CRC checks, block assembly, etc.

        }

        //Short Data DD_Head (13), and R_Head or SP_Head (14)
        if (dpf == 13 || dpf == 14) {
            //only set if not all zeroes
            if (s_ab_fin) state->data_header_blocks[slot] = s_ab_fin;

            //Short Data: Defined
            if (dpf == 13)
                fprintf(stderr, "\n  SD:D [DD_HEAD] - SAP %02d [%s] - BLOCKS %02d - DD %02X - PADb %d - FMT %02X [%s]",
                        sap, sap_string, s_ab_fin, dd_format, sd_bp, dd_format, sddd_string);
            //Short Data: Raw or S/P
            if (dpf == 14) {
                //S/P has all appended block bits set to zero -- any other way to tell difference?
                if (s_ab_fin == 0)
                    fprintf(stderr, "\n  SD:S/P [SP_HEAD] - SAP %02d [%s] - SP %02d - DP %02d - S/P %02X", sap,
                            sap_string, s_source_port, s_dest_port, s_status_precoded);

                    //Raw
                else
                    fprintf(stderr,
                            "\n  SD:RAW [R_HEAD] - SAP %02d [%s] - BLOCKS %02d - SP %02d - DP %02d - SARQ %d - FMF %d - PDb %d",
                            sap, sap_string, s_ab_fin, s_source_port, s_dest_port, sd_sarq, sd_f, sd_bp);
            }

            //6.2.2 The Response Requested (A) information element of the header shall be set to 0
            //for unconfirmed data and shall be set to 1 for confirmed data. DD_HEAD, R_HEAD, or SP_HEAD (double check)
            if (a == 1) {
                state->data_conf_data[slot] = 1;
                fprintf(stderr, " - Confirmed Data");
            }

        }

        //Proprietary Data Header
        if (dpf == 15) {
            fprintf(stderr, " - P_SAP %02d [%s] - MFID %02X [%s]", p_sap, sap_string, p_mfid, mfid_string);

            //look for LRRP potential report 0x1F (may be others as well)
            //this could just be a sap == 1, which is 'reserved', unsure (or maybe no actual correlation?)
            if (p_sap != 1)
                state->data_header_blocks[slot]--; //decrement the blocks to follow count in order to pass CRC32 and properly assemble blocks
            else //add block to start of the dmr_pdu_sf since it probably contains vital LRRP data (0x1F report) -- won't pass CRC32 either way
            {
                int blocks = state->data_header_blocks[slot] - 1;
                if (blocks < 1) blocks = 4; //safety value
                if (blocks > 9) blocks = 9;
                for (i = 0; i < 12; i++) state->dmr_pdu_sf[slot][i + (blocks * 12)] = dheader[i];
                state->data_block_counter[slot]++;
                //no matter how you assemble this, with or without the p_head block(s), there is ALWAYS a CRC32 Failure
                //even when it continues to pass on other SAP values, SAP 1 must have a different checksum?
                //Users trying to get this will want to enable -F to bypass the CRC32 failure

                //a recent discovery on a few systems and samples show the existence of multiple Prop Head Packets (how does that work)
            }

        }

        //block storage sanity
        if (state->data_header_blocks[slot] > 9) state->data_header_blocks[slot] = 9;
        //assuming we didn't receive the initial data header block on a p_head and then decremented it
        //3 or 4 seems to be the average safe value
        if (state->data_header_blocks[slot] < 1) state->data_header_blocks[slot] = 4;
        //set data header validity unless its a p_head (should be set prior, if received)
        if (dpf != 15) state->data_header_valid[slot] = 1;

        //REMUS! Uncomment these lines if desired
        if (dpf != 15 && dpf != 1) {
            sprintf(state->dmr_lrrp_gps[slot], "Data Call - %s SRC [%d] DST [%d] ", sap_string, source, target);
            if (a == 1) strcat(state->dmr_lrrp_gps[slot], "- RSP REQ ");
        }

        //REMUS! Uncomment these lines if desired -- if the data request data header is missing, though, this may keep accumulating
        // if (dpf == 1)
        // {
        //   strcat (state->dmr_lrrp_gps[slot], "- RSP ");
        //   if (r_class == 0) strcat (state->dmr_lrrp_gps[slot], "ACK - RX OK");
        //   if (r_class == 1) strcat (state->dmr_lrrp_gps[slot], "NACK - RX ERR");
        //   if (r_class == 2) strcat (state->dmr_lrrp_gps[slot], "SACK - RETRY");

        // }
        //End REMUS Data string

        //store SAP value
        state->data_header_sap[slot] = sap;

    } //End Irrecoverable Errors

    if (IrrecoverableErrors != 0) {
        state->data_header_valid[slot] = 0;
        sprintf(state->dmr_lrrp_gps[slot], "%s", "");
        state->data_p_head[slot] = 0;
        state->data_conf_data[slot] = 0;
        state->data_block_counter[slot] = 1;
        state->data_header_blocks[slot] = 1;
        state->data_header_format[slot] = 7;

    }

    fprintf(stderr, "%s", KNRM);

}

//convert UDT to a CSBK friendly PDU format
void dmr_udt_cspdu_converter(dsd_opts *opts, dsd_state *state, uint8_t block_bytes[], uint8_t block_bits[],
                             uint32_t CRCCorrect, uint32_t IrrecoverableErrors) {
    int i, j;

    //rearrange UDT data to a CSBK message format
    uint8_t cs_byte[48]; //12 x 4, 2 bit AB should mean only 3 total appended blocks, so 4 all together (minus rearrangement)
    memset(cs_byte, 0, sizeof(cs_byte));
    uint8_t cs_bits[48 * 8];
    memset(cs_bits, 0, sizeof(cs_bits));

    cs_byte[0] = (uint8_t) ConvertBitIntoBytes(&block_bits[73], 7); //lb, pf and opcode byte
    uint8_t udt_opcode = (uint8_t) ConvertBitIntoBytes(&block_bits[74], 6); //for observation testing
    cs_byte[1] = 0; //set mfid to zero for now (may use custom value if needed later)
    uint8_t udt_mfid = 0; //setting to zero

    //should be 34 total (maximum) after removing tailing CRC16 (initial and last block)
    for (i = 0; i < 34; i++) cs_byte[2 + i] = block_bytes[12 + i];

    //bytes to bits
    for (i = 0, j = 0; i < 36; i++, j += 8) //4 blocks max?
    {
        cs_bits[j + 0] = (cs_byte[i] >> 7) & 0x01;
        cs_bits[j + 1] = (cs_byte[i] >> 6) & 0x01;
        cs_bits[j + 2] = (cs_byte[i] >> 5) & 0x01;
        cs_bits[j + 3] = (cs_byte[i] >> 4) & 0x01;
        cs_bits[j + 4] = (cs_byte[i] >> 3) & 0x01;
        cs_bits[j + 5] = (cs_byte[i] >> 2) & 0x01;
        cs_bits[j + 6] = (cs_byte[i] >> 1) & 0x01;
        cs_bits[j + 7] = (cs_byte[i] >> 0) & 0x01;
    }

    dmr_cspdu(opts, state, cs_bits, cs_byte, CRCCorrect, IrrecoverableErrors);

}

//assemble the blocks as they come in, shuffle them into the unified dmr_pdu_sf
void dmr_block_assembler(dsd_opts *opts, dsd_state *state, uint8_t block_bytes[], uint8_t block_len, uint8_t databurst,
                         uint8_t type) {
    int i, j, k;
    uint8_t lb = 0; //mbc last block
    uint8_t pf = 0; //mbc protect flag

    uint8_t slot = state->currentslot;
    int blocks;
    uint8_t blockcounter = state->data_block_counter[slot];
    uint8_t block_num = state->data_header_blocks[slot];

    uint32_t CRCCorrect = 0;
    uint32_t CRCComputed = 0;
    uint32_t CRCExtracted = 0;
    uint32_t IrrecoverableErrors = 0;

    uint8_t dmr_pdu_sf_bits[8 * 24 * 50]; //give plenty of space so we don't go oob

    //MBC Header and Block CRC
    uint8_t mbc_crc_good[2]; //header and blocks crc pass/fail local storage
    memset(mbc_crc_good, 0, sizeof(mbc_crc_good)); //init on 0 - bad crc

    if (type == 1) blocks = state->data_header_blocks[slot] - 1;
    if (type == 2) blocks = state->data_block_counter[slot];

    //UDT Header and Block Check
    uint8_t is_udt = 0;
    if (type == 3) {
        blocks = state->data_block_counter[slot];
        is_udt = 1;
        type = 2;
    }

    //sanity check, setting block_len and block values to sane numbers in case of missing header, else could overload array (crash) or print out
    if (blocks < 1) blocks = 1; //changed blocks from uint8_t to int
    if (blocks > 9) blocks = 9;
    if (block_len == 0) block_len = 18;
    if (block_len > 24) block_len = 24;

    if (type == 1) {
        //type 1 data block, shuffle method
        for (j = 0; j < blocks; j++) {
            for (i = 0; i < block_len; i++) {
                state->dmr_pdu_sf[slot][i + block_len * j] = state->dmr_pdu_sf[slot][i + block_len * (j + 1)];
            }
        }

        for (i = 0; i < block_len; i++) {
            state->dmr_pdu_sf[slot][i + (blocks * block_len)] = block_bytes[i];
        }

        //time to send the completed 'superframe' to the DMR PDU message handler
        if (state->data_block_counter[slot] == state->data_header_blocks[slot] && state->data_header_valid[slot] == 1) {
            //CRC32 on completed messages
            for (i = 0, j = 0; i < (block_num * block_len); i++, j += 8) {
                dmr_pdu_sf_bits[j + 0] = (state->dmr_pdu_sf[slot][i] >> 7) & 0x01;
                dmr_pdu_sf_bits[j + 1] = (state->dmr_pdu_sf[slot][i] >> 6) & 0x01;
                dmr_pdu_sf_bits[j + 2] = (state->dmr_pdu_sf[slot][i] >> 5) & 0x01;
                dmr_pdu_sf_bits[j + 3] = (state->dmr_pdu_sf[slot][i] >> 4) & 0x01;
                dmr_pdu_sf_bits[j + 4] = (state->dmr_pdu_sf[slot][i] >> 3) & 0x01;
                dmr_pdu_sf_bits[j + 5] = (state->dmr_pdu_sf[slot][i] >> 2) & 0x01;
                dmr_pdu_sf_bits[j + 6] = (state->dmr_pdu_sf[slot][i] >> 1) & 0x01;
                dmr_pdu_sf_bits[j + 7] = (state->dmr_pdu_sf[slot][i] >> 0) & 0x01;

            }

            //sanity check to prevent a negative or zero block_num or sending a negative value into the bit array
            if (block_num < 1 || block_num == 0) block_num = 1;

            CRCExtracted = 0;
            //extract crc from last block, apply to completed 'superframe' minus header
            for (i = 0; i < 32; i++) {
                CRCExtracted = CRCExtracted << 1;
                CRCExtracted =
                        CRCExtracted | (uint32_t) (dmr_pdu_sf_bits[i + (block_len * 8) * block_num - 32] & 1); //96
            }

            //rearrage for ridiculously stupid CRC32 LSO/MSO ordering
            for (i = 0, j = 0; i < (block_num * block_len); i += 2, j += 16) {
                dmr_pdu_sf_bits[j + 0] = (state->dmr_pdu_sf[slot][i + 1] >> 7) & 0x01;
                dmr_pdu_sf_bits[j + 1] = (state->dmr_pdu_sf[slot][i + 1] >> 6) & 0x01;
                dmr_pdu_sf_bits[j + 2] = (state->dmr_pdu_sf[slot][i + 1] >> 5) & 0x01;
                dmr_pdu_sf_bits[j + 3] = (state->dmr_pdu_sf[slot][i + 1] >> 4) & 0x01;
                dmr_pdu_sf_bits[j + 4] = (state->dmr_pdu_sf[slot][i + 1] >> 3) & 0x01;
                dmr_pdu_sf_bits[j + 5] = (state->dmr_pdu_sf[slot][i + 1] >> 2) & 0x01;
                dmr_pdu_sf_bits[j + 6] = (state->dmr_pdu_sf[slot][i + 1] >> 1) & 0x01;
                dmr_pdu_sf_bits[j + 7] = (state->dmr_pdu_sf[slot][i + 1] >> 0) & 0x01;

                dmr_pdu_sf_bits[j + 8] = (state->dmr_pdu_sf[slot][i] >> 7) & 0x01;
                dmr_pdu_sf_bits[j + 9] = (state->dmr_pdu_sf[slot][i] >> 6) & 0x01;
                dmr_pdu_sf_bits[j + 10] = (state->dmr_pdu_sf[slot][i] >> 5) & 0x01;
                dmr_pdu_sf_bits[j + 11] = (state->dmr_pdu_sf[slot][i] >> 4) & 0x01;
                dmr_pdu_sf_bits[j + 12] = (state->dmr_pdu_sf[slot][i] >> 3) & 0x01;
                dmr_pdu_sf_bits[j + 13] = (state->dmr_pdu_sf[slot][i] >> 2) & 0x01;
                dmr_pdu_sf_bits[j + 14] = (state->dmr_pdu_sf[slot][i] >> 1) & 0x01;
                dmr_pdu_sf_bits[j + 15] = (state->dmr_pdu_sf[slot][i] >> 0) & 0x01;

                //increment extra 2 bytes on each nth byte (depends on data type 1/2, 3/4, 1) if conf data
                if (i == (block_len - 1)) {
                    if (state->data_conf_data[slot] == 0) i += 2;
                }
            }

            //confirmed working now!
            CRCComputed = (uint32_t) ComputeCrc32Bit(dmr_pdu_sf_bits, (block_num * block_len * 8) - 32);

            //if the CRC32 is correct, I think its fair to assume we don't need to worry about if the
            //individual CRC9s are correct on confirmed data blocks (but we can confirm now that they are all good)
            if (CRCComputed == CRCExtracted) CRCCorrect = 1;

            //only run on good CRC or CRC/RAS bypass (aggressive_framesync == 0)
            if (CRCCorrect || opts->aggressive_framesync == 0)
                dmr_pdu(opts, state, block_len, state->dmr_pdu_sf[slot]);

            if (CRCCorrect); //print nothing
            else {
                fprintf(stderr, "%s", KRED);
                fprintf(stderr, "\n Slot %d - Multi Block PDU Message CRC32 ERR", slot + 1);

                //debug print
                // fprintf (stderr, " %X - %X", CRCExtracted, CRCComputed);

                fprintf(stderr, "%s", KNRM);

            }

            //Full Super Frame Type 1 - Payload Output
            if (opts->payload == 1) {
                fprintf(stderr, "%s", KGRN);
                fprintf(stderr, "\n Slot %d - Multi Block PDU Message\n  ", slot + 1);
                for (i = 0; i < ((blocks + 1) * block_len); i++) {
                    fprintf(stderr, "[%02X]", state->dmr_pdu_sf[slot][i]);
                    if (i == 11 || i == 23 || i == 35 || i == 47 || i == 59 || i == 71 || i == 83 || i == 95) {
                        fprintf(stderr, "\n  ");
                    }
                }

                //debug print
                // fprintf (stderr, " CRC - EXT %X CMP %X", CRCExtracted, CRCComputed);

                if (state->data_header_format[slot] == 13 || state->data_header_format[slot] == 14 ||
                    state->data_header_sap[slot] == 10) {
                    fprintf(stderr, "%s", KCYN);
                    fprintf(stderr, "\n Short Data: ASCII \n");
                    for (i = 0; i < ((blocks + 1) * block_len); i++) {
                        if (state->dmr_pdu_sf[slot][i] <= 0x7E && state->dmr_pdu_sf[slot][i] >= 0x20) {
                            fprintf(stderr, "%c", state->dmr_pdu_sf[slot][i]);
                        } else fprintf(stderr, " ");

                        if (i == 47 || i == 95) fprintf(stderr, "\n  ");
                    }

                }

                fprintf(stderr, "%s ", KNRM);
            }

            //reset data header format storage
            state->data_header_format[slot] = 7;
            //flag off data header validity
            state->data_header_valid[slot] = 0;
            //flag off conf data flag
            state->data_conf_data[slot] = 0;

        } //end completed sf

    } //end type 1 blocks


    //type 2 - MBC and UDT header, MBC and UDT continuation blocks
    if (type == 2) {
        //sanity check (marginal signal, bad decodes, etc) -- may go a little lower (find out max number of MBC blocks supported)
        if (state->data_block_counter[slot] > 4) state->data_block_counter[slot] = 4;

        //Type 2 data block, additive method
        for (i = 0; i < block_len; i++) {
            state->dmr_pdu_sf[slot][i + (blockcounter * block_len)] = block_bytes[i];
        }

        memset(dmr_pdu_sf_bits, 0, sizeof(dmr_pdu_sf_bits));

        lb = block_bytes[0] >> 7; //last block flag
        pf = (block_bytes[0] >> 6) & 1;

        if (is_udt) {
            blocks = state->data_header_blocks[slot];
            if (blocks == blockcounter) lb = 1; //double check
            pf = 0; //flag off is set from bit in error
        }

        //last block arrived and we have a valid data header, time to send to cspdu decoder
        if (lb == 1 && state->data_header_valid[slot] == 1) {
            for (i = 0, j = 0; i < 12 * 4; i++, j += 8) //4 blocks max?
            {
                dmr_pdu_sf_bits[j + 0] = (state->dmr_pdu_sf[slot][i] >> 7) & 0x01;
                dmr_pdu_sf_bits[j + 1] = (state->dmr_pdu_sf[slot][i] >> 6) & 0x01;
                dmr_pdu_sf_bits[j + 2] = (state->dmr_pdu_sf[slot][i] >> 5) & 0x01;
                dmr_pdu_sf_bits[j + 3] = (state->dmr_pdu_sf[slot][i] >> 4) & 0x01;
                dmr_pdu_sf_bits[j + 4] = (state->dmr_pdu_sf[slot][i] >> 3) & 0x01;
                dmr_pdu_sf_bits[j + 5] = (state->dmr_pdu_sf[slot][i] >> 2) & 0x01;
                dmr_pdu_sf_bits[j + 6] = (state->dmr_pdu_sf[slot][i] >> 1) & 0x01;
                dmr_pdu_sf_bits[j + 7] = (state->dmr_pdu_sf[slot][i] >> 0) & 0x01;
            }

            //check UDT PF
            if (is_udt) pf = dmr_pdu_sf_bits[73];

            //CRC check on Header and full frames as appropriate (header crc already stored)
            //The 16 bit CRC in the header shall include the data carried by the header.
            //The 16 bit CRC in the last block shall be performed on
            //all MBC blocks (conmbined) except the header block.

            mbc_crc_good[0] = state->data_block_crc_valid[slot][0];

            CRCExtracted = 0;
            //extract crc from last block, apply to completed 'superframe' minus header
            for (i = 0; i < 16; i++) {
                CRCExtracted = CRCExtracted << 1;
                CRCExtracted = CRCExtracted | (uint32_t) (dmr_pdu_sf_bits[i + 96 * (1 + blocks) - 16] & 1);
            }

            uint8_t mbc_block_bits[12 * 8 * 3];
            memset(mbc_block_bits, 0, sizeof(mbc_block_bits));
            //shift continuation blocks and last block into seperate array for crc check
            for (i = 0; i < 12 * 8 * 3; i++) //only doing 3 blocks (4 minus the header)
            {
                mbc_block_bits[i] = dmr_pdu_sf_bits[i + 96]; //skip mbc header
            }

            CRCComputed = ComputeCrcCCITT16d(mbc_block_bits, ((blocks + 0) * 96) - 16);

            if (CRCComputed == CRCExtracted) mbc_crc_good[1] = 1;

            CRCCorrect = 0;
            IrrecoverableErrors = 1;

            //set good on good header and good blocks
            if (mbc_crc_good[0] == 1 && mbc_crc_good[1] == 1) {
                CRCCorrect = 1;
                IrrecoverableErrors = 0;
            } else {
                fprintf(stderr, "%s", KRED);
                fprintf(stderr, "\n Slot %d - Multi Block Control Message CRC16 ERR", slot + 1);

                //debug print
                fprintf(stderr, " %X - %X", CRCExtracted, CRCComputed);

                fprintf(stderr, "%s", KNRM);
            }

            //cspdu will only act on any fid/opcodes if good CRC to prevent falsing on control signalling
            if (!is_udt && !pf)
                dmr_cspdu(opts, state, dmr_pdu_sf_bits, state->dmr_pdu_sf[slot], CRCCorrect, IrrecoverableErrors);

            //will need to reassemble partially before sending to dmr_pdu or dmr_cspdu for handling -- needs testing
            if (is_udt && !pf)
                dmr_udt_cspdu_converter(opts, state, state->dmr_pdu_sf[slot], dmr_pdu_sf_bits, CRCCorrect,
                                        IrrecoverableErrors);

            //Full Super Frame MBC/UDT - Debug Output
            if (opts->payload == 1) {
                fprintf(stderr, "%s", KGRN);
                fprintf(stderr, "\n Slot %d - Multi Block Control Message\n  ", slot + 1);
                for (i = 0; i < ((blocks + 1) * block_len); i++) {
                    fprintf(stderr, "[%02X]", state->dmr_pdu_sf[slot][i]);
                    if (i == 11 || i == 23 || i == 35 || i == 47 || i == 59 || i == 71 || i == 83 || i == 95) {
                        fprintf(stderr, "\n  ");
                    }
                }
                fprintf(stderr, "%s", KRED);
                if (mbc_crc_good[0] == 0) fprintf(stderr, "MBC/UDT Header CRC ERR ");
                if (mbc_crc_good[1] == 0) fprintf(stderr, "MBC/UDT Blocks CRC ERR ");
                if (pf) fprintf(stderr, "MBC/UDT Header/Blocks Protected ");
                fprintf(stderr, "%s ", KNRM);
            }

        } //end last block flag on MBC

    } //end type 2 (MBC Header and Continuation)

    //leave this seperate so we can reset/zero stuff in case the data header isn't valid, etc
    //switched to an if-elseif-else so we could get the block counter increment on the end
    //without having it in an akward position

    //if the end of normal data header and blocks
    if (type == 1 && state->data_block_counter[slot] == state->data_header_blocks[slot]) {

        //clear out unified pdu 'superframe' slot
        for (short int i = 0; i < 288; i++) {
            state->dmr_pdu_sf[slot][i] = 0;
        }

        //Zero Out MBC Header Block CRC Valid
        state->data_block_crc_valid[slot][0] = 0;
        //reset the block counter (data blocks)
        state->data_block_counter[slot] = 1;
        //reset data header format storage
        state->data_header_format[slot] = 7;
        //flag off data header validity
        state->data_header_valid[slot] = 0;
        //flag off conf data flag
        state->data_conf_data[slot] = 0;

    }

        //else if the end of MBC Header and Blocks
    else if (type == 2 && lb == 1) {

        //clear out unified pdu 'superframe' slot
        for (short int i = 0; i < 288; i++) {
            state->dmr_pdu_sf[slot][i] = 0;
        }

        //Zero Out MBC Header Block CRC Valid
        state->data_block_crc_valid[slot][0] = 0;
        //reset the block counter (data blocks)
        state->data_block_counter[slot] = 1;
        //reset data header format storage
        state->data_header_format[slot] = 7;
        //flag off data header validity
        state->data_header_valid[slot] = 0;
        //flag off conf data flag
        state->data_conf_data[slot] = 0;

    }

        //else increment block counter after sorting/shuffling blocks
    else state->data_block_counter[slot]++;

}

//failsafe to clear old data header, block info, cach, in case of tact/emb/slottype failures 
//or tuning away and we can no longer verify accurate data block reporting
void dmr_reset_blocks(dsd_opts *opts, dsd_state *state) {
    memset(state->data_p_head, 0, sizeof(state->data_p_head));
    memset(state->data_conf_data, 0, sizeof(state->data_conf_data));
    memset(state->dmr_pdu_sf, 0, sizeof(state->dmr_pdu_sf));
    memset(state->data_block_counter, 1, sizeof(state->data_block_counter));
    memset(state->data_header_blocks, 1, sizeof(state->data_header_blocks));
    memset(state->data_block_crc_valid, 0, sizeof(state->data_block_crc_valid));
    memset(state->dmr_lrrp_source, 0, sizeof(state->dmr_lrrp_source));
    memset(state->dmr_cach_fragment, 1, sizeof(state->dmr_cach_fragment));
    memset(state->cap_plus_csbk_bits, 0, sizeof(state->cap_plus_csbk_bits));
    memset(state->cap_plus_block_num, 0, sizeof(state->cap_plus_block_num));
    memset(state->data_header_valid, 0, sizeof(state->data_header_valid));
    memset(state->data_header_format, 7, sizeof(state->data_header_format));
    //reset some strings -- resetting call string here causes random blink on ncurses terminal (cap+)
    // sprintf (state->call_string[0], "%s", "                     "); //21 spaces
    // sprintf (state->call_string[1], "%s", "                     "); //21 spaces
//    sprintf(state->dmr_lrrp_gps[0], "%s", "");
//    sprintf(state->dmr_lrrp_gps[1], "%s", "");
}