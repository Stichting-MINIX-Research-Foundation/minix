/*	$NetBSD: kern_uuid.c,v 1.20 2014/10/05 10:00:03 riastradh Exp $	*/

/*
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $FreeBSD: /repoman/r/ncvs/src/sys/kern/kern_uuid.c,v 1.7 2004/01/12 13:34:11 rse Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_uuid.c,v 1.20 2014/10/05 10:00:03 riastradh Exp $");

#include <sys/param.h>
#include <sys/cprng.h>
#include <sys/endian.h>
#include <sys/syscallargs.h>
#include <sys/systm.h>
#include <sys/uuid.h>

/*
 * See also:
 *	http://www.opengroup.org/dce/info/draft-leach-uuids-guids-01.txt
 *	http://www.opengroup.org/onlinepubs/009629399/apdxa.htm
 */

CTASSERT(sizeof(struct uuid) == 16);

static void
uuid_generate(struct uuid *uuid)
{

	/* Randomly generate the content.  */
	cprng_fast(uuid, sizeof(*uuid));

	/* Set the version number to 4.  */
	uuid->time_hi_and_version &= ~(uint32_t)0xf000;
	uuid->time_hi_and_version |= 0x4000;

	/* Fix the reserved bits.  */
	uuid->clock_seq_hi_and_reserved &= ~(uint8_t)0x40;
	uuid->clock_seq_hi_and_reserved |= 0x80;
}

int
sys_uuidgen(struct lwp *l, const struct sys_uuidgen_args *uap, register_t *retval)
{
	struct uuid *store, tmp;
	unsigned count;
	int error;

	/*
	 * Limit the number of UUIDs that can be created at the same time
	 * to some arbitrary number. This isn't really necessary, but I
	 * like to have some sort of upper-bound that's less than 2G :-)
	 * XXX needs to be tunable.
	 */
	if (SCARG(uap,count) < 1 || SCARG(uap,count) > 2048)
		return (EINVAL);

	for (store = SCARG(uap,store), count = SCARG(uap,count);
	     count > 0;
	     store++, count--) {
		uuid_generate(&tmp);
		error = copyout(&tmp, store, sizeof tmp);
		if (error)
			return error;
	}

	return 0;
}

int
uuidgen(struct uuid *store, int count)
{

	while (--count > 0)
		uuid_generate(store++);
	return 0;
}

int
uuid_snprintf(char *buf, size_t sz, const struct uuid *uuid)
{

	return snprintf(buf, sz,
	    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    uuid->time_low, uuid->time_mid, uuid->time_hi_and_version,
	    uuid->clock_seq_hi_and_reserved, uuid->clock_seq_low,
	    uuid->node[0], uuid->node[1], uuid->node[2], uuid->node[3],
	    uuid->node[4], uuid->node[5]);
}

int
uuid_printf(const struct uuid *uuid)
{
	char buf[UUID_STR_LEN];

	(void) uuid_snprintf(buf, sizeof(buf), uuid);
	printf("%s", buf);
	return (0);
}

/*
 * Encode/Decode UUID into octet-stream.
 *   http://www.opengroup.org/dce/info/draft-leach-uuids-guids-01.txt
 *
 * 0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                          time_low                             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |       time_mid                |         time_hi_and_version   |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |clk_seq_hi_res |  clk_seq_low  |         node (0-1)            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                         node (2-5)                            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

void
uuid_enc_le(void *buf, const struct uuid *uuid)
{
	uint8_t *p = buf;
	int i;

	le32enc(p, uuid->time_low);
	le16enc(p + 4, uuid->time_mid);
	le16enc(p + 6, uuid->time_hi_and_version);
	p[8] = uuid->clock_seq_hi_and_reserved;
	p[9] = uuid->clock_seq_low;
	for (i = 0; i < _UUID_NODE_LEN; i++)
		p[10 + i] = uuid->node[i];
}

void
uuid_dec_le(void const *buf, struct uuid *uuid)
{
	const uint8_t *p = buf;
	int i;

	uuid->time_low = le32dec(p);
	uuid->time_mid = le16dec(p + 4);
	uuid->time_hi_and_version = le16dec(p + 6);
	uuid->clock_seq_hi_and_reserved = p[8];
	uuid->clock_seq_low = p[9];
	for (i = 0; i < _UUID_NODE_LEN; i++)
		uuid->node[i] = p[10 + i];
}

void
uuid_enc_be(void *buf, const struct uuid *uuid)
{
	uint8_t *p = buf;
	int i;

	be32enc(p, uuid->time_low);
	be16enc(p + 4, uuid->time_mid);
	be16enc(p + 6, uuid->time_hi_and_version);
	p[8] = uuid->clock_seq_hi_and_reserved;
	p[9] = uuid->clock_seq_low;
	for (i = 0; i < _UUID_NODE_LEN; i++)
		p[10 + i] = uuid->node[i];
}

void
uuid_dec_be(void const *buf, struct uuid *uuid)
{
	const uint8_t *p = buf;
	int i;

	uuid->time_low = be32dec(p);
	uuid->time_mid = be16dec(p + 4);
	uuid->time_hi_and_version = be16dec(p + 6);
	uuid->clock_seq_hi_and_reserved = p[8];
	uuid->clock_seq_low = p[9];
	for (i = 0; i < _UUID_NODE_LEN; i++)
		uuid->node[i] = p[10 + i];
}
