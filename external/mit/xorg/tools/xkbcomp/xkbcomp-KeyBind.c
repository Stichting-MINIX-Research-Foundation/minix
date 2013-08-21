/*	$NetBSD: xkbcomp-KeyBind.c,v 1.1.1.1 2008/07/29 05:01:23 mrg Exp $	*/

/* $Xorg: KeyBind.c,v 1.4 2001/02/09 02:03:34 xorgcvs Exp $ */
/* 

Copyright 1985, 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/* $XFree86: xc/lib/X11/KeyBind.c,v 1.5 2001/12/14 19:54:02 dawes Exp $ */

/* Beware, here be monsters (still under construction... - JG */

#define NEED_EVENTS
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#define XK_MISCELLANY
#define XK_LATIN1
#define XK_LATIN2
#define XK_LATIN3
#define XK_LATIN4
#define XK_CYRILLIC
#define XK_GREEK
#define XK_ARMENIAN
#define XK_XKB_KEYS
#include <X11/keysymdef.h>
#include <stdio.h>

#include "XKBlib.h"

#ifdef USE_OWN_COMPOSE
#include "imComp.h"

#endif

#ifdef XKB
#define	XKeycodeToKeysym	_XKeycodeToKeysym
#define	XKeysymToKeycode	_XKeysymToKeycode
#define	XLookupKeysym		_XLookupKeysym
#define	XRefreshKeyboardMapping	_XRefreshKeyboardMapping
#define	XLookupString		_XLookupString
/* XKBBind.c */
#else
#define	XkbKeysymToModifiers	_XKeysymToModifiers
#endif

#define AllMods (ShiftMask|LockMask|ControlMask| \
		 Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)

#if 0	/* { */

static void ComputeMaskFromKeytrans();
int _XKeyInitialize();

struct _XKeytrans {
	struct _XKeytrans *next;/* next on list */
	char *string;		/* string to return when the time comes */
	int len;		/* length of string (since NULL is legit)*/
	KeySym key;		/* keysym rebound */
	unsigned int state;	/* modifier state */
	KeySym *modifiers;	/* modifier keysyms you want */
	int mlen;		/* length of modifier list */
};

static KeySym
#if NeedFunctionPrototypes
KeyCodetoKeySym(register Display *dpy, KeyCode keycode, int col)
#else
KeyCodetoKeySym(dpy, keycode, col)
    register Display *dpy;
    KeyCode keycode;
    int col;
#endif
{
    register int per = dpy->keysyms_per_keycode;
    register KeySym *syms;
    KeySym lsym, usym;

    if ((col < 0) || ((col >= per) && (col > 3)) ||
	((int)keycode < dpy->min_keycode) || ((int)keycode > dpy->max_keycode))
      return NoSymbol;

    syms = &dpy->keysyms[(keycode - dpy->min_keycode) * per];
    if (col < 4) {
	if (col > 1) {
	    while ((per > 2) && (syms[per - 1] == NoSymbol))
		per--;
	    if (per < 3)
		col -= 2;
	}
	if ((per <= (col|1)) || (syms[col|1] == NoSymbol)) {
	    XConvertCase(syms[col&~1], &lsym, &usym);
	    if (!(col & 1))
		return lsym;
	    else if (usym == lsym)
		return NoSymbol;
	    else
		return usym;
	}
    }
    return syms[col];
}

#if NeedFunctionPrototypes
KeySym
XKeycodeToKeysym(Display *dpy,
#if NeedWidePrototypes
		 unsigned int kc,
#else
		 KeyCode kc,
#endif
		 int col)
#else
KeySym
XKeycodeToKeysym(dpy, kc, col)
    Display *dpy;
    KeyCode kc;
    int col;
#endif
{
    if ((! dpy->keysyms) && (! _XKeyInitialize(dpy)))
	return NoSymbol;
    return KeyCodetoKeySym(dpy, kc, col);
}

KeyCode
XKeysymToKeycode(dpy, ks)
    Display *dpy;
    KeySym ks;
{
    register int i, j;

    if ((! dpy->keysyms) && (! _XKeyInitialize(dpy)))
	return (KeyCode) 0;
    for (j = 0; j < dpy->keysyms_per_keycode; j++) {
	for (i = dpy->min_keycode; i <= dpy->max_keycode; i++) {
	    if (KeyCodetoKeySym(dpy, (KeyCode) i, j) == ks)
		return i;
	}
    }
    return 0;
}

KeySym
XLookupKeysym(event, col)
    register XKeyEvent *event;
    int col;
{
    if ((! event->display->keysyms) && (! _XKeyInitialize(event->display)))
	return NoSymbol;
    return KeyCodetoKeySym(event->display, event->keycode, col);
}

static void
ResetModMap(dpy)
    Display *dpy;
{
    register XModifierKeymap *map;
    register int i, j, n;
    KeySym sym;
    register struct _XKeytrans *p;

    map = dpy->modifiermap;
    /* If any Lock key contains Caps_Lock, then interpret as Caps_Lock,
     * else if any contains Shift_Lock, then interpret as Shift_Lock,
     * else ignore Lock altogether.
     */
    dpy->lock_meaning = NoSymbol;
    /* Lock modifiers are in the second row of the matrix */
    n = 2 * map->max_keypermod;
    for (i = map->max_keypermod; i < n; i++) {
	for (j = 0; j < dpy->keysyms_per_keycode; j++) {
	    sym = KeyCodetoKeySym(dpy, map->modifiermap[i], j);
	    if (sym == XK_Caps_Lock) {
		dpy->lock_meaning = XK_Caps_Lock;
		break;
	    } else if (sym == XK_Shift_Lock) {
		dpy->lock_meaning = XK_Shift_Lock;
	    }
	    else if (sym == XK_ISO_Lock) {
		dpy->lock_meaning = XK_Caps_Lock;
		break;
	    }
	}
    }
    /* Now find any Mod<n> modifier acting as the Group or Numlock modifier */
    dpy->mode_switch = 0;
    dpy->num_lock = 0;
    n *= 4;
    for (i = 3*map->max_keypermod; i < n; i++) {
	for (j = 0; j < dpy->keysyms_per_keycode; j++) {
	    sym = KeyCodetoKeySym(dpy, map->modifiermap[i], j);
	    if (sym == XK_Mode_switch)
		dpy->mode_switch |= 1 << (i / map->max_keypermod);
	    if (sym == XK_Num_Lock)
		dpy->num_lock |= 1 << (i / map->max_keypermod);
	}
    }
    for (p = dpy->key_bindings; p; p = p->next)
	ComputeMaskFromKeytrans(dpy, p);
}

static int
InitModMap(dpy)
    Display *dpy;
{
    register XModifierKeymap *map;

    if (! (map = XGetModifierMapping(dpy)))
	return 0;
    LockDisplay(dpy);
    if (dpy->modifiermap)
	XFreeModifiermap(dpy->modifiermap);
    dpy->modifiermap = map;
    dpy->free_funcs->modifiermap = XFreeModifiermap;
    if (dpy->keysyms)
	ResetModMap(dpy);
    UnlockDisplay(dpy);
    return 1;
}

int
XRefreshKeyboardMapping(event)
    register XMappingEvent *event;
{

    if(event->request == MappingKeyboard) {
	/* XXX should really only refresh what is necessary
	 * for now, make initialize test fail
	 */
	LockDisplay(event->display);
	if (event->display->keysyms) {
	     Xfree ((char *)event->display->keysyms);
	     event->display->keysyms = NULL;
	}
	UnlockDisplay(event->display);
    }
    if(event->request == MappingModifier) {
	LockDisplay(event->display);
	if (event->display->modifiermap) {
	    XFreeModifiermap(event->display->modifiermap);
	    event->display->modifiermap = NULL;
	}
	UnlockDisplay(event->display);
	/* go ahead and get it now, since initialize test may not fail */
	if (event->display->keysyms)
	    (void) InitModMap(event->display);
    }
    return 1;
}

int
_XKeyInitialize(dpy)
    Display *dpy;
{
    int per, n;
    KeySym *keysyms;

    /* 
     * lets go get the keysyms from the server.
     */
    if (!dpy->keysyms) {
	n = dpy->max_keycode - dpy->min_keycode + 1;
	keysyms = XGetKeyboardMapping (dpy, (KeyCode) dpy->min_keycode,
				       n, &per);
	/* keysyms may be NULL */
	if (! keysyms) return 0;

	LockDisplay(dpy);
	if (dpy->keysyms)
	    Xfree ((char *)dpy->keysyms);
	dpy->keysyms = keysyms;
	dpy->keysyms_per_keycode = per;
	if (dpy->modifiermap)
	    ResetModMap(dpy);
	UnlockDisplay(dpy);
    }
    if (!dpy->modifiermap)
        return InitModMap(dpy);
    return 1;
}

#endif	/* } */

void
XConvertCase(sym, lower, upper)
    register KeySym sym;
    KeySym *lower;
    KeySym *upper;
{
    *lower = sym;
    *upper = sym;
    switch(sym >> 8) {
    case 0: /* Latin 1 */
	if ((sym >= XK_A) && (sym <= XK_Z))
	    *lower += (XK_a - XK_A);
	else if ((sym >= XK_a) && (sym <= XK_z))
	    *upper -= (XK_a - XK_A);
	else if ((sym >= XK_Agrave) && (sym <= XK_Odiaeresis))
	    *lower += (XK_agrave - XK_Agrave);
	else if ((sym >= XK_agrave) && (sym <= XK_odiaeresis))
	    *upper -= (XK_agrave - XK_Agrave);
	else if ((sym >= XK_Ooblique) && (sym <= XK_Thorn))
	    *lower += (XK_oslash - XK_Ooblique);
	else if ((sym >= XK_oslash) && (sym <= XK_thorn))
	    *upper -= (XK_oslash - XK_Ooblique);
	break;
    case 1: /* Latin 2 */
	/* Assume the KeySym is a legal value (ignore discontinuities) */
	if (sym == XK_Aogonek)
	    *lower = XK_aogonek;
	else if (sym >= XK_Lstroke && sym <= XK_Sacute)
	    *lower += (XK_lstroke - XK_Lstroke);
	else if (sym >= XK_Scaron && sym <= XK_Zacute)
	    *lower += (XK_scaron - XK_Scaron);
	else if (sym >= XK_Zcaron && sym <= XK_Zabovedot)
	    *lower += (XK_zcaron - XK_Zcaron);
	else if (sym == XK_aogonek)
	    *upper = XK_Aogonek;
	else if (sym >= XK_lstroke && sym <= XK_sacute)
	    *upper -= (XK_lstroke - XK_Lstroke);
	else if (sym >= XK_scaron && sym <= XK_zacute)
	    *upper -= (XK_scaron - XK_Scaron);
	else if (sym >= XK_zcaron && sym <= XK_zabovedot)
	    *upper -= (XK_zcaron - XK_Zcaron);
	else if (sym >= XK_Racute && sym <= XK_Tcedilla)
	    *lower += (XK_racute - XK_Racute);
	else if (sym >= XK_racute && sym <= XK_tcedilla)
	    *upper -= (XK_racute - XK_Racute);
	break;
    case 2: /* Latin 3 */
	/* Assume the KeySym is a legal value (ignore discontinuities) */
	if (sym >= XK_Hstroke && sym <= XK_Hcircumflex)
	    *lower += (XK_hstroke - XK_Hstroke);
	else if (sym >= XK_Gbreve && sym <= XK_Jcircumflex)
	    *lower += (XK_gbreve - XK_Gbreve);
	else if (sym >= XK_hstroke && sym <= XK_hcircumflex)
	    *upper -= (XK_hstroke - XK_Hstroke);
	else if (sym >= XK_gbreve && sym <= XK_jcircumflex)
	    *upper -= (XK_gbreve - XK_Gbreve);
	else if (sym >= XK_Cabovedot && sym <= XK_Scircumflex)
	    *lower += (XK_cabovedot - XK_Cabovedot);
	else if (sym >= XK_cabovedot && sym <= XK_scircumflex)
	    *upper -= (XK_cabovedot - XK_Cabovedot);
	break;
    case 3: /* Latin 4 */
	/* Assume the KeySym is a legal value (ignore discontinuities) */
	if (sym >= XK_Rcedilla && sym <= XK_Tslash)
	    *lower += (XK_rcedilla - XK_Rcedilla);
	else if (sym >= XK_rcedilla && sym <= XK_tslash)
	    *upper -= (XK_rcedilla - XK_Rcedilla);
	else if (sym == XK_ENG)
	    *lower = XK_eng;
	else if (sym == XK_eng)
	    *upper = XK_ENG;
	else if (sym >= XK_Amacron && sym <= XK_Umacron)
	    *lower += (XK_amacron - XK_Amacron);
	else if (sym >= XK_amacron && sym <= XK_umacron)
	    *upper -= (XK_amacron - XK_Amacron);
	break;
    case 6: /* Cyrillic */
	/* Assume the KeySym is a legal value (ignore discontinuities) */
	if (sym >= XK_Serbian_DJE && sym <= XK_Serbian_DZE)
	    *lower -= (XK_Serbian_DJE - XK_Serbian_dje);
	else if (sym >= XK_Serbian_dje && sym <= XK_Serbian_dze)
	    *upper += (XK_Serbian_DJE - XK_Serbian_dje);
	else if (sym >= XK_Cyrillic_YU && sym <= XK_Cyrillic_HARDSIGN)
	    *lower -= (XK_Cyrillic_YU - XK_Cyrillic_yu);
	else if (sym >= XK_Cyrillic_yu && sym <= XK_Cyrillic_hardsign)
	    *upper += (XK_Cyrillic_YU - XK_Cyrillic_yu);
        break;
    case 7: /* Greek */
	/* Assume the KeySym is a legal value (ignore discontinuities) */
	if (sym >= XK_Greek_ALPHAaccent && sym <= XK_Greek_OMEGAaccent)
	    *lower += (XK_Greek_alphaaccent - XK_Greek_ALPHAaccent);
	else if (sym >= XK_Greek_alphaaccent && sym <= XK_Greek_omegaaccent &&
		 sym != XK_Greek_iotaaccentdieresis &&
		 sym != XK_Greek_upsilonaccentdieresis)
	    *upper -= (XK_Greek_alphaaccent - XK_Greek_ALPHAaccent);
	else if (sym >= XK_Greek_ALPHA && sym <= XK_Greek_OMEGA)
	    *lower += (XK_Greek_alpha - XK_Greek_ALPHA);
	else if (sym >= XK_Greek_alpha && sym <= XK_Greek_omega &&
		 sym != XK_Greek_finalsmallsigma)
	    *upper -= (XK_Greek_alpha - XK_Greek_ALPHA);
        break;
    case 0x14: /* Armenian */
	if (sym >= XK_Armenian_AYB && sym <= XK_Armenian_fe) {
	    *lower = sym | 1;
	    *upper = sym & ~1;
	}
        break;
    }
}

#if __disabled_for_imakeicide__	/* { */

int
#if NeedFunctionPrototypes
_XTranslateKey(	register Display *dpy,
		KeyCode keycode,
		register unsigned int modifiers,
		unsigned int *modifiers_return,
		KeySym *keysym_return)
#else
_XTranslateKey(dpy, keycode, modifiers, modifiers_return, keysym_return)
    register Display *dpy;
    KeyCode keycode;
    register unsigned int modifiers;
    unsigned int *modifiers_return;
    KeySym *keysym_return;
#endif
{
    int per;
    register KeySym *syms;
    KeySym sym, lsym, usym;

    if ((! dpy->keysyms) && (! _XKeyInitialize(dpy)))
	return 0;
    *modifiers_return = ((ShiftMask|LockMask)
			 | dpy->mode_switch | dpy->num_lock);
    if (((int)keycode < dpy->min_keycode) || ((int)keycode > dpy->max_keycode))
    {
	*keysym_return = NoSymbol;
	return 1;
    }
    per = dpy->keysyms_per_keycode;
    syms = &dpy->keysyms[(keycode - dpy->min_keycode) * per];
    while ((per > 2) && (syms[per - 1] == NoSymbol))
	per--;
    if ((per > 2) && (modifiers & dpy->mode_switch)) {
	syms += 2;
	per -= 2;
    }
    if ((modifiers & dpy->num_lock) &&
	(per > 1 && (IsKeypadKey(syms[1]) || IsPrivateKeypadKey(syms[1])))) {
	if ((modifiers & ShiftMask) ||
	    ((modifiers & LockMask) && (dpy->lock_meaning == XK_Shift_Lock)))
	    *keysym_return = syms[0];
	else
	    *keysym_return = syms[1];
    } else if (!(modifiers & ShiftMask) &&
	(!(modifiers & LockMask) || (dpy->lock_meaning == NoSymbol))) {
	if ((per == 1) || (syms[1] == NoSymbol))
	    XConvertCase(syms[0], keysym_return, &usym);
	else
	    *keysym_return = syms[0];
    } else if (!(modifiers & LockMask) ||
	       (dpy->lock_meaning != XK_Caps_Lock)) {
	if ((per == 1) || ((usym = syms[1]) == NoSymbol))
	    XConvertCase(syms[0], &lsym, &usym);
	*keysym_return = usym;
    } else {
	if ((per == 1) || ((sym = syms[1]) == NoSymbol))
	    sym = syms[0];
	XConvertCase(sym, &lsym, &usym);
	if (!(modifiers & ShiftMask) && (sym != syms[0]) &&
	    ((sym != usym) || (lsym == usym)))
	    XConvertCase(syms[0], &lsym, &usym);
	*keysym_return = usym;
    }
    if (*keysym_return == XK_VoidSymbol)
	*keysym_return = NoSymbol;
    return 1;
}

int
_XTranslateKeySym(dpy, symbol, modifiers, buffer, nbytes)
    Display *dpy;
    register KeySym symbol;
    unsigned int modifiers;
    char *buffer;
    int nbytes;
{
    register struct _XKeytrans *p; 
    int length;
    unsigned long hiBytes;
    register unsigned char c;

    if (!symbol)
	return 0;
    /* see if symbol rebound, if so, return that string. */
    for (p = dpy->key_bindings; p; p = p->next) {
	if (((modifiers & AllMods) == p->state) && (symbol == p->key)) {
	    length = p->len;
	    if (length > nbytes) length = nbytes;
	    memcpy (buffer, p->string, length);
	    return length;
	}
    }
    /* try to convert to Latin-1, handling control */
    hiBytes = symbol >> 8;
    if (!(nbytes &&
	  ((hiBytes == 0) ||
	   ((hiBytes == 0xFF) &&
	    (((symbol >= XK_BackSpace) && (symbol <= XK_Clear)) ||
	     (symbol == XK_Return) ||
	     (symbol == XK_Escape) ||
	     (symbol == XK_KP_Space) ||
	     (symbol == XK_KP_Tab) ||
	     (symbol == XK_KP_Enter) ||
	     ((symbol >= XK_KP_Multiply) && (symbol <= XK_KP_9)) ||
	     (symbol == XK_KP_Equal) ||
	     (symbol == XK_Delete))))))
	return 0;

    /* if X keysym, convert to ascii by grabbing low 7 bits */
    if (symbol == XK_KP_Space)
	c = XK_space & 0x7F; /* patch encoding botch */
    else if (hiBytes == 0xFF)
	c = symbol & 0x7F;
    else
	c = symbol & 0xFF;
    /* only apply Control key if it makes sense, else ignore it */
    if (modifiers & ControlMask) {
	if ((c >= '@' && c < '\177') || c == ' ') c &= 0x1F;
	else if (c == '2') c = '\000';
	else if (c >= '3' && c <= '7') c -= ('3' - '\033');
	else if (c == '8') c = '\177';
	else if (c == '/') c = '_' & 0x1F;
    }
    buffer[0] = c;
    return 1;
}
  
/*ARGSUSED*/
int
XLookupString (event, buffer, nbytes, keysym, status)
    register XKeyEvent *event;
    char *buffer;	/* buffer */
    int nbytes;	/* space in buffer for characters */
    KeySym *keysym;
    XComposeStatus *status;	/* not implemented */
{
    unsigned int modifiers;
    KeySym symbol;

    if (! _XTranslateKey(event->display, event->keycode, event->state,
		  &modifiers, &symbol))
	return 0;

#ifdef USE_OWN_COMPOSE
    if ( status ) {
	static int been_here= 0;
	if ( !been_here ) {
	    XimCompInitTables();
	    been_here = 1;
	}
	if ( !XimCompLegalStatus(status) ) {
	    status->compose_ptr = NULL;
	    status->chars_matched = 0;
	}
	if ( ((status->chars_matched>0)&&(status->compose_ptr!=NULL)) || 
		XimCompIsComposeKey(symbol,event->keycode,status) ) {
	    XimCompRtrn rtrn;
	    switch (XimCompProcessSym(status,symbol,&rtrn)) {
		case XIM_COMP_IGNORE:
		    break;
		case XIM_COMP_IN_PROGRESS:
		    if ( keysym!=NULL )
			*keysym = NoSymbol;
		    return 0;
		case XIM_COMP_FAIL:
		{
		    int n = 0, len= 0;
		    for (n=len=0;rtrn.sym[n]!=XK_VoidSymbol;n++) {
			if ( nbytes-len > 0 ) {
			    len+= _XTranslateKeySym(event->display,rtrn.sym[n],
							event->state,
							buffer+len,nbytes-len);
			}
		    }
		    if ( keysym!=NULL ) {
			if ( n==1 )	*keysym = rtrn.sym[0];
			else		*keysym = NoSymbol;
		    }
		    return len;
		}
		case XIM_COMP_SUCCEED:
		{
		    int len,n = 0;

		    symbol = rtrn.matchSym;
		    if ( keysym!=NULL )	*keysym = symbol;
		    if ( rtrn.str[0]!='\0' ) {
			strncpy(buffer,rtrn.str,nbytes-1);
			buffer[nbytes-1]= '\0';
			len = strlen(buffer);
		    }
		    else {
			len = _XTranslateKeySym(event->display,symbol,
							event->state,
							buffer,nbytes);
		    }
		    for (n=0;rtrn.sym[n]!=XK_VoidSymbol;n++) {
			if ( nbytes-len > 0 ) {
			    len+= _XTranslateKeySym(event->display,rtrn.sym[n],
							event->state,
							buffer+len,nbytes-len);
			}
		    }
		    return len;
		}
	    }
	}
    }
#endif

    if (keysym)
	*keysym = symbol;
    /* arguable whether to use (event->state & ~modifiers) here */
    return _XTranslateKeySym(event->display, symbol, event->state,
			     buffer, nbytes);
}

static void
_XFreeKeyBindings (dpy)
    Display *dpy;
{
    register struct _XKeytrans *p, *np;

    for (p = dpy->key_bindings; p; p = np) {
	np = p->next;
	Xfree(p->string);
	Xfree((char *)p->modifiers);
	Xfree((char *)p);
    }   
}

int
#if NeedFunctionPrototypes
XRebindKeysym (
    Display *dpy,
    KeySym keysym,
    KeySym *mlist,
    int nm,		/* number of modifiers in mlist */
    _Xconst unsigned char *str,
    int nbytes)
#else
XRebindKeysym (dpy, keysym, mlist, nm, str, nbytes)
    Display *dpy;
    KeySym keysym;
    KeySym *mlist;
    int nm;		/* number of modifiers in mlist */
    unsigned char *str;
    int nbytes;
#endif
{
    register struct _XKeytrans *tmp, *p;
    int nb;

    if ((! dpy->keysyms) && (! _XKeyInitialize(dpy)))
	return 0;
    LockDisplay(dpy);
    tmp = dpy->key_bindings;
    nb = sizeof(KeySym) * nm;

    if ((! (p = (struct _XKeytrans *) Xmalloc( sizeof(struct _XKeytrans)))) ||
	((! (p->string = (char *) Xmalloc( (unsigned) nbytes))) && 
	 (nbytes > 0)) ||
	((! (p->modifiers = (KeySym *) Xmalloc( (unsigned) nb))) &&
	 (nb > 0))) {
	if (p) {
	    if (p->string) Xfree(p->string);
	    if (p->modifiers) Xfree((char *) p->modifiers);
	    Xfree((char *) p);
	}
	UnlockDisplay(dpy);
	return 0;
    }

    dpy->key_bindings = p;
    dpy->free_funcs->key_bindings = _XFreeKeyBindings;
    p->next = tmp;	/* chain onto list */
    memcpy (p->string, (char *) str, nbytes);
    p->len = nbytes;
    memcpy ((char *) p->modifiers, (char *) mlist, nb);
    p->key = keysym;
    p->mlen = nm;
    ComputeMaskFromKeytrans(dpy, p);
    UnlockDisplay(dpy);
    return 0;
}

unsigned
_XKeysymToModifiers(dpy,ks)
    Display *dpy;
    KeySym ks;
{
    CARD8 code,mods;
    register KeySym *kmax;
    register KeySym *k;
    register XModifierKeymap *m;

    if ((! dpy->keysyms) && (! _XKeyInitialize(dpy)))
	return 0;
    kmax = dpy->keysyms + 
	   (dpy->max_keycode - dpy->min_keycode + 1) * dpy->keysyms_per_keycode;
    k = dpy->keysyms;
    m = dpy->modifiermap;
    mods= 0;
    while (k<kmax) {
	if (*k == ks ) {
	    register int j = m->max_keypermod<<3;

	    code=(((k-dpy->keysyms)/dpy->keysyms_per_keycode)+dpy->min_keycode);

	    while (--j >= 0) {
		if (code == m->modifiermap[j])
		    mods|= (1<<(j/m->max_keypermod));
	    }
	}
	k++;
    }
    return mods;
}
	
/*
 * given a list of modifiers, computes the mask necessary for later matching.
 * This routine must lookup the key in the Keymap and then search to see
 * what modifier it is bound to, if any.  Sets the AnyModifier bit if it
 * can't map some keysym to a modifier.
 */
static void
ComputeMaskFromKeytrans(dpy, p)
    Display *dpy;
    register struct _XKeytrans *p;
{
    register int i;

    p->state = AnyModifier;
    for (i = 0; i < p->mlen; i++) {
	p->state|= XkbKeysymToModifiers(dpy,p->modifiers[i]);
    }
    p->state &= AllMods;
}

#endif	/* __disabled_for_imakeicide__ } */
