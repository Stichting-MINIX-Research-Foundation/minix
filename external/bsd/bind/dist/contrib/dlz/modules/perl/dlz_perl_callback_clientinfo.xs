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

#define ADDR_BUF_LEN INET6_ADDRSTRLEN

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "dlz_perl_driver.h"

#include <dlz_minimal.h>

/* And some XS code. */
MODULE = DLZ_Perl::clientinfo	   PACKAGE = DLZ_Perl::clientinfo

PROTOTYPES: DISABLE

void
sourceip(opaque)
	SV *opaque

	PREINIT:
		const char *ret;
		char addr_buf[ADDR_BUF_LEN];
		int port;
		isc_sockaddr_t *src;
		dlz_perl_clientinfo_opaque *ci;
		I32 wantarray = GIMME_V;

	PPCODE:
		if (!SvTRUE(opaque) || !SvIOK(opaque)) XSRETURN_EMPTY;

                /*
                 * Safe, because Perl guarantees that an IV (the type we
                 * pass into DLZ functions who pass it here) is able to
                 * hold a pointer.
                 */
		ci = (dlz_perl_clientinfo_opaque *) SvIV(opaque);
		if (wantarray == G_VOID || ci->methods == NULL ||
			ci->methods->version - ci->methods->age <
			DNS_CLIENTINFOMETHODS_VERSION)
			XSRETURN_EMPTY;

		ci->methods->sourceip(ci->clientinfo, &src);

		switch (src->type.sa.sa_family) {
			case AF_INET:
				port = ntohs(src->type.sin.sin_port);
				ret = inet_ntop(AF_INET,
                                                &src->type.sin.sin_addr,
                                                addr_buf, ADDR_BUF_LEN);
				break;
			case AF_INET6:
				port = ntohs(src->type.sin6.sin6_port);
				ret = inet_ntop(AF_INET6,
                                                &src->type.sin6.sin6_addr,
                                                addr_buf, ADDR_BUF_LEN);
				break;
			default:
				ret = NULL;
		}

		if (ret == NULL) XSRETURN_EMPTY;

		XPUSHs(sv_2mortal(newSVpv(addr_buf, strlen(addr_buf))));
		if (wantarray == G_ARRAY) XPUSHs(sv_2mortal(newSViv(port)));

