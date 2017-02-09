/*	$NetBSD: wskbdmap_sun.c,v 1.17 2010/01/14 02:18:59 macallan Exp $	*/
/*	$OpenBSD: sunkbd.c,v 1.9 2002/09/08 23:22:00 miod Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wskbdmap_sun.c,v 1.17 2010/01/14 02:18:59 macallan Exp $");

#include <sys/types.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include "opt_sunkbd.h"

#define KC(n) KS_KEYCODE(n)

static const keysym_t wssun_keydesctab_us [] = {
/*  pos      command			normal		shifted */
    KC(0x01), KS_Cmd,
    KC(0x02), KS_Cmd_VolumeDown,
    KC(0x03),				KS_Again,
    KC(0x04), KS_Cmd_VolumeUp,
    KC(0x05), KS_Cmd_Screen0,				KS_f1,
    KC(0x06), KS_Cmd_Screen1,				KS_f2,
    KC(0x07), KS_Cmd_Screen9,				KS_f10,
    KC(0x08), KS_Cmd_Screen2,				KS_f3,
    KC(0x09),				KS_f11,
    KC(0x0a), KS_Cmd_Screen3,				KS_f4,
    KC(0x0b),				KS_f12,
    KC(0x0c), KS_Cmd_Screen4,				KS_f5,
    KC(0x0d),				KS_Alt_R,
    KC(0x0e), KS_Cmd_Screen5,				KS_f6,
    KC(0x10), KS_Cmd_Screen6,				KS_f7,
    KC(0x11), KS_Cmd_Screen7,				KS_f8,
    KC(0x12), KS_Cmd_Screen8,				KS_f9,
#ifdef SPARCBOOK_CMD
    KC(0x13), KS_Cmd,			KS_Alt_L,
#else
    KC(0x13),				KS_Alt_L,
#endif
    KC(0x14), KS_Cmd_ScrollSlowUp,	KS_Up,
    KC(0x15),				KS_Pause,
    KC(0x16),				KS_Print_Screen,
    KC(0x17),				KS_Hold_Screen,
    KC(0x18),				KS_Left,
    KC(0x19),				KS_Props,
    KC(0x1a),				KS_Undo,
    KC(0x1b), KS_Cmd_ScrollSlowDown,	KS_Down,
    KC(0x1c),				KS_Right,
    KC(0x1d),				KS_Escape,
    KC(0x1e),				KS_1,		KS_exclam,
    KC(0x1f),				KS_2,		KS_at,
    KC(0x20),				KS_3,		KS_numbersign,
    KC(0x21),				KS_4,		KS_dollar,
    KC(0x22),				KS_5,		KS_percent,
    KC(0x23),				KS_6,		KS_asciicircum,
    KC(0x24),				KS_7,		KS_ampersand,
    KC(0x25),				KS_8,		KS_asterisk,
    KC(0x26),				KS_9,		KS_parenleft,
    KC(0x27),				KS_0,		KS_parenright,
    KC(0x28),				KS_minus,	KS_underscore,
    KC(0x29),				KS_equal,	KS_plus,
    KC(0x2a),				KS_grave,	KS_asciitilde,
    KC(0x2b),				KS_Delete,
    KC(0x2c),				KS_Insert,
    KC(0x2d),				KS_KP_Equal,
    KC(0x2e),				KS_KP_Divide,
    KC(0x2f),				KS_KP_Multiply,
    KC(0x30),				KS_Power,
    KC(0x31),				KS_Front,
    KC(0x32),				KS_KP_Delete,	KS_KP_Decimal,
    KC(0x33),				KS_Copy,
    KC(0x34),				KS_Home,
    KC(0x35),				KS_Tab,
    KC(0x36),				KS_q,
    KC(0x37),				KS_w,
    KC(0x38),				KS_e,
    KC(0x39),				KS_r,
    KC(0x3a),				KS_t,
    KC(0x3b),				KS_y,
    KC(0x3c),				KS_u,
    KC(0x3d),				KS_i,
    KC(0x3e),				KS_o,
    KC(0x3f),				KS_p,
    KC(0x40),				KS_bracketleft,	KS_braceleft,
    KC(0x41),				KS_bracketright,KS_braceright,
    KC(0x42),				KS_Delete,
    KC(0x43),				KS_Multi_key,
    KC(0x44),				KS_KP_Home,	KS_KP_7,
    KC(0x45), KS_Cmd_ScrollSlowUp,	KS_KP_Up,	KS_KP_8,
    KC(0x46), KS_Cmd_ScrollFastUp,	KS_KP_Prior,	KS_KP_9,
    KC(0x47),				KS_KP_Subtract,
    KC(0x48),				KS_Open,
    KC(0x49),				KS_Paste,
    KC(0x4a),				KS_End,
    KC(0x4c),				KS_Control_L,
    KC(0x4d), KS_Cmd_Debugger,		KS_a,
    KC(0x4e),				KS_s,
    KC(0x4f),				KS_d,
    KC(0x50),				KS_f,
    KC(0x51),				KS_g,
    KC(0x52),				KS_h,
    KC(0x53),				KS_j,
    KC(0x54),				KS_k,
    KC(0x55),				KS_l,
    KC(0x56),				KS_semicolon,	KS_colon,
    KC(0x57),				KS_apostrophe,	KS_quotedbl,
    KC(0x58),				KS_backslash,	KS_bar,
    KC(0x59),				KS_Return,
    KC(0x5a),				KS_KP_Enter,
    KC(0x5b),				KS_KP_Left,	KS_KP_4,
    KC(0x5c),				KS_KP_Begin,	KS_KP_5,
    KC(0x5d),				KS_KP_Right,	KS_KP_6,
    KC(0x5e),				KS_KP_Insert,	KS_KP_0,
    KC(0x5f),				KS_Find,
    KC(0x60), KS_Cmd_ScrollFastUp,	KS_Prior,
    KC(0x61),				KS_Cut,
    KC(0x62),				KS_Num_Lock,
    KC(0x63),				KS_Shift_L,
    KC(0x64),				KS_z,
    KC(0x65),				KS_x,
    KC(0x66),				KS_c,
    KC(0x67),				KS_v,
    KC(0x68),				KS_b,
    KC(0x69),				KS_n,
    KC(0x6a),				KS_m,
    KC(0x6b),				KS_comma,	KS_less,
    KC(0x6c),				KS_period,	KS_greater,
    KC(0x6d),				KS_slash,	KS_question,
    KC(0x6e),				KS_Shift_R,
    KC(0x6f),				KS_Linefeed,
    KC(0x70),				KS_KP_End,	KS_KP_1,
    KC(0x71), KS_Cmd_ScrollSlowDown,	KS_KP_Down,	KS_KP_2,
    KC(0x72), KS_Cmd_ScrollFastDown,	KS_KP_Next,	KS_KP_3,
    KC(0x76),				KS_Help,
    KC(0x77),				KS_Caps_Lock,
    KC(0x78),				KS_Meta_L,
    KC(0x79),				KS_space,
    KC(0x7a),				KS_Meta_R,
    KC(0x7b), KS_Cmd_ScrollFastDown,	KS_Next,
    KC(0x7d),				KS_KP_Add,
};

const keysym_t wssun_keydesctab_sv[] = {
    KC(0x0d),		KS_Multi_key,
    KC(0x0f),		KS_asciitilde,	KS_asciicircum,
    KC(0x1f),		KS_2,		KS_quotedbl,	KS_at,
    KC(0x20),		KS_3,		KS_numbersign,	KS_sterling,
    KC(0x21),		KS_4,		KS_currency,	KS_dollar,
    KC(0x23),		KS_6,		KS_ampersand,
    KC(0x24),		KS_7,		KS_slash,	KS_braceleft,
    KC(0x25),		KS_8,		KS_parenleft,	KS_bracketleft,
    KC(0x26),		KS_9,		KS_parenright,	KS_bracketright,
    KC(0x27),		KS_0,		KS_equal,	KS_braceright,
    KC(0x28),		KS_plus,	KS_question,	KS_backslash,
    KC(0x29),		KS_dead_acute,	KS_dead_grave,
    KC(0x2a),		KS_apostrophe,	KS_asterisk,	KS_grave,
    KC(0x40),		KS_aring,
    KC(0x41),		KS_dead_diaeresis,KS_dead_circumflex,KS_dead_tilde,
    KC(0x43),		KS_Mode_switch,
    KC(0x4c),		KS_Caps_Lock,
    KC(0x56),		KS_odiaeresis,
    KC(0x57),		KS_adiaeresis,
    KC(0x58),		KS_paragraph,	KS_onehalf,
    KC(0x6b),		KS_comma,	KS_semicolon,
    KC(0x6c),		KS_period,	KS_colon,
    KC(0x6d),		KS_minus,	KS_underscore,
    KC(0x77),		KS_Control_L,
    KC(0x7c),		KS_less,	KS_greater,	KS_bar,
};

const keysym_t wssun_keydesctab_sv_nodead[] = {
    KC(0x29),		KS_apostrophe,	KS_grave,
    KC(0x41),		KS_diaeresis,	KS_asciicircum,	KS_asciitilde,
};

const keysym_t wssun_keydesctab_de[] = {
    KC(0x0d),		KS_Mode_switch,	/* Alt Graph (Alt R code) */
    KC(0x1f),		KS_2,		KS_quotedbl,	KS_twosuperior,
    KC(0x20),		KS_3,		KS_paragraph,	KS_threesuperior,
    KC(0x21),		KS_4,		KS_dollar,
    KC(0x22),		KS_5,		KS_percent,
    KC(0x23),		KS_6,		KS_ampersand,
    KC(0x24),		KS_7,		KS_slash,	KS_braceleft,
    KC(0x25),		KS_8,		KS_parenleft,	KS_bracketleft,
    KC(0x26),		KS_9,		KS_parenright,	KS_bracketright,
    KC(0x27),		KS_0,		KS_equal,	KS_braceright,
    KC(0x28),		KS_ssharp,	KS_question,	KS_backslash,
    KC(0x29),		KS_dead_acute,	KS_dead_grave,
    KC(0x2a),		KS_dead_circumflex, KS_dead_abovering,
    KC(0x36),		KS_q,		KS_Q,		KS_at,
    KC(0x3b),		KS_z,
    KC(0x40),		KS_udiaeresis,
    /* KC(0x43) - Compose - could be used for mode switch too */
    KC(0x41),		KS_plus,	KS_asterisk,	KS_dead_tilde,
    KC(0x56),		KS_odiaeresis,
    KC(0x57),		KS_adiaeresis,
    KC(0x58),		KS_numbersign,	KS_apostrophe,	KS_acute,
    KC(0x64),		KS_y,
    KC(0x6a),		KS_m,		KS_M,		KS_mu,
    KC(0x6b),		KS_comma,	KS_semicolon,
    KC(0x6c),		KS_period,	KS_colon,
    KC(0x6d),		KS_minus,	KS_underscore,
    KC(0x7c),		KS_less,	KS_greater,	KS_bar,
};

const keysym_t wssun_keydesctab_de_nodead[] = {
    KC(0x29),		KS_apostrophe,	KS_grave,
    KC(0x2a),		KS_asciicircum,	KS_degree,
    KC(0x41),		KS_plus,	KS_asterisk,	KS_asciitilde,
};

const keysym_t wssun_keydesctab_uk[] = {
    KC(0x1f),		KS_2,		KS_quotedbl,
    KC(0x20),		KS_3,		KS_sterling,
    KC(0x21),		KS_4,		KS_dollar,	KS_currency,
    KC(0x2a),		KS_grave,	KS_notsign,
    KC(0x57),		KS_apostrophe,	KS_at,
    KC(0x58),		KS_numbersign,	KS_asciitilde,
    KC(0x7c),		KS_backslash,	KS_bar,
};

static const keysym_t wssun_keydesctab_us_dvorak[] = {
/*  pos      command			normal		shifted */
    KC(0x28),				KS_bracketleft,	KS_braceleft,
    KC(0x29),				KS_bracketright,KS_braceright,
    KC(0x36),				KS_apostrophe,	KS_quotedbl,
    KC(0x37),				KS_comma,	KS_less,
    KC(0x38),				KS_period,	KS_greater,
    KC(0x39),				KS_p,
    KC(0x3a),				KS_y,
    KC(0x3b),				KS_f,
    KC(0x3c),				KS_g,
    KC(0x3d),				KS_c,
    KC(0x3e),				KS_r,
    KC(0x3f),				KS_l,
    KC(0x40),				KS_slash,	KS_question,
    KC(0x41),				KS_equal,	KS_plus,
    KC(0x4d), KS_Cmd_Debugger,		KS_a,
    KC(0x4e),				KS_o,
    KC(0x4f),				KS_e,
    KC(0x50),				KS_u,
    KC(0x51),				KS_i,
    KC(0x52),				KS_d,
    KC(0x53),				KS_h,
    KC(0x54),				KS_t,
    KC(0x55),				KS_n,
    KC(0x56),				KS_s,
    KC(0x57),				KS_minus,	KS_underscore,
    KC(0x64),				KS_semicolon,	KS_colon,
    KC(0x65),				KS_q,
    KC(0x66),				KS_j,
    KC(0x67),				KS_k,
    KC(0x68),				KS_x,
    KC(0x69),				KS_b,
    KC(0x6a),				KS_m,
    KC(0x6b),				KS_w,
    KC(0x6c),				KS_v,
    KC(0x6d),				KS_z,
};

static const keysym_t wssun_keydesctab_us_colemak [] = {
/*  pos      command			normal		shifted */
    KC(0x36),				KS_q,
    KC(0x37),				KS_w,
    KC(0x38),				KS_f,
    KC(0x39),				KS_p,
    KC(0x3a),				KS_g,
    KC(0x3b),				KS_j,
    KC(0x3c),				KS_l,
    KC(0x3d),				KS_u,
    KC(0x3e),				KS_y,
    KC(0x3f),				KS_semicolon,	KS_colon,
    KC(0x4d), KS_Cmd_Debugger,		KS_a,
    KC(0x4e),				KS_r,
    KC(0x4f),				KS_s,
    KC(0x50),				KS_t,
    KC(0x51),				KS_d,
    KC(0x52),				KS_h,
    KC(0x53),				KS_n,
    KC(0x54),				KS_e,
    KC(0x55),				KS_i,
    KC(0x56),				KS_o,
    KC(0x64),				KS_z,
    KC(0x65),				KS_x,
    KC(0x66),				KS_c,
    KC(0x67),				KS_v,
    KC(0x68),				KS_b,
    KC(0x69),				KS_k,
    KC(0x6a),				KS_m,
    KC(0x77),				KS_Delete,
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }
/* KBD_NULLMAP generates a entry for machine native variant.
   the entry will be modified by machine dependent keyboard driver. */
#define KBD_NULLMAP(name, base) { name, base, 0, 0 }

const struct wscons_keydesc wssun_keydesctab[] = {
	KBD_MAP(KB_US,			0,	wssun_keydesctab_us),
	KBD_MAP(KB_SV,			KB_US,	wssun_keydesctab_sv),
	KBD_MAP(KB_SV | KB_NODEAD,	KB_SV,	wssun_keydesctab_sv_nodead),
	KBD_MAP(KB_DE,			KB_US,	wssun_keydesctab_de),
	KBD_MAP(KB_DE | KB_NODEAD,	KB_DE,	wssun_keydesctab_de_nodead),
	KBD_MAP(KB_UK,			KB_US,	wssun_keydesctab_uk),
	KBD_MAP(KB_US | KB_DVORAK,	KB_US,	wssun_keydesctab_us_dvorak),
	KBD_MAP(KB_US | KB_COLEMAK,	KB_US,	wssun_keydesctab_us_colemak),
	{ 0, 0, 0, 0 }
};
