/* $NetBSD: unichromehw.h,v 1.1 2006/08/02 01:44:09 jmcneill Exp $ */

/*
 * Copyright 1998-2006 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2006 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) OR COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _DEV_PCI_UNICHROMEHW_H
#define _DEV_PCI_UNICHROMEHW_H

//***************************************************//
//* Definition IGA1 Design Method of CRTC Registers *//
//***************************************************//
#define IGA1_HOR_TOTAL_FORMULA(x)           ((x)/8)-5
#define IGA1_HOR_ADDR_FORMULA(x)            ((x)/8)-1
#define IGA1_HOR_BLANK_START_FORMULA(x)     ((x)/8)-1
#define IGA1_HOR_BLANK_END_FORMULA(x,y)     ((x+y)/8)-1
#define IGA1_HOR_SYNC_START_FORMULA(x)      ((x)/8)-1
#define IGA1_HOR_SYNC_END_FORMULA(x,y)      ((x+y)/8)-1

#define IGA1_VER_TOTAL_FORMULA(x)           (x)-2
#define IGA1_VER_ADDR_FORMULA(x)            (x)-1
#define IGA1_VER_BLANK_START_FORMULA(x)     (x)-1
#define IGA1_VER_BLANK_END_FORMULA(x,y)     (x+y)-1
#define IGA1_VER_SYNC_START_FORMULA(x)      (x)-1
#define IGA1_VER_SYNC_END_FORMULA(x,y)      (x+y)-1

//***************************************************//
//* Definition IGA2 Design Method of CRTC Registers *//
//***************************************************//
#define IGA2_HOR_TOTAL_FORMULA(x)           (x)-1
#define IGA2_HOR_ADDR_FORMULA(x)            (x)-1
#define IGA2_HOR_BLANK_START_FORMULA(x)     (x)-1
#define IGA2_HOR_BLANK_END_FORMULA(x,y)     (x+y)-1
#define IGA2_HOR_SYNC_START_FORMULA(x)      (x)-1
#define IGA2_HOR_SYNC_END_FORMULA(x,y)      (x+y)-1

#define IGA2_VER_TOTAL_FORMULA(x)           (x)-1
#define IGA2_VER_ADDR_FORMULA(x)            (x)-1
#define IGA2_VER_BLANK_START_FORMULA(x)     (x)-1
#define IGA2_VER_BLANK_END_FORMULA(x,y)     (x+y)-1
#define IGA2_VER_SYNC_START_FORMULA(x)      (x)-1
#define IGA2_VER_SYNC_END_FORMULA(x,y)      (x+y)-1

/**********************************************************/
/* Definition IGA2 Design Method of CRTC Shadow Registers */
/**********************************************************/
#define IGA2_HOR_TOTAL_SHADOW_FORMULA(x)           (x/8)-5
#define IGA2_HOR_BLANK_END_SHADOW_FORMULA(x,y)     ((x+y)/8)-1
#define IGA2_VER_TOTAL_SHADOW_FORMULA(x)           (x)-2
#define IGA2_VER_ADDR_SHADOW_FORMULA(x)            (x)-1
#define IGA2_VER_BLANK_START_SHADOW_FORMULA(x)     (x)-1
#define IGA2_VER_BLANK_END_SHADOW_FORMULA(x,y)     (x+y)-1
#define IGA2_VER_SYNC_START_SHADOW_FORMULA(x)      (x)
#define IGA2_VER_SYNC_END_SHADOW_FORMULA(x,y)      (x+y)

/* Define Register Number for IGA1 CRTC Timing */
#define IGA1_HOR_TOTAL_REG_NUM          2           // location: {CR00,0,7},{CR36,3,3}
#define IGA1_HOR_ADDR_REG_NUM           1           // location: {CR01,0,7}
#define IGA1_HOR_BLANK_START_REG_NUM    1           // location: {CR02,0,7}
#define IGA1_HOR_BLANK_END_REG_NUM      3           // location: {CR03,0,4},{CR05,7,7},{CR33,5,5}
#define IGA1_HOR_SYNC_START_REG_NUM     2           // location: {CR04,0,7},{CR33,4,4}
#define IGA1_HOR_SYNC_END_REG_NUM       1           // location: {CR05,0,4}
#define IGA1_VER_TOTAL_REG_NUM          4           // location: {CR06,0,7},{CR07,0,0},{CR07,5,5},{CR35,0,0}
#define IGA1_VER_ADDR_REG_NUM           4           // location: {CR12,0,7},{CR07,1,1},{CR07,6,6},{CR35,2,2}
#define IGA1_VER_BLANK_START_REG_NUM    4           // location: {CR15,0,7},{CR07,3,3},{CR09,5,5},{CR35,3,3}
#define IGA1_VER_BLANK_END_REG_NUM      1           // location: {CR16,0,7}
#define IGA1_VER_SYNC_START_REG_NUM     4           // location: {CR10,0,7},{CR07,2,2},{CR07,7,7},{CR35,1,1}
#define IGA1_VER_SYNC_END_REG_NUM       1           // location: {CR11,0,3}

/* Define Register Number for IGA2 Shadow CRTC Timing */
#define IGA2_SHADOW_HOR_TOTAL_REG_NUM       2       // location: {CR6D,0,7},{CR71,3,3}
#define IGA2_SHADOW_HOR_BLANK_END_REG_NUM   1       // location: {CR6E,0,7}
#define IGA2_SHADOW_VER_TOTAL_REG_NUM       2       // location: {CR6F,0,7},{CR71,0,2}
#define IGA2_SHADOW_VER_ADDR_REG_NUM        2       // location: {CR70,0,7},{CR71,4,6}
#define IGA2_SHADOW_VER_BLANK_START_REG_NUM 2       // location: {CR72,0,7},{CR74,4,6}
#define IGA2_SHADOW_VER_BLANK_END_REG_NUM   2       // location: {CR73,0,7},{CR74,0,2}
#define IGA2_SHADOW_VER_SYNC_START_REG_NUM  2       // location: {CR75,0,7},{CR76,4,6}
#define IGA2_SHADOW_VER_SYNC_END_REG_NUM    1       // location: {CR76,0,3}

/* Define Register Number for IGA2 CRTC Timing */
#define IGA2_HOR_TOTAL_REG_NUM          2           // location: {CR50,0,7},{CR55,0,3}
#define IGA2_HOR_ADDR_REG_NUM           2           // location: {CR51,0,7},{CR55,4,6}
#define IGA2_HOR_BLANK_START_REG_NUM    2           // location: {CR52,0,7},{CR54,0,2}
#define IGA2_HOR_BLANK_END_REG_NUM      3           // location: {CR53,0,7},{CR54,3,5},{CR5D,6,6}
#define IGA2_HOR_SYNC_START_REG_NUM     3           // location: {CR56,0,7},{CR54,6,7},{CR5C,7,7}
#define IGA2_HOR_SYNC_END_REG_NUM       2           // location: {CR57,0,7},{CR5C,6,6}
#define IGA2_VER_TOTAL_REG_NUM          2           // location: {CR58,0,7},{CR5D,0,2}
#define IGA2_VER_ADDR_REG_NUM           2           // location: {CR59,0,7},{CR5D,3,5}
#define IGA2_VER_BLANK_START_REG_NUM    2           // location: {CR5A,0,7},{CR5C,0,2}
#define IGA2_VER_BLANK_END_REG_NUM      2           // location: {CR5E,0,7},{CR5C,3,5}
#define IGA2_VER_SYNC_START_REG_NUM     2           // location: {CR5E,0,7},{CR5F,5,7}
#define IGA2_VER_SYNC_END_REG_NUM       1           // location: {CR5F,0,4}

/* Define Offset and Fetch Count Register*/
#define IGA1_OFFSET_REG_NUM             2                                 // location: {CR13,0,7},{CR35,5,7}
#define IGA1_OFFSER_ALIGN_BYTE          8                                 // 8 bytes alignment.
#define IGA1_OFFSET_FORMULA(x,y)        (x*y)/IGA1_OFFSER_ALIGN_BYTE      // x: H resolution, y: color depth

#define IGA1_FETCH_COUNT_REG_NUM        2                                 // location: {SR1C,0,7},{SR1D,0,1}
#define IGA1_FETCH_COUNT_ALIGN_BYTE     16                                // 16 bytes alignment.
#define IGA1_FETCH_COUNT_PATCH_VALUE    4                                 // x: H resolution, y: color depth
#define IGA1_FETCH_COUNT_FORMULA(x,y)   ((x*y)/IGA1_FETCH_COUNT_ALIGN_BYTE)+ IGA1_FETCH_COUNT_PATCH_VALUE

#define IGA2_OFFSET_REG_NUM             2           // location: {CR66,0,7},{CR67,0,1}
#define IGA2_OFFSET_ALIGN_BYTE          8
#define IGA2_OFFSET_FORMULA(x,y)        (x*y)/IGA2_OFFSET_ALIGN_BYTE     // x: H resolution, y: color depth

#define IGA2_FETCH_COUNT_REG_NUM        2           // location: {CR65,0,7},{CR67,2,3}
#define IGA2_FETCH_COUNT_ALIGN_BYTE     16
#define IGA2_FETCH_COUNT_PATCH_VALUE    0
#define IGA2_FETCH_COUNT_FORMULA(x,y)   ((x*y)/IGA2_FETCH_COUNT_ALIGN_BYTE)+ IGA2_FETCH_COUNT_PATCH_VALUE

// Staring Address
#define IGA1_STARTING_ADDR_REG_NUM      4           // location: {CR0C,0,7},{CR0D,0,7},{CR34,0,7},{CR48,0,1}
#define IGA2_STARTING_ADDR_REG_NUM      3           // location: {CR62,1,7},{CR63,0,7},{CR64,0,7}

// Define Display OFFSET
// These value are by HW suggested value
#define K800_IGA1_FIFO_MAX_DEPTH                384     // location: {SR17,0,7}
#define K800_IGA1_FIFO_THRESHOLD                328     // location: {SR16,0,5},{SR16,7,7}
#define K800_IGA1_FIFO_HIGH_THRESHOLD           296     // location: {SR18,0,5},{SR18,7,7}
#define K800_IGA1_DISPLAY_QUEUE_EXPIRE_NUM      0       // location: {SR22,0,4}. (128/4) =64, K800 must be set zero,
                                                        // because HW only 5 bits

#define K800_IGA2_FIFO_MAX_DEPTH                384     // location: {CR68,4,7},{CR94,7,7},{CR95,7,7}
#define K800_IGA2_FIFO_THRESHOLD                328     // location: {CR68,0,3},{CR95,4,6}
#define K800_IGA2_FIFO_HIGH_THRESHOLD           296     // location: {CR92,0,3},{CR95,0,2}
#define K800_IGA2_DISPLAY_QUEUE_EXPIRE_NUM      128     // location: {CR94,0,6}

#define P880_IGA1_FIFO_MAX_DEPTH                192     // location: {SR17,0,7}
#define P880_IGA1_FIFO_THRESHOLD                128     // location: {SR16,0,5},{SR16,7,7}
#define P880_IGA1_FIFO_HIGH_THRESHOLD           64      // location: {SR18,0,5},{SR18,7,7}
#define P880_IGA1_DISPLAY_QUEUE_EXPIRE_NUM      0       // location: {SR22,0,4}. (128/4) =64, K800 must be set zero,
                                                        // because HW only 5 bits

#define P880_IGA2_FIFO_MAX_DEPTH                96      // location: {CR68,4,7},{CR94,7,7},{CR95,7,7}
#define P880_IGA2_FIFO_THRESHOLD                64      // location: {CR68,0,3},{CR95,4,6}
#define P880_IGA2_FIFO_HIGH_THRESHOLD           32      // location: {CR92,0,3},{CR95,0,2}
#define P880_IGA2_DISPLAY_QUEUE_EXPIRE_NUM      128     // location: {CR94,0,6}

/* VT3314 chipset*/
#define CN900_IGA1_FIFO_MAX_DEPTH               96 /* location: {SR17,0,7}*/
#define CN900_IGA1_FIFO_THRESHOLD               80 /* location: {SR16,0,5},{SR16,7,7}*/
#define CN900_IGA1_FIFO_HIGH_THRESHOLD          64  /* location: {SR18,0,5},{SR18,7,7}*/
#define CN900_IGA1_DISPLAY_QUEUE_EXPIRE_NUM     0   /* location: {SR22,0,4}. (128/4) =64, P800 must be set zero, because HW only 5 bits*/

#define CN900_IGA2_FIFO_MAX_DEPTH               96  /* location: {CR68,4,7},{CR94,7,7},{CR95,7,7}*/
#define CN900_IGA2_FIFO_THRESHOLD               80  /* location: {CR68,0,3},{CR95,4,6}*/
#define CN900_IGA2_FIFO_HIGH_THRESHOLD          32  /* location: {CR92,0,3},{CR95,0,2}*/
#define CN900_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     128 /* location: {CR94,0,6}*/

/* For VT3324, these values are suggested by HW */
#define CX700_IGA1_FIFO_MAX_DEPTH               192     /* location: {SR17,0,7}*/
#define CX700_IGA1_FIFO_THRESHOLD               128     /* location: {SR16,0,5},{SR16,7,7}*/
#define CX700_IGA1_FIFO_HIGH_THRESHOLD          128     /* location: {SR18,0,5},{SR18,7,7} */
#define CX700_IGA1_DISPLAY_QUEUE_EXPIRE_NUM     124     /* location: {SR22,0,4} */

#define CX700_IGA2_FIFO_MAX_DEPTH               96      /* location: {CR68,4,7},{CR94,7,7},{CR95,7,7}*/
#define CX700_IGA2_FIFO_THRESHOLD               64      /* location: {CR68,0,3},{CR95,4,6}*/
#define CX700_IGA2_FIFO_HIGH_THRESHOLD          32      /* location: {CR92,0,3},{CR95,0,2} */
#define CX700_IGA2_DISPLAY_QUEUE_EXPIRE_NUM     128     /* location: {CR94,0,6}*/

#define IGA1_FIFO_DEPTH_SELECT_REG_NUM          1
#define IGA1_FIFO_THRESHOLD_REG_NUM             2
#define IGA1_FIFO_HIGH_THRESHOLD_REG_NUM        2
#define IGA1_DISPLAY_QUEUE_EXPIRE_NUM_REG_NUM   1

#define IGA2_FIFO_DEPTH_SELECT_REG_NUM          3
#define IGA2_FIFO_THRESHOLD_REG_NUM             2
#define IGA2_FIFO_HIGH_THRESHOLD_REG_NUM        2
#define IGA2_DISPLAY_QUEUE_EXPIRE_NUM_REG_NUM   1


#define IGA1_FIFO_DEPTH_SELECT_FORMULA(x)                   (x/2)-1
#define IGA1_FIFO_THRESHOLD_FORMULA(x)                      (x/4)
#define IGA1_DISPLAY_QUEUE_EXPIRE_NUM_FORMULA(x)            (x/4)
#define IGA1_FIFO_HIGH_THRESHOLD_FORMULA(x)                 (x/4)
#define IGA2_FIFO_DEPTH_SELECT_FORMULA(x)                   ((x/2)/4)-1
#define IGA2_FIFO_THRESHOLD_FORMULA(x)                      (x/4)
#define IGA2_DISPLAY_QUEUE_EXPIRE_NUM_FORMULA(x)            (x/4)
#define IGA2_FIFO_HIGH_THRESHOLD_FORMULA(x)                 (x/4)

/************************************************************************/
/*  LCD Timing                                                          */
/************************************************************************/
#define LCD_POWER_SEQ_TD0               500000         // 500 ms = 500000 us
#define LCD_POWER_SEQ_TD1               50000          // 50 ms = 50000 us
#define LCD_POWER_SEQ_TD2               0              // 0 us
#define LCD_POWER_SEQ_TD3               210000         // 210 ms = 210000 us

#define CLE266_POWER_SEQ_UNIT           71             // 2^10 * (1/14.31818M) = 71.475 us (K400.revA)
#define K800_POWER_SEQ_UNIT             142            // 2^11 * (1/14.31818M) = 142.95 us (K400.revB)
#define P880_POWER_SEQ_UNIT             572            // 2^13 * (1/14.31818M) = 572.1 us

#define CLE266_POWER_SEQ_FORMULA(x)     (x)/CLE266_POWER_SEQ_UNIT
#define K800_POWER_SEQ_FORMULA(x)       (x)/K800_POWER_SEQ_UNIT
#define P880_POWER_SEQ_FORMULA(x)       (x)/P880_POWER_SEQ_UNIT


#define LCD_POWER_SEQ_TD0_REG_NUM       2   // location: {CR8B,0,7},{CR8F,0,3}
#define LCD_POWER_SEQ_TD1_REG_NUM       2   // location: {CR8C,0,7},{CR8F,4,7}
#define LCD_POWER_SEQ_TD2_REG_NUM       2   // location: {CR8D,0,7},{CR90,0,3}
#define LCD_POWER_SEQ_TD3_REG_NUM       2   // location: {CR8E,0,7},{CR90,4,7}


// LCD Scaling factor
// x: indicate setting horizontal size
// y: indicate panel horizontal size

#define CLE266_LCD_HOR_SCF_FORMULA(x,y)   (((x-1)*1024)/(y-1))    // Horizontal scaling factor 10 bits (2^10)
#define CLE266_LCD_VER_SCF_FORMULA(x,y)   (((x-1)*1024)/(y-1))    // Vertical scaling factor 10 bits (2^10)
#define K800_LCD_HOR_SCF_FORMULA(x,y)     (((x-1)*4096)/(y-1))    // Horizontal scaling factor 10 bits (2^12)
#define K800_LCD_VER_SCF_FORMULA(x,y)     (((x-1)*2048)/(y-1))    // Vertical scaling factor 10 bits (2^11)

#define LCD_HOR_SCALING_FACTOR_REG_NUM  3   // location: {CR9F,0,1},{CR77,0,7},{CR79,4,5}
#define LCD_VER_SCALING_FACTOR_REG_NUM  3   // location: {CR79,3,3},{CR78,0,7},{CR79,6,7}
#define LCD_HOR_SCALING_FACTOR_REG_NUM_CLE  2               /* location: {CR77,0,7},{CR79,4,5} */
#define LCD_VER_SCALING_FACTOR_REG_NUM_CLE  2               /* location: {CR78,0,7},{CR79,6,7} */



//************************************************//
//*      Define IGA1 Display Timing              *//
//************************************************//
struct io_register {
    uint8_t      io_addr;
    uint8_t      start_bit;
    uint8_t      end_bit;
};


/* IGA1 Horizontal Total */
struct iga1_hor_total
{
    int     reg_num;
    struct  io_register reg[IGA1_HOR_TOTAL_REG_NUM];
};

/* IGA1 Horizontal Addressable Video */
struct iga1_hor_addr {
    int     reg_num;
    struct  io_register reg[IGA1_HOR_ADDR_REG_NUM];
};

/* IGA1 Horizontal Blank Start */
struct iga1_hor_blank_start {
    int     reg_num;
    struct  io_register reg[IGA1_HOR_BLANK_START_REG_NUM];
};

/* IGA1 Horizontal Blank End */
struct iga1_hor_blank_end {
    int     reg_num;
    struct  io_register reg[IGA1_HOR_BLANK_END_REG_NUM];
};

/* IGA1 Horizontal Sync Start */
struct iga1_hor_sync_start {
    int     reg_num;
    struct  io_register reg[IGA1_HOR_SYNC_START_REG_NUM];
};

/* IGA1 Horizontal Sync End */
struct iga1_hor_sync_end {
    int     reg_num;
    struct  io_register reg[IGA1_HOR_SYNC_END_REG_NUM];
};

/* IGA1 Vertical Total */
struct iga1_ver_total {
    int     reg_num;
    struct  io_register reg[IGA1_VER_TOTAL_REG_NUM];
};

/* IGA1 Vertical Addressable Video */
struct iga1_ver_addr {
    int     reg_num;
    struct  io_register reg[IGA1_VER_ADDR_REG_NUM];
};

/* IGA1 Vertical Blank Start */
struct iga1_ver_blank_start {
    int     reg_num;
    struct  io_register reg[IGA1_VER_BLANK_START_REG_NUM];
};

/* IGA1 Vertical Blank End */
struct iga1_ver_blank_end {
    int     reg_num;
    struct  io_register reg[IGA1_VER_BLANK_END_REG_NUM];
};

/* IGA1 Vertical Sync Start */
struct iga1_ver_sync_start {
    int     reg_num;
    struct  io_register reg[IGA1_VER_SYNC_START_REG_NUM];
};

/* IGA1 Vertical Sync End */
struct iga1_ver_sync_end {
    int     reg_num;
    struct  io_register reg[IGA1_VER_SYNC_END_REG_NUM];
};

//************************************************//
//      Define IGA2 Shadow Display Timing         //
//************************************************//

/* IGA2 Shadow Horizontal Total */
struct iga2_shadow_hor_total
{
    int     reg_num;
    struct  io_register reg[IGA2_SHADOW_HOR_TOTAL_REG_NUM];
};

/* IGA2 Shadow Horizontal Blank End */
struct iga2_shadow_hor_blank_end {
    int     reg_num;
    struct  io_register reg[IGA2_SHADOW_HOR_BLANK_END_REG_NUM];
};


/* IGA2 Shadow Vertical Total */
struct iga2_shadow_ver_total {
    int     reg_num;
    struct  io_register reg[IGA2_SHADOW_VER_TOTAL_REG_NUM];
};

/* IGA2 Shadow Vertical Addressable Video */
struct iga2_shadow_ver_addr {
    int     reg_num;
    struct  io_register reg[IGA2_SHADOW_VER_ADDR_REG_NUM];
};

/* IGA2 Shadow Vertical Blank Start */
struct iga2_shadow_ver_blank_start {
    int     reg_num;
    struct  io_register reg[IGA2_SHADOW_VER_BLANK_START_REG_NUM];
};

/* IGA2 Shadow Vertical Blank End */
struct iga2_shadow_ver_blank_end {
    int     reg_num;
    struct  io_register reg[IGA2_SHADOW_VER_BLANK_END_REG_NUM];
};

/* IGA2 Shadow Vertical Sync Start */
struct iga2_shadow_ver_sync_start {
    int     reg_num;
    struct  io_register reg[IGA2_SHADOW_VER_SYNC_START_REG_NUM];
};

/* IGA2 Shadow Vertical Sync End */
struct iga2_shadow_ver_sync_end {
    int     reg_num;
    struct  io_register reg[IGA2_SHADOW_VER_SYNC_END_REG_NUM];
};

//************************************************//
//      Define IGA2 Display Timing                //
//************************************************//

/* IGA2 Horizontal Total */
struct iga2_hor_total {
    int     reg_num;
    struct  io_register reg[IGA2_HOR_TOTAL_REG_NUM];
};

/* IGA2 Horizontal Addressable Video */
struct iga2_hor_addr {
    int     reg_num;
    struct  io_register reg[IGA2_HOR_ADDR_REG_NUM];
};

/* IGA2 Horizontal Blank Start */
struct iga2_hor_blank_start {
    int     reg_num;
    struct  io_register reg[IGA2_HOR_BLANK_START_REG_NUM];
};

/* IGA2 Horizontal Blank End */
struct iga2_hor_blank_end {
    int     reg_num;
    struct  io_register reg[IGA2_HOR_BLANK_END_REG_NUM];
};

/* IGA2 Horizontal Sync Start */
struct iga2_hor_sync_start {
    int     reg_num;
    struct  io_register reg[IGA2_HOR_SYNC_START_REG_NUM];
};

/* IGA2 Horizontal Sync End */
struct iga2_hor_sync_end {
    int     reg_num;
    struct  io_register reg[IGA2_HOR_SYNC_END_REG_NUM];
};

/* IGA2 Vertical Total */
struct iga2_ver_total {
    int     reg_num;
    struct  io_register reg[IGA2_VER_TOTAL_REG_NUM];
};

/* IGA2 Vertical Addressable Video */
struct iga2_ver_addr {
    int     reg_num;
    struct  io_register reg[IGA2_VER_ADDR_REG_NUM];
};

/* IGA2 Vertical Blank Start */
struct iga2_ver_blank_start {
    int     reg_num;
    struct  io_register reg[IGA2_VER_BLANK_START_REG_NUM];
};

/* IGA2 Vertical Blank End */
struct iga2_ver_blank_end {
    int     reg_num;
    struct  io_register reg[IGA2_VER_BLANK_END_REG_NUM];
};

/* IGA2 Vertical Sync Start */
struct iga2_ver_sync_start {
    int     reg_num;
    struct  io_register reg[IGA2_VER_SYNC_START_REG_NUM];
};

/* IGA2 Vertical Sync End */
struct iga2_ver_sync_end {
    int     reg_num;
    struct  io_register reg[IGA2_VER_SYNC_END_REG_NUM];
};

/* IGA1 Offset Register */
struct iga1_offset {
    int     reg_num;
    struct  io_register reg[IGA1_OFFSET_REG_NUM];
};

/* IGA2 Offset Register */
struct iga2_offset {
    int     reg_num;
    struct  io_register reg[IGA2_OFFSET_REG_NUM];
};

struct offset{
    struct iga1_offset            iga1_offset_reg;
    struct iga2_offset            iga2_offset_reg;
};

/* IGA1 Fetch Count Register */
struct iga1_fetch_count {
    int     reg_num;
    struct  io_register reg[IGA1_FETCH_COUNT_REG_NUM];
};

/* IGA2 Fetch Count Register */
struct iga2_fetch_count {
    int     reg_num;
    struct  io_register reg[IGA2_FETCH_COUNT_REG_NUM];
};

struct fetch_count{
    struct iga1_fetch_count       iga1_fetch_count_reg;
    struct iga2_fetch_count       iga2_fetch_count_reg;
};

/* Starting Address Register */
struct iga1_starting_addr {
    int     reg_num;
    struct  io_register reg[IGA1_STARTING_ADDR_REG_NUM];
};

struct iga2_starting_addr {
    int     reg_num;
    struct  io_register reg[IGA2_STARTING_ADDR_REG_NUM];
};

struct starting_addr {
    struct iga1_starting_addr       iga1_starting_addr_reg;
    struct iga2_starting_addr       iga2_starting_addr_reg;
};

/* LCD Power Sequence Timer */
struct lcd_pwd_seq_td0{
    int     reg_num;
    struct  io_register reg[LCD_POWER_SEQ_TD0_REG_NUM];
};

struct lcd_pwd_seq_td1{
    int     reg_num;
    struct  io_register reg[LCD_POWER_SEQ_TD1_REG_NUM];
};

struct lcd_pwd_seq_td2{
    int     reg_num;
    struct  io_register reg[LCD_POWER_SEQ_TD2_REG_NUM];
};

struct lcd_pwd_seq_td3{
    int     reg_num;
    struct  io_register reg[LCD_POWER_SEQ_TD3_REG_NUM];
};

struct _lcd_pwd_seq_timer{
    struct lcd_pwd_seq_td0       td0;
    struct lcd_pwd_seq_td1       td1;
    struct lcd_pwd_seq_td2       td2;
    struct lcd_pwd_seq_td3       td3;
};

/* LCD Scaling Factor */
struct _lcd_hor_scaling_factor{
    int     reg_num;
    struct  io_register reg[LCD_HOR_SCALING_FACTOR_REG_NUM];
};

struct _lcd_ver_scaling_factor{
    int     reg_num;
    struct  io_register reg[LCD_VER_SCALING_FACTOR_REG_NUM];
};


struct _lcd_scaling_factor{
    struct _lcd_hor_scaling_factor  lcd_hor_scaling_factor;
    struct _lcd_ver_scaling_factor  lcd_ver_scaling_factor;
};

struct pll_map {
    uint32_t     clk;
    uint32_t     cle266_pll;
    uint32_t     k800_pll;
    uint32_t     cx700_pll;
};

struct rgbLUT {
    uint8_t     red;
    uint8_t     green;
    uint8_t     blue;
};

struct lcd_pwd_seq_timer {
    uint16_t     td0;
    uint16_t     td1;
    uint16_t     td2;
    uint16_t     td3;
};


// Display FIFO Relation Registers
struct iga1_fifo_depth_select {
    int     reg_num;
    struct  io_register reg[IGA1_FIFO_DEPTH_SELECT_REG_NUM];
};

struct iga1_fifo_threshold_select {
    int     reg_num;
    struct  io_register reg[IGA1_FIFO_THRESHOLD_REG_NUM];
};

struct iga1_fifo_high_threshold_select {
    int     reg_num;
    struct  io_register reg[IGA1_FIFO_HIGH_THRESHOLD_REG_NUM];
};

struct iga1_display_queue_expire_num {
    int     reg_num;
    struct  io_register reg[IGA1_DISPLAY_QUEUE_EXPIRE_NUM_REG_NUM];
};

struct iga2_fifo_depth_select {
    int     reg_num;
    struct  io_register reg[IGA2_FIFO_DEPTH_SELECT_REG_NUM];
};

struct iga2_fifo_threshold_select {
    int     reg_num;
    struct  io_register reg[IGA2_FIFO_THRESHOLD_REG_NUM];
};

struct iga2_fifo_high_threshold_select {
    int     reg_num;
    struct  io_register reg[IGA2_FIFO_HIGH_THRESHOLD_REG_NUM];
};

struct iga2_display_queue_expire_num {
    int     reg_num;
    struct  io_register reg[IGA2_DISPLAY_QUEUE_EXPIRE_NUM_REG_NUM];
};

struct fifo_depth_select {
    struct  iga1_fifo_depth_select iga1_fifo_depth_select_reg;
    struct  iga2_fifo_depth_select iga2_fifo_depth_select_reg;
};

struct fifo_threshold_select {
    struct  iga1_fifo_threshold_select iga1_fifo_threshold_select_reg;
    struct  iga2_fifo_threshold_select iga2_fifo_threshold_select_reg;
};

struct fifo_high_threshold_select {
    struct  iga1_fifo_high_threshold_select iga1_fifo_high_threshold_select_reg;
    struct  iga2_fifo_high_threshold_select iga2_fifo_high_threshold_select_reg;
};

struct display_queue_expire_num {
    struct  iga1_display_queue_expire_num iga1_display_queue_expire_num_reg;
    struct  iga2_display_queue_expire_num iga2_display_queue_expire_num_reg;
};



struct iga1_crtc_timing {
    struct iga1_hor_total         hor_total;
    struct iga1_hor_addr          hor_addr;
    struct iga1_hor_blank_start   hor_blank_start;
    struct iga1_hor_blank_end     hor_blank_end;
    struct iga1_hor_sync_start    hor_sync_start;
    struct iga1_hor_sync_end      hor_sync_end;
    struct iga1_ver_total         ver_total;
    struct iga1_ver_addr          ver_addr;
    struct iga1_ver_blank_start   ver_blank_start;
    struct iga1_ver_blank_end     ver_blank_end;
    struct iga1_ver_sync_start    ver_sync_start;
    struct iga1_ver_sync_end      ver_sync_end;
};

struct iga2_shadow_crtc_timing {
    struct iga2_shadow_hor_total        hor_total_shadow;
    struct iga2_shadow_hor_blank_end    hor_blank_end_shadow;
    struct iga2_shadow_ver_total        ver_total_shadow;
    struct iga2_shadow_ver_addr         ver_addr_shadow;
    struct iga2_shadow_ver_blank_start  ver_blank_start_shadow;
    struct iga2_shadow_ver_blank_end    ver_blank_end_shadow;
    struct iga2_shadow_ver_sync_start   ver_sync_start_shadow;
    struct iga2_shadow_ver_sync_end     ver_sync_end_shadow;
};

struct iga2_crtc_timing {
    struct iga2_hor_total         hor_total;
    struct iga2_hor_addr          hor_addr;
    struct iga2_hor_blank_start   hor_blank_start;
    struct iga2_hor_blank_end     hor_blank_end;
    struct iga2_hor_sync_start    hor_sync_start;
    struct iga2_hor_sync_end      hor_sync_end;
    struct iga2_ver_total         ver_total;
    struct iga2_ver_addr          ver_addr;
    struct iga2_ver_blank_start   ver_blank_start;
    struct iga2_ver_blank_end     ver_blank_end;
    struct iga2_ver_sync_start    ver_sync_start;
    struct iga2_ver_sync_end      ver_sync_end;
};

#endif /* _DEV_PCI_UNICHROMEHW_H */
