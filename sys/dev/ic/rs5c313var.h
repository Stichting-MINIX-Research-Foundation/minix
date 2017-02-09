/*	$NetBSD: rs5c313var.h,v 1.3 2010/04/06 15:29:19 nonaka Exp $	*/

/*
 * Copyright (c) 2006 Valeriy E. Ushakov
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
 * 3. The name of the author may not be used to endorse or promote products
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

#ifndef	_DEV_IC_RS5C313VAR_H_
#define	_DEV_IC_RS5C313VAR_H_

/*
 * RICOH RS5C313 Real Time Clock
 */

struct rs5c313_ops;

struct rs5c313_softc {
	device_t sc_dev;

	struct todr_chip_handle sc_todr;
	struct rs5c313_ops *sc_ops;

	int sc_valid;		/* oscillation halt sensing on init */

	enum {
		MODEL_5C313 = 0,
		MODEL_5C316,
		MODEL_NUM
	} sc_model;

	int sc_ctrl[2];		/* ctrl registers */
};

struct rs5c313_ops {
	void (*rs5c313_op_begin)(struct rs5c313_softc *);

	/* CE pin */
	void (*rs5c313_op_ce)(struct rs5c313_softc *, int);

	/* SCLK pin */
	void (*rs5c313_op_clk)(struct rs5c313_softc *, int);

	/* SIO pin */
	void (*rs5c313_op_dir)(struct rs5c313_softc *, int);
	int  (*rs5c313_op_read)(struct rs5c313_softc *);
	void (*rs5c313_op_write)(struct rs5c313_softc *, int);
};

void rs5c313_attach(struct rs5c313_softc *);

#endif	/* _DEV_IC_RS5C313VAR_H_ */
