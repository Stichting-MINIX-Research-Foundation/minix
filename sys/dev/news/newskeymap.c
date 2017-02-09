/*	$NetBSD: newskeymap.c,v 1.2 2001/11/13 07:30:37 lukem Exp $	*/

/*-
 * Copyright (c) 2000 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: newskeymap.c,v 1.2 2001/11/13 07:30:37 lukem Exp $");

#include <sys/types.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#define KC(n) KS_KEYCODE(n)

const keysym_t newskb_keydesc_jp[] = {
/*  pos		command			normal		shifted */
    KC(1),				KS_F1,
    KC(2),				KS_F2,
    KC(3),				KS_F3,
    KC(4),				KS_F4,
    KC(5),				KS_F5,
    KC(6),				KS_F6,
    KC(7),				KS_F7,
    KC(8),				KS_F8,
    KC(9),				KS_F9,
    KC(10),				KS_F10,

    KC(11),	KS_Cmd_Debugger,	KS_Escape,
    KC(12),				KS_1,		KS_exclam,
    KC(13),				KS_2,		KS_at,
    KC(14),				KS_3,		KS_numbersign,
    KC(15),				KS_4,		KS_dollar,
    KC(16),				KS_5,		KS_percent,
    KC(17),				KS_6,		KS_asciicircum,
    KC(18),				KS_7,		KS_ampersand,
    KC(19),				KS_8,		KS_asterisk,
    KC(20),				KS_9,		KS_parenleft,
    KC(21),				KS_0,		KS_parenright,
    KC(22),				KS_minus,	KS_underscore,
    KC(23),				KS_equal,	KS_plus,
    KC(24),				KS_backslash,	KS_bar,
    KC(25),				KS_BackSpace,

    KC(26),				KS_Tab,
    KC(27),				KS_q,
    KC(28),				KS_w,
    KC(29),				KS_e,
    KC(30),				KS_r,
    KC(31),				KS_t,
    KC(32),				KS_y,
    KC(33),				KS_u,
    KC(34),				KS_i,
    KC(35),				KS_o,
    KC(36),				KS_p,
    KC(37),				KS_bracketleft,	KS_braceleft,
    KC(38),				KS_bracketright, KS_braceright,
    KC(39),	KS_Cmd_ResetEmul,	KS_Delete,

    KC(40),	KS_Cmd1,		KS_Control_L,
    KC(41),				KS_a,
    KC(42),				KS_s,
    KC(43),				KS_d,
    KC(44),				KS_f,
    KC(45),				KS_g,
    KC(46),				KS_h,
    KC(47),				KS_j,
    KC(48),				KS_k,
    KC(49),				KS_l,
    KC(50),				KS_semicolon,	KS_colon,
    KC(51),				KS_apostrophe,	KS_quotedbl,
    KC(52),				KS_grave,	KS_asciitilde,
    KC(53),				KS_Return,

    KC(54),				KS_Shift_L,
    KC(55),				KS_z,
    KC(56),				KS_x,
    KC(57),				KS_c,
    KC(58),				KS_v,
    KC(59),				KS_b,
    KC(60),				KS_n,
    KC(61),				KS_m,
    KC(62),				KS_comma,	KS_less,
    KC(63),				KS_period,	KS_greater,
    KC(64),				KS_slash,	KS_question,
    KC(65),
    KC(66),				KS_Shift_R,
    KC(67),	KS_Cmd2,		KS_Alt_L,
    KC(68),				KS_Caps_Lock,
    KC(69),	KS_Muhenkan,
    KC(70),				KS_space,
    KC(71),	KS_Henkan,
    KC(72),	/* eisu */
    KC(73),	/* kana */
    KC(74),	KS_Execute,

    KC(75),	KS_KP_7,
    KC(76),	KS_KP_8,
    KC(77),	KS_KP_9,
    KC(78),	KS_KP_Subtract,
    KC(79),	KS_KP_4,
    KC(80),	KS_KP_5,
    KC(81),	KS_KP_6,
    KC(82),	KS_KP_Add,
    KC(83),	KS_KP_1,
    KC(84),	KS_KP_2,
    KC(85),	KS_KP_3,
    KC(86),	KS_KP_Separator,
    KC(87),	KS_KP_0,
    KC(88),	KS_KP_Up,
    KC(89),	KS_KP_Decimal,
    KC(90),	KS_KP_Enter,
    KC(91),	KS_KP_Left,
    KC(92),	KS_KP_Down,
    KC(93),	KS_KP_Right,

    KC(100),	KS_KP_Multiply,
    KC(101),	KS_KP_Divide,
    KC(102),	KS_KP_Tab,
    KC(104),	KS_F11,
    KC(105),	KS_F12,
    KC(106),	KS_Help,
    KC(107),	KS_Insert,
    KC(108),	KS_Clear,
    KC(109),	KS_Prior,
    KC(110),	KS_Next,

    KC(122),	/* power on */
};

#define KBD_MAP(name, base, map) \
	{ name, base, sizeof(map) / sizeof(keysym_t), map }

const struct wscons_keydesc newskb_keydesctab[] = {
	KBD_MAP(KB_JP,	0,	newskb_keydesc_jp),
	{ 0 }
};
