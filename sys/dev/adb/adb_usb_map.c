/*	$NetBSD: adb_usb_map.c,v 1.2 2014/11/08 16:52:35 macallan Exp $ */

/*-
 * Copyright (c) 2006 Michael Lorenz
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: adb_usb_map.c,v 1.2 2014/11/08 16:52:35 macallan Exp $");

#include <sys/param.h>
#include <sys/device.h>

#include <dev/wscons/wsksymvar.h>

keysym_t adb_to_usb[] = {
/*   0, KS_a 		*/		4,
/*   1, KS_s 		*/		22,
/*   2, KS_d 		*/		7,
/*   3, KS_f 		*/		9,
/*   4, KS_h 		*/		11,
/*   5, KS_g 		*/		10,
/*   6, KS_z 		*/		29,
/*   7, KS_x 		*/		27,
/*   8, KS_c 		*/		6,
/*   9, KS_v		*/		25,
/*  10, KS_paragraph	*/		53,
/*  11, KS_b		*/		5,
/*  12, KS_q		*/		20,
/*  13, KS_w		*/		26,
/*  14, KS_e		*/		8,
/*  15, KS_r		*/		21,
/*  16, KS_y		*/		28,
/*  17, KS_t		*/		23,
/*  18, KS_1		*/		30,
/*  19, KS_2		*/		31,
/*  20, KS_3		*/		32,
/*  21, KS_4		*/		33,
/*  22, KS_6		*/		35,
/*  23, KS_5		*/		34,
/*  24, KS_equal	*/		46,
/*  25, KS_9		*/		38,
/*  26, KS_7		*/		36,
/*  27, KS_minus	*/		45,
/*  28, KS_8		*/		37,
/*  29, KS_0		*/		39,
/*  30, KS_bracketright	*/		48,
/*  31, KS_o		*/		18,
/*  32, KS_u		*/		24,
/*  33, KS_bracketleft	*/		47,
/*  34, KS_i		*/		12,
/*  35, KS_p		*/		19,
/*  36, KS_Return	*/		40,
/*  37, KS_l		*/		15,
/*  38, KS_j		*/		13,
/*  39, KS_apostrophe	*/		52,
/*  40, KS_k		*/		14,
/*  41, KS_semicolon	*/		51,
/*  42, KS_backslash	*/		50,
/*  43, KS_comma	*/		54,
/*  44, KS_slash	*/		56,
/*  45, KS_n		*/		17,
/*  46, KS_m		*/		16,
/*  47, KS_period	*/		55,
/*  48, KS_Tab		*/		43,
/*  49, KS_space	*/		44,
/*  50, KS_grave	*/		53,
/*  51, KS_Delete	*/		42,
/*  52, KS_KP_Enter	*/		88,
/*  53, KS_Escape	*/		41,
/*  54, KS_Control_L	*/		224,
/*  55, KS_Cmd		*/		227,	/* left meta */
/*  56, KS_Shift_L	*/		225,
/*  57, KS_Caps_Lock	*/		57,
/*  58, KS_Option	*/		226,
/*  59, KS_Left		*/		80,
/*  60, KS_Right	*/		79,
/*  61, KS_Down		*/		81,
/*  62, KS_Up		*/		82,
/*  63			*/		0,
/*  64			*/		0,
/*  65, KS_KP_Decimal	*/		99,
/*  66			*/		0,
/*  67, KS_KP_Multiply	*/		85,
/*  68			*/		0,
/*  69, KS_KP_Add	*/		87,
/*  70			*/		0,
/*  71, KS_Num_Lock	*/		83,
/*  72			*/		0,
/*  73			*/		0,
/*  74			*/		0,
/*  75, KS_KP_Divide	*/		84,
/*  76, KS_KP_Enter	*/		88,
/*  77			*/		0,
/*  78, KS_KP_Subtract	*/		86,
/*  79			*/		0,
/*  80			*/		0,
/*  81, KS_KP_Equal	*/		46,	/* no KP_EQUAL on USB? */
/*  82, KS_KP_Insert, 0	*/		98,
/*  83, KS_KP_End,    1	*/		89,
/*  84, KS_KP_Down,   2	*/		90,
/*  85, KS_KP_Next,   3	*/		91,
/*  86, KS_KP_Left,   4	*/		92,
/*  87, KS_KP_Begin   5	*/		93,
/*  88, KS_KP_Right   6	*/		94,
/*  89, KS_KP_Home    7	*/		95,
/*  90			*/		0,
/*  91, KS_KP_Up      8	*/		96,
/*  92, KS_KP_Prior   9	*/		97,
/*  93, KS_backslash	*/		100,
/*  94, KS_underscore	*/		45,
/*  95, KS_KP_Delete  . */		99,
/*  96, KS_f5		*/		62,
/*  97, KS_f6		*/		63,
/*  98, KS_f7		*/		64,
/*  99, KS_f3		*/		60,
/* 100, KS_f8		*/		65,
/* 101, KS_f9		*/		66,
/* 102			*/		0,
/* 103, KS_f11		*/		68,
/* 104			*/		0,
/* 105, KS_Print_Screen	*/		70,
/* 106, KS_KP_Enter	*/		88,
/* 107, KS_Hold_Screen	*/		71,
/* 108			*/		0,
/* 109, KS_f10		*/		67,
/* 110			*/		0,
/* 111, KS_f12		*/		69,
/* 112			*/		0,
/* 113, KS_Pause	*/		72,
/* 114, KS_Insert	*/		73,
/* 115, KS_Home		*/		74,
/* 116, KS_Prior	*/		75,
/* 117, KS_BackSpace	*/		76,
/* 118, KS_f4		*/		61,
/* 119, KS_End		*/		77,
/* 120, KS_f2		*/		59,
/* 121, KS_Next		*/		78,
/* 122, KS_f1		*/		58,
/* 123, KS_Shift_R	*/		229,
/* 124, KS_Alt_R	*/		230,
/* 125, KS_Control_R	*/		228,
/* 126			*/		0,
/* 127, KS_Cmd_Debugger	*/		102
};
