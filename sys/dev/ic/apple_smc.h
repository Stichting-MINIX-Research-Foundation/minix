/*	$NetBSD: apple_smc.h,v 1.3 2014/04/01 17:48:52 riastradh Exp $	*/

/*
 * Apple System Management Controller Interface
 */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_DEV_IC_APPLE_SMC_H_
#define	_DEV_IC_APPLE_SMC_H_

#include <sys/types.h>

struct apple_smc_tag;

struct apple_smc_attach_args {
	struct apple_smc_tag	*asa_smc;
};

struct apple_smc_key;

struct apple_smc_desc {
	uint8_t asd_size;

	char asd_type[4];
#define	APPLE_SMC_TYPE_UINT8	"ui8 "
#define	APPLE_SMC_TYPE_UINT16	"ui16"
#define	APPLE_SMC_TYPE_UINT32	"ui32"
#define	APPLE_SMC_TYPE_SINT8	"si8 "
#define	APPLE_SMC_TYPE_SINT16	"si16"
#define	APPLE_SMC_TYPE_SINT32	"si32"
#define	APPLE_SMC_TYPE_STRING	"ch8*"
#define	APPLE_SMC_TYPE_FANDESC	"{fds" /* fan description */
#define	APPLE_SMC_TYPE_FPE2	"fpe2" /* fan RPM */
#define	APPLE_SMC_TYPE_SP78	"sp78" /* temperature in a weird scale */

	uint8_t asd_flags;
#define	APPLE_SMC_FLAG_UNKNOWN0	0x01
#define	APPLE_SMC_FLAG_UNKNOWN1	0x02
#define	APPLE_SMC_FLAG_UNKNOWN2	0x04
#define	APPLE_SMC_FLAG_UNKNOWN3	0x08
#define	APPLE_SMC_FLAG_UNKNOWN4	0x10
#define	APPLE_SMC_FLAG_UNKNOWN5	0x20
#define	APPLE_SMC_FLAG_WRITE	0x40
#define	APPLE_SMC_FLAG_READ	0x80
} __packed;

uint32_t	apple_smc_nkeys(struct apple_smc_tag *);
int		apple_smc_nth_key(struct apple_smc_tag *,
		    uint32_t, const char[4 + 1],
		    struct apple_smc_key **);
int		apple_smc_named_key(struct apple_smc_tag *,
		    const char[4 + 1], const char[4 + 1],
		    struct apple_smc_key **);
void		apple_smc_release_key(struct apple_smc_tag *,
		    struct apple_smc_key *);
int		apple_smc_key_search(struct apple_smc_tag *, const char[4 + 1],
		    uint32_t *);
const char *	apple_smc_key_name(const struct apple_smc_key *);
uint32_t	apple_smc_key_index(const struct apple_smc_key *);
const struct apple_smc_desc *
		apple_smc_key_desc(const struct apple_smc_key *);

int	apple_smc_read_key(struct apple_smc_tag *,
	    const struct apple_smc_key *, void *, uint8_t);
int	apple_smc_read_key_1(struct apple_smc_tag *,
	    const struct apple_smc_key *, uint8_t *);
int	apple_smc_read_key_2(struct apple_smc_tag *,
	    const struct apple_smc_key *, uint16_t *);
int	apple_smc_read_key_4(struct apple_smc_tag *,
	    const struct apple_smc_key *, uint32_t *);

int	apple_smc_write_key(struct apple_smc_tag *,
	    const struct apple_smc_key *, const void *, uint8_t);
int	apple_smc_write_key_1(struct apple_smc_tag *,
	    const struct apple_smc_key *, uint8_t);
int	apple_smc_write_key_2(struct apple_smc_tag *,
	    const struct apple_smc_key *, uint16_t);
int	apple_smc_write_key_4(struct apple_smc_tag *,
	    const struct apple_smc_key *, uint32_t);

#endif  /* _DEV_IC_APPLE_SMC_H_ */
