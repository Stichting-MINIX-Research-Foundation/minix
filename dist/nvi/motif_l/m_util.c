/*	$NetBSD: m_util.c,v 1.1.1.2 2008/05/18 14:31:29 aymeric Exp $ */

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
static const char sccsid[] = "Id: m_util.c,v 8.12 2003/11/05 17:10:00 skimo Exp (Berkeley) Date: 2003/11/05 17:10:00";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>

#include <bitstring.h>
#include <stdio.h>
#include <string.h>

#undef LOCK_SUCCESS
#include "../common/common.h"
#include "../ipc/ip.h"
#include "m_motif.h"


/* Widget hierarchy routines
 *
 * void XutShowWidgetTree( FILE *fp, Widget root, int indent )
 *	prints the widgets and sub-widgets beneath the named root widget
 */
#ifdef DEBUG
#if defined(__STDC__)
void	XutShowWidgetTree( FILE *fp, Widget root, int indent )
#else
void	XutShowWidgetTree( fp, root, indent )
FILE	*fp;
Widget	root;
int	indent;
#endif
{
#if ! defined(DECWINDOWS)
    WidgetList	l, l2;
    int		i, count = 0;

    /* print where we are right now */
    fprintf( fp,
	     "%*.*swidget => 0x%x name => \"%s\"\n\r",
	     indent,
	     indent,
	     "",
	     root,
	     XtName(root) );

    /* get the correct widget values */
    XtVaGetValues( root, XtNchildren, &l, 0 );
    XtVaGetValues( root, XtNchildren, &l2, 0 );
    XtVaGetValues( root, XtNnumChildren, &count, 0 );

    /* print the sub-widgets */
    for ( i=0; i<count; i++ ) {
	XutShowWidgetTree( fp, l[i], indent+4 );
    }

    /* tidy up if this thing contained children */
    if ( count > 0 ) {
	/* we may or may not have to free the children */
	if ( l != l2 ) {
	    XtFree( (char *) l );
	    XtFree( (char *) l2 );
	}
    }
#endif
}
#endif


/* Utilities for converting X resources...
 *
 * __XutConvertResources( Widget, String root, XutResource *, int count )
 *	The resource block is loaded with converted values
 *	If the X resource does not exist, no change is made to the value
 *	'root' should be the application name.
 *
 * PUBLIC: void __XutConvertResources __P((Widget, String, XutResource *, int));
 */
void __XutConvertResources(Widget wid, String root, XutResource *resources, int count)
{
    int		i;
    XrmValue	from, to;
    String	kind;
    Boolean	success = True;

    /* for each resource */
    for (i=0; i<count; i++) {

	/* is there a value in the database? */
	from.addr = XGetDefault( XtDisplay(wid), root, resources[i].name );
	if ( from.addr == NULL || *from.addr == '\0' )
	    continue;

	/* load common parameters */
	from.size = strlen( from.addr );
	to.addr   = resources[i].value;

	/* load type-specific parameters */
	switch ( resources[i].kind ) {
	    case XutRKinteger:
		to.size	= sizeof(int);
		kind	= XtRInt;
		break;

	    case XutRKboolean:
		/* String to Boolean */
		to.size	= sizeof(Boolean);
		kind	= XtRBoolean;
		break;

	    case XutRKfont:
		/* String to Font structure */
		to.size	= sizeof(XFontStruct *);
		kind	= XtRFontStruct;
		break;

	    case XutRKpixelBackup:
		/* String to Pixel backup algorithm */
		if ( success ) continue;
		/* FALL through */

	    case XutRKpixel:
		/* String to Pixel */
		to.size	= sizeof(Pixel);
		kind	= XtRPixel;
		break;

	    case XutRKcursor:
		/* String to Cursor */
		to.size	= sizeof(int);
		kind	= XtRCursor;
		break;

	    default:
		return;
	}

	/* call the converter */
	success = XtConvertAndStore( wid, XtRString, &from, kind, &to );
    }
}
