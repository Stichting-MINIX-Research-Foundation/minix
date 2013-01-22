/*	$NetBSD: m_copypaste.c,v 1.1.1.2 2008/05/18 14:31:26 aymeric Exp $ */

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
static const char sccsid[] = "Id: m_copypaste.c,v 8.10 2003/11/05 17:09:59 skimo Exp (Berkeley) Date: 2003/11/05 17:09:59";
#endif /* not lint */

/* ICCCM Cut and paste Utilities: */

#include	<sys/types.h>
#include	<sys/queue.h>

#include	<X11/X.h>
#include	<X11/Intrinsic.h>
#include	<X11/Xatom.h>

#include	<bitstring.h>
#include	<stdio.h>

#undef LOCK_SUCCESS
#include	"../common/common.h"
#include	"../ipc/ip.h"
#include	"m_motif.h"

typedef	int	(*PFI)();

static	PFI	icccm_paste,
		icccm_copy,
		icccm_clear,
		icccm_error;

/*
 * InitCopyPaste --
 *
 * PUBLIC: void __vi_InitCopyPaste
 * PUBLIC:    __P((int (*)(), int (*)(), int (*)(), int (*)())); 
 */
void
__vi_InitCopyPaste(PFI f_copy, PFI f_paste, PFI f_clear, PFI f_error)
{
    icccm_paste	= f_paste;
    icccm_clear	= f_clear;
    icccm_copy	= f_copy;
    icccm_error	= f_error;
}


#if defined(__STDC__)
static	void	peekProc( Widget widget,
			  void *data,
			  Atom *selection,
			  Atom *type,
			  void *value,
			  unsigned long *length,
			  int *format
			  )
#else
static	void	peekProc( widget, data, selection, type, value, length, format )
	Widget	widget;
	void	*data;
	Atom	*selection, *type;
	void	*value;
	unsigned long *length;
	int	*format;
#endif
{
    if ( *type == 0 )
	(*icccm_error)( stderr, "Nothing in the primary selection buffer");
    else if ( *type != XA_STRING )
	(*icccm_error)( stderr, "Unknown type return from selection");
    else
	XtFree( value );
}


#if 0
#if defined(__STDC__)
void 	_vi_AcquireClipboard( Widget wid )
#else
void 	_vi_AcquireClipboard( wid )
Widget	wid;
#endif
{
    XtGetSelectionValue( wid,
			 XA_PRIMARY,
			 XA_STRING,
			 (XtSelectionCallbackProc) peekProc,
			 NULL,
			 XtLastTimestampProcessed( XtDisplay(wid) )
			 );
}
#endif


#if defined(__STDC__)
static	void	loseProc( Widget widget )
#else
static	void	loseProc( widget )
	Widget	widget;
#endif
{
    /* we have lost ownership of the selection.  clear it */
    (*icccm_clear)( widget );

    /* also participate in the protocols */
    XtDisownSelection(	widget,
			XA_PRIMARY,
			XtLastTimestampProcessed( XtDisplay(widget) )
			);
}


#if defined(__STDC__)
static	int convertProc( Widget widget,
		     Atom *selection,
		     Atom *target,
		     Atom *type,
		     void **value,
		     int *length,
		     int *format
		     )
#else
static	int convertProc( widget, selection, target, type, value, length, format )
Widget	widget;
Atom	*selection, *target, *type;
void	**value;
int	*length;
int	*format;
#endif
{
    String	buffer;
    int		len;

    /* someone wants a copy of the selection.  is there one? */
    (*icccm_copy)( &buffer, &len );
    if ( len == 0 ) return False;

    /* do they want the string? */
    if ( *target == XA_STRING ) {
	*length = len;
	*value  = (void *) XtMalloc( len );
	*type   = XA_STRING;
	*format = 8;
	memcpy( (char *) *value, buffer, *length );
	return True;
	}

    /* do they want the length? */
    if ( *target == XInternAtom( XtDisplay(widget), "LENGTH", FALSE) ) {
	*length = 1;
	*value  = (void *) XtMalloc( sizeof(int) );
	*type   = *target;
	*format = 32;
	* ((int *) *value) = len;
	return True;
	}

    /* we lose */
    return False;
}

/*
 * __vi_AcquirePrimary --
 *
 * PUBLIC: void	__vi_AcquirePrimary __P((Widget));
 */
void 
__vi_AcquirePrimary(Widget widget)
{
    /* assert we own the primary selection */
    XtOwnSelection( widget,
		    XA_PRIMARY,
		    XtLastTimestampProcessed( XtDisplay(widget) ),
		    (XtConvertSelectionProc) convertProc,
		    (XtLoseSelectionProc) loseProc,
		    NULL
		    );

#if defined(OPENLOOK)
    /* assert we also own the clipboard */
    XtOwnSelection( widget,
		    XA_CLIPBOARD( XtDisplay(widget) ),
		    XtLastTimestampProcessed( XtDisplay(widget) ),
		    convertProc,
		    loseProc,
		    NULL
		    );
#endif
}


#if defined(__STDC__)
static	void	gotProc( Widget widget,
			 void *data,
			 Atom *selection,
			 Atom *type,
			 void *value,
			 unsigned long *length,
			 int *format
			 )
#else
static	void	gotProc( widget, data, selection, type, value, length, format )
	Widget	widget;
	void	*data;
	Atom	*selection, *type;
	void	*value;
	unsigned long *length;
	int	*format;
#endif
{
    if ( *type == 0 )
	(*icccm_error)( stderr, "Nothing in the primary selection buffer");
    else if ( *type != XA_STRING )
	(*icccm_error)( stderr, "Unknown type return from selection");
    else {
	(*icccm_paste)( widget, value, *length );
	XtFree( value );
    }
}

/*
 * __vi_PasteFromClipboard --
 *
 * PUBLIC: void	__vi_PasteFromClipboard __P((Widget));
 */
void
__vi_PasteFromClipboard(Widget widget)
{
    XtGetSelectionValue( widget,
			 XA_PRIMARY,
			 XA_STRING,
			 (XtSelectionCallbackProc) gotProc,
			 NULL,
			 XtLastTimestampProcessed( XtDisplay(widget) )
			 );
}
