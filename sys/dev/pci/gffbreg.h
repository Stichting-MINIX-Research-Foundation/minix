/*	$NetBSD: gffbreg.h,v 1.3 2013/10/23 13:15:47 macallan Exp $	*/

/*
 * Copyright (c) 2013 Michael Lorenz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * A console driver for nvidia geforce graphics controllers
 * tested on macppc only so far
 * register definitions are mostly from the xf86-video-nv driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gffbreg.h,v 1.3 2013/10/23 13:15:47 macallan Exp $");

#ifndef GFFBREG_H
#define GFFBREG_H

#define GFFB_RAMDAC0	0x00680000
#define GFFB_RAMDAC1	0x00682000

#define GFFB_PCIO0	0x00601000
#define GFFB_PCIO1	0x00603000

/* VGA registers live here, one set for each head */
#define GFFB_PDIO0	0x00681000
#define GFFB_PDIO1	0x00683000

#define GFFB_CRTC0	0x00600000
#define GFFB_CRTC1	0x00602000

#define GFFB_FIFO	0x00800000
#define GFFB_FIFO_PUT	0x00800040	/* command list stop */
#define GFFB_FIFO_GET	0x00800044	/* command list pointer */

#define GFFB_PGRAPH	0x00400000
#define GFFB_BUSY	0x00400700

#define GFFB_PFB	0x00100000
#define GFFB_VRAM	0x0010020c	/* vram size in 0xfff00000 */

#define GFFB_PRAMIN	0x00710000
#define GFFB_CMDSTART	0x00712098	/* ??? */

#define GFFB_PMC	0x00000000
#define GFFB_PFIFO	0x00002000
#define GFFB_PEXTDEV	0x00101000
#define GFFB_PTIMER	0x00009000

/* CRTC registers */
#define GFFB_DISPLAYSTART	0x800

/* VGA registers */
#define GFFB_PEL_MASK	0x3c6
#define GFFB_PEL_IR	0x3c7
#define GFFB_PEL_IW	0x3c8
#define GFFB_PEL_D	0x3c9

/* engine command definitions from xf86_video_nv/nv_dma.h */

/*
 * Copyright (c) 2003 NVIDIA, Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define SURFACE_FORMAT                                              0x00000300
#define SURFACE_FORMAT_DEPTH8                                       0x00000001
#define SURFACE_FORMAT_DEPTH15                                      0x00000002
#define SURFACE_FORMAT_DEPTH16                                      0x00000004
#define SURFACE_FORMAT_DEPTH24                                      0x00000006
#define SURFACE_PITCH                                               0x00000304
#define SURFACE_PITCH_SRC                                           15:0
#define SURFACE_PITCH_DST                                           31:16
#define SURFACE_OFFSET_SRC                                          0x00000308
#define SURFACE_OFFSET_DST                                          0x0000030C

#define ROP_SET                                                     0x00002300

#define PATTERN_FORMAT                                              0x00004300
#define PATTERN_FORMAT_DEPTH8                                       0x00000003
#define PATTERN_FORMAT_DEPTH16                                      0x00000001
#define PATTERN_FORMAT_DEPTH24                                      0x00000003
#define PATTERN_COLOR_0                                             0x00004310
#define PATTERN_COLOR_1                                             0x00004314
#define PATTERN_PATTERN_0                                           0x00004318
#define PATTERN_PATTERN_1                                           0x0000431C

#define CLIP_POINT                                                  0x00006300
#define CLIP_POINT_X                                                15:0
#define CLIP_POINT_Y                                                31:16
#define CLIP_SIZE                                                   0x00006304
#define CLIP_SIZE_WIDTH                                             15:0
#define CLIP_SIZE_HEIGHT                                            31:16

#define LINE_FORMAT                                                 0x00008300
#define LINE_FORMAT_DEPTH8                                          0x00000003
#define LINE_FORMAT_DEPTH16                                         0x00000001
#define LINE_FORMAT_DEPTH24                                         0x00000003
#define LINE_COLOR                                                  0x00008304
#define LINE_MAX_LINES                                              16
#define LINE_LINES(i)                                               0x00008400\
                                                                    +(i)*8
#define LINE_LINES_POINT0_X                                         15:0
#define LINE_LINES_POINT0_Y                                         31:16 
#define LINE_LINES_POINT1_X                                         47:32
#define LINE_LINES_POINT1_Y                                         63:48

#define BLIT_POINT_SRC                                              0x0000A300
#define BLIT_POINT_SRC_X                                            15:0
#define BLIT_POINT_SRC_Y                                            31:16
#define BLIT_POINT_DST                                              0x0000A304
#define BLIT_POINT_DST_X                                            15:0
#define BLIT_POINT_DST_Y                                            31:16
#define BLIT_SIZE                                                   0x0000A308
#define BLIT_SIZE_WIDTH                                             15:0
#define BLIT_SIZE_HEIGHT                                            31:16

#define RECT_FORMAT                                                 0x0000C300
#define RECT_FORMAT_DEPTH8                                          0x00000003
#define RECT_FORMAT_DEPTH16                                         0x00000001
#define RECT_FORMAT_DEPTH24                                         0x00000003
#define RECT_SOLID_COLOR                                            0x0000C3FC
#define RECT_SOLID_RECTS_MAX_RECTS                                  32
#define RECT_SOLID_RECTS(i)                                         0x0000C400\
                                                                    +(i)*8
#define RECT_SOLID_RECTS_Y                                          15:0
#define RECT_SOLID_RECTS_X                                          31:16
#define RECT_SOLID_RECTS_HEIGHT                                     47:32
#define RECT_SOLID_RECTS_WIDTH                                      63:48

#define RECT_EXPAND_ONE_COLOR_CLIP                                  0x0000C7EC
#define RECT_EXPAND_ONE_COLOR_CLIP_POINT0_X                         15:0
#define RECT_EXPAND_ONE_COLOR_CLIP_POINT0_Y                         31:16
#define RECT_EXPAND_ONE_COLOR_CLIP_POINT1_X                         47:32
#define RECT_EXPAND_ONE_COLOR_CLIP_POINT1_Y                         63:48
#define RECT_EXPAND_ONE_COLOR_COLOR                                 0x0000C7F4
#define RECT_EXPAND_ONE_COLOR_SIZE                                  0x0000C7F8
#define RECT_EXPAND_ONE_COLOR_SIZE_WIDTH                            15:0
#define RECT_EXPAND_ONE_COLOR_SIZE_HEIGHT                           31:16
#define RECT_EXPAND_ONE_COLOR_POINT                                 0x0000C7FC
#define RECT_EXPAND_ONE_COLOR_POINT_X                               15:0
#define RECT_EXPAND_ONE_COLOR_POINT_Y                               31:16
#define RECT_EXPAND_ONE_COLOR_DATA_MAX_DWORDS                       128
#define RECT_EXPAND_ONE_COLOR_DATA(i)                               0x0000C800\
                                                                    +(i)*4

#define RECT_EXPAND_TWO_COLOR_CLIP                                  0x0000CBE4
#define RECT_EXPAND_TWO_COLOR_CLIP_POINT0_X                         15:0
#define RECT_EXPAND_TWO_COLOR_CLIP_POINT0_Y                         31:16
#define RECT_EXPAND_TWO_COLOR_CLIP_POINT1_X                         47:32
#define RECT_EXPAND_TWO_COLOR_CLIP_POINT1_Y                         63:48
#define RECT_EXPAND_TWO_COLOR_COLOR_0                               0x0000CBEC
#define RECT_EXPAND_TWO_COLOR_COLOR_1                               0x0000CBF0
#define RECT_EXPAND_TWO_COLOR_SIZE_IN                               0x0000CBF4
#define RECT_EXPAND_TWO_COLOR_SIZE_IN_WIDTH                         15:0
#define RECT_EXPAND_TWO_COLOR_SIZE_IN_HEIGHT                        31:16
#define RECT_EXPAND_TWO_COLOR_SIZE_OUT                              0x0000CBF8
#define RECT_EXPAND_TWO_COLOR_SIZE_OUT_WIDTH                        15:0
#define RECT_EXPAND_TWO_COLOR_SIZE_OUT_HEIGHT                       31:16
#define RECT_EXPAND_TWO_COLOR_POINT                                 0x0000CBFC
#define RECT_EXPAND_TWO_COLOR_POINT_X                               15:0
#define RECT_EXPAND_TWO_COLOR_POINT_Y                               31:16
#define RECT_EXPAND_TWO_COLOR_DATA_MAX_DWORDS                       128
#define RECT_EXPAND_TWO_COLOR_DATA(i)                               0x0000CC00\
                                                                    +(i)*4

#define STRETCH_BLIT_FORMAT                                         0x0000E300
#define STRETCH_BLIT_FORMAT_DEPTH8                                  0x00000004
#define STRETCH_BLIT_FORMAT_DEPTH16                                 0x00000007
#define STRETCH_BLIT_FORMAT_DEPTH24                                 0x00000004
#define STRETCH_BLIT_FORMAT_A8R8G8B8                                0x00000003
#define STRETCH_BLIT_FORMAT_X8R8G8B8                                0x00000004
#define STRETCH_BLIT_FORMAT_YUYV                                    0x00000005
#define STRETCH_BLIT_FORMAT_UYVY                                    0x00000006
/* STRETCH_BLIT_OPERATION is only supported on TNT2 and newer */
#define STRETCH_BLIT_OPERATION                                      0x0000E304
#define STRETCH_BLIT_OPERATION_ROP                                  0x00000001
#define STRETCH_BLIT_OPERATION_COPY                                 0x00000003
#define STRETCH_BLIT_OPERATION_BLEND                                0x00000002
#define STRETCH_BLIT_CLIP_POINT                                     0x0000E308
#define STRETCH_BLIT_CLIP_POINT_X                                   15:0 
#define STRETCH_BLIT_CLIP_POINT_Y                                   31:16
#define STRETCH_BLIT_CLIP_POINT                                     0x0000E308
#define STRETCH_BLIT_CLIP_SIZE                                      0x0000E30C
#define STRETCH_BLIT_CLIP_SIZE_WIDTH                                15:0
#define STRETCH_BLIT_CLIP_SIZE_HEIGHT                               31:16
#define STRETCH_BLIT_DST_POINT                                      0x0000E310
#define STRETCH_BLIT_DST_POINT_X                                    15:0
#define STRETCH_BLIT_DST_POINT_Y                                    31:16
#define STRETCH_BLIT_DST_SIZE                                       0x0000E314
#define STRETCH_BLIT_DST_SIZE_WIDTH                                 15:0
#define STRETCH_BLIT_DST_SIZE_HEIGHT                                31:16
#define STRETCH_BLIT_DU_DX                                          0x0000E318
#define STRETCH_BLIT_DV_DY                                          0x0000E31C
#define STRETCH_BLIT_SRC_SIZE                                       0x0000E400
#define STRETCH_BLIT_SRC_SIZE_WIDTH                                 15:0
#define STRETCH_BLIT_SRC_SIZE_HEIGHT                                31:16
#define STRETCH_BLIT_SRC_FORMAT                                     0x0000E404
#define STRETCH_BLIT_SRC_FORMAT_PITCH                               15:0
#define STRETCH_BLIT_SRC_FORMAT_ORIGIN                              23:16
#define STRETCH_BLIT_SRC_FORMAT_ORIGIN_CENTER                       0x00000001
#define STRETCH_BLIT_SRC_FORMAT_ORIGIN_CORNER                       0x00000002
#define STRETCH_BLIT_SRC_FORMAT_FILTER                              31:24
#define STRETCH_BLIT_SRC_FORMAT_FILTER_POINT_SAMPLE                 0x00000000
#define STRETCH_BLIT_SRC_FORMAT_FILTER_BILINEAR                     0x00000001
#define STRETCH_BLIT_SRC_OFFSET                                     0x0000E408
#define STRETCH_BLIT_SRC_POINT                                      0x0000E40C
#define STRETCH_BLIT_SRC_POINT_U                                    15:0
#define STRETCH_BLIT_SRC_POINT_V                                    31:16


#endif /* GFFBREG_H */
