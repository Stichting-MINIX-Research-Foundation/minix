/*	$NetBSD: m_main.c,v 1.1.1.2 2008/05/18 14:31:26 aymeric Exp $ */

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
static const char sccsid[] = "Id: m_main.c,v 8.40 2003/11/05 17:09:58 skimo Exp (Berkeley) Date: 2003/11/05 17:09:58";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xm/MainW.h>

#include <bitstring.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef LOCK_SUCCESS
#include "../common/common.h"
#include "../ipc/ip.h"
#include "../motif_l/m_motif.h"
#include "../motif_l/vi_mextern.h"
#include "extern.h"

int     vi_ifd = -1;
int     vi_ofd = -1;
IPVIWIN   *ipvi_motif;

#if XtSpecificationRelease == 4
#define	ArgcType	Cardinal *
#else
#define	ArgcType	int *
#endif

#if defined(ColorIcon)
#if XT_REVISION >= 6
#include <X11/xpm.h>
#else
#include "xpm.h"
#endif

#include "nvi.xpm"		/* Icon pixmap. */
#else
#include "nvi.xbm"		/* Icon bitmap. */
#endif

static	pid_t		pid;
static	Pixel		icon_fg,
			icon_bg;
static	Pixmap		icon_pm;
static	Widget		top_level;
static	XtAppContext	ctx;

static void XutInstallColormap __P((String, Widget));
static void XutSetIcon __P((Widget, int, int, Pixmap));
static void onchld __P((int));
static void onexit __P((void));

#if ! defined(ColorIcon)
static  XutResource resource[] = {
    { "iconForeground",	XutRKpixel,	&icon_fg	},
    { "iconBackground",	XutRKpixel,	&icon_bg	},
};
#endif


/* resources for the vi widgets unless the user overrides them */
String	fallback_rsrcs[] = {

    "*font:			-*-*-*-r-*--14-*-*-*-m-*-*-*",
    "*text*fontList:		-*-*-*-r-*--14-*-*-*-m-*-*-*",
    "*Menu*fontList:		-*-helvetica-bold-r-normal--14-*-*-*-*-*-*-*",
    "*fontList:			-*-helvetica-medium-r-normal--14-*-*-*-*-*-*-*",
    "*pointerShape:		xterm",
    "*busyShape:		watch",
    "*iconName:			vi",

#if ! defined(ColorIcon)
    /* coloring for the icons */
    "*iconForeground:	XtDefaultForeground",
    "*iconBackground:	XtDefaultBackground",
#endif

    /* layout for the tag stack dialog */
    "*Tags*visibleItemCount:			5",

    /* for the text ruler */
    "*rulerFont:		-*-helvetica-medium-r-normal--14-*-*-*-*-*-*-*",
    "*rulerBorder:		5",

    /* layout for the new, temporary preferences page */
    "*toggleOptions.numColumns:			6",	/* also used by Find */
    "*Preferences*tabWidthPercentage:		0",
    "*Preferences*tabs.shadowThickness:		2",
    "*Preferences*tabs.font:	-*-helvetica-bold-r-normal--14-*-*-*-*-*-*-*",

    /* --------------------------------------------------------------------- *
     * anything below this point is only defined when we are not running CDE *
     * --------------------------------------------------------------------- */

    /* Do not define default colors when running under CDE
     * (e.g. VUE on HPUX). The result is that you don't look
     * like a normal desktop application
     */
    "?background:			gray75",
    "?screen.background:		wheat",
    "?highlightColor:			red",
    "?Preferences*options.background:	gray90",
};

#if defined(__STDC__)
static	String	*get_fallback_rsrcs( String name )
#else
static	String	*get_fallback_rsrcs( name )
	String	name;
#endif
{
    String	*copy = (String *) malloc( (1+XtNumber(fallback_rsrcs))*sizeof(String) );
    int		i, running_cde;
    Display	*d;

    /* connect to server and see if the CDE atoms are present */
    d = XOpenDisplay(0);
    running_cde = is_cde( d );
    XCloseDisplay(d);

    for ( i=0; i<XtNumber(fallback_rsrcs); i++ ) {

	/* stop here if running CDE */
	if ( fallback_rsrcs[i][0] == '?' ) {
	    if ( running_cde ) break;
	    fallback_rsrcs[i] = strdup(fallback_rsrcs[i]);
	    fallback_rsrcs[i][0] = '*';
	}

	copy[i] = malloc( strlen(name) + strlen(fallback_rsrcs[i]) + 1 );
	strcpy( copy[i], name );
	strcat( copy[i], fallback_rsrcs[i] );
    }

    copy[i] = NULL;
    return copy;
}


/* create the shell widgetry */

#if defined(__STDC__)
static	void	create_top_level_shell( int *argc, char **argv )
#else
static	void	create_top_level_shell( argc, argv )
	int	*argc;
	char	**argv;
#endif
{
    char	*ptr;
    Widget	main_w, editor;
    Display	*display;

    /* X gets quite upset if the program name is not simple */
    if (( ptr = strrchr( argv[0], '/' )) != NULL ) argv[0] = ++ptr;
    vi_progname = argv[0];

    /* create a top-level shell for the window manager */
    top_level = XtVaAppInitialize( &ctx,
				   vi_progname,
				   NULL, 0,	/* options */
				   (ArgcType) argc,
				   argv,	/* might get modified */
				   get_fallback_rsrcs( argv[0] ),
				   NULL
				   );
    display = XtDisplay(top_level);

    /* might need to go technicolor... */
    XutInstallColormap( argv[0], top_level );

    /* create our icon
     * do this *before* realizing the shell widget in case the -iconic
     * option was specified.
     */
    {
#if defined(ColorIcon)
    int			nvi_width, nvi_height;
    XpmAttributes	attr;

    attr.valuemask = 0;
    XpmCreatePixmapFromData( display,
			     DefaultRootWindow(display),
			     nvi_xpm,
			     &icon_pm,
			     NULL,
			     &attr
			     );
    nvi_width = attr.width;
    nvi_height = attr.height;
#else
    /* check the resource database for interesting resources */
    __XutConvertResources( top_level,
			 vi_progname,
			 resource,
			 XtNumber(resource)
			 );

    icon_pm = XCreatePixmapFromBitmapData(
			display,
		        DefaultRootWindow(display),
			(char *) nvi_bits,
			nvi_width,
			nvi_height,
			icon_fg,
			icon_bg,
			DefaultDepth( display, DefaultScreen(display) )
			);
#endif
    XutSetIcon( top_level, nvi_height, nvi_width, icon_pm );
    }

    /* in the shell, we will stack a menubar an editor */
    main_w = XtVaCreateManagedWidget( "main",
				      xmMainWindowWidgetClass,
				      top_level,
				      NULL
				      );

    /* create the menubar */
    XtManageChild( (Widget) vi_create_menubar( main_w ) );

    /* add the VI widget from the library */
    editor = vi_create_editor( "editor", main_w, onexit );

    /* put it up */
    XtRealizeWidget( top_level );

    /* We *may* want all keyboard events to go to the editing screen.
     * If the editor is the only widget in the shell that accepts
     * keyboard input, then the user will expect that he can type when
     * the pointer is over the scrollbar (for example).  This call
     * causes that to happen.
     */
    XtSetKeyboardFocus( top_level, XtNameToWidget( editor, "*screen" ) );
}


int
main(int argc, char **argv)
{
	IPVI* ipvi;
	/*
	 * Initialize the X widgetry.  We must do this before picking off
	 * arguments as well-behaved X programs have common argument lists
	 * (e.g. -rv for reverse video).
	 */
	create_top_level_shell(&argc, argv);

	/* We need to know if the child process goes away. */
	(void)signal(SIGCHLD, onchld);

	vi_create(&ipvi, 0);
	(void)ipvi->run(ipvi, argc, argv);
	ipvi->new_window(ipvi,&ipvi_motif,-1);
	ipvi_motif->set_ops(ipvi_motif, &ipsi_ops_motif);
	/* Run vi: the parent returns, the child is the vi process. */
	vi_ifd = ipvi_motif->ifd;
	vi_ofd = ipvi_motif->ofd;
	pid = ipvi->pid;

	/* Tell X that we are interested in input on the pipe. */
	XtAppAddInput(ctx, vi_ifd,
	    (XtPointer)XtInputReadMask, vi_input_func, NULL);

	/* Main loop. */
	XtAppMainLoop(ctx);

	/* NOTREACHED */
	abort();
}

static void
XutSetIcon(Widget wid, int height, int width, Pixmap p)
{
    Display	*display = XtDisplay(wid);
    Window	win;

    /* best bet is to set the icon window */
    XtVaGetValues( wid, XtNiconWindow, &win, 0 );

    if ( win == None ) {
	win = XCreateSimpleWindow( display,
				   RootWindow( display,
				   DefaultScreen( display ) ),
				   0, 0,
				   width, height,
				   0,
				   CopyFromParent,
				   CopyFromParent
				   );
    }

    if ( win != None ) {
	XtVaSetValues( wid, XtNiconWindow, win, 0 );
	XSetWindowBackgroundPixmap( display, win, p );
    }

    else {
	/* do it the old fashioned way */
	XtVaSetValues( wid, XtNiconPixmap, p, 0 );
    }
}

/* Support for multiple colormaps
 *
 * XutInstallColormap( String name, Widget wid )
 *	The first time called, this routine checks to see if the
 *	resource "name*installColormap" is "True".  If so, the
 *	widget is assigned a newly allocated colormap.
 *
 *	Subsequent calls ignore the "name" parameter and use the
 *	same colormap.
 *
 *	Future versions of this routine may handle multiple colormaps
 *	by name.
 */
static	enum { cmap_look, cmap_use, cmap_ignore } cmap_state = cmap_look;

static	Boolean	use_colormap = False;

static XutResource	colormap_resources[] = {
    { "installColormap",	XutRKboolean,	&use_colormap	}
};

static void
XutInstallColormap(String name, Widget wid)
{
    static Colormap cmap = 0;
    static Display  *cmap_display = 0;
    Display	*display = XtDisplay(wid);

    /* what is the current finite state? */
    if ( cmap_state == cmap_look ) {

	/* what does the resource say? */
	__XutConvertResources( wid,
			     name,
			     colormap_resources,
			     XtNumber(colormap_resources)
			     );

	/* was the result "True"? */
	if ( ! use_colormap ) {
	    cmap_state = cmap_ignore;
	    return;
	}

	/* yes it was */
	cmap_state = cmap_use;
	cmap_display = display;
	cmap = XCopyColormapAndFree( display,
				     DefaultColormap( display,
						      DefaultScreen( display )
						      )
				     );
    }

    /* use the private colormap? */
    if ( cmap_state == cmap_use ) {
	XtVaSetValues( wid, XtNcolormap, cmap, 0 );
    }
}

/*
 * onchld --
 *	Handle SIGCHLD.
 */
static void
onchld(int signo)
{
	/* If the vi process goes away, we exit as well. */
	if (kill(pid, 0))
		vi_fatal_message(top_level, "The vi process died.  Exiting.");
}

/*
 * onexit --
 *	Function called when the editor "quits".
 */
static void
onexit(void)
{
	exit (0);
}
