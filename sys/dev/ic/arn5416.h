/*	$NetBSD: arn5416.h,v 1.1 2013/03/30 02:53:01 christos Exp $	*/
/*
 * Copyright (c) 2001, 2004 The NetBSD Foundation, Inc.
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

#ifndef _IF_ARN5416_H_
#define _IF_ARN5416_H_

int	ar5416_attach(struct athn_softc *);
int	ar5416_init_calib(struct athn_softc *,
	    struct ieee80211_channel *, struct ieee80211_channel *);
uint8_t	ar5416_get_rf_rev(struct athn_softc *);
void	ar5416_reset_addac(struct athn_softc *, struct ieee80211_channel *);
void	ar5416_rf_reset(struct athn_softc *, struct ieee80211_channel *);
void	ar5416_reset_bb_gain(struct athn_softc *, struct ieee80211_channel *);
void	ar5416_swap_rom(struct athn_softc *);
void	ar5416_set_txpower(struct athn_softc *, struct ieee80211_channel *,
	    struct ieee80211_channel *);
const struct ar_spur_chan *
	ar5416_get_spur_chans(struct athn_softc *, int);

#endif /* _IF_ARN5416_H_ */
