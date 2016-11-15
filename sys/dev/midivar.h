/*	$NetBSD: midivar.h,v 1.20 2014/12/22 07:02:22 mrg Exp $	*/

/*
 * Copyright (c) 1998, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@NetBSD.org) and (midi FST refactoring and
 * Active Sense) Chapman Flack (chap@NetBSD.org).
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

#ifndef _SYS_DEV_MIDIVAR_H_
#define _SYS_DEV_MIDIVAR_H_

#define MIDI_BUFSIZE 1024

#include <sys/callout.h>
#include <sys/cdefs.h>
#include <sys/device.h>
#include <sys/condvar.h>
#include <sys/mutex.h>

/*
 * In both xmt and rcv direction, the midi_fst runs at the time data are
 * buffered (midi_writebytes for xmt, midi_in for rcv) so what's in the
 * buffer is always in canonical form (or compressed, on xmt, if the hw
 * wants it that way). To preserve message boundaries for the buffer
 * consumer, but allow transfers larger than one message, the buffer is
 * split into a buf fork and an idx fork, where each byte of idx encodes
 * the type and length of a message. Because messages are variable length,
 * it is a guess how to set the relative sizes of idx and buf, or how many
 * messages can be buffered before one or the other fills.
 *
 * The producer adds only complete messages to a buffer (except for SysEx
 * messages, which have unpredictable length). A consumer serving byte-at-a-
 * time hardware may partially consume a message, in which case it updates
 * the length count at *idx_consumerp to reflect the remaining length of the
 * message, only incrementing idx_consumerp when the message has been entirely
 * consumed.
 *
 * The buffers are structured in the simple 1 reader 1 writer bounded buffer
 * form, considered full when 1 unused byte remains. This should allow their
 * use with minimal locking provided single pointer reads and writes can be
 * assured atomic ... but then I chickened out on assuming that assurance, and
 * added the extra locks to the code.
 *
 * Macros for manipulating the buffers:
 *
 * MIDI_BUF_DECLARE(frk) where frk is either buf or idx:
 *   declares the local variables frk_cur, frk_lim, frk_org, and frk_end.
 *
 * MIDI_BUF_CONSUMER_INIT(mb,frk)
 * MIDI_BUF_PRODUCER_INIT(mb,frk)
 *   initializes frk_org and frk_end to the base and end (that is, address just
 *   past the last valid byte) of the buffer fork frk, frk_cur to the
 *   consumer's or producer's current position, respectively, and frk_lim to
 *   the current limit (for either consumer or producer, immediately following
 *   this macro, frk_lim-frk_cur gives the number of bytes to play with). That
 *   means frk_lim may actually point past the buffer; loops on the condition
 *   (frk_cur < frk_lim) must contain WRAP(frk) if proceeding byte-by-byte, or
 *   must explicitly handle wrapping around frk_end if doing anything clever.
 *   These are expression-shaped macros that have the value frk_lim. When used
 *   without locking--provided pointer reads and writes can be assumed atomic--
 *   these macros give a conservative estimate of what is available to consume
 *   or produce.
 *
 * MIDI_BUF_WRAP(frk)
 *   tests whether frk_cur == frk_end and, if so, wraps both frk_cur and
 *   frk_lim around the beginning of the buffer. Because the test is ==, it
 *   must be applied at each byte in a loop; if the loop is proceeding in
 *   bigger steps, the possibility of wrap must be coded for. This expression-
 *   shaped macro has the value of frk_cur after wrapping.
 *
 * MIDI_BUF_CONSUMER_REFRESH(mb,frk)
 * MIDI_BUF_PRODUCER_REFRESH(mb,frk)
 *   refresh the local value frk_lim for a new snapshot of bytes available; an
 *   expression-shaped macro with the new value of frk_lim. Usually used after
 *   using up the first conservative estimate and obtaining a lock to get a
 *   final value. Used unlocked, just gives a more recent conservative estimate.
 *
 * MIDI_BUF_CONSUMER_WBACK(mb,frk)
 * MIDI_BUF_PRODUCER_WBACK(mb,frk)
 *   write back the local copy of frk_cur to the buffer, after a barrier to
 *   ensure prior writes go first. Under the right atomicity conditions a
 *   producer could get away with using these unlocked, as long as the order
 *   is buf followed by idx. A consumer should update both in a critical
 *   section.
 */
struct midi_buffer {
	u_char * __volatile idx_producerp;
	u_char * __volatile idx_consumerp;
	u_char * __volatile buf_producerp;
	u_char * __volatile buf_consumerp;
	u_char idx[MIDI_BUFSIZE/3];
	u_char buf[MIDI_BUFSIZE-MIDI_BUFSIZE/3];
};
#define MIDI_BUF_DECLARE(frk) \
u_char *__CONCAT(frk,_cur); \
u_char *__CONCAT(frk,_lim); \
u_char *__CONCAT(frk,_org); \
u_char *__CONCAT(frk,_end)

#define MIDI_BUF_CONSUMER_REFRESH(mb,frk) \
((__CONCAT(frk,_lim)=(mb)->__CONCAT(frk,_producerp)), \
__CONCAT(frk,_lim) < __CONCAT(frk,_cur) ? \
(__CONCAT(frk,_lim) += sizeof (mb)->frk) : __CONCAT(frk,_lim))

#define MIDI_BUF_PRODUCER_REFRESH(mb,frk) \
((__CONCAT(frk,_lim)=(mb)->__CONCAT(frk,_consumerp)-1), \
__CONCAT(frk,_lim) < __CONCAT(frk,_cur) ? \
(__CONCAT(frk,_lim) += sizeof (mb)->frk) : __CONCAT(frk,_lim))

#define MIDI_BUF_EXTENT_INIT(mb,frk) \
((__CONCAT(frk,_org)=(mb)->frk), \
(__CONCAT(frk,_end)=__CONCAT(frk,_org)+sizeof (mb)->frk))

#define MIDI_BUF_CONSUMER_INIT(mb,frk) \
(MIDI_BUF_EXTENT_INIT((mb),frk), \
(__CONCAT(frk,_cur)=(mb)->__CONCAT(frk,_consumerp)), \
MIDI_BUF_CONSUMER_REFRESH((mb),frk))

#define MIDI_BUF_PRODUCER_INIT(mb,frk) \
(MIDI_BUF_EXTENT_INIT((mb),frk), \
(__CONCAT(frk,_cur)=(mb)->__CONCAT(frk,_producerp)), \
MIDI_BUF_PRODUCER_REFRESH((mb),frk))

#define MIDI_BUF_WRAP(frk) \
(__predict_false(__CONCAT(frk,_cur)==__CONCAT(frk,_end)) ? (\
(__CONCAT(frk,_lim)-=__CONCAT(frk,_end)-__CONCAT(frk,_org)), \
(__CONCAT(frk,_cur)=__CONCAT(frk,_org))) : __CONCAT(frk,_cur))

#define MIDI_BUF_CONSUMER_WBACK(mb,frk) do { \
__insn_barrier(); \
(mb)->__CONCAT(frk,_consumerp)=__CONCAT(frk,_cur); \
} while (/*CONSTCOND*/0)

#define MIDI_BUF_PRODUCER_WBACK(mb,frk) do { \
__insn_barrier(); \
(mb)->__CONCAT(frk,_producerp)=__CONCAT(frk,_cur); \
} while (/*CONSTCOND*/0)


#define MIDI_MAX_WRITE 32	/* max bytes written with busy wait */
#define MIDI_WAIT 10000		/* microseconds to wait after busy wait */

struct midi_state {
	struct  evcnt bytesDiscarded;
	struct  evcnt incompleteMessages;
	struct {
		uint32_t bytesDiscarded;
		uint32_t incompleteMessages;
	}	atOpen,
		atQuery;
	int     state;
	u_char *pos;
	u_char *end;
	u_char  msg[3];
};

struct midi_softc {
	device_t dev;		/* Hardware device struct */
	void	*hw_hdl;	/* Hardware driver handle */
	const struct	midi_hw_if *hw_if; /* Hardware interface */
	const struct	midi_hw_if_ext *hw_if_ext; /* see midi_if.h */
	int	isopen;		/* Open indicator */
	int	flags;		/* Open flags */
	int	dying;
	struct	midi_buffer outbuf;
	struct	midi_buffer inbuf;
	int	props;
	int	refcnt;
	kcondvar_t detach_cv;
	kcondvar_t rchan;
	kcondvar_t wchan;
	kmutex_t *lock;
	int	pbus;
	int	rcv_expect_asense;
	int	rcv_quiescent;
	int	rcv_eof;
	struct	selinfo wsel;	/* write selector */
	struct	selinfo rsel;	/* read selector */
	pid_t	async;	/* process who wants audio SIGIO */
	void	*sih;

	struct callout xmt_asense_co;
	struct callout rcv_asense_co;

	/* MIDI input state machine; states are *s of 4 to allow | CAT bits */
	struct midi_state rcv;
	struct midi_state xmt;
#define MIDI_IN_START	0
#define MIDI_IN_RUN0_1	4
#define MIDI_IN_RUN1_1	8
#define MIDI_IN_RUN0_2 12
#define MIDI_IN_RUN1_2 16
#define MIDI_IN_RUN2_2 20
#define MIDI_IN_COM0_1 24
#define MIDI_IN_COM0_2 28
#define MIDI_IN_COM1_2 32
#define MIDI_IN_SYX1_3 36
#define MIDI_IN_SYX2_3 40
#define MIDI_IN_SYX0_3 44
#define MIDI_IN_RNX0_1 48
#define MIDI_IN_RNX0_2 52
#define MIDI_IN_RNX1_2 56
#define MIDI_IN_RNY1_2 60 /* not needed except for accurate error counts */
/*
 * Four more states are needed to model the equivalence of NoteOff vel. 64
 * and NoteOn vel. 0 for canonicalization or compression. In each of these 4
 * states, we know the last message input and output was a NoteOn or a NoteOff.
 */
#define MIDI_IN_RXX2_2 64 /* last output == msg[0] != last input */
#define MIDI_IN_RXX0_2 68 /* last output != msg[0] == this input */
#define MIDI_IN_RXX1_2 72 /* " */
#define MIDI_IN_RXY1_2 76 /* variant of RXX1_2 needed for error count only */

#define MIDI_CAT_DATA 0
#define MIDI_CAT_STATUS1 1
#define MIDI_CAT_STATUS2 2
#define MIDI_CAT_COMMON 3

	/* Synthesizer emulation stuff */
	int	seqopen;
	struct	midi_dev *seq_md; /* structure that links us with the seq. */
};

#define MIDIUNIT(d) ((d) & 0xff)

#endif /* _SYS_DEV_MIDIVAR_H_ */
