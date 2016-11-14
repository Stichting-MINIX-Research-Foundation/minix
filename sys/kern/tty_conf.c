/*	$NetBSD: tty_conf.c,v 1.56 2014/05/22 16:28:06 dholland Exp $	*/

/*-
 * Copyright (c) 2005, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)tty_conf.c	8.5 (Berkeley) 1/9/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tty_conf.c,v 1.56 2014/05/22 16:28:06 dholland Exp $");

#define TTY_ALLOW_PRIVATE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/ttycom.h>
#include <sys/conf.h>
#include <sys/mutex.h>
#include <sys/queue.h>

static struct linesw termios_disc = {
	.l_name = "termios",
	.l_open = ttylopen,
	.l_close = ttylclose,
	.l_read = ttread,
	.l_write = ttwrite,
	.l_ioctl = ttynullioctl,
	.l_rint = ttyinput,
	.l_start = ttstart,
	.l_modem = ttymodem,
	.l_poll = ttpoll
};

/*
 * This is for the benefit of old BSD TTY compatbility, but since it is
 * identical to termios (except for the name), don't bother conditionalizing
 * it.
 */
static struct linesw ntty_disc = {	/* old NTTYDISC */
	.l_name = "ntty",
	.l_open = ttylopen,
	.l_close = ttylclose,
	.l_read = ttread,
	.l_write = ttwrite,
	.l_ioctl = ttynullioctl,
	.l_rint = ttyinput,
	.l_start = ttstart,
	.l_modem = ttymodem,
	.l_poll = ttpoll
};

static LIST_HEAD(, linesw) ttyldisc_list = LIST_HEAD_INITIALIZER(ttyldisc_head);

/*
 * Note: We don't bother refcounting termios_disc and ntty_disc; they can't
 * be removed from the list, and termios_disc is likely to have very many
 * references (could we overflow the count?).
 */
#define	TTYLDISC_ISSTATIC(disc)					\
	((disc) == &termios_disc || (disc) == &ntty_disc)

#define	TTYLDISC_HOLD(disc)					\
do {								\
	if (! TTYLDISC_ISSTATIC(disc)) {			\
		KASSERT((disc)->l_refcnt != UINT_MAX);		\
		(disc)->l_refcnt++;				\
	}							\
} while (/*CONSTCOND*/0)

#define	TTYLDISC_RELE(disc)					\
do {								\
	if (! TTYLDISC_ISSTATIC(disc)) {			\
		KASSERT((disc)->l_refcnt != 0);			\
		(disc)->l_refcnt--;				\
	}							\
} while (/*CONSTCOND*/0)

#define	TTYLDISC_ISINUSE(disc)					\
	(TTYLDISC_ISSTATIC(disc) || (disc)->l_refcnt != 0)

/*
 * Do nothing specific version of line
 * discipline specific ioctl command.
 */
/*ARGSUSED*/
int
ttynullioctl(struct tty *tp, u_long cmd, void *data, int flags, struct lwp *l)
{

	return (EPASSTHROUGH);
}

/*
 * Return error to line discipline
 * specific poll call.
 */
/*ARGSUSED*/
int
ttyerrpoll(struct tty *tp, int events, struct lwp *l)
{

	return (POLLERR);
}

void
ttyldisc_init(void)
{

	if (ttyldisc_attach(&termios_disc) != 0)
		panic("ttyldisc_init: termios_disc");
	if (ttyldisc_attach(&ntty_disc) != 0)
		panic("ttyldisc_init: ntty_disc");
}

static struct linesw *
ttyldisc_lookup_locked(const char *name)
{
	struct linesw *disc;

	LIST_FOREACH(disc, &ttyldisc_list, l_list) {
		if (strcmp(name, disc->l_name) == 0)
			return (disc);
	}

	return (NULL);
}

/*
 * Look up a line discipline by its name.  Caller holds a reference on
 * the returned line discipline.
 */
struct linesw *
ttyldisc_lookup(const char *name)
{
	struct linesw *disc;

	mutex_spin_enter(&tty_lock);
	disc = ttyldisc_lookup_locked(name);
	if (disc != NULL)
		TTYLDISC_HOLD(disc);
	mutex_spin_exit(&tty_lock);

	return (disc);
}

/*
 * Look up a line discipline by its legacy number.  Caller holds a
 * reference on the returned line discipline.
 */
struct linesw *
ttyldisc_lookup_bynum(int num)
{
	struct linesw *disc;

	mutex_spin_enter(&tty_lock);

	LIST_FOREACH(disc, &ttyldisc_list, l_list) {
		if (disc->l_no == num) {
			TTYLDISC_HOLD(disc);
			mutex_spin_exit(&tty_lock);
			return (disc);
		}
	}

	mutex_spin_exit(&tty_lock);
	return (NULL);
}

/*
 * Release a reference on a line discipline previously added by
 * ttyldisc_lookup() or ttyldisc_lookup_bynum().
 */
void
ttyldisc_release(struct linesw *disc)
{

	if (disc == NULL)
		return;

	mutex_spin_enter(&tty_lock);
	TTYLDISC_RELE(disc);
	mutex_spin_exit(&tty_lock);
}

#define	TTYLDISC_LEGACY_NUMBER_MIN	10
#define	TTYLDISC_LEGACY_NUMBER_MAX	INT_MAX

static void
ttyldisc_assign_legacy_number(struct linesw *disc)
{
	static const struct {
		const char *name;
		int num;
	} table[] = {
		{ "termios",		TTYDISC },
		{ "ntty",		2 /* XXX old NTTYDISC */ },
		{ "tablet",		TABLDISC },
		{ "slip",		SLIPDISC },
		{ "ppp",		PPPDISC },
		{ "strip",		STRIPDISC },
		{ "hdlc",		HDLCDISC },
		{ NULL,			0 }
	};
	struct linesw *ldisc;
	int i;

	for (i = 0; table[i].name != NULL; i++) {
		if (strcmp(disc->l_name, table[i].name) == 0) {
			disc->l_no = table[i].num;
			return;
		}
	}

	disc->l_no = TTYLDISC_LEGACY_NUMBER_MIN;

	LIST_FOREACH(ldisc, &ttyldisc_list, l_list) {
		if (disc->l_no == ldisc->l_no) {
			KASSERT(disc->l_no < TTYLDISC_LEGACY_NUMBER_MAX);
			disc->l_no++;
		}
	}
}

/*
 * Register a line discipline.
 */
int
ttyldisc_attach(struct linesw *disc)
{

	KASSERT(disc->l_name != NULL);
	KASSERT(disc->l_open != NULL);
	KASSERT(disc->l_close != NULL);
	KASSERT(disc->l_read != NULL);
	KASSERT(disc->l_write != NULL);
	KASSERT(disc->l_ioctl != NULL);
	KASSERT(disc->l_rint != NULL);
	KASSERT(disc->l_start != NULL);
	KASSERT(disc->l_modem != NULL);
	KASSERT(disc->l_poll != NULL);

	/* You are not allowed to exceed TTLINEDNAMELEN */
	if (strlen(disc->l_name) >= TTLINEDNAMELEN)
		return (ENAMETOOLONG);

	mutex_spin_enter(&tty_lock);

	if (ttyldisc_lookup_locked(disc->l_name) != NULL) {
		mutex_spin_exit(&tty_lock);
		return (EEXIST);
	}

	ttyldisc_assign_legacy_number(disc);
	LIST_INSERT_HEAD(&ttyldisc_list, disc, l_list);

	mutex_spin_exit(&tty_lock);

	return (0);
}

/*
 * Remove a line discipline.
 */
int
ttyldisc_detach(struct linesw *disc)
{
#ifdef DIAGNOSTIC
	struct linesw *ldisc = ttyldisc_lookup(disc->l_name);

	KASSERT(ldisc != NULL);
	KASSERT(ldisc == disc);
	ttyldisc_release(ldisc);
#endif

	mutex_spin_enter(&tty_lock);

	if (TTYLDISC_ISINUSE(disc)) {
		mutex_spin_exit(&tty_lock);
		return (EBUSY);
	}

	LIST_REMOVE(disc, l_list);

	mutex_spin_exit(&tty_lock);

	return (0);
}

/*
 * Return the default line discipline.
 */
struct linesw *
ttyldisc_default(void)
{

	return (&termios_disc);
}
