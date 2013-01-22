/*	$NetBSD: m_vi.c,v 1.2 2011/03/21 14:53:03 tnozaki Exp $ */

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
static const char sccsid[] = "Id: m_vi.c,v 8.41 2003/11/05 17:10:01 skimo Exp (Berkeley) Date: 2003/11/05 17:10:01";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <Xm/PanedW.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/ScrollBar.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#undef LOCK_SUCCESS
#include "../common/common.h"
#include "../ipc/ip.h"
#include "m_motif.h"
#include "vi_mextern.h"
#include "pathnames.h"

extern int vi_ofd;

static	void	f_copy(String *buffer, int *len);
static	void	f_paste(int widget, int buffer, int length);
static	void	f_clear(Widget widget);


/*
 * Globals and costants
 */

#define	BufferSize	1024

static	XFontStruct	*font;
static	GC		gc;
	GC		__vi_copy_gc;
static	XtAppContext	ctx;

	xvi_screen	*__vi_screen = NULL;
static	Cursor		std_cursor;
static	Cursor		busy_cursor;
static	XtTranslations	area_trans;
static	int		multi_click_length;

void (*__vi_exitp)();				/* Exit function. */


/* hack for drag scrolling...
 * I'm not sure why, but the current protocol gets out of sync when
 * a lot of drag messages get passed around.  Likely, we need to wait
 * for core to finish repainting the screen before sending more drag
 * messages.
 * To that end, we set scroll_block when we receive input from the scrollbar,
 * and we clear it when we process the IPO_REFRESH message from core.
 * A specific SCROLL_COMPLETED message would be better, but this seems to work.
 */

static Boolean scroll_block = False;

/*
 * PUBLIC: void __vi_set_scroll_block __P((void));
 */
void
__vi_set_scroll_block(void)
{
	scroll_block = True;
}

/*
 * PUBLIC: void __vi_clear_scroll_block __P((void));
 */
void
__vi_clear_scroll_block(void)
{
	scroll_block = False;
}


#if defined(__STDC__)
static	void	set_gc_colors( xvi_screen *this_screen, int val )
#else
static	void	set_gc_colors( this_screen, val )
xvi_screen	*this_screen;
int		val;
#endif
{
    static Pixel	fg, bg, hi, shade;
    static int		prev = COLOR_INVALID;

    /* no change? */
    if ( prev == val ) return;

    /* init? */
    if ( gc == NULL ) {

	/* what colors are selected for the drawing area? */
	XtVaGetValues( this_screen->area,
		       XtNbackground,		&bg,
		       XtNforeground,		&fg,
		       XmNhighlightColor,	&hi,
		       XmNtopShadowColor,	&shade,
		       0
		       );

	gc = XCreateGC( XtDisplay(this_screen->area),
		        DefaultRootWindow(XtDisplay(this_screen->area)),
			0,
			0
			);

	XSetFont( XtDisplay(this_screen->area), gc, font->fid );
    }

    /* special colors? */
    if ( val & COLOR_CARET ) {
	XSetForeground( XtDisplay(this_screen->area), gc, fg );
	XSetBackground( XtDisplay(this_screen->area), gc, hi );
    }
    else if ( val & COLOR_SELECT ) {
	XSetForeground( XtDisplay(this_screen->area), gc, fg );
	XSetBackground( XtDisplay(this_screen->area), gc, shade );
    }
    else switch (val) {
	case COLOR_STANDARD:
	    XSetForeground( XtDisplay(this_screen->area), gc, fg );
	    XSetBackground( XtDisplay(this_screen->area), gc, bg );
	    break;
	case COLOR_INVERSE:
	    XSetForeground( XtDisplay(this_screen->area), gc, bg );
	    XSetBackground( XtDisplay(this_screen->area), gc, fg );
	    break;
	default:	/* implement color map later */
	    break;
    }
}


/*
 * Memory utilities
 */

#ifdef REALLOC
#undef REALLOC
#endif

#define REALLOC( ptr, size )	\
	((ptr == NULL) ? malloc(size) : realloc(ptr,size))


/* X windows routines.
 * We currently create a single, top-level shell.  In that is a
 * single drawing area into which we will draw text.  This allows
 * us to put multi-color (and font, but we'll never build that) text
 * into the drawing area.  In the future, we'll add scrollbars to the
 * drawing areas
 */

void	select_start();
void	select_extend();
void	select_paste();
void	key_press();
void	insert_string();
void	beep __P((Widget w));
void	find();
void	command();

static XtActionsRec	area_actions[] = {
    { "select_start",	select_start	},
    { "select_extend",	select_extend	},
    { "select_paste",	select_paste	},
    { "key_press",	key_press	},
    { "insert_string",	insert_string	},
    { "beep",		beep		},
    { "find",		find		},
    { "command",	command		},
};

char	areaTrans[] =
    "<Btn1Down>:	select_start()		\n\
     <Btn1Motion>:	select_extend()		\n\
     <Btn2Down>:	select_paste()		\n\
     <Btn3Down>:	select_extend()		\n\
     <Btn3Motion>:	select_extend()		\n\
     <Key>End:		command(VI_C_BOTTOM)	\n\
     <Key>Escape:	command(EINSERT)	\n\
     <Key>Find:		find()			\n\
     <Key>Home:		command(VI_C_TOP)	\n\
     <Key>Next:		command(VI_C_PGDOWN)	\n\
     <Key>Prior:	command(VI_C_PGUP)	\n\
     <Key>osfBackSpace:	command(VI_C_LEFT)	\n\
     <Key>osfBeginLine:	command(VI_C_BOL)	\n\
     <Key>osfCopy:	beep()			\n\
     <Key>osfCut:	beep()			\n\
     <Key>osfDelete:	command(VI_C_DEL)	\n\
     <Key>osfDown:	command(VI_C_DOWN)	\n\
     <Key>osfEndLine:	command(VI_C_EOL)	\n\
     <Key>osfInsert:	command(VI_C_INSERT)	\n\
     <Key>osfLeft:	command(VI_C_LEFT)	\n\
     <Key>osfPageDown:	command(VI_C_PGDOWN)	\n\
     <Key>osfPageUp:	command(VI_C_PGUP)	\n\
     <Key>osfPaste:	insert_string(p)	\n\
     <Key>osfRight:	command(VI_C_RIGHT)	\n\
     <Key>osfUndo:	command(VI_UNDO)	\n\
     <Key>osfUp:	command(VI_C_UP)	\n\
     Ctrl<Key>C:	command(VI_INTERRUPT)	\n\
     <Key>:		key_press()";


static  XutResource resource[] = {
    { "font",		XutRKfont,	&font		},
    { "pointerShape",	XutRKcursor,	&std_cursor	},
    { "busyShape",	XutRKcursor,	&busy_cursor	},
};


/*
 * vi_input_func --
 *	We've received input on the pipe from vi.
 *
 * PUBLIC: void vi_input_func __P((XtPointer, int *, XtInputId *));
 */
void
vi_input_func(XtPointer client_data, int *source, XtInputId *id)
{
	/* Parse and dispatch on commands in the queue. */
	(void)ipvi_motif->input(ipvi_motif, *source);

#ifdef notdef
	/* Check the pipe for unused events when not busy. */
	XtAppAddWorkProc(ctx, process_pipe_input, NULL);
#endif
}



/* Send the window size. */
#if defined(__STDC__)
static	void	send_resize( xvi_screen *this_screen )
#else
static	void	send_resize( this_screen )
xvi_screen	*this_screen;
#endif
{
    IP_BUF	ipb;

    ipb.val1 = this_screen->rows;
    ipb.val2 = this_screen->cols;
    ipb.code = VI_RESIZE;

#ifdef TRACE
    vtrace("resize_func ( %d x %d )\n", this_screen->rows, this_screen->cols);
#endif

    /* send up the pipe */
    vi_send(vi_ofd, "12", &ipb);
}


#if defined(__STDC__)
static	void	resize_backing_store( xvi_screen *this_screen )
#else
static	void	resize_backing_store( this_screen )
xvi_screen	*this_screen;
#endif
{
    int	total_chars = this_screen->rows * this_screen->cols;

    this_screen->characters	= REALLOC( this_screen->characters,
					   total_chars
					   );
    memset( this_screen->characters, ' ', total_chars );

    this_screen->flags		= REALLOC( this_screen->flags,
					   total_chars
					   );
    memset( this_screen->flags, 0, total_chars );
}



/* X will call this when we are resized */
#if defined(__STDC__)
static	void	resize_func( Widget wid,
			     XtPointer client_data,
			     XtPointer call_data
			     )
#else
static	void	resize_func( wid, client_data, call_data )
Widget		wid;
XtPointer	client_data;
XtPointer	call_data;
#endif
{
    xvi_screen			*this_screen = (xvi_screen *) client_data;
    Dimension			height, width;

    XtVaGetValues( wid, XmNheight, &height, XmNwidth, &width, 0 );

    /* generate correct sizes when we have font metrics implemented */
    this_screen->cols = width / this_screen->ch_width;
    this_screen->rows = height / this_screen->ch_height;

    resize_backing_store( this_screen );
    send_resize( this_screen );
}


/*
 * __vi_draw_text --
 *	Draw from backing store.
 *
 * PUBLIC: void	__vi_draw_text __P((xvi_screen *, int, int, int));
 */
void
__vi_draw_text(xvi_screen *this_screen, int row, int start_col, int len)
{
    int		col, color, xpos;
    char	*start, *end;

    start = CharAt( __vi_screen, row, start_col );
    color = *FlagAt( __vi_screen, row, start_col );
    xpos  = XPOS( __vi_screen, start_col );

    /* one column at a time */
    for ( col=start_col;
	  col<this_screen->cols && col<start_col+len;
	  col++ ) {

	/* has the color changed? */
	if ( *FlagAt( __vi_screen, row, col ) == color )
	    continue;

	/* is there anything to write? */
	end  = CharAt( __vi_screen, row, col );
	if ( end == start )
	    continue;

	/* yes. write in the previous color */
	set_gc_colors( __vi_screen, color );

	/* add to display */
	XDrawImageString( XtDisplay(__vi_screen->area),
			  XtWindow(__vi_screen->area),
			  gc,
			  xpos,
			  YPOS( __vi_screen, row ),
			  start,
			  end - start
			  );

	/* this is the new context */
	color = *FlagAt( __vi_screen, row, col );
	xpos  = XPOS( __vi_screen, col );
	start = end;
    }

    /* is there anything to write? */
    end = CharAt( __vi_screen, row, col );
    if ( end != start ) {
	/* yes. write in the previous color */
	set_gc_colors( __vi_screen, color );

	/* add to display */
	XDrawImageString( XtDisplay(__vi_screen->area),
			  XtWindow(__vi_screen->area),
			  gc,
			  xpos,
			  YPOS( __vi_screen, row ),
			  start,
			  end - start
			  );
    }
}


/* set clipping rectangles accordingly */
#if defined(__STDC__)
static	void	add_to_clip( xvi_screen *cur_screen, int x, int y, int width, int height )
#else
static	void	add_to_clip( cur_screen, x, y, width, height )
	xvi_screen *cur_screen;
	int	x;
	int	y;
	int	width;
	int	height;
#endif
{
    XRectangle	rect;
    rect.x	= x;
    rect.y	= y;
    rect.height	= height;
    rect.width	= width;
    if ( cur_screen->clip == NULL )
	cur_screen->clip = XCreateRegion();
    XUnionRectWithRegion( &rect, cur_screen->clip, cur_screen->clip );
}


/*
 * __vi_expose_func --
 *	Redraw the window's contents.
 *
 * NOTE: When vi wants to force a redraw, we are called with NULL widget
 *	 and call_data.
 *
 * PUBLIC: void	__vi_expose_func __P((Widget, XtPointer, XtPointer));
 */
void
__vi_expose_func(Widget wid, XtPointer client_data, XtPointer call_data)
{
    xvi_screen			*this_screen;
    XmDrawingAreaCallbackStruct	*cbs;
    XExposeEvent		*xev;
    XGraphicsExposeEvent	*gev;
    int				row;

    /* convert pointers */
    this_screen = (xvi_screen *) client_data;
    cbs		= (XmDrawingAreaCallbackStruct *) call_data;

    /* first exposure? tell vi we are ready... */
    if ( this_screen->init == False ) {

	/* what does the user want to see? */
	__vi_set_cursor( __vi_screen, False );

	/* vi wants a resize as the first event */
	send_resize( __vi_screen );

	/* fine for now.  we'll be back */
	this_screen->init = True;
	return;
    }

    if ( call_data == NULL ) {

	/* vi core calls this when it wants a full refresh */
#ifdef TRACE
	vtrace("expose_func:  full refresh\n");
#endif

	XClearWindow( XtDisplay(this_screen->area),
		      XtWindow(this_screen->area)
		      );
    }
    else {
	switch ( cbs->event->type ) {

	    case GraphicsExpose:
		gev = (XGraphicsExposeEvent *) cbs->event;

		/* set clipping rectangles accordingly */
		add_to_clip( this_screen,
			     gev->x, gev->y,
			     gev->width, gev->height
			     );

		/* X calls here when XCopyArea exposes new bits */
#ifdef TRACE
		vtrace("expose_func (X):  (x=%d,y=%d,w=%d,h=%d), count=%d\n",
			     gev->x, gev->y,
			     gev->width, gev->height,
			     gev->count);
#endif

		/* more coming?  do it then */
		if ( gev->count > 0 ) return;

		/* set clipping region */
		XSetRegion( XtDisplay(wid), gc, this_screen->clip );
		break;

	    case Expose:
		xev = (XExposeEvent *) cbs->event;

		/* set clipping rectangles accordingly */
		add_to_clip( this_screen,
			     xev->x, xev->y,
			     xev->width, xev->height
			     );

		/* Motif calls here when DrawingArea is exposed */
#ifdef TRACE
		vtrace("expose_func (Motif): (x=%d,y=%d,w=%d,h=%d), count=%d\n",
			     xev->x, xev->y,
			     xev->width, xev->height,
			     xev->count);
#endif

		/* more coming?  do it then */
		if ( xev->count > 0 ) return;

		/* set clipping region */
		XSetRegion( XtDisplay(wid), gc, this_screen->clip );
		break;

	    default:
		/* don't care? */
		return;
	}
    }

    /* one row at a time */
    for (row=0; row<this_screen->rows; row++) {

	/* draw from the backing store */
	__vi_draw_text( this_screen, row, 0, this_screen->cols );
    }

    /* clear clipping region */
    XSetClipMask( XtDisplay(this_screen->area), gc, None );
    if ( this_screen->clip != NULL ) {
	XDestroyRegion( this_screen->clip );
	this_screen->clip = NULL;
    }

}


#if defined(__STDC__)
static void	xexpose	( Widget w,
			  XtPointer client_data,
			  XEvent *ev,
			  Boolean *cont
			  )
#else
static void	xexpose	( w, client_data, ev, cont )
Widget		w;
XtPointer	client_data;
XEvent		*ev;
Boolean		*cont;
#endif
{
    XmDrawingAreaCallbackStruct	cbs;

    switch ( ev->type ) {
	case GraphicsExpose:
	    cbs.event	= ev;
	    cbs.window	= XtWindow(w);
	    cbs.reason	= XmCR_EXPOSE;
	    __vi_expose_func( w, client_data, (XtPointer) &cbs );
	    *cont	= False;	/* we took care of it */
	    break;
	default:
	    /* don't care */
	    break;
    }
}


/* unimplemented keystroke or command */
#if defined(__STDC__)
static void	beep( Widget w )
#else
static void	beep( w )
Widget	w;
#endif
{
    XBell(XtDisplay(w),0);
}


/* give me a search dialog */
#if defined(__STDC__)
static void	find( Widget w )
#else
static void	find( w )
Widget	w;
#endif
{
    __vi_show_search_dialog( w, "Find" );
}

/*
 * command --
 *	Translate simple keyboard input into vi protocol commands.
 */
static	void
command(Widget widget, XKeyEvent *event, String *str, Cardinal *cardinal)
{
	static struct {
		String	name;
		int	code;
		int	count;
	} table[] = {
		{ "VI_C_BOL",		VI_C_BOL,	0 },
		{ "VI_C_BOTTOM",	VI_C_BOTTOM,	0 },
		{ "VI_C_DEL",		VI_C_DEL,	0 },
		{ "VI_C_DOWN",		VI_C_DOWN,	1 },
		{ "VI_C_EOL",		VI_C_EOL,	0 },
		{ "VI_C_INSERT",	VI_C_INSERT,	0 },
		{ "VI_C_LEFT",		VI_C_LEFT,	0 },
		{ "VI_C_PGDOWN",	VI_C_PGDOWN,	1 },
		{ "VI_C_PGUP",		VI_C_PGUP,	1 },
		{ "VI_C_RIGHT",		VI_C_RIGHT,	0 },
		{ "VI_C_TOP",		VI_C_TOP,	0 },
		{ "VI_C_UP",		VI_C_UP,	1 },
		{ "VI_INTERRUPT",	VI_INTERRUPT,	0 },
	};
	IP_BUF ipb;
	int i;

	/*
	 * XXX
	 * Do fast lookup based on character #6 -- sleazy, but I don't
	 * want to do 10 strcmp's per keystroke.
	 */
	ipb.val1 = 1;
	for (i = 0; i < XtNumber(table); i++)
		if (table[i].name[6] == (*str)[6] &&
		    strcmp(table[i].name, *str) == 0) {
			ipb.code = table[i].code;
			vi_send(vi_ofd, table[i].count ? "1" : NULL, &ipb);
			return;
		}

	/* oops. */
	beep(widget);
}

/* mouse or keyboard input. */
#if defined(__STDC__)
static	void	insert_string( Widget widget, 
			       XKeyEvent *event, 
			       String *str, 
			       Cardinal *cardinal
			       )
#else
static	void	insert_string( widget, event, str, cardinal )
Widget          widget; 
XKeyEvent       *event; 
String          *str;  
Cardinal        *cardinal;
#endif
{
    IP_BUF	ipb;

    ipb.len1 = strlen( *str );
    if ( ipb.len1 != 0 ) {
	ipb.code = VI_STRING;
	ipb.str1 = *str;
	vi_send(vi_ofd, "a", &ipb);
    }

#ifdef TRACE
    vtrace("insert_string {%.*s}\n", strlen( *str ), *str );
#endif
}


/* mouse or keyboard input. */
#if defined(__STDC__)
static	void	key_press( Widget widget, 
			   XKeyEvent *event, 
			   String str, 
			   Cardinal *cardinal
			   )
#else
static	void	key_press( widget, event, str, cardinal )
Widget          widget; 
XKeyEvent       *event; 
String          str;  
Cardinal        *cardinal;
#endif
{
    IP_BUF	ipb;
    char	bp[BufferSize];

    ipb.len1 = XLookupString( event, bp, BufferSize, NULL, NULL );
    if ( ipb.len1 != 0 ) {
	ipb.code = VI_STRING;
	ipb.str1 = bp;
#ifdef TRACE
	vtrace("key_press {%.*s}\n", ipb.len1, bp );
#endif
	vi_send(vi_ofd, "a", &ipb);
    }

}


#if defined(__STDC__)
static	void	scrollbar_moved( Widget widget,
				 XtPointer ptr,
				 XmScrollBarCallbackStruct *cbs
				 )
#else
static	void				scrollbar_moved( widget, ptr, cbs )
	Widget				widget;
	XtPointer			ptr;
	XmScrollBarCallbackStruct	*cbs;
#endif
{
    /* Future:  Need to scroll the correct screen! */
    xvi_screen	*cur_screen = (xvi_screen *) ptr;
    IP_BUF	ipb;

    /* if we are still processing messages from core, skip this event
     * (see comments near __vi_set_scroll_block())
     */
    if ( scroll_block ) {
	return;
    }
    __vi_set_scroll_block();

#ifdef TRACE
    switch ( cbs->reason ) {
	case XmCR_VALUE_CHANGED:
	    vtrace( "scrollbar VALUE_CHANGED %d\n", cbs->value );
	    break;
	case XmCR_DRAG:
	    vtrace( "scrollbar DRAG %d\n", cbs->value );
	    break;
	default:
	    vtrace( "scrollbar <default> %d\n", cbs->value );
	    break;
    }
    vtrace("scrollto {%d}\n", cbs->value );
#endif

    /* Send the new cursor position. */
    ipb.code = VI_C_SETTOP;
    ipb.val1 = cbs->value;
    (void)vi_send(vi_ofd, "1", &ipb);
}


#if defined(__STDC__)
static	xvi_screen	*create_screen( Widget parent, int rows, int cols )
#else
static	xvi_screen	*create_screen( parent, rows, cols )
	Widget		parent;
	int		rows, cols;
#endif
{
    xvi_screen	*new_screen = (xvi_screen *) calloc( 1, sizeof(xvi_screen) );
    Widget	frame;

    /* init... */
    new_screen->color		= COLOR_STANDARD;
    new_screen->parent		= parent;

    /* figure out the sizes */
    new_screen->rows		= rows;
    new_screen->cols		= cols;
    new_screen->ch_width	= font->max_bounds.width;
    new_screen->ch_height	= font->descent + font->ascent;
    new_screen->ch_descent	= font->descent;
    new_screen->clip		= NULL;

    /* allocate and init the backing stores */
    resize_backing_store( new_screen );

    /* set up a translation table for the X toolkit */
    if ( area_trans == NULL ) 
	area_trans = XtParseTranslationTable(areaTrans);

    /* future, new screen gets inserted into the parent sash
     * immediately after the current screen.  Default Pane action is
     * to add it to the end
     */

    /* use a form to hold the drawing area and the scrollbar */
    new_screen->form = XtVaCreateManagedWidget( "form",
	    xmFormWidgetClass,
	    parent,
	    XmNpaneMinimum,		2*new_screen->ch_height,
	    XmNallowResize,		True,
	    NULL
	    );

    /* create a scrollbar. */
    new_screen->scroll = XtVaCreateManagedWidget( "scroll",
	    xmScrollBarWidgetClass,
	    new_screen->form,
	    XmNtopAttachment,		XmATTACH_FORM,
	    XmNbottomAttachment,	XmATTACH_FORM,
	    XmNrightAttachment,		XmATTACH_FORM,
	    XmNminimum,			1,
	    XmNmaximum,			2,
	    XmNsliderSize,		1,
	    NULL
	    );
    XtAddCallback( new_screen->scroll,
		   XmNvalueChangedCallback,
		   scrollbar_moved,
		   new_screen
		   );
    XtAddCallback( new_screen->scroll,
		   XmNdragCallback,
		   scrollbar_moved,
		   new_screen
		   );

    /* create a frame because they look nice */
    frame = XtVaCreateManagedWidget( "frame",
	    xmFrameWidgetClass,
	    new_screen->form,
	    XmNshadowType,		XmSHADOW_ETCHED_IN,
	    XmNtopAttachment,		XmATTACH_FORM,
	    XmNbottomAttachment,	XmATTACH_FORM,
	    XmNleftAttachment,		XmATTACH_FORM,
	    XmNrightAttachment,		XmATTACH_WIDGET,
	    XmNrightWidget,		new_screen->scroll,
	    NULL
	    );

    /* create a drawing area into which we will put text */
    new_screen->area = XtVaCreateManagedWidget( "screen",
	    xmDrawingAreaWidgetClass,
	    frame,
	    XmNheight,		new_screen->ch_height * new_screen->rows,
	    XmNwidth,		new_screen->ch_width * new_screen->cols,
	    XmNtranslations,	area_trans,
	    XmNuserData,	new_screen,
	    XmNnavigationType,	XmNONE,
	    XmNtraversalOn,	False,
	    NULL
	    );

    /* this callback is for when the drawing area is resized */
    XtAddCallback( new_screen->area,
		   XmNresizeCallback,
		   resize_func,
		   new_screen
		   );

    /* this callback is for when the drawing area is exposed */
    XtAddCallback( new_screen->area,
		   XmNexposeCallback,
		   __vi_expose_func,
		   new_screen
		   );

    /* this callback is for when we expose obscured bits 
     * (e.g. there is a window over part of our drawing area
     */
    XtAddEventHandler( new_screen->area,
		       0,	/* no standard events */
		       True,	/* we *WANT* GraphicsExpose */
		       xexpose,	/* what to do */
		       new_screen
		       );

    return new_screen;
}


static	xvi_screen	*split_screen(void)
{
    Cardinal	num;
    WidgetList	c;
    int		rows = __vi_screen->rows / 2;
    xvi_screen	*new_screen;

    /* Note that (global) cur_screen needs to be correctly set so that
     * insert_here knows which screen to put the new one after
     */
    new_screen = create_screen( __vi_screen->parent,
				rows,
				__vi_screen->cols
				);

    /* what are the screens? */
    XtVaGetValues( __vi_screen->parent,
		   XmNnumChildren,	&num,
		   XmNchildren,		&c,
		   NULL
		   );

    /* unmanage all children in preparation for resizing */
    XtUnmanageChildren( c, num );

    /* force resize of the affected screens */
    XtVaSetValues( new_screen->form,
		   XmNheight,	new_screen->ch_height * rows,
		   NULL
		   );
    XtVaSetValues( __vi_screen->form,
		   XmNheight,	__vi_screen->ch_height * rows,
		   NULL
		   );

    /* re-manage */
    XtManageChildren( c, num );

    /* done */
    return new_screen;
}


/* Tell me where to insert the next subpane */
#if defined(__STDC__)
static	Cardinal	insert_here( Widget wid )
#else
static	Cardinal	insert_here( wid )
	Widget		wid;
#endif
{
    Cardinal	i, num;
    WidgetList	c;

    XtVaGetValues( XtParent(wid),
		   XmNnumChildren,	&num,
		   XmNchildren,		&c,
		   NULL
		   );

    /* The  default  XmNinsertPosition  procedure  for  PanedWindow
     * causes sashes to be inserted at the end of the list of children
     * and causes non-sash widgets to be inserted after  other
     * non-sash children but before any sashes.
     */
    if ( ! XmIsForm( wid ) )
	return num;

    /* We will put the widget after the one with the current screen */
    for (i=0; i<num && XmIsForm(c[i]); i++) {
	if ( __vi_screen == NULL || __vi_screen->form == c[i] )
	    return i+1;	/* after the i-th */
    }

    /* could not find it?  this should never happen */
    return num;
}


/*
 * vi_create_editor --
 *	Create the necessary widgetry.
 *
 * PUBLIC: Widget vi_create_editor __P((String, Widget, void (*)(void)));
 */
Widget
vi_create_editor(String name, Widget parent, void (*exitp) (void))
{
    Widget	pane_w;
    Display	*display = XtDisplay( parent );

    __vi_exitp = exitp;

    /* first time through? */
    if ( ctx == NULL ) {

	/* save this for later */
	ctx = XtWidgetToApplicationContext( parent );

	/* add our own special actions */
	XtAppAddActions( ctx, area_actions, XtNumber(area_actions) );

	/* how long is double-click? */
	multi_click_length = XtGetMultiClickTime( display );

	/* check the resource database for interesting resources */
	__XutConvertResources( parent,
			     vi_progname,
			     resource,
			     XtNumber(resource)
			     );

	/* we need a context for moving bits around in the windows */
	__vi_copy_gc = XCreateGC( display,
				 DefaultRootWindow(display),
				 0,
				 0
				 );

	/* routines for inter client communications conventions */
	__vi_InitCopyPaste( f_copy, f_paste, f_clear, fprintf );
    }

    /* create the paned window */
    pane_w = XtVaCreateManagedWidget( "pane",
				      xmPanedWindowWidgetClass,
				      parent,
				      XmNinsertPosition,	insert_here,
				      NULL
				      );

    /* allocate our data structure.  in the future we will have several
     * screens running around at the same time
     */
    __vi_screen = create_screen( pane_w, 24, 80 );

    /* force creation of our color text context */
    set_gc_colors( __vi_screen, COLOR_STANDARD );

    /* done */
    return pane_w;
}


/* These routines deal with the selection buffer */

static	int	selection_start, selection_end, selection_anchor;
static	enum	select_enum {
	    select_char, select_word, select_line
	}	select_type = select_char;
static	int	last_click;

static	char	*clipboard = NULL;
static	int	clipboard_size = 0,
		clipboard_length;


#if defined(__STDC__)
static	void	copy_to_clipboard( xvi_screen *cur_screen )
#else
static	void	copy_to_clipboard( cur_screen )
xvi_screen	*cur_screen;
#endif
{
    /* for now, copy from the backing store.  in the future,
     * vi core will tell us exactly what the selection buffer contains
     */
    clipboard_length = 1 + selection_end - selection_start;

    if ( clipboard == NULL )
	clipboard = (char *) malloc( clipboard_length );
    else if ( clipboard_size < clipboard_length )
	clipboard = (char *) realloc( clipboard, clipboard_length );

    memcpy( clipboard,
	    cur_screen->characters + selection_start,
	    clipboard_length
	    );
}


#if defined(__STDC__)
static	void	mark_selection( xvi_screen *cur_screen, int start, int end )
#else
static	void	mark_selection( cur_screen, start, end )
xvi_screen	*cur_screen;
int		start;
int		end;
#endif
{
    int	row, col, i;

    for ( i=start; i<=end; i++ ) {
	if ( !( cur_screen->flags[i] & COLOR_SELECT ) ) {
	    cur_screen->flags[i] |= COLOR_SELECT;
	    ToRowCol( cur_screen, i, row, col );
	    __vi_draw_text( cur_screen, row, col, 1 );
	}
    }
}


#if defined(__STDC__)
static	void	erase_selection( xvi_screen *cur_screen, int start, int end )
#else
static	void	erase_selection( cur_screen, start, end )
xvi_screen	*cur_screen;
int		start;
int		end;
#endif
{
    int	row, col, i;

    for ( i=start; i<=end; i++ ) {
	if ( cur_screen->flags[i] & COLOR_SELECT ) {
	    cur_screen->flags[i] &= ~COLOR_SELECT;
	    ToRowCol( cur_screen, i, row, col );
	    __vi_draw_text( cur_screen, row, col, 1 );
	}
    }
}


#if defined(__STDC__)
static	void	left_expand_selection( xvi_screen *cur_screen, int *start )
#else
static	void	left_expand_selection( cur_screen, start )
xvi_screen	*cur_screen;
int		*start;
#endif
{
    int row, col;

    switch ( select_type ) {
	case select_word:
	    if ( *start == 0 || isspace( (unsigned char)cur_screen->characters[*start] ) )
		return;
	    for (;;) {
		if ( isspace( (unsigned char)cur_screen->characters[*start-1] ) )
		    return;
		if ( --(*start) == 0 )
		   return;
	    }
	case select_line:
	    ToRowCol( cur_screen, *start, row, col );
	    col = 0;
	    *start = Linear( cur_screen, row, col );
	    break;
    }
}


#if defined(__STDC__)
static	void	right_expand_selection( xvi_screen *cur_screen, int *end )
#else
static	void	right_expand_selection( cur_screen, end )
xvi_screen	*cur_screen;
int		*end;
#endif
{
    int row, col, last = cur_screen->cols * cur_screen->rows - 1;

    switch ( select_type ) {
	case select_word:
	    if ( *end == last || isspace( (unsigned char)cur_screen->characters[*end] ) )
		return;
	    for (;;) {
		if ( isspace( (unsigned char)cur_screen->characters[*end+1] ) )
		    return;
		if ( ++(*end) == last )
		   return;
	    }
	case select_line:
	    ToRowCol( cur_screen, *end, row, col );
	    col = cur_screen->cols -1;
	    *end = Linear( cur_screen, row, col );
	    break;
    }
}


#if defined(__STDC__)
static	void	select_start( Widget widget, 
			      XEvent *event,
			      String str, 
			      Cardinal *cardinal
			      )
#else
static	void	select_start( widget, event, str, cardinal )
Widget		widget;   
XEvent		*event;
String		str; 
Cardinal        *cardinal;
#endif
{
    IP_BUF		ipb;
    int			xpos, ypos;
    XPointerMovedEvent	*ev = (XPointerMovedEvent *) event;
    static int		last_click;

    /*
     * NOTE: when multiple panes are implemented, we need to find the correct
     * screen.  For now, there is only one.
     */
    xpos = COLUMN( __vi_screen, ev->x );
    ypos = ROW( __vi_screen, ev->y );

    /* Remove the old one. */
    erase_selection( __vi_screen, selection_start, selection_end );

    /* Send the new cursor position. */
    ipb.code = VI_MOUSE_MOVE;
    ipb.val1 = ypos;
    ipb.val2 = xpos;
    (void)vi_send(vi_ofd, "12", &ipb);

    /* click-click, and we go for words, lines, etc */
    if ( ev->time - last_click < multi_click_length )
	select_type = (enum select_enum) ((((int)select_type)+1)%3);
    else
	select_type = select_char;
    last_click = ev->time;

    /* put the selection here */
    selection_anchor	= Linear( __vi_screen, ypos, xpos );
    selection_start	= selection_anchor;
    selection_end	= selection_anchor;

    /* expand to include words, line, etc */
    left_expand_selection( __vi_screen, &selection_start );
    right_expand_selection( __vi_screen, &selection_end );

    /* draw the new one */
    mark_selection( __vi_screen, selection_start, selection_end );

    /* and tell the window manager we own the selection */
    if ( select_type != select_char ) {
	__vi_AcquirePrimary( widget );
	copy_to_clipboard( __vi_screen );
    }
}


#if defined(__STDC__)
static	void	select_extend( Widget widget, 
			       XEvent *event,
			       String str, 
			       Cardinal *cardinal
			       )
#else
static	void	select_extend( widget, event, str, cardinal )
Widget		widget;   
XEvent		*event;
String		str; 
Cardinal        *cardinal;
#endif
{
    int			xpos, ypos, pos;
    XPointerMovedEvent	*ev = (XPointerMovedEvent *) event;

    /* NOTE:  when multiple panes are implemented, we need to find
     * the correct screen.  For now, there is only one.
     */
    xpos = COLUMN( __vi_screen, ev->x );
    ypos = ROW( __vi_screen, ev->y );

    /* deal with words, lines, etc */
    pos = Linear( __vi_screen, ypos, xpos );
    if ( pos < selection_anchor )
	left_expand_selection( __vi_screen, &pos );
    else
	right_expand_selection( __vi_screen, &pos );

    /* extend from before the start? */
    if ( pos < selection_start ) {
	mark_selection( __vi_screen, pos, selection_start-1 );
	selection_start = pos;
    }

    /* extend past the end? */
    else if ( pos > selection_end ) {
	mark_selection( __vi_screen, selection_end+1, pos );
	selection_end = pos;
    }

    /* between the anchor and the start? */
    else if ( pos < selection_anchor ) {
	erase_selection( __vi_screen, selection_start, pos-1 );
	selection_start = pos;
    }

    /* between the anchor and the end? */
    else {
	erase_selection( __vi_screen, pos+1, selection_end );
	selection_end = pos;
    }

    /* and tell the window manager we own the selection */
    __vi_AcquirePrimary( widget );
    copy_to_clipboard( __vi_screen );
}


#if defined(__STDC__)
static	void	select_paste( Widget widget, 
			      XEvent *event,
			      String str, 
			      Cardinal *cardinal
			      )
#else
static	void	select_paste( widget, event, str, cardinal )
Widget		widget;   
XEvent		*event;
String		str; 
Cardinal        *cardinal;
#endif
{
    __vi_PasteFromClipboard( widget );
}


/* Interface to copy and paste
 * (a) callbacks from the window manager
 *	f_copy	-	it wants our buffer
 *	f_paste	-	it wants us to paste some text
 *	f_clear	-	we've lost the selection, clear it
 */

#if defined(__STDC__)
static	void	f_copy( String *buffer, int *len )
#else
static	void	f_copy( buffer, len )
	String	*buffer;
	int	*len;
#endif
{
#ifdef TRACE
    vtrace("f_copy() called");
#endif
    *buffer	= clipboard;
    *len	= clipboard_length;
}



static	void	f_paste(int widget, int buffer, int length)
{
    /* NOTE:  when multiple panes are implemented, we need to find
     * the correct screen.  For now, there is only one.
     */
#ifdef TRACE
    vtrace("f_paste() called with '%*.*s'\n", length, length, buffer);
#endif
}


#if defined(__STDC__)
static	void	f_clear( Widget widget )
#else
static	void	f_clear( widget )
Widget	widget;
#endif
{
    xvi_screen	*cur_screen;

#ifdef TRACE
    vtrace("f_clear() called");
#endif

    XtVaGetValues( widget, XmNuserData, &cur_screen, 0 );

    erase_selection( cur_screen, selection_start, selection_end );
}


/*
 * These routines deal with the cursor.
 *
 * PUBLIC: void __vi_set_cursor __P((xvi_screen *, int));
 */
void
__vi_set_cursor(xvi_screen *cur_screen, int is_busy)
{
    XDefineCursor( XtDisplay(cur_screen->area),
		   XtWindow(cur_screen->area),
		   (is_busy) ? busy_cursor : std_cursor
		   );
}



/* hooks for the tags widget */

static	String	cur_word = NULL;

/*
 * PUBLIC: void __vi_set_word_at_caret __P((xvi_screen *));
 */
void
__vi_set_word_at_caret(xvi_screen *this_screen)
{
    char	*start, *end, save;
    int		newx, newy;

    newx = this_screen->curx;
    newy = this_screen->cury;

    /* Note that this really ought to be done by core due to wrapping issues */
    for ( end = start = CharAt( this_screen, newy, newx );
	  (isalnum( (unsigned char)*end ) || *end == '_') && (newx < this_screen->cols);
	  end++, newx++
	  );
    save = *end;
    *end = '\0';
    if ( cur_word != NULL ) free( cur_word );
    cur_word = strdup( start );
    *end = save;

    /* if the tag stack widget is active, set the text field there
     * to agree with the current caret position.
     */
    __vi_set_tag_text( cur_word );
}


String	__vi_get_word_at_caret(xvi_screen *this_screen)
{
    return (cur_word) ? cur_word : "";
}


/*
 * These routines deal with the caret.
 *
 * PUBLIC: void draw_caret __P((xvi_screen *));
 */
static void
draw_caret(xvi_screen *this_screen)
{
    /* draw the caret by drawing the text in highlight color */
    *FlagAt( this_screen, this_screen->cury, this_screen->curx ) |= COLOR_CARET;
    __vi_draw_text( this_screen, this_screen->cury, this_screen->curx, 1 );
}

/*
 * PUBLIC: void __vi_erase_caret __P((xvi_screen *));
 */
void
__vi_erase_caret(xvi_screen *this_screen)
{
    /* erase the caret by drawing the text in normal video */
    *FlagAt( this_screen, this_screen->cury, this_screen->curx ) &= ~COLOR_CARET;
    __vi_draw_text( this_screen, this_screen->cury, this_screen->curx, 1 );
}

/*
 * PUBLIC: void	__vi_move_caret __P((xvi_screen *, int, int));
 */
void
__vi_move_caret(xvi_screen *this_screen, int newy, int newx)
{
    /* remove the old caret */
    __vi_erase_caret( this_screen );

    /* caret is now here */
    this_screen->curx = newx;
    this_screen->cury = newy;
    draw_caret( this_screen );
}
