/*	$NetBSD: keyname.c,v 1.6 2008/04/28 20:23:01 martin Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julian Coleman.
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
#ifndef lint
__RCSID("$NetBSD: keyname.c,v 1.6 2008/04/28 20:23:01 martin Exp $");
#endif				/* not lint */

#include <stdlib.h>
#include <string.h>

#include "curses.h"
#include "curses_private.h"

#define KEYNAMEMAX (size_t) 14	/* "KEY_BACKSPACE\0" */
static char name[KEYNAMEMAX + 1];

/*
 * keyname --
 *	Return name of key or NULL;
 */
char *
keyname(int key)
{
/* We don't bother with the large keyname table if SMALL is defined. */
#ifdef SMALL
	strcpy(name, "-1\0");
	return name;
#else
	if (key < 0) {
		strcpy(name, "-1\0");
		return name;
	}

	/* No name. */
	if (key == 0x100) {
		strcpy(name, "-1\0");
		return name;
	}

	/* Control codes */
	if (key < 0x20) {
		name[0] = '^';
		name[1] = (char) (key + 64);	/* Offset of '@' */
		name[2] = '\0';
		return name;
	}

	/* "Normal" keys */
	if (key < 0x7F) {
		name[0] = (char) key;
		name[1] = '\0';
		return name;
	}

	/* Delete key */
	if (key == 0x7F) {
		strcpy(name, "^?\0");
		return name;
	}

	/* Meta + control codes */
	if (key < 0x9F) {
		strcpy(name, "M-^"); 
		name[3] = (char) (key - 64);	/* Offset of '@' */
		name[4] = '\0';
		return name;
	}

	/* Meta + "normal" keys */
	if (key < 0xFF) {
		strcpy (name, "M-");
		name[2] = (char) (key - 128);
		name[3] = '\0';
		return name;
	}

	/* Meta + delete key */
	if (key == 0xFF) {
		strcpy(name, "M-^?\0");
		return name;
	}

	/* Key names.  Synchronise this with curses.h. */
	if (key == 0x101) {
		strncpy(name, "KEY_BREAK\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x102) {
		strncpy(name, "KEY_DOWN\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x103) {
		strncpy(name, "KEY_UP\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x104) {
		strncpy(name, "KEY_LEFT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x105) {
		strncpy(name, "KEY_RIGHT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x106) {
		strncpy(name, "KEY_HOME\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x107) {
		strncpy(name, "KEY_BACKSPACE\0", KEYNAMEMAX);
		return name;
	}
	/* Function key block (64 keys). */
	if (key < 0x148) {
		int i;

		strcpy(name, "KEY_F(");
		i = snprintf(&name[6], (size_t) 3, "%d", key - 0x108);
		name[6 + i] = ')';
		name[7 + i] = '\0';
		return name;
	}
	if (key == 0x148) {
		strncpy(name, "KEY_DL\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x149) {
		strncpy(name, "KEY_IL\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x14A) {
		strncpy(name, "KEY_DC\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x14B) {
		strncpy(name, "KEY_IC\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x14C) {
		strncpy(name, "KEY_EIC\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x14D) {
		strncpy(name, "KEY_CLEAR\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x14E) {
		strncpy(name, "KEY_EOS\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x14F) {
		strncpy(name, "KEY_EOL\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x150) {
		strncpy(name, "KEY_SF\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x151) {
		strncpy(name, "KEY_SR\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x152) {
		strncpy(name, "KEY_NPAGE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x153) {
		strncpy(name, "KEY_PPAGE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x154) {
		strncpy(name, "KEY_STAB\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x155) {
		strncpy(name, "KEY_CTAB\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x156) {
		strncpy(name, "KEY_CATAB\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x157) {
		strncpy(name, "KEY_ENTER\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x158) {
		strncpy(name, "KEY_SRESET\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x159) {
		strncpy(name, "KEY_RESET\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x15A) {
		strncpy(name, "KEY_PRINT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x15B) {
		strncpy(name, "KEY_LL\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x15C) {
		strncpy(name, "KEY_A1\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x15D) {
		strncpy(name, "KEY_A3\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x15E) {
		strncpy(name, "KEY_B2\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x15F) {
		strncpy(name, "KEY_C1\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x160) {
		strncpy(name, "KEY_C3\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x161) {
		strncpy(name, "KEY_BTAB\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x162) {
		strncpy(name, "KEY_BEG\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x163) {
		strncpy(name, "KEY_CANCEL\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x164) {
		strncpy(name, "KEY_CLOSE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x165) {
		strncpy(name, "KEY_COMMAND\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x166) {
		strncpy(name, "KEY_COPY\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x167) {
		strncpy(name, "KEY_CREATE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x168) {
		strncpy(name, "KEY_END\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x169) {
		strncpy(name, "KEY_EXIT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x16A) {
		strncpy(name, "KEY_FIND\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x16B) {
		strncpy(name, "KEY_HELP\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x16C) {
		strncpy(name, "KEY_MARK\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x16D) {
		strncpy(name, "KEY_MESSAGE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x16E) {
		strncpy(name, "KEY_MOVE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x16F) {
		strncpy(name, "KEY_NEXT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x170) {
		strncpy(name, "KEY_OPEN\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x171) {
		strncpy(name, "KEY_OPTIONS\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x172) {
		strncpy(name, "KEY_PREVIOUS\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x173) {
		strncpy(name, "KEY_REDO\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x174) {
		strncpy(name, "KEY_REFERENCE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x175) {
		strncpy(name, "KEY_REFRESH\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x176) {
		strncpy(name, "KEY_REPLACE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x177) {
		strncpy(name, "KEY_RESTART\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x178) {
		strncpy(name, "KEY_RESUME\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x179) {
		strncpy(name, "KEY_SAVE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x17A) {
		strncpy(name, "KEY_SBEG\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x17B) {
		strncpy(name, "KEY_SCANCEL\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x17C) {
		strncpy(name, "KEY_SCOMMAND\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x17D) {
		strncpy(name, "KEY_SCOPY\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x17E) {
		strncpy(name, "KEY_SCREATE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x17F) {
		strncpy(name, "KEY_SDC\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x180) {
		strncpy(name, "KEY_SDL\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x181) {
		strncpy(name, "KEY_SELECT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x182) {
		strncpy(name, "KEY_SEND\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x183) {
		strncpy(name, "KEY_SEOL\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x184) {
		strncpy(name, "KEY_SEXIT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x185) {
		strncpy(name, "KEY_SFIND\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x186) {
		strncpy(name, "KEY_SHELP\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x187) {
		strncpy(name, "KEY_SHOME\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x188) {
		strncpy(name, "KEY_SIC\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x189) {
		strncpy(name, "KEY_SLEFT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x18A) {
		strncpy(name, "KEY_SMESSAGE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x18B) {
		strncpy(name, "KEY_SMOVE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x18C) {
		strncpy(name, "KEY_SNEXT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x18D) {
		strncpy(name, "KEY_SOPTIONS\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x18E) {
		strncpy(name, "KEY_SPREVIOUS\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x18F) {
		strncpy(name, "KEY_SPRINT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x190) {
		strncpy(name, "KEY_SREDO\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x191) {
		strncpy(name, "KEY_SREPLACE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x192) {
		strncpy(name, "KEY_SRIGHT\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x193) {
		strncpy(name, "KEY_SRSUME\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x194) {
		strncpy(name, "KEY_SSAVE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x195) {
		strncpy(name, "KEY_SSUSPEND\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x196) {
		strncpy(name, "KEY_SUNDO\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x197) {
		strncpy(name, "KEY_SUSPEND\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x198) {
		strncpy(name, "KEY_UNDO\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x199) {
		strncpy(name, "KEY_MOUSE\0", KEYNAMEMAX);
		return name;
	}
	if (key == 0x200) {
		strncpy(name, "KEY_RESIZE\0", KEYNAMEMAX);
		return name;
	}
	/* No more names. */
	strncpy(name, "UNKOWN KEY\0", KEYNAMEMAX);
	return name;
#endif
}
/*
 * key_name --
 *	Return name of key or NULL;
 */
char *
key_name(wchar_t key)
{
#ifndef HAVE_WCHAR
	return NULL;
#else
	(void) keyname((int) key);

	if (!strncmp(name, "M-", 2)) {
		/* Remove the "M-" */
		name[0] = name[2];
		name[1] = '\0';
	}
	return name;
#endif /* HAVE_WCHAR */
}

