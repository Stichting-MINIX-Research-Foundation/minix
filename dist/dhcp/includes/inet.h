/* inet.h

   Portable definitions for internet addresses */

/*
 * Copyright (c) 2004,2007,2009 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``https://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

/* An internet address of up to 128 bits. */

struct iaddr {
	unsigned len;
	unsigned char iabuf [16];
};

struct iaddrlist {
	struct iaddrlist *next;
	struct iaddr addr;
};


/* struct iaddrmatch - used to compare a host IP against a subnet spec
 *
 * There is a space/speed tradeoff here implied by the use of a second
 * struct iaddr to hold the mask; while using an unsigned (byte!) to
 * represent the subnet prefix length would be more memory efficient,
 * it makes run-time mask comparisons more expensive.  Since such
 * entries are used currently only in restricted circumstances
 * (wanting to reject a subnet), the decision is in favour of run-time
 * efficiency.
 */

struct iaddrmatch {
	struct iaddr addr;
	struct iaddr mask;
};

/* its list ... */
 
struct iaddrmatchlist {
	struct iaddrmatchlist *next;
	struct iaddrmatch match;
};


/*
 * Structure to store information about a CIDR network.
 */

struct iaddrcidrnet {
	struct iaddr lo_addr;
	int bits;
};

struct iaddrcidrnetlist {
	struct iaddrcidrnetlist *next;
	struct iaddrcidrnet cidrnet;
};

