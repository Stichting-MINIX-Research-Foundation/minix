/*	$NetBSD: m_func.c,v 1.1.1.2 2008/05/18 14:31:26 aymeric Exp $ */

/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: m_func.c,v 8.28 2003/11/05 17:09:59 skimo Exp (Berkeley) Date: 2003/11/05 17:09:59";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <Xm/PanedW.h>
#include <Xm/ScrollBar.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef LOCK_SUCCESS
#include "../common/common.h"
#include "../ipc/ip.h"
#include "m_motif.h"


static int
vi_addstr(int ipvi, char *str1, u_int32_t len1)
{
#ifdef TRACE
	vtrace("addstr() {%.*s}\n", ipbp->len1, ipbp->str1);
#endif
	/* Add to backing store. */
	memcpy(CharAt(__vi_screen, __vi_screen->cury, __vi_screen->curx),
	    str1, len1);
	memset(FlagAt(__vi_screen, __vi_screen->cury, __vi_screen->curx),
	    __vi_screen->color, len1);

	/* Draw from backing store. */
	__vi_draw_text(__vi_screen,
	    __vi_screen->cury, __vi_screen->curx, len1);

	/* Advance the caret. */
	__vi_move_caret(__vi_screen,
	    __vi_screen->cury, __vi_screen->curx + len1);
	return (0);
}

static int
vi_attribute(int ipvi, u_int32_t val1, u_int32_t val2)
{
	switch (val1) {
	case SA_ALTERNATE:
		/* XXX: Nothing. */
		break;
	case SA_INVERSE:
		__vi_screen->color = val2;
		break;
	}
	return (0);
}

static int
vi_bell(int ipvi)
{
	/*
	 * XXX
	 * Future... implement visible bell.
	 */
	XBell(XtDisplay(__vi_screen->area), 0);
	return (0);
}

static int
vi_busyon(int ipvi, char *str1, u_int32_t len1)
{
	__vi_set_cursor(__vi_screen, 1);
	return (0);
}

static int
vi_busyoff(int ipvi)
{
	__vi_set_cursor(__vi_screen, 0);
	return (0);
}

static int
vi_clrtoeol(int ipvi)
{
	int len;
	char *ptr;

	len = __vi_screen->cols - __vi_screen->curx;
	ptr = CharAt(__vi_screen, __vi_screen->cury, __vi_screen->curx);
	
	/* Clear backing store. */
	memset(ptr, ' ', len);
	memset(FlagAt(__vi_screen, __vi_screen->cury, __vi_screen->curx),
	    COLOR_STANDARD, len);

	/* Draw from backing store. */
	__vi_draw_text(__vi_screen, __vi_screen->cury, __vi_screen->curx, len);

	return (0);
}

static int
vi_deleteln(int ipvi)
{
	int y, rows, len, height, width;

	y = __vi_screen->cury;
	rows = __vi_screen->rows - (y+1);
	len = __vi_screen->cols * rows;

	/* Don't want to copy the caret! */
	__vi_erase_caret(__vi_screen);

	/* Adjust backing store and the flags. */
	memmove(CharAt(__vi_screen, y, 0), CharAt(__vi_screen, y+1, 0), len);
	memmove(FlagAt(__vi_screen, y, 0), FlagAt(__vi_screen, y+1, 0), len);

	/* Move the bits on the screen. */
	width = __vi_screen->ch_width * __vi_screen->cols;
	height = __vi_screen->ch_height * rows;
	XCopyArea(XtDisplay(__vi_screen->area),		/* display */
		  XtWindow(__vi_screen->area),		/* src */
		  XtWindow(__vi_screen->area),		/* dest */
		  __vi_copy_gc,				/* context */
		  0, YTOP(__vi_screen, y+1),		/* srcx, srcy */
		  width, height,
		  0, YTOP(__vi_screen, y)		/* dstx, dsty */
		  );
	/* Need to let X take over. */
	XmUpdateDisplay(__vi_screen->area);

	return (0);
}

static int
vi_discard(int ipvi)
{
	/* XXX: Nothing. */
	return (0);
}

static int
vi_insertln(int ipvi)
{
	int y, rows, height, width;
	char *from, *to;

	y = __vi_screen->cury;
	rows = __vi_screen->rows - (1+y);
	from = CharAt(__vi_screen, y, 0),
	to = CharAt(__vi_screen, y+1, 0);

	/* Don't want to copy the caret! */
	__vi_erase_caret(__vi_screen);

	/* Adjust backing store. */
	memmove(to, from, __vi_screen->cols * rows);
	memset(from, ' ', __vi_screen->cols);

	/* And the backing store. */
	from = FlagAt(__vi_screen, y, 0),
	to = FlagAt(__vi_screen, y+1, 0);
	memmove(to, from, __vi_screen->cols * rows);
	memset(from, COLOR_STANDARD, __vi_screen->cols);

	/* Move the bits on the screen. */
	width = __vi_screen->ch_width * __vi_screen->cols;
	height = __vi_screen->ch_height * rows;

	XCopyArea(XtDisplay(__vi_screen->area),		/* display */
		  XtWindow(__vi_screen->area),		/* src */
		  XtWindow(__vi_screen->area),		/* dest */
		  __vi_copy_gc,				/* context */
		  0, YTOP(__vi_screen, y),		/* srcx, srcy */
		  width, height,
		  0, YTOP(__vi_screen, y+1)		/* dstx, dsty */
		  );

	/* clear out the new space */
	XClearArea(XtDisplay(__vi_screen->area),	/* display */
		   XtWindow(__vi_screen->area),		/* window */
		   0, YTOP(__vi_screen, y),		/* srcx, srcy */
		   0, __vi_screen->ch_height,		/* w=full, height */
		   True					/* no exposures */
		   );

	/* Need to let X take over. */
	XmUpdateDisplay(__vi_screen->area);

	return (0);
}

static int
vi_move(int ipvi, u_int32_t val1, u_int32_t val2)
{
	__vi_move_caret(__vi_screen, val1, val2);
	return (0);
}

static int
vi_redraw(int ipvi)
{
	__vi_expose_func(0, __vi_screen, 0);
	return (0);
}

static int
vi_refresh(int ipvi)
{
	/* probably ok to scroll again */
	__vi_clear_scroll_block();

	/* if the tag stack widget is active, set the text field there
	 * to agree with the current caret position.
	 * Note that this really ought to be done by core due to wrapping issues
	 */
	__vi_set_word_at_caret( __vi_screen );

	/* similarly, the text ruler... */
	__vi_set_text_ruler( __vi_screen->cury, __vi_screen->curx );

	return (0);
}

static int
vi_quit(int ipvi)
{
	if (__vi_exitp != NULL)
		__vi_exitp();

	return (0);
}

static int
vi_rename(int ipvi, char *str1, u_int32_t len1)
{
	Widget shell;
	size_t len;
	const char *tail, *p;

	/* For the icon, use the tail. */
	for (p = str1, len = len1; len > 1; ++p, --len)
		if (p[0] == '/')
			tail = p + 1;
	/*
	 * XXX
	 * Future:  Attach a title to each screen.  For now, we change
	 * the title of the shell.
	 */
	shell = __vi_screen->area;
	while ( ! XtIsShell(shell) ) shell = XtParent(shell);
	XtVaSetValues(shell,
		      XmNiconName,	tail,
		      XmNtitle,		str1,
		      0
		      );
	return (0);
}

static int
vi_rewrite(int ipvi, u_int32_t val1)
{
	/* XXX: Nothing. */
	return (0);
}


static int
vi_scrollbar(int ipvi, u_int32_t val1, u_int32_t val2, u_int32_t val3)
{
	int top, size, maximum, old_max;

	/* in the buffer,
	 *	val1 contains the top visible line number
	 *	val2 contains the number of visible lines
	 *	val3 contains the number of lines in the file
	 */
	top	= val1;
	size	= val2;
	maximum	= val3;

#if 0
	fprintf( stderr, "Setting scrollbar\n" );
	fprintf( stderr, "\tvalue\t\t%d\n",	top );
	fprintf( stderr, "\tsize\t\t%d\n",	size );
	fprintf( stderr, "\tmaximum\t\t%d\n",	maximum );
#endif

	/* armor plating.  core thinks there are no lines in an
	 * empty file, but says we are on line 1
	 */
	if ( top >= maximum ) {
#if 0
	    fprintf( stderr, "Correcting for top >= maximum\n" );
#endif
	    maximum	= top + 1;
	    size	= 1;
	}

	/* armor plating.  core may think there are more
	 * lines visible than remain in the file
	 */
	if ( top+size >= maximum ) {
#if 0
	    fprintf( stderr, "Correcting for top+size >= maximum\n" );
#endif
	    size	= maximum - top;
	}

	/* need to increase the maximum before changing the values */
	XtVaGetValues( __vi_screen->scroll, XmNmaximum, &old_max, 0 );
	if ( maximum > old_max )
	    XtVaSetValues( __vi_screen->scroll, XmNmaximum, maximum, 0 );

	/* change the rest of the values without generating a callback */
	XmScrollBarSetValues( __vi_screen->scroll,
			      top,
			      size,
			      1,	/* increment */
			      size,	/* page_increment */
			      False	/* do not notify me */
			      );

	/* need to decrease the maximum after changing the values */
	if ( maximum < old_max )
	    XtVaSetValues( __vi_screen->scroll, XmNmaximum, maximum, 0 );

	/* done */
	return (0);
}

static int
vi_select(int ipvi, char *str1, u_int32_t len1)
{
	/* XXX: Nothing. */
	return (0);
}

static int
vi_split(int ipvi)
{
	/* XXX: Nothing. */
	return (0);
}

IPSIOPS ipsi_ops_motif = {
	vi_addstr,
	vi_attribute,
	vi_bell,
	vi_busyoff,
	vi_busyon,
	vi_clrtoeol,
	vi_deleteln,
	vi_discard,
	__vi_editopt,
	vi_insertln,
	vi_move,
	vi_quit,
	vi_redraw,
	vi_refresh,
	vi_rename,
	vi_rewrite,
	vi_scrollbar,
	vi_select,
	vi_split,
	vi_addstr,
};
