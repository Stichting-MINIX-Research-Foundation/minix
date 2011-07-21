/*	$NetBSD: border.c,v 1.14 2010/12/25 09:59:52 blymn Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: border.c,v 1.14 2010/12/25 09:59:52 blymn Exp $");
#endif				/* not lint */

#include <stdlib.h>
#include <string.h>

#include "curses.h"
#include "curses_private.h"

#ifndef _CURSES_USE_MACROS

/*
 * border --
 *	Draw a border around stdscr using the specified
 *	delimiting characters.
 */
int
border(chtype left, chtype right, chtype top, chtype bottom, chtype topleft,
       chtype topright, chtype botleft, chtype botright)
{
	return wborder(stdscr, left, right, top, bottom, topleft, topright,
	    botleft, botright);
}

#endif

/*
 * wborder --
 *	Draw a border around the given window using the specified delimiting
 *	characters.
 */
int
wborder(WINDOW *win, chtype left, chtype right, chtype top, chtype bottom,
	chtype topleft, chtype topright, chtype botleft, chtype botright)
{
#ifndef HAVE_WCHAR
	int	 endy, endx, i;
	__LDATA	*fp, *lp;

	if (!(left & __CHARTEXT))
		left |= ACS_VLINE;
	if (!(right & __CHARTEXT))
		right |= ACS_VLINE;
	if (!(top & __CHARTEXT))
		top |= ACS_HLINE;
	if (!(bottom & __CHARTEXT))
		bottom |= ACS_HLINE;
	if (!(topleft & __CHARTEXT))
		topleft |= ACS_ULCORNER;
	if (!(topright & __CHARTEXT))
		topright |= ACS_URCORNER;
	if (!(botleft & __CHARTEXT))
		botleft |= ACS_LLCORNER;
	if (!(botright & __CHARTEXT))
		botright |= ACS_LRCORNER;

#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "wborder: left = %c, 0x%x\n",
	    left & __CHARTEXT, left & __ATTRIBUTES);
	__CTRACE(__CTRACE_INPUT, "wborder: right = %c, 0x%x\n",
	    right & __CHARTEXT, right & __ATTRIBUTES);
	__CTRACE(__CTRACE_INPUT, "wborder: top = %c, 0x%x\n",
	    top & __CHARTEXT, top & __ATTRIBUTES);
	__CTRACE(__CTRACE_INPUT, "wborder: bottom = %c, 0x%x\n",
	    bottom & __CHARTEXT, bottom & __ATTRIBUTES);
	__CTRACE(__CTRACE_INPUT, "wborder: topleft = %c, 0x%x\n",
	    topleft & __CHARTEXT, topleft & __ATTRIBUTES);
	__CTRACE(__CTRACE_INPUT, "wborder: topright = %c, 0x%x\n",
	    topright & __CHARTEXT, topright & __ATTRIBUTES);
	__CTRACE(__CTRACE_INPUT, "wborder: botleft = %c, 0x%x\n",
	    botleft & __CHARTEXT, botleft & __ATTRIBUTES);
	__CTRACE(__CTRACE_INPUT, "wborder: botright = %c, 0x%x\n",
	    botright & __CHARTEXT, botright & __ATTRIBUTES);
#endif

	/* Merge window and background attributes */
	left |= (left & __COLOR) ? (win->wattr & ~__COLOR) : win->wattr;
	left |= (left & __COLOR) ? (win->battr & ~__COLOR) : win->battr;
	right |= (right & __COLOR) ? (win->wattr & ~__COLOR) : win->wattr;
	right |= (right & __COLOR) ? (win->battr & ~__COLOR) : win->battr;
	top |= (top & __COLOR) ? (win->wattr & ~__COLOR) : win->wattr;
	top |= (top & __COLOR) ? (win->battr & ~__COLOR) : win->battr;
	bottom |= (bottom & __COLOR) ? (win->wattr & ~__COLOR) : win->wattr;
	bottom |= (bottom & __COLOR) ? (win->battr & ~__COLOR) : win->battr;
	topleft |= (topleft & __COLOR) ? (win->wattr & ~__COLOR) : win->wattr;
	topleft |= (topleft & __COLOR) ? (win->battr & ~__COLOR) : win->battr;
	topright |= (topright & __COLOR) ? (win->wattr & ~__COLOR) : win->wattr;
	topright |= (topright & __COLOR) ? (win->battr & ~__COLOR) : win->battr;
	botleft |= (botleft & __COLOR) ? (win->wattr & ~__COLOR) : win->wattr;
	botleft |= (botleft & __COLOR) ? (win->battr & ~__COLOR) : win->battr;
	botright |= (botright & __COLOR) ? (win->wattr & ~__COLOR) : win->wattr;
	botright |= (botright & __COLOR) ? (win->battr & ~__COLOR) : win->battr;

	endx = win->maxx - 1;
	endy = win->maxy - 1;
	fp = win->alines[0]->line;
	lp = win->alines[endy]->line;

	/* Sides */
	for (i = 1; i < endy; i++) {
		win->alines[i]->line[0].ch = (wchar_t) left & __CHARTEXT;
		win->alines[i]->line[0].attr = (attr_t) left & __ATTRIBUTES;
		win->alines[i]->line[endx].ch = (wchar_t) right & __CHARTEXT;
		win->alines[i]->line[endx].attr = (attr_t) right & __ATTRIBUTES;
	}
	for (i = 1; i < endx; i++) {
		fp[i].ch = (wchar_t) top & __CHARTEXT;
		fp[i].attr = (attr_t) top & __ATTRIBUTES;
		lp[i].ch = (wchar_t) bottom & __CHARTEXT;
		lp[i].attr = (attr_t) bottom & __ATTRIBUTES;
	}

	/* Corners */
	if (!(win->maxx == LINES && win->maxy == COLS &&
	    (win->flags & __SCROLLOK) && (win->flags & __SCROLLWIN))) {
		fp[0].ch = (wchar_t) topleft & __CHARTEXT;
		fp[0].attr = (attr_t) topleft & __ATTRIBUTES;
		fp[endx].ch = (wchar_t) topright & __CHARTEXT;
		fp[endx].attr = (attr_t) topright & __ATTRIBUTES;
		lp[0].ch = (wchar_t) botleft & __CHARTEXT;
		lp[0].attr = (attr_t) botleft & __ATTRIBUTES;
		lp[endx].ch = (wchar_t) botright & __CHARTEXT;
		lp[endx].attr = (attr_t) botright & __ATTRIBUTES;
	}
	__touchwin(win);
	return (OK);
#else /* HAVE_WCHAR */
	cchar_t ls, rs, ts, bs, tl, tr, bl, br;
	cchar_t *lsp, *rsp, *tsp, *bsp, *tlp, *trp, *blp, *brp;

#define S(in, out, def) \
	if (in & __CHARTEXT) { \
		__cursesi_chtype_to_cchar(in, &out); \
	} else { \
		memcpy(&out, def, sizeof(cchar_t)); \
		out.attributes |= in & __ATTRIBUTES; \
	} \
	out##p = &out;

	S(left, ls, WACS_VLINE);
	S(right, rs, WACS_VLINE);
	S(top, ts, WACS_HLINE);
	S(bottom, bs, WACS_HLINE);
	S(topleft, tl, WACS_ULCORNER);
	S(topright, tr, WACS_URCORNER);
	S(botleft, bl, WACS_LLCORNER);
	S(botright, br, WACS_LRCORNER);
#undef S
	return wborder_set(win, lsp, rsp, tsp, bsp, tlp, trp, blp, brp);
#endif /* HAVE_WCHAR */
}

int border_set(const cchar_t *ls, const cchar_t *rs, const cchar_t *ts,
	   const cchar_t *bs, const cchar_t *tl, const cchar_t *tr,
	   const cchar_t *bl, const cchar_t *br)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	return wborder_set(stdscr, ls, rs, ts, bs, tl, tr, bl, br);
#endif /* HAVE_WCHAR */
}

int wborder_set(WINDOW *win, const cchar_t *ls, const cchar_t *rs,
		const cchar_t *ts, const cchar_t *bs,
		const cchar_t *tl, const cchar_t *tr,
		const cchar_t *bl, const cchar_t *br)
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	int	 endy, endx, i, j, k, cw, pcw, tlcw, blcw, trcw, brcw;
	cchar_t left, right, bottom, top, topleft, topright, botleft, botright;
	nschar_t *np, *tnp;

	if ( ls && wcwidth( ls->vals[ 0 ]))
		memcpy( &left, ls, sizeof( cchar_t ));
	else
		memcpy( &left, WACS_VLINE, sizeof( cchar_t ));
	if ( rs && wcwidth( rs->vals[ 0 ]))
		memcpy( &right, rs, sizeof( cchar_t ));
	else
		memcpy( &right, WACS_VLINE, sizeof( cchar_t ));
	if ( ts && wcwidth( ts->vals[ 0 ]))
		memcpy( &top, ts, sizeof( cchar_t ));
	else
		memcpy( &top, WACS_HLINE, sizeof( cchar_t ));
	if ( bs && wcwidth( bs->vals[ 0 ]))
		memcpy( &bottom, bs, sizeof( cchar_t ));
	else
		memcpy( &bottom, WACS_HLINE, sizeof( cchar_t ));
	if ( tl && wcwidth( tl->vals[ 0 ]))
		memcpy( &topleft, tl, sizeof( cchar_t ));
	else
		memcpy( &topleft, WACS_ULCORNER, sizeof( cchar_t ));
	if ( tr && wcwidth( tr->vals[ 0 ]))
		memcpy( &topright, tr, sizeof( cchar_t ));
	else
		memcpy( &topright, WACS_URCORNER, sizeof( cchar_t ));
	if ( bl && wcwidth( bl->vals[ 0 ]))
		memcpy( &botleft, bl, sizeof( cchar_t ));
	else
		memcpy( &botleft, WACS_LLCORNER, sizeof( cchar_t ));
	if ( br && wcwidth( br->vals[ 0 ]))
		memcpy( &botright, br, sizeof( cchar_t ));
	else
		memcpy( &botright, WACS_LRCORNER, sizeof( cchar_t ));

#ifdef DEBUG
	__CTRACE(__CTRACE_INPUT, "wborder_set: left = %c, 0x%x\n",
	    left.vals[0], left.attributes );
	__CTRACE(__CTRACE_INPUT, "wborder_set: right = %c, 0x%x\n",
	    right.vals[0], right.attributes );
	__CTRACE(__CTRACE_INPUT, "wborder_set: top = %c, 0x%x\n",
	    top.vals[0], top.attributes );
	__CTRACE(__CTRACE_INPUT, "wborder_set: bottom = %c, 0x%x\n",
	    bottom.vals[0], bottom.attributes );
	__CTRACE(__CTRACE_INPUT, "wborder_set: topleft = %c, 0x%x\n",
	    topleft.vals[0], topleft.attributes );
	__CTRACE(__CTRACE_INPUT, "wborder_set: topright = %c, 0x%x\n",
	    topright.vals[0], topright.attributes );
	__CTRACE(__CTRACE_INPUT, "wborder_set: botleft = %c, 0x%x\n",
	    botleft.vals[0], botleft.attributes );
	__CTRACE(__CTRACE_INPUT, "wborder_set: botright = %c, 0x%x\n",
	    botright.vals[0], botright.attributes );
#endif

	/* Merge window attributes */
	left.attributes |= (left.attributes & __COLOR) ?
		(win->wattr & ~__COLOR) : win->wattr;
	right.attributes |= (right.attributes & __COLOR) ?
		(win->wattr & ~__COLOR) : win->wattr;
	top.attributes |= (top.attributes & __COLOR) ?
		(win->wattr & ~__COLOR) : win->wattr;
	bottom.attributes |= (bottom.attributes & __COLOR) ?
		(win->wattr & ~__COLOR) : win->wattr;
	topleft.attributes |= (topleft.attributes & __COLOR) ?
		(win->wattr & ~__COLOR) : win->wattr;
	topright.attributes |= (topright.attributes & __COLOR) ?
		(win->wattr & ~__COLOR) : win->wattr;
	botleft.attributes |= (botleft.attributes & __COLOR) ?
		(win->wattr & ~__COLOR) : win->wattr;
	botright.attributes |= (botright.attributes & __COLOR) ?
		(win->wattr & ~__COLOR) : win->wattr;

	endx = win->maxx - 1;
	endy = win->maxy - 1;

	/* Sides */
	for (i = 1; i < endy; i++) {
		/* left border */
		cw = wcwidth( left.vals[ 0 ]);
		if (cw < 0)
			cw = 1;
		for ( j = 0; j < cw; j++ ) {
			win->alines[i]->line[j].ch = left.vals[ 0 ];
			win->alines[i]->line[j].attr = left.attributes;
			np = win->alines[i]->line[j].nsp;
			if (np) {
				while ( np ) {
					tnp = np->next;
					free( np );
					np = tnp;
				}
				win->alines[i]->line[j].nsp = NULL;
			}
			if ( j )
				SET_WCOL( win->alines[i]->line[j], -j );
			else {
				SET_WCOL( win->alines[i]->line[j], cw );
				if ( left.elements > 1 ) {
					for (k = 1; k < left.elements; k++) {
						np = (nschar_t *)malloc(sizeof(nschar_t));
						if (!np)
							return ERR;
						np->ch = left.vals[ k ];
						np->next = win->alines[i]->line[j].nsp;
						win->alines[i]->line[j].nsp
							= np;
					}
				}
			}
		}
		for ( j = cw; WCOL( win->alines[i]->line[j]) < 0; j++ ) {
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "wborder_set: clean out partial char[%d]", j);
#endif /* DEBUG */
			win->alines[i]->line[j].ch = ( wchar_t )btowc(win->bch);
			if (_cursesi_copy_nsp(win->bnsp,
					      &win->alines[i]->line[j]) == ERR)
				return ERR;
			SET_WCOL( win->alines[i]->line[j], 1 );
		}
		/* right border */
		cw = wcwidth( right.vals[ 0 ]);
		if (cw < 0)
			cw = 1;
		pcw = WCOL( win->alines[i]->line[endx - cw]);
		for ( j = endx - cw + 1; j <= endx; j++ ) {
			win->alines[i]->line[j].ch = right.vals[ 0 ];
			win->alines[i]->line[j].attr = right.attributes;
			np = win->alines[i]->line[j].nsp;
			if (np) {
				while ( np ) {
					tnp = np->next;
					free( np );
					np = tnp;
				}
				win->alines[i]->line[j].nsp = NULL;
			}
			if ( j == endx - cw + 1 ) {
				SET_WCOL( win->alines[i]->line[j], cw );
				if ( right.elements > 1 ) {
					for (k = 1; k < right.elements; k++) {
						np = (nschar_t *)malloc(sizeof(nschar_t));
						if (!np)
							return ERR;
						np->ch = right.vals[ k ];
						np->next = win->alines[i]->line[j].nsp;
						win->alines[i]->line[j].nsp
							= np;
					}
				}
			} else
				SET_WCOL( win->alines[i]->line[j],
					endx - cw + 1 - j );
		}
		if ( pcw != 1 ) {
#ifdef DEBUG
			__CTRACE(__CTRACE_INPUT,
			    "wborder_set: clean out partial chars[%d:%d]",
			    endx - cw + pcw, endx - cw );
#endif /* DEBUG */
			k = pcw < 0 ? endx -cw + pcw : endx - cw;
			for ( j = endx - cw; j >= k; j-- ) {
				win->alines[i]->line[j].ch
					= (wchar_t)btowc(win->bch);
				if (_cursesi_copy_nsp(win->bnsp,
					       &win->alines[i]->line[j]) == ERR)
					return ERR;
				win->alines[i]->line[j].attr = win->battr;
				SET_WCOL( win->alines[i]->line[j], 1 );
			}
		}
	}
	tlcw = wcwidth( topleft.vals[ 0 ]);
	if (tlcw < 0)
		tlcw = 1;
	blcw = wcwidth( botleft.vals[ 0 ]);
	if (blcw < 0)
		blcw = 1;
	trcw = wcwidth( topright.vals[ 0 ]);
	if (trcw < 0)
		trcw = 1;
	brcw = wcwidth( botright.vals[ 0 ]);
	if (brcw < 0)
		brcw = 1;
	/* upper border */
	cw = wcwidth( top.vals[ 0 ]);
	if (cw < 0)
		cw = 1;
	for (i = tlcw; i <= min( endx - cw, endx - trcw ); i += cw ) {
		for ( j = 0; j < cw; j++ ) {
			win->alines[ 0 ]->line[i + j].ch = top.vals[ 0 ];
			win->alines[ 0 ]->line[i + j].attr = top.attributes;
			np = win->alines[ 0 ]->line[i + j].nsp;
			if (np) {
				while ( np ) {
					tnp = np->next;
					free( np );
					np = tnp;
				}
				win->alines[ 0 ]->line[i + j].nsp = NULL;
			}
			if ( j )
				SET_WCOL( win->alines[ 0 ]->line[ i + j ], -j );
			else {
				SET_WCOL( win->alines[ 0 ]->line[ i + j ], cw );
				if ( top.elements > 1 ) {
					for ( k = 1; k < top.elements; k++ ) {
						np = (nschar_t *)malloc(sizeof(nschar_t));
						if (!np)
							return ERR;
						np->ch = top.vals[ k ];
						np->next = win->alines[0]->line[i + j].nsp;
						win->alines[0]->line[i + j].nsp
							= np;
					}
				}
			}
		}
	}
	while ( i <= endx - trcw ) {
		win->alines[0]->line[i].ch =
			( wchar_t )btowc(( int ) win->bch );
		if (_cursesi_copy_nsp(win->bnsp,
				      &win->alines[0]->line[i]) == ERR)
			return ERR;
		win->alines[ 0 ]->line[ i ].attr = win->battr;
		SET_WCOL( win->alines[ 0 ]->line[ i ], 1 );
		i++;
	}
	/* lower border */
	for (i = blcw; i <= min( endx - cw, endx - brcw ); i += cw ) {
		for ( j = 0; j < cw; j++ ) {
			win->alines[ endy ]->line[i + j].ch = bottom.vals[ 0 ];
			win->alines[endy]->line[i + j].attr = bottom.attributes;
			np = win->alines[ endy ]->line[i + j].nsp;
			if (np) {
				while ( np ) {
					tnp = np->next;
					free( np );
					np = tnp;
				}
				win->alines[ endy ]->line[i + j].nsp = NULL;
			}
			if ( j )
				SET_WCOL( win->alines[endy]->line[i + j], -j);
			else {
				SET_WCOL( win->alines[endy]->line[i + j], cw );
				if ( bottom.elements > 1 ) {
					for ( k = 1; k < bottom.elements;
							k++ ) {
						if ( !( np = ( nschar_t *)malloc( sizeof( nschar_t ))))
							return ERR;
						np->ch = bottom.vals[ k ];
						np->next = win->alines[endy]->line[i + j].nsp;
						win->alines[endy]->line[i + j].nsp = np;
					}
				}
			}
		}
	}
	while ( i <= endx - brcw ) {
		win->alines[endy]->line[i].ch =
			(wchar_t)btowc((int) win->bch );
		if (_cursesi_copy_nsp(win->bnsp,
				      &win->alines[endy]->line[i]) == ERR)
			return ERR;
		win->alines[ endy ]->line[ i ].attr = win->battr;
		SET_WCOL( win->alines[ endy ]->line[ i ], 1 );
		i++;
	}

	/* Corners */
	if (!(win->maxx == LINES && win->maxy == COLS &&
		(win->flags & __SCROLLOK) && (win->flags & __SCROLLWIN))) {
		for ( i = 0; i < tlcw; i++ ) {
			win->alines[ 0 ]->line[i].ch = topleft.vals[ 0 ];
			win->alines[ 0 ]->line[i].attr = topleft.attributes;
			np = win->alines[ 0 ]->line[i].nsp;
			if (np) {
				while ( np ) {
					tnp = np->next;
					free( np );
					np = tnp;
				}
				win->alines[ 0 ]->line[i].nsp = NULL;
			}
			if ( i )
				SET_WCOL( win->alines[ 0 ]->line[ i ], -i );
			else {
				SET_WCOL( win->alines[ 0 ]->line[ i ], tlcw );
				if ( topleft.elements > 1 ) {
					for ( k = 1; k < topleft.elements;
							k++ ) {
						np = (nschar_t *)malloc(sizeof(nschar_t));
						if (!np)
							return ERR;
						np->ch = topleft.vals[ k ];
						np->next = win->alines[ 0 ]->line[i].nsp;
						win->alines[ 0 ]->line[i].nsp
							= np;
					}
				}
			}
		}
		for ( i = endx - trcw + 1; i <= endx; i++ ) {
			win->alines[ 0 ]->line[i].ch = topright.vals[ 0 ];
			win->alines[ 0 ]->line[i].attr = topright.attributes;
			np = win->alines[ 0 ]->line[i].nsp;
			if (np) {
				while ( np ) {
					tnp = np->next;
					free( np );
					np = tnp;
				}
				win->alines[ 0 ]->line[i].nsp = NULL;
			}
			if ( i == endx - trcw + 1 ) {
				SET_WCOL( win->alines[ 0 ]->line[ i ], trcw );
				if ( topright.elements > 1 ) {
					for ( k = 1; k < topright.elements;
							k++ ) {
						np = (nschar_t *)malloc(sizeof(nschar_t));
						if (!np)
							return ERR;
						np->ch = topright.vals[ k ];
						np->next = win->alines[0]->line[i].nsp;
						win->alines[ 0 ]->line[i].nsp
							= np;
					}
				}
			} else
				SET_WCOL( win->alines[ 0 ]->line[ i ],
					  endx - trcw + 1 - i );
		}
		for ( i = 0; i < blcw; i++ ) {
			win->alines[ endy ]->line[i].ch = botleft.vals[ 0 ];
			win->alines[ endy ]->line[i].attr = botleft.attributes;
			np = win->alines[ endy ]->line[i].nsp;
			if (np) {
				while ( np ) {
					tnp = np->next;
					free( np );
					np = tnp;
				}
				win->alines[ endy ]->line[i].nsp = NULL;
			}
			if ( i )
				SET_WCOL( win->alines[endy]->line[i], -i );
			else {
				SET_WCOL( win->alines[endy]->line[i], blcw );
				if ( botleft.elements > 1 ) {
					for ( k = 1; k < botleft.elements;
							k++ ) {
						np = (nschar_t *)malloc(sizeof(nschar_t));
						if (!np)
							return ERR;
						np->ch = botleft.vals[ k ];
						np->next = win->alines[endy]->line[i].nsp;
						win->alines[endy]->line[i].nsp
							= np;
					}
				}
			}
		}
		for ( i = endx - brcw + 1; i <= endx; i++ ) {
			win->alines[ endy ]->line[i].ch = botright.vals[ 0 ];
			win->alines[ endy ]->line[i].attr = botright.attributes;
			np = win->alines[ endy ]->line[i].nsp;
			if (np) {
				while ( np ) {
					tnp = np->next;
					free( np );
					np = tnp;
				}
				win->alines[ endy ]->line[i].nsp = NULL;
			}
			if ( i == endx - brcw + 1 ) {
				SET_WCOL( win->alines[ endy ]->line[ i ],
					  brcw );
				if ( botright.elements > 1 ) {
					for ( k = 1; k < botright.elements; k++ ) {
						np = (nschar_t *)malloc(sizeof(nschar_t));
						if (!np)
							return ERR;
						np->ch = botright.vals[ k ];
						np->next = win->alines[endy]->line[i].nsp;
						win->alines[endy]->line[i].nsp
							= np;
					}
				}
			} else
				SET_WCOL( win->alines[ endy ]->line[ i ],
					endx - brcw + 1 - i );
		}
	}
	__touchwin(win);
	return (OK);
#endif /* HAVE_WCHAR */
}
