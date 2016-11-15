/*	$NetBSD: midi_if.h,v 1.27 2015/03/01 00:34:14 mrg Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@NetBSD.org).
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

#ifndef _SYS_DEV_MIDI_IF_H_
#define _SYS_DEV_MIDI_IF_H_

#include <sys/mutex.h>

struct midi_info {
	const char *name;		/* Name of MIDI hardware */
	int	props;
};
#define MIDI_PROP_OUT_INTR  1
#define MIDI_PROP_CAN_INPUT 2
#define MIDI_PROP_NO_OUTPUT 4

/*
 * XXX expand
 *
 * List of hardware interface methods, and when locks are held by each
 * called by this module:
 *
 *	METHOD			INTR	NOTES
 *	----------------------- ------- -------------------------
 *	open 			-	
 *	close 			-	
 *	output 			-	
 *	getinfo 		-	Called at attach time
 *	ioctl 			-	
 *	get_locks 		-	Called at attach time
 */

struct midi_softc;

struct midi_hw_if {
	int	(*open)(void *, int, 	/* open hardware */
			void (*)(void *, int), /* input callback */
			void (*)(void *), /* output callback */
			void *);
	void	(*close)(void *);		/* close hardware */
	int	(*output)(void *, int);	/* output a byte */
	void	(*getinfo)(void *, struct midi_info *);
	int	(*ioctl)(void *, u_long, void *, int, struct lwp *);
	void	(*get_locks)(void *, kmutex_t **, kmutex_t **);
};

/*
 * The extended hardware interface is for use by drivers that are better off
 * getting messages whole to transmit, rather than byte-by-byte through
 * output().  Two examples are midisyn (which interprets MIDI messages in
 * software to drive synth chips) and umidi (which has to send messages in the
 * packet-based USB MIDI protocol).  It is silly for them to have to reassemble
 * messages midi had to split up to poke through the single-byte interface.
 *
 * To register use of the extended interface, a driver will call back midi's
 * midi_register_hw_if_ext() function during getinfo(); thereafter midi will
 * deliver channel messages, system common messages other than sysex, and sysex
 * messages, respectively, through these methods, and use the original output
 * method only for system realtime messages (all of which are single byte).
 * Other drivers that have no reason to change from the single-byte interface
 * simply don't call the register function, and nothing changes for them.
 *
 * IMPORTANT: any code that provides a midi_hw_if_ext struct MUST initialize
 * its members BY NAME (typically with a C99-style initializer with designators)
 * and assure that any unused members contain zeroes (which is what C99
 * initializers will do), and make no assumptions about the size or order of
 * the struct, to allow for further extension of this interface as needed.
 */
struct midi_hw_if_ext {
	int	(*channel)(void *, int, int, u_char *, int);
	int	(*common)(void *, int, u_char *, int);
	int	(*sysex)(void *, u_char *, int);
	int	compress:1; /* if hw wants channel msgs in compressed form */
};
void midi_register_hw_if_ext(struct midi_hw_if_ext *);

void	midi_attach(struct midi_softc *);
int	mididetach(device_t, int);
device_t midi_attach_mi(const struct midi_hw_if *, void *, device_t);

int	midi_unit_count(void);
void	midi_getinfo(dev_t, struct midi_info *);
int	midi_writebytes(int, u_char *, int);

#if !defined(IPL_AUDIO)
#define splaudio splbio		/* XXX */
#define IPL_AUDIO IPL_BIO	/* XXX */
#endif

#endif /* _SYS_DEV_MIDI_IF_H_ */
