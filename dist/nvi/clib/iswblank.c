/*	$NetBSD: iswblank.c,v 1.1.1.2 2008/05/18 14:29:38 aymeric Exp $ */

#include "config.h"

#if defined(LIBC_SCCS) && !defined(lint)
static const char sccsid[] = "Id: iswblank.c,v 1.1 2001/10/11 19:22:29 skimo Exp";
#endif /* LIBC_SCCS and not lint */

#include <wchar.h>
#include <wctype.h>

int
iswblank (wint_t wc)
{
    return iswctype(wc, wctype("blank"));
}
