/*
 * Copyright (C) 2004, 2005, 2007, 2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: notify.h,v 1.16 2009-01-17 23:47:42 tbox Exp $ */

#ifndef NAMED_NOTIFY_H
#define NAMED_NOTIFY_H 1

#include <named/types.h>
#include <named/client.h>

/***
 ***	Module Info
 ***/

/*! \file
 * \brief
 *	RFC1996
 *	A Mechanism for Prompt Notification of Zone Changes (DNS NOTIFY)
 */

/***
 ***	Functions.
 ***/

void
ns_notify_start(ns_client_t *client);

/*%<
 *	Examines the incoming message to determine appropriate zone.
 *	Returns FORMERR if there is not exactly one question.
 *	Returns REFUSED if we do not serve the listed zone.
 *	Pass the message to the zone module for processing
 *	and returns the return status.
 *
 * Requires
 *\li	client to be valid.
 */

#endif /* NAMED_NOTIFY_H */

