/*	$NetBSD: ofw.h,v 1.5 2014/09/13 17:41:03 matt Exp $	*/

/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

#ifndef _ARM_OFW_H_
#define _ARM_OFW_H_


/* Virtual address range reserved for OFW. */
/* Maybe this should be elsewhere? -JJK */
#define	OFW_VIRT_BASE	0xF7000000
#define	OFW_VIRT_SIZE	0x01000000


/* OFW client services handle. */
typedef int (*ofw_handle_t)(void *);


/* Implemented in <ofw/ofw.c> */
void ofw_init(ofw_handle_t);
void ofw_boot(int, char *);
void ofw_getbootinfo(char **, char **);
void ofw_configmem(void);
void ofw_configisa(vaddr_t *, vaddr_t *);
void ofw_configisadma(vaddr_t *);
int  ofw_isadmarangeintersect(vaddr_t, vaddr_t, vaddr_t *, vaddr_t *);
vaddr_t ofw_gettranslation(vaddr_t);
vaddr_t ofw_map(vaddr_t, vsize_t, int);
vaddr_t ofw_getcleaninfo(void);

#ifdef	OFWGENCFG
/* Implemented in <ofw/ofwgencfg_machdep.c> */
extern int ofw_handleticks;
extern void cpu_reboot(int, char *);
extern void ofrootfound(void);
#endif

#endif /* !_ARM_OFW_H_ */
