/* $NetBSD: unichromemode.h,v 1.1 2006/08/02 01:44:09 jmcneill Exp $ */

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

#ifndef _DEV_PCI_UNICHROMEMODE_H
#define _DEV_PCI_UNICHROMEMODE_H

#define	ARRAY_SIZE(x)	(sizeof((x)) / sizeof((x)[0]))

struct VPITTable {
  unsigned char  Misc;
  unsigned char  SR[StdSR];
  unsigned char  GR[StdGR];
  unsigned char  AR[StdAR];
};

struct VideoModeTable {
  int                               ModeIndex;
  struct crt_mode_table             *crtc;
  int                               mode_array;
};

struct patch_table {
  int           mode_index;
  int           table_length;
  struct io_reg *io_reg_table;
};

struct res_map_refresh {
  int       hres;
  int       vres;
  int       pixclock;
  int       vmode_refresh;
};

struct res_map_refresh res_map_refresh_tbl[] = {
//hres, vres, vclock, vmode_refresh
  {640, 480, RES_640X480_60HZ_PIXCLOCK,   60},
  {640, 480, RES_640X480_75HZ_PIXCLOCK,   75},
  {640, 480, RES_640X480_85HZ_PIXCLOCK,   85},
  {640, 480, RES_640X480_100HZ_PIXCLOCK,  100},
  {640, 480, RES_640X480_120HZ_PIXCLOCK,  120},
  {720, 480, RES_720X480_60HZ_PIXCLOCK,   60},
  {720, 576, RES_720X576_60HZ_PIXCLOCK,   60},
  {800, 480, RES_800X480_60HZ_PIXCLOCK,   60},
  {800, 600, RES_800X600_60HZ_PIXCLOCK,   60},
  {800, 600, RES_800X600_75HZ_PIXCLOCK,   75},
  {800, 600, RES_800X600_85HZ_PIXCLOCK,   85},
  {800, 600, RES_800X600_100HZ_PIXCLOCK,  100},
  {800, 600, RES_800X600_120HZ_PIXCLOCK,  120},
  {848, 480, RES_848X480_60HZ_PIXCLOCK,   60},
  {856, 480, RES_856X480_60HZ_PIXCLOCK,   60},
  {1024,512, RES_1024X512_60HZ_PIXCLOCK,  60},
  {1024,768, RES_1024X768_60HZ_PIXCLOCK,  60},
  {1024,768, RES_1024X768_75HZ_PIXCLOCK,  75},
  {1024,768, RES_1024X768_85HZ_PIXCLOCK,  85},
  {1024,768, RES_1024X768_100HZ_PIXCLOCK, 100},
  {1152,864, RES_1152X864_70HZ_PIXCLOCK,  70},
  {1152,864, RES_1152X864_75HZ_PIXCLOCK,  75},
  {1280,768, RES_1280X768_60HZ_PIXCLOCK,  60},
  {1280,960, RES_1280X960_60HZ_PIXCLOCK,  60},
  {1280,1024,RES_1280X1024_60HZ_PIXCLOCK, 60},
  {1280,1024,RES_1280X1024_75HZ_PIXCLOCK, 75},
  {1280,1024,RES_1280X768_85HZ_PIXCLOCK,  85},
  {1440,1050,RES_1440X1050_60HZ_PIXCLOCK, 60},
  {1600,1200,RES_1600X1200_60HZ_PIXCLOCK, 60},
  {1600,1200,RES_1600X1200_75HZ_PIXCLOCK, 75},
  {1280,720, RES_1280X720_60HZ_PIXCLOCK,  60},
  {1920,1080,RES_1920X1080_60HZ_PIXCLOCK, 60},
  {1400,1050,RES_1400X1050_60HZ_PIXCLOCK, 60},
  {1366,768, RES_1366X768_60HZ_PIXCLOCK,60}
};
#define NUM_TOTAL_RES_MAP_REFRESH ARRAY_SIZE(res_map_refresh_tbl)

struct io_reg CN400_ModeXregs[] = {
    {VIASR, SR10, 0xFF, 0x01},
    {VIASR, SR15, 0x02, 0x02},
    {VIASR, SR16, 0xBF, 0x08},
    {VIASR, SR17, 0xFF, 0x1F},
    {VIASR, SR18, 0xFF, 0x4E},
    {VIASR, SR1A, 0xFB, 0x08},
    {VIASR, SR1E, 0x0F, 0x01},
    {VIASR, SR2A, 0xF0, 0x00},
    {VIACR, CR0A, 0xFF, 0x1E},          /* Cursor Start                        */
    {VIACR, CR0B, 0xFF, 0x00},          /* Cursor End                          */
    {VIACR, CR0E, 0xFF, 0x00},          /* Cursor Location High                */
    {VIACR, CR0F, 0xFF, 0x00},          /* Cursor Localtion Low                */
    {VIACR, CR32, 0xFF, 0x00},
    {VIACR, CR33, 0xFF, 0x00},
    {VIACR, CR34, 0xFF, 0x00},
    {VIACR, CR35, 0xFF, 0x00},
    {VIACR, CR36, 0x08, 0x00},
    {VIACR, CR62, 0xFF, 0x00},          /* Secondary Display Starting Address  */
    {VIACR, CR63, 0xFF, 0x00},          /* Secondary Display Starting Address  */
    {VIACR, CR64, 0xFF, 0x00},          /* Secondary Display Starting Address  */
    {VIACR, CR69, 0xFF, 0x00},
    {VIACR, CR6A, 0xFF, 0x40},
    {VIACR, CR6B, 0xFF, 0x00},
    {VIACR, CR6C, 0xFF, 0x00},
    {VIACR, CR7A, 0xFF, 0x01},          /* LCD Scaling Parameter 1             */
    {VIACR, CR7B, 0xFF, 0x02},          /* LCD Scaling Parameter 2             */
    {VIACR, CR7C, 0xFF, 0x03},          /* LCD Scaling Parameter 3             */
    {VIACR, CR7D, 0xFF, 0x04},          /* LCD Scaling Parameter 4             */
    {VIACR, CR7E, 0xFF, 0x07},          /* LCD Scaling Parameter 5             */
    {VIACR, CR7F, 0xFF, 0x0A},          /* LCD Scaling Parameter 6             */
    {VIACR, CR80, 0xFF, 0x0D},          /* LCD Scaling Parameter 7             */
    {VIACR, CR81, 0xFF, 0x13},          /* LCD Scaling Parameter 8             */
    {VIACR, CR82, 0xFF, 0x16},          /* LCD Scaling Parameter 9             */
    {VIACR, CR83, 0xFF, 0x19},          /* LCD Scaling Parameter 10            */
    {VIACR, CR84, 0xFF, 0x1C},          /* LCD Scaling Parameter 11            */
    {VIACR, CR85, 0xFF, 0x1D},          /* LCD Scaling Parameter 12            */
    {VIACR, CR86, 0xFF, 0x1E},          /* LCD Scaling Parameter 13            */
    {VIACR, CR87, 0xFF, 0x1F},          /* LCD Scaling Parameter 14            */
    {VIACR, CR88, 0xFF, 0x40},          /* LCD Panel Type                      */
    {VIACR, CR89, 0xFF, 0x00},          /* LCD Timing Control 0                */
    {VIACR, CR8A, 0xFF, 0x88},          /* LCD Timing Control 1                */
    {VIACR, CR8B, 0xFF, 0x69},          /* LCD Power Sequence Control 0        */
    {VIACR, CR8C, 0xFF, 0x57},          /* LCD Power Sequence Control 1        */
    {VIACR, CR8D, 0xFF, 0x00},          /* LCD Power Sequence Control 2        */
    {VIACR, CR8E, 0xFF, 0x7B},          /* LCD Power Sequence Control 3        */
    {VIACR, CR8F, 0xFF, 0x03},          /* LCD Power Sequence Control 4        */
    {VIACR, CR90, 0xFF, 0x30},          /* LCD Power Sequence Control 5        */
    {VIACR, CR91, 0xFF, 0xA0},          /* 24/12 bit LVDS Data off             */
    {VIACR, CR96, 0xFF, 0x00},
    {VIACR, CR97, 0xFF, 0x00},
    {VIACR, CR99, 0xFF, 0x00},
    {VIACR, CR9B, 0xFF, 0x00}
};
#define NUM_TOTAL_CN400_ModeXregs ARRAY_SIZE(CN400_ModeXregs)

/* Video Mode Table for VT3314 chipset*/
/* Common Setting for Video Mode */
struct io_reg CN900_ModeXregs[] = {
  {VIASR,SR10,0xFF,0x01},
  {VIASR,SR15,0x02,0x02},
  {VIASR,SR16,0xBF,0x08},
  {VIASR,SR17,0xFF,0x1F},
  {VIASR,SR18,0xFF,0x4E},
  {VIASR,SR1A,0xFB,0x82},
  {VIASR,SR1B,0xFF,0xF0},
  {VIASR,SR1F,0xFF,0x00},
  {VIASR,SR1E,0xFF,0xF1},
  {VIASR,SR22,0xFF,0x1F},
  {VIASR,SR2A,0x0F,0x0F},
  {VIASR,SR2E,0xFF,0xFF},
  {VIASR,SR3F,0xFF,0xFF},
  {VIASR,SR40,0xFF,0x00},  
  {VIASR,CR30,0xFF,0x04},
  {VIACR,CR32,0xFF,0x00},
  {VIACR,CR33,0xFF,0x00},
  {VIACR,CR34,0xFF,0x00},
  {VIACR,CR35,0xFF,0x00},
  {VIACR,CR36,0xFF,0x31},
  {VIACR,CR41,0xFF,0x80},
  {VIACR,CR42,0xFF,0x00},
  {VIACR,CR5D,0x80,0x00},                                      /* Horizontal Retrace Start bit [11] should be 0*/   
  {VIACR,CR62,0xFF,0x00},                                      /* Secondary Display Starting Address*/
  {VIACR,CR63,0xFF,0x00},                                      /* Secondary Display Starting Address*/
  {VIACR,CR64,0xFF,0x00},                                      /* Secondary Display Starting Address*/
  {VIACR,CR68,0xFF,0x67},                                      /* Default FIFO For IGA2 */ 
  {VIACR,CR69,0xFF,0x00},
  {VIACR,CR6A,0xFF,0x40},
  {VIACR,CR6B,0xFF,0x00},
  {VIACR,CR6C,0xFF,0x00},  
  {VIACR,CR77,0xFF,0x00},                                      /* LCD scaling Factor*/
  {VIACR,CR78,0xFF,0x00},                                      /* LCD scaling Factor */
  {VIACR,CR79,0xFF,0x00},                                      /* LCD scaling Factor*/
  {VIACR,CR9F,0x03,0x00},                                      /* LCD scaling Factor */
  {VIACR,CR7A,0xFF,0x01},                                      /* LCD Scaling Parameter 1*/
  {VIACR,CR7B,0xFF,0x02},                                      /* LCD Scaling Parameter 2*/
  {VIACR,CR7C,0xFF,0x03},                                      /* LCD Scaling Parameter 3 */
  {VIACR,CR7D,0xFF,0x04},                                      /* LCD Scaling Parameter 4*/
  {VIACR,CR7E,0xFF,0x07},                                      /* LCD Scaling Parameter 5*/
  {VIACR,CR7F,0xFF,0x0A},                                      /* LCD Scaling Parameter 6*/
  {VIACR,CR80,0xFF,0x0D},                                      /* LCD Scaling Parameter 7*/
  {VIACR,CR81,0xFF,0x13},                                      /* LCD Scaling Parameter 8*/
  {VIACR,CR82,0xFF,0x16},                                      /* LCD Scaling Parameter 9*/
  {VIACR,CR83,0xFF,0x19},                                      /* LCD Scaling Parameter 10*/
  {VIACR,CR84,0xFF,0x1C},                                      /* LCD Scaling Parameter 11*/
  {VIACR,CR85,0xFF,0x1D},                                      /* LCD Scaling Parameter 12*/
  {VIACR,CR86,0xFF,0x1E},                                      /* LCD Scaling Parameter 13*/
  {VIACR,CR87,0xFF,0x1F},                                      /* LCD Scaling Parameter 14*/
  {VIACR,CR88,0xFF,0x40},                                      /* LCD Panel Type */
  {VIACR,CR89,0xFF,0x00},                                      /* LCD Timing Control 0 */
  {VIACR,CR8A,0xFF,0x88},                                      /* LCD Timing Control 1*/
  {VIACR,CR8B,0xFF,0x69},                                      /* LCD Power Sequence Control 0*/
  {VIACR,CR8C,0xFF,0x57},                                      /* LCD Power Sequence Control 1*/
  {VIACR,CR8D,0xFF,0x00},                                      /* LCD Power Sequence Control 2*/
  {VIACR,CR8E,0xFF,0x7B},                                      /* LCD Power Sequence Control 3*/
  {VIACR,CR8F,0xFF,0x03},                                      /* LCD Power Sequence Control 4*/
  {VIACR,CR90,0xFF,0x30},                                      /* LCD Power Sequence Control 5*/
  {VIACR,CR91,0xFF,0xA0},                                      /* 24/12 bit LVDS Data off*/
  {VIACR,CR96,0xFF,0x00},
  {VIACR,CR97,0xFF,0x00},
  {VIACR,CR99,0xFF,0x00},
  {VIACR,CR9B,0xFF,0x00},
  {VIACR,CR9D,0xFF,0x80},
  {VIACR,CR9E,0xFF,0x80}
};
#define NUM_TOTAL_CN900_ModeXregs ARRAY_SIZE(CN900_ModeXregs)

struct io_reg KM400_ModeXregs[] = {
    {VIASR, SR10, 0xFF, 0x01},          /* Unlock Register                     */
    {VIASR, SR16, 0xFF, 0x08},          /* Display FIFO threshold Control      */
    {VIASR, SR17, 0xFF, 0x1F},          /* Display FIFO Control                */
    {VIASR, SR18, 0xFF, 0x4E},          /* GFX PREQ threshold                  */
    {VIASR, SR1A, 0xFF, 0x0a},          /* GFX PREQ threshold                  */
    {VIASR, SR1F, 0xFF, 0x00},          /* Memory Control 0                    */
    {VIASR, SR1B, 0xFF, 0xF0},          /* Power Management Control 0          */
    {VIASR, SR1E, 0x0F, 0x01},          /* Power Management Control            */
    {VIASR, SR20, 0xFF, 0x00},          /* Sequencer Arbiter Control 0         */
    {VIASR, SR21, 0xFF, 0x00},          /* Sequencer Arbiter Control 1         */
    {VIASR, SR22, 0xFF, 0x1F},          /* Display Arbiter Control 1           */
    {VIASR, SR2A, 0xF0, 0x00},          /* Power Management Control 5          */
    {VIASR, SR2D, 0xFF, 0xFF},          /* Power Management Control 1          */
    {VIASR, SR2E, 0xFF, 0xFF},          /* Power Management Control 2          */
    {VIACR, CR0A, 0xFF, 0x1E},          /* Cursor Start                        */
    {VIACR, CR0B, 0xFF, 0x00},          /* Cursor End                          */
    {VIACR, CR0E, 0xFF, 0x00},          /* Cursor Location High                */
    {VIACR, CR0F, 0xFF, 0x00},          /* Cursor Localtion Low                */
    {VIACR, CR33, 0xFF, 0x00},
    {VIACR, CR55, 0x80, 0x00},
    {VIACR, CR5D, 0x80, 0x00},
    {VIACR, CR36, 0xFF, 0x01},          /* Power Mangement 3                   */
    {VIACR, CR62, 0xFF, 0x00},          /* Secondary Display Starting Address  */
    {VIACR, CR63, 0xFF, 0x00},          /* Secondary Display Starting Address  */
    {VIACR, CR64, 0xFF, 0x00},          /* Secondary Display Starting Address  */
    {VIACR, CR68, 0xFF, 0x67},          /* Default FIFO For IGA2               */
    {VIACR, CR6A, 0x20, 0x20},          /* Extended FIFO On                    */
    {VIACR, CR7A, 0xFF, 0x01},          /* LCD Scaling Parameter 1             */
    {VIACR, CR7B, 0xFF, 0x02},          /* LCD Scaling Parameter 2             */
    {VIACR, CR7C, 0xFF, 0x03},          /* LCD Scaling Parameter 3             */
    {VIACR, CR7D, 0xFF, 0x04},          /* LCD Scaling Parameter 4             */
    {VIACR, CR7E, 0xFF, 0x07},          /* LCD Scaling Parameter 5             */
    {VIACR, CR7F, 0xFF, 0x0A},          /* LCD Scaling Parameter 6             */
    {VIACR, CR80, 0xFF, 0x0D},          /* LCD Scaling Parameter 7             */
    {VIACR, CR81, 0xFF, 0x13},          /* LCD Scaling Parameter 8             */
    {VIACR, CR82, 0xFF, 0x16},          /* LCD Scaling Parameter 9             */
    {VIACR, CR83, 0xFF, 0x19},          /* LCD Scaling Parameter 10            */
    {VIACR, CR84, 0xFF, 0x1C},          /* LCD Scaling Parameter 11            */
    {VIACR, CR85, 0xFF, 0x1D},          /* LCD Scaling Parameter 12            */
    {VIACR, CR86, 0xFF, 0x1E},          /* LCD Scaling Parameter 13            */
    {VIACR, CR87, 0xFF, 0x1F},          /* LCD Scaling Parameter 14            */
    {VIACR, CR88, 0xFF, 0x40},          /* LCD Panel Type                      */
    {VIACR, CR89, 0xFF, 0x00},          /* LCD Timing Control 0                */
    {VIACR, CR8A, 0xFF, 0x88},          /* LCD Timing Control 1                */
    {VIACR, CR8B, 0xFF, 0x2D},          /* LCD Power Sequence Control 0        */
    {VIACR, CR8C, 0xFF, 0x2D},          /* LCD Power Sequence Control 1        */
    {VIACR, CR8D, 0xFF, 0xC8},          /* LCD Power Sequence Control 2        */
    {VIACR, CR8E, 0xFF, 0x36},          /* LCD Power Sequence Control 3        */
    {VIACR, CR8F, 0xFF, 0x00},          /* LCD Power Sequence Control 4        */
    {VIACR, CR90, 0xFF, 0x10},          /* LCD Power Sequence Control 5        */
    {VIACR, CR91, 0xFF, 0xA0},          /* 24/12 bit LVDS Data off             */
    {VIACR, CR96, 0xFF, 0x03},          /* TV on DVP0        ; DVP0 Clock Skew */
    {VIACR, CR97, 0xFF, 0x03},          /* TV on DFP high    ; DFPH Clock Skew */
    {VIACR, CR99, 0xFF, 0x03},          /* DFP low           ; DFPL Clock Skew */
    {VIACR, CR9B, 0xFF, 0x07}           /* DVI on DVP1       ; DVP1 Clock Skew */
};
#define NUM_TOTAL_KM400_ModeXregs ARRAY_SIZE(KM400_ModeXregs)

/* For VT3324: Common Setting for Video Mode */
struct io_reg CX700_ModeXregs[] = {
    {VIASR, SR10, 0xFF, 0x01},
    {VIASR, SR15, 0x02, 0x02},
    {VIASR, SR16, 0xBF, 0x08},
    {VIASR, SR17, 0xFF, 0x1F},
    {VIASR, SR18, 0xFF, 0x4E},
    {VIASR, SR1A, 0xFB, 0x08},
    {VIASR, SR1B, 0xFF, 0xF0},
    {VIASR, SR1E, 0x0F, 0x01},
    {VIASR, SR2A, 0xF0, 0x00},
    {VIACR, CR0A, 0xFF, 0x1E},          /* Cursor Start                        */
    {VIACR, CR0B, 0xFF, 0x00},          /* Cursor End                          */
    {VIACR, CR0E, 0xFF, 0x00},          /* Cursor Location High                */
    {VIACR, CR0F, 0xFF, 0x00},          /* Cursor Localtion Low                */
    {VIACR, CR32, 0xFF, 0x00},
    {VIACR, CR33, 0xFF, 0x00},
    {VIACR, CR34, 0xFF, 0x00},
    {VIACR, CR35, 0xFF, 0x00},
    {VIACR, CR36, 0x08, 0x00},
    {VIACR, CR47, 0xC8, 0x00},          /* Clear VCK Plus. */
    {VIACR, CR62, 0xFF, 0x00},          /* Secondary Display Starting Address  */
    {VIACR, CR63, 0xFF, 0x00},          /* Secondary Display Starting Address  */
    {VIACR, CR64, 0xFF, 0x00},          /* Secondary Display Starting Address  */
    {VIACR, CRA3, 0xFF, 0x00},          /* Secondary Display Starting Address  */
    {VIACR, CR69, 0xFF, 0x00},
    {VIACR, CR6A, 0xFF, 0x40},
    {VIACR, CR6B, 0xFF, 0x00},
    {VIACR, CR6C, 0xFF, 0x00},
    {VIACR, CR7A, 0xFF, 0x01},          /* LCD Scaling Parameter 1             */
    {VIACR, CR7B, 0xFF, 0x02},          /* LCD Scaling Parameter 2             */
    {VIACR, CR7C, 0xFF, 0x03},          /* LCD Scaling Parameter 3             */
    {VIACR, CR7D, 0xFF, 0x04},          /* LCD Scaling Parameter 4             */
    {VIACR, CR7E, 0xFF, 0x07},          /* LCD Scaling Parameter 5             */
    {VIACR, CR7F, 0xFF, 0x0A},          /* LCD Scaling Parameter 6             */
    {VIACR, CR80, 0xFF, 0x0D},          /* LCD Scaling Parameter 7             */
    {VIACR, CR81, 0xFF, 0x13},          /* LCD Scaling Parameter 8             */
    {VIACR, CR82, 0xFF, 0x16},          /* LCD Scaling Parameter 9             */
    {VIACR, CR83, 0xFF, 0x19},          /* LCD Scaling Parameter 10            */
    {VIACR, CR84, 0xFF, 0x1C},          /* LCD Scaling Parameter 11            */
    {VIACR, CR85, 0xFF, 0x1D},          /* LCD Scaling Parameter 12            */
    {VIACR, CR86, 0xFF, 0x1E},          /* LCD Scaling Parameter 13            */
    {VIACR, CR87, 0xFF, 0x1F},          /* LCD Scaling Parameter 14            */
    {VIACR, CR88, 0xFF, 0x40},          /* LCD Panel Type                      */
    {VIACR, CR89, 0xFF, 0x00},          /* LCD Timing Control 0                */
    {VIACR, CR8A, 0xFF, 0x88},          /* LCD Timing Control 1                */
    {VIACR, CRD4, 0xFF, 0x81},          /* Second power sequence control       */
    {VIACR, CR8B, 0xFF, 0x5D},          /* LCD Power Sequence Control 0        */
    {VIACR, CR8C, 0xFF, 0x2B},          /* LCD Power Sequence Control 1        */
    {VIACR, CR8D, 0xFF, 0x6F},          /* LCD Power Sequence Control 2        */
    {VIACR, CR8E, 0xFF, 0x2B},          /* LCD Power Sequence Control 3        */
    {VIACR, CR8F, 0xFF, 0x01},          /* LCD Power Sequence Control 4        */
    {VIACR, CR90, 0xFF, 0x01},          /* LCD Power Sequence Control 5        */
    {VIACR, CR91, 0xFF, 0x80},          /* 24/12 bit LVDS Data off             */
    {VIACR, CR96, 0xFF, 0x00},
    {VIACR, CR97, 0xFF, 0x00},
    {VIACR, CR99, 0xFF, 0x00},
    {VIACR, CR9B, 0xFF, 0x00},
    {VIACR, CRD2, 0xFF, 0x03}           /* LVDS0/LVDS1 Channel format.         */
};

#define NUM_TOTAL_CX700_ModeXregs ARRAY_SIZE(CX700_ModeXregs)

/* Video Mode Table */
/* Common Setting for Video Mode */
struct io_reg CLE266_ModeXregs[] = {
  {VIASR,SR1E,0xF0,0xF0},
  {VIASR,SR2A,0x0F,0x0F},
  {VIASR,SR15,0x02,0x02},
  {VIASR,SR16,0xBF,0x08},
  {VIASR,SR17,0xFF,0x1F},
  {VIASR,SR18,0xFF,0x4E},
  {VIASR,SR1A,0xFB,0x08},

  {VIACR,CR32,0xFF,0x00},
//  {VIACR,CR33,0xFF,0x08}, // for K800 prefetch mode
  {VIACR,CR34,0xFF,0x00},
  {VIACR,CR35,0xFF,0x00},
  {VIACR,CR36,0x08,0x00},
  {VIACR,CR6A,0xFF,0x80},
  {VIACR,CR6A,0xFF,0xC0},

  {VIACR,CR55,0x80,0x00},
  {VIACR,CR5D,0x80,0x00},

  {VIAGR,GR20,0xFF,0x00},
  {VIAGR,GR21,0xFF,0x00},
  {VIAGR,GR22,0xFF,0x00},
                            // LCD Parameters
  {VIACR,CR7A,0xFF,0x01},   // LCD Parameter 1
  {VIACR,CR7B,0xFF,0x02},   // LCD Parameter 2
  {VIACR,CR7C,0xFF,0x03},   // LCD Parameter 3
  {VIACR,CR7D,0xFF,0x04},   // LCD Parameter 4
  {VIACR,CR7E,0xFF,0x07},   // LCD Parameter 5
  {VIACR,CR7F,0xFF,0x0A},   // LCD Parameter 6
  {VIACR,CR80,0xFF,0x0D},   // LCD Parameter 7
  {VIACR,CR81,0xFF,0x13},   // LCD Parameter 8
  {VIACR,CR82,0xFF,0x16},   // LCD Parameter 9
  {VIACR,CR83,0xFF,0x19},   // LCD Parameter 10
  {VIACR,CR84,0xFF,0x1C},   // LCD Parameter 11
  {VIACR,CR85,0xFF,0x1D},   // LCD Parameter 12
  {VIACR,CR86,0xFF,0x1E},   // LCD Parameter 13
  {VIACR,CR87,0xFF,0x1F},   // LCD Parameter 14

};

#define NUM_TOTAL_CLE266_ModeXregs ARRAY_SIZE(CLE266_ModeXregs)

/* Mode:1024X768 */
struct io_reg PM1024x768[] = {
  {VIASR,0x16,0xBF,0x0C},
  {VIASR,0x18,0xFF,0x4C}
};

struct patch_table res_patch_table[]= {
  {VIA_RES_1024X768, ARRAY_SIZE(PM1024x768), PM1024x768}
};
#define NUM_TOTAL_PATCH_MODE ARRAY_SIZE(res_patch_table)

// struct VPITTable {
//  unsigned char  Misc;
//  unsigned char  SR[StdSR];
//  unsigned char  CR[StdCR];
//  unsigned char  GR[StdGR];
//  unsigned char  AR[StdAR];
// };

struct VPITTable VPIT = {
    // Msic
    0xC7,
    // Sequencer
    {0x01,0x0F,0x00,0x0E },
    // Graphic Controller
    {0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x0F,0xFF},
    // Attribute Controller
    {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
     0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
     0x01,0x00,0x0F,0x00}
};

/********************/
/* Mode Table       */
/********************/

// 640x480
struct crt_mode_table CRTM640x480[] = {
  //r_rate,vclk,hsp,vsp
  //HT,  HA,  HBS, HBE, HSS, HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60, CLK_25_175M ,M640X480_R60_HSP, M640X480_R60_VSP,\
  {800, 640, 648, 144, 656, 96,  525, 480, 480, 45,  490, 2}},
  {REFRESH_75, CLK_31_500M ,M640X480_R75_HSP, M640X480_R75_VSP,\
  {840, 640, 640, 200, 656, 64,  500, 480, 480, 20,  481, 3}},
  {REFRESH_85, CLK_36_000M ,M640X480_R85_HSP, M640X480_R85_VSP,\
  {832, 640, 640, 192, 696, 56,  509, 480, 480, 29,  481, 3}},
  {REFRESH_100,CLK_43_163M ,M640X480_R100_HSP, M640X480_R100_VSP,\
  {848, 640, 640, 208, 680, 64,  509, 480, 480, 29,  481, 3}}, //GTF
  {REFRESH_120,CLK_52_406M ,M640X480_R120_HSP, M640X480_R120_VSP,\
  {848, 640, 640, 208, 680, 64,  515, 480, 480, 35,  481, 3}}  //GTF
};

//720x480 (GTF)
struct crt_mode_table CRTM720x480[] = {
  //r_rate,vclk,hsp,vsp      
  //HT, HA,  HBS, HBE, HSS, HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_26_880M ,M720X480_R60_HSP, M720X480_R60_VSP,\
  {896, 720, 720, 176, 736, 72,  497, 480, 480, 17,  481, 3}}

};

//720x576 (GTF)
struct crt_mode_table CRTM720x576[] = {
  //r_rate,vclk,hsp,vsp
  //HT,  HA,  HBS, HBE, HSS, HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_32_668M ,M720X576_R60_HSP, M720X576_R60_VSP,\
  {912, 720, 720, 192, 744, 72,  597, 576, 576, 21,  577, 3}}
};

//800x480 (GTF)
struct crt_mode_table CRTM800x480[] = {
  //r_rate,vclk,hsp,vsp
  //HT,  HA,  HBS, HBE, HSS, HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_29_581M ,M800X480_R60_HSP,M800X480_R60_VSP,\
  {992, 800, 800, 192, 816, 80,  497, 480, 480, 17,  481, 3}}
};
// 800x600
struct crt_mode_table CRTM800x600[] = {
  //r_rate,vclk,hsp,vsp     
  //HT,   HA,  HBS, HBE, HSS, HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60, CLK_40_000M ,M800X600_R60_HSP, M800X600_R60_VSP,\
  {1056, 800, 800, 256, 840, 128, 628, 600, 600, 28,  601, 4}},
  {REFRESH_75, CLK_49_500M ,M800X600_R75_HSP, M800X600_R75_VSP,\
  {1056, 800, 800, 256, 816, 80,  625, 600, 600, 25,  601, 3}},
  {REFRESH_85, CLK_56_250M ,M800X600_R85_HSP, M800X600_R85_VSP,\
  {1048, 800, 800, 248, 832, 64,  631, 600, 600, 31,  601, 3}},
  {REFRESH_100,CLK_68_179M ,M800X600_R100_HSP, M800X600_R100_VSP,\
  {1072, 800, 800, 272, 848, 88,  636, 600, 600, 36,  601, 3}}, //GTF
  {REFRESH_120,CLK_83_950M ,M800X600_R120_HSP, M800X600_R120_VSP,\
  {1088, 800, 800, 288, 856, 88,  643, 600, 600, 43,  601, 3}}  //GTF
};
//848x480 (GTF)
struct crt_mode_table CRTM848x480[] = {
  //r_rate,vclk,hsp,vsp     
  //HT,   HA,  HBS, HBE, HSS, HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_31_490M ,M848X480_R60_HSP, M848X480_R60_VSP,\
  {1056, 848, 848, 208, 864, 88,  497, 480, 480, 17,  481, 3}}
};

//856x480 (GTF) convert to 852x480
struct crt_mode_table CRTM852x480[] = {
  //r_rate,vclk,hsp,vsp     
  //HT,   HA,  HBS, HBE, HSS, HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_31_728M ,M852X480_R60_HSP, M852X480_R60_VSP,\
  {1064, 856, 856, 208, 872, 88,  497, 480, 480, 17,  481, 3}}

};

//1024x512 (GTF)
struct crt_mode_table CRTM1024x512[] = {
  //r_rate,vclk,hsp,vsp     
  //HT,   HA,  HBS, HBE, HSS, HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_41_291M ,M1024X512_R60_HSP, M1024X512_R60_VSP,\
  {1296, 1024,1024,272, 1056,104, 531, 512, 512, 19,  513, 3}}

};

//1024x576 (GTF)
/*static struct crt_mode_table CRTM1024x576[] = {
  //r_rate,vclk,     HT,   HA,  HBS, HBE, HSS, HSE, VT,  VA,  VBS, VBE, VSS, VSE
  { 60,CLK_46_996M ,{1312, 1024,1024,288, 1064,104, 597, 576, 576, 21,  577, 3}}

};*/

// 1024x768
struct crt_mode_table CRTM1024x768[] = {
  //r_rate,vclk,hsp,vsp
  //HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_65_000M ,M1024X768_R60_HSP, M1024X768_R60_VSP,\
  {1344, 1024, 1024, 320, 1048, 136, 806, 768, 768, 38,  771, 6}},
  {REFRESH_75,CLK_78_750M ,M1024X768_R75_HSP, M1024X768_R75_VSP,\
  {1312, 1024, 1024, 288, 1040, 96,  800, 768, 768, 32,  769, 3}},
  {REFRESH_85,CLK_94_500M ,M1024X768_R85_HSP, M1024X768_R85_VSP,\
  {1376, 1024, 1024, 352, 1072, 96,  808, 768, 768, 40,  769, 3}},
  {REFRESH_100,CLK_133_308M,M1024X768_R100_HSP, M1024X768_R100_VSP,\
  {1392, 1024, 1024, 368, 1096, 112, 814, 768, 768, 46,  769, 3}} //GTF
};

// 1152x864
struct crt_mode_table CRTM1152x864[] = {
  //r_rate,vclk,hsp,vsp      
  //HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_75,CLK_108_000M ,M1152X864_R75_HSP, M1152X864_R75_VSP,\
  {1600, 1152,1152, 448, 1216, 128, 900, 864, 864, 36,  865, 3}}

};

// 1280x720 (GTF)
struct crt_mode_table CRTM1280x720[] = {
  //r_rate,vclk,hsp,vsp
  //HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE      
  {REFRESH_60,CLK_74_481M ,M1280X720_R60_HSP, M1280X720_R60_VSP,\
  {1664,1280, 1280, 384, 1336, 136, 746, 720, 720, 26,  721, 3}}
};

//1280x768 (GTF)
struct crt_mode_table CRTM1280x768[] = {
  //r_rate,vclk,hsp,vsp     
  //HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_80_136M ,M1280X768_R60_HSP, M1280X768_R60_VSP,\
  {1680,1280, 1280, 400, 1344, 136, 795, 768, 768, 27,  769, 3}}
};

//1280x960
struct crt_mode_table CRTM1280x960[] = {
  //r_rate,vclk,hsp,vsp
  //HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_108_000M ,M1280X960_R60_HSP, M1280X960_R60_VSP,\
  {1800,1280, 1280, 520, 1376, 112, 1000,960, 960, 40,  961, 3}}  
};

// 1280x1024
struct crt_mode_table CRTM1280x1024[] = {
  //r_rate,vclk,,hsp,vsp
  //HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_108_000M ,M1280X1024_R60_HSP, M1280X1024_R60_VSP,\
  {1688,1280, 1280, 408, 1328, 112, 1066,1024,1024,42,  1025,3}},
  {REFRESH_75,CLK_135_000M ,M1280X1024_R75_HSP, M1280X1024_R75_VSP,\
  {1688,1280, 1280, 408, 1296, 144, 1066,1024,1024,42,  1025,3}},
  {REFRESH_85,CLK_157_500M ,M1280X1024_R85_HSP, M1280X1024_R85_VSP,\
  {1728,1280, 1280, 448, 1344, 160, 1072,1024,1024,48,  1025,3}}
};

/* 1366x768 (GTF) */
struct crt_mode_table CRTM1366x768[] = {
  // r_rate,  vclk, hsp, vsp
  // HT,  HA,  HBS, HBE, HSS, HSE, VT,  VA,  VBS, VBE, VSS, VSE 
  {REFRESH_60, CLK_85_860M, M1368X768_R60_HSP, M1368X768_R60_VSP,\
  {1800,1366, 1366, 432, 1440, 144, 795, 768, 768, 27,  769, 3}}
};

//1368x768 (GTF)
/*static struct crt_mode_table CRTM1368x768[] = {
  //r_rate,vclk,     HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE
  { 60,CLK_85_860M ,{1800,1368, 1368, 432, 1440, 144, 795, 768, 768, 27,  769, 3}}
};*/

//1440x1050 (GTF)
struct crt_mode_table CRTM1440x1050[] = {
  //r_rate,vclk,hsp,vsp      
  //HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60 ,CLK_125_104M ,M1440X1050_R60_HSP, M1440X1050_R60_VSP,\
  {1936,1440, 1440, 496, 1536, 152, 1077,1040,1040,37,  1041,3}}
};

// 1600x1200
struct crt_mode_table CRTM1600x1200[] = {
  //r_rate,vclk,hsp,vsp
  //HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE      
  {REFRESH_60 ,CLK_162_000M ,M1600X1200_R60_HSP, M1600X1200_R60_VSP,\
  {2160,1600, 1600, 560, 1664, 192, 1250,1200,1200,50,  1201,3}},
  {REFRESH_75 ,CLK_202_500M ,M1600X1200_R75_HSP, M1600X1200_R75_VSP,\
  {2160,1600, 1600, 560, 1664, 192, 1250,1200,1200,50,  1201,3}}

};

// 1920x1080 (GTF)
struct crt_mode_table CRTM1920x1080[] = {
  //r_rate,vclk,hsp,vsp
  //HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE                           
  {REFRESH_60,CLK_172_798M ,M1920X1080_R60_HSP, M1920X1080_R60_VSP,\
  {2576,1920, 1920, 656, 2040, 208, 1118,1080,1080,38, 1081, 3}}
};

// 1920x1440
struct crt_mode_table CRTM1920x1440[] = {
  //r_rate,vclk,hsp,vsp
  //HT,  HA,   HBS,  HBE, HSS,  HSE, VT,  VA,  VBS, VBE, VSS, VSE
  {REFRESH_60,CLK_234_000M ,M1920X1440_R60_HSP, M1920X1440_R60_VSP,\
  {2600,1920, 1920, 680, 2048, 208, 1500,1440,1440,60,  1441,3}},
  {REFRESH_75,CLK_297_500M ,M1920X1440_R75_HSP, M1920X1440_R75_VSP,\
  {2640,1920, 1920, 720, 2064, 224, 1500,1440,1440,60,  1441,3}}
};

/* 1400x1050 (VESA) */
struct crt_mode_table CRTM1400x1050[] = {
  /* r_rate,          vclk,              hsp,             vsp   */
  /* HT,  HA,  HBS, HBE, HSS, HSE,    VT,  VA,  VBS, VBE,  VSS, VSE */
  {REFRESH_60,CLK_108_000M, M1400X1050_R60_HSP, M1400X1050_R60_VSP,
  {1688,1400, 1400, 288, 1448, 112, 1066,1050, 1050,  16, 1051, 3}}
};

/* Video Mode Table */
// struct VideoModeTable {
//  int                               ModeIndex;
//  struct crt_mode_table             *crtc;
//  int                               mode_array;
// };
struct VideoModeTable CLE266Modes[] = {
   /* Display : 640x480 */
   { VIA_RES_640X480,  CRTM640x480, ARRAY_SIZE(CRTM640x480)},

   /* Display : 720x480 (GTF)*/
   { VIA_RES_720X480,  CRTM720x480, ARRAY_SIZE(CRTM720x480)},

   /* Display : 720x576 (GTF)*/
   { VIA_RES_720X576,  CRTM720x576, ARRAY_SIZE(CRTM720x576)},

   /* Display : 800x600 */
   { VIA_RES_800X600,  CRTM800x600, ARRAY_SIZE(CRTM800x600)},

   /* Display : 800x480 (GTF)*/
   { VIA_RES_800X480,  CRTM800x480, ARRAY_SIZE(CRTM800x480)},

   /* Display : 848x480 (GTF)*/
   { VIA_RES_848X480,  CRTM848x480, ARRAY_SIZE(CRTM848x480)},

   /* Display : 852x480 (GTF)*/
   { VIA_RES_856X480,  CRTM852x480, ARRAY_SIZE(CRTM852x480)},

   /* Display : 1024x512 (GTF)*/
   { VIA_RES_1024X512, CRTM1024x512, ARRAY_SIZE(CRTM1024x512)},

    /* Display : 1024x576 (GTF)*/
   //{ VIA_RES_1024X576, CRTM1024x576, ARRAY_SIZE(CRTM1024x576)},

   /* Display : 1024x768 */
   { VIA_RES_1024X768, CRTM1024x768, ARRAY_SIZE(CRTM1024x768)},

   /* Display : 1152x864 */
   { VIA_RES_1152X864, CRTM1152x864, ARRAY_SIZE(CRTM1152x864)},

   /* Display : 1280x768 (GTF)*/
   { VIA_RES_1280X768, CRTM1280x768, ARRAY_SIZE(CRTM1280x768)},

   /* Display : 1280x800 (GTF)*/
   //{ M1280x800, CRTM1280x800, ARRAY_SIZE(CRTM1280x800)},

   /* Display : 1280x960 */
   { VIA_RES_1280X960, CRTM1280x960, ARRAY_SIZE(CRTM1280x960)},

   /* Display : 1280x1024 */
   { VIA_RES_1280X1024, CRTM1280x1024,ARRAY_SIZE(CRTM1280x1024)},

   /* Display : 1368x768 (GTF)*/
   //{ M1368x768,CRTM1368x768,ARRAY_SIZE(CRTM1368x768)},
/* Display : 1366x768 (GTF)*/
   { VIA_RES_1366X768,CRTM1366x768,ARRAY_SIZE(CRTM1366x768)},

   /* Display : 1440x1050 (GTF)*/
   { VIA_RES_1440X1050, CRTM1440x1050, ARRAY_SIZE(CRTM1440x1050)},

   /* Display : 1600x1200 */
   { VIA_RES_1600X1200, CRTM1600x1200, ARRAY_SIZE(CRTM1600x1200)},

   /* Display : 1920x1440 */
   { VIA_RES_1920X1440, CRTM1920x1440, ARRAY_SIZE(CRTM1920x1440)},

   /* Display : 1280x720 */
   { VIA_RES_1280X720, CRTM1280x720, ARRAY_SIZE(CRTM1280x720)},

   /* Display : 1920x1080 */
   { VIA_RES_1920X1080, CRTM1920x1080, ARRAY_SIZE(CRTM1920x1080)},

   /* Display : 1400x1050 */
   { VIA_RES_1400X1050, CRTM1400x1050, ARRAY_SIZE(CRTM1400x1050)}
};

#define NUM_TOTAL_MODETABLE ARRAY_SIZE(CLE266Modes)


#endif /* _DEV_PCI_UNICHROMEMODE_H */
