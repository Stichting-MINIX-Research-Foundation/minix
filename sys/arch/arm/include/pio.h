/*	$NetBSD: pio.h,v 1.3 2014/01/29 00:42:15 matt Exp $	*/

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

#ifndef _ARM_PIO_H_
#define _ARM_PIO_H_

#include <sys/bus.h>

extern struct bus_space isa_io_bs_tag;

#define	inb(port)	 bus_space_read_1( &isa_io_bs_tag, (bus_space_handle_t)isa_io_bs_tag.bs_cookie, (port))
#define	outb(port, byte) bus_space_write_1(&isa_io_bs_tag, (bus_space_handle_t)isa_io_bs_tag.bs_cookie, (port), (byte))

#endif /* _ARM_PIO_H_ */
