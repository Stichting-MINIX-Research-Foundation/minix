/*	$NetBSD: umidi_quirks.c,v 1.19 2014/12/21 23:00:35 mrg Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Takuya SHIOZAKI (tshiozak@NetBSD.org).
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: umidi_quirks.c,v 1.19 2014/12/21 23:00:35 mrg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/auconv.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uaudioreg.h>
#include <dev/usb/umidi_quirks.h>

/*
 * quirk codes for UMIDI
 */

#ifdef UMIDIQUIRK_DEBUG
#define DPRINTF(x)	if (umidiquirkdebug) printf x
#define DPRINTFN(n,x)	if (umidiquirkdebug >= (n)) printf x
int	umidiquirkdebug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif


/*
 * YAMAHA UX-256
 *  --- this is a typical yamaha device, but has a broken descriptor :-<
 */

UMQ_FIXED_EP_DATA_DEF(YAMAHA, YAMAHA_UX256, ANYIFACE, 1, 1) = {
	/* out */
	{ 0, 16 },
	/* in */
	{ 1, 8 }
};
UMQ_FIXED_EP_DEF(YAMAHA, YAMAHA_UX256, ANYIFACE, 1, 1);

UMQ_DEF(YAMAHA, YAMAHA_UX256, ANYIFACE) = {
	UMQ_FIXED_EP_REG(YAMAHA, YAMAHA_UX256, ANYIFACE),
#if 0
	UMQ_YAMAHA_REG(YAMAHA, ANYPRODUCT, ANYIFACE),
#endif
	UMQ_TERMINATOR
};


/*
 * YAMAHA generic
 */
UMQ_DEF(YAMAHA, ANYPRODUCT, ANYIFACE) = {
	UMQ_YAMAHA_REG(YAMAHA, ANYPRODUCT, ANYIFACE),
	UMQ_TERMINATOR
};


/*
 * ROLAND UM-1
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UM1, 2, 1, 1) = {
	/* out */
	{ 0, 1 },
	/* in */
	{ 1, 1 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UM1, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UM1, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UM1, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND SC-8850
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_SC8850, 2, 1, 1) = {
	/* out */
	{ 0, 6 },
	/* in */
	{ 1, 6 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_SC8850, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_SC8850, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_SC8850, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND SD-90
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_SD90, 2, 1, 1) = {
	/* out */
	{ 0, 4 },
	/* in */
	{ 1, 4 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_SD90, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_SD90, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_SD90, 2),
	UMQ_TERMINATOR
};


/*
 * ROLAND UM-880 (native mode)
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UM880N, 0, 1, 1) = {
	/* out */
	{ 0, 9 },
	/* in */
	{ 1, 9 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UM880N, 0, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UM880N, 0) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UM880N, 0),
	UMQ_TERMINATOR
};

/*
 * ROLAND UA-100
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UA100, 2, 1, 1) = {
	/* out */
	{ 0, 3 },
	/* in */
	{ 1, 3 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UA100, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UA100, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UA100, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND UM-4
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UM4, 2, 1, 1) = {
	/* out */
	{ 0, 4 },
	/* in */
	{ 1, 4 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UM4, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UM4, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UM4, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND U-8
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_U8, 2, 1, 1) = {
	/* out */
	{ 0, 2 },
	/* in */
	{ 1, 2 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_U8, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_U8, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_U8, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND UM-2
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UM2, 2, 1, 1) = {
	/* out */
	{ 0, 2 },
	/* in */
	{ 1, 2 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UM2, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UM2, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UM2, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND SC-8820
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_SC8820, 2, 1, 1) = {
	/* out */
	{ 0, 5 }, /* cables 0, 1, 4 only */
	/* in */
	{ 1, 5 } /* do. */
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_SC8820, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_SC8820, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_SC8820, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND PC-300
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_PC300, 2, 1, 1) = {
	/* out */
	{ 0, 1 },
	/* in */
	{ 1, 1 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_PC300, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_PC300, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_PC300, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND SK-500
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_SK500, 2, 1, 1) = {
	/* out */
	{ 0, 5 }, /* cables 0, 1, 4 only */
	/* in */
	{ 1, 5 } /* do. */
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_SK500, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_SK500, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_SK500, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND SC-D70
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_SCD70, 2, 1, 1) = {
	/* out */
	{ 0, 3 },
	/* in */
	{ 1, 3 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_SCD70, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_SCD70, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_SCD70, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND XV-5050
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_XV5050, 0, 1, 1) = {
	/* out */
	{ 0, 1 },
	/* in */
	{ 1, 1 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_XV5050, 0, 1, 1);

UMQ_DEF(ROLAND, ROLAND_XV5050, 0) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_XV5050, 0),
	UMQ_TERMINATOR
};

/*
 * ROLAND UM-550
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UM550, 0, 1, 1) = {
	/* out */
	{ 0, 6 },
	/* in */
	{ 1, 6 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UM550, 0, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UM550, 0) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UM550, 0),
	UMQ_TERMINATOR
};

/*
 * ROLAND SD-20
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_SD20, 0, 1, 1) = {
	/* out */
	{ 0, 2 },
	/* in */
	{ 1, 3 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_SD20, 0, 1, 1);

UMQ_DEF(ROLAND, ROLAND_SD20, 0) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_SD20, 0),
	UMQ_TERMINATOR
};

/*
 * ROLAND SD-80
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_SD80, 0, 1, 1) = {
	/* out */
	{ 0, 4 },
	/* in */
	{ 1, 4 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_SD80, 0, 1, 1);

UMQ_DEF(ROLAND, ROLAND_SD80, 0) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_SD80, 0),
	UMQ_TERMINATOR
};

/*
 * ROLAND UA-700
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UA700, 3, 1, 1) = {
	/* out */
	{ 0, 2 },
	/* in */
	{ 1, 2 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UA700, 3, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UA700, 3) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UA700, 3),
	UMQ_TERMINATOR
};

/*
 * ROLAND UA-1000
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UA1000, 3, 1, 1) = {
	/* out */
	{ 0, 2 },
	/* in */
	{ 1, 2 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UA1000, 3, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UA1000, 3) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UA1000, 3),
	UMQ_TERMINATOR
};

/*
 * ROLAND UA-101
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UA101, 2, 1, 1) = {
	/* out */
	{ 0, 2 },
	/* in */
	{ 1, 2 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UA101, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UA101, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UA101, 2),
	UMQ_TERMINATOR
};

UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UA101F, 2, 1, 1) = {
	/* out */
	{ 0, 2 },
	/* in */
	{ 1, 2 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UA101F, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UA101F, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UA101F, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND Fantom-X
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_FANTOMX, 0, 1, 1) = {
	/* out */
	{ 0, 1 },
	/* in */
	{ 1, 1 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_FANTOMX, 0, 1, 1);

UMQ_DEF(ROLAND, ROLAND_FANTOMX, 0) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_FANTOMX, 0),
	UMQ_TERMINATOR
};

/*
 * ROLAND PCR
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_PCR, 0, 1, 1) = {
	/* out */
	{ 0, 3 },
	/* in */
	{ 1, 3 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_PCR, 0, 1, 1);

UMQ_DEF(ROLAND, ROLAND_PCR, 0) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_PCR, 0),
	UMQ_TERMINATOR
};

/*
 * ROLAND UM-3EX
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UM3, 0, 1, 1) = {
	/* out */
	{ 0, 3 },
	/* in */
	{ 1, 3 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UM3, 0, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UM3, 0) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UM3, 0),
	UMQ_TERMINATOR
};

/*
 * ROLAND UA-25
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UA25, 2, 1, 1) = {
	/* out */
	{ 0, 1 },
	/* in */
	{ 1, 1 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UA25, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UA25, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UA25, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND UA-4FX
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UA4FX, 2, 1, 1) = {
	/* out */
	{ 0, 1 },
	/* in */
	{ 1, 1 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UA4FX, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UA4FX, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UA4FX, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND SonicCell
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_SONICCELL, 2, 1, 1) = {
	/* out */
	{ 0, 1 },
	/* in */
	{ 1, 1 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_SONICCELL, 2, 1, 1);

UMQ_DEF(ROLAND, ROLAND_SONICCELL, 2) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_SONICCELL, 2),
	UMQ_TERMINATOR
};

/*
 * ROLAND UM-ONE
 */
UMQ_FIXED_EP_DATA_DEF(ROLAND, ROLAND_UMONE, ANYIFACE, 1, 1) = {
	/* out */
	{ 0, 1 },
	/* in */
	{ 1, 1 }
};
UMQ_FIXED_EP_DEF(ROLAND, ROLAND_UMONE, ANYIFACE, 1, 1);

UMQ_DEF(ROLAND, ROLAND_UMONE, ANYIFACE) = {
	UMQ_FIXED_EP_REG(ROLAND, ROLAND_UMONE, ANYIFACE),
	UMQ_TERMINATOR
};

/*
 * Midiman Midisport 2x4. This has 2 physical MIDI IN jacks that are read
 * on endpoint 0x81 (descriptor index 0). It has 4 physical MIDI OUT jacks
 * that can be written on endpoints 2 or 4 (at descriptor index 2 or 4,
 * coincidentally) interchangeably: either endpoint will accept a Cable Number
 * field of 0 to 3, and data for a given CN will be routed to the same
 * physical output regardless of the endpoint used for the transfer. But
 * there's a catch: flow-control feedback only goes to endpoint 2 for
 * CN 0 and 2, and only to endpoint 4 for CN 1 and 3. If you send output at
 * high rates for CN 0 or 2 over endpoint 4, or for CN 1 or 3 over endpoint 2,
 * the USB transfers complete as fast as possible, giving you an apparent data
 * rate much higher than MIDI's 3125 cps (easy to measure using dd to blast a
 * bunch of midi data to the rmidi device). Of course that isn't a way to make
 * MIDI faster, just a way to overrun the device buffer and spray bits on the
 * floor. So this device needs the fixed endpoint quirk, the fixed cable number
 * quirk (to make sure CNs 0 and 2 are put on the first endpoint and 1 and 3
 * on the other), and then the fixed mididev-assignment quirk (to match jacks
 * to mididevs so the rmidi devices match the order of the blinkenlights).
 */
UMQ_FIXED_EP_DATA_DEF(MIDIMAN, MIDIMAN_MIDISPORT2X4, ANYIFACE, 2, 1) = {
	/* out: ep# jacks */
	{ 2, 2 },
	{ 4, 2 },
	/* in: ep# jacks */
	{ 0, 2 }
};
UMQ_FIXED_EP_DEF(MIDIMAN, MIDIMAN_MIDISPORT2X4, ANYIFACE, 2, 1);
UMQ_FIXED_CN_DEF(MIDIMAN, MIDIMAN_MIDISPORT2X4, ANYIFACE) = {
	0, 2, 1, 3, 0, 1
};
UMQ_FIXED_MD_DEF(MIDIMAN, MIDIMAN_MIDISPORT2X4, ANYIFACE) = {
	 0, 0, 2, 1, 1, -1, 3, -1
};
UMQ_DEF(MIDIMAN, MIDIMAN_MIDISPORT2X4, ANYIFACE) = {
	UMQ_FIXED_EP_REG(MIDIMAN, MIDIMAN_MIDISPORT2X4, ANYIFACE),
	UMQ_FIXED_CN_REG(MIDIMAN, MIDIMAN_MIDISPORT2X4, ANYIFACE),
	UMQ_FIXED_MD_REG(MIDIMAN, MIDIMAN_MIDISPORT2X4, ANYIFACE),
	UMQ_TYPE(MIDIMAN_GARBLE),
	UMQ_TERMINATOR
};

/*
 * quirk list
 */
static struct umidi_quirk umidi_quirklist[] = {
	UMQ_REG(YAMAHA, YAMAHA_UX256, ANYIFACE),
	UMQ_REG(YAMAHA, ANYPRODUCT, ANYIFACE),
	UMQ_REG(ROLAND, ROLAND_UM1, 2),
	UMQ_REG(ROLAND, ROLAND_SC8850, 2),
	UMQ_REG(ROLAND, ROLAND_SD90, 2),
	UMQ_REG(ROLAND, ROLAND_UM880N, 0),
	UMQ_REG(ROLAND, ROLAND_UA100, 2),
	UMQ_REG(ROLAND, ROLAND_UM4, 2),
	UMQ_REG(ROLAND, ROLAND_U8, 2),
	UMQ_REG(ROLAND, ROLAND_UM2, 2),
	UMQ_REG(ROLAND, ROLAND_SC8820, 2),
	UMQ_REG(ROLAND, ROLAND_PC300, 2),
	UMQ_REG(ROLAND, ROLAND_SK500, 2),
	UMQ_REG(ROLAND, ROLAND_SCD70, 2),
	UMQ_REG(ROLAND, ROLAND_XV5050, 0),
	UMQ_REG(ROLAND, ROLAND_UM550, 0),
	UMQ_REG(ROLAND, ROLAND_SD20, 0),
	UMQ_REG(ROLAND, ROLAND_SD80, 0),
	UMQ_REG(ROLAND, ROLAND_UA700, 3),
	UMQ_REG(ROLAND, ROLAND_UA1000, 3),
	UMQ_REG(ROLAND, ROLAND_UA101, 2),
	UMQ_REG(ROLAND, ROLAND_UA101F, 2),
	UMQ_REG(ROLAND, ROLAND_FANTOMX, 0),
	UMQ_REG(ROLAND, ROLAND_PCR, 0),
	UMQ_REG(ROLAND, ROLAND_UM3, 0),
	UMQ_REG(ROLAND, ROLAND_UA25, 2),
	UMQ_REG(ROLAND, ROLAND_UA4FX, 2),
	UMQ_REG(ROLAND, ROLAND_SONICCELL, 2),
	UMQ_REG(ROLAND, ROLAND_UMONE, ANYIFACE),
	UMQ_REG(MIDIMAN, MIDIMAN_MIDISPORT2X4, ANYIFACE),
	{ .vendor = 0 },
};


/*
 * quirk utilities
 */

const struct umidi_quirk *
umidi_search_quirk(int vendor, int product, int ifaceno)
{
	struct umidi_quirk *p;
	const struct umq_data *q;

	DPRINTF(("umidi_search_quirk: v=%d, p=%d, i=%d\n",
		 vendor, product, ifaceno));

	for (p=&umidi_quirklist[0]; p->vendor; p++) {
		DPRINTFN(10, ("\tv=%d, p=%d, i=%d",
			      p->vendor, p->product, p->iface));
		if ((p->vendor==vendor || p->vendor==ANYVENDOR) &&
		    (p->product==product || p->product==ANYPRODUCT) &&
		    (p->iface==ifaceno || p->iface==ANYIFACE)) {
			DPRINTFN(10, (" found\n"));
			if (!p->type_mask)
				/* make quirk mask */
				for (q=p->quirks; q->type; q++)
					p->type_mask |= 1<<(q->type-1);
			return p;
		}
		DPRINTFN(10, ("\n"));
	}

	return NULL;
}

static const char *quirk_name[] = {
	"NULL",
	"Fixed Endpoint",
	"Yamaha Specific",
	"Midiman Packet Garbling",
	"Cable Numbers per Endpoint",
	"Cable Numbers Global",
	"Cable Numbers Fixed",
	"Unit Mapping Fixed",
};

void
umidi_print_quirk(const struct umidi_quirk *q)
{
	const struct umq_data *qd;
	if (q) {
		printf("(");
		for (qd=q->quirks; qd->type; qd++)
			printf("%s%s", quirk_name[qd->type],
			       (qd+1)->type?", ":")\n");
	} else {
		printf("(genuine USB-MIDI)\n");
	}
}

const void *
umidi_get_quirk_data_from_type(const struct umidi_quirk *q, u_int32_t type)
{
	const struct umq_data *qd;
	if (q) {
		for (qd=q->quirks; qd->type; qd++)
			if (qd->type == type)
				return qd->data;
	}
	return NULL;
}
