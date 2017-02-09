/* $NetBSD: midictl.h,v 1.4 2011/11/23 23:07:31 jmcneill Exp $ */

/*-
 * Copyright (c) 2006, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Chapman Flack and by Andrew Doran.
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

#ifndef _SYS_DEV_MIDICTL_H_
#define _SYS_DEV_MIDICTL_H_

/*
 * General support for MIDI controllers, registered parameters, and
 * nonregistered parameters. It interprets MIDI messages that update
 * these values, maintains the associated state, and provides an API
 * for obtaining the current value of any controller or parameter and
 * tracking changes to it.
 *
 * One function provides the interface for message parsing. When a message
 * is received, once it has been determined to be a MIDI_CTL_CHANGE message,
 * simply call midictl_change(&mc, chan, ctlval) where chan is the channel
 * extracted from the first byte, and ctlval points to the remaining two
 * bytes of the message received.
 *
 * The API for reading the state is equally simple. Use
 * midictl_read(&mc, chan, ctlr, dflt)
 * midictl_rpn_read(&mc, chan, rpn, dflt)
 * midictl_nrpn_read(&mc, chan, nrpn, dflt)
 * to read the current value of controller #ctlr, RP #rpn, or NRP #nrpn,
 * respectively. (For 14-bit controllers, use the MSB number as ctlr, not
 * the LSB number.) You get the complete current value; for 14-bit controllers
 * and parameters you get a single 14-bit integer without fussing about the
 * multiple MIDI messages needed to set it. If you read a controller or
 * parameter that no MIDI message has yet written, you get back the value dflt.
 * If you read one whose MSB or LSB only has been written, you get what you
 * would get if the value had been dflt before the write.
 *
 * The functions may be called from any context but reentrant calls operating
 * on the same midictl are unsupported, with one exception: calls back into
 * midictl from a notify handler it has called are permitted. If you are
 * calling midictl_change in a driver function called by midi(4), you are ok
 * as midi(4) itself serializes its calls into the driver. For other uses,
 * avoiding reentrant calls is up to you.
 *
 * A strict division of labor limits complexity. This module knows as little
 * about the meanings of different MIDI parameters and controllers as possible
 * to do its job: it knows which controllers are overloaded to serve as
 * channel mode messages, and which are overloaded to provide access to the
 * RPN and NRPN space. It knows which controllers are 14-bit, 7-bit, or 1-bit
 * according to the table online at midi.org. (All parameters are treated as
 * 14-bit.) It does not know or care about the specified default values;
 * client code is expected to know those defaults for whatever controls it
 * actually implements, and supply them when calling midictl_*read(). That
 * avoids the need for a large table of specified values for things most
 * clients will never read. A header file defining the official defaults could
 * be useful for clients to include for use when calling midictl_*read, but
 * is not part of this module. Reset All Controllers is simply handled by
 * forgetting controllers have been written at all, so the next read by
 * the client will return the client's supplied default.
 *
 * An incoming MIDI stream may refer to many controllers and parameters the
 * client does not implement. To limit memory use, messages are ignored by
 * default if they target a controller or parameter the client has never
 * read. To indicate which controllers/parameters it supports, the client
 * should simply read them when starting.
 *
 * Where the task is to generically process some MIDI stream without losing
 * data, accept_any_ctl_rpn can be set to 1 in the midictl structure, and
 * state will be kept for any incoming controller or RPN update. The separate
 * flag accept_any_nrpn enables the same behavior for nonregistered parameters.
 *
 * Whenever a change is made to any value for which state is being kept, the
 * notify function will be called with MIDICTL_CTLR, MIDICTL_RPN, or
 * MIDICTL_NRPN, the channel, and the controller, rp, or nrp number,
 * respectively. The controller number will never refer to the LSB of a 14-bit
 * controller. The new /value/ is not included; if the change is of interest,
 * the client reads the value and thereby supplies the default (which can still
 * be needed if the update is to half of a 14-bit value). The notify function
 * is also called, with appropriate evt codes, on receipt of channel mode
 * messages.
 * 
 * Reset All Controllers:
 *
 * The Reset All Controllers message will cause this module to forget settings
 * for all controllers on the affected channel other than those specifically
 * excepted by MIDI RP-015. Registered and nonregistered parameters are not
 * affected. The notify function is then called with evt = MIDICTL_RESET.
 *
 * The client's response to MIDICTL_RESET should include reading all
 * controllers it cares about, to ensure (if the accept_any_ctl_rpn flag is not
 * set) that they will continue to be tracked. The client must also reset to
 * defaults the following pieces of channel state that are not managed by this
 * module, but required by RP-015 to be reset:
 *  Pitch Bend
 *  Channel Pressure
 *  Key Pressure (for all keys on channel)
 * The client does NOT reset the current Program.
 */
#include <sys/midiio.h>
#include <sys/stdint.h>

/*
 * Events that may be reported via a midictl_notify function.
 * Enum starts at 1<<16 so that enum|key can be used as a switch expression.
 * Key is 0 except where shown below.
 */
typedef enum {
	MIDICTL_CTLR      = 1<<16,	/* key=ctlr */
	MIDICTL_RPN       = 2<<16,	/* key=rpn */
	MIDICTL_NRPN      = 3<<16,	/* key=nrpn */
	MIDICTL_RESET     = 4<<16,	/* Reset All Controllers received */
	MIDICTL_NOTES_OFF = 5<<16,	/* All Notes Off received */
	MIDICTL_SOUND_OFF = 6<<16,	/* All Sound Off received */
	MIDICTL_LOCAL     = 7<<16,	/* if (key) localIsOn else localIsOff */
	MIDICTL_MODE      = 8<<16	/* key=mode(1-4)? TBD unimplemented */
} midictl_evt;

/*
 * midictl_notify(void *cookie, midictl_evt evt,
 *                uint_fast8_t chan, uint_fast16_t key)
 */
typedef void
midictl_notify(void *, midictl_evt, uint_fast8_t, uint_fast16_t);

typedef struct midictl_store midictl_store;

typedef struct {
	uint_fast8_t accept_any_ctl_rpn:1; /* 0 ==> ignore chgs for unqueried */
	uint_fast8_t accept_any_nrpn:1;    /* likewise for NRPNs */
	uint_fast8_t base_channel; /* set >= 16 to ignore any MODE messages */
	void *cookie;		   /* this value will be passed to notify */
	midictl_notify *notify;
	/* */
	uint16_t rpn;
	uint16_t nrpn;
	midictl_store *store;
	kmutex_t *lock;
} midictl;

extern int
midictl_open(midictl *);

extern void
midictl_close(midictl *);

/*
 * Called on receipt of a Control Change message. Updates the controller,
 * RPN, or NRPN value as appropriate. When updating a controller or RPN that
 * is defined in the spec as boolean, all values that by definition represent
 * false are coerced to zero. Fires the callback if a value of interest has
 * been changed.
 * ctlval: points to the second byte of the message (therefore, to a two-
 * byte array: controller number and value).
 * midictl_change(midictl *mc, uint_fast8_t chan, uint8_t *ctlval);
 */
extern void
midictl_change(midictl *, uint_fast8_t, uint8_t *);

/*
 * Read the current value of controller ctlr for channel chan.
 * If this is the first time querying this controller on this channel,
 * and accept_any_ctl_rpn is false, any earlier change message for it
 * will have been ignored, so it will be given the value dflt, which is
 * also returned, and future change messages for it will take effect.
 * If the controller has a two-byte value and only one has been explicitly set
 * at the time of the first query, the effect is as if the value had been
 * first set to dflt, then the explicitly-set byte updated.
 * midictl_read(midictl *mc, uint_fast8_t chan,
 *              uint_fast8_t ctlr, uint_fast16_t dflt);
 */
extern uint_fast16_t
midictl_read(midictl *, uint_fast8_t, uint_fast8_t, uint_fast16_t);

/*
 * As for midictl_read, but for registered parameters or nonregistered
 * parameters, respectively.
 */
extern uint_fast16_t
midictl_rpn_read(midictl *mc, uint_fast8_t, uint_fast16_t, uint_fast16_t);
extern uint_fast16_t
midictl_nrpn_read(midictl *mc, uint_fast8_t, uint_fast16_t, uint_fast16_t);

#endif /* _SYS_DEV_MIDICTL_H_ */
