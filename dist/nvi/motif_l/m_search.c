/*	$NetBSD: m_search.c,v 1.1.1.2 2008/05/18 14:31:28 aymeric Exp $ */

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
static const char sccsid[] = "Id: m_search.c,v 8.14 2003/11/05 17:10:00 skimo Exp (Berkeley) Date: 2003/11/05 17:10:00";
#endif /* not lint */

#include <sys/queue.h>

/* context */
#include <X11/X.h>
#include <X11/Intrinsic.h>
#include <Xm/DialogS.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushBG.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>

#undef LOCK_SUCCESS
#include "../common/common.h"
#include "../ipc/ip.h"
#include "m_motif.h"

extern int vi_ofd;


/* types */

typedef struct sds {
    struct sds	*next;
    Widget	shell;
} save_dialog;

static	save_dialog	*dialogs = NULL;

typedef struct	{
    String	name;
    void	(*cb)();
} ButtonData;


/* globals and constants */

static	String	PatternWidget = "text";
static	String	pattern = NULL;

static optData  search_toggles[] = {
	{ optToggle,	"extended",	NULL,	VI_SEARCH_EXT	},
	{ optToggle,	"iclower",	NULL,	VI_SEARCH_ICL	},
	{ optToggle,	"ignorecase",	NULL,	VI_SEARCH_IC	},
	{ optToggle,	"literal",	NULL,	VI_SEARCH_LIT	},
	{ optToggle,	"searchincr",	NULL,	VI_SEARCH_INCR	},
	{ optToggle,	"wrapscan",	NULL,	VI_SEARCH_WR	},
	{ optTerminator,		},
};

static void done_func __P((Widget));
static void next_func __P((Widget));
static void prev_func __P((Widget));
static void search __P((Widget, int));

static	ButtonData button_data[] = {
    { "Next",		next_func	},
    { "Previous", 	prev_func	},
    { "Cancel", 	done_func	}	/* always last */
};


/* Xt utilities */

#if defined(__STDC__)
static	Widget	get_child_widget( Widget parent, String name )
#else
static	Widget	get_child_widget( parent, name )
	Widget	parent;
	String	name;
#endif
{
    char buffer[1024];

    strcpy( buffer, "*" );
    strcat( buffer, name );
    return XtNameToWidget( parent, buffer );
}


/* sync the global state */

#if defined(__STDC__)
static	void	get_state( Widget w )
#else
static	void	get_state( w )
	Widget	w;
#endif
{
#if defined(SelfTest)
    int		i;
#endif
    Widget	shell = w;

    /* get all the data from the root of the widget tree */
    while ( ! XtIsShell(shell) ) shell = XtParent(shell);

#if defined(SelfTest)
    /* which flags? */
    for (i=0; i<XtNumber(toggle_data); i++) {
	if (( w = get_child_widget( shell, toggle_data[i].name )) != NULL ) {
	    XtVaGetValues( w, XmNset, &toggle_data[i].value, 0 );
	}
    }
#endif

    /* what's the pattern? */
    if (( w = get_child_widget( shell, PatternWidget )) != NULL ) {
	if ( pattern != NULL ) XtFree( pattern );
	pattern = XmTextFieldGetString( w );
    }
}


/* Translate the user's actions into nvi commands */
/*
 * next_func --
 *	Action for next button.
 */
static void
next_func(Widget w)
{
	search(w, 0);
}

/*
 * prev_func --
 *	Action for previous button.
 */
static void
prev_func(Widget w)
{
	search(w, VI_SEARCH_REV);
}

/*
 * search --
 *	Perform the search.
 */
static void
search(Widget w, int flags)
{
	IP_BUF ipb;
	optData *opt;
	Widget shell;

	shell = w;
	while ( ! XtIsShell(shell) ) shell = XtParent(shell);

	/* Get current data from the root of the widget tree?
	 * Do it if we are a child of a dialog shell (assume we
	 * are a 'Find' dialog).  Otherwise don't (we are the child
	 * of a menu and being invoked via accelerator)
	 */
	if (XmIsDialogShell(shell))
		get_state(w);

	/* no pattern? probably, we haven't posted a search dialog yet.
	 * there ought to be a better thing to do here.
	 */
	if ( pattern == NULL ) {
	    vi_info_message( w, "No previous string specified" );
	    return;
	}

	ipb.str1 = pattern;
	ipb.len1 = strlen(pattern);

	/* Initialize the search flags based on the buttons. */
	ipb.val1 = flags;
	for (opt = search_toggles; opt->kind != optTerminator; ++opt)
		if (opt->value != NULL)
			ipb.val1 |= opt->flags;

	ipb.code = VI_C_SEARCH;
	vi_send(vi_ofd, "a1", &ipb);
}

#if defined(__STDC__)
static	void	done_func( Widget w )
#else
static	void	done_func( w )
	Widget	w;
#endif
{
    save_dialog	*ptr;

#if defined(SelfTest)
    puts( XtName(w) );
#endif

    while ( ! XtIsShell(w) ) w = XtParent(w);
    XtPopdown( w );

    /* save it for later */
    ptr		= (save_dialog *) malloc( sizeof(save_dialog) );
    ptr->next	= dialogs;
    ptr->shell	= w;
    dialogs	= ptr;
}


/* create a set of push buttons */

#define	SpacingRatio	4	/* 3:1 button to spaces */

#if defined(__STDC__)
static	Widget	create_push_buttons( Widget parent,
				     ButtonData *data,
				     int count
				    )
#else
static	Widget	create_push_buttons( parent, data, count )
    Widget	parent;
    ButtonData	*data;
    int		count;
#endif
{
    Widget	w, form;
    int		pos = 1, base;

    base = SpacingRatio*count + 1;
    form = XtVaCreateManagedWidget( "buttons", 
				    xmFormWidgetClass,
				    parent,
				    XmNleftAttachment,	XmATTACH_FORM,
				    XmNrightAttachment,	XmATTACH_FORM,
				    XmNfractionBase,	base,
				    XmNshadowType,	XmSHADOW_ETCHED_IN,
				    XmNshadowThickness,	2,
				    XmNverticalSpacing,	4,
				    0
				    );

    while ( count-- > 0 ) {
	w = XtVaCreateManagedWidget(data->name,
				    xmPushButtonGadgetClass,
				    form,
				    XmNtopAttachment,	XmATTACH_FORM,
				    XmNbottomAttachment,XmATTACH_FORM,
				    XmNleftAttachment,	XmATTACH_POSITION,
				    XmNleftPosition,	pos,
				    XmNshowAsDefault,	False,
				    XmNrightAttachment,	XmATTACH_POSITION,
				    XmNrightPosition,	pos+SpacingRatio-1,
				    0
				    );
	XtAddCallback( w, XmNactivateCallback, data->cb, 0 );
	pos += SpacingRatio;
	data++;
    }

    /* last button is 'cancel' */
    XtVaSetValues( XtParent(form), XmNcancelButton, w, 0 );

    return form;
}


/* create a set of check boxes */

#if defined(SelfTest)

#if defined(__STDC__)
static	Widget	create_check_boxes( Widget parent,
				    ToggleData *toggles,
				    int count
				    )
#else
static	Widget	create_check_boxes( parent, toggles, count )
	Widget	parent;
	ToggleData *toggles;
	int	count;
#endif
{
    Widget	form;
    int		pos = 1, base;

    base = SpacingRatio*count +1;
    form = XtVaCreateManagedWidget( "toggles", 
				    xmFormWidgetClass,
				    parent,
				    XmNleftAttachment,	XmATTACH_FORM,
				    XmNrightAttachment,	XmATTACH_FORM,
				    XmNfractionBase,	base,
				    XmNverticalSpacing,	4,
				    0
				    );

    while ( count-- > 0 ) {
	XtVaCreateManagedWidget(toggles->name,
				xmToggleButtonWidgetClass,
				form,
				XmNtopAttachment,	XmATTACH_FORM,
				XmNbottomAttachment,	XmATTACH_FORM,
				XmNleftAttachment,	XmATTACH_POSITION,
				XmNleftPosition,	pos,
				XmNrightAttachment,	XmATTACH_POSITION,
				XmNrightPosition,	pos+SpacingRatio-1,
				0
				);
	pos += SpacingRatio;
	++toggles;
    }

    return form;
}

#endif


/* Routines to handle the text field widget */

/* when the user hits 'CR' in a text widget, fire the default pushbutton */
#if defined(__STDC__)
static	void	text_cr( Widget w, void *ptr, void *ptr2 )
#else
static	void	text_cr( w, ptr, ptr2 )
	Widget	w;
	void	*ptr;
	void	*ptr2;
#endif
{
    next_func( w );
}


#ifdef notdef
/*
 * when the user hits any other character, if we are doing incremental
 * search, send the updated string to nvi
 *
 * XXX
 * I don't currently see any way to make this work -- incremental search
 * is going to be really nasty.  What makes it worse is that the dialog
 * box almost certainly obscured a chunk of the text file, so there's no
 * way to use it even if it works.  
 */
#if defined(__STDC__)
static	void	value_changed( Widget w, void *ptr, void *ptr2 )
#else
static	void	value_changed( w, ptr, ptr2 )
	Widget	w;
	void	*ptr;
	void	*ptr2;
#endif
{
    /* get all the data from the root of the widget tree */
    get_state( w );

    /* send it along? */
#if defined(SelfTest)
    if ( incremental_search ) send_command( w );
#else
    if ( __vi_incremental_search() ) send_command( w );
#endif
}
#endif /* notdef */


/* Draw and display a dialog the describes nvi search capability */

#if defined(__STDC__)
static	Widget	create_search_dialog( Widget parent, String title )
#else
static	Widget	create_search_dialog( parent, title )
	Widget	parent;
	String	title;
#endif
{
    Widget	box, form, label, text, checks, buttons, form2;
    save_dialog	*ptr;

    /* use an existing one? */
    if ( dialogs != NULL ) {
	box = dialogs->shell;
	ptr = dialogs->next;
	free( dialogs );
	dialogs = ptr;
	return box;
    }

    box = XtVaCreatePopupShell( title,
				xmDialogShellWidgetClass,
				parent,
				XmNtitle,		title,
				XmNallowShellResize,	False,
				0
				);

    form = XtVaCreateWidget( "form", 
				xmFormWidgetClass,
				box,
				XmNverticalSpacing,	4,
				XmNhorizontalSpacing,	4,
				0
				);

    form2 = XtVaCreateManagedWidget( "form", 
				xmFormWidgetClass,
				form,
				XmNtopAttachment,	XmATTACH_FORM,
				XmNleftAttachment,	XmATTACH_FORM,
				XmNrightAttachment,	XmATTACH_FORM,
				0
				);

    label = XtVaCreateManagedWidget( "Pattern:", 
				    xmLabelWidgetClass,
				    form2,
				    XmNtopAttachment,	XmATTACH_FORM,
				    XmNbottomAttachment,XmATTACH_FORM,
				    XmNleftAttachment,	XmATTACH_FORM,
				    0
				    );

    text = XtVaCreateManagedWidget( PatternWidget, 
				    xmTextFieldWidgetClass,
				    form2,
				    XmNtopAttachment,	XmATTACH_FORM,
				    XmNbottomAttachment,XmATTACH_FORM,
				    XmNleftAttachment,	XmATTACH_WIDGET,
				    XmNleftWidget,	label,
				    XmNrightAttachment,	XmATTACH_FORM,
				    0
				    );
#ifdef notdef
    XtAddCallback( text, XmNvalueChangedCallback, value_changed, 0 );
#endif
    XtAddCallback( text, XmNactivateCallback, text_cr, 0 );

    buttons = create_push_buttons( form, button_data, XtNumber(button_data) );
    XtVaSetValues( buttons,
		   XmNbottomAttachment,	XmATTACH_FORM,
		   0
		   );

#if defined(SelfTest)
    checks = create_check_boxes( form, toggle_data, XtNumber(toggle_data) );
#else
    checks = (Widget) __vi_create_search_toggles( form, search_toggles );
#endif
    XtVaSetValues( checks,
		   XmNtopAttachment,	XmATTACH_WIDGET,
		   XmNtopWidget,	form2,
		   XmNbottomAttachment,	XmATTACH_WIDGET,
		   XmNbottomWidget,	buttons,
		   0
		   );

    XtManageChild( form );
    return box;
}


/* Module interface to the outside world
 *
 *	xip_show_search_dialog( parent, title )
 *	pops up a search dialog
 *
 *	xip_next_search()
 *	simulates a 'next' assuming that a search has been done
 */

#if defined(__STDC__)
void 	__vi_show_search_dialog( Widget parent, String title )
#else
void	__vi_show_search_dialog( parent, data, cbs )
Widget	parent;
String	title;
#endif
{
    Widget 	db = create_search_dialog( parent, title );
    Dimension	height;

    /* we can handle getting taller and wider or narrower, but not shorter */
    XtVaGetValues( db, XmNheight, &height, 0 );
    XtVaSetValues( db, XmNmaxHeight, height, XmNminHeight, height, 0 );

    /* post the dialog */
    XtPopup( db, XtGrabNone );

    /* request initial focus to the text widget */
    XmProcessTraversal( get_child_widget( db, PatternWidget ),
			XmTRAVERSE_CURRENT
			);
}


/*
 * __vi_search --
 *
 * PUBLIC: void __vi_search __P((Widget));
 */
void
__vi_search(Widget w)
{
    next_func( w );
}


#if defined(SelfTest)

#if defined(__STDC__)
static void show_search( Widget w, XtPointer data, XtPointer cbs )
#else
static void show_search( w, data, cbs )
Widget w;
XtPointer	data;
XtPointer	cbs;
#endif
{
    __vi_show_search_dialog( data, "Search" );
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

    button = XtVaCreateManagedWidget( "Pop up search dialog",
				      xmPushButtonGadgetClass,
				      rc,
				      0
				      );
    XtAddCallback( button, XmNactivateCallback, show_search, rc );

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
