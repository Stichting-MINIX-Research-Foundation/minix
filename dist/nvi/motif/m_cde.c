/*	$NetBSD: m_cde.c,v 1.1.1.2 2008/05/18 14:31:25 aymeric Exp $ */

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
static const char sccsid[] = "Id: m_cde.c,v 8.11 2003/11/05 17:09:58 skimo Exp (Berkeley) Date: 2003/11/05 17:09:58";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <bitstring.h>
#include <stdio.h>

#undef LOCK_SUCCESS
#include "../common/common.h"
#include "extern.h"

#if SelfTest
#define	_TRACE( x )	printf x
#else
#define	_TRACE( x )
#endif

#define	Required	10
#define	Useful		3
#define	Present		(Required+Useful)

static struct {
    char	*name;
    int		value;
} Atoms[] = {
    { "_VUE_SM_WINDOW_INFO",	Required,	/* "vue" */		},
    { "_DT_SM_WINDOW_INFO",	Required,	/* "dtwm" */		},
    { "_SUN_WM_PROTOCOLS",	Useful,		/* "olwm" */		},
    { "_MOTIF_WM_INFO",		Useful,		/* "mwm/dtwm" */	},
};

/*
 * is_cde --
 *
 * When running under CDE (or VUE on HPUX) applications should not define
 * fallback colors (or fonts).  The only way to tell is to check the atoms
 * attached to the server.  This routine does that.
 *
 * PUBLIC: int is_cde __P((Display *));
 */
int
is_cde(Display *d)
{
    int			i, r, format;
    unsigned long	nitems, remaining;
    unsigned char	*prop;
    Window		root = DefaultRootWindow( d );
    Atom		atom, type;
    int			retval = 0;

    _TRACE( ( "Root window is 0x%x\n", root ) );

    /* create our atoms */
    for (i=0; i< (sizeof(Atoms)/sizeof(Atoms[0])); i++ ) {

	atom = XInternAtom( d, Atoms[i].name, True );
	if ( atom == None ) {
	    _TRACE( ( "Atom \"%s\" does not exist\n", Atoms[i].name ) );
	    continue;
	}

	/* what is the value of the atom? */
	r = XGetWindowProperty( d,
				root,
				atom,
				0,
				1024,
				False,			/* do not delete */
				AnyPropertyType,	/* request type */
				&type,			/* actual type */
				&format,		/* byte size */
				&nitems,		/* number of items */
				&remaining,		/* anything left over? */
				&prop			/* the data itself */
				);
	if ( r != Success ) {
	    _TRACE( ( "Atom \"%s\" cannot be converted to string\n", Atoms[i].name ) );
	    continue;
	}

	retval += Atoms[i].value;


#if SelfTest
	_TRACE( ( "Atom \"%s\"\n", Atoms[i].name ) );

	switch ( type ) {
	    case 0:
		_TRACE( ( "\t does not exist on the root window\n", Atoms[i].name ) );

	    case XA_ATOM:
		for (j=0; j<nitems; j++) {
		    name = XGetAtomName( d, ((Atom *) prop)[j] );
		    _TRACE( ( "\t[%d] = \"%s\"\n", j, name ) );
		    XFree( name );
		}
		break;

	    case XA_STRING:
		_TRACE( ( "\t is a string\n", Atoms[i].name ) );
		break;

	    default:
		_TRACE( ( "\tunknown type %s\n", XGetAtomName( d, type ) ) );
		break;
	}
#endif

	/* done */
	XFree( (caddr_t) prop );

    }

    _TRACE( ( "retval = %d\n", retval ) );
    return retval >= Present;
}

#if SelfTest

main () {
    Display *d = XOpenDisplay( 0 );

    if ( d == 0 )
	printf ( "Could not open display\n" );
    else {
	printf ( "_vi_is_cde() == %d\n", _vi_is_cde( d ) );
	XCloseDisplay( d );
    }
}
#endif
