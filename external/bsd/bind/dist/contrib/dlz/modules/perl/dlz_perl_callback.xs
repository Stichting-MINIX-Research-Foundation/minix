/*
 * Copyright (C) 2012  John Eaglesham
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND JOHN EAGLESHAM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * JOHN EAGLESHAM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "dlz_perl_driver.h"

#include <dlz_minimal.h>

/* And some XS code. */
MODULE = DLZ_Perl	   PACKAGE = DLZ_Perl

int
LOG_INFO()
	CODE:
		RETVAL = ISC_LOG_INFO;
	OUTPUT:
		RETVAL

int
LOG_NOTICE()
	CODE:
		RETVAL = ISC_LOG_NOTICE;
	OUTPUT:
		RETVAL

int
LOG_WARNING()
	CODE:
		RETVAL = ISC_LOG_WARNING;
	OUTPUT:
		RETVAL

int
LOG_ERROR()
	CODE:
		RETVAL = ISC_LOG_ERROR;
	OUTPUT:
		RETVAL

int
LOG_CRITICAL()
	CODE:
		RETVAL = ISC_LOG_CRITICAL;
	OUTPUT:
		RETVAL


void
log(opaque, level, msg)
	IV opaque
	int level
	char *msg

	PREINIT:
		log_t *log = (log_t *) opaque;

	CODE:
		log( level, msg );

