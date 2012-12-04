/*   $NetBSD: get_wch.c,v 1.10 2012/06/29 10:40:29 blymn Exp $ */

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
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the NetBSD Foundation nor the names of its
 *	contributors may be used to endorse or promote products derived
 *	from this software without specific prior written permission.
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: get_wch.c,v 1.10 2012/06/29 10:40:29 blymn Exp $");
#endif						  /* not lint */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "curses.h"
#include "curses_private.h"
#include "keymap.h"

#ifdef HAVE_WCHAR
static short   wstate;		  /* state of the wcinkey function */
#endif /* HAVE_WCHAR */
extern short state;		/* storage declared in getch.c */

/* prototypes for private functions */
#ifdef HAVE_WCHAR
static int inkey(wchar_t *wc, int to, int delay);
#endif /* HAVE_WCHAR */

#ifdef HAVE_WCHAR
/*
 * __init_get_wch - initialise all the pointers & structures needed to make
 * get_wch work in keypad mode.
 *
 */
void
__init_get_wch(SCREEN *screen)
{
	wstate = INKEY_NORM;
	memset( &screen->cbuf, 0, sizeof(screen->cbuf));
	screen->cbuf_head = screen->cbuf_tail = screen->cbuf_cur = 0;
}
#endif /* HAVE_WCHAR */


#ifdef HAVE_WCHAR
/*
 * inkey - do the work to process keyboard input, check for multi-key
 * sequences and return the appropriate symbol if we get a match.
 *
 */
static int
inkey(wchar_t *wc, int to, int delay)
{
	wchar_t		 k = 0;
	int		  c, mapping, ret = 0;
	size_t	  mlen = 0;
	keymap_t	*current = _cursesi_screen->base_keymap;
	FILE		*infd = _cursesi_screen->infd;
	int		 *start = &_cursesi_screen->cbuf_head,
				*working = &_cursesi_screen->cbuf_cur,
				*end = &_cursesi_screen->cbuf_tail;
	char		*inbuf = &_cursesi_screen->cbuf[ 0 ];

#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "inkey (%p, %d, %d)\n", wc, to, delay);
#endif
	for (;;) { /* loop until we get a complete key sequence */
		if (wstate == INKEY_NORM) {
			if (delay && __timeout(delay) == ERR)
				return ERR;
			c = fgetc(infd);
			if (c == WEOF) {
				clearerr(infd);
				return ERR;
			}

			if (delay && (__notimeout() == ERR))
				return ERR;

			k = (wchar_t) c;
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "inkey (wstate normal) got '%s'\n", unctrl(k));
#endif

			inbuf[ *end ] = k;
			*end = ( *end + 1 ) % MAX_CBUF_SIZE;
			*working = *start;
			wstate = INKEY_ASSEMBLING; /* go to assembling state */
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "inkey: NORM=>ASSEMBLING: start(%d), "
			    "current(%d), end(%d)\n", *start, *working, *end);
#endif /* DEBUG */
		} else if (wstate == INKEY_BACKOUT) {
			k = inbuf[*working];
			*working = ( *working + 1 ) % MAX_CBUF_SIZE;
			if (*working == *end) {	/* see if run out of keys */
				/* if so, switch to assembling */
				wstate = INKEY_ASSEMBLING;
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "inkey: BACKOUT=>ASSEMBLING, start(%d), "
				    "current(%d), end(%d)\n",
				    *start, *working, *end);
#endif /* DEBUG */
			}
		} else if (wstate == INKEY_ASSEMBLING) {
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

			k = (wchar_t) c;
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "inkey (wstate assembling) got '%s'\n", unctrl(k));
#endif /* DEBUG */
			if (feof(infd)) { /* inter-char T/O, start backout */
				clearerr(infd);
				if (*start == *end)
					/* no chars in the buffer, restart */
					continue;

				k = inbuf[*start];
				wstate = INKEY_TIMEOUT;
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "inkey: ASSEMBLING=>TIMEOUT, start(%d), "
				    "current(%d), end(%d)\n",
				    *start, *working, *end);
#endif /* DEBUG */
			} else {
				inbuf[ *end ] = k;
				*working = *end;
				*end = ( *end + 1 ) % MAX_CBUF_SIZE;
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "inkey: ASSEMBLING: start(%d), "
				    "current(%d), end(%d)",
				    *start, *working, *end);
#endif /* DEBUG */
			}
		} else if (wstate == INKEY_WCASSEMBLING) {
			/* assembling a wide-char sequence */
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

			k = (wchar_t) c;
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "inkey (wstate wcassembling) got '%s'\n",
				unctrl(k));
#endif
			if (feof(infd)) { /* inter-char T/O, start backout */
				clearerr(infd);
				if (*start == *end)
					/* no chars in the buffer, restart */
					continue;

				*wc = inbuf[*start];
				*working = *start
					= ( *start + 1 ) % MAX_CBUF_SIZE;
				if (*start == *end) {
					state = wstate = INKEY_NORM;
#ifdef DEBUG
					__CTRACE(__CTRACE_INPUT,
					    "inkey: WCASSEMBLING=>NORM, "
					    "start(%d), current(%d), end(%d)",
					    *start, *working, *end);
#endif /* DEBUG */
				} else {
					state = wstate = INKEY_BACKOUT;
#ifdef DEBUG
					__CTRACE(__CTRACE_INPUT,
					    "inkey: WCASSEMBLING=>BACKOUT, "
					    "start(%d), current(%d), end(%d)",
					    *start, *working, *end);
#endif /* DEBUG */
				}
				return OK;
			} else {
				/* assembling wide characters */
				inbuf[ *end ] = k;
				*working = *end;
				*end = ( *end + 1 ) % MAX_CBUF_SIZE;
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "inkey: WCASSEMBLING[head(%d), "
				    "urrent(%d), tail(%d)]\n",
				    *start, *working, *end);
#endif /* DEBUG */
				ret = (int) mbrtowc( wc, inbuf + (*working), 1,
					&_cursesi_screen->sp );
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "inkey: mbrtowc returns %d, wc(%x)\n",
				    ret, *wc );
#endif /* DEBUG */
				if ( ret == -2 ) {
					*working = (*working + 1)
						% MAX_CBUF_SIZE;
					continue;
				}
				if ( ret == 0 )
					ret = 1;
				if ( ret == -1 ) {
					/* return the 1st character we know */
					*wc = inbuf[ *start ];
					*working = *start = ( *start + 1 ) % MAX_CBUF_SIZE;
#ifdef DEBUG
					__CTRACE(__CTRACE_INPUT,
					    "inkey: Invalid wide char(%x) "
					    "[head(%d), current(%d), "
					    "tail(%d)]\n",
					    *wc, *start, *working, *end);
#endif /* DEBUG */
				} else { /* > 0 */
					/* return the wide character */
					*start = *working
					       = (*working + ret)%MAX_CBUF_SIZE;
#ifdef DEBUG
					__CTRACE(__CTRACE_INPUT,
					    "inkey: Wide char found(%x) "
					    "[head(%d), current(%d), "
					    "tail(%d)]\n",
					    *wc, *start, *working, *end);
#endif /* DEBUG */
				}

				if (*start == *end) {	/* only one char processed */
					state = wstate = INKEY_NORM;
#ifdef DEBUG
					__CTRACE(__CTRACE_INPUT,
					    "inkey: WCASSEMBLING=>NORM, "
					    "start(%d), current(%d), end(%d)",
					    *start, *working, *end);
#endif /* DEBUG */
				} else {
					/* otherwise we must have more than one char to backout */
					state = wstate = INKEY_BACKOUT;
#ifdef DEBUG
					__CTRACE(__CTRACE_INPUT,
					    "inkey: WCASSEMBLING=>BACKOUT, "
					    "start(%d), current(%d), end(%d)",
					    *start, *working, *end);
#endif /* DEBUG */
				}
				return OK;
			}
		} else {
			fprintf(stderr, "Inkey wstate screwed - exiting!!!");
			exit(2);
		}

		/*
		 * Check key has no special meaning and we have not
		 * timed out and the key has not been disabled
		 */
		mapping = current->mapping[k];
		if (((wstate == INKEY_TIMEOUT) || (mapping < 0))
				|| ((current->key[mapping]->type
					== KEYMAP_LEAF)
				&& (current->key[mapping]->enable == FALSE))) {
			/* wide-character specific code */
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "inkey: Checking for wide char\n");
#endif /* DEBUG */
			mbrtowc( NULL, NULL, 1, &_cursesi_screen->sp );
			*working = *start;
			mlen = *end > *working ?
				*end - *working : MAX_CBUF_SIZE - *working;
			if ( !mlen )
				return ERR;
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "inkey: Check wide char[head(%d), "
			    "current(%d), tail(%d), mlen(%ld)]\n",
			    *start, *working, *end, (long) mlen);
#endif /* DEBUG */
			ret = (int) mbrtowc( wc, inbuf + (*working), mlen,
				&_cursesi_screen->sp );
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "inkey: mbrtowc returns %d, wc(%x)\n", ret, *wc);
#endif /* DEBUG */
			if ( ret == -2 && *end < *working ) {
				/* second half of a wide character */
				*working = 0;
				mlen = *end;
				if ( mlen )
					ret = (int) mbrtowc( wc, inbuf, mlen,
						&_cursesi_screen->sp );
			}
			if ( ret == -2 && wstate != INKEY_TIMEOUT ) {
				*working = (*working + (int) mlen)
					% MAX_CBUF_SIZE;
				wstate = INKEY_WCASSEMBLING;
				continue;
			}
			if ( ret == 0 )
				ret = 1;
			if ( ret == -1 ) {
				/* return the first key we know about */
				*wc = inbuf[ *start ];
				*working = *start
					= ( *start + 1 ) % MAX_CBUF_SIZE;
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "inkey: Invalid wide char(%x)[head(%d), "
				    "current(%d), tail(%d)]\n",
				    *wc, *start, *working, *end);
#endif /* DEBUG */
			} else { /* > 0 */
				/* return the wide character */
				*start = *working
					= ( *working + ret ) % MAX_CBUF_SIZE;
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "inkey: Wide char found(%x)[head(%d), "
				    "current(%d), tail(%d)]\n",
				    *wc, *start, *working, *end);
#endif /* DEBUG */
			}

			if (*start == *end) {	/* only one char processed */
				state = wstate = INKEY_NORM;
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "inkey: Empty cbuf=>NORM, "
				    "start(%d), current(%d), end(%d)\n",
				    *start, *working, *end);
#endif /* DEBUG */
			} else {
				/* otherwise we must have more than one char to backout */
				state = wstate = INKEY_BACKOUT;
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "inkey: Non-empty cbuf=>BACKOUT, "
				    "start(%d), current(%d), end(%d)\n",
				    *start, *working, *end);
#endif /* DEBUG */
			}
			return OK;
		} else {	/* must be part of a multikey sequence */
					/* check for completed key sequence */
			if (current->key[current->mapping[k]]->type
					== KEYMAP_LEAF) {
				/* eat the key sequence in cbuf */
				*start = *working = ( *working + 1 ) % MAX_CBUF_SIZE;

				/* check if inbuf empty now */
#ifdef DEBUG
				__CTRACE(__CTRACE_INPUT,
				    "inkey: Key found(%s)\n",
				    key_name(current->key[mapping]->value.symbol));
#endif /* DEBUG */
				if (*start == *end) {
					/* if it is go back to normal */
					state = wstate = INKEY_NORM;
#ifdef DEBUG
					__CTRACE(__CTRACE_INPUT,
					    "[inkey]=>NORM, start(%d), "
					    "current(%d), end(%d)",
					    *start, *working, *end);
#endif /* DEBUG */
				} else {
					/* otherwise go to backout state */
					state = wstate = INKEY_BACKOUT;
#ifdef DEBUG
					__CTRACE(__CTRACE_INPUT,
					    "[inkey]=>BACKOUT, start(%d), "
					    "current(%d), end(%d)",
					    *start, *working, *end );
#endif /* DEBUG */
				}

				/* return the symbol */
				*wc = current->key[mapping]->value.symbol;
				return KEY_CODE_YES;
			} else {
				/* Step to next part of multi-key sequence */
				current = current->key[current->mapping[k]]->value.next;
			}
		}
	}
}
#endif /* HAVE_WCHAR */

/*
 * get_wch --
 *	Read in a wide character from stdscr.
 */
int
get_wch(wint_t *ch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wget_wch(stdscr, ch);
#endif /* HAVE_WCHAR */
}

/*
 * mvget_wch --
 *	  Read in a character from stdscr at the given location.
 */
int
mvget_wch(int y, int x, wint_t *ch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return mvwget_wch(stdscr, y, x, ch);
#endif /* HAVE_WCHAR */
}

/*
 * mvwget_wch --
 *	  Read in a character from stdscr at the given location in the
 *	  given window.
 */
int
mvwget_wch(WINDOW *win, int y, int x, wint_t *ch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (wmove(win, y, x) == ERR)
		return ERR;

	return wget_wch(win, ch);
#endif /* HAVE_WCHAR */
}

/*
 * wget_wch --
 *	Read in a wide character from the window.
 */
int
wget_wch(WINDOW *win, wint_t *ch)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	int ret, weset;
	int c;
	FILE *infd = _cursesi_screen->infd;
	cchar_t wc;
	wchar_t inp, ws[ 2 ];

	if (!(win->flags & __SCROLLOK) && (win->flags & __FULLWIN)
			&& win->curx == win->maxx - 1
			&& win->cury == win->maxy - 1
			&& __echoit)
		return (ERR);

	if (is_wintouched(win))
		wrefresh(win);
#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "wget_wch: __echoit = %d, "
	    "__rawmode = %d, __nl = %d, flags = %#.4x\n",
	    __echoit, __rawmode, _cursesi_screen->nl, win->flags);
#endif
	if (_cursesi_screen->resized) {
		_cursesi_screen->resized = 0;
		*ch = KEY_RESIZE;
		return KEY_CODE_YES;
	}
	if (_cursesi_screen->unget_pos) {
#ifdef DEBUG
		__CTRACE(__CTRACE_INPUT, "wget_wch returning char at %d\n",
		    _cursesi_screen->unget_pos);
#endif
		_cursesi_screen->unget_pos--;
		*ch = _cursesi_screen->unget_list[_cursesi_screen->unget_pos];
		if (__echoit) {
			ws[0] = *ch, ws[1] = L'\0';
			setcchar(&wc, ws, win->wattr, 0, NULL);
			wadd_wch(win, &wc);
		}
		return KEY_CODE_YES;
	}
	if (__echoit && !__rawmode) {
		cbreak();
		weset = 1;
	} else
		weset = 0;

	__save_termios();

	if (win->flags & __KEYPAD) {
		switch (win->delay) {
			case -1:
				ret = inkey(&inp,
					win->flags & __NOTIMEOUT ? 0 : 1, 0);
				break;
			case 0:
				if (__nodelay() == ERR)
					return ERR;
				ret = inkey(&inp, 0, 0);
				break;
			default:
				ret = inkey(&inp,
					win->flags & __NOTIMEOUT ? 0 : 1,
					win->delay);
				break;
		}
		if ( ret == ERR )
			return ERR;
	} else {
		switch (win->delay) {
			case -1:
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

		c = getwchar();
		if (feof(infd)) {
			clearerr(infd);
			__restore_termios();
			return ERR;	/* we have timed out */
		}

		if (ferror(infd)) {
			clearerr(infd);
			return ERR;
		} else {
			ret = c;
			inp = c;
		}
	}
#ifdef DEBUG
	if (inp > 255)
		/* we have a key symbol - treat it differently */
		/* XXXX perhaps __unctrl should be expanded to include
		 * XXXX the keysyms in the table....
		 */
		__CTRACE(__CTRACE_INPUT, "wget_wch assembled keysym 0x%x\n",
		    inp);
	else
		__CTRACE(__CTRACE_INPUT, "wget_wch got '%s'\n", unctrl(inp));
#endif
	if (win->delay > -1) {
		if (__delay() == ERR)
			return ERR;
	}

	__restore_termios();

	if (__echoit) {
		if ( ret == KEY_CODE_YES ) {
			/* handle [DEL], [BS], and [LEFT] */
			if ( win->curx &&
					( inp == KEY_DC ||
					  inp == KEY_BACKSPACE ||
					  inp == KEY_LEFT )) {
				wmove( win, win->cury, win->curx - 1 );
				wdelch( win );
			}
		} else {
			ws[ 0 ] = inp, ws[ 1 ] = L'\0';
			setcchar( &wc, ws, win->wattr, 0, NULL );
			wadd_wch( win, &wc );
		}
	}

	if (weset)
		nocbreak();

	if (_cursesi_screen->nl && inp == 13)
		inp = 10;

	*ch = inp;

	if ( ret == KEY_CODE_YES )
		return KEY_CODE_YES;
	return ( inp < 0 ? ERR : OK );
#endif /* HAVE_WCHAR */
}

/*
 * unget_wch --
 *	 Put the wide character back into the input queue.
 */
int
unget_wch(const wchar_t c)
{
	return __unget((wint_t) c);
}
