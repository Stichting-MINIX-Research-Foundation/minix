/*	$NetBSD: kgdb_stub.c,v 1.29 2015/06/26 14:26:38 christos Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	@(#)kgdb_stub.c	8.4 (Berkeley) 1/12/94
 */

/*
 * "Stub" to allow remote CPU to debug over a serial line using gdb.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kgdb_stub.c,v 1.29 2015/06/26 14:26:38 christos Exp $");

#include "opt_ddb.h"
#include "opt_kgdb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kgdb.h>

#undef	DEBUG_KGDB

#ifdef DEBUG_KGDB
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

/* XXX: Maybe these should be in the MD files? */
#ifndef KGDB_DEV
#define KGDB_DEV NODEV
#endif
#ifndef KGDB_DEVRATE
#define KGDB_DEVRATE 19200
#endif

dev_t kgdb_dev = KGDB_DEV;	/* remote debugging device (NODEV if none) */
int kgdb_rate = KGDB_DEVRATE;	/* remote debugging baud rate */
int kgdb_active = 0;		/* remote debugging active if != 0 */
int kgdb_debug_init = 0;	/* != 0 waits for remote at system init */
int kgdb_debug_panic = 0;	/* != 0 waits for remote on panic */
label_t *kgdb_recover = 0;

static int (*kgdb_getc)(void *);
static void (*kgdb_putc)(void *, int);
static void *kgdb_ioarg;

/* KGDB_BUFLEN must be at least (2*KGDB_NUMREGS*sizeof(kgdb_reg_t)+1) */
static u_char buffer[KGDB_BUFLEN];
static kgdb_reg_t gdb_regs[KGDB_NUMREGS];

#define GETC()	kgdb_waitc(kgdb_ioarg)
#define PUTC(c)	(*kgdb_putc)(kgdb_ioarg, c)

static int
kgdb_waitc(void *arg)
{
	int c;
	while ((c = (*kgdb_getc)(arg)) == -1)
		continue;
	return c;
}

/*
 * db_trap_callback can be hooked by MD port code to handle special
 * cases such as disabling hardware watchdogs while in kgdb.  Name
 * is shared with DDB.
 */
#ifdef DDB
extern void (*db_trap_callback)(int);
#else
void (*db_trap_callback)(int);
#endif

void kgdb_voidop(void);

__weak_alias(kgdb_entry_notice, kgdb_voidop);

void
kgdb_voidop(void)
{
	return;
}

/*
 * This little routine exists simply so that bcopy() can be debugged.
 */
static void
kgdb_copy(void *vsrc, void *vdst, int len)
{
	char *src = vsrc;
	char *dst = vdst;

	while (--len >= 0)
		*dst++ = *src++;
}

#if 0
/* ditto for bzero */
static void
kgdb_zero(void *vptr, int len)
{
	char *ptr = vptr;

	while (--len >= 0)
		*ptr++ = (char) 0;
}
#endif

/*
 * Convert a hex digit into an integer.
 * This returns -1 if the argument passed is no
 * valid hex digit.
 */
static int
digit2i(u_char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	else if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	else if (c >= 'A' && c <= 'F')

		return (c - 'A' + 10);
	else
		return (-1);
}

/*
 * Convert the low 4 bits of an integer into
 * an hex digit.
 */
static u_char
i2digit(int n)
{
	return (hexdigits[n & 0x0f]);
}

/*
 * Convert a byte array into an hex string.
 */
static void
mem2hex(void *vdst, void *vsrc, int len)
{
	u_char *dst = vdst;
	u_char *src = vsrc;

	while (len--) {
		*dst++ = i2digit(*src >> 4);
		*dst++ = i2digit(*src++);
	}
	*dst = '\0';
}

/*
 * Convert an hex string into a byte array.
 * This returns a pointer to the character following
 * the last valid hex digit. If the string ends in
 * the middle of a byte, NULL is returned.
 */
static u_char *
hex2mem(void *vdst, u_char *src, int maxlen)
{
	u_char *dst = vdst;
	int msb, lsb;

	while (*src && maxlen--) {
		msb = digit2i(*src++);
		if (msb < 0)
			return (src - 1);
		lsb = digit2i(*src++);
		if (lsb < 0)
			return (NULL);
		*dst++ = (msb << 4) | lsb;
	}
	return (src);
}

/*
 * Convert an hex string into an integer.
 * This returns a pointer to the character following
 * the last valid hex digit.
 */
static vaddr_t
hex2i(u_char **srcp)
{
	char *src = *srcp;
	vaddr_t r = 0;
	int nibble;

	while ((nibble = digit2i(*src)) >= 0) {
		r *= 16;
		r += nibble;
		src++;
	}
	*srcp = src;
	return (r);
}

/*
 * Send a packet.
 */
static void
kgdb_send(const u_char *bp)
{
	const u_char *p;
	u_char csum, c;

	DPRINTF(("kgdb_send: %s\n", bp));
	do {
		p = bp;
		PUTC(KGDB_START);
		for (csum = 0; (c = *p); p++) {
			PUTC(c);
			csum += c;
		}
		PUTC(KGDB_END);
		PUTC(i2digit(csum >> 4));
		PUTC(i2digit(csum));
	} while ((c = GETC() & 0x7f) == KGDB_BADP);
}

/*
 * Receive a packet.
 */
static int
kgdb_recv(u_char *bp, int maxlen)
{
	u_char *p;
	int c, csum, tmpcsum;
	int len;

	DPRINTF(("kgdb_recv:  "));
	do {
		p = bp;
		csum = len = 0;
		while ((c = GETC()) != KGDB_START)
			DPRINTF(("%c",c));
		DPRINTF(("%c Start ",c));

		while ((c = GETC()) != KGDB_END && len < maxlen) {
			DPRINTF(("%c",c));
			c &= 0x7f;
			csum += c;
			*p++ = c;
			len++;
		}
		csum &= 0xff;
		*p = '\0';
		DPRINTF(("%c End ", c));

		if (len >= maxlen) {
			DPRINTF(("Long- "));
			PUTC(KGDB_BADP);
			continue;
		}
		tmpcsum = csum;

		c = GETC();
		DPRINTF(("%c",c));
		csum -= digit2i(c) * 16;
		c = GETC();
		DPRINTF(("%c",c));
		csum -= digit2i(c);

		if (csum == 0) {
			DPRINTF(("Good+ "));
			PUTC(KGDB_GOODP);
			/* Sequence present? */
			if (bp[2] == ':') {
				DPRINTF(("Seq %c%c ", bp[0], bp[1]));
				PUTC(bp[0]);
				PUTC(bp[1]);
				len -= 3;
				kgdb_copy(bp + 3, bp, len);
			}
			break;
		}
		DPRINTF((" Bad(wanted %x, off by %d)- ", tmpcsum, csum));
		__USE(tmpcsum);
		PUTC(KGDB_BADP);
	} while (1);
	DPRINTF(("kgdb_recv: %s\n", bp));
	return (len);
}

/*
 * This is called by the appropriate tty driver.
 */
void
kgdb_attach(int (*getfn)(void *), void (*putfn)(void *, int), void *ioarg)
{
	kgdb_getc = getfn;
	kgdb_putc = putfn;
	kgdb_ioarg = ioarg;
}

/*
 * This function does all command processing for interfacing to
 * a remote gdb.  Note that the error codes are ignored by gdb
 * at present, but might eventually become meaningful. (XXX)
 * It might makes sense to use POSIX errno values, because
 * that is what the gdb/remote.c functions want to return.
 */
int
kgdb_trap(int type, db_regs_t *regs)
{
	label_t jmpbuf;
	vaddr_t addr;
	size_t len;
	u_char *p;

	kgdb_entry_notice(type, regs);

	if (kgdb_dev == NODEV || kgdb_getc == NULL) {
		/* not debugging */
		return (0);
	}

	db_clear_single_step(regs);

	if (db_trap_callback)
		db_trap_callback(1);

	/* Detect and recover from unexpected traps. */
	if (kgdb_recover != 0) {
		printf("kgdb: caught trap 0x%x at %p\n",
			   type, (void*)PC_REGS(regs));
		kgdb_send("E0E"); /* 14==EFAULT */
		longjmp(kgdb_recover);
	}

	/*
	 * The first entry to this function is normally through
	 * a breakpoint trap in kgdb_connect(), in which case we
	 * must advance past the breakpoint because gdb will not.
	 *
	 * Machines vary as to where they leave the PC after a
	 * breakpoint trap.  Those that leave the PC set to the
	 * address of the trap instruction (i.e. pc532) will not
	 * define FIXUP_PC_AFTER_BREAK(), and therefore will just
	 * advance the PC.  On machines that leave the PC set to
	 * the instruction after the trap, FIXUP_PC_AFTER_BREAK
	 * will be defined to back-up the PC, so that after the
	 * "first-time" part of the if statement below has run,
	 * the PC will be the same as it was on entry.
	 *
	 * On the first entry here, we expect that gdb is not yet
	 * listening to us, so just enter the interaction loop.
	 * After the debugger is "active" (connected) it will be
	 * waiting for a "signaled" message from us.
	 */
	if (kgdb_active == 0) {
		if (!IS_BREAKPOINT_TRAP(type, 0)) {
			/* No debugger active -- let trap handle this. */
			if (db_trap_callback)
				db_trap_callback(0);
			return (0);
		}
		/* Make the PC point at the breakpoint... */
#ifdef	FIXUP_PC_AFTER_BREAK
		FIXUP_PC_AFTER_BREAK(regs);
#endif
		/* ... and then advance past it. */
#ifdef	PC_ADVANCE
		PC_ADVANCE(regs);
#else
		PC_REGS(regs) += BKPT_SIZE;
#endif
		kgdb_active = 1;
	} else {
		/* Tell remote host that an exception has occurred. */
		snprintf(buffer, sizeof(buffer), "S%02x", kgdb_signal(type));
		kgdb_send(buffer);
	}

	/* Stick frame regs into our reg cache. */
	kgdb_getregs(regs, gdb_regs);

	/*
	 * Interact with gdb until it lets us go.
	 * If we cause a trap, resume here.
	 */
	(void)setjmp((kgdb_recover = &jmpbuf));
	for (;;) {
		kgdb_recv(buffer, sizeof(buffer));
		switch (buffer[0]) {

		default:
			/* Unknown command. */
			kgdb_send("");
			continue;

		case KGDB_SIGNAL:
			/*
			 * if this command came from a running gdb,
			 * answer it -- the other guy has no way of
			 * knowing if we're in or out of this loop
			 * when he issues a "remote-signal".
			 */
			snprintf(buffer, sizeof(buffer), "S%02x",
			    kgdb_signal(type));
			kgdb_send(buffer);
			continue;

		case KGDB_REG_R:
			mem2hex(buffer, gdb_regs, sizeof(gdb_regs));
			kgdb_send(buffer);
			continue;

		case KGDB_REG_W:
			p = hex2mem(gdb_regs, buffer + 1, sizeof(gdb_regs));
			if (p == NULL || *p != '\0')
				kgdb_send("E01");
			else {
				kgdb_setregs(regs, gdb_regs);
				kgdb_send("OK");
			}
			continue;

		case KGDB_MEM_R:
			p = buffer + 1;
			addr = hex2i(&p);
			if (*p++ != ',') {
				kgdb_send("E02");
				continue;
			}
			len = hex2i(&p);
			if (*p != '\0') {
				kgdb_send("E03");
				continue;
			}
			if (len > sizeof(buffer) / 2) {
				kgdb_send("E04");
				continue;
			}
			if (kgdb_acc(addr, len) == 0) {
				kgdb_send("E05");
				continue;
			}
			char *ptr = (char *)buffer + sizeof(buffer) / 2;
			db_read_bytes(addr, len, ptr);
			mem2hex(buffer, ptr, len);
			kgdb_send(buffer);
			continue;

		case KGDB_MEM_W:
			p = buffer + 1;
			addr = hex2i(&p);
			if (*p++ != ',') {
				kgdb_send("E06");
				continue;
			}
			len = hex2i(&p);
			if (*p++ != ':') {
				kgdb_send("E07");
				continue;
			}
			if (len > (sizeof(buffer) - (p - buffer))) {
				kgdb_send("E08");
				continue;
			}
			p = hex2mem(buffer, p, sizeof(buffer));
			if (p == NULL) {
				kgdb_send("E09");
				continue;
			}
			if (kgdb_acc(addr, len) == 0) {
				kgdb_send("E0A");
				continue;
			}
			db_write_bytes(addr, len, (char *)buffer);
			kgdb_send("OK");
			continue;

		case KGDB_DETACH:
		case KGDB_KILL:
			kgdb_active = 0;
			printf("kgdb detached\n");
			db_clear_single_step(regs);
			kgdb_send("OK");
			goto out;

		case KGDB_CONT:
			if (buffer[1]) {
				p = buffer + 1;
				addr = hex2i(&p);
				if (*p) {
					kgdb_send("E0B");
					continue;
				}
				PC_REGS(regs) = addr;
				DPRINTF(("kgdb: continuing at %08lx\n", addr));

			} else {
				DPRINTF((
				  "kgdb: continuing at old address %08lx\n",
				  (vaddr_t)PC_REGS(regs)));
			}

			db_clear_single_step(regs);
			goto out;

		case KGDB_STEP:
			if (buffer[1]) {
				p = buffer + 1;
				addr = hex2i(&p);
				if (*p) {
					kgdb_send("E0B");
					continue;
				}
				PC_REGS(regs) = addr;
			}
			db_set_single_step(regs);
			goto out;
		}
	}
 out:
	if (db_trap_callback)
		db_trap_callback(0);
	kgdb_recover = 0;
	return (1);
}

int
kgdb_disconnected(void)
{
	return 1;
}
