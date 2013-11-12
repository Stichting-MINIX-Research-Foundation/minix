/*	$NetBSD: eti.h,v 1.8 2001/06/13 10:45:59 wiz Exp $	*/

/*-
 * Copyright (c) 1998-1999 Brett Lymn (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#ifndef	_ETI_H_
#define	_ETI_H_

/* common return codes for libmenu and libpanel functions */

#define E_OK              (0)
#define E_SYSTEM_ERROR    (-1)
#define E_BAD_ARGUMENT    (-2)
#define E_POSTED          (-3)
#define E_CONNECTED       (-4)
#define E_BAD_STATE       (-5)
#define E_NO_ROOM         (-6)
#define E_NOT_POSTED      (-7)
#define E_UNKNOWN_COMMAND (-8)
#define E_NO_MATCH        (-9)
#define E_NOT_SELECTABLE  (-10)
#define E_NOT_CONNECTED   (-11)
#define E_REQUEST_DENIED  (-12)
#define E_INVALID_FIELD   (-13)
#define E_CURRENT         (-14)

#endif /* !_ETI_H_ */
