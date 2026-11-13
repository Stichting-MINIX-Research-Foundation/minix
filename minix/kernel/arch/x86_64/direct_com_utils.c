/*	$NetBSD: comio_direct.c,v 1.12 2022/09/05 14:14:42 tsutsui Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995, 1996, 1997
 *	Charles M. Hannum.  All rights reserved.
 *
 * Taken from sys/dev/isa/com.c and integrated into standalone boot
 * programs by Martin Husemann.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */
 
#define UNPAGED 1	/* for proper kmain() prototype */

#include <minix/minlib.h>
#include <minix/cpufeature.h>
#include "string.h"
#include "direct_utils.h"
#include "serial.h"
#include "glo.h"

/* preread buffer for xon/xoff handling */
#define XON	0x11
#define	XOFF	0x13
#define	SERBUFSIZE	16
static u_char serbuf[SERBUFSIZE];
static int serbuf_read = 0;
static int serbuf_write = 0;
static int stopped = 0;

#define	ISSET(t,f)	((t) & (f))

#define	divrnd(n, q)	(((n)*2/(q)+1)/2)	/* divide and round off */
#define RATE_9600 divrnd((COM_FREQ / 16), 9600)



int cominit_d(int, int);
int computc_d(int, int);
int comgetc_d(int);
int comstatus_d(int);

void direct_com_init()
{
	cominit_d(COM1_BASE, 115200);
}

void direct_com_print(const char *str)
{
	while (*str) {
		direct_com_print_char(*str);
		str++;
	}
}

void direct_com_print_char(char c)
{
	computc_d(c, COM1_BASE);
	//ser_putc(c);
}

int direct_com_read_char(unsigned char *ch)
{
	u_char c;
	
	c = comgetc_d(COM1_BASE);
	*ch = c;
	return 1;
}


/*
 * calculate divisor for a given speed
 */
static int comspeed(long speed)
{
	int x, err;

	if (speed <= 0)
		speed = 9600;
	x = divrnd((COM_FREQ / 16), speed);
	if (x <= 0)
		return RATE_9600;
	err = divrnd((COM_FREQ / 16) * 1000, speed * x) - 1000;
	if (err < 0)
		err = -err;
	if (err > COM_TOLERANCE)
		return RATE_9600;
	return x;
}

/*
 * get a character
 */
int comgetc_d(int combase)
{
	u_char stat, c;

	if (serbuf_read != serbuf_write) {
		c = serbuf[serbuf_read++];
		if (serbuf_read >= SERBUFSIZE)
			serbuf_read = 0;
		return c;
	}

	for (;;) {
		while (!ISSET(stat = inb(combase + LSRREG), LSR_DR))
			continue;
		c = inb(combase + RBRREG);
		inb(combase + IRRREG);
		if (c != XOFF) {
			stopped = 0;
			break;	/* got a real char, deliver it... */
		}
		stopped = 1;
	}
	return c;
}

/*
 * output a character, return nonzero on success
 */
int computc_d(int c, int combase)
{
	u_char stat;
	int timo;

	/* check for old XOFF */
	while (stopped)
		comgetc_d(combase);	/* wait for XON */

	/* check for new XOFF */
	if (comstatus_d(combase)) {
		int x = comgetc_d(combase);	/* XOFF handled in comgetc_d */
		/* stuff char into preread buffer */
		serbuf[serbuf_write++] = x;
		if (serbuf_write >= SERBUFSIZE)
			serbuf_write = 0;
	}

	/* wait for any pending transmission to finish */
	timo = 50000;
	while (!ISSET(stat = inb(combase + LSRREG), LSR_THRE)
	    && --timo)
		continue;
	if (timo == 0) return 0;
	outb(combase + THRREG, c);
	/* wait for this transmission to complete */
	timo = 1500000;
	while (!ISSET(stat = inb(combase + LSRREG), LSR_THRE)
	    && --timo)
		continue;
	if (timo == 0) return 0;
	/* clear any interrupts generated by this transmission */
	inb(combase + IRRREG);

	return 1;
}

/*
 * Initialize UART to known state.
 */
int cominit_d(int combase, int speed)
{
	int rate, err;

	serbuf_read = 0;
	serbuf_write = 0;

	outb(combase + LCRREG, LCR_DLAB);
	if (speed == 0) {
		/* Try to determine the current baud rate */
		rate = inb(combase + DLLREG) | inb(combase + DLMREG) << 8;
		if (rate == 0)
			rate = RATE_9600;
		speed = divrnd((COM_FREQ / 16), rate);
		err = speed - (speed + 150)/300 * 300;
		speed -= err;
		if (err < 0)
			err = -err;
		if (err > 50)
			speed = 9600;
	}
	rate = comspeed(speed);
	outb(combase + DLLREG, rate);
	outb(combase + DLMREG, rate >> 8);
	outb(combase + LCRREG, LCR_8BIT);
	outb(combase + MCRREG, MCR_DTR | MCR_RTS);
	outb(combase + FICRREG,
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_1);
	outb(combase + IERREG, 0);

	return speed;
}

/*
 * return nonzero if input char available, do XON/XOFF handling
 */
int comstatus_d(int combase)
{
	/* check if any preread input is already there */
	if (serbuf_read != serbuf_write) return 1;

	/* check for new stuff on the port */
	if (ISSET(inb(combase + LSRREG), LSR_DR)) {
		/* this could be XOFF, which we would swallow, so we can't
		   claim there is input available... */
		int c = inb(combase + RBRREG);
		inb(combase + IRRREG);
		if (c == XOFF) {
			stopped = 1;
		} else {
			/* stuff char into preread buffer */
			serbuf[serbuf_write++] = c;
			if (serbuf_write >= SERBUFSIZE)
				serbuf_write = 0;
			return 1;
		}
	}

	return 0;	/* nothing out there... */
}
