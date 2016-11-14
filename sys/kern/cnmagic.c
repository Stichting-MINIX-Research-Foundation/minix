/*	$NetBSD: cnmagic.c,v 1.13 2011/11/19 17:34:41 christos Exp $	*/

/*
 * Copyright (c) 2000 Eduardo Horvath
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cnmagic.c,v 1.13 2011/11/19 17:34:41 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#define ENCODE_STATE(c, n) (short)(((c)&0x1ff)|(((n)&0x7f)<<9))

static unsigned short cn_magic[CNS_LEN];

/*
 * Initialize a cnm_state_t.
 */
void
cn_init_magic(cnm_state_t *cnm)
{
	cnm->cnm_state = 0;
	cnm->cnm_magic = cn_magic;
}

/*
 * Destroy a cnm_state_t.
 */
void
cn_destroy_magic(cnm_state_t *cnm)
{
	cnm->cnm_state = 0;
	cnm->cnm_magic = NULL;
}

/*
 * Translate a magic string to a state
 * machine table.
 */
int
cn_set_magic(const char *smagic)
{
	const unsigned char *magic = (const unsigned char *)smagic;
	unsigned short i, c, n;
	unsigned short m[CNS_LEN];

	for (i = 0; i < CNS_LEN; i++) {
		c = *magic++;
		switch (c) {
		case 0:
			/* End of string */
			if (i == 0) {
				/* empty string? */
#ifdef DEBUG
				printf("cn_set_magic(): empty!\n");
#endif
			}
			cn_magic[i] = 0;
			while (i--)
				cn_magic[i] = m[i];
			return 0;
		case 0x27:
			/* Escape sequence */
			c = *magic++;
			switch (c) {
			case 0x27:
				break;
			case 0x01:
				/* BREAK */
				c = CNC_BREAK;
				break;
			case 0x02:
				/* NUL */
				c = 0;
				break;
			}
			/* FALLTHROUGH */
		default:
			/* Transition to the next state. */
			n = *magic ? i + 1 : CNS_TERM;
#ifdef DEBUG
			if (!cold)
				aprint_normal("mag %d %x:%x\n", i, c, n);
#endif
			m[i] = ENCODE_STATE(c, n);
			break;
		}
	}
	return EINVAL;
}

/*
 * Translatea state machine table back to
 * a magic string.
 */
int
cn_get_magic(char *magic, size_t maglen)
{
	size_t i, n = 0;

#define ADD_CHAR(x) \
do \
	if (n < maglen) \
		magic[n++] = (x); \
	else \
		goto error; \
while (/*CONSTCOND*/0)

	for (i = 0; i < CNS_LEN; /* empty */) {
		unsigned short c = cn_magic[i];
		i = CNS_MAGIC_NEXT(c);
		if (i == 0)
			goto finish;

		/* Translate a character */
		switch (CNS_MAGIC_VAL(c)) {
		case CNC_BREAK:
			ADD_CHAR(0x27);
			ADD_CHAR(0x01);
			break;
		case 0:
			ADD_CHAR(0x27);
			ADD_CHAR(0x02);
			break;
		case 0x27:
			ADD_CHAR(0x27);
			ADD_CHAR(0x27);
			break;
		default:
			ADD_CHAR(c);
			break;
		}
		/* Now go to the next state */
		if (i == CNS_TERM)
			goto finish;
	}

error:
	return EINVAL;

finish:
	/* Either termination state or empty machine */
	ADD_CHAR('\0');
	return 0;
}

