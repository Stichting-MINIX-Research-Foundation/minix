/* $NetBSD: ppbus_io.h,v 1.6 2005/12/11 12:23:28 christos Exp $ */

/*-
 * Copyright (c) 1999 Nicolas Souchu
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
 *
 * FreeBSD: src/sys/dev/ppbus/ppbio.h,v 1.1 2000/01/14 00:17:57 nsouch Exp
 *
 */

#ifndef __PPBUS_IO_H
#define __PPBUS_IO_H

/* Parallel port bus I/O opcodes */
#define PPBUS_OUTSB_EPP 1
#define PPBUS_OUTSW_EPP 2
#define PPBUS_OUTSL_EPP 3
#define PPBUS_INSB_EPP  4
#define PPBUS_INSW_EPP  5
#define PPBUS_INSL_EPP  6
#define PPBUS_RDTR      7
#define PPBUS_RSTR      8
#define PPBUS_RCTR      9
#define PPBUS_REPP_A    10
#define PPBUS_REPP_D    11
#define PPBUS_RECR      12
#define PPBUS_RFIFO     13
#define PPBUS_WDTR      14
#define PPBUS_WSTR      15
#define PPBUS_WCTR      16
#define PPBUS_WEPP_A    17
#define PPBUS_WEPP_D    18
#define PPBUS_WECR      19
#define PPBUS_WFIFO     20


/*
 * Set of ppbus i/o routines callable from ppbus device drivers
 */

#define ppbus_outsb_epp(dev,buf,cnt) \
		ppbus_io((dev), PPBUS_OUTSB_EPP, (buf), (cnt), 0)

#define ppbus_outsw_epp(dev,buf,cnt) \
		ppbus_io((dev), PPBUS_OUTSW_EPP, (buf), (cnt), 0)

#define ppbus_outsl_epp(dev,buf,cnt) \
		ppbus_io((dev), PPBUS_OUTSL_EPP, (buf), (cnt), 0)

#define ppbus_insb_epp(dev,buf,cnt) \
		ppbus_io((dev), PPBUS_INSB_EPP, (buf), (cnt), 0)

#define ppbus_insw_epp(dev,buf,cnt) \
		ppbus_io(( dev), PPBUS_INSW_EPP, (buf), (cnt), 0)

#define ppbus_insl_epp(dev,buf,cnt) \
		ppbus_io((dev), PPBUS_INSL_EPP, (buf), (cnt), 0))

#define ppbus_repp_A(dev) \
		ppbus_io((dev), PPBUS_REPP_A, NULL, 0, 0)

#define ppbus_repp_D(dev) \
		ppbus_io((dev), PPBUS_REPP_D, NULL, 0, 0)

#define ppbus_recr(dev)	\
		ppbus_io((dev), PPBUS_RECR, NULL, 0, 0)

#define ppbus_rfifo(dev) \
		ppbus_io((dev), PPBUS_RFIFO, NULL, 0, 0)

#define ppbus_wepp_A(dev,byte) \
		ppbus_io((dev), PPBUS_WEPP_A, NULL, 0, (byte))

#define ppbus_wepp_D(dev,byte) \
		ppbus_io((dev), PPBUS_WEPP_D, NULL, 0, (byte))

#define ppbus_wecr(dev,byte) \
		ppbus_io((dev), PPBUS_WECR, NULL, 0, (byte))

#define ppbus_wfifo(dev,byte) \
		ppbus_io((dev), PPBUS_WFIFO, NULL, 0, (byte))

#define ppbus_rdtr(dev) \
		ppbus_io((dev), PPBUS_RDTR, NULL, 0, 0)

#define ppbus_rstr(dev) \
		ppbus_io((dev), PPBUS_RSTR, NULL, 0, 0)

#define ppbus_rctr(dev) \
		ppbus_io((dev), PPBUS_RCTR, NULL, 0, 0)

#define ppbus_wdtr(dev,byte) \
		ppbus_io((dev), PPBUS_WDTR, NULL, 0, (byte))

#define ppbus_wstr(dev,byte) \
		ppbus_io((dev), PPBUS_WSTR, NULL, 0, (byte))

#define ppbus_wctr(dev,byte) \
		ppbus_io((dev), PPBUS_WCTR, NULL, 0, (byte))

#endif /* __PPBUS_IO_H */
