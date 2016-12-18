/*	$NetBSD: fwmem.h,v 1.4 2010/03/29 03:05:27 kiyohara Exp $	*/
/*-
 * Copyright (C) 2002-2003
 * 	Hidetoshi Shimokawa. All rights reserved.
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
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: /repoman/r/ncvs/src/sys/dev/firewire/fwmem.h,v 1.8 2005/01/06 01:42:41 imp Exp $
 */
#ifndef _FWMEM_H_
#define _FWMEM_H_

struct fw_xfer *fwmem_read_quad(struct fw_device *, void *, uint8_t, uint16_t,
				uint32_t, void *, void (*)(struct fw_xfer *));
struct fw_xfer *fwmem_write_quad(struct fw_device *, void *, uint8_t, uint16_t,
				 uint32_t, void *, void (*)(struct fw_xfer *));
struct fw_xfer *fwmem_read_block(struct fw_device *, void *, uint8_t, uint16_t,
				 uint32_t, int, void *,
				 void (*)(struct fw_xfer *));
struct fw_xfer *fwmem_write_block(struct fw_device *, void *, uint8_t, uint16_t,
				  uint32_t, int, void *,
				  void (*)(struct fw_xfer *));

dev_type_open(fwmem_open);
dev_type_close(fwmem_close);
dev_type_ioctl(fwmem_ioctl);
dev_type_strategy(fwmem_strategy);

#endif	/* _FWMEM_H_ */
