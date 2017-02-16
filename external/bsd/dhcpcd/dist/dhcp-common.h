/* $NetBSD: dhcp-common.h,v 1.10 2015/07/09 10:15:34 roy Exp $ */

/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DHCPCOMMON_H
#define DHCPCOMMON_H

#include <arpa/inet.h>
#include <netinet/in.h>

#include <stdint.h>

#include "common.h"
#include "dhcpcd.h"

/* Max MTU - defines dhcp option length */
#define MTU_MAX             1500
#define MTU_MIN             576

#define REQUEST		(1 << 0)
#define UINT8		(1 << 1)
#define UINT16		(1 << 2)
#define SINT16		(1 << 3)
#define UINT32		(1 << 4)
#define SINT32		(1 << 5)
#define ADDRIPV4	(1 << 6)
#define STRING		(1 << 7)
#define ARRAY		(1 << 8)
#define RFC3361		(1 << 9)
#define RFC1035		(1 << 10)
#define RFC3442		(1 << 11)
#define RFC5969		(1 << 12)
#define ADDRIPV6	(1 << 13)
#define BINHEX		(1 << 14)
#define FLAG		(1 << 15)
#define NOREQ		(1 << 16)
#define EMBED		(1 << 17)
#define ENCAP		(1 << 18)
#define INDEX		(1 << 19)
#define OPTION		(1 << 20)
#define DOMAIN		(1 << 21)
#define ASCII		(1 << 22)
#define RAW		(1 << 23)
#define ESCSTRING	(1 << 24)
#define ESCFILE		(1 << 25)
#define BITFLAG		(1 << 26)
#define RESERVED	(1 << 27)

struct dhcp_opt {
	uint32_t option; /* Also used for IANA Enterpise Number */
	int type;
	size_t len;
	char *var;

	int index; /* Index counter for many instances of the same option */
	char bitflags[8];

	/* Embedded options.
	 * The option code is irrelevant here. */
	struct dhcp_opt *embopts;
	size_t embopts_len;

	/* Encapsulated options */
	struct dhcp_opt *encopts;
	size_t encopts_len;
};

struct dhcp_opt *vivso_find(uint32_t, const void *);

ssize_t dhcp_vendor(char *, size_t);

void dhcp_print_option_encoding(const struct dhcp_opt *opt, int cols);
#define add_option_mask(var, val) \
	((var)[(val) >> 3] = (uint8_t)((var)[(val) >> 3] | 1 << ((val) & 7)))
#define del_option_mask(var, val) \
	((var)[(val) >> 3] = (uint8_t)((var)[(val) >> 3] & ~(1 << ((val) & 7))))
#define has_option_mask(var, val) \
	((var)[(val) >> 3] & (uint8_t)(1 << ((val) & 7)))
int make_option_mask(const struct dhcp_opt *, size_t,
    const struct dhcp_opt *, size_t,
    uint8_t *, const char *, int);

size_t encode_rfc1035(const char *src, uint8_t *dst);
ssize_t decode_rfc1035(char *, size_t, const uint8_t *, size_t);
ssize_t print_string(char *, size_t, int, const uint8_t *, size_t);
int dhcp_set_leasefile(char *, size_t, int, const struct interface *);

size_t dhcp_envoption(struct dhcpcd_ctx *,
    char **, const char *, const char *, struct dhcp_opt *,
    const uint8_t *(*dgetopt)(struct dhcpcd_ctx *,
    size_t *, unsigned int *, size_t *,
    const uint8_t *, size_t, struct dhcp_opt **),
    const uint8_t *od, size_t ol);
void dhcp_zero_index(struct dhcp_opt *);

#endif
