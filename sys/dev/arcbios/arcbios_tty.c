/*	$NetBSD: arcbios_tty.c,v 1.25 2014/07/25 08:10:36 dholland Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: arcbios_tty.c,v 1.25 2014/07/25 08:10:36 dholland Exp $");

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/termios.h>
#include <sys/kauth.h>

#include <dev/cons.h>

#include <dev/arcbios/arcbios.h>
#include <dev/arcbios/arcbiosvar.h>

callout_t  arcbios_tty_ch;
bool arcbios_ch_init;
static struct tty *arcbios_tty[1];

void	arcbios_tty_start(struct tty *);
void	arcbios_tty_poll(void *);
int	arcbios_tty_param(struct tty *, struct termios *);

dev_type_open(arcbios_ttyopen);
dev_type_close(arcbios_ttyclose);
dev_type_read(arcbios_ttyread);
dev_type_write(arcbios_ttywrite);
dev_type_ioctl(arcbios_ttyioctl);
dev_type_stop(arcbios_ttystop);
dev_type_tty(arcbios_ttytty);
dev_type_poll(arcbios_ttypoll);

const struct cdevsw arcbios_cdevsw = {
	.d_open = arcbios_ttyopen,
	.d_close = arcbios_ttyclose,
	.d_read = arcbios_ttyread,
	.d_write = arcbios_ttywrite,
	.d_ioctl = arcbios_ttyioctl,
	.d_stop = arcbios_ttystop,
	.d_tty = arcbios_ttytty,
	.d_poll = arcbios_ttypoll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY,
};

int
arcbios_ttyopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	int unit = minor(dev);
	struct tty *tp;
	int s, error = 0, setuptimeout = 0;

	if (!arcbios_ch_init) {
		arcbios_ch_init = true;
		callout_init(&arcbios_tty_ch, 0);
	}

	if (unit != 0)
		return (ENODEV);

	s = spltty();

	if (arcbios_tty[unit] == NULL) {
		tp = arcbios_tty[unit] = tty_alloc();
		tty_attach(tp);
	} else
		tp = arcbios_tty[unit];

	tp->t_oproc = arcbios_tty_start;
	tp->t_param = arcbios_tty_param;
	tp->t_dev = dev;

	if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN, tp)) {
		splx(s);
		return (EBUSY);
	}

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG | CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = 9600;
		ttsetwater(tp);

		setuptimeout = 1;
	}

	splx(s);

	error = (*tp->t_linesw->l_open)(dev, tp);
	if (error == 0 && setuptimeout)
		callout_reset(&arcbios_tty_ch, 1, arcbios_tty_poll, tp);

	return (error);
}

int
arcbios_ttyclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	int unit = minor(dev);
	struct tty *tp = arcbios_tty[unit];

	callout_stop(&arcbios_tty_ch);
	(*tp->t_linesw->l_close)(tp, flag);
	ttyclose(tp);
	return (0);
}

int
arcbios_ttyread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = arcbios_tty[minor(dev)];

	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}

int
arcbios_ttywrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = arcbios_tty[minor(dev)];

	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

int
arcbios_ttypoll(dev_t dev, int events, struct lwp *l)
{
	struct tty *tp = arcbios_tty[minor(dev)];

	return ((*tp->t_linesw->l_poll)(tp, events, l));
}

int
arcbios_ttyioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	int unit = minor(dev);
	struct tty *tp = arcbios_tty[unit];
	int error;

	error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);
	return (ttioctl(tp, cmd, data, flag, l));
}

int
arcbios_tty_param(struct tty *tp, struct termios *t)
{

	return (0);
}

void
arcbios_tty_start(struct tty *tp)
{
	u_long count;
	int s;

	s = spltty();
	if (tp->t_state & (TS_TTSTOP | TS_BUSY))
		goto out;
	ttypull(tp);
	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0) {
		arcbios_Write(ARCBIOS_STDOUT, tp->t_outq.c_cf,
		    ndqb(&tp->t_outq, 0), &count);
		ndflush(&tp->t_outq, count);
	}
	tp->t_state &= ~TS_BUSY;
 out:
	splx(s);
}

void
arcbios_ttystop(struct tty *tp, int flag)
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
}

static int
arcbios_tty_getchar(int *cp)
{
	char c;
	int32_t q;
	u_long count;

	q = arcbios_GetReadStatus(ARCBIOS_STDIN);

	if (q == 0) {
		arcbios_Read(ARCBIOS_STDIN, &c, 1, &count);
		*cp = c;

		return 1;
	}

	return 0;
}

void
arcbios_tty_poll(void *v)
{
	struct tty *tp = v;
	int c;

	while (arcbios_tty_getchar(&c)) {
		if (tp->t_state & TS_ISOPEN)
			(void)(*tp->t_linesw->l_rint)(c, tp);
	}
	callout_reset(&arcbios_tty_ch, 1, arcbios_tty_poll, tp);
}

struct tty *
arcbios_ttytty(dev_t dev)
{

	if (minor(dev) != 0)
		panic("arcbios_ttytty: bogus");

	return (arcbios_tty[0]);
}
