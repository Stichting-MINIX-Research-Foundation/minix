/*-
 * Copyright (c) 2004 Video54 Technologies, Inc.
 * Copyright (c) 2004-2005 Sam Leffler, Errno Consulting
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
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
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

#include <sys/cdefs.h>
#ifdef __FreeBSD__
__FBSDID("$FreeBSD: src/sys/net80211/ieee80211_xauth.c,v 1.2 2004/12/31 22:42:38 sam Exp $");
#endif
#ifdef __NetBSD__
__KERNEL_RCSID(0, "$NetBSD: ieee80211_xauth.c,v 1.5 2006/02/27 01:08:28 dyoung Exp $");
#endif

/*
 * External authenticator placeholder module.
 *
 * This support is optional; it is only used when the 802.11 layer's
 * authentication mode is set to use 802.1x or WPA is enabled separately
 * (for WPA-PSK).  If compiled as a module this code does not need
 * to be present unless 802.1x/WPA is in use.
 *
 * The authenticator hooks into the 802.11 layer.  At present we use none
 * of the available callbacks--the user mode authenticator process works
 * entirely from messages about stations joining and leaving.
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   

#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>
#include <net/route.h>

#include <net80211/ieee80211_var.h>

/*
 * One module handles everything for now.  May want
 * to split things up for embedded applications.
 */
static const struct ieee80211_authenticator xauth = {
	.ia_name	= "external",
	.ia_attach	= NULL,
	.ia_detach	= NULL,
	.ia_node_join	= NULL,
	.ia_node_leave	= NULL,
};

IEEE80211_CRYPTO_SETUP(ieee80211_external_auth_setup)
{
	ieee80211_authenticator_register(IEEE80211_AUTH_8021X, &xauth);
	ieee80211_authenticator_register(IEEE80211_AUTH_WPA, &xauth);
}
