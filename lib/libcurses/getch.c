/*	$NetBSD: getch.c,v 1.59 2012/04/21 12:27:28 roy Exp $	*/

/*
 * Copyright (c) 1981, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)getch.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: getch.c,v 1.59 2012/04/21 12:27:28 roy Exp $");
#endif
#endif					/* not lint */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "curses.h"
#include "curses_private.h"
#include "keymap.h"

short	state;		/* state of the inkey function */

static const struct tcdata tc[] = {
	{TICODE_kSAV, KEY_SSAVE},
	{TICODE_kSPD, KEY_SSUSPEND},
	{TICODE_kUND, KEY_SUNDO},
	{TICODE_kHLP, KEY_SHELP},
	{TICODE_kHOM, KEY_SHOME},
	{TICODE_kIC, KEY_SIC},
	{TICODE_kLFT, KEY_SLEFT},
	{TICODE_krdo, KEY_REDO},
	{TICODE_khlp, KEY_HELP},
	{TICODE_kmrk, KEY_MARK},
	{TICODE_kmsg, KEY_MESSAGE},
	{TICODE_kmov, KEY_MOVE},
	{TICODE_knxt, KEY_NEXT},
	{TICODE_kopn, KEY_OPEN},
	{TICODE_kopt, KEY_OPTIONS},
	{TICODE_kprv, KEY_PREVIOUS},
	{TICODE_kprt, KEY_PRINT},
	{TICODE_kMSG, KEY_SMESSAGE},
	{TICODE_kMOV, KEY_SMOVE},
	{TICODE_kNXT, KEY_SNEXT},
	{TICODE_kOPT, KEY_SOPTIONS},
	{TICODE_kPRV, KEY_SPREVIOUS},
	{TICODE_kPRT, KEY_SPRINT},
	{TICODE_kRDO, KEY_SREDO},
	{TICODE_kRPL, KEY_SREPLACE},
	{TICODE_kRIT, KEY_SRIGHT},
	{TICODE_kRES, KEY_SRSUME},
	{TICODE_kCAN, KEY_SCANCEL},
	{TICODE_kref, KEY_REFERENCE},
	{TICODE_krfr, KEY_REFRESH},
	{TICODE_krpl, KEY_REPLACE},
	{TICODE_krst, KEY_RESTART},
	{TICODE_kres, KEY_RESUME},
	{TICODE_ksav, KEY_SAVE},
	{TICODE_kspd, KEY_SUSPEND},
	{TICODE_kund, KEY_UNDO},
	{TICODE_kBEG, KEY_SBEG},
	{TICODE_kFND, KEY_SFIND},
	{TICODE_kCMD, KEY_SCOMMAND},
	{TICODE_kCPY, KEY_SCOPY},
	{TICODE_kCRT, KEY_SCREATE},
	{TICODE_kDC, KEY_SDC},
	{TICODE_kDL, KEY_SDL},
	{TICODE_kslt, KEY_SELECT},
	{TICODE_kEND, KEY_SEND},
	{TICODE_kEOL, KEY_SEOL},
	{TICODE_kEXT, KEY_SEXIT},
	{TICODE_kfnd, KEY_FIND},
	{TICODE_kbeg, KEY_BEG},
	{TICODE_kcan, KEY_CANCEL},
	{TICODE_kclo, KEY_CLOSE},
	{TICODE_kcmd, KEY_COMMAND},
	{TICODE_kcpy, KEY_COPY},
	{TICODE_kcrt, KEY_CREATE},
	{TICODE_kend, KEY_END},
	{TICODE_kent, KEY_ENTER},
	{TICODE_kext, KEY_EXIT},
	{TICODE_kf11, KEY_F(11)},
	{TICODE_kf12, KEY_F(12)},
	{TICODE_kf13, KEY_F(13)},
	{TICODE_kf14, KEY_F(14)},
	{TICODE_kf15, KEY_F(15)},
	{TICODE_kf16, KEY_F(16)},
	{TICODE_kf17, KEY_F(17)},
	{TICODE_kf18, KEY_F(18)},
	{TICODE_kf19, KEY_F(19)},
	{TICODE_kf20, KEY_F(20)},
	{TICODE_kf21, KEY_F(21)},
	{TICODE_kf22, KEY_F(22)},
	{TICODE_kf23, KEY_F(23)},
	{TICODE_kf24, KEY_F(24)},
	{TICODE_kf25, KEY_F(25)},
	{TICODE_kf26, KEY_F(26)},
	{TICODE_kf27, KEY_F(27)},
	{TICODE_kf28, KEY_F(28)},
	{TICODE_kf29, KEY_F(29)},
	{TICODE_kf30, KEY_F(30)},
	{TICODE_kf31, KEY_F(31)},
	{TICODE_kf32, KEY_F(32)},
	{TICODE_kf33, KEY_F(33)},
	{TICODE_kf34, KEY_F(34)},
	{TICODE_kf35, KEY_F(35)},
	{TICODE_kf36, KEY_F(36)},
	{TICODE_kf37, KEY_F(37)},
	{TICODE_kf38, KEY_F(38)},
	{TICODE_kf39, KEY_F(39)},
	{TICODE_kf40, KEY_F(40)},
	{TICODE_kf41, KEY_F(41)},
	{TICODE_kf42, KEY_F(42)},
	{TICODE_kf43, KEY_F(43)},
	{TICODE_kf44, KEY_F(44)},
	{TICODE_kf45, KEY_F(45)},
	{TICODE_kf46, KEY_F(46)},
	{TICODE_kf47, KEY_F(47)},
	{TICODE_kf48, KEY_F(48)},
	{TICODE_kf49, KEY_F(49)},
	{TICODE_kf50, KEY_F(50)},
	{TICODE_kf51, KEY_F(51)},
	{TICODE_kf52, KEY_F(52)},
	{TICODE_kf53, KEY_F(53)},
	{TICODE_kf54, KEY_F(54)},
	{TICODE_kf55, KEY_F(55)},
	{TICODE_kf56, KEY_F(56)},
	{TICODE_kf57, KEY_F(57)},
	{TICODE_kf58, KEY_F(58)},
	{TICODE_kf59, KEY_F(59)},
	{TICODE_kf60, KEY_F(60)},
	{TICODE_kf61, KEY_F(61)},
	{TICODE_kf62, KEY_F(62)},
	{TICODE_kf63, KEY_F(63)},
	{TICODE_ka1, KEY_A1},
	{TICODE_kb2, KEY_B2},
	{TICODE_ka3, KEY_A3},
	{TICODE_kc1, KEY_C1},
	{TICODE_kc3, KEY_C3},
	{TICODE_kmous, KEY_MOUSE},
	{TICODE_kf0, KEY_F0},
	{TICODE_kf1, KEY_F(1)},
	{TICODE_kf2, KEY_F(2)},
	{TICODE_kf3, KEY_F(3)},
	{TICODE_kf4, KEY_F(4)},
	{TICODE_kf5, KEY_F(5)},
	{TICODE_kf6, KEY_F(6)},
	{TICODE_kf7, KEY_F(7)},
	{TICODE_kf8, KEY_F(8)},
	{TICODE_kf9, KEY_F(9)},
	{TICODE_kf10, KEY_F(10)},
	{TICODE_kil1, KEY_IL},
	{TICODE_ktbc, KEY_CATAB},
	{TICODE_kcbt, KEY_BTAB},
	{TICODE_kbs, KEY_BACKSPACE},
	{TICODE_kclr, KEY_CLEAR},
	{TICODE_kdch1, KEY_DC},
	{TICODE_kcud1, KEY_DOWN},
	{TICODE_kel, KEY_EOL},
	{TICODE_kind, KEY_SF},
	{TICODE_kll, KEY_LL},
	{TICODE_khome, KEY_HOME},
	{TICODE_kich1, KEY_IC},
	{TICODE_kdl1, KEY_DL},
	{TICODE_kcub1, KEY_LEFT},
	{TICODE_krmir, KEY_EIC},
	{TICODE_knp, KEY_NPAGE},
	{TICODE_kpp, KEY_PPAGE},
	{TICODE_kri, KEY_SR},
	{TICODE_kcuf1, KEY_RIGHT},
	{TICODE_ked, KEY_EOS},
	{TICODE_khts, KEY_STAB},
	{TICODE_kctab, KEY_CTAB},
	{TICODE_kcuu1, KEY_UP}
};
/* Number of TC entries .... */
static const int num_tcs = (sizeof(tc) / sizeof(struct tcdata));

int	ESCDELAY = 300;		/* Delay in ms between keys for esc seq's */

/* Key buffer */
#define INBUF_SZ 16		/* size of key buffer - must be larger than
				 * longest multi-key sequence */
static wchar_t	inbuf[INBUF_SZ];
static int	start, end, working; /* pointers for manipulating inbuf data */

/* prototypes for private functions */
static void add_key_sequence(SCREEN *screen, char *sequence, int key_type);
static key_entry_t *add_new_key(keymap_t *current, char ch, int key_type,
        int symbol);
static void delete_key_sequence(keymap_t *current, int key_type);
static void do_keyok(keymap_t *current, int key_type, bool flag, int *retval);
static keymap_t *new_keymap(void); /* create a new keymap */
static key_entry_t *new_key(void); /* create a new key entry */
static wchar_t		inkey(int to, int delay);

/*
 * Free the storage associated with the given keymap
 */
void
_cursesi_free_keymap(keymap_t *map)
{
	int i;

	  /* check for, and free, child keymaps */
	for (i = 0; i < MAX_CHAR; i++) {
		if (map->mapping[i] >= 0) {
			if (map->key[map->mapping[i]]->type == KEYMAP_MULTI)
				_cursesi_free_keymap(
					map->key[map->mapping[i]]->value.next);
		}
	}

	  /* now free any allocated keymap structs */
	for (i = 0; i < map->count; i += KEYMAP_ALLOC_CHUNK) {
		free(map->key[i]);
	}

	free(map->key);
	free(map);
}


/*
 * Add a new key entry to the keymap pointed to by current.  Entry
 * contains the character to add to the keymap, type is the type of
 * entry to add (either multikey or leaf) and symbol is the symbolic
 * value for a leaf type entry.  The function returns a pointer to the
 * new keymap entry.
 */
static key_entry_t *
add_new_key(keymap_t *current, char chr, int key_type, int symbol)
{
	key_entry_t *the_key;
        int i, ki;

#ifdef DEBUG
	__CTRACE(__CTRACE_MISC,
	    "Adding character %s of type %d, symbol 0x%x\n",
	    unctrl(chr), key_type, symbol);
#endif
	if (current->mapping[(unsigned char) chr] < 0) {
		if (current->mapping[(unsigned char) chr] == MAPPING_UNUSED) {
			  /* first time for this char */
			current->mapping[(unsigned char) chr] =
				current->count;	/* map new entry */
			ki = current->count;

			  /* make sure we have room in the key array first */
			if ((current->count & (KEYMAP_ALLOC_CHUNK - 1)) == 0)
			{
				if ((current->key =
				     realloc(current->key,
					     ki * sizeof(key_entry_t *)
					     + KEYMAP_ALLOC_CHUNK * sizeof(key_entry_t *))) == NULL) {
					fprintf(stderr,
					  "Could not malloc for key entry\n");
					exit(1);
				}

				the_key = new_key();
				for (i = 0; i < KEYMAP_ALLOC_CHUNK; i++) {
					current->key[ki + i] = &the_key[i];
				}
			}
                } else {
			  /* the mapping was used but freed, reuse it */
			ki = - current->mapping[(unsigned char) chr];
			current->mapping[(unsigned char) chr] = ki;
		}

		current->count++;

		  /* point at the current key array element to use */
		the_key = current->key[ki];

		the_key->type = key_type;

		switch (key_type) {
		  case KEYMAP_MULTI:
			    /* need for next key */
#ifdef DEBUG
			  __CTRACE(__CTRACE_MISC, "Creating new keymap\n");
#endif
			  the_key->value.next = new_keymap();
			  the_key->enable = TRUE;
			  break;

		  case KEYMAP_LEAF:
				/* the associated symbol for the key */
#ifdef DEBUG
			  __CTRACE(__CTRACE_MISC, "Adding leaf key\n");
#endif
			  the_key->value.symbol = symbol;
			  the_key->enable = TRUE;
			  break;

		  default:
			  fprintf(stderr, "add_new_key: bad type passed\n");
			  exit(1);
		}
	} else {
		  /* the key is already known - just return the address. */
#ifdef DEBUG
		__CTRACE(__CTRACE_MISC, "Keymap already known\n");
#endif
		the_key = current->key[current->mapping[(unsigned char) chr]];
	}

        return the_key;
}

/*
 * Delete the given key symbol from the key mappings for the screen.
 *
 */
void
delete_key_sequence(keymap_t *current, int key_type)
{
	key_entry_t *key;
	int i;

	  /*
	   * we need to iterate over all the keys as there may be
	   * multiple instances of the leaf symbol.
	   */
	for (i = 0; i < MAX_CHAR; i++) {
		if (current->mapping[i] < 0)
			continue; /* no mapping for the key, next! */

		key = current->key[current->mapping[i]];

		if (key->type == KEYMAP_MULTI) {
			  /* have not found the leaf, recurse down */
			delete_key_sequence(key->value.next, key_type);
			  /* if we deleted the last key in the map, free */
			if (key->value.next->count == 0)
				_cursesi_free_keymap(key->value.next);
		} else if ((key->type == KEYMAP_LEAF)
			   && (key->value.symbol == key_type)) {
#ifdef DEBUG
		__CTRACE(__CTRACE_INPUT, "delete_key_sequence: found keysym %d, deleting\n",
		    key_type);
#endif
			key->enable = FALSE;
		}
	}
}

/*
 * Add the sequence of characters given in sequence as the key mapping
 * for the given key symbol.
 */
void
add_key_sequence(SCREEN *screen, char *sequence, int key_type)
{
	key_entry_t *tmp_key;
	keymap_t *current;
	int length, j, key_ent;

#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "add_key_sequence: add key sequence: %s(%s)\n",
	    sequence, keyname(key_type));
#endif /* DEBUG */
	current = screen->base_keymap;	/* always start with
					 * base keymap. */
	length = (int) strlen(sequence);

	/*
	 * OK - we really should never get a zero length string here, either
	 * the terminfo entry is there and it has a value or we are not called
	 * at all.  Unfortunately, if someone assigns a terminfo string to the
	 * ^@ value we get passed a null string which messes up our length.
	 * So, if we get a null string then just insert a leaf value in
	 * the 0th char position of the root keymap.  Note that we are
	 * totally screwed if someone terminates a multichar sequence
	 * with ^@... oh well.
	 */
	if (length == 0)
		length = 1;

	for (j = 0; j < length - 1; j++) {
		  /* add the entry to the struct */
		tmp_key = add_new_key(current, sequence[j], KEYMAP_MULTI, 0);

		  /* index into the key array - it's
		     clearer if we stash this */
		key_ent = current->mapping[(unsigned char) sequence[j]];

		current->key[key_ent] = tmp_key;

		  /* next key uses this map... */
		current = current->key[key_ent]->value.next;
	}

	/*
	 * This is the last key in the sequence (it may have been the
	 * only one but that does not matter) this means it is a leaf
	 * key and should have a symbol associated with it.
	 */
	tmp_key = add_new_key(current, sequence[length - 1], KEYMAP_LEAF,
			      key_type);
	current->key[current->mapping[(int)sequence[length - 1]]] = tmp_key;
}

/*
 * Init_getch - initialise all the pointers & structures needed to make
 * getch work in keypad mode.
 *
 */
void
__init_getch(SCREEN *screen)
{
	char entry[1024], *p;
	const char *s;
	int     i;
	size_t limit, l;
#ifdef DEBUG
	int k, length;
#endif

	/* init the inkey state variable */
	state = INKEY_NORM;

	/* init the base keymap */
	screen->base_keymap = new_keymap();

	/* key input buffer pointers */
	start = end = working = 0;

	/* now do the terminfo snarfing ... */

	for (i = 0; i < num_tcs; i++) {
		p = entry;
		limit = 1023;
		s = screen->term->strs[tc[i].code];
		if (s == NULL)
			continue;
		l = strlen(s) + 1;
		if (limit < l)
			continue;
		strlcpy(p, s, limit);
		p += l;
		limit -= l;
#ifdef DEBUG
			__CTRACE(__CTRACE_INIT,
			    "Processing terminfo entry %d, sequence ",
			    tc[i].code);
			length = (int) strlen(entry);
			for (k = 0; k <= length -1; k++)
				__CTRACE(__CTRACE_INIT, "%s", unctrl(entry[k]));
			__CTRACE(__CTRACE_INIT, "\n");
#endif
		add_key_sequence(screen, entry, tc[i].symbol);
	}
}


/*
 * new_keymap - allocates & initialises a new keymap structure.  This
 * function returns a pointer to the new keymap.
 *
 */
static keymap_t *
new_keymap(void)
{
	int     i;
	keymap_t *new_map;

	if ((new_map = malloc(sizeof(keymap_t))) == NULL) {
		perror("Inkey: Cannot allocate new keymap");
		exit(2);
	}

	/* Initialise the new map */
	new_map->count = 0;
	for (i = 0; i < MAX_CHAR; i++) {
		new_map->mapping[i] = MAPPING_UNUSED; /* no mapping for char */
	}

	/* key array will be allocated when first key is added */
	new_map->key = NULL;

	return new_map;
}

/*
 * new_key - allocates & initialises a new key entry.  This function returns
 * a pointer to the newly allocated key entry.
 *
 */
static key_entry_t *
new_key(void)
{
	key_entry_t *new_one;
	int i;

	if ((new_one = malloc(KEYMAP_ALLOC_CHUNK * sizeof(key_entry_t)))
	    == NULL) {
		perror("inkey: Cannot allocate new key entry chunk");
		exit(2);
	}

	for (i = 0; i < KEYMAP_ALLOC_CHUNK; i++) {
		new_one[i].type = 0;
		new_one[i].value.next = NULL;
	}

	return new_one;
}

/*
 * inkey - do the work to process keyboard input, check for multi-key
 * sequences and return the appropriate symbol if we get a match.
 *
 */

wchar_t
inkey(int to, int delay)
{
	wchar_t		 k;
	int              c, mapping;
	keymap_t	*current = _cursesi_screen->base_keymap;
	FILE            *infd = _cursesi_screen->infd;

	k = 0;		/* XXX gcc -Wuninitialized */

#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "inkey (%d, %d)\n", to, delay);
#endif
	for (;;) {		/* loop until we get a complete key sequence */
reread:
		if (state == INKEY_NORM) {
			if (delay && __timeout(delay) == ERR)
				return ERR;
			c = fgetc(infd);
			if (c == EOF) {
				clearerr(infd);
				return ERR;
			}

			if (delay && (__notimeout() == ERR))
				return ERR;

			k = (wchar_t) c;
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "inkey (state normal) got '%s'\n", unctrl(k));
#endif

			working = start;
			inbuf[working] = k;
			INC_POINTER(working);
			end = working;
			state = INKEY_ASSEMBLING;	/* go to the assembling
							 * state now */
		} else if (state == INKEY_BACKOUT) {
			k = inbuf[working];
			INC_POINTER(working);
			if (working == end) {	/* see if we have run
						 * out of keys in the
						 * backlog */

				/* if we have then switch to assembling */
				state = INKEY_ASSEMBLING;
			}
		} else if (state == INKEY_ASSEMBLING) {
			/* assembling a key sequence */
			if (delay) {
				if (__timeout(to ? (ESCDELAY / 100) : delay)
				    == ERR)
					return ERR;
			} else {
				if (to && (__timeout(ESCDELAY / 100) == ERR))
					return ERR;
			}

			c = fgetc(infd);
			if (ferror(infd)) {
				clearerr(infd);
				return ERR;
			}

			if ((to || delay) && (__notimeout() == ERR))
					return ERR;

#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "inkey (state assembling) got '%s'\n", unctrl(k));
#endif
			if (feof(infd) || c == -1) {	/* inter-char timeout,
							 * start backing out */
				clearerr(infd);
				if (start == end)
					/* no chars in the buffer, restart */
					goto reread;

				k = inbuf[start];
				state = INKEY_TIMEOUT;
			} else {
				k = (wchar_t) c;
				inbuf[working] = k;
				INC_POINTER(working);
				end = working;
			}
		} else {
			fprintf(stderr, "Inkey state screwed - exiting!!!");
			exit(2);
		}

		  /*
		   * Check key has no special meaning and we have not
		   * timed out and the key has not been disabled
		   */
		mapping = current->mapping[k];
		if (((state == INKEY_TIMEOUT) || (mapping < 0))
			|| ((current->key[mapping]->type == KEYMAP_LEAF)
			    && (current->key[mapping]->enable == FALSE))) {
			/* return the first key we know about */
			k = inbuf[start];

			INC_POINTER(start);
			working = start;

			if (start == end) {	/* only one char processed */
				state = INKEY_NORM;
			} else {/* otherwise we must have more than one char
				 * to backout */
				state = INKEY_BACKOUT;
			}
			return k;
		} else {	/* must be part of a multikey sequence */
			/* check for completed key sequence */
			if (current->key[current->mapping[k]]->type == KEYMAP_LEAF) {
				start = working;	/* eat the key sequence
							 * in inbuf */

				/* check if inbuf empty now */
				if (start == end) {
					/* if it is go back to normal */
					state = INKEY_NORM;
				} else {
					/* otherwise go to backout state */
					state = INKEY_BACKOUT;
				}

				/* return the symbol */
				return current->key[current->mapping[k]]->value.symbol;

			} else {
				/*
				 * Step on to next part of the multi-key
				 * sequence.
				 */
				current = current->key[current->mapping[k]]->value.next;
			}
		}
	}
}

#ifndef _CURSES_USE_MACROS
/*
 * getch --
 *	Read in a character from stdscr.
 */
int
getch(void)
{
	return wgetch(stdscr);
}

/*
 * mvgetch --
 *      Read in a character from stdscr at the given location.
 */
int
mvgetch(int y, int x)
{
	return mvwgetch(stdscr, y, x);
}

/*
 * mvwgetch --
 *      Read in a character from stdscr at the given location in the
 *      given window.
 */
int
mvwgetch(WINDOW *win, int y, int x)
{
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wgetch(win);
}

#endif

/*
 * keyok --
 *      Set the enable flag for a keysym, if the flag is false then
 * getch will not return this keysym even if the matching key sequence
 * is seen.
 */
int
keyok(int key_type, bool flag)
{
	int result = ERR;

	do_keyok(_cursesi_screen->base_keymap, key_type, flag, &result);
	return result;
}

/*
 * do_keyok --
 *       Does the actual work for keyok, we need to recurse through the
 * keymaps finding the passed key symbol.
 */
void
do_keyok(keymap_t *current, int key_type, bool flag, int *retval)
{
	key_entry_t *key;
	int i;

	  /*
	   * we need to iterate over all the keys as there may be
	   * multiple instances of the leaf symbol.
	   */
	for (i = 0; i < MAX_CHAR; i++) {
		if (current->mapping[i] < 0)
			continue; /* no mapping for the key, next! */

		key = current->key[current->mapping[i]];

		if (key->type == KEYMAP_MULTI)
			do_keyok(key->value.next, key_type, flag, retval);
		else if ((key->type == KEYMAP_LEAF)
			 && (key->value.symbol == key_type)) {
			key->enable = flag;
			*retval = OK; /* we found at least one instance, ok */
		}
	}
}

/*
 * define_key --
 *      Add a custom mapping of a key sequence to key symbol.
 *
 */
int
define_key(char *sequence, int symbol)
{

	if (symbol <= 0)
		return ERR;

	if (sequence == NULL) {
#ifdef DEBUG
		__CTRACE(__CTRACE_INPUT, "define_key: deleting keysym %d\n",
		    symbol);
#endif
		delete_key_sequence(_cursesi_screen->base_keymap, symbol);
	} else
		add_key_sequence(_cursesi_screen, sequence, symbol);

	return OK;
}

/*
 * wgetch --
 *	Read in a character from the window.
 */
int
wgetch(WINDOW *win)
{
	int inp, weset;
	int c;
	FILE *infd = _cursesi_screen->infd;

#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "wgetch: win(%p)\n", win);
#endif
	if (!(win->flags & __SCROLLOK) && (win->flags & __FULLWIN)
	    && win->curx == win->maxx - 1 && win->cury == win->maxy - 1
	    && __echoit)
		return (ERR);

	if (is_wintouched(win))
		wrefresh(win);
#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "wgetch: __echoit = %d, "
	    "__rawmode = %d, __nl = %d, flags = %#.4x, delay = %d\n",
	    __echoit, __rawmode, _cursesi_screen->nl, win->flags, win->delay);
#endif
	if (_cursesi_screen->resized) {
		_cursesi_screen->resized = 0;
#ifdef DEBUG
		__CTRACE(__CTRACE_INPUT, "wgetch returning KEY_RESIZE\n");
#endif
		return KEY_RESIZE;
	}
	if (_cursesi_screen->unget_pos) {
#ifdef DEBUG
		__CTRACE(__CTRACE_INPUT, "wgetch returning char at %d\n",
		    _cursesi_screen->unget_pos);
#endif
		_cursesi_screen->unget_pos--;
		c = _cursesi_screen->unget_list[_cursesi_screen->unget_pos];
		if (__echoit)
			waddch(win, (chtype) c);
		return c;
	}
	if (__echoit && !__rawmode) {
		cbreak();
		weset = 1;
	} else
		weset = 0;

	__save_termios();

	if (win->flags & __KEYPAD) {
		switch (win->delay)
		{
		case -1:
			inp = inkey (win->flags & __NOTIMEOUT ? 0 : 1, 0);
			break;
		case 0:
			if (__nodelay() == ERR)
				return ERR;
			inp = inkey(0, 0);
			break;
		default:
			inp = inkey(win->flags & __NOTIMEOUT ? 0 : 1, win->delay);
			break;
		}
	} else {
		switch (win->delay)
		{
		case -1:
			if (__delay() == ERR)
				return ERR;
			break;
		case 0:
			if (__nodelay() == ERR)
				return ERR;
			break;
		default:
			if (__timeout(win->delay) == ERR)
				return ERR;
			break;
		}

		c = fgetc(infd);
		if (feof(infd)) {
			clearerr(infd);
			__restore_termios();
			return ERR;	/* we have timed out */
		}

		if (ferror(infd)) {
			clearerr(infd);
			inp = ERR;
		} else {
			inp = c;
		}
	}
#ifdef DEBUG
	if (inp > 255)
		  /* we have a key symbol - treat it differently */
		  /* XXXX perhaps __unctrl should be expanded to include
		   * XXXX the keysyms in the table....
		   */
		__CTRACE(__CTRACE_INPUT, "wgetch assembled keysym 0x%x\n", inp);
	else
		__CTRACE(__CTRACE_INPUT, "wgetch got '%s'\n", unctrl(inp));
#endif
	if (win->delay > -1) {
		if (__delay() == ERR)
			return ERR;
	}

	__restore_termios();

	if ((__echoit) && (inp < KEY_MIN))
		waddch(win, (chtype) inp);

	if (weset)
		nocbreak();

	if (_cursesi_screen->nl && inp == 13)
		inp = 10;

	return ((inp < 0) || (inp == ERR) ? ERR : inp);
}

/*
 * ungetch --
 *     Put the character back into the input queue.
 */
int
ungetch(int c)
{
	return __unget((wint_t) c);
}

/*
 * __unget --
 *    Do the work for ungetch() and unget_wch();
 */
int
__unget(wint_t c)
{
	wchar_t	*p;
	int	len;

#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "__unget(%x)\n", c);
#endif
	if (_cursesi_screen->unget_pos >= _cursesi_screen->unget_len) {
		len = _cursesi_screen->unget_len + 32;
		if ((p = realloc(_cursesi_screen->unget_list,
		    sizeof(wchar_t) * len)) == NULL) {
			/* Can't realloc(), so just lose the oldest entry */
			memmove(_cursesi_screen->unget_list,
			    _cursesi_screen->unget_list + sizeof(wchar_t),
			    _cursesi_screen->unget_len - 1);
			_cursesi_screen->unget_list[_cursesi_screen->unget_len
			    - 1] = c;
			_cursesi_screen->unget_pos =
			    _cursesi_screen->unget_len;
			return OK;
		} else {
			_cursesi_screen->unget_pos =
			    _cursesi_screen->unget_len;
			_cursesi_screen->unget_len = len;
			_cursesi_screen->unget_list = p;
		}
	}
	_cursesi_screen->unget_list[_cursesi_screen->unget_pos] = c;
	_cursesi_screen->unget_pos++;
	return OK;
}
