/*	$NetBSD: hpckbdkeymap.h,v 1.51 2011/08/06 03:53:40 kiyohara Exp $	*/

/*-
 * Copyright (c) 1999-2002 The NetBSD Foundation, Inc.
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

#define UNK		255	/* unknown */
#define IGN		254	/* ignore */
#define SPL		253	/* special key */
#define KC(n)		KS_KEYCODE(n)
#define CMDMAP(map)	{ map, (sizeof(map)/sizeof(keysym_t)) }
#define NULLCMDMAP	{ NULL, 0 }

#define KEY_SPECIAL_OFF		0
#define KEY_SPECIAL_LIGHT	1

const uint8_t default_keymap[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 1 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 2 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 3 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 4 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 5 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 6 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 7 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 8 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/* 9 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*10 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*11 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*12 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*13 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*14 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*15 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK
};

const int default_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= -1,
	[KEY_SPECIAL_LIGHT]	= -1
};

#ifdef hpcmips
const uint8_t tc5165_mobilon_keymap[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */	37 , 45 , 44 , UNK, 9  , 51 , 23 , UNK,
/* 1 */	UNK, 56 , UNK, UNK, UNK, UNK, UNK, UNK,
/* 2 */	UNK, UNK, 29 , UNK, UNK, UNK, UNK, UNK,
/* 3 */	24 , 203, UNK, 38 , 10 , 27 , 13 , UNK,
/* 4 */	40 , UNK, UNK, 39 , 26 , 53 , 11 , 12 ,
/* 5 */	UNK, UNK, UNK, 53 , 25 , UNK, UNK, SPL, /* Light */
/* 6 */	208, UNK, UNK, UNK, 52 , UNK, 43 , 14 ,
/* 7 */	205, 200, UNK, UNK, SPL, UNK, UNK, 28 , /* Off key */
/* 8 */	UNK, 41 , 59 , 15 , 2  , UNK, UNK, UNK,
/* 9 */	63 , 64 , 1  , UNK, 65 , 16 , 17 , UNK,
/*10 */	60 , UNK, 61 , 62 , 3  , UNK, UNK, UNK,
/*11 */	UNK, UNK, UNK, 42 , 58 , UNK, UNK, UNK,
/*12 */	47 , 33 , 46 , 5  , 4  , 18 , 19 , UNK,
/*13 */	34 , 35 , 20 , 48 , 6  , 7  , 21 , 49 ,
/*14 */	22 , 31 , 32 , 36 , 8  , 30 , 50 , 57 ,
/*15 */	UNK, IGN, UNK, UNK, UNK, UNK, UNK, UNK /* Windows key */
};

const int tc5165_mobilon_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= -1,	/* 60 */
	[KEY_SPECIAL_LIGHT]	= 47
};

const uint8_t tc5165_telios_jp_keymap[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */	58,  15,  IGN, 1,   IGN, IGN, IGN, IGN,
/* 1 */	IGN, IGN, IGN, IGN, 54,  42,  IGN, IGN,
/* 2 */	31,  18,  4,   IGN, IGN, 32,  45,  59,
/* 3 */	33,  19,  5,   61,  IGN, 46,  123, 60,
/* 4 */	35,  21,  8,   64,  IGN, 48,  49,  63,
/* 5 */	17,  16,  3,   IGN, 2,   30,  44,  41,
/* 6 */	IGN, IGN, IGN, IGN, IGN, IGN, 221, IGN,
/* 7 */	IGN, IGN, IGN, IGN, IGN, IGN, 56,  IGN,
/* 8 */	34,  20,  7,   IGN, 6,   47,  57,  62,
/* 9 */	IGN, IGN, IGN, IGN, IGN, IGN, 29,  IGN,
/*10 */	27,  125, 13,  203, 208, 40,  115, 68,
/*11 */	39,  26,  25,  112, 12,  52,  53,  67,
/*12 */	37,  24,  11,  121, 10,  38,  51,  66,
/*13 */	23,  22,  9,   IGN, IGN, 36,  50,  65,
/*14 */	28,  43,  14,  200, 205, IGN, IGN, 211,
/*15 */	IGN, IGN, IGN, IGN, IGN, IGN, 184, IGN
};

static const keysym_t tc5165_telios_jp_cmdmap[] = {
/*	pos      command		normal		shifted		*/
	KC(184), KS_Cmd,		KS_Alt_R,	KS_Multi_key,
	KC(205), KS_Cmd_BrightnessUp,	KS_Right,
	KC(203), KS_Cmd_BrightnessDown,	KS_Left,
	KC(57),  KS_Cmd_BacklightToggle,KS_space,
};

const uint8_t tc5165_compaq_c_jp_keymap[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */	38,  50,  49,  48,  47,  46,  45,  44,
/* 1 */	56,  IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/* 2 */	13,  IGN, 112, 121, 123, 41,  28,  57,
/* 3 */	205, 203, 208, 200, 39,  53,  52,  51,
/* 4 */	24,  25,  40,  IGN, 43,  26,  115, 58,
/* 5 */	54,  IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/* 6 */	IGN, IGN, IGN, SPL, IGN, IGN, IGN, IGN, /* Light */
/* 7 */	IGN, IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/* 8 */	42,  IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/* 9 */	29,  IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*10 */	221, IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*11 */	221, IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*12 */	14,  27,  12,  11,  10,  15,  1,   125,
/*13 */	9,   8,   7,   6,   5,   4,   3,   2,
/*14 */	23,  22,  21,  20,  19,  18,  17,  16,
/*15 */	37,  36,  35,  34,  33,  32,  31,  30
};

const int tc5165_compaq_c_jp_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= -1, /* don't have off button */
	[KEY_SPECIAL_LIGHT]	= 51
};

const uint8_t m38813c_keymap[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */	0,   1,   2,   3,   4,   5,   6,   7,
/* 1 */	8,   9,   10,  11,  12,  13,  14,  15,
/* 2 */	16,  17,  18,  19,  20,  21,  22,  23,
/* 3 */	24,  25,  26,  27,  28,  29,  30,  31,
/* 4 */	32,  33,  34,  35,  36,  37,  38,  39,
/* 5 */	40,  41,  42,  43,  44,  45,  46,  47,
/* 6 */	48,  49,  50,  51,  52,  53,  54,  55,
/* 7 */	56,  57,  58,  59,  60,  61,  62,  63,
/* 8 */	64,  65,  66,  67,  68,  69,  70,  71,
/* 9 */	200, 73,  74,  203, 76,  205,  78,  79,
/*10 */	208, 81,  82,  83,  84,  85,  86,  87,
/*11 */	88,  89,  90,  91,  92,  93,  94,  95,
/*12 */	96,  97,  98,  99,  100, 101, 102, 103,
/*13 */	104, 105, 106, 107, 108, 109, 110, 111,
/*14 */	112, 113, 114, 115, 116, 117, 118, 119,
/*15 */	120, 121, 122, 123, 124, 125, 126, 127
};

/* NEC MobileGearII MCR series (Japan) */
static uint8_t mcr_jp_keytrans[] = {
/*00	right	ent	p	.	y	b	z	space	*/
/*08	down	/	o	,	t	v	a	nfer	*/
/*10	left	\	i	m	r	c	w	menu	*/
/*18	^	-	u	-	e	x	q	1	*/
/*20	pgdn	h/z	0	l	:	g	tab	f1	*/
/*28	xfer	;	9	n	5	f	2	k	*/
/*30	up	[	8	j	4	d	6	-	*/
/*38	-	@	7	h	3	]	s	-	*/
/*40	caps	-	-	-	bs	fnc	f8	f3	*/
/*48	-	alt	-	-	|	k/h	f7	f4	*/
/*50	-	-	ctrl	-	f10	pgup	f6	f2	*/
/*58	-	-	-	shift	del	f9	f5	esc	*/
/*----------------------------------------------------------------------*/
/*00*/	205,	 28,	 25,	 52,	 21,	 48,	 44,	 57,
/*08*/	208,	 53,	 24,	 51,	 20,	 47,	 30,	123,
/*10*/	203,	115,	 23,	 50,	 19,	 46,	 17,	221,
/*18*/	 13,	IGN,	 22,	IGN,	 18,	 45,	 16,	  2,
/*20*/	 81,	 41,	 11,	 38,	 40,	 34,	 15,	 59,
/*28*/	121,	 39,	 10,	 49,	  6,	 33,	  3,	 37,
/*30*/	200,	 27,	  9,	 36,	  5,	 32,	  7,	IGN,
/*38*/	 12,	 26,	  8,	 35,	  4,	 43,	 31,	IGN,
/*40*/	 58,	IGN,	IGN,	IGN,	 14,	184,	 66,	 61,
/*48*/	IGN,	 56,	IGN,	IGN,	125,	112,	 65,	 62,
/*50*/	IGN,	IGN,	 29,	IGN,	 68,	 73,	 64,	 60,
/*58*/	IGN,	IGN,	IGN,	 42,	 14,	 67,	 63,	  1,
};

static const keysym_t mcr_jp_cmdmap[] = {
/*	pos      command		normal		shifted		*/
	KC(184), KS_Cmd,		KS_Alt_R,	KS_Multi_key,
	KC(73),  KS_Cmd_BrightnessUp,	KS_KP_Prior,	KS_KP_9,
	KC(81),  KS_Cmd_BrightnessDown,	KS_KP_Next,	KS_KP_3,
	KC(51),  KS_Cmd_ContrastDown,	KS_comma,	KS_less,
	KC(52),  KS_Cmd_ContrastUp,	KS_period,	KS_greater,
	KC(57),  KS_Cmd_BacklightToggle,KS_space,
};

/* IBM WorkPad z50 */
static uint8_t z50_keytrans[] = {
/*00	f1	f3	f5	f7	f9	-	-	f11	*/
/*08	f2	f4	f6	f8	f10	-	-	f12	*/
/*10	'	[	-	0	p	;	up	/	*/
/*18	-	-	-	9	o	l	.	-	*/
/*20	left	]	=	8	i	k	,	-	*/
/*28	h	y	6	7	u	j	m	n	*/
/*30	-	bs	num	del	-	\	ent	sp	*/
/*38	g	t	5	4	r	f	v	b	*/
/*40	-	-	-	3	e	d	c	right	*/
/*48	-	-	-	2	w	s	x	down	*/
/*50	esc	tab	~	1	q	a	z	-	*/
/*58	menu	Ls	Lc	Rc	La	Ra	Rs	-	*/
/*----------------------------------------------------------------------*/
/*00*/	 59,	 61,	 63,	 65,	 67,	IGN,	IGN,	 87,
/*08*/	 60,	 62,	 64,	 66,	 68,	IGN,	IGN,	 88,
/*10*/	 40,	 26,	 12,	 11,	 25,	 39,	200,	 53,
/*18*/	IGN,	IGN,	IGN,	 10,	 24,	 38,	 52,	IGN,
/*20*/	203,	 27,	 13,	  9,	 23,	 37,	 51,	IGN,
/*28*/	 35,	 21,	  7,	  8,	 22,	 36,	 50,	 49,
/*30*/	IGN,	 14,	 69,	 14,	IGN,	 43,	 28,	 57,
/*38*/	 34,	 20,	  6,	  5,	 19,	 33,	 47,	 48,
/*40*/	IGN,	IGN,	IGN,	  4,	 18,	 32,	 46,	205,
/*48*/	IGN,	IGN,	IGN,	  3,	 17,	 31,	 45,	208,
/*50*/	  1,	 15,	 41,	  2,	 16,	 30,	 44,	IGN,
/*58*/	221,	 42,	 29,	 29,	 56,	 56,	 54,	IGN,
};

/* Sharp Tripad PV6000 and VADEM CLIO */
static uint8_t tripad_keytrans[] = {
/*00	lsh	tab	`	q	esc	1	WIN	-	*/
/*08	ctrl	z	x	a	s	w	e	2	*/
/*10	lalt	sp	c	v	d	f	r	3	*/
/*18	b	n	g	h	t	y	4	5	*/
/*20	m	,	j	k	u	i	6	7	*/
/*28	Fn	caps	l	o	p	8	9	0	*/
/*30	[	]	la	.	/	;	-	=	*/
/*38	rsh	ra	ua	da	'	ent	\	del	*/
/*40	-	-	-	-	-	-	-	-	*/
/*48	-	-	-	-	-	-	-	-	*/
/*50	-	-	-	-	-	-	-	-	*/
/*58	-	-	-	-	-	-	-	-	*/
/*----------------------------------------------------------------------*/
/*00*/	 42,	 15,	 41,	 16,	  1,	  2,	104,	221,
/*08*/	 29,	 44,	 45,	 30,	 31,	 17,	 18,	  3,
/*10*/	 56,	 57,	 46,	 47,	 32,	 33,	 19,	  4,
/*18*/	 48,	 49,	 34,	 35,	 20,	 21,	  5,	  6,
/*20*/	 50,	 51,	 36,	 37,	 22,	 23,	  7,	  8,
/*28*/	184,	 58,	 38,	 24,	 25,	  9,	 10,	 11,
/*30*/	 26,	 27,	203,	 52,	 53,	 39,	 12,	 13,
/*38*/	 54,	205,	200,	208,	 40,	 28,	 43,	 14,
/*40*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*48*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*50*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*58*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
};

static const keysym_t tripad_cmdmap[] = {
/*  pos      command		normal		shifted		fn    */
KC(2),	 KS_Cmd_Screen0,	KS_1,		KS_exclam,	KS_f1,
KC(3),	 KS_Cmd_Screen1,	KS_2,		KS_at,		KS_f2,
KC(4),	 KS_Cmd_Screen2,	KS_3,		KS_numbersign,	KS_f3,
KC(5),	 KS_Cmd_Screen3,	KS_4,		KS_dollar,	KS_f4,
KC(6),	 KS_Cmd_Screen4,	KS_5,		KS_percent,	KS_f5,
KC(7),	 KS_Cmd_Screen5,	KS_6,		KS_asciicircum,	KS_f6,
KC(8),	 KS_Cmd_Screen6,	KS_7,		KS_ampersand,	KS_f7,
KC(9),	 KS_Cmd_Screen7,	KS_8,		KS_asterisk,	KS_f8,
KC(10),  KS_Cmd_Screen8,	KS_9,		KS_parenleft,	KS_f9,
KC(11),  KS_Cmd_Screen9,	KS_0,		KS_parenright,	KS_f10,
KC(12),  KS_Cmd_BrightnessDown,	KS_minus,	KS_underscore,	KS_f11,
KC(13),  KS_Cmd_BrightnessUp,	KS_equal,	KS_plus,	KS_f12,
KC(20),  KS_Cmd_BacklightToggle, KS_t,
KC(33),  KS_Cmd_BacklightOff,	KS_f,
KC(49),  KS_Cmd_BacklightOn,	KS_n,
KC(51),  KS_Cmd_ContrastDown,	KS_comma,	KS_less,
KC(52),  KS_Cmd_ContrastUp,	KS_period,	KS_greater,
KC(184), KS_Mode_switch,	KS_Multi_key,
KC(200), KS_Cmd_ScrollSlowUp,	KS_Up,		KS_Up,		KS_Prior,
KC(203), KS_Cmd_ScrollFastUp,	KS_Left,	KS_Left,	KS_Home,
KC(205), KS_Cmd_ScrollFastDown,	KS_Right,	KS_Right,	KS_End,
KC(208), KS_Cmd_ScrollSlowDown,	KS_Down,	KS_Down,	KS_Next,
};

/* NEC Mobile Gear MCCS series */
static uint8_t mccs_keytrans[] = {
/*00	caps	cr	rar	p	.	y	b	z	*/
/*08	alt	[	dar	o	,	t	v	a	*/
/*10	zen	@	lar	i	m	r	c	w	*/
/*18	lctrl	;	uar	u	n	e	x	q	*/
/*20	lshft	bs	\	0	l	6	g	tab	*/
/*28	nconv	|	/	9	k	5	f	2	*/
/*30	conv	=	]	8	j	4	d	1	*/
/*38	hira	-	'	7	h	3	s	esc	*/
/*40	-	sp	-	-	-	-	-	-	*/
/*48	-	-	-	-	-	-	-	-	*/
/*50	-	-	-	-	-	-	-	-	*/
/*58	-	-	-	-	-	-	-	-	*/
/*----------------------------------------------------------------------*/
/*00*/	 58,	 28,	205,	 25,	 52,	 21,	 48,	 44,
/*08*/	 56,	 27,	208,	 24,	 51,	 20,	 47,	 30,
/*10*/	 41,	 26,	203,	 23,	 50,	 19,	 46,	 17,
/*18*/	 29,	 39,	200,	 22,	 49,	 18,	 45,	 16,
/*20*/	 42,	 14,	115,	 11,	 38,	  7,	 34,	 15,
/*28*/	123,	125,	 53,	 10,	 37,	  6,	 33,	  3,
/*30*/	121,	 13,	 43,	  9,	 36,	  5,	 32,	  2,
/*38*/	112,	 12,	 40,	  8,	 35,	  4,	 31,	  1,
/*40*/	IGN,	 57,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*48*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*50*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
/*58*/	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,	IGN,
};

static const keysym_t mccs_cmdmap[] = {
/*	pos      command		normal		shifted		*/
	KC(51),  KS_Cmd_ContrastDown,	KS_comma,	KS_less,
	KC(52),  KS_Cmd_ContrastUp,	KS_period,	KS_greater,
	KC(57),  KS_Cmd_BacklightToggle,KS_space,
};

static uint8_t mobilepro_keytrans[] = {
/*00	space	]	\	/	left	down	enter	l	*/
/*08	-	[	'	;	right	up	.	o	*/
/*10	-	-	-	Windows	v	c	x	z	*/
/*18	-	=	\-	`	f	d	s	a	*/
/*20	8	7	6	5	r	e	w	q	*/
/*28	,	m	n	b	-	-	0	9	*/
/*30	k	j	h	g	4	3	2	1	*/
/*38	i	u	y	t	-	caps	del	esc	*/
/*40	alt_R	-	-	-	BS	p	TAB	Fn	*/
/*48	-	alt_L	-	-	pgdn	pgup	f10	f9	*/
/*50	-	-	ctrl	-	f8	f7	f6	f5	*/
/*58	-	-	-	shift	f4	f3	f2	f1	*/
/*----------------------------------------------------------------------*/
/*00*/	 57,	 27,	 43,	 53,	203,	208,	 28,	 38,
/*08*/	IGN,	 26,	 40,	 39,	205,	200,	 52,	 24,
/*10*/	IGN,	IGN,	IGN,	221,	 47,	 46,	 45,	 44,
/*18*/	IGN,	 13,	 12,	 41,	 33,	 32,	 31,	 30,
/*20*/	  9,	  8,	  7,	  6,	 19,	 18,	 17,	 16,
/*28*/	 51,	 50,	 49,	 48,	IGN,	IGN,	 11,	 10,
/*30*/	 37,	 36,	 35,	 34,	  5,	  4,	  3,	  2,
/*38*/	 23,	 22,	 21,	 20,	IGN,	 58,	 14,	  1,
/*40*/	184,	IGN,	IGN,	IGN,	 14,	 25,	 15,	219,
/*48*/	IGN,	 56,	IGN,	IGN,	 81,	 73,	 68,	 67,
/*50*/	IGN,	IGN,	 29,	IGN,	 66,	 65,	 64,	 63,
/*58*/	IGN,	IGN,	IGN,	 42,	 62,	 61,	 60,	 59,
};

static const keysym_t mobilepro_cmdmap[] = {
/*	pos      command		normal		shifted		*/
	KC(219), KS_Cmd,		KS_Meta_L,	KS_Multi_key,
	KC(73),  KS_Cmd_BrightnessUp,	KS_KP_Prior,	KS_KP_9,
	KC(81),  KS_Cmd_BrightnessDown,	KS_KP_Next,	KS_KP_3,
	KC(51),  KS_Cmd_ContrastDown,	KS_comma,	KS_less,
	KC(52),  KS_Cmd_ContrastUp,	KS_period,	KS_greater,
	KC(57),  KS_Cmd_BacklightToggle,KS_space,
};

/* NEC MobilePro 750c by "Castor Fu" <castor@geocast.com> */
static uint8_t mobilepro750c_keytrans[] = {
/*00	right	\	p	.	y	b	z	space	*/
/*08	down	/	o	,	t	v	a	-	*/
/*10	left	enter	i	m	r	c	w	Win	*/
/*18	num	]	u	n	e	x	q	caps	*/
/*20	pgdn	-	0	l	:	g	tab	esc	*/
/*28	-	;	9	k	5	f	2	`	*/
/*30	up	[	8	j	4	d	1	'	*/
/*38	-	@	7	h	3	s	del	-	*/
/*40	shift	-	-	-	bs	f12	f8	f4	*/
/*48	-	alt	-	-	|	f11	f7	f3	*/
/*50	-	-	ctrl	-	f10	f10	f6	f2	*/
/*58	-	-	-	shift	del	f9	f5	f1	*/
/*----------------------------------------------------------------------*/
/*00*/	205,	43,	25,	52,	21,	48,	44,	57,
/*08*/	208,	53,	24,	51,	20,	47,	30,	IGN,
/*10*/	203,	28,	23,	50,	19,	46,	17,	221,
/*18*/	69,	27,	22,	49,	18,	45,	16,	58,
/*20*/	81,	IGN,	11,	38,	7,	34,	15,	1,
/*28*/	IGN,	39,	10,	37,	6,	33,	3,	41,
/*30*/	200,	26,	9,	36,	5,	32,	2,	40,
/*38*/	12,	26,	8,	35,	4,	31,	83,	IGN,
/*40*/	42,	IGN,	IGN,	IGN,	14,	88,	66,	62,
/*48*/	IGN,	56,	IGN,	IGN,	125,	87,	65,	61,
/*50*/	IGN,	IGN,	29,	IGN,	68,	68,	64,	60,
/*58*/	IGN,	IGN,	IGN,	42,	13,	67,	63,	59,
};

/* NEC MobilePro 780 */
static uint8_t mobilepro780_keytrans[] = {
/*00	space	]	\	/	left	right	enter	l	*/
/*08	-	[	'	;	up	down	.	o	*/
/*10	-	-	-	Windows	v	c	x	z	*/
/*18	-	=	\-	`	f	d	s	a	*/
/*20	8	7	6	5	r	e	w	q	*/
/*28	,	m	n	b	-	-	0	9	*/
/*30	k	j	h	g	4	3	2	1	*/
/*38	i	u	y	t	-	caps	del	esc	*/
/*40	alt_R	-	-	-	BS	p	TAB	Fn	*/
/*48	-	alt_L	-	-	f12	f11	f10	f9	*/
/*50	-	-	ctrl	-	f8	f7	f6	f5	*/
/*58	-	-	-	shift	f4	f3	f2	f1	*/
/*----------------------------------------------------------------------*/
/*00*/	 57,	 27,	 43,	 53,	203,	205,	 28,	 38,
/*08*/	IGN,	 26,	 40,	 39,	200,	208,	 52,	 24,
/*10*/	IGN,	IGN,	IGN,	221,	 47,	 46,	 45,	 44,
/*18*/	IGN,	 13,	 12,	 41,	 33,	 32,	 31,	 30,
/*20*/	  9,	  8,	  7,	  6,	 19,	 18,	 17,	 16,
/*28*/	 51,	 50,	 49,	 48,	IGN,	IGN,	 11,	 10,
/*30*/	 37,	 36,	 35,	 34,	  5,	  4,	  3,	  2,
/*38*/	 23,	 22,	 21,	 20,	IGN,	 58,	 14,	  1,
/*40*/	184,	IGN,	IGN,	IGN,	 14,	 25,	 15,	IGN,
/*48*/	IGN,	 56,	IGN,	IGN,	 88,	 87,	 68,	 67,
/*50*/	IGN,	IGN,	 29,	IGN,	 66,	 65,	 64,	 63,
/*58*/	IGN,	IGN,	IGN,	 42,	 62,	 61,	 60,	 59,
};

/* NEC MobilePro 8x0 */
static uint8_t mobilepro8x0_keytrans[] = {
/*00	space	]	\	/	left	right	enter	l	*/
/*08	-	[	'	;	up	down	.	o	*/
/*10	-	-	-	Windows	v	c	x	z	*/
/*18	-	=	\-	`	f	d	s	a	*/
/*20	8	7	6	5	r	e	w	q	*/
/*28	,	m	n	b	-	-	0	9	*/
/*30	k	j	h	g	4	3	2	1	*/
/*38	i	u	y	t	-	caps	del	esc	*/
/*40	alt_R	-	-	-	BS	p	TAB	Fn	*/
/*48	-	alt_L	-	-	pgdn	pgup	f10	f9	*/
/*50	-	-	ctrl	-	f8	f7	f6	f5	*/
/*58	-	-	-	shift	f4	f3	f2	f1	*/
/*----------------------------------------------------------------------*/
/*00*/	 57,	 27,	 43,	 53,	203,	205,	 28,	 38,
/*08*/	IGN,	 26,	 40,	 39,	200,	208,	 52,	 24,
/*10*/	IGN,	IGN,	IGN,	221,	 47,	 46,	 45,	 44,
/*18*/	IGN,	 13,	 12,	 41,	 33,	 32,	 31,	 30,
/*20*/	  9,	  8,	  7,	  6,	 19,	 18,	 17,	 16,
/*28*/	 51,	 50,	 49,	 48,	IGN,	IGN,	 11,	 10,
/*30*/	 37,	 36,	 35,	 34,	  5,	  4,	  3,	  2,
/*38*/	 23,	 22,	 21,	 20,	IGN,	 58,	 14,	  1,
/*40*/	184,	IGN,	IGN,	IGN,	 14,	 25,	 15,	219,
/*48*/	IGN,	 56,	IGN,	IGN,	 81,	 73,	 68,	 67,
/*50*/	IGN,	IGN,	 29,	IGN,	 66,	 65,	 64,	 63,
/*58*/	IGN,	IGN,	IGN,	 42,	 62,	 61,	 60,	 59,
};

static const keysym_t mobilepro8x0_cmdmap[] = {
/*	pos      command		normal		shifted		*/
	KC(219), KS_Cmd,		KS_Meta_L,	KS_Multi_key,
	KC(73),  KS_Cmd_BrightnessUp,	KS_KP_Prior,	KS_KP_9,
	KC(81),  KS_Cmd_BrightnessDown,	KS_KP_Next,	KS_KP_3,
	KC(51),  KS_Cmd_ContrastDown,	KS_comma,	KS_less,
	KC(52),  KS_Cmd_ContrastUp,	KS_period,	KS_greater,
	KC(57),  KS_Cmd_BacklightToggle,KS_space,
};

/* FUJITSU INTERTOP CX300 */
static uint8_t intertop_keytrans[] = {
/*00	space   a2      1       tab     enter   caps    left    zenkaku	*/
/*08	hiraga  a1      2       q       -       a       fnc     esc	*/
/*10	ins     w       3       s       del     ]       down    x	*/
/*18	z       e       4       d       a10     \       right   c	*/
/*20	backsla r       ;       f       a9      @       ^       v	*/
/*28	/       t       5       g       a8      p       -       b	*/
/*30	.       y       6       h       a7      l       0       n	*/
/*38	-       u       7       j       a5      o       bs      m	*/
/*40	-       a3      8       a4      -       i       k       ,	*/
/*48	num     :       9       [       a6      -       up      -	*/
/*50	-       -       -       -       shift_L -       -       shift_R	*/
/*58	ctrl    win     muhenka henkan  alt     -       -       -	*/
/*----------------------------------------------------------------------*/
/*00*/	57,	60,	2,	15,	28,	58,	205,	41,
/*08*/	112,	59,	3,	16,	IGN,	30,	56,	1,
/*10*/	210,	17,	4,	31,	83,	43,	208,	45,
/*18*/	44,	18,	5,	32,	68,	125,	203,	46,
/*20*/	115,	19,	39,	33,	67,	26,	13,	47,
/*28*/	53,	20,	6,	34,	66,	25,	12,	48,
/*30*/	52,	21,	7,	35,	65,	38,	11,	49,
/*38*/	IGN,	22,	8,	36,	63,	24,	14,	50,
/*40*/	IGN,	61,	9,	62,	IGN,	23,	37,	51,
/*48*/	69,	40,	10,	27,	64,	IGN,	200,	IGN,
/*50*/	IGN,	IGN,	IGN,	IGN,	42,	IGN,	IGN,	54,
/*58*/	29,	221,	123,	121,	184,	IGN,	IGN,	IGN,
};

/* DoCoMo sigmarion (Japan) */
static uint8_t sigmarion_jp_keytrans[] = {
/*00	right	ent	p	.	y	b	z	space	*/
/*08	down	/	o	,	t	v	a	nfer	*/
/*10	left	\	i	m	r	c	w	menu	*/
/*18	|	-	u	-	e	x	q	1	*/
/*20	pgdn	h/z	0	l	:	g	tab	f1	*/
/*28	xfer	;	9	n	5	f	2	k	*/
/*30	up	[	8	j	4	d	6	-	*/
/*38	-	@	7	h	3	]	s	-	*/
/*40	caps	-	-	-	bs	fnc	f8	f3	*/
/*48	-	alt	-	-	^	k/h	f7	f4	*/
/*50	-	-	ctrl	-	f10	pgup	f6	f2	*/
/*58	-	-	-	shift	del	f9	f5	esc	*/
/*----------------------------------------------------------------------*/
/*00*/	205,	 28,	 25,	 52,	 21,	 48,	 44,	 57,
/*08*/	208,	 53,	 24,	 51,	 20,	 47,	 30,	123,
/*10*/	203,	115,	 23,	 50,	 19,	 46,	 17,	221,
/*18*/	125,	IGN,	 22,	IGN,	 18,	 45,	 16,	  2,
/*20*/	 81,	 41,	 11,	 38,	 40,	 34,	 15,	IGN,
/*28*/	121,	 39,	 10,	 49,	  6,	 33,	  3,	 37,
/*30*/	200,	 27,	  9,	 36,	  5,	 32,	  7,	IGN,
/*38*/	 12,	 26,	  8,	 35,	  4,	 43,	 31,	IGN,
/*40*/	 58,	IGN,	IGN,	IGN,	 14,	184,	 66,	IGN,
/*48*/	IGN,	 56,	IGN,	IGN,	 13,	112,	 65,	IGN,
/*50*/	IGN,	IGN,	 29,	IGN,	 68,	 73,	 64,	IGN,
/*58*/	IGN,	IGN,	IGN,	 42,	 14,	 67,	IGN,	  1,
};

static const keysym_t sigmarion_cmdmap[] = {
/*	pos      command		normal		shifted		*/
	KC(184), KS_Cmd,		KS_Alt_R,	KS_Multi_key,
	KC(64),  KS_Cmd_Screen0,	KS_f6,		KS_f1,
	KC(65),  KS_Cmd_Screen1,	KS_f7,		KS_f2,
	KC(66),  KS_Cmd_Screen2,	KS_f8,		KS_f3,
	KC(67),  KS_Cmd_Screen3,	KS_f9,		KS_f4,
	KC(68),  KS_Cmd_Screen4,	KS_f10,		KS_f5,
	KC(27),  KS_Cmd_BrightnessUp,	KS_bracketleft,	KS_braceleft,
	KC(43),  KS_Cmd_BrightnessDown,	KS_bracketright,KS_braceright,
	KC(51),  KS_Cmd_ContrastDown,	KS_comma,	KS_less,
	KC(52),  KS_Cmd_ContrastUp,	KS_period,	KS_greater,
	KC(57),  KS_Cmd_BacklightToggle,KS_space,
};

/* NTT DoCoMo Pocket PostPet (Japan) */
static uint8_t pocketpostpet_keytrans[] = {
/*00	esc	1	q	a	ctrl	-	down	left	*/
/*08	tab	2	w	s	z	-	right	up	*/
/*10	pgup	3	e	d	x	shift	-	-	*/
/*18	pgdn	4	r	f	c	alt	-	-	*/
/*20	f5	5	t	g	v	nfer	-	-	*/
/*28	f6	6	y	h	b	-	-	-	*/
/*30	f7	7	u	j	n	space	-	-	*/
/*38	f8	8	i	k	m	-	-	-	*/
/*40	f9	9	o	l	,	xfer	-	-	*/
/*48	f10	0	p	;	.	-	-	-	*/
/*50	\|	minus	@	:	/	ent	-	-	*/
/*58	bs	^	[	]	\_	del	-	-	*/
/* MailCheck  -> tab	*/
/* Prev       -> pgup	*/
/* Next       -> pgdn	*/
/* tab        -> ctrl	*/
/* h/z        -> menu	*/
/* k/h        -> alt	*/
/* knj        -> nfer	*/
/* eng        -> xfer	*/
/*----------------------------------------------------------------------*/
/*00*/	1,	2,	16,	30,	29,	UNK,	208,	203,
/*08*/	15,	3,	17,	31,	44,	UNK,	205,	200,
/*10*/	73,	4,	18,	32,	45,	42,	UNK,	UNK,
/*08*/	81,	5,	19,	33,	46,	56,	UNK,	UNK,
/*20*/	63,	6,	20,	34,	47,	123,	UNK,	UNK,
/*28*/	64,	7,	21,	35,	48,	UNK,	UNK,	UNK,
/*30*/	65,	8,	22,	36,	49,	57,	UNK,	UNK,
/*38*/	66,	9,	23,	37,	50,	UNK,	UNK,	UNK,
/*40*/	67,	10,	24,	38,	51,	121,	UNK,	UNK,
/*48*/	68,	11,	25,	39,	52,	UNK,	UNK,	UNK,
/*50*/	125,	12,	26,	40,	53,	28,	UNK,	UNK,
/*58*/	14,	13,	27,	43,	115,	14,	UNK,	UNK,
};
#endif /* hpcmips */

#ifdef hpcsh
/*
 * HP Jornada 680/690
 */
/* Japanese */
const uint8_t jornada6x0_jp_keytrans[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */ 59 , 45 , 31 , 17 , 3  , UNK, 29 , UNK, /* ctrl 29 */
/* 1 */ 60 , 46 , 32 , 18 , 4  , 42 , UNK, UNK, /* shift L 42 */
/* 2 */ 61 , 47 , 33 , 19 , 5  , UNK, 57 , UNK,
/* 3 */ 66 , 52 , 38 , 24 , 10 , UNK, 14 , 203,
/* 4 */ 65 , 51 , 37 , 23 , 9  , UNK, 115, UNK,
/* 5 */ 64 , 50 , 36 , 22 , 8  , UNK, 121, UNK,
/* 6 */ 62 , 48 , 34 , 20 , 6  , UNK, UNK, 56 , /* alt 56 */
/* 7 */ 63 , 49 , 35 , 21 , 7  , UNK, UNK, 123,
/* 8 */ IGN, 53 , 39 , 25 , 11 , 200, UNK, 208,
/* 9 */ 112, 40 , 27 , 26 , 12 , 125, UNK, 205,
/*10 */ 41 , 28 , 43 , 14 , 13 , 54 , UNK, UNK, /* shift R 54 */
/*11 */ SPL, IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*12 */ 1  , 44 , 30 , 16 , 2  , 15 , 221, UNK,
/*13 */ UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*14 */ UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*15 */ UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
};

/* US/UK - Fn to the left of the space bar and missing few keys */
const uint8_t jornada6x0_us_keytrans[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */ 59 , 45 , 31 , 17 , 3  , UNK, 29 , UNK,
/* 1 */ 60 , 46 , 32 , 18 , 4  , 42 , UNK, UNK,
/* 2 */ 61 , 47 , 33 , 19 , 5  , UNK, 57 , UNK,
/* 3 */ 66 , 52 , 38 , 24 , 10 , UNK, 211, 203,
/* 4 */ 65 , 51 , 37 , 23 , 9  , UNK, 53 , UNK,
/* 5 */ 64 , 50 , 36 , 22 , 8  , UNK, UNK, UNK,
/* 6 */ 62 , 48 , 34 , 20 , 6  , UNK, UNK, 56 ,
/* 7 */ 63 , 49 , 35 , 21 , 7  , UNK, UNK, 184,
/* 8 */ 67 , UNK, 39 , 25 , 11 , 200, UNK, 208,
/* 9 */ 68 , 40 , UNK, 43 , 12 , UNK, UNK, 205,
/*10 */ 87 , 28 , UNK, 14 , 13 , 54 , UNK, UNK,
/*11 */ SPL, IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*12 */ 1  , 44 , 30 , 16 , 2  , 15 , 219, UNK,
/*13 */ UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*14 */ UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*15 */ UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
};

/* International - AltGr to the right and extra keys in three middle rows */
const uint8_t jornada6x0_intl_keytrans[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */ 59 , 45 , 31 , 17 , 3  , UNK, 29 , UNK,
/* 1 */ 60 , 46 , 32 , 18 , 4  , 42 , UNK, UNK,
/* 2 */ 61 , 47 , 33 , 19 , 5  , UNK, 57 , UNK,
/* 3 */ 66 , 52 , 38 , 24 , 10 , UNK, 211, 203,
/* 4 */ 65 , 51 , 37 , 23 , 9  , UNK, 184, UNK,
/* 5 */ 64 , 50 , 36 , 22 , 8  , UNK, UNK, UNK,
/* 6 */ 62 , 48 , 34 , 20 , 6  , UNK, UNK, 56 ,
/* 7 */ 63 , 49 , 35 , 21 , 7  , UNK, UNK, 41 ,
/* 8 */ 67 , 53 , 39 , 25 , 11 , 200, UNK, 208,
/* 9 */ 68 , 40 , 27 , 26 , 12 , UNK, UNK, 205,
/*10 */ 87 , 28 , 43 , 14 , 13 , 54 , UNK, UNK,
/*11 */ SPL, IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*12 */ 1  , 44 , 30 , 16 , 2  , 15 , 219, UNK,
/*13 */ UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*14 */ UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*15 */ UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
};

const int jornada6x0_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= -1,
	[KEY_SPECIAL_LIGHT]	= -1
};


/*
 * HP 620LX
 */
/* Japanese */
const uint8_t hp620lx_jp_keytrans[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */	2  , 16 , 112, UNK, IGN, UNK, 42 , 30 , /* REC button */
/* 1 */	3  , 17 , 58 , 44 , UNK, 45 , 15 , 31 ,
/* 2 */	4  , 18 , UNK, 56 , 59 , 46 , 1  , 32 ,
/* 3 */	5  , 19 , UNK, UNK, 60 , 123, UNK, 33 ,
/* 4 */	6  , 20 , 47 , 57 , 61 , 48 , UNK, 34 ,
/* 5 */	7  , 21 , UNK, 121, 62 , 49 , UNK, 35 ,
/* 6 */	8  , 22 , UNK, 125, 63 , 50 , UNK, 36 ,
/* 7 */	9  , 23 , 52 , 115, 64 , 51 , UNK, 37 ,
/* 8 */	10 , 24 , 53 , 203, 65 , 200, 39 , 38 ,
/* 9 */	11 , 25 , 40 , 208, 66 , 205, 27 , 26 ,
/*10 */	12 , 13 , 28 , UNK, UNK, UNK, 41 , 43 ,
/*11 */	SPL, IGN, IGN, IGN, IGN, IGN, IGN, IGN, /* ON button */
/*12 */	29 , IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*13 */	14 , IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*14 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*15 */	SPL, IGN, IGN, IGN, IGN, IGN, IGN, IGN  /* LED button */
};

/* Other models */
const uint8_t hp620lx_intl_keytrans[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */	2  , 16 , 219, UNK, IGN, UNK, 42 , 30 , /* REC button */
/* 1 */	3  , 17 , 15 , 44 , UNK, 45 , 41 , 31 ,
/* 2 */	4  , 18 , UNK, 69 , 59 , 46 , 1  , 32 ,
/* 3 */	5  , 19 , UNK, UNK, 60 , 56 , UNK, 33 ,
/* 4 */	6  , 20 , 47 , 57 , 61 , 48 , UNK, 34 ,
/* 5 */	7  , 21 , UNK, UNK, 62 , 49 , UNK, 35 ,
/* 6 */	8  , 22 , UNK, 184, 63 , 50 , UNK, 36 ,
/* 7 */	9  , 23 , 52 , 211, 64 , 51 , UNK, 37 ,
/* 8 */	10 , 24 , 53 , 203, 65 , 200, 39 , 38 ,
/* 9 */	11 , 25 , 40 , 208, 66 , 205, 27 , 26 ,
/*10 */	12 , 13 , 28 , UNK, UNK, UNK, 54 , 43 ,
/*11 */	SPL, IGN, IGN, IGN, IGN, IGN, IGN, IGN, /* ON button */
/*12 */	29 , IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*13 */	14 , IGN, IGN, IGN, IGN, IGN, IGN, IGN,
/*14 */	UNK, UNK, UNK, UNK, UNK, UNK, UNK, UNK,
/*15 */	SPL, IGN, IGN, IGN, IGN, IGN, IGN, IGN  /* LED button */
};

const int hp620lx_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= -1,	/* 88 */
	[KEY_SPECIAL_LIGHT]	= 120
};


/*
 * HITACHI PERSONA HPW50PAD
 */
/* Japanese */
const uint8_t persona_hpw50pad_jp_keytrans[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */ 59 , 61 , 63 , 65 , 67 , SPL, UNK, UNK, /* ON button */
/* 1 */ 29 , UNK, 47 , 48 , 121, UNK, UNK, UNK,
/* 2 */  1 , UNK, 34 , 51 , 112, UNK, UNK, UNK,
/* 3 */ 41 , 221, 35 , 37 , 52 , UNK, UNK, UNK,
/* 4 */ 45 , 58 , 22 , 38 , 40 , 42 , UNK, UNK,
/* 5 */ 30 , 16 , 21 , 24 , 28 , 54 , UNK, UNK,
/* 6 */ 17 ,  4 , UNK, 10 , 12 , 205, UNK, UNK,
/* 7 */ 31 , 18 ,  7 , 25 , 13 , 83 , UNK, UNK,
/* 8 */ 60 , 62 , 64 , 66 , 68 , UNK, UNK, UNK,
/* 9 */ UNK, 123, 49 , 53 , 203, UNK, UNK, UNK,
/*10 */ 56 , 46 , 50 , 57 , 208, UNK, UNK, UNK,
/*11 */ 15 , 33 , 36 , 39 , 43 , UNK, UNK, UNK,
/*12 */ 44 , 20 , 23 , 26 , 27 , UNK, UNK, UNK,
/*13 */  2 ,  6 ,  9 , 115, 200, UNK, UNK, UNK,
/*14 */  3 ,  5 , UNK, UNK, 125, UNK, UNK, UNK,
/*15 */ 32 , 19 ,  8 , 11 , 14 , UNK, UNK, UNK,
};

const int persona_hpw50pad_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= 5,
	[KEY_SPECIAL_LIGHT]	= -1
};

static const keysym_t persona_hpw50pad_jp_keydesc[] = {
/*	pos	command			normal		shifted		*/
	KC(63),	KS_Cmd_ContrastDown,	KS_f5,
	KC(64),	KS_Cmd_ContrastUp,	KS_f6,
	KC(65),	KS_Cmd_BrightnessDown,	KS_f7,
	KC(66),	KS_Cmd_BrightnessUp,	KS_f8,

	KC(200),KS_Cmd_ScrollFastUp,	KS_Up,
	KC(208),KS_Cmd_ScrollFastDown,	KS_Down,
	KC(203),KS_KP_Home,		KS_Left,
	KC(205),KS_KP_End,		KS_Right,
};

/*
 * HITACHI PERSONA HPW200EC
*/
/* US */
const uint8_t persona_hpw200ec_keytrans[] = {
/*      0    1    2    3    4    5    6    7 */
/* 0 */  3 ,  2 , 71 , 69 , 67 , SPL, UNK, UNK,
/* 1 */ 11 , UNK, 79 , 77 , 75 , UNK, UNK, UNK,
/* 2 */ 19 , UNK, UNK, 85 , 83 , UNK, UNK, UNK,
/* 3 */ 27 , 26 , 95 , 93 , 91 , UNK, UNK, UNK,
/* 4 */ 35 , 34 , 103, 101, 99 , 32 , UNK, UNK,
/* 5 */ 43 , 42 , 111, 109, 107, 40 , UNK, UNK,
/* 6 */ 51 , 50 , 119, 117, 115, 48 , UNK, UNK,
/* 7 */ 59 , 58 , UNK, UNK, 123, 56 , UNK, UNK,
/* 8 */  5 ,  6 , 70 , 68 , 66 , UNK, UNK, UNK,
/* 9 */ UNK, UNK, 78 , 76 , 74 , UNK, UNK, UNK,
/*10 */ 21 , UNK, UNK, 84 , 82 , UNK, UNK, UNK,
/*11 */ 29 , 30 , 94 , 92 , 90 , UNK, UNK, UNK,
/*12 */ UNK, 26 , 102, 100, 98 , UNK, UNK, UNK,
/*13 */ UNK, 46 , 110, 108, 106, 104, UNK, UNK,
/*14 */ 53 , 54 , 118, 124, 114, UNK, UNK, UNK,
/*15 */ 61 , 62 , 126, UNK, 122, UNK, UNK, UNK,
};

const int persona_hpw200ec_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= 5,
	[KEY_SPECIAL_LIGHT]	= -1
};

#endif /* hpcsh */

#ifdef hpcarm
/*
 * HP Jornada 710/720/728
 */

/* Japanese */
const uint8_t jornada7xx_jp_keytrans[] = {
/* 00 */ UNK,  1 , 59 , 60 , 61 , 62 , 63 , 64 ,
/* 08 */ 65 , 66 , 67 , 112, 41 , IGN, IGN, IGN,
/* 10 */ UNK,  2 ,  3 ,  4 ,  5 ,  6 ,  7 ,  8 ,
/* 18 */  9 , 10 , 11 , 12 , 13 , UNK, UNK, UNK,
/* 20 */ UNK, 16 , 17 , 18 , 19 , 20 , 21 , 22 ,
/* 28 */  23, 24 , 25 , 26 , UNK, UNK, UNK, UNK,
/* 30 */ UNK, 30 , 31 , 32 , 33 , 34 , 35 , 36 ,
/* 38 */  37, 38 , 39 , 27 , 43 , UNK, UNK, UNK,
/* 40 */ UNK, 44 , 45 , 46 , 47 , 48 , 49 , 50 ,
/* 48 */  51, 52 , 53 , 40 , 28 , UNK, UNK, UNK,
/* 50 */ UNK, 15 , UNK, 42 , UNK, UNK, UNK, UNK,
/* 58 */ UNK, UNK, 200, 125, 54 , UNK, UNK, UNK,
/* 60 */ UNK, UNK, UNK, UNK, UNK, 56 , 123, UNK,
/* 68 */ UNK, 203, 208, 205, UNK, UNK, UNK, UNK,
/* 70 */ UNK, 184, 29 , UNK, 57 , UNK, UNK, 121,
/* 78 */ 53 , 14 , UNK, UNK, UNK, UNK, UNK, SPL,
};

/* US/UK - Fn to the left of the space bar and missing few keys */
const uint8_t jornada7xx_us_keytrans[] = {
/* 00 */ UNK,  1 , 59 , 60 , 61 , 62 , 63 , 64 ,
/* 08 */ 65 , 66 , 67 , 68 , 87 , IGN, IGN, IGN,
/* 10 */ UNK,  2 ,  3 ,  4 ,  5 ,  6 ,  7 ,  8 ,
/* 18 */  9 , 10 , 11 , 12 , 13 , UNK, UNK, UNK,
/* 20 */ UNK, 16 , 17 , 18 , 19 , 20 , 21 , 22 ,
/* 28 */  23, 24 , 25 , 43 , 14 , UNK, UNK, UNK,
/* 30 */ UNK, 30 , 31 , 32 , 33 , 34 , 35 , 36 ,
/* 38 */  37, 38 , 39 , 40 , UNK, UNK, UNK, UNK,
/* 40 */ UNK, 44 , 45 , 46 , 47 , 48 , 49 , 50 ,
/* 48 */  51, 52 , UNK, 40 , 28 , UNK, UNK, UNK,
/* 50 */ UNK, 15 , UNK, 42 , UNK, UNK, UNK, UNK,
/* 58 */ UNK, UNK, 200, UNK, 54 , UNK, UNK, UNK,
/* 60 */ UNK, UNK, UNK, UNK, UNK, 56 , 184, UNK,
/* 68 */ UNK, 203, 208, 205, UNK, UNK, UNK, UNK,
/* 70 */ UNK, 221, 29 , UNK, 57 , UNK, UNK, UNK,
/* 78 */ 53 , 14 , UNK, UNK, UNK, UNK, UNK, SPL,
};

/* International - AltGr to the right and extra keys in three middle rows */
const uint8_t jornada7xx_intl_keytrans[] = {
/* 00 */ UNK,  1 , 59 , 60 , 61 , 62 , 63 , 64 ,
/* 08 */ 65 , 66 , 67 , 68 , 87 , IGN, IGN, IGN,
/* 10 */ UNK,  2 ,  3 ,  4 ,  5 ,  6 ,  7 ,  8 ,
/* 18 */  9 , 10 , 11 , 12 , 13 , UNK, UNK, UNK,
/* 20 */ UNK, 16 , 17 , 18 , 19 , 20 , 21 , 22 ,
/* 28 */  23, 24 , 25 , 26 , 14 , UNK, UNK, UNK,
/* 30 */ UNK, 30 , 31 , 32 , 33 , 34 , 35 , 36 ,
/* 38 */  37, 38 , 39 , 27 , 43 , UNK, UNK, UNK,
/* 40 */ UNK, 44 , 45 , 46 , 47 , 48 , 49 , 50 ,
/* 48 */  51, 52 , 53 , 40 , 28 , UNK, UNK, UNK,
/* 50 */ UNK, 15 , UNK, 42 , UNK, UNK, UNK, UNK,
/* 58 */ UNK, UNK, 200, UNK, 54 , UNK, UNK, UNK,
/* 60 */ UNK, UNK, UNK, UNK, UNK, 56 , 41 , UNK,
/* 68 */ UNK, 203, 208, 205, UNK, UNK, UNK, UNK,
/* 70 */ UNK, 221, 29 , UNK, 57 , UNK, UNK, UNK,
/* 78 */ 184, 211, UNK, UNK, UNK, UNK, UNK, SPL,
};

const int jornada7xx_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= 127,
	[KEY_SPECIAL_LIGHT]	= -1
};

/*
 * Sharp W-ZERO3 series
 */
/*
 * WS003SH/WS004SH/WS007SH keyscan map
	CTRL	(none)	TAB	(none)	CALL	MAIL	IE
	1	2	q	w	a	z	MOJI
	3	4	e	s	d	x	(none)
	5	r	t	f	c	-	OK
	6	y	g	v	b	SPACE	ACTION
	7	8	u	h	n	/	,
	9	i	j	m	.	(none)	LEFT
	0	o	k	l	(none)	UP	DOWN
	BS	p	(none)	(none)	ENTER	(none)	RIGHT
	(none)	(none)	(none)	(none)	(none)	(none)	(none)
	ROTATE	VOL-	(none)	SHIFT	WIN	LSOFT	RSOFT
	CAMERA	VOL+	(none)	(none)	(none)	FN	(none)
*/
/* Japanese */
const uint8_t ws003sh_jp_keytrans[] = {
/*	row#0,	row#1,	row#2,	row#3,	row#4,	row#5,	row#6,	*/
/*00*/	29,	UNK,	15,	UNK,	IGN,	IGN,	IGN,
/*01*/	2,	3,	16,	17,	30,	44,	1,
/*02*/	4,	5,	18,	31,	32,	45,	UNK,
/*03*/	6,	19,	20,	33,	46,	12,	28,
/*04*/	7,	21,	34,	47,	48,	57,	28,
/*05*/	8,	9,	22,	35,	49,	53,	51,
/*06*/	10,	23,	36,	50,	52,	UNK,	203,
/*07*/	11,	24,	37,	38,	UNK,	200,	208,
/*08*/	14,	25,	UNK,	UNK,	28,	UNK,	205,
/*09*/	UNK,	UNK,	UNK,	UNK,	UNK,	UNK,	UNK,
/*10*/	IGN,	174,	UNK,	42,	IGN,	IGN,	IGN,
/*11*/	IGN,	176,	UNK,	UNK,	UNK,	184,	SPL,
};

const int ws003sh_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= 83,
	[KEY_SPECIAL_LIGHT]	= -1
};

static const keysym_t ws003sh_jp_keydesc[] = {
/*  pos		normal		shifted		altgr	*/
    KC(4),	KS_3,		KS_numbersign,	KS_Cmd_BrightnessDown,
    KC(5),	KS_4,		KS_dollar,	KS_Cmd_BrightnessUp,
    KC(8),	KS_7,		KS_apostrophe,	KS_grave,
    KC(9),	KS_8,		KS_parenleft,	KS_braceleft,
    KC(10),	KS_9,		KS_parenright,	KS_braceright,
    KC(12),	KS_minus,	KS_equal,	KS_backslash,
    KC(14),	KS_Delete,	KS_Delete,	KS_BackSpace,
    KC(15),	KS_Tab,		KS_Tab,		KS_Escape,
    KC(17),	KS_w,		KS_W,		KS_asciicircum,
    KC(18),	KS_e,		KS_E,		KS_asciitilde,
    KC(19),	KS_r,		KS_R,		KS_bar,
    KC(22),	KS_u,		KS_U,		KS_bracketleft,
    KC(23),	KS_i,		KS_I,		KS_bracketright,
    KC(24),	KS_o,		KS_O,		KS_underscore,
    KC(25),	KS_p,		KS_P,		KS_at,
    KC(37),	KS_k,		KS_K,		KS_plus,
    KC(38),	KS_l,		KS_L,		KS_asterisk,
    KC(42),	KS_Shift_L,	KS_Shift_L,	KS_Shift_Lock,
    KC(51),	KS_comma,	KS_semicolon,	KS_less,
    KC(52),	KS_period,	KS_colon,	KS_greater,
    KC(184),	KS_Mode_switch,	KS_Multi_key,
    KC(200),	KS_Up,		KS_Up,		KS_Prior,
    KC(203),	KS_Left,	KS_Left,	KS_Home,
    KC(205),	KS_Right,	KS_Right,	KS_End,
    KC(208),	KS_Down,	KS_Down,	KS_Next,
};

/*
 * WS011SH keyscan map
	Ctrl	(none)	Tab	(none)	(none)	(none)	(none)
	(none)	(none)	Q	W	A	Z	MOJI
	(none)	(none)	E	S	D	X	HAN/ZEN
	(none)	R	T	F	C	-	OK
	(none)	Y	G	V	B	Space	(none)
	(none)	(none)	U	H	N	/	,
	(none)	I	J	M	.	(none)	LEFT
	(none)	O	K	L	(none)	UP	DOWN
	Del	P	(none)	(none)	Enter	(none)	RIGHT
	(none)	(none)	(none)	(none)	(none)	(none)	(none)
	ROTATE	(none)	(none)	SHIFT	(none)	(none)	(none)
	(none)	(none)	(none)	(none)	(none)	FN	(none)
*/
/* Japanese */
const uint8_t ws011sh_jp_keytrans[] = {
/*	row#0,	row#1,	row#2,	row#3,	row#4,	row#5,	row#6,	*/
/*00*/	29,	UNK,	15,	UNK,	UNK,	UNK,	UNK,
/*01*/	UNK,	UNK,	16,	17,	30,	44,	1,
/*02*/	UNK,	UNK,	18,	31,	32,	45,	41,
/*03*/	UNK,	19,	20,	33,	46,	12,	3,
/*04*/	UNK,	21,	34,	47,	48,	57,	UNK,
/*05*/	UNK,	UNK,	22,	35,	49,	53,	51,
/*06*/	UNK,	23,	36,	50,	52,	UNK,	203,
/*07*/	UNK,	24,	37,	38,	UNK,	200,	208,
/*08*/	14,	25,	UNK,	UNK,	28,	UNK,	205,
/*09*/	UNK,	UNK,	UNK,	UNK,	UNK,	UNK,	UNK,
/*10*/	IGN,	UNK,	UNK,	42,	UNK,	UNK,	UNK,
/*11*/	UNK,	UNK,	UNK,	UNK,	UNK,	184,	SPL,
};

const int ws011sh_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= 83,
	[KEY_SPECIAL_LIGHT]	= -1
};

static const keysym_t ws011sh_jp_keydesc[] = {
/*  pos		normal		shifted		altgr	*/
    KC(3),	KS_grave,	KS_braceleft,	KS_braceright,
    KC(12),	KS_minus,	KS_equal,	KS_backslash,
    KC(14),	KS_Delete,	KS_Delete,	KS_BackSpace,
    KC(15),	KS_Tab,		KS_Tab,		KS_Escape,
    KC(16),	KS_q,		KS_Q,		KS_quotedbl,	
    KC(17),	KS_w,		KS_W,		KS_numbersign,
    KC(18),	KS_e,		KS_E,		KS_dollar,
    KC(19),	KS_r,		KS_R,		KS_percent,
    KC(20),	KS_t,		KS_T,		KS_ampersand,
    KC(21),	KS_y,		KS_Y,		KS_1,
    KC(22),	KS_u,		KS_U,		KS_2,
    KC(23),	KS_i,		KS_I,		KS_3,
    KC(24),	KS_o,		KS_O,		KS_underscore,
    KC(25),	KS_p,		KS_P,		KS_at,
    KC(30),	KS_a,		KS_A,		KS_bracketleft,
    KC(31),	KS_s,		KS_S,		KS_apostrophe,
    KC(32),	KS_d,		KS_D,		KS_parenleft,
    KC(33),	KS_f,		KS_F,		KS_parenright,
    KC(34),	KS_g,		KS_G,		KS_asterisk,
    KC(35),	KS_h,		KS_H,		KS_4,
    KC(36),	KS_j,		KS_J,		KS_5,
    KC(37),	KS_k,		KS_K,		KS_6,
    KC(38),	KS_l,		KS_L,		KS_plus,
    KC(41),	KS_Zenkaku_Hankaku, KS_Zenkaku_Hankaku, KS_exclam,
    KC(42),	KS_Shift_L,	KS_Shift_L,	KS_Shift_Lock,
    KC(44),	KS_z,		KS_Z,		KS_bracketright,
    KC(45),	KS_x,		KS_X,		KS_asciicircum,
    KC(46),	KS_c,		KS_C,		KS_asciitilde,
    KC(47),	KS_v,		KS_V,		KS_bar,
    KC(48),	KS_b,		KS_B,		KS_7,
    KC(49),	KS_n,		KS_N,		KS_8,
    KC(50),	KS_m,		KS_M,		KS_9,
    KC(51),	KS_comma,	KS_less,	KS_semicolon,
    KC(52),	KS_period,	KS_greater,	KS_colon,
    KC(53),	KS_slash,	KS_question,	KS_0,
    KC(57),	KS_space,
    KC(184),	KS_Mode_switch,	KS_Multi_key,
    KC(200),	KS_Up,		KS_Up,		KS_Prior,
    KC(203),	KS_Left,	KS_Left,	KS_Home,
    KC(205),	KS_Right,	KS_Right,	KS_End,
    KC(208),	KS_Down,	KS_Down,	KS_Next,
};

/*
 * WS020SH keyscan map
	Ctrl	(none)	Tab	(none)	(none)	(none)	(none)
	(none)	(none)	Q	W	A	Z	MOJI
	(none)	(none)	E	S	D	X	HAN/ZEN
	(none)	R	T	F	C	-	OK
	(none)	Y	G	V	B	Space	(none)
	(none)	(none)	U	H	N	/	,
	(none)	I	J	M	.	(none)	LEFT
	(none)	O	K	L	(none)	UP	(none)
	Del	P	(none)	(none)	Enter	(none)	RIGHT
	(none)	(none)	(none)	(none)	(none)	DOWN	(none)
	ROTATE	MEDIA	(none)	LSHIFT	RSHIFT	(none)	(none)
	(none)	(none)	(none)	(none)	(none)	FN	(none)
*/
/* Japanese */
const uint8_t ws020sh_jp_keytrans[] = {
/*	row#0,	row#1,	row#2,	row#3,	row#4,	row#5,	row#6,	*/
/*00*/	29,	UNK,	15,	UNK,	UNK,	UNK,	UNK,
/*01*/	UNK,	UNK,	16,	17,	30,	44,	1,
/*02*/	UNK,	UNK,	18,	31,	32,	45,	41,
/*03*/	UNK,	19,	20,	33,	46,	12,	3,
/*04*/	UNK,	21,	34,	47,	48,	57,	UNK,
/*05*/	UNK,	UNK,	22,	35,	49,	53,	51,
/*06*/	UNK,	23,	36,	50,	52,	UNK,	203,
/*07*/	UNK,	24,	37,	38,	UNK,	200,	UNK,
/*08*/	14,	25,	UNK,	UNK,	28,	UNK,	205,
/*09*/	UNK,	UNK,	UNK,	UNK,	UNK,	208,	UNK,
/*10*/	IGN,	IGN,	UNK,	42,	54,	UNK,	UNK,
/*11*/	UNK,	UNK,	UNK,	UNK,	UNK,	184,	SPL,
};

const int ws020sh_special_keymap[] = {
	[KEY_SPECIAL_OFF]	= 83,
	[KEY_SPECIAL_LIGHT]	= -1
};

static const keysym_t ws020sh_jp_keydesc[] = {
/*  pos		normal		shifted		altgr	*/
    KC(3),	KS_grave,	KS_braceleft,	KS_braceright,
    KC(12),	KS_minus,	KS_equal,	KS_backslash,
    KC(14),	KS_Delete,	KS_Delete,	KS_BackSpace,
    KC(15),	KS_Tab,		KS_Tab,		KS_Escape,
    KC(16),	KS_q,		KS_Q,		KS_quotedbl,	
    KC(17),	KS_w,		KS_W,		KS_numbersign,
    KC(18),	KS_e,		KS_E,		KS_dollar,
    KC(19),	KS_r,		KS_R,		KS_percent,
    KC(20),	KS_t,		KS_T,		KS_ampersand,
    KC(21),	KS_y,		KS_Y,		KS_underscore,
    KC(22),	KS_u,		KS_U,		KS_1,
    KC(23),	KS_i,		KS_I,		KS_2,
    KC(24),	KS_o,		KS_O,		KS_3,
    KC(25),	KS_p,		KS_P,		KS_at,
    KC(30),	KS_a,		KS_A,		KS_bracketleft,
    KC(31),	KS_s,		KS_S,		KS_apostrophe,
    KC(32),	KS_d,		KS_D,		KS_parenleft,
    KC(33),	KS_f,		KS_F,		KS_parenright,
    KC(34),	KS_g,		KS_G,		KS_asterisk,
    KC(35),	KS_h,		KS_H,		KS_4,
    KC(36),	KS_j,		KS_J,		KS_5,
    KC(37),	KS_k,		KS_K,		KS_6,
    KC(38),	KS_l,		KS_L,		KS_plus,
    KC(41),	KS_Zenkaku_Hankaku, KS_Zenkaku_Hankaku, KS_exclam,
    KC(42),	KS_Shift_L,	KS_Shift_L,	KS_Shift_Lock,
    KC(44),	KS_z,		KS_Z,		KS_bracketright,
    KC(45),	KS_x,		KS_X,		KS_asciicircum,
    KC(46),	KS_c,		KS_C,		KS_asciitilde,
    KC(47),	KS_v,		KS_V,		KS_bar,
    KC(48),	KS_b,		KS_B,		KS_7,
    KC(49),	KS_n,		KS_N,		KS_8,
    KC(50),	KS_m,		KS_M,		KS_9,
    KC(51),	KS_comma,	KS_less,	KS_semicolon,
    KC(52),	KS_period,	KS_greater,	KS_colon,
    KC(53),	KS_slash,	KS_question,	KS_0,
    KC(54),	KS_Shift_R,	KS_Shift_R,	KS_Shift_Lock,
    KC(57),	KS_space,
    KC(184),	KS_Mode_switch,	KS_Multi_key,
    KC(200),	KS_Up,		KS_Up,		KS_Prior,
    KC(203),	KS_Left,	KS_Left,	KS_Home,
    KC(205),	KS_Right,	KS_Right,	KS_End,
    KC(208),	KS_Down,	KS_Down,	KS_Next,
};

const uint8_t netbookpro_keytrans[] = {
/*	row#0,	row#1,	row#2,	row#3,	row#4,	row#5,	row#6,	row#7	*/
/*00*/	 28,	205,	 15,	 21,	203,	208,	 49,	 42,
	UNK,	UNK,	UNK,	UNK,	UNK,	 14,	UNK,	 12,
/*10*/	 13,	 11,	 25,	 39,	UNK,	 54,	UNK,	UNK,
	UNK,	UNK,	UNK,	 37,	 23,	  9,	 10,	 24,
/*20*/	 38,	UNK,	UNK,	 29,	UNK,	UNK,	UNK,	UNK,
	 51,	 40,	 50,	 36,	 22,	  8,	UNK,	UNK,
/*30*/	UNK,	184,	UNK,	UNK,	 57,	 19,	  5,	  6,
	 20,	 34,	 48,	UNK,	UNK,	UNK,	UNK,	 43,
/*40*/	UNK,	 56,	 33,	 47,	 46,	 32,	 18,	  4,
	UNK,	UNK,	UNK,	UNK,	UNK,	 53,	UNK,	 16,
/*50*/	 30,	 44,	 31,	 17,	 45,	UNK,	UNK,	UNK,
	UNK,	UNK,	UNK,	  1,	  2,	  3,	  7,	 52,
/*60*/	UNK,	 35,	UNK,	UNK,	UNK,	UNK,	UNK,	UNK,
	UNK,	UNK,	UNK,	UNK,	UNK,	UNK,	UNK,	UNK,
};

static const keysym_t netbookpro_keydesc[] = {
/* pos      command		normal		shifted		fn	*/
   KC(2),   KS_Cmd_Screen0,	KS_1,		KS_exclam,	KS_f1,
   KC(3),   KS_Cmd_Screen1,	KS_2,		KS_at,		KS_f2,
   KC(4),   KS_Cmd_Screen2,	KS_3,		KS_numbersign,	KS_f3,
   KC(5),   KS_Cmd_Screen3,	KS_4,		KS_dollar,	KS_f4,
   KC(6),   			KS_5,		KS_percent,	KS_grave,
   KC(7),   			KS_6,		KS_asciicircum,	KS_apostrophe,
   KC(8),   			KS_7,		KS_ampersand,	KS_braceleft,
   KC(9),   			KS_8,		KS_asterisk,	KS_braceright,
   KC(10),  			KS_9,		KS_parenleft,	KS_bracketleft,
   KC(11),  			KS_0,		KS_parenright,	KS_bracketright,
   KC(12),  			KS_minus,	KS_underscore,	KS_numbersign,
   KC(15),  			KS_Tab,		KS_BackSpace,	KS_Caps_Lock,
   KC(40),			KS_apostrophe,	KS_quotedbl,	KS_at,
   KC(43),			KS_backslash,	KS_asciitilde,	KS_bar,
   KC(51),			KS_comma,	KS_less,	KS_minus,
   KC(52),  			KS_period,	KS_greater,	KS_plus,
   KC(53),  			KS_slash,	KS_question,	KS_Help,
   KC(184),			KS_Mode_switch,	KS_Multi_key,
   KC(203),			KS_Left,	KS_Left,	KS_Home,
   KC(205),			KS_Right,	KS_Right,	KS_End,
};
#endif /* hpcarm */

#if defined(hpcarm) || defined(hpcsh)
/*
 * Shared keymaps between the Jornada series (6xx, 7xx).
 */

/* US (ABA), UK (ABU) */
static const keysym_t jornada_us_keydesc[] = {
/*  pos      normal          shifted        altgr       */
    KC(2),   KS_1,           KS_exclam,     KS_asciitilde,
    KC(3),   KS_2,           KS_at,         KS_grave,
    KC(4),   KS_3,           KS_numbersign, KS_sterling,
#ifdef KS_euro
    KC(5),   KS_4,           KS_dollar,     KS_euro,
#endif
    KC(25),  KS_p,           KS_P,          KS_braceleft,
    KC(39),  KS_semicolon,   KS_colon,      KS_bracketleft,
    KC(40),  KS_apostrophe,  KS_quotedbl,   KS_bracketright,
    KC(43),  KS_backslash,   KS_bar,        KS_braceright,
    KC(184), KS_Mode_switch, KS_Multi_key,

    KC(200), KS_Cmd_BrightnessUp,   KS_Up,
    KC(203), KS_Cmd_ContrastDown,   KS_Left,
    KC(205), KS_Cmd_ContrastUp,     KS_Right,
    KC(208), KS_Cmd_BrightnessDown, KS_Down,
};

/*
 * XXX: Add AltGr layer for #ABB here?  OTOH, all the keys necessary
 * for basic actions in DDB or shell are on the primary layer, so it
 * makes sense to support AltGr via wsconsctl(8) instead, as the same
 * primary layer is used e.g. in Russian models.  But it does make
 * sense to define the <AltGr> key itself here, as we base this layout
 * on KB_US that defines it as the right <Alt>.
 */
/* European English (ABB) */
static const keysym_t jornada_intl_keydesc[] = {
/*  pos      normal          shifted        altgr       */
    KC(42),  KS_Shift_L,     KS_Shift_L,    KS_Caps_Lock,
    KC(184), KS_Mode_switch, KS_Multi_key,

    KC(200), KS_Cmd_BrightnessUp,   KS_Up,
    KC(203), KS_Cmd_ContrastDown,   KS_Left,
    KC(205), KS_Cmd_ContrastUp,     KS_Right,
    KC(208), KS_Cmd_BrightnessDown, KS_Down,
};

/* German (ABD) */
static const keysym_t jornada_de_keydesc[] = {
/*  pos      normal          shifted        altgr       */
    KC(2),   KS_1,           KS_exclam,     KS_brokenbar,
    KC(5),   KS_4,           KS_dollar,     KS_ccedilla,
    KC(6),   KS_5,           KS_percent,    KS_sterling,
    KC(7),   KS_6,           KS_ampersand,  KS_notsign,
#ifdef KS_euro
    KC(18),  KS_e,           KS_E,          KS_euro,
#endif
    KC(27),  KS_plus,        KS_asterisk,   KS_asciitilde, /* NB: not dead */
    KC(30),  KS_a,           KS_A,          KS_bar,
    KC(41),  KS_asciicircum, KS_degree,                    /* NB: not dead */
    KC(43),  KS_numbersign,  KS_apostrophe, KS_dead_diaeresis,
    KC(44),  KS_y,           KS_Y,          KS_less,
    KC(45),  KS_x,           KS_X,          KS_greater,
    KC(46),  KS_c,           KS_C,          KS_cent,

    KC(200), KS_Cmd_BrightnessUp,   KS_Up,
    KC(203), KS_Cmd_ContrastDown,   KS_Left,
    KC(205), KS_Cmd_ContrastUp,     KS_Right,
    KC(208), KS_Cmd_BrightnessDown, KS_Down,
};

/* French (ABF) */
static const keysym_t jornada_fr_keydesc[] = {
/*  pos      normal          shifted        altgr       */
    KC(2),   KS_ampersand,   KS_1,          KS_plusminus,
#ifdef KS_euro
    KC(18),  KS_e,           KS_E,          KS_euro,
#endif
    KC(19),  KS_r,           KS_R,          KS_onequarter,
    KC(20),  KS_t,           KS_T,          KS_onehalf,
    KC(21),  KS_y,           KS_Y,          KS_threequarters,
    KC(25),  KS_p,           KS_P,          KS_paragraph,
    KC(30),  KS_q,           KS_Q,          KS_brokenbar,
    KC(31),  KS_s,           KS_S,          KS_guillemotleft,
    KC(32),  KS_d,           KS_D,          KS_guillemotright,
    KC(40),  KS_ugrave,      KS_percent,    KS_dead_acute,
    KC(41),  KS_twosuperior, KS_voidSymbol, KS_threesuperior,
    KC(43),  KS_asterisk,    KS_mu,         KS_notsign,
    KC(44),  KS_w,           KS_W,          KS_less,
    KC(45),  KS_x,           KS_X,          KS_greater,
    KC(46),  KS_c,           KS_C,          KS_cent,
    KC(50),  KS_comma,       KS_question,   KS_mu,
    KC(53),  KS_exclam,      KS_section,    KS_Eacute,
    KC(184), KS_Mode_switch, KS_Multi_key,

    KC(200), KS_Cmd_BrightnessUp,   KS_Up,
    KC(203), KS_Cmd_ContrastDown,   KS_Left,
    KC(205), KS_Cmd_ContrastUp,     KS_Right,
    KC(208), KS_Cmd_BrightnessDown, KS_Down,
};

/* Scandinavian */
static const keysym_t jornada_scnv_keydesc[] = {
/*  pos      normal          shifted        altgr       */
    KC(2),   KS_1,           KS_exclam,     KS_asciitilde,
    KC(3),   KS_2,           KS_quotedbl,   KS_at,
    KC(4),   KS_3,           KS_numbersign, KS_sterling,
    KC(5),   KS_4,           KS_currency,   KS_dollar,
    KC(7),   KS_6,           KS_ampersand,
    KC(8),   KS_7,           KS_slash,      KS_braceleft,
    KC(9),   KS_8,           KS_parenleft,  KS_bracketleft,
    KC(10),  KS_9,           KS_parenright, KS_bracketright,
    KC(11),  KS_0,           KS_equal,      KS_braceright,
    KC(12),  KS_plus,        KS_question,   KS_backslash,
    KC(13),  KS_apostrophe,  KS_grave,
    KC(25),  KS_p,           KS_P,          KS_braceleft,
    KC(26),  KS_aring,
    /*
     * XXX: KC(39) and KC(40) has odiaeresis/adiaeresis *and*
     * oslash/ae on them.  Apparently localized WinCE uses the former
     * for Swedish and Finnish and the latter for Danish and
     * Norwegian.  But as the keyboard doesn't seem to have
     * semicolon/colon and minus/underscore nowhere on the primary and
     * altgr layers, I put them here (semicolon/colon is inherited).
     */
    KC(40),  KS_minus,       KS_underscore, /* XXX */
    KC(41),  KS_paragraph,   KS_onehalf,    KS_bar,
    KC(184), KS_Mode_switch, KS_Multi_key,

    KC(200), KS_Cmd_BrightnessUp,   KS_Up,
    KC(203), KS_Cmd_ContrastDown,   KS_Left,
    KC(205), KS_Cmd_ContrastUp,     KS_Right,
    KC(208), KS_Cmd_BrightnessDown, KS_Down,
};

/* Spanish (ABE) */
static const keysym_t jornada_es_keydesc[] = {
/*  pos      normal          shifted        altgr            */
    KC(2),   KS_1,           KS_exclam,     KS_ordfeminine,
    KC(3),   KS_2,           KS_quotedbl,   KS_masculine,
    KC(4),   KS_3,           KS_numbersign,
    KC(12),  KS_apostrophe,  KS_question,   KS_backslash,
    KC(13),  KS_questiondown,KS_exclamdown,
    KC(16),  KS_q,           KS_Q,          KS_at,
#ifdef KS_euro
    KC(18),  KS_e,           KS_E,          KS_euro,
#endif
    KC(26),  KS_dead_acute,  KS_dead_diaeresis,
    KC(27),  KS_plus,        KS_asterisk,   KS_dead_tilde,
    KC(40),  KS_braceleft,   KS_bracketleft,KS_dead_circumflex,
    KC(41),  KS_bar,         KS_degree,     KS_notsign,
    KC(43),  KS_braceright,  KS_bracketright,KS_dead_grave,
    KC(44),  KS_z,           KS_Z,          KS_less,
    KC(45),  KS_x,           KS_X,          KS_greater,
    KC(46),  KS_c,           KS_C,          KS_Ccedilla,

    KC(200), KS_Cmd_BrightnessUp,   KS_Up,
    KC(203), KS_Cmd_ContrastDown,   KS_Left,
    KC(205), KS_Cmd_ContrastUp,     KS_Right,
    KC(208), KS_Cmd_BrightnessDown, KS_Down,
};
#endif /* hpcarm || hpcsh */

/*
 * REMINDER:
 *   When adding new entry to this array, make sure that pckbd_keydesctab[]
 *   in sys/dev/pckbport/wskbdmap_mfii.c has a placeholder KB_MACHDEP entry
 *   for the base ht_layout that you refer.
 */
const struct hpckbd_keymap_table {
	platid_t	*ht_platform;
	const uint8_t	*ht_keymap;
	const int	*ht_special;
	struct {
		const keysym_t	*map;
		int size;
	} ht_cmdmap;
	kbd_t		ht_layout;
} hpckbd_keymap_table[] = {
#ifdef hpcmips
	{	&platid_mask_MACH_COMPAQ_C,
		tc5165_compaq_c_jp_keymap,
		tc5165_compaq_c_jp_special_keymap,
		NULLCMDMAP,
		KB_JP },
	{	&platid_mask_MACH_VICTOR_INTERLINK,
		m38813c_keymap,
		default_special_keymap,
		NULLCMDMAP,
		KB_JP },
	{	&platid_mask_MACH_SHARP_TELIOS,
		tc5165_telios_jp_keymap,
		default_special_keymap,
		CMDMAP(tc5165_telios_jp_cmdmap),
		KB_JP },
	{	&platid_mask_MACH_SHARP_MOBILON,
		tc5165_mobilon_keymap,
		tc5165_mobilon_special_keymap,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_500A,
		mobilepro750c_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_520A,
		mobilepro_keytrans,
		NULL,
		CMDMAP(mobilepro_cmdmap),
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_530A,
		mobilepro780_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_700A,
		mobilepro8x0_keytrans,
		NULL,
		CMDMAP(mobilepro8x0_cmdmap),
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_730A,
		mobilepro8x0_keytrans,
		NULL,
		CMDMAP(mobilepro8x0_cmdmap),
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_MPRO700,
		mobilepro_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_NEC_MCR_SIGMARION,
		sigmarion_jp_keytrans,
		NULL,
		CMDMAP(sigmarion_cmdmap),
		KB_JP },
	{	&platid_mask_MACH_NEC_MCR_SIGMARION2,
		sigmarion_jp_keytrans,
		NULL,
		CMDMAP(sigmarion_cmdmap),
		KB_JP },
	{	&platid_mask_MACH_NEC_MCR,
		mcr_jp_keytrans,
		NULL,
		CMDMAP(mcr_jp_cmdmap),
		KB_JP },
	{	&platid_mask_MACH_IBM_WORKPAD_Z50,
		z50_keytrans,
		NULL,
		NULLCMDMAP,
		KB_US },
	{	&platid_mask_MACH_SHARP_TRIPAD,
		tripad_keytrans,
		NULL,
		CMDMAP(tripad_cmdmap),
		KB_US },
	{	&platid_mask_MACH_VADEM_CLIO_C,
		tripad_keytrans,
		NULL,
		CMDMAP(tripad_cmdmap),
		KB_US },
	{	&platid_mask_MACH_NEC_MCCS,
		mccs_keytrans,
		NULL,
		CMDMAP(mccs_cmdmap),
		KB_JP },
	{	&platid_mask_MACH_FUJITSU_INTERTOP,
		intertop_keytrans,
		NULL,
		NULLCMDMAP,
		KB_JP },
	{	&platid_mask_MACH_CASIO_POCKETPOSTPET,
		pocketpostpet_keytrans,
		NULL,
		NULLCMDMAP,
		KB_JP },
#endif /* hpcmips */
#ifdef hpcsh
	/*
	 * HP Jornada 680/690
	 */
	/* Japanese */
	{	&platid_mask_MACH_HP_JORNADA_680JP,
		jornada6x0_jp_keytrans,
		jornada6x0_special_keymap,
		NULLCMDMAP,
		KB_JP },
	{	&platid_mask_MACH_HP_JORNADA_690JP,
		jornada6x0_jp_keytrans,
		jornada6x0_special_keymap,
		NULLCMDMAP,
		KB_JP },
	/* US (ABA), UK (ABU) */
	{	&platid_mask_MACH_HP_JORNADA_680,
		jornada6x0_us_keytrans,
		jornada6x0_special_keymap,
		CMDMAP(jornada_us_keydesc),
		KB_US },
	{	&platid_mask_MACH_HP_JORNADA_690,
		jornada6x0_us_keytrans,
		jornada6x0_special_keymap,
		CMDMAP(jornada_us_keydesc),
		KB_US },
	/* European English (ABB) */
	{	&platid_mask_MACH_HP_JORNADA_680EU,
		jornada6x0_intl_keytrans,
		jornada6x0_special_keymap,
		CMDMAP(jornada_intl_keydesc),
		KB_US },
	{	&platid_mask_MACH_HP_JORNADA_690EU,
		jornada6x0_intl_keytrans,
		jornada6x0_special_keymap,
		CMDMAP(jornada_intl_keydesc),
		KB_US },
	/* German (ABD) */
	{	&platid_mask_MACH_HP_JORNADA_680DE,
		jornada6x0_intl_keytrans,
		jornada6x0_special_keymap,
		CMDMAP(jornada_de_keydesc),
		KB_DE },
	{	&platid_mask_MACH_HP_JORNADA_690DE,
		jornada6x0_intl_keytrans,
		jornada6x0_special_keymap,
 		CMDMAP(jornada_de_keydesc),
		KB_DE },
	/* French (ABF) */
	{	&platid_mask_MACH_HP_JORNADA_680FR,
		jornada6x0_intl_keytrans,
		jornada6x0_special_keymap,
		CMDMAP(jornada_fr_keydesc),
		KB_FR },
	{	&platid_mask_MACH_HP_JORNADA_690FR,
		jornada6x0_intl_keytrans,
		jornada6x0_special_keymap,
 		CMDMAP(jornada_fr_keydesc),
		KB_FR },
	/* Scandinavian */
	{	&platid_mask_MACH_HP_JORNADA_680SV,
		jornada6x0_intl_keytrans,
		jornada6x0_special_keymap,
		CMDMAP(jornada_scnv_keydesc),
		KB_US },
	{	&platid_mask_MACH_HP_JORNADA_690SV,
		jornada6x0_intl_keytrans,
		jornada6x0_special_keymap,
 		CMDMAP(jornada_scnv_keydesc),
		KB_US },
	/* Spanish (ABE) */
	{	&platid_mask_MACH_HP_JORNADA_680ES,
		jornada6x0_intl_keytrans,
		jornada6x0_special_keymap,
		CMDMAP(jornada_es_keydesc),
		KB_ES },
	{	&platid_mask_MACH_HP_JORNADA_690ES,
		jornada6x0_intl_keytrans,
		jornada6x0_special_keymap,
 		CMDMAP(jornada_es_keydesc),
		KB_ES },
	/*
	 * HP 620LX
	 */
	/* Japanese */
	{	&platid_mask_MACH_HP_LX_620JP,
		hp620lx_jp_keytrans,
		hp620lx_special_keymap,
		NULLCMDMAP,
		KB_JP },
	/* Other models */
	{	&platid_mask_MACH_HP_LX_620,
		hp620lx_intl_keytrans,
		hp620lx_special_keymap,
		NULLCMDMAP,
		KB_US },

	/*
	 * PERSONA HPW50PAD
	 */
	/* Japanese */
	{ 	&platid_mask_MACH_HITACHI_PERSONA_HPW50PAD,
		persona_hpw50pad_jp_keytrans,
		persona_hpw50pad_special_keymap,
 		CMDMAP(persona_hpw50pad_jp_keydesc),
		KB_JP },

	/*
	 * PERSONA HPW200EC
	 */
	/* US */
	{ 	&platid_mask_MACH_HITACHI_PERSONA_HPW200EC,
		persona_hpw200ec_keytrans,
		persona_hpw200ec_special_keymap,
		NULLCMDMAP,
		KB_US },

#endif /* hpcsh */
#ifdef hpcarm
	/*
	 * HP Jornada 710/720/728
	 */
	/* US (ABA), UK (ABU) */
	{	&platid_mask_MACH_HP_JORNADA_720,
		jornada7xx_us_keytrans,
		jornada7xx_special_keymap,
		CMDMAP(jornada_us_keydesc),
		KB_US },
	/* Japanese */
	{	&platid_mask_MACH_HP_JORNADA_720JP,
		jornada7xx_jp_keytrans,
		jornada7xx_special_keymap,
		NULLCMDMAP,
		KB_JP },
	/* European English (ABB) */
	{	&platid_mask_MACH_HP_JORNADA_720EU,
		jornada7xx_intl_keytrans,
		jornada7xx_special_keymap,
		CMDMAP(jornada_intl_keydesc),
		KB_US },
	/* German (ABD) */
	{	&platid_mask_MACH_HP_JORNADA_720DE,
		jornada7xx_intl_keytrans,
		jornada7xx_special_keymap,
		CMDMAP(jornada_de_keydesc),
		KB_DE },
	/* French (ABF) */
	{	&platid_mask_MACH_HP_JORNADA_720FR,
		jornada7xx_intl_keytrans,
		jornada7xx_special_keymap,
		CMDMAP(jornada_fr_keydesc),
		KB_FR },
	/* Scandinavian */
	{	&platid_mask_MACH_HP_JORNADA_720SV,
		jornada7xx_intl_keytrans,
		jornada7xx_special_keymap,
		CMDMAP(jornada_scnv_keydesc),
		KB_US },
	/* Spanish (ABE) */
	{	&platid_mask_MACH_HP_JORNADA_720ES,
		jornada7xx_intl_keytrans,
		jornada7xx_special_keymap,
		CMDMAP(jornada_es_keydesc),
		KB_ES },
	/*
	 * Sharp W-ZERO3
	 */
	/* WS003SH */
	{	&platid_mask_MACH_SHARP_WZERO3_WS003SH,
		ws003sh_jp_keytrans,
		ws003sh_special_keymap,
		CMDMAP(ws003sh_jp_keydesc),
		KB_JP },
	/* WS004SH */
	{	&platid_mask_MACH_SHARP_WZERO3_WS004SH,
		ws003sh_jp_keytrans,
		ws003sh_special_keymap,
		CMDMAP(ws003sh_jp_keydesc),
		KB_JP },
	/* WS007SH */
	{	&platid_mask_MACH_SHARP_WZERO3_WS007SH,
		ws003sh_jp_keytrans,
		ws003sh_special_keymap,
		CMDMAP(ws003sh_jp_keydesc),
		KB_JP },
	/* WS011SH */
	{	&platid_mask_MACH_SHARP_WZERO3_WS011SH,
		ws011sh_jp_keytrans,
		ws011sh_special_keymap,
		CMDMAP(ws011sh_jp_keydesc),
		KB_JP },
	/* WS020SH */
	{	&platid_mask_MACH_SHARP_WZERO3_WS020SH,
		ws020sh_jp_keytrans,
		ws020sh_special_keymap,
		CMDMAP(ws020sh_jp_keydesc),
		KB_JP },
	/* NETBOOK PRO */
	{	&platid_mask_MACH_PSIONTEKLOGIX_NETBOOK_PRO,
		netbookpro_keytrans,
		NULL,
		CMDMAP(netbookpro_keydesc),
		KB_US },
#endif /* hpcarm */

	{ .ht_platform = NULL } /* end mark */
};
