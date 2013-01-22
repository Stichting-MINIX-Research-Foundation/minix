/*	$NetBSD: m_tags.c,v 1.1.1.2 2008/05/18 14:31:28 aymeric Exp $ */

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
static const char sccsid[] = "Id: m_tags.c,v 8.9 2003/11/05 17:10:00 skimo Exp (Berkeley) Date: 2003/11/05 17:10:00";
#endif /* not lint */

/*
 * This module implements a dialog for navigating the tag stack
 *
 * Interface:
 * void	__vi_show_tags_dialog( Widget parent, String title )
 *	Pops up a Tags dialog.  We allow one per session.  It is not modal.
 *
 * void __vi_push_tag( String text )
 * void __vi_pop_tag()
 * void __vi_clear_tag()
 *	When core changes the tag stack we will change our representation
 *	of it.  When this dialog appears, we need core to send a slew of
 *	messages so we can display the current tag stack.
 *
 * void __vi_set_tag_text( String text )
 *	the text field in the dialog is set to the string.  ideally,
 *	this should be kept in sync with the word following the caret
 *	in the current editor window
 *
 * To Do:
 *	The push-buttons should activate and deactivate according to
 *	the state of the tag stack and the text field.
 *
 *	Need to send VI commands rather than strings
 *
 *	Need IPO command to start/stop asking for the current tag stack
 */


/* context */
#include <X11/X.h>
#include <X11/Intrinsic.h>
#include <Xm/DialogS.h>
#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>
#include <Xm/List.h>
#include <Xm/RowColumn.h>
#include <Xm/PushBG.h>

#if ! defined(SelfTest)
#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>
#include <string.h>

#undef LOCK_SUCCESS
#include "../common/common.h"
#include "../ipc/ip.h"
#include "m_motif.h"
#endif

extern int vi_ofd;


/* globals */

static	Widget	db_tabs = NULL,
		db_text,
		db_list;

static	Boolean	active = False;

typedef struct	{
    String	name;
    Boolean	is_default;
    void	(*cb)();
} ButtonData;

static	void go_to_tag(Widget w);
static	void split_to_tag(Widget w);
static	void pop_tag(Widget w);

static	ButtonData button_data[] = {
    { "Go To Tag",	True,	go_to_tag	},
    { "Split To Tag", 	False,	split_to_tag	},
    { "Pop Tag", 	False,	pop_tag		},
};


/* manage the tags stack list */

void	__vi_pop_tag(void)
{
    if ( ! active ) return;

    XmListDeletePos( db_list, 1 );
}


void	__vi_clear_tag(void)
{
    if ( ! active ) return;

    XmListDeleteAllItems( db_list );
}


void	__vi_push_tag(String text)
{
    XmString	str;

    if ( ! active ) return;

    str = XmStringCreateSimple( text );
    XmListAddItem( db_list, str, 1 );
    XmStringFree( str );
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
				    XmNshowAsDefault,	data->is_default,
				    XmNdefaultButtonShadowThickness,	data->is_default,
				    XmNrightAttachment,	XmATTACH_POSITION,
				    XmNrightPosition,	pos+SpacingRatio-1,
				    0
				    );
	if ( data->is_default )
	    XtVaSetValues( form, XmNdefaultButton, w, 0 );
	XtAddCallback( w, XmNactivateCallback, data->cb, 0 );
	pos += SpacingRatio;
	data++;
    }

    return form;
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


static	void 		set_text_field(Widget w, XtPointer ptr, XmListCallbackStruct *cbs)
{
    String str;

    XmStringGetLtoR( cbs->item, XmSTRING_DEFAULT_CHARSET, &str );
    XmTextFieldSetString( db_text, str );
    XtFree( str );
}


void	__vi_set_tag_text(String text)
{
    if ( active ) XmTextFieldSetString( db_text, text );
}


static	void destroyed(void)
{
#if defined(SelfTest)
    puts( "destroyed" );
#endif

    /* some window managers destroy us upon popdown */
    db_tabs = NULL;
    active  = False;
}


#if defined(__STDC__)
static	void	pop_tag( Widget w )
#else
static	void	pop_tag( w )
	Widget	w;
#endif
{
    static String buffer = ":tagpop";

#if defined(SelfTest)
    printf( "sending command <<%s>>\n", buffer );
    __vi_pop_tag();
#else
    __vi_send_command_string( buffer );
#endif
}


#if defined(__STDC__)
static	void	split_to_tag( Widget w )
#else
static	void	split_to_tag( w )
	Widget	w;
#endif
{
    IP_BUF	ipb;
    String	str;

    str = XmTextFieldGetString( db_text );

#if defined(SelfTest)
    printf( "sending command <<:Tag %s>>\n", str );
#else
    /*
     * XXX
     * This is REALLY sleazy.  We pass the nul along with the
     * string so that the core editor doesn't have to copy the
     * string to get a nul termination.  This should be fixed
     * as part of making the editor fully 8-bit clean.
     */
    ipb.code = VI_TAGSPLIT;
    ipb.str1 = str;
    ipb.len1 = strlen(str) + 1;
    vi_send(vi_ofd, "a", &ipb);
#endif

    XtFree( str );
}


#if defined(__STDC__)
static	void	go_to_tag( Widget w )
#else
static	void	go_to_tag( w )
	Widget	w;
#endif
{
    IP_BUF	ipb;
    String	str;

    str = XmTextFieldGetString( db_text );

#if defined(SelfTest)
    printf( "sending command <<:tag %s>>\n", str );
#else
    /*
     * XXX
     * This is REALLY sleazy.  We pass the nul along with the
     * string so that the core editor doesn't have to copy the
     * string to get a nul termination.  This should be fixed
     * as part of making the editor fully 8-bit clean.
     */
    ipb.code = VI_TAGAS;
    ipb.str1 = str;
    ipb.len1 = strlen(str) + 1;
    vi_send(vi_ofd, "a", &ipb);
#endif

    XtFree( str );
}



/* Draw and display a dialog the describes nvi options */

#if defined(__STDC__)
static	Widget	create_tags_dialog( Widget parent, String title )
#else
static	Widget	create_tags_dialog( parent, title )
	Widget	parent;
	String	title;
#endif
{
    Widget	box, form, form2, form3, buttons;

    /* already built? */
    if ( db_tabs != NULL ) return db_tabs;

    box = XtVaCreatePopupShell( title,
				xmDialogShellWidgetClass,
				parent,
				XmNtitle,		title,
				XmNallowShellResize,	False,
				0
				);
    XtAddCallback( box, XmNpopdownCallback, cancel_cb, 0 );
    XtAddCallback( box, XmNdestroyCallback, destroyed, 0 );

    form = XtVaCreateWidget( "Tags", 
			     xmFormWidgetClass,
			     box,
			     0
			     );

    buttons = create_push_buttons( form, button_data, XtNumber(button_data) );
    XtVaSetValues( buttons,
		   XmNbottomAttachment,	XmATTACH_FORM,
		   0
		   );

    form3 = XtVaCreateWidget( "form",
			      xmFormWidgetClass,
			      form,
			      XmNleftAttachment,	XmATTACH_FORM,
			      XmNrightAttachment,	XmATTACH_FORM,
			      XmNbottomAttachment,	XmATTACH_WIDGET,
			      XmNbottomWidget,		buttons,
			      0
			      );

    form2 = XtVaCreateWidget( "form",
			      xmFormWidgetClass,
			      form,
			      XmNtopAttachment,		XmATTACH_FORM,
			      XmNleftAttachment,	XmATTACH_FORM,
			      XmNrightAttachment,	XmATTACH_FORM,
			      XmNbottomAttachment,	XmATTACH_WIDGET,
			      XmNbottomWidget,		form3,
			      0
			      );

    db_list = XtVaCreateManagedWidget( "list",
				xmListWidgetClass,
				form2,
				XmNtopAttachment,	XmATTACH_FORM,
				XmNleftAttachment,	XmATTACH_POSITION,
				XmNleftPosition,	20,
				XmNrightAttachment,	XmATTACH_FORM,
				XmNbottomAttachment,	XmATTACH_FORM,
#if defined(SelfTest)
				XmNvisibleItemCount,	5,
#endif
				XmNselectionPolicy,	XmSINGLE_SELECT,
				0
				);
    XtAddCallback( db_list, XmNsingleSelectionCallback, set_text_field, 0 );
    XtAddCallback( db_list, XmNdefaultActionCallback, go_to_tag, 0 );

    XtVaCreateManagedWidget( "Tag Stack",
			     xmLabelGadgetClass,
			     form2,
			     XmNtopAttachment,		XmATTACH_FORM,
			     XmNbottomAttachment,	XmATTACH_FORM,
			     XmNleftAttachment,		XmATTACH_FORM,
			     XmNrightAttachment,	XmATTACH_POSITION,
			     XmNrightPosition,		20,
			     XmNalignment,		XmALIGNMENT_END,
			     0
			     );

    XtVaCreateManagedWidget( "Tag",
			     xmLabelGadgetClass,
			     form3,
			     XmNtopAttachment,		XmATTACH_FORM,
			     XmNbottomAttachment,	XmATTACH_FORM,
			     XmNleftAttachment,		XmATTACH_FORM,
			     XmNrightAttachment,	XmATTACH_POSITION,
			     XmNrightPosition,		20,
			     XmNalignment,		XmALIGNMENT_END,
			     0
			     );

    db_text = XtVaCreateManagedWidget( "text",
			    xmTextFieldWidgetClass,
			    form3,
			    XmNtopAttachment,		XmATTACH_FORM,
			    XmNbottomAttachment,	XmATTACH_FORM,
			    XmNrightAttachment,		XmATTACH_FORM,
			    XmNleftAttachment,		XmATTACH_POSITION,
			    XmNleftPosition,		20,
			    0
			    );
    XtAddCallback( db_text, XmNactivateCallback, go_to_tag, 0 );

    /* keep this global, we might destroy it later */
    db_tabs = form;

    /* done */
    XtManageChild( form3 );
    XtManageChild( form2 );
    return form;
}



/* module entry point
 *	__vi_show_tags_dialog( parent, title )
 */

#if defined(__STDC__)
void	__vi_show_tags_dialog( Widget parent, String title )
#else
void	__vi_show_tags_dialog( parent, title )
Widget	parent;
String	title;
#endif
{
    Widget 	db = create_tags_dialog( parent, title ),
		shell = XtParent(db);

    XtManageChild( db );

    /* get the current window's text */
    __vi_set_tag_text( (String) __vi_get_word_at_caret( NULL ) );

    /* TODO:  ask vi core for the current tag stack now */

    /* leave this guy up (or just raise it) */
    XtPopup( shell, XtGrabNone );
    XMapRaised( XtDisplay( shell ), XtWindow( shell ) );

    active = True;
}



#if defined(SelfTest)

#if XtSpecificationRelease == 4
#define	ArgcType	Cardinal *
#else
#define	ArgcType	int *
#endif

static	void	add_a_tag( Widget w )
{
    static	String	tags[] = { "first", "second", "this is the third" };
    static	int	i = 0;

    __vi_push_tag( tags[i] );
    i = (i+1) % XtNumber(tags);
}

#if defined(__STDC__)
static void show_tags( Widget w, XtPointer data, XtPointer cbs )
#else
static void show_tags( w, data, cbs )
Widget w;
XtPointer	data;
XtPointer	cbs;
#endif
{
    __vi_show_tags_dialog( data, "Tags" );
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

    button = XtVaCreateManagedWidget( "Pop up tags dialog",
				      xmPushButtonGadgetClass,
				      rc,
				      0
				      );
    XtAddCallback( button, XmNactivateCallback, show_tags, rc );

    button = XtVaCreateManagedWidget( "Add a tag",
				      xmPushButtonGadgetClass,
				      rc,
				      0
				      );
    XtAddCallback( button, XmNactivateCallback, add_a_tag, rc );

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
