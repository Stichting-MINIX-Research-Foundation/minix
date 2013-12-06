/*	$NetBSD: pcio.c,v 1.30 2011/06/08 16:04:40 joerg Exp $	 */

/*
 * Copyright (c) 1996, 1997
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
 * console I/O
 * needs lowlevel routines from conio.S and comio.S
 */

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>
#include <sys/bootblock.h>

#include "libi386.h"
#include "bootinfo.h"

extern struct x86_boot_params boot_params;

struct btinfo_console btinfo_console;

#ifdef SUPPORT_SERIAL
static int iodev;

#ifdef DIRECT_SERIAL
#include "comio_direct.h"

#define cominit_x()	btinfo_console.speed = \
			    cominit_d(btinfo_console.addr, btinfo_console.speed)
#define computc_x(ch)	computc_d(ch, btinfo_console.addr)
#define comgetc_x()	comgetc_d(btinfo_console.addr)
#define comstatus_x()	comstatus_d(btinfo_console.addr)

#else
#define cominit_x()	cominit(iodev - CONSDEV_COM0)
#define computc_x(ch)	computc(ch, iodev - CONSDEV_COM0)
#define comgetc_x()	comgetc(iodev - CONSDEV_COM0)
#define comstatus_x()	comstatus(iodev - CONSDEV_COM0)

#endif /* DIRECT_SERIAL */

static int getcomaddr(int);
#endif /* SUPPORT_SERIAL */

#define POLL_FREQ 10

static void
wait(int us)
{
	int prev = biosgetsystime();
	int tgt = prev + (20 * us) / 1000000;
	int new;

	while ((new = biosgetsystime()) < tgt) {
		if (new < prev) /* XXX timer wrapped */
			break;
		prev = new;
	}
}

#ifdef SUPPORT_SERIAL
static int
getcomaddr(int idx)
{
	short addr;
#ifdef CONSADDR
	if (CONSADDR != 0)
		return CONSADDR;
#endif
	/* read in BIOS data area */
	pvbcopy((void *)(0x400 + 2 * idx), &addr, 2);
	return addr;
}
#endif

void
clear_pc_screen(void)
{
#ifdef SUPPORT_SERIAL
	/* Clear the screen if we are on a glass tty. */
	if (iodev == CONSDEV_PC)
		conclr();
#endif
}

void
initio(int dev)
{
#ifdef SUPPORT_SERIAL
	int i;

#if defined(DIRECT_SERIAL) && defined(CONSPEED)
	btinfo_console.speed = CONSPEED;
#else
	btinfo_console.speed = 9600;
#endif

	switch (dev) {
	case CONSDEV_AUTO:
		for (i = 0; i < 3; i++) {
			iodev = CONSDEV_COM0 + i;
			btinfo_console.addr = getcomaddr(i);
			if (!btinfo_console.addr)
				break;
			conputc('0' + i); /* to tell user what happens */
			cominit_x();
#ifdef DIRECT_SERIAL
			/* check for:
			 *  1. successful output
			 *  2. optionally, keypress within 7s
			 */
			if (	computc_x(':') &&
				computc_x('-') &&
				computc_x('(')
#ifdef COMCONS_KEYPRESS
			   && awaitkey(7, 0)
#endif
			   )
				goto ok;
#else /* ! DIRECT_SERIAL */
			/*
			 * serial console must have hardware handshake!
			 * check:
			 *  1. character output without error
			 *  2. status bits for modem ready set
			 *     (status seems only useful after character output)
			 *  3. optionally, keypress within 7s
			 */
			if (!(computc_x('@') & 0x80)
			    && (comstatus_x() & 0x00b0)
#ifdef COMCONS_KEYPRESS
			    && awaitkey(7, 0)
#endif
			    )
				goto ok;
#endif /* DIRECT_SERIAL */
		}
		iodev = CONSDEV_PC;
ok:
		break;
	case CONSDEV_COM0:
	case CONSDEV_COM1:
	case CONSDEV_COM2:
	case CONSDEV_COM3:
		iodev = dev;
		btinfo_console.addr = getcomaddr(iodev - CONSDEV_COM0);
		if (!btinfo_console.addr)
			goto nocom;
		cominit_x();
		break;
	case CONSDEV_COM0KBD:
	case CONSDEV_COM1KBD:
	case CONSDEV_COM2KBD:
	case CONSDEV_COM3KBD:
		iodev = dev - CONSDEV_COM0KBD + CONSDEV_COM0;
		i = iodev - CONSDEV_COM0;
		btinfo_console.addr = getcomaddr(i);
		if (!btinfo_console.addr)
			goto nocom;
		conputc('0' + i); /* to tell user what happens */
		cominit_x();
#ifdef DIRECT_SERIAL
			/* check for:
			 *  1. successful output
			 *  2. optionally, keypress within 7s
			 */
			if (	computc_x(':') &&
				computc_x('-') &&
				computc_x('(')
#ifdef COMCONS_KEYPRESS
			   && awaitkey(7, 0)
#endif
			   )
				break;
#else /* ! DIRECT_SERIAL */
			/*
			 * serial console must have hardware handshake!
			 * check:
			 *  1. character output without error
			 *  2. status bits for modem ready set
			 *     (status seems only useful after character output)
			 *  3. optionally, keypress within 7s
			 */
			if (!(computc_x('@') & 0x80)
			    && (comstatus_x() & 0x00b0)
#ifdef COMCONS_KEYPRESS
			    && awaitkey(7, 0)
#endif
			    )
				break;
#endif /* DIRECT_SERIAL */
	default:
nocom:
		iodev = CONSDEV_PC;
		break;
	}
	conputc('\015');
	conputc('\n');
	strncpy(btinfo_console.devname, iodev == CONSDEV_PC ? "pc" : "com", 16);

#else /* !SUPPORT_SERIAL */
	btinfo_console.devname[0] = 'p';
	btinfo_console.devname[1] = 'c';
	btinfo_console.devname[2] = 0;
#endif /* SUPPORT_SERIAL */
}

static inline void internal_putchar(int);

static inline void
internal_putchar(int c)
{
#ifdef SUPPORT_SERIAL
	switch (iodev) {
	case CONSDEV_PC:
#endif
		conputc(c);
#ifdef SUPPORT_SERIAL
		break;
	case CONSDEV_COM0:
	case CONSDEV_COM1:
	case CONSDEV_COM2:
	case CONSDEV_COM3:
		computc_x(c);
		break;
	}
#endif
}

void
putchar(int c)
{
	if (c == '\n')
		internal_putchar('\r');
	internal_putchar(c);
}

int
#if !defined(__minix)
getchar(void)
#else
getchar_ex(void)
#endif /* !defined(__minix) */
{
	int c;
#ifdef SUPPORT_SERIAL
	switch (iodev) {
	default: /* to make gcc -Wall happy... */
	case CONSDEV_PC:
#endif
		while (!coniskey())
			;
		c = congetc();
#ifdef CONSOLE_KEYMAP
		{
#if !defined(__minix)
			char *cp = strchr(CONSOLE_KEYMAP, c);
#else
			char *cp = strchr(CONSOLE_KEYMAP, c & 0xff);
#endif /* !defined(__minix) */
			if (cp != 0 && cp[1] != 0)
#if !defined(__minix)
				c = cp[1];
#else
				c = cp[1] | (c & 0xff00);
#endif /* !defined(__minix) */
		}
#endif
		return c;
#ifdef SUPPORT_SERIAL
	case CONSDEV_COM0:
	case CONSDEV_COM1:
	case CONSDEV_COM2:
	case CONSDEV_COM3:
#ifdef DIRECT_SERIAL
		c = comgetc_x();
#else
		do {
			c = comgetc_x();
		} while ((c >> 8) == 0xe0); /* catch timeout */
#ifdef COMDEBUG
		if (c & 0x8000) {
			printf("com input %x, status %x\n",
			       c, comstatus_x());
		}
#endif
		c &= 0xff;
#endif /* DIRECT_SERIAL */
		return c;
	}
#endif /* SUPPORT_SERIAL */
}

#if defined(__minix)
int
getchar(void)
{
	return getchar_ex() & 0xff;
}
#endif /* defined(__minix) */

int
iskey(int intr)
{
#ifdef SUPPORT_SERIAL
	switch (iodev) {
	default: /* to make gcc -Wall happy... */
	case CONSDEV_PC:
#endif
		return (intr && conisshift()) || coniskey();
#ifdef SUPPORT_SERIAL
	case CONSDEV_COM0:
	case CONSDEV_COM1:
	case CONSDEV_COM2:
	case CONSDEV_COM3:
#ifdef DIRECT_SERIAL
		return !!comstatus_x();
#else
		return !!(comstatus_x() & 0x0100);
#endif
	}
#endif /* SUPPORT_SERIAL */
}

char
awaitkey(int timeout, int tell)
{
	int i;
	char c = 0;

	i = timeout * POLL_FREQ;

	for (;;) {
		if (tell && (i % POLL_FREQ) == 0) {
			char numbuf[32];
			int len;

			len = snprintf(numbuf, sizeof(numbuf), "%d seconds. ",
			    i/POLL_FREQ);
			if (len > 0 && len < sizeof(numbuf)) {
				char *p = numbuf;

				printf("%s", numbuf);
				while (*p)
					*p++ = '\b';
				printf("%s", numbuf);
			}
		}
		if (iskey(1)) {
			/* flush input buffer */
			while (iskey(0))
				c = getchar();
			if (c == 0)
				c = -1;
			goto out;
		}
		if (i--)
			wait(1000000 / POLL_FREQ);
		else
			break;
	}

out:
	if (tell)
		printf("0 seconds.     \n");

	return c;
}

void
wait_sec(int sec)
{

	wait(sec * 1000000);
}
