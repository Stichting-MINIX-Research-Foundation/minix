/* $NetBSD: hcidereg.h,v 1.1 2001/06/13 18:31:43 bjh21 Exp $ */

/* This file is in the public domain */

/*
 * hcidereg.h - hardware-related constants of the HCCS IDE interface.
 */

#define HCIDE_NCHANNELS	3

/* In FAST space: */

#define HCIDE_CMD0	0x2100	/* Command registers, on-board channel */
#define HCIDE_CMD1	0x2140	/* Command registers, internal channel */
#define HCIDE_CMD2	0x2180	/* Command registers, external channel */
#define HCIDE_CTL	0x21c0	/* Control registers, all channels */
