/* $NetBSD: netisr_dispatch.h,v 1.18 2014/06/05 23:48:16 rmind Exp $ */

#ifndef _NET_NETISR_DISPATCH_H_
#define _NET_NETISR_DISPATCH_H_

/*
 * netisr_dispatch: This file is included by the
 *	machine dependent softnet function.  The
 *	DONETISR macro should be set before including
 *	this file.  i.e.:
 *
 * softintr() {
 *	...do setup stuff...
 *	#define DONETISR(bit, fn) do { ... } while (0)
 *	#include <net/netisr_dispatch.h>
 *	#undef DONETISR
 *	...do cleanup stuff.
 * }
 */

#ifndef _NET_NETISR_H_
#error <net/netisr.h> must be included before <net/netisr_dispatch.h>
#endif

/*
 * When adding functions to this list, be sure to add headers to provide
 * their prototypes in <net/netisr.h> (if necessary).
 */

#ifdef INET
#if NARP > 0
	DONETISR(NETISR_ARP,arpintr);
#endif
#endif
#ifdef NETATALK
	DONETISR(NETISR_ATALK,atintr);
#endif
#ifdef MPLS
	DONETISR(NETISR_MPLS,mplsintr);
#endif
#ifdef NATM
	DONETISR(NETISR_NATM,natmintr);
#endif

#endif /* !_NET_NETISR_DISPATCH_H_ */
