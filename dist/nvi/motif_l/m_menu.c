/*	$NetBSD: m_menu.c,v 1.1.1.2 2008/05/18 14:31:27 aymeric Exp $ */

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
static const char sccsid[] = "Id: m_menu.c,v 8.26 2003/11/05 17:09:59 skimo Exp (Berkeley) Date: 2003/11/05 17:09:59";
#endif /* not lint */

#include <sys/queue.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/FileSB.h>
#include <Xm/SelectioB.h>

#include <bitstring.h>
#include <stdio.h>

#undef LOCK_SUCCESS
#include "../common/common.h"
#include "../ipc/ip.h"
#include "m_motif.h"

extern int vi_ofd;

/* save this for creation of children */
static	Widget	main_widget = NULL;

/* This module defines the menu structure for vi.  Each menu
 * item has an action routine associated with it.  For the most
 * part, those actions will simply call vi_send with vi commands.
 * others will pop up file selection dialogs and use them for
 * vi commands, and other will have to have special actions.
 *
 * Future:
 *	vi core will have to let us know when to be sensitive
 *	change VI_STRING to VI_COMMAND so that menu actions cannot
 *		be confusing when in insert mode
 *	need VI_CUT, VI_COPY, and VI_PASTE to perform the appropriate
 *		operations on the visible text of yank buffer.  VI_COPY
 *		is likely a NOP, but it will make users happy
 *	add mnemonics
 *	add accelerators
 *	implement file selection dialog boxes
 *	implement string prompt dialog boxes (e.g. for 'find')
 *
 * Interface:
 *	Widget	create_menubar( Widget parent ) creates and returns the
 *		X menu structure.  The caller can place this
 *		anywhere in the widget heirarchy.
 */

#define	BufferSize	1024

/*
 * __vi_send_command_string --
 *	Utility:  Send a menu command to vi
 *
 * Future:
 * Change VI_STRING to VI_COMMAND so that menu actions cannot be confusing
 * when in insert mode.
 *
 * XXX
 * THIS SHOULD GO AWAY -- WE SHOULDN'T SEND UNINTERPRETED STRINGS TO THE
 * CORE.
 *
 * PUBLIC: void __vi_send_command_string __P((String));
 */
void
__vi_send_command_string(String str)
{
    IP_BUF	ipb;
    char	buffer[BufferSize];

    /* Future:  Need VI_COMMAND so vi knows this is not text to insert
     * At that point, appending a cr/lf will not be necessary.  For now,
     * append iff we are a colon or slash command.  Of course, if we are in
     * insert mode, all bets are off.
     */
    strcpy( buffer, str );
    switch ( *str ) {
	case ':':
	case '/':
	    strcat( buffer, "\n" );
	    break;
    }

    ipb.code = VI_STRING;
    ipb.str1 = buffer;
    ipb.len1 = strlen(buffer);
    vi_send(vi_ofd, "a", &ipb);
}


/* Utility:  beep for unimplemented command */

#if defined(__STDC__)
static	void	send_beep( Widget w )
#else
static	void	send_beep( w )
	Widget	w;
#endif
{
    XBell( XtDisplay(w), 0 );
}


/*
 * __vi_cancel_cb --
 *	Utility:  make a dialog box go Modal
 *
 * PUBLIC: void __vi_cancel_cb __P((Widget, XtPointer, XtPointer));
 */
static	Bool	have_answer;
void
__vi_cancel_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	have_answer = True;
}

/*
 * PUBLIC: void __vi_modal_dialog __P((Widget));
 */
void
__vi_modal_dialog(Widget db)
{
    XtAppContext	ctx;

    /* post the dialog */
    XtManageChild( db );
    XtPopup( XtParent(db), XtGrabExclusive );

    /* wait for a response */
    ctx = XtWidgetToApplicationContext(db);
    XtAddGrab( XtParent(db), TRUE, FALSE );
    for ( have_answer = False; ! have_answer; )
	XtAppProcessEvent( ctx, XtIMAll );

    /* done with db */
    XtPopdown( XtParent(db) );
    XtRemoveGrab( XtParent(db) );
}


/* Utility:  Get a file (using standard File Selection Dialog Box) */

static	String	file_name;


#if defined(__STDC__)
static	void		ok_file_name( Widget w,
				      XtPointer client_data,
				      XtPointer call_data
				      )
#else
static	void		ok_file_name( w, client_data, call_data )
	Widget		w;
	XtPointer	client_data;
	XtPointer	call_data;
#endif
{
    XmFileSelectionBoxCallbackStruct	*cbs;

    cbs = (XmFileSelectionBoxCallbackStruct *) call_data;
    XmStringGetLtoR( cbs->value, XmSTRING_DEFAULT_CHARSET, &file_name );

    have_answer = True;
}


#if defined(__STDC__)
static	String	get_file( Widget w, String prompt )
#else
static	String	get_file( w, prompt )
	Widget	w;
	String	prompt;
#endif
{
    /* make it static so we can reuse it */
    static	Widget	db;

    /* our return parameter */
    if ( file_name != NULL ) {
	XtFree( file_name );
	file_name = NULL;
    }

    /* create one? */
    if ( db == NULL ){ 
	db = XmCreateFileSelectionDialog( main_widget, "file", NULL, 0 );
	XtAddCallback( db, XmNokCallback, ok_file_name, NULL );
	XtAddCallback( db, XmNcancelCallback, __vi_cancel_cb, NULL );
    }

    /* use the title as a prompt */
    XtVaSetValues( XtParent(db), XmNtitle, prompt, 0 );

    /* wait for a response */
    __vi_modal_dialog( db );

    /* done */
    return file_name;
}


/*
 * file_command --
 *	Get a file name and send it with the command to the core.
 */
static void
file_command(Widget w, int code, String prompt)
{
	IP_BUF ipb;
	char *file;

	if ((file = get_file(w, prompt)) != NULL) {
		ipb.code = code;
		ipb.str1 = file;
		ipb.len1 = strlen(file);
		vi_send(vi_ofd, "a", &ipb);
	}
}


/*
 * Menu action routines (one per menu entry)
 *
 * These are in the order in which they appear in the menu structure.
 */
static void
ma_edit_file(Widget w, XtPointer call_data, XtPointer client_data)
{
	file_command(w, VI_EDIT, "Edit");
}

static void
ma_split(Widget w, XtPointer call_data, XtPointer client_data)
{
	file_command(w, VI_EDITSPLIT, "Edit");
}

static void
ma_save(Widget w, XtPointer call_data, XtPointer client_data)
{
	IP_BUF ipb;

	ipb.code = VI_WRITE;
	(void)vi_send(vi_ofd, NULL, &ipb);
}

static void
ma_save_as(Widget w, XtPointer call_data, XtPointer client_data)
{
	file_command(w, VI_WRITEAS, "Save As");
}

static void
ma_wq(Widget w, XtPointer call_data, XtPointer client_data)
{
	IP_BUF ipb;

	ipb.code = VI_WQ;
	(void)vi_send(vi_ofd, NULL, &ipb);
}

static void
ma_quit(Widget w, XtPointer call_data, XtPointer client_data)
{
	IP_BUF ipb;

	ipb.code = VI_QUIT;
	(void)vi_send(vi_ofd, NULL, &ipb);
}

static void
ma_undo(Widget w, XtPointer call_data, XtPointer client_data)
{
	IP_BUF ipb;

	ipb.code = VI_UNDO;
	(void)vi_send(vi_ofd, NULL, &ipb);
}

#if defined(__STDC__)
static void	ma_cut(	Widget w,
			XtPointer call_data,
			XtPointer client_data
			)
#else
static	void		ma_cut( w, call_data, client_data )
	Widget		w;
	XtPointer	call_data;
	XtPointer	client_data;
#endif
{
    /* future */
    send_beep( w );
}


#if defined(__STDC__)
static	void	ma_copy(	Widget w,
				XtPointer call_data,
				XtPointer client_data
				)
#else
static	void		ma_copy( w, call_data, client_data )
	Widget		w;
	XtPointer	call_data;
	XtPointer	client_data;
#endif
{
    /* future */
    send_beep( w );
}


#if defined(__STDC__)
static	void	ma_paste(	Widget w,
				XtPointer call_data,
				XtPointer client_data
				)
#else
static	void		ma_paste( w, call_data, client_data )
	Widget		w;
	XtPointer	call_data;
	XtPointer	client_data;
#endif
{
    /* future */
    send_beep( w );
}

static void
ma_find(Widget w, XtPointer call_data, XtPointer client_data)
{
	__vi_show_search_dialog( main_widget, "Find" );
}

static void
ma_find_next(Widget w, XtPointer call_data, XtPointer client_data)
{
	__vi_search( w );
}

static void
ma_tags(Widget w, XtPointer call_data, XtPointer client_data)
{
	__vi_show_tags_dialog( main_widget, "Tag Stack" );
}

static void
ma_tagpop(Widget w, XtPointer call_data, XtPointer client_data)
{
	__vi_send_command_string( "\024" );
}

static void
ma_tagtop(Widget w, XtPointer call_data, XtPointer client_data)
{
	__vi_send_command_string( ":tagtop" );
}

#if defined(__STDC__)
static void	ma_preferences(	Widget w,
				XtPointer call_data,
				XtPointer client_data
				)
#else
static	void		ma_preferences( w, call_data, client_data )
	Widget		w;
	XtPointer	call_data;
	XtPointer	client_data;
#endif
{
	__vi_show_options_dialog( main_widget, "Preferences" );
}


/* Menu construction routines */

typedef	struct {
    String	title;
    void	(*action)();
    String	accel;		/* for Motif */
    String	accel_text;	/* for the user */
} pull_down;

typedef	struct {
    char	mnemonic;
    String	title;
    pull_down	*actions;
} menu_bar;

static	pull_down	file_menu[] = {
    { "Edit File...",		ma_edit_file,	"Alt<Key>e",	"Alt+E"	},
    { "",			NULL,		NULL,	NULL	},
    { "Split Window...",	ma_split,	NULL,	NULL	},
    { "",			NULL,		NULL,	NULL	},
    { "Save ",			ma_save,	"Alt<Key>s",	"Alt+S"	},
    { "Save As...",		ma_save_as,	"Shift Alt<Key>s",	"Shift+Alt+S"	},
    { "",			NULL,		NULL,	NULL	},
    { "Write and Quit",		ma_wq,		"Shift Alt<Key>q",	"Shift+Alt+Q"	},
    { "Quit",			ma_quit,	"Alt<Key>q",	"Alt+Q"	},
    { NULL,			NULL,		NULL,	NULL	},
};

static	pull_down	edit_menu[] = {
    { "Undo",			ma_undo,	NULL,	NULL	},
    { "",			NULL,		NULL,	NULL	},
    { "Cut",			ma_cut,		"Alt<Key>x",	"Alt+X"	},
    { "Copy",			ma_copy,	"Alt<Key>c",	"Alt+C"	},
    { "Paste",			ma_paste,	"Alt<Key>v",	"Alt+V"	},
    { "",			NULL,		NULL,	NULL	},
    { "Find",			ma_find,	"Alt<Key>f",	"Alt+F"	},
    { "Find Next",		ma_find_next,	"Alt<Key>g",	"Alt+G"	},
    { NULL,			NULL,		NULL,	NULL	},
};

static	pull_down	options_menu[] = {
    { "Preferences",		ma_preferences,	NULL,	NULL	},
    { "Command Mode Maps",	NULL,		NULL,	NULL	},
    { "Insert Mode Maps",	NULL,		NULL,	NULL	},
    { NULL,			NULL,		NULL,	NULL	},
};

static	pull_down	tag_menu[] = {
    { "Show Tag Stack",		ma_tags,	"Alt<Key>t",	"Alt+T"	},
    { "",			NULL,		NULL,	NULL	},
    { "Pop Tag",		ma_tagpop,	NULL,	NULL	},
    { "Clear Stack",		ma_tagtop,	NULL,	NULL	},
    { NULL,			NULL,		NULL,	NULL	},
};

static	pull_down	help_menu[] = {
    { NULL,			NULL,		NULL,	NULL	},
};

static	menu_bar	main_menu[] = {
    { 'F',	"File",		file_menu	},
    { 'E',	"Edit", 	edit_menu	},
    { 'O',	"Options",	options_menu	},
    { 'T',	"Tag",		tag_menu	},
    { 'H',	"Help",		help_menu	},
    { 0,	NULL,		NULL		},
};


#if defined(__STDC__)
static	void	add_entries( Widget parent, pull_down *actions )
#else
static	void		add_entries( parent, actions )
	Widget		parent;
	pull_down	*actions;
#endif
{
    Widget	w;
    XmString	str;

    for ( ; actions->title != NULL; actions++ ) {

	/* a separator? */
	if ( *actions->title != '\0' ) {
	    w = XmCreatePushButton( parent, actions->title, NULL, 0 );
	    if ( actions->action == NULL )
		XtSetSensitive( w, False );
	    else
		XtAddCallback(  w,
				XmNactivateCallback,
				(XtCallbackProc) actions->action,
				actions
				);
	    if ( actions->accel != NULL ) {
		str = XmStringCreateSimple( actions->accel_text );
		XtVaSetValues(	w,
				XmNaccelerator,		actions->accel,
				XmNacceleratorText,	str,
				0
				);
		XmStringFree( str );
	    }
	}
	else {
	    w = XmCreateSeparator( parent, "separator", NULL, 0 );
	}

	XtManageChild( w );

    }
}

/*
 * vi_create_menubar --
 *
 * PUBLIC: Widget vi_create_menubar __P((Widget));
 */
Widget
vi_create_menubar(Widget parent)
{
    Widget	menu, pull, button;
    menu_bar	*ptr;

    /* save this for creation of children */
    main_widget = parent;

    menu = XmCreateMenuBar( parent, "Menu", NULL, 0 );

    for ( ptr=main_menu; ptr->title != NULL; ptr++ ) {

	pull = XmCreatePulldownMenu( menu, "pull", NULL, 0 );
	add_entries( pull, ptr->actions );
	button = XmCreateCascadeButton( menu, ptr->title, NULL, 0 );
	XtVaSetValues( button, XmNsubMenuId, pull, 0 );

	if ( strcmp( ptr->title, "Help" ) == 0 )
	    XtVaSetValues( menu, XmNmenuHelpWidget, button, 0 );

#if 0
	/* These screw up accelerator processing.  Punt for now */
	if ( ptr->mnemonic )
	    XtVaSetValues( button, XmNmnemonic, ptr->mnemonic, 0 );
#endif

	XtManageChild( button );
    }

    return menu;
}
