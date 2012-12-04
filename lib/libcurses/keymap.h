/*   $NetBSD: keymap.h,v 1.4 2012/04/21 12:27:28 roy Exp $ */

/*
 * Copyright (c) 2005 The NetBSD Foundation Inc.
 * All rights reserved.
 *
 * This code is derived from code donated to the NetBSD Foundation
 * by Ruibiao Qiu <ruibiao@arl.wustl.edu,ruibiao@gmail.com>.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _KEYMAP_H_
#define _KEYMAP_H_

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: keymap.h,v 1.4 2012/04/21 12:27:28 roy Exp $");
#endif                          /* not lint */

/* keymap related stuff */
/*
 * moved here by Ruibiao Qiu
 * because it is needed by both getch() and get_wch()
 *
 * Keyboard input handler.  Do this by snarfing
 * all the info we can out of the terminfo entry for TERM and putting it
 * into a set of keymaps.  A keymap is an array the size of all the possible
 * single characters we can get, the contents of the array is a structure
 * that contains the type of entry this character is (i.e. part/end of a
 * multi-char sequence or a plain char) and either a pointer which will point
 * to another keymap (in the case of a multi-char sequence) OR the data value
 * that this key should return.
 *
 */

/* private data structures for holding the key definitions */
typedef struct key_entry key_entry_t;

struct key_entry {
	short   type;		/* type of key this is */
	bool    enable;         /* true if the key is active */
	union {
		keymap_t *next;	/* next keymap is key is multi-key sequence */
		wchar_t   symbol;	/* key symbol if key is a leaf entry */
	} value;
};
/* Types of key structures we can have */
#define KEYMAP_MULTI  1		/* part of a multi char sequence */
#define KEYMAP_LEAF   2		/* key has a symbol associated with it, either
				 * it is the end of a multi-char sequence or a
				 * single char key that generates a symbol */

/* allocate this many key_entry structs at once to speed start up must
 * be a power of 2.
 */
#define KEYMAP_ALLOC_CHUNK 4

/* The max number of different chars we can receive */
#define MAX_CHAR 256

/*
 * Unused mapping flag.
 */
#define MAPPING_UNUSED (0 - MAX_CHAR) /* never been used */

struct keymap {
	int	count;	       /* count of number of key structs allocated */
	short	mapping[MAX_CHAR]; /* mapping of key to allocated structs */
	key_entry_t **key;     /* dynamic array of keys */
};

#define INC_POINTER(ptr)  do {	\
	(ptr)++;		\
	(ptr) %= INBUF_SZ;	\
} while(/*CONSTCOND*/0)

#define INKEY_NORM	   0 /* no key backlog to process */
#define INKEY_ASSEMBLING   1 /* assembling a multi-key sequence */
#define INKEY_BACKOUT	   2 /* recovering from an unrecognised key */
#define INKEY_TIMEOUT	   3 /* multi-key sequence timeout */
#ifdef HAVE_WCHAR
#define INKEY_WCASSEMBLING 4 /* assembling a wide char sequence */
#endif /* HAVE_WCHAR */

/* The terminfo data we are interested in and the symbols they map to */
struct tcdata {
	int code;		/* code of the terminfo entry */
	wchar_t	symbol;		/* the symbol associated with it */
};

#endif /* _KEYMAP_H_ */
