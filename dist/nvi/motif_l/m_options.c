/*	$NetBSD: m_options.c,v 1.1.1.2 2008/05/18 14:31:27 aymeric Exp $ */

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
static const char sccsid[] = "Id: m_options.c,v 8.22 2003/11/05 17:09:59 skimo Exp (Berkeley) Date: 2003/11/05 17:09:59";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <X11/X.h>
#include <X11/Intrinsic.h>
#include <Xm/DialogS.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/LabelG.h>
#include <Xm/PushBG.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>
#include <Xm/RowColumn.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef LOCK_SUCCESS
#include "../common/common.h"
#include "../ipc/ip.h"
#include "m_motif.h"

extern int vi_ofd;

static void set_opt __P((Widget, XtPointer, XtPointer));


/* constants */

#if defined(SelfTest)

/* in production, get these from the resource list */

#define	toggleColumns	6

#endif


/*
 * global data
 */

static Widget	preferences = NULL;

static optData display[] = {
	{ optToggle,	"comment",	},
	{ optToggle,	"flash",	},
	{ optToggle,	"leftright",	},
	{ optToggle,	"list",		},
	{ optToggle,	"number",	},
	{ optToggle,	"octal",	},
	{ optToggle,	"ruler",	},
	{ optToggle,	"showmode",	},
	{ optToggle,	"slowopen",	},
	{ optToggle,	"verbose",	},
	{ optToggle,	"windowname",	},
	{ optTerminator,		},
}, display_int[] = {
	{ optInteger,	"report",	},
	{ optInteger,	"scroll",	},
	{ optInteger,	"shiftwidth",	},
	{ optInteger,	"sidescroll",	},
	{ optInteger,	"tabstop",	},
	{ optInteger,	"window",	},
	{ optTerminator,		},
}, display_str[] = {
	{ optString,	"noprint",	},
	{ optString,	"print",	},
	{ optTerminator,		},
}, files[] = {
	{ optToggle,	"autowrite",	},
	{ optToggle,	"lock",		},
	{ optToggle,	"readonly",	},
	{ optToggle,	"writeany",	},
	{ optTerminator,		},
}, files_str[] = {
	{ optString,	"backup",	},
	{ optString,	"path",		},
	{ optTerminator,		},
}, general[] = {
	{ optToggle,	"exrc",		},
	{ optToggle,	"lisp",		},
	{ optToggle,	"modeline",	},
	{ optToggle,	"sourceany",	},
	{ optToggle,	"tildeop",	},
	{ optTerminator,		},
}, general_int[] = {
	{ optInteger,	"taglength",	},
	{ optTerminator,		},
}, general_str[] = {
	{ optString,	"cdpath",	},
	{ optString,	"directory",	},
	{ optString,	"msgcat",	},
	{ optString,	"recdir",	},
	{ optString,	"shell",	},
	{ optString,	"shellmeta",	},
	{ optString,	"tags",		},
	{ optTerminator,		},
}, input[] = {
	{ optToggle,	"altwerase",	},
	{ optToggle,	"autoindent",	},
	{ optToggle,	"remap",	},
	{ optToggle,	"showmatch",	},
	{ optToggle,	"ttywerase",	},
	{ optTerminator,		},
}, input_int[] = {
	{ optInteger,	"escapetime",	},
	{ optInteger,	"keytime",	},
	{ optInteger,	"matchtime",	},
	{ optInteger,	"timeout",	},
	{ optInteger,	"wraplen",	},
	{ optInteger,	"wrapmargin",	},
	{ optTerminator,		},
}, input_str[] = {
	{ optString,	"cedit",	},
	{ optString,	"filec",	},
	{ optTerminator,		},
}, search[] = {
	{ optToggle,	"extended",	},
	{ optToggle,	"iclower",	},
	{ optToggle,	"ignorecase",	},
	{ optToggle,	"magic",	},
	{ optToggle,	"searchincr",	},
	{ optToggle,	"wrapscan",	},
	{ optTerminator,		},
}, search_str[] = {
	{ optString,	"paragraphs",	},
	{ optString,	"sections",	},
	{ optTerminator,		},
};

/* ********* NOTE ***********
 * Sheet 0 will always be shown first.  It does not matter to the Xt code
 * which sheet that is, so it ought to be the one users interact with most.
 * Best guess is that's general editor options, but it might be Search/re.
 * ********* NOTE ***********
 */
static	optSheet sheets[] = {
	{	"Display",
		"These options control how text is displayed on the screen",
		NULL,
		display,
		display_int,
		display_str,
	},
	{	"Files",
		"These options control how the editor handles files",
		NULL,
		files,
		NULL,
		files_str,
	},
	{	"Input",
		"These options control text input behavior",
		NULL,
		input,
		input_int,
		input_str,
	},
	{	"Search/RE",
	"These options control searching and Regular Expression behavior",
		NULL,
		search,
		NULL,
		search_str,
	},
	{	"Editor",
		"These options control general editor configuration",
		NULL,
		general,
		general_int,
		general_str,
	},
};


/* callbacks */

#if defined(SelfTest)
void __vi_cancel_cb()
{
    puts( "cancelled" );
}
#endif


static	void destroyed(void)
{
    int i;

    puts( "destroyed" );

    /* some window managers destroy us upon popdown */
    for (i=0; i<XtNumber(sheets); i++) {
	sheets[i].holder = NULL;
    }
    preferences = NULL;
}


static	void	window_unmapped(Widget w, XtPointer ptr, XEvent *ev, Boolean *cont)
{
    if ( ev->type == UnmapNotify ) {
#if defined(SelfTest)
	puts( "unmapped" );
#endif
	XtPopdown( XtParent( preferences ) );
    }
}

/*
 * __vi_editopt --
 *	Set an edit option based on a core message.
 *
 * PUBLIC: int __vi_editopt __P((IPVI *, const char *, u_int32_t, const char *, u_int32_t, u_int32_t));
 */
int
__vi_editopt(IPVI *ipvi, const char *str1, u_int32_t len1, const char *str2, u_int32_t len2, u_int32_t val1)
{
	optData *opt;

#undef	NSEARCH
#define	NSEARCH(list) {							\
	for (opt = list; opt->kind != optTerminator; ++opt)		\
		if (!strcmp(opt->name, str1))			\
			goto found;					\
}

	NSEARCH(display);
	NSEARCH(display_int);
	NSEARCH(display_str);
	NSEARCH(files);
	NSEARCH(files_str);
	NSEARCH(general);
	NSEARCH(general_int);
	NSEARCH(general_str);
	NSEARCH(input);
	NSEARCH(input_int);
	NSEARCH(input_str);
	NSEARCH(search);
	NSEARCH(search_str);

	return (0);

found:	switch (opt->kind) {
	case optToggle:
		opt->value = (void *)val1;
		break;
	case optInteger:
		if (opt->value != NULL)
			free(opt->value);
		if ((opt->value = malloc(8)) != NULL)
			(void)snprintf(opt->value,
			    8, "%lu", (u_long)val1);
		break;
	case optString:
	case optFile:
		if (opt->value != NULL)
			free(opt->value);
		if ((opt->value = malloc(len2)) != NULL)
			memcpy(opt->value, str2, len2);
		break;
	case optTerminator:
		abort();
	}
	return (0);
}

/*
 * set_opt --
 *	Send a set-edit-option message to core.
 */
static void
set_opt(Widget w, XtPointer closure, XtPointer call_data)
{
	optData *opt;
	Boolean set;
	IP_BUF ipb;
	String str;
	extern IPVI ipvi_motif;

	opt = closure;

	ipb.code = VI_EDITOPT;
	ipb.str1 = opt->name;
	ipb.len1 = strlen(opt->name);

	switch (opt->kind) {
	case optToggle:
		XtVaGetValues(w, XmNset, &set, 0);
		ipb.val1 = set;
		ipb.len2 = 0;

		vi_wsend(&ipvi_motif, "ab1", &ipb);
		if (ipb.val1) {
			opt->value = (void *)!set;
			/*
			 * RAZ:
			 * How do we turn off the button?  We don't want to
			 * go recursive where we set it and it calls set_opt
			 * to tell the core.  Is that possible?
			 */
			XtVaSetValues(w, XmNset, &set, 0);
			break;
		}

		if (strcmp(opt->name, "ruler") == 0)
			if (set)
				__vi_show_text_ruler_dialog(
				    __vi_screen->area, "Ruler");
			else
				__vi_clear_text_ruler_dialog();
		break;
	case optInteger:
		str = XmTextFieldGetString(w);
		ipb.val1 = atoi(str);
		ipb.len2 = 0;
		vi_send(vi_ofd, "ab1", &ipb);
		break;
	case optFile:
	case optString:
		ipb.str2 = XmTextFieldGetString(w);
		ipb.len2 = strlen(ipb.str2);
		vi_send(vi_ofd, "ab1", &ipb);
		break;
	case optTerminator:
		abort();
	}
}


/* add toggles to the property sheet */

#if defined(__STDC__)
static	void	add_toggle( Widget parent, optData *option )
#else
static	void	add_toggle( parent, option )
	Widget	parent;
	optData	*option;
#endif
{
    Widget	w;

    w = XtVaCreateManagedWidget( option->name,
				 xmToggleButtonGadgetClass,
				 parent,
				 XmNset,	(Boolean) option->value,
				 0
				 );
    XtAddCallback( w, XmNvalueChangedCallback, set_opt, option );
}


static	Widget	create_toggles(Widget outer, optData *toggles)
{
    Widget	inner;
    int		i;

    inner = XtVaCreateWidget( "toggleOptions",
			      xmRowColumnWidgetClass,
			      outer,
			      XmNpacking,		XmPACK_COLUMN,
			      XmNnumColumns,		4,
			      XmNtopAttachment,		XmATTACH_FORM,
			      XmNrightAttachment,	XmATTACH_FORM,
			      XmNleftAttachment,	XmATTACH_FORM,
			      0
			      );

    /* first the booleans */
    for (i=0; toggles[i].kind != optTerminator; i++) {
	add_toggle( inner, &toggles[i] );
    }
    XtManageChild( inner );

    return inner;
}


/* draw text fields and their labels */

#if defined(__STDC__)
static	void	add_string_options( Widget parent,
				    optData *options
				    )
#else
static	void	add_string_options( parent, options )
	Widget	parent;
	optData	*options;
#endif
{
    int		i;
    Widget	f, w;

    for ( i=0; options[i].kind != optTerminator; i++ ) {

	f = XtVaCreateWidget( "form",
			      xmFormWidgetClass,
			      parent,
			      0
			      );

	XtVaCreateManagedWidget( options[i].name,
				 xmLabelGadgetClass,
				 f,
				 XmNtopAttachment,	XmATTACH_FORM,
				 XmNbottomAttachment,	XmATTACH_FORM,
				 XmNleftAttachment,	XmATTACH_FORM,
				 XmNrightAttachment,	XmATTACH_POSITION,
				 XmNrightPosition,	20,
				 XmNalignment,		XmALIGNMENT_END,
				 0
				 );

	w = XtVaCreateManagedWidget( "text",
				     xmTextFieldWidgetClass,
				     f,
				     XmNtopAttachment,		XmATTACH_FORM,
				     XmNbottomAttachment,	XmATTACH_FORM,
				     XmNrightAttachment,	XmATTACH_FORM,
				     XmNleftAttachment,		XmATTACH_POSITION,
				     XmNleftPosition,		20,
				     0
				     );

	XmTextFieldSetString( w, (char *) options[i].value );
	XtAddCallback( w, XmNactivateCallback, set_opt, &options[i] );
	XtManageChild( f );
    }
}


/* draw and display a single page of properties */

#if defined(__STDC__)
static	Widget		create_sheet( Widget parent, optSheet *sheet )
#else
static	Widget		create_sheet( parent, sheet )
	Widget		parent;
	optSheet	*sheet;
#endif
{
    Widget	outer, inner, frame;
    Dimension	height;
    XmString	str;

    outer = XtVaCreateWidget( sheet->name,
			      xmFormWidgetClass,
			      parent,
			      XmNtopAttachment,		XmATTACH_FORM,
			      XmNrightAttachment,	XmATTACH_FORM,
			      XmNbottomAttachment,	XmATTACH_FORM,
			      XmNleftAttachment,	XmATTACH_FORM,
			      XmNshadowType,		XmSHADOW_ETCHED_IN,
			      XmNshadowThickness,	2,
			      XmNverticalSpacing,	4,
			      XmNhorizontalSpacing,	4,
			      0
			      );

    /* add descriptive text */
    frame = XtVaCreateManagedWidget( "frame",
				    xmFrameWidgetClass,
				    outer,
				    XmNtopAttachment,		XmATTACH_FORM,
				    XmNrightAttachment,		XmATTACH_FORM,
				    XmNleftAttachment,		XmATTACH_FORM,
				    0
				    );
    str = XmStringCreateLtoR( sheet->description, XmSTRING_DEFAULT_CHARSET );
    XtVaCreateManagedWidget( "description",
			     xmLabelGadgetClass,
			     frame,
			     XmNlabelString,		str,
			     0
			     );
    XmStringFree( str );

    /* Add the toggles. */
    inner = create_toggles( outer, sheet->toggles );
    XtVaSetValues( inner,
		   XmNtopAttachment,	XmATTACH_WIDGET,
		   XmNtopWidget,	frame,
		   0
		   );

    /* the string options go here */
    inner = XtVaCreateWidget( "otherOptions",
			      xmRowColumnWidgetClass,
			      outer,
			      XmNpacking,		XmPACK_COLUMN,
			      XmNtopAttachment,		XmATTACH_WIDGET,
			      XmNtopWidget,		inner,
			      XmNrightAttachment,	XmATTACH_FORM,
			      XmNbottomAttachment,	XmATTACH_FORM,
			      XmNleftAttachment,	XmATTACH_FORM,
			      0
			      );

    /* Optionally, the ints. */
    if ( sheet->ints != NULL )
	add_string_options( inner, sheet->ints );

     /* Optionally, the rest. */
    if ( sheet->others != NULL )
	add_string_options( inner, sheet->others );

    XtManageChild( inner );

    /* finally, force resize of the parent */
    XtVaGetValues( outer, XmNheight, &height, 0 );
    XtVaSetValues( parent, XmNheight, height, 0 );

    return outer;
}


/* change preferences to another sheet */

static	void	change_sheet(Widget parent, int current)
{
    static int		current_sheet = -1;

    /* create a new one? */
    if ( sheets[current].holder == NULL )
	sheets[current].holder = create_sheet( parent, &sheets[current] );

    /* done with the old one? */
    if ( current_sheet != -1 && sheets[current_sheet].holder != NULL )
	XtUnmanageChild( sheets[current_sheet].holder );

    current_sheet = current;
    XtManageChild( sheets[current].holder );
    XtManageChild( parent );
}


/* Draw and display a dialog the describes vi options */

#if defined(__STDC__)
static	Widget	create_options_dialog( Widget parent, String title )
#else
static	Widget	create_options_dialog( parent, title )
	Widget	parent;
	String	title;
#endif
{
    Widget	box, form, inner;
    int		i;
    char	buffer[1024];

    /* already built? */
    if ( preferences != NULL ) return preferences;

    box = XtVaCreatePopupShell( title,
				xmDialogShellWidgetClass,
				parent,
				XmNtitle,		title,
				XmNallowShellResize,	False,
				0
				);
    XtAddCallback( box, XmNpopdownCallback, __vi_cancel_cb, 0 );
    XtAddCallback( box, XmNdestroyCallback, destroyed, 0 );
    XtAddEventHandler( box,
		       SubstructureNotifyMask,
		       False,
		       window_unmapped,
		       NULL
		       );

    form = XtVaCreateWidget( "options", 
			     xmFormWidgetClass,
			     box,
			     0
			     );

    /* copy the pointers to the sheet names */
    *buffer = '\0';
    for (i=0; i<XtNumber(sheets); i++) {
	strcat( buffer, "|" );
	strcat( buffer, sheets[i].name );
    }

    inner = __vi_CreateTabbedFolder( "tabs",
				    form,
				    buffer,
				    XtNumber(sheets),
				    change_sheet
				    );

    /* build the property sheets early */
    for ( i=0; i<XtNumber(sheets); i++ )
	change_sheet( inner, i );

    /* manage all of the sheets right now */
    for ( i=0; i<XtNumber(sheets); i++ )
	XtManageChild( sheets[i].holder );
    XtManageChild( form );

    /* remove all but the first one */
    for ( i=0; i<XtNumber(sheets); i++ )
	XtUnmanageChild( sheets[i].holder );
    change_sheet( inner, 0 );	/* show first sheet first */

    /* keep this global, we might destroy it later */
    preferences = form;

    /* done */
    return form;
}



/*
 * module entry point
 *
 * __vi_show_options_dialog --
 *
 *
 * PUBLIC: void __vi_show_options_dialog __P((Widget, String));
 */
void
__vi_show_options_dialog(Widget parent, String title)
{
    Widget 	db = create_options_dialog( parent, title );
#if defined(SelfTest)
    Widget	shell = XtParent( db );
#endif

    XtManageChild( db );

#if defined(SelfTest)
    /* wait until it goes away */
    XtPopup( shell, XtGrabNone );
#else
    /* wait until it goes away */
    __vi_modal_dialog( db );
#endif
}



/* module entry point
 * Utilities for the search dialog
 *
 * __vi_toggle --
 *	Returns the current value of a toggle.
 *
 * PUBLIC: int __vi_toggle __P((char *));
 */
int
__vi_toggle(char *name)
{
	optData *opt;

#undef	NSEARCH
#define	NSEARCH(list) {							\
	for (opt = list; opt->kind != optTerminator; ++opt)		\
		if (!strcmp(opt->name, name))				\
			return ((int)opt->value);			\
}
	NSEARCH(display);
	NSEARCH(files);
	NSEARCH(general);
	NSEARCH(input);
	NSEARCH(search);

	return (0);
}

/*
 * __vi_create_search_toggles --
 *	Creates the search toggles.  This is so the options and search widgets
 *	share their appearance.
 *
 * PUBLIC: Widget __vi_create_search_toggles __P((Widget, optData[]));
 */
Widget
__vi_create_search_toggles(Widget parent, optData *list)
{
	optData *opt;

	/*
	 * Copy current options information into the search table.
	 *
	 * XXX
	 * This is an O(M*N) loop, but I don't think it matters.
	 */
	for (opt = list; opt->kind != optTerminator; ++opt)
		opt->value = (void *)__vi_toggle(opt->name);

	return (create_toggles(parent, list));
}


#if defined(SelfTest)

#if defined(__STDC__)
static void show_options( Widget w, XtPointer data, XtPointer cbs )
#else
static void show_options( w, data, cbs )
Widget w;
XtPointer	data;
XtPointer	cbs;
#endif
{
    __vi_show_options_dialog( data, "Preferences" );
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

    button = XtVaCreateManagedWidget( "Pop up options dialog",
				      xmPushButtonGadgetClass,
				      rc,
				      0
				      );
    XtAddCallback( button, XmNactivateCallback, show_options, rc );

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
