/* $NetBSD: unichromereg.h,v 1.1 2006/08/02 01:44:09 jmcneill Exp $ */

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

#ifndef _DEV_PCI_UNICHROMEREG_H
#define _DEV_PCI_UNICHROMEREG_H

/* Define Return Value */
#define FAIL        -1
#define OK          1

/* S.T.Chen[2005.12.26]: Define Boolean Value */
#ifndef bool
typedef int bool;
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

/* Define Bit Field */
#define BIT0    0x01
#define BIT1    0x02
#define BIT2    0x04
#define BIT3    0x08
#define BIT4    0x10
#define BIT5    0x20
#define BIT6    0x40
#define BIT7    0x80

/* Video Memory Size */
#define VIDEO_MEMORY_SIZE_16M    0x1000000

// Definition Mode Index
#define     VIA_RES_640X480                 0
#define     VIA_RES_800X600                 1
#define     VIA_RES_1024X768                2
#define     VIA_RES_1152X864                3
#define     VIA_RES_1280X1024               4
#define     VIA_RES_1600X1200               5
#define     VIA_RES_1440X1050               6
#define     VIA_RES_1280X768                7
#define     VIA_RES_1280X960                8
#define     VIA_RES_1920X1440               9
#define     VIA_RES_848X480                 10
#define     VIA_RES_1400X1050               11
#define     VIA_RES_720X480                 12
#define     VIA_RES_720X576                 13
#define     VIA_RES_1024X512                14
#define     VIA_RES_856X480                 15
#define     VIA_RES_1024X576                16
#define     VIA_RES_640X400                 17
#define     VIA_RES_1280X720                18
#define     VIA_RES_1920X1080               19
#define     VIA_RES_800X480                 20
#define	  VIA_RES_1366X768			21
#define     VIA_RES_INVALID                 255


// standard VGA IO port
#define	VIA_REGBASE	0x3C0
#define VIAAR       0x000
#define VIARMisc    0x00C
#define VIAWMisc    0x002
#define VIAStatus   0x01A
#define VIACR       0x014
#define VIASR       0x004
#define VIAGR       0x00E

#define StdCR       0x19
#define StdSR       0x04
#define StdGR       0x09
#define StdAR       0x14

#define PatchCR     11

/* Display path */
#define IGA1        1
#define IGA2        2
#define IGA1_IGA2   3

/* Define Color Depth  */
#define MODE_8BPP       1
#define MODE_16BPP      2
#define MODE_32BPP      4

#define GR20    0x20
#define GR21    0x21
#define GR22    0x22


/* Sequencer Registers */
#define SR01    0x01
#define SR10    0x10
#define SR12    0x12
#define SR15    0x15
#define SR16    0x16
#define SR17    0x17
#define SR18    0x18
#define SR1B    0x1B
#define SR1A    0x1A
#define SR1C    0x1C
#define SR1D    0x1D
#define SR1E    0x1E
#define SR1F    0x1F
#define SR20    0x20
#define SR21    0x21
#define SR22    0x22
#define SR2A    0x2A
#define SR2D    0x2D
#define SR2E    0x2E

#define SR30    0x30
#define SR39    0x39
#define SR3D    0x3D
#define SR3E    0x3E
#define SR3F    0x3F
#define SR40    0x40
#define SR44    0x44
#define SR45    0x45
#define SR46    0x46
#define SR47    0x47
#define SR48    0x48
#define SR49    0x49
#define SR4A    0x4A
#define SR4B    0x4B
#define SR4C    0x4C
#define SR52    0x52
#define SR5E    0x5E


/* CRT Controller Registers */
#define CR00    0x00
#define CR01    0x01
#define CR02    0x02
#define CR03    0x03
#define CR04    0x04
#define CR05    0x05
#define CR06    0x06
#define CR07    0x07
#define CR08    0x08
#define CR09    0x09
#define CR0A    0x0A
#define CR0B    0x0B
#define CR0C    0x0C
#define CR0D    0x0D
#define CR0E    0x0E
#define CR0F    0x0F
#define CR10    0x10
#define CR11    0x11
#define CR12    0x12
#define CR13    0x13
#define CR14    0x14
#define CR15    0x15
#define CR16    0x16
#define CR17    0x17
#define CR18    0x18

/* Extend CRT Controller Registers */
#define CR30    0x30
#define CR31    0x31
#define CR32    0x32
#define CR33    0x33
#define CR34    0x34
#define CR35    0x35
#define CR36    0x36
#define CR37    0x37
#define CR38    0x38
#define CR39    0x39
#define CR3A    0x3A
#define CR3B    0x3B
#define CR3C    0x3C
#define CR3D    0x3D
#define CR3E    0x3E
#define CR3F    0x3F
#define CR40    0x40
#define CR41    0x41
#define CR42    0x42
#define CR43    0x43
#define CR44    0x44
#define CR45    0x45
#define CR46    0x46
#define CR47    0x47
#define CR48    0x48
#define CR49    0x49
#define CR4A    0x4A
#define CR4B    0x4B
#define CR4C    0x4C
#define CR4D    0x4D
#define CR4E    0x4E
#define CR4F    0x4F
#define CR50    0x50
#define CR51    0x51
#define CR52    0x52
#define CR53    0x53
#define CR54    0x54
#define CR55    0x55
#define CR56    0x56
#define CR57    0x57
#define CR58    0x58
#define CR59    0x59
#define CR5A    0x5A
#define CR5B    0x5B
#define CR5C    0x5C
#define CR5D    0x5D
#define CR5E    0x5E
#define CR5F    0x5F
#define CR60    0x60
#define CR61    0x61
#define CR62    0x62
#define CR63    0x63
#define CR64    0x64
#define CR65    0x65
#define CR66    0x66
#define CR67    0x67
#define CR68    0x68
#define CR69    0x69
#define CR6A    0x6A
#define CR6B    0x6B
#define CR6C    0x6C
#define CR6D    0x6D
#define CR6E    0x6E
#define CR6F    0x6F
#define CR70    0x70
#define CR71    0x71
#define CR72    0x72
#define CR73    0x73
#define CR74    0x74
#define CR75    0x75
#define CR76    0x76
#define CR77    0x77
#define CR78    0x78
#define CR79    0x79
#define CR7A    0x7A
#define CR7B    0x7B
#define CR7C    0x7C
#define CR7D    0x7D
#define CR7E    0x7E
#define CR7F    0x7F
#define CR80    0x80
#define CR81    0x81
#define CR82    0x82
#define CR83    0x83
#define CR84    0x84
#define CR85    0x85
#define CR86    0x86
#define CR87    0x87
#define CR88    0x88
#define CR89    0x89
#define CR8A    0x8A
#define CR8B    0x8B
#define CR8C    0x8C
#define CR8D    0x8D
#define CR8E    0x8E
#define CR8F    0x8F
#define CR90    0x90
#define CR91    0x91
#define CR92    0x92
#define CR93    0x93
#define CR94    0x94
#define CR95    0x95
#define CR96    0x96
#define CR97    0x97
#define CR98    0x98
#define CR99    0x99
#define CR9A    0x9A
#define CR9B    0x9B
#define CR9C    0x9C
#define CR9D    0x9D
#define CR9E    0x9E
#define CR9F    0x9F
#define CRA0    0xA0
#define CRA1    0xA1
#define CRA2    0xA2
#define CRA3    0xA3
#define CRD2    0xD2
#define CRD3    0xD3
#define CRD4    0xD4

/* LUT Table*/
#define LUT_DATA             0x09        /* DACDATA */
#define LUT_INDEX_READ       0x07        /* DACRX */
#define LUT_INDEX_WRITE      0x08        /* DACWX */
#define DACMASK              0x06

/* Definition Device */
#define DEVICE_CRT  0x01
#define DEVICE_TV   0x02
#define DEVICE_DVI  0x03
#define DEVICE_LCD  0x04

/* Device output interface */
#define INTERFACE_NONE          0x00
#define INTERFACE_ANALOG_RGB    0x01
#define INTERFACE_DVP0          0x02
#define INTERFACE_DVP1          0x03
#define INTERFACE_DFP_HIGH      0x04
#define INTERFACE_DFP_LOW       0x05
#define INTERFACE_DFP           0x06
#define INTERFACE_LVDS0         0x07
#define INTERFACE_LVDS1         0x08
#define INTERFACE_LVDS0LVDS1    0x09
#define INTERFACE_TMDS          0x0A

/* Definition Refresh Rate */
#define REFRESH_60      60
#define REFRESH_75      75
#define REFRESH_85      85
#define REFRESH_100     100
#define REFRESH_120     120

/* Definition Sync Polarity*/
#define NEGATIVE        1
#define POSITIVE        0

//640x480@60 Sync Polarity (VESA Mode)
#define M640X480_R60_HSP        NEGATIVE            
#define M640X480_R60_VSP        NEGATIVE

//640x480@75 Sync Polarity (VESA Mode)
#define M640X480_R75_HSP        NEGATIVE
#define M640X480_R75_VSP        NEGATIVE

//640x480@85 Sync Polarity (VESA Mode)
#define M640X480_R85_HSP        NEGATIVE
#define M640X480_R85_VSP        NEGATIVE

//640x480@100 Sync Polarity (GTF Mode)
#define M640X480_R100_HSP       NEGATIVE
#define M640X480_R100_VSP       POSITIVE

//640x480@120 Sync Polarity (GTF Mode)
#define M640X480_R120_HSP       NEGATIVE
#define M640X480_R120_VSP       POSITIVE

//720x480@60 Sync Polarity  (GTF Mode)
#define M720X480_R60_HSP        NEGATIVE            
#define M720X480_R60_VSP        POSITIVE

//720x576@60 Sync Polarity  (GTF Mode)
#define M720X576_R60_HSP        NEGATIVE            
#define M720X576_R60_VSP        POSITIVE

//800x600@60 Sync Polarity (VESA Mode)
#define M800X600_R60_HSP        POSITIVE            
#define M800X600_R60_VSP        POSITIVE

//800x600@75 Sync Polarity (VESA Mode)
#define M800X600_R75_HSP        POSITIVE            
#define M800X600_R75_VSP        POSITIVE

//800x600@85 Sync Polarity (VESA Mode)
#define M800X600_R85_HSP        POSITIVE            
#define M800X600_R85_VSP        POSITIVE

//800x600@100 Sync Polarity (GTF Mode)
#define M800X600_R100_HSP       NEGATIVE            
#define M800X600_R100_VSP       POSITIVE

//800x600@120 Sync Polarity (GTF Mode)
#define M800X600_R120_HSP       NEGATIVE            
#define M800X600_R120_VSP       POSITIVE

//800x480@60 Sync Polarity  (GTF Mode)
#define M800X480_R60_HSP        NEGATIVE            
#define M800X480_R60_VSP        POSITIVE

//848x480@60 Sync Polarity  (GTF Mode)
#define M848X480_R60_HSP        NEGATIVE            
#define M848X480_R60_VSP        POSITIVE

//852x480@60 Sync Polarity  (GTF Mode)
#define M852X480_R60_HSP        NEGATIVE            
#define M852X480_R60_VSP        POSITIVE

//1024x512@60 Sync Polarity (GTF Mode)
#define M1024X512_R60_HSP       NEGATIVE            
#define M1024X512_R60_VSP       POSITIVE

//1024x768@60 Sync Polarity (VESA Mode)
#define M1024X768_R60_HSP       NEGATIVE            
#define M1024X768_R60_VSP       NEGATIVE

//1024x768@75 Sync Polarity (VESA Mode)
#define M1024X768_R75_HSP       POSITIVE            
#define M1024X768_R75_VSP       POSITIVE

//1024x768@85 Sync Polarity (VESA Mode)
#define M1024X768_R85_HSP       POSITIVE            
#define M1024X768_R85_VSP       POSITIVE

//1024x768@100 Sync Polarity (GTF Mode)
#define M1024X768_R100_HSP      NEGATIVE            
#define M1024X768_R100_VSP      POSITIVE

//1152x864@75 Sync Polarity (VESA Mode)
#define M1152X864_R75_HSP       POSITIVE            
#define M1152X864_R75_VSP       POSITIVE

//1280x720@60 Sync Polarity  (GTF Mode)
#define M1280X720_R60_HSP       NEGATIVE            
#define M1280X720_R60_VSP       POSITIVE

//1280x768@60 Sync Polarity  (GTF Mode)
#define M1280X768_R60_HSP       NEGATIVE            
#define M1280X768_R60_VSP       POSITIVE

//1280x960@60 Sync Polarity (VESA Mode)
#define M1280X960_R60_HSP       POSITIVE            
#define M1280X960_R60_VSP       POSITIVE

//1280x1024@60 Sync Polarity (VESA Mode)
#define M1280X1024_R60_HSP      POSITIVE                   
#define M1280X1024_R60_VSP      POSITIVE

/* 1368x768@60 Sync Polarity (VESA Mode) */
#define M1368X768_R60_HSP       NEGATIVE
#define M1368X768_R60_VSP       POSITIVE

//1280x1024@75 Sync Polarity (VESA Mode)
#define M1280X1024_R75_HSP      POSITIVE                   
#define M1280X1024_R75_VSP      POSITIVE 

//1280x1024@85 Sync Polarity (VESA Mode)
#define M1280X1024_R85_HSP      POSITIVE                   
#define M1280X1024_R85_VSP      POSITIVE

//1440x1050@60 Sync Polarity (GTF Mode)
#define M1440X1050_R60_HSP      NEGATIVE                   
#define M1440X1050_R60_VSP      POSITIVE  

//1600x1200@60 Sync Polarity (VESA Mode) 
#define M1600X1200_R60_HSP      POSITIVE                  
#define M1600X1200_R60_VSP      POSITIVE       

//1600x1200@75 Sync Polarity (VESA Mode)     
#define M1600X1200_R75_HSP      POSITIVE                  
#define M1600X1200_R75_VSP      POSITIVE      

//1920x1080@60 Sync Polarity (GTF Mode)
#define M1920X1080_R60_HSP      NEGATIVE            
#define M1920X1080_R60_VSP      POSITIVE

//1920x1440@60 Sync Polarity (VESA Mode) 
#define M1920X1440_R60_HSP      NEGATIVE            
#define M1920X1440_R60_VSP      POSITIVE 

//1920x1440@75 Sync Polarity (VESA Mode)
#define M1920X1440_R75_HSP      NEGATIVE            
#define M1920X1440_R75_VSP      POSITIVE

/* 1400x1050@60 Sync Polarity (VESA Mode) */
#define M1400X1050_R60_HSP      NEGATIVE
#define M1400X1050_R60_VSP      NEGATIVE


/* define PLL index: */
#define CLK_25_175M     25175000
#define CLK_26_880M     26880000
#define CLK_29_581M     29581000
#define CLK_31_490M     31490000
#define CLK_31_500M     31500000
#define CLK_31_728M     31728000
#define CLK_32_668M     32688000
#define CLK_36_000M     36000000
#define CLK_40_000M     40000000
#define CLK_41_291M     41291000
#define CLK_43_163M     43163000
//#define CLK_46_996M     46996000
#define CLK_49_500M     49500000
#define CLK_52_406M     52406000
#define CLK_56_250M     56250000
#define CLK_65_000M     65000000
#define CLK_68_179M     68179000
#define CLK_78_750M     78750000
#define CLK_80_136M     80136000
#define CLK_83_950M     83950000
#define CLK_85_860M     85860000
#define CLK_94_500M     94500000
#define CLK_108_000M    108000000
#define CLK_125_104M    125104000
#define CLK_133_308M    133308000
#define CLK_135_000M    135000000
//#define CLK_148_500M    148500000
#define CLK_157_500M    157500000
#define CLK_162_000M    162000000
#define CLK_202_500M    202500000
#define CLK_234_000M    234000000
#define CLK_297_500M    297500000
#define CLK_74_481M     74481000
#define CLK_172_798M    172798000


// CLE266 PLL value
#define CLE266_PLL_25_175M     0x0000C763
#define CLE266_PLL_26_880M     0x0000440F
#define CLE266_PLL_29_581M     0x00008421
#define CLE266_PLL_31_490M     0x00004721
#define CLE266_PLL_31_500M     0x0000C3B5
#define CLE266_PLL_31_728M     0x0000471F
#define CLE266_PLL_32_668M     0x0000C449
#define CLE266_PLL_36_000M     0x0000C5E5
#define CLE266_PLL_40_000M     0x0000C459
#define CLE266_PLL_41_291M     0x00004417
#define CLE266_PLL_43_163M     0x0000C579
//#define CLE266_PLL_46_996M     0x0000C4E9
#define CLE266_PLL_49_500M     0x00008653
#define CLE266_PLL_52_406M     0x0000C475
#define CLE266_PLL_56_250M     0x000047B7
#define CLE266_PLL_65_000M     0x000086ED
#define CLE266_PLL_68_179M     0x00000413
#define CLE266_PLL_78_750M     0x00004321
#define CLE266_PLL_80_136M     0x0000051C
#define CLE266_PLL_83_950M     0x00000729
#define CLE266_PLL_85_860M     0x00004754
#define CLE266_PLL_94_500M     0x00000521
#define CLE266_PLL_108_000M    0x00008479
#define CLE266_PLL_125_104M    0x000006B5
#define CLE266_PLL_133_308M    0x0000465F
#define CLE266_PLL_135_000M    0x0000455E
//#define CLE266_PLL_148_500M    0x0000
#define CLE266_PLL_157_500M    0x000005B7
#define CLE266_PLL_162_000M    0x00004571
#define CLE266_PLL_202_500M    0x00000763
#define CLE266_PLL_234_000M    0x00000662
#define CLE266_PLL_297_500M    0x000005E6
#define CLE266_PLL_74_481M     0x0000051A
#define CLE266_PLL_172_798M    0x00004579

// K800 PLL value
#define K800_PLL_25_175M     0x00539001
#define K800_PLL_26_880M     0x001C8C80
#define K800_PLL_29_581M     0x00409080
#define K800_PLL_31_490M     0x006F9001
#define K800_PLL_31_500M     0x008B9002
#define K800_PLL_31_728M     0x00AF9003
#define K800_PLL_32_668M     0x00909002
#define K800_PLL_36_000M     0x009F9002
#define K800_PLL_40_000M     0x00578C02
#define K800_PLL_41_291M     0x00438C01
#define K800_PLL_43_163M     0x00778C03
//#define K800_PLL_46_996M     0x00000000
#define K800_PLL_49_500M     0x00518C01
#define K800_PLL_52_406M     0x00738C02
#define K800_PLL_56_250M     0x007C8C02
#define K800_PLL_65_000M     0x006B8C01
#define K800_PLL_68_179M     0x00708C01
#define K800_PLL_78_750M     0x00408801
#define K800_PLL_80_136M     0x00428801
#define K800_PLL_83_950M     0x00738803
#define K800_PLL_85_860M     0x00768883
#define K800_PLL_94_500M     0x00828803
#define K800_PLL_108_000M    0x00778882
#define K800_PLL_125_104M    0x00688801
#define K800_PLL_133_308M    0x005D8801
#define K800_PLL_135_000M    0x001A4081
//#define K800_PLL_148_500M    0x0000
#define K800_PLL_157_500M    0x00142080
#define K800_PLL_162_000M    0x006F8483
#define K800_PLL_202_500M    0x00538481
#define K800_PLL_234_000M    0x00608401
#define K800_PLL_297_500M    0x00A48402
#define K800_PLL_74_481M     0x007B8C81
#define K800_PLL_172_798M    0x00778483

/* PLL for VT3324 */
#define CX700_25_175M     0x008B1003
#define CX700_26_719M     0x00931003
#define CX700_26_880M     0x00941003
#define CX700_29_581M     0x00A49003
#define CX700_31_490M     0x00AE1003
#define CX700_31_500M     0x00AE1003
#define CX700_31_728M     0x00AF1003
#define CX700_32_668M     0x00B51003
#define CX700_36_000M     0x00C81003
#define CX700_40_000M     0x006E0C03
#define CX700_41_291M     0x00710C03
#define CX700_43_163M     0x00770C03
#define CX700_49_500M     0x00880C03
#define CX700_52_406M     0x00730C02
#define CX700_56_250M     0x009B0C03
#define CX700_65_000M     0x006B0C01
#define CX700_68_179M     0x00BC0C03
#define CX700_74_481M     0x00CE0C03
#define CX700_78_750M     0x006C0803
#define CX700_80_136M     0x006E0803
#define CX700_83_375M     0x005B0882
#define CX700_83_950M     0x00730803
#define CX700_85_860M     0x00760803
#define CX700_94_500M     0x00820803
#define CX700_108_000M    0x00950803
#define CX700_125_104M    0x00AD0803
#define CX700_133_308M    0x00930802
#define CX700_135_000M    0x00950802
#define CX700_157_500M    0x006C0403
#define CX700_162_000M    0x006F0403
#define CX700_172_798M    0x00770403
#define CX700_202_500M    0x008C0403
#define CX700_234_000M    0x00600401
#define CX700_297_500M    0x00CE0403

/* Definition CRTC Timing Index */
#define H_TOTAL_INDEX               0
#define H_ADDR_INDEX                1
#define H_BLANK_START_INDEX         2
#define H_BLANK_END_INDEX           3
#define H_SYNC_START_INDEX          4
#define H_SYNC_END_INDEX            5
#define V_TOTAL_INDEX               6
#define V_ADDR_INDEX                7
#define V_BLANK_START_INDEX         8
#define V_BLANK_END_INDEX           9
#define V_SYNC_START_INDEX          10
#define V_SYNC_END_INDEX            11
#define H_TOTAL_SHADOW_INDEX        12
#define H_BLANK_END_SHADOW_INDEX    13
#define V_TOTAL_SHADOW_INDEX        14
#define V_ADDR_SHADOW_INDEX         15
#define V_BLANK_SATRT_SHADOW_INDEX  16
#define V_BLANK_END_SHADOW_INDEX    17
#define V_SYNC_SATRT_SHADOW_INDEX   18
#define V_SYNC_END_SHADOW_INDEX     19

// Definition Video Mode Pixel Clock (picoseconds)
#define RES_640X480_60HZ_PIXCLOCK    39722
#define RES_640X480_75HZ_PIXCLOCK    31747
#define RES_640X480_85HZ_PIXCLOCK    27777
#define RES_640X480_100HZ_PIXCLOCK   23168
#define RES_640X480_120HZ_PIXCLOCK   19081
#define RES_720X480_60HZ_PIXCLOCK    37020
#define RES_720X576_60HZ_PIXCLOCK    30611
#define RES_800X600_60HZ_PIXCLOCK    25000
#define RES_800X600_75HZ_PIXCLOCK    20203
#define RES_800X600_85HZ_PIXCLOCK    17777
#define RES_800X600_100HZ_PIXCLOCK   14815
#define RES_800X600_120HZ_PIXCLOCK   11912
#define RES_800X480_60HZ_PIXCLOCK    33805
#define RES_848X480_60HZ_PIXCLOCK    31756
#define RES_856X480_60HZ_PIXCLOCK    31518
#define RES_1024X512_60HZ_PIXCLOCK   24218
#define RES_1024X768_60HZ_PIXCLOCK   15385
#define RES_1024X768_75HZ_PIXCLOCK   12699
#define RES_1024X768_85HZ_PIXCLOCK   10582
#define RES_1024X768_100HZ_PIXCLOCK  9091
#define RES_1152X864_70HZ_PIXCLOCK   10000
#define RES_1152X864_75HZ_PIXCLOCK   9091
#define RES_1280X768_60HZ_PIXCLOCK   12480
#define RES_1280X960_60HZ_PIXCLOCK   9259
#define RES_1280X1024_60HZ_PIXCLOCK  9260
#define RES_1280X1024_75HZ_PIXCLOCK  7408
#define RES_1280X768_85HZ_PIXCLOCK   6349
#define RES_1440X1050_60HZ_PIXCLOCK  7993
#define RES_1600X1200_60HZ_PIXCLOCK  6411
#define RES_1600X1200_75HZ_PIXCLOCK  4938
#define RES_1280X720_60HZ_PIXCLOCK   13426
#define RES_1920X1080_60HZ_PIXCLOCK  5787
#define RES_1400X1050_60HZ_PIXCLOCK  9260
#define RES_1366X768_60HZ_PIXCLOCK    11647

// LCD display method
#define     LCD_EXPANDSION              0x00
#define     LCD_CENTERING               0x01

// Define display timing

struct display_timing {
    uint16_t     hor_total;
    uint16_t     hor_addr;
    uint16_t     hor_blank_start;
    uint16_t     hor_blank_end;
    uint16_t     hor_sync_start;
    uint16_t     hor_sync_end;
    uint16_t     ver_total;
    uint16_t     ver_addr;
    uint16_t     ver_blank_start;
    uint16_t     ver_blank_end;
    uint16_t     ver_sync_start;
    uint16_t     ver_sync_end;
};

struct crt_mode_table {
  int                               refresh_rate;
  unsigned long                     clk;
  int                               h_sync_polarity;
  int                               v_sync_polarity;
  struct display_timing             crtc;
};

struct io_reg{
  int   port;
  uint8_t    index;
  uint8_t    mask;
  uint8_t    value;
};

#endif /* _DEV_PCI_UNICHROMEREG_H */

