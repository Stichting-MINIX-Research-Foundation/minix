/*	$NetBSD: isapnp.c,v 1.5 2008/12/14 17:03:43 christos Exp $	 */

/*
 * Copyright (c) 1997
 *	Matthias Drochner.  All rights reserved.
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
 *
 */

/*
 * minimal ISA PnP implementation: find adapter, return settings (1 IO and 1
 * DMA only for now)
 */

#include <sys/types.h>
#include <machine/pio.h>

#include <lib/libsa/stand.h>

#include <libi386.h>

#include "isapnpvar.h"

#define PNPADDR 0x0279
#define PNPWDATA 0x0a79
#define PNPRDATAMIN 0x0203
#define PNPRDATAMAX 0x03ff

enum {
	DATAPORT,
	ISOL,
	CONTROL,
	WAKE,
	RESDATA,
	RESSTAT,
	SETCSN,
	SETLDEV
};

#define MEMBASE 0x40
#define IOBASE 0x60
#define INTBASE 0x70
#define DMABASE 0x74

static int      pnpdataport;

static int
getiobase(int nr)
{
	unsigned short  iobase;

	outb(PNPADDR, SETLDEV);
	outb(PNPWDATA, 0);	/* subdev 0 */

	outb(PNPADDR, IOBASE + nr * 2);
	iobase = (inb(pnpdataport) << 8);
	outb(PNPADDR, IOBASE + nr * 2 + 1);
	iobase |= inb(pnpdataport);

	return iobase;
}

static int
getdmachan(int nr)
{
	unsigned char   dmachannel;

	outb(PNPADDR, SETLDEV);
	outb(PNPWDATA, 0);	/* subdev 0 */

	outb(PNPADDR, DMABASE + nr);
	dmachannel = inb(pnpdataport) & 0x07;

	return dmachannel;
}

struct cardid {
	unsigned char   eisaid[4];
	unsigned int    serial;
	unsigned char   crc;
};

/*
 do isolation, call pnpscanresc() in board config state
 */
static int
pnpisol(int csn)
{
	unsigned char   buf[9];
	int             i, j;
	struct cardid  *id;
	unsigned char   crc = 0x6a;

	/*
	 * do 72 pairs of reads from ISOL register all but 1 go to sleep
	 * state (ch. 3.3)
	 */
	outb(PNPADDR, ISOL);
	delay(1000);

	for (i = 0; i < 9; i++) {
		for (j = 0; j < 8; j++) {
			unsigned char   a, b;
			int             bitset;

			a = inb(pnpdataport);
			b = inb(pnpdataport);
			if ((a == 0x55) && (b == 0xaa))
				bitset = 1;
			else if ((a == 0xff) && (b == 0xff))
				bitset = 0;
			else
				return -1;	/* data port conflict */

			buf[i] = (buf[i] >> 1) | (bitset << 7);

			if (i < 8)	/* calc crc for first 8 bytes (app.
					 * B.2) */
				crc = (crc >> 1) |
				  ((bitset != ((crc & 1) == !(crc & 2))) << 7);

			delay(250);
		}
	}
	id = (struct cardid *) buf;

	if (id->crc != crc)
		return 0;	/* normal end */

	outb(PNPADDR, SETCSN);
	outb(PNPWDATA, csn);	/* set csn for winning card and put it to
				 * config state */

	return (id->eisaid[0] << 24) | (id->eisaid[1] << 16)
		| (id->eisaid[2] << 8) | (id->eisaid[3]);
}

static void
pnpisolreset(void)
{
	outb(PNPADDR, WAKE);
	outb(PNPWDATA, 0);	/* put all remaining cards to isolation state */
}

/*
 send initiation sequence (app. B.1)
 */
static void
pnpinit(void)
{
	int             i;
	unsigned char   key = 0x6a;

	outb(PNPADDR, 0);
	outb(PNPADDR, 0);

	for (i = 0; i < 32; i++) {
		outb(PNPADDR, key);
		key = (key >> 1) |
			(((key & 1) == !(key & 2)) << 7);
	}
}

int
isapnp_finddev(int id, int *iobase, int *dmachan)
{
	int             csn;

	outb(PNPADDR, CONTROL);
	outb(PNPWDATA, 2);	/* XXX force wait for key */

	/* scan all allowed data ports (ch. 3.1) */
	for (pnpdataport = PNPRDATAMIN; pnpdataport <= PNPRDATAMAX;
	     pnpdataport += 4) {
		int             res, found = 0;

		pnpinit();	/* initiation sequence */

		outb(PNPADDR, CONTROL);
		outb(PNPWDATA, 4);	/* CSN=0 - only these respond to
					 * WAKE[0] */

		outb(PNPADDR, WAKE);
		outb(PNPWDATA, 0);	/* put into isolation state */

		outb(PNPADDR, DATAPORT);
		outb(PNPWDATA, pnpdataport >> 2);	/* set READ_DATA port */

		csn = 0;
		do {
			res = pnpisol(++csn);

			if ((res) == id) {
				if (iobase)
					*iobase = getiobase(0);
				if (dmachan)
					*dmachan = getdmachan(0);
				found = 1;
			}
			pnpisolreset();
		} while ((res != 0) && (res != -1));

		outb(PNPADDR, CONTROL);
		outb(PNPWDATA, 2);	/* return to wait for key */

		if (csn > 1)	/* at least 1 board found */
			return !found;

		/* if no board found, try next dataport */
	}
	return -1;		/* nothing found */
}
