/*	$NetBSD: m_ruler.c,v 1.1.1.2 2008/05/18 14:31:28 aymeric Exp $ */

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
static const char sccsid[] = "Id: m_ruler.c,v 8.6 2003/11/05 17:10:00 skimo Exp (Berkeley) Date: 2003/11/05 17:10:00";
#endif /* not lint */

/* This module implements a dialog for the text ruler
 *
 * Interface:
 * void	__vi_show_text_ruler_dialog( Widget parent, String title )
 *	Pops up a text ruler dialog.
 *	We allow one per session.  It is not modal.
 *
 * void	__vi_clear_text_ruler_dialog( Widget parent, String title )
 *	Pops down the text ruler dialog.
 *
 * void	__vi_set_text_ruler( int row, int col )
 *	Changes the displayed position
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <X11/X.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/DrawingA.h>
#include <Xm/RowColumn.h>
#include <Xm/PushBG.h>

#include <bitstring.h>
#include <stdio.h>

#undef LOCK_SUCCESS
#include "../common/common.h"
#include "../ipc/ip.h"
#include "m_motif.h"
#include "vi_mextern.h"


/* globals */

static	Widget		db_ruler = NULL;

static	Boolean		active = False;

static	int		ruler_border = 5,
			ruler_asc;

static	GC		gc_ruler;

static	XFontStruct	*ruler_font;

static	char		text[256];

#if ! defined(SelfTest)
static  XutResource resource[] = {
    { 	"rulerFont",	XutRKfont,	&ruler_font	},
    { 	"rulerBorder",	XutRKinteger,	&ruler_border	},
};
#endif


/* change the displayed position */

static void
set_ruler_text(int row, int col, int *h, int *w, int *asc)
{
    int		dir, des;
    XCharStruct	over;

    /* format the data */
    sprintf( text, "%9.d,%-9.d", row+1, col+1 );

    /* how big will it be? */
    XTextExtents( ruler_font, text, strlen(text), &dir, asc, &des, &over );

    /* how big a window will we need? */
    *h = 2*ruler_border + over.ascent + over.descent;
    *w = 2*ruler_border + over.width;
}


static void
redraw_text(void)
{
    XClearArea( XtDisplay(db_ruler), XtWindow(db_ruler), 0, 0, 0, 0, False );
    XDrawString( XtDisplay(db_ruler),
		 XtWindow(db_ruler),
		 gc_ruler,
		 ruler_border, ruler_border + ruler_asc,
		 text,
		 strlen(text)
		 );
}


/*
 * PUBLIC: void __vi_set_text_ruler __P((int, int));
 */
void
__vi_set_text_ruler(int row, int col)
{
    int h, w;

    if ( ! active ) return;

    set_ruler_text( row, col, &h, &w, &ruler_asc );

    redraw_text();
}


/* callbacks */

static void
cancel_cb(void)
{
#if defined(SelfTest)
    puts( "cancelled" );
#endif
    active = False;
}


static	void destroyed(void)
{
#if defined(SelfTest)
    puts( "destroyed" );
#endif

    /* some window managers destroy us upon popdown */
    db_ruler = NULL;
    active   = False;
}



/* Draw and display a dialog the describes nvi options */

#if defined(__STDC__)
static	Widget	create_text_ruler_dialog( Widget parent, String title )
#else
static	Widget	create_text_ruler_dialog( parent, title )
	Widget	parent;
	String	title;
#endif
{
    Widget	box;
    int		h, w, asc;
    Pixel	fg, bg;

    /* already built? */
    if ( db_ruler != NULL ) return db_ruler;

#if defined(SelfTest)
    ruler_font = XLoadQueryFont( XtDisplay(parent), "9x15" );
#else
    /* check the resource database for interesting resources */
    __XutConvertResources( parent,
			 vi_progname,
			 resource,
			 XtNumber(resource)
			 );
#endif

    gc_ruler = XCreateGC( XtDisplay(parent), XtWindow(parent), 0, NULL );
    XSetFont( XtDisplay(parent), gc_ruler, ruler_font->fid );

    box = XtVaCreatePopupShell( title,
				transientShellWidgetClass,
				parent,
				XmNallowShellResize,	False,
				0
				);
    XtAddCallback( box, XmNpopdownCallback, cancel_cb, 0 );
    XtAddCallback( box, XmNdestroyCallback, destroyed, 0 );

    /* should be ok to use the font now */
    active = True;

    /* how big a window? */
    set_ruler_text( 0, 0, &h, &w, &asc );

    /* keep this global, we might destroy it later */
    db_ruler = XtVaCreateManagedWidget( "Ruler", 
					xmDrawingAreaWidgetClass,
					box,
					XmNheight,	h,
					XmNwidth,	w,
					0
					);
    /* this callback is for when the drawing area is exposed */
    XtAddCallback( db_ruler,
		   XmNexposeCallback,
		   redraw_text,
		   0
		   );

    /* what colors are selected for the drawing area? */
    XtVaGetValues( db_ruler,
		   XmNbackground,		&bg,
		   XmNforeground,		&fg,
		   0
		   );
    XSetForeground( XtDisplay(db_ruler), gc_ruler, fg );
    XSetBackground( XtDisplay(db_ruler), gc_ruler, bg );

    /* done */
    return db_ruler;
}



/* module entry point
 *	__vi_show_text_ruler_dialog( parent, title )
 *	__vi_clear_text_ruler_dialog( parent, title )
 */

#if defined(__STDC__)
void	__vi_show_text_ruler_dialog( Widget parent, String title )
#else
void	__vi_show_text_ruler_dialog( parent, title )
Widget	parent;
String	title;
#endif
{
    Widget 	db = create_text_ruler_dialog( parent, title ),
		shell = XtParent(db);
    Dimension	height, width;

    /* this guy does not resize */
    XtVaGetValues( db,
		   XmNheight,	&height,
		   XmNwidth,	&width,
		   0
		   );
    XtVaSetValues( shell,
		   XmNmaxWidth,		width,
		   XmNminWidth,		width,
		   XmNmaxHeight,	height,
		   XmNminHeight,	height,
		   0
		   );

    XtManageChild( db );

    /* leave this guy up */
    XtPopup( shell, XtGrabNone );

    active = True;

    /* ask vi core for the current r,c now */
#if ! defined(SelfTest)
    __vi_set_text_ruler( __vi_screen->cury, __vi_screen->curx );
#else
    __vi_set_text_ruler( rand(), rand() );
#endif
}


#if defined(__STDC__)
void	__vi_clear_text_ruler_dialog()
#else
void	__vi_clear_text_ruler_dialog(void)
#endif
{
    if ( active )
	XtPopdown( XtParent(db_ruler) );
}


#if defined(SelfTest)

#if XtSpecificationRelease == 4
#define	ArgcType	Cardinal *
#else
#define	ArgcType	int *
#endif

static	void	change_pos( Widget w )
{
    __vi_set_text_ruler( rand(), rand() );
}

#if defined(__STDC__)
static void show_text_ruler( Widget w, XtPointer data, XtPointer cbs )
#else
static void show_text_ruler( w, data, cbs )
Widget w;
XtPointer	data;
XtPointer	cbs;
#endif
{
    __vi_show_text_ruler_dialog( data, "Ruler" );
}

main( int argc, char *argv[] )
{
    XtAppContext	ctx;
    Widget		top_level, rc, button;
    extern		exit();

    /* create a top-level shell for the window manager */
    top_level = XtVaAppInitialize( &ctx,
				   argv[0],
				   NULL, 0,	/* options */
				   (ArgcType) &argc,
				   argv,	/* might get modified */
				   NULL,
				   NULL
				   );

    rc = XtVaCreateManagedWidget( "rc",
				  xmRowColumnWidgetClass,
				  top_level,
				  0
				  );

    button = XtVaCreateManagedWidget( "Pop up text ruler dialog",
				      xmPushButtonGadgetClass,
				      rc,
				      0
				      );
    XtAddCallback( button, XmNactivateCallback, show_text_ruler, rc );

    button = XtVaCreateManagedWidget( "Change Position",
				      xmPushButtonGadgetClass,
				      rc,
				      0
				      );
    XtAddCallback( button, XmNactivateCallback, change_pos, rc );

    button = XtVaCreateManagedWidget( "Quit",
				      xmPushButtonGadgetClass,
				      rc,
				      0
				      );
    XtAddCallback( button, XmNactivateCallback, exit, 0 );

    XtRealizeWidget(top_level);
    XtAppMainLoop(ctx);
}
#endif
