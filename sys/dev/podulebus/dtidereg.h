/* $NetBSD: dtidereg.h,v 1.4 2005/12/11 12:23:28 christos Exp $ */

/* This file is in the public domain */

/*
 * dtidereg.h - hardware-related constants of the D.T. Software IDE interface.
 */

/*
 * This is mostly reverse-engineered by Ben Harris from the driver that
 * comes with the board and the board itself.  Treat with caution.
 */

#define DTIDE_NCHANNELS	2

#define DTIDE_MAGICBASE	0x2000

#define DTIDE_REGSHIFT	5 /* ie DA0 == LA5 */
#define DTIDE_CMDBASE0	0x2400
#define DTIDE_CTLBASE0	0x2500
#define DTIDE_CMDBASE1	0x2600
#define DTIDE_CTLBASE1	0x2700

