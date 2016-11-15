/*	$NetBSD: rlreg.h,v 1.4 2005/12/11 12:23:29 christos Exp $	*/

/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

/*
 * RL11 registers.
 */
#define	RL_CS		0
#define	RL_BA		2
#define	RL_DA		4
#define	RL_MP		6
#define	RL_BAE		8	/* Only on RLV12 */

/*
 * Bits in Control/Status register
 */
#define	RLCS_DRDY	0000001
#define	RLCS_IE		0000100
#define	RLCS_CRDY	0000200
#define	RLCS_DE		0040000
#define	RLCS_ERR	0100000
#define	RLCS_ERRMSK	0036000
/* Command to give to drive */
#define	RLCS_NOOP	0000000		/* No operation */
#define	RLCS_WCK	0000002		/* Write Check */
#define	RLCS_GS		0000004		/* Get Status */
#define	RLCS_SEEK	0000006		/* Seek */
#define	RLCS_RHDR	0000010		/* Read Header */
#define	RLCS_WD		0000012		/* Write data */
#define	RLCS_RD		0000014		/* Read data */
#define	RLCS_RDWO	0000016		/* Read data without hdr check */
#define	RLCS_USHFT	8		/* Shift unit number left */
#define	RLCS_ADSHFT	4		/* Shift address bits 16-17 left */

/*
 * Bits in Disk Address register
 */
#define RLDA_SEEK	0000001		/* Issue a "Seek" command */
#define RLDA_GS		0000003		/* Issue a "Get Status" command */
#define	RLDA_DIR	0000004		/* Move head towards spindle */
#define	RLDA_RST	0000010		/* Reset the drive */
#define	RLDA_HSSEEK	0000020		/* Head select for seek */
#define	RLDA_HSRW	0000100		/* Head select for read/write */
#define	RLDA_CYLSHFT	7

/*
 * Bits in Multipurpose register
 */
#define	RLMP_STATUS	0000007		/* Status bits mask */
#define	RLMP_UNLOAD	0		/* not loaded */
#define	RLMP_SPINUP	1		/* spinning up */
#define	RLMP_BRUSHING	2		/* brushes out */
#define	RLMP_LOADHEADS	3		/* loading heads */
#define	RLMP_SEEKING	4		/* seeking */
#define	RLMP_LOCKED	5		/* lock turned on */
#define	RLMP_UNLHEADS	6		/* unloading heads */
#define	RLMP_SPUNDOWN	7		/* disk spun down */

#define	RLMP_DT		0000200		/* Set if RL02, otherwise RL01 */
#define	RLMP_HS		0000100		/* Head select */

/*
 * RL01/RL02 disk layout.
 */
#define	RL_BPS		256		/* Bytes/sector */
#define	RL_SPT		40		/* Sectors/track */
#define	RL_TPS01	256		/* Tracks/surface RL01 */
#define RL_TPS02	512		/* Tracks/surface RL02 */
#define	RL_SPD		2		/* Surfaces/disk */

/*
 * Other...
 */
#define	RL_MAXDPC	4		/* Max disks/controller */
