/*	$NetBSD: mm58167var.h,v 1.6 2008/07/06 13:29:50 tsutsui Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthew Fredette.
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

#ifndef	_MM58167VAR_H
#define	_MM58167VAR_H

/*
 * Driver support for the National Semiconductor MM58167
 * time-of-day chip.  See
 * http://www.national.com/ds/MM/MM58167B.pdf for data sheets.
 */

struct mm58167_softc {
	device_t	mm58167_dev;

	/* Pointers to bus_space */
	bus_space_tag_t 	mm58167_regt;
	bus_space_handle_t 	mm58167_regh;

	/*
	 * Pointers to MM58167 registers.  All of these values are in
         * BCD.
	 */

	/*
	 * The most significant digit of this first value is the
	 * milliseconds unit; least significant digit is undefined:
	 */
	bus_size_t	mm58167_msec_xxx;
	/* both digits of this value make up centiseconds: */
	bus_size_t	mm58167_csec;
	bus_size_t	mm58167_sec;
	bus_size_t	mm58167_min;
	bus_size_t	mm58167_hour;
	bus_size_t	mm58167_wday;
	bus_size_t	mm58167_day;
	bus_size_t	mm58167_mon;

	/*
	 * The MM58167 has compare latches that line up with the
	 * above, interrupt registers, etc., but we don't use them, so
	 * we don't mention them here.
	 */
	bus_size_t	mm58167_status;		/* bad counter read status */
	bus_size_t	mm58167_go;		/* GO - start at integral seconds */

	/*
	 * We keep a TODR handle in the softc to save
	 * us from having to allocate it ourselves.
	 * However, only mm58167.c should know this.
	 */
	struct todr_chip_handle _mm58167_todr_handle;
};

todr_chip_handle_t mm58167_attach(struct mm58167_softc *);

#endif	/* _MM58167VAR_H */
