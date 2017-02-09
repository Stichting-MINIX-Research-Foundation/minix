/*	$NetBSD: tpm.c,v 1.11 2014/07/25 08:10:37 dholland Exp $	*/
/*
 * Copyright (c) 2008, 2009 Michael Shalayeff
 * Copyright (c) 2009, 2010 Hans-Jörg Höxer
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tpm.c,v 1.11 2014/07/25 08:10:37 dholland Exp $");

#if 0
#define	TPM_DEBUG 
#define aprint_debug_dev aprint_error_dev
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/pmf.h>

#include <dev/ic/tpmreg.h>
#include <dev/ic/tpmvar.h>

/* Set when enabling legacy interface in host bridge. */
int tpm_enabled;

const struct {
	uint32_t devid;
	char name[32];
	int flags;
#define TPM_DEV_NOINTS	0x0001
} tpm_devs[] = {
	{ 0x000615d1, "IFX SLD 9630 TT 1.1", 0 },
	{ 0x000b15d1, "IFX SLB 9635 TT 1.2", 0 },
	{ 0x100214e4, "Broadcom BCM0102", TPM_DEV_NOINTS },
	{ 0x00fe1050, "WEC WPCT200", 0 },
	{ 0x687119fa, "SNS SSX35", 0 },
	{ 0x2e4d5453, "STM ST19WP18", 0 },
	{ 0x32021114, "ATML 97SC3203", TPM_DEV_NOINTS },
	{ 0x10408086, "INTEL INTC0102", 0 },
	{ 0, "", TPM_DEV_NOINTS },
};

int tpm_tis12_irqinit(struct tpm_softc *, int, int);

int tpm_waitfor_poll(struct tpm_softc *, uint8_t, int, void *);
int tpm_waitfor_int(struct tpm_softc *, uint8_t, int, void *, int);
int tpm_waitfor(struct tpm_softc *, uint8_t, int, void *);
int tpm_request_locality(struct tpm_softc *, int);
int tpm_getburst(struct tpm_softc *);
uint8_t tpm_status(struct tpm_softc *);
int tpm_tmotohz(int);

static dev_type_open(tpmopen);
static dev_type_close(tpmclose); 
static dev_type_read(tpmread); 
static dev_type_read(tpmwrite); 
static dev_type_ioctl(tpmioctl);

extern struct cfdriver	tpm_cd;
#define TPMUNIT(a)	minor(a)
 
const struct cdevsw tpm_cdevsw = {
	.d_open = tpmopen,
	.d_close = tpmclose,
	.d_read = tpmread,
	.d_write = tpmwrite,
	.d_ioctl = tpmioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER,
}; 

/* Probe TPM using TIS 1.2 interface. */
int
tpm_tis12_probe(bus_space_tag_t bt, bus_space_handle_t bh)
{
	uint32_t r;
	uint8_t save, reg;

	r = bus_space_read_4(bt, bh, TPM_INTF_CAPABILITIES);
	if (r == 0xffffffff)
		return 0;

#ifdef TPM_DEBUG
	char buf[128];
	snprintb(buf, sizeof(buf), TPM_CAPBITS, r);
	printf("%s: caps=%s\n", __func__, buf);
#endif
	if ((r & TPM_CAPSREQ) != TPM_CAPSREQ ||
	    !(r & (TPM_INTF_INT_EDGE_RISING | TPM_INTF_INT_LEVEL_LOW))) {
#ifdef TPM_DEBUG
		printf("%s: caps too low (caps=%s)\n", __func__, buf);
#endif
		return 0;
	}

	save = bus_space_read_1(bt, bh, TPM_ACCESS);
	bus_space_write_1(bt, bh, TPM_ACCESS, TPM_ACCESS_REQUEST_USE);
	reg = bus_space_read_1(bt, bh, TPM_ACCESS);
	if ((reg & TPM_ACCESS_VALID) && (reg & TPM_ACCESS_ACTIVE_LOCALITY) &&
	    bus_space_read_4(bt, bh, TPM_ID) != 0xffffffff)
		return 1;

	bus_space_write_1(bt, bh, TPM_ACCESS, save);
	return 0;
}

/*
 * Setup interrupt vector if one is provided and interrupts are know to
 * work on that particular chip.
 */
int
tpm_tis12_irqinit(struct tpm_softc *sc, int irq, int idx)
{
	uint32_t r;

	if ((irq == -1) || (tpm_devs[idx].flags & TPM_DEV_NOINTS)) {
		sc->sc_vector = -1;
		return 0;
	}

	/* Ack and disable all interrupts. */
	r = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE,
	    r & ~TPM_GLOBAL_INT_ENABLE);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INT_STATUS,
	    bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INT_STATUS));
#ifdef TPM_DEBUG
	char buf[128];
	snprintb(buf, sizeof(buf), TPM_INTERRUPT_ENABLE_BITS, r);
	aprint_debug_dev(sc->sc_dev, "%s: before ien %s\n", __func__, buf);
#endif

	/* Program interrupt vector. */
	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_INT_VECTOR, irq);
	sc->sc_vector = irq;

	/* Program interrupt type. */
	r &= ~(TPM_INT_EDGE_RISING|TPM_INT_EDGE_FALLING|TPM_INT_LEVEL_HIGH|
	    TPM_INT_LEVEL_LOW);
	r |= TPM_GLOBAL_INT_ENABLE|TPM_CMD_READY_INT|TPM_LOCALITY_CHANGE_INT|
	    TPM_STS_VALID_INT|TPM_DATA_AVAIL_INT;
	if (sc->sc_capabilities & TPM_INTF_INT_EDGE_RISING)
		r |= TPM_INT_EDGE_RISING;
	else if (sc->sc_capabilities & TPM_INTF_INT_EDGE_FALLING)
		r |= TPM_INT_EDGE_FALLING;
	else if (sc->sc_capabilities & TPM_INTF_INT_LEVEL_HIGH)
		r |= TPM_INT_LEVEL_HIGH;
	else
		r |= TPM_INT_LEVEL_LOW;

	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE, r);
#ifdef TPM_DEBUG
	snprintb(buf, sizeof(buf), TPM_INTERRUPT_ENABLE_BITS, r);
	aprint_debug_dev(sc->sc_dev, "%s: after ien %s\n", __func__, buf);
#endif

	return 0;
}

/* Setup TPM using TIS 1.2 interface. */
int
tpm_tis12_init(struct tpm_softc *sc, int irq, const char *name)
{
	uint32_t r;
	int i;

	r = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTF_CAPABILITIES);
#ifdef TPM_DEBUG
	char cbuf[128];
	snprintb(cbuf, sizeof(cbuf), TPM_CAPBITS, r);
	aprint_debug_dev(sc->sc_dev, "%s: caps=%s ", __func__, cbuf);
#endif
	if ((r & TPM_CAPSREQ) != TPM_CAPSREQ ||
	    !(r & (TPM_INTF_INT_EDGE_RISING | TPM_INTF_INT_LEVEL_LOW))) {
		char buf[128];
		snprintb(buf, sizeof(buf), TPM_CAPBITS, r);
		aprint_error_dev(sc->sc_dev, "capabilities too low (caps=%s)\n",
		    buf);
		return 1;
	}
	sc->sc_capabilities = r;

	sc->sc_devid = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_ID);
	sc->sc_rev = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_REV);

	for (i = 0; tpm_devs[i].devid; i++)
		if (tpm_devs[i].devid == sc->sc_devid)
			break;

	if (tpm_devs[i].devid)
		aprint_normal(": %s rev 0x%x\n",
		    tpm_devs[i].name, sc->sc_rev);
	else
		aprint_normal(": device 0x%08x rev 0x%x\n",
		    sc->sc_devid, sc->sc_rev);

	if (tpm_tis12_irqinit(sc, irq, i))
		return 1;

	if (tpm_request_locality(sc, 0))
		return 1;

	/* Abort whatever it thought it was doing. */
	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS, TPM_STS_CMD_READY);

	return 0;
}

int
tpm_request_locality(struct tpm_softc *sc, int l)
{
	uint32_t r;
	int to, rv;

	if (l != 0)
		return EINVAL;

	if ((bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS) &
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) ==
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY))
		return 0;

	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS,
	    TPM_ACCESS_REQUEST_USE);

	to = tpm_tmotohz(TPM_ACCESS_TMO);

	while ((r = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_ACCESS) &
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) !=
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY) && to--) {
		rv = tsleep(sc->sc_init, PRIBIO | PCATCH, "tpm_locality", 1);
		if (rv &&  rv != EWOULDBLOCK) {
#ifdef TPM_DEBUG
			aprint_debug_dev(sc->sc_dev, "%s: interrupted %d\n",
			    __func__, rv);
#endif
			return rv;
		}
	}

	if ((r & (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) !=
	    (TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY)) {
#ifdef TPM_DEBUG
		char buf[128];
		snprintb(buf, sizeof(buf), TPM_ACCESS_BITS, r);
		aprint_debug_dev(sc->sc_dev, "%s: access %s\n", __func__, buf);
#endif
		return EBUSY;
	}

	return 0;
}

int
tpm_getburst(struct tpm_softc *sc)
{
	int burst, to, rv;

	to = tpm_tmotohz(TPM_BURST_TMO);

	burst = 0;
	while (burst == 0 && to--) {
		/*
		 * Burst count has to be read from bits 8 to 23 without
		 * touching any other bits, eg. the actual status bits 0
		 * to 7.
		 */
		burst = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_STS + 1);
		burst |= bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_STS + 2)
		    << 8;
#ifdef TPM_DEBUG
		aprint_debug_dev(sc->sc_dev, "%s: read %d\n", __func__, burst);
#endif
		if (burst)
			return burst;

		rv = tsleep(sc, PRIBIO | PCATCH, "tpm_getburst", 1);
		if (rv && rv != EWOULDBLOCK) {
			return 0;
		}
	}

	return 0;
}

uint8_t
tpm_status(struct tpm_softc *sc)
{
	return bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_STS) & TPM_STS_MASK;
}

int
tpm_tmotohz(int tmo)
{
	struct timeval tv;

	tv.tv_sec = tmo / 1000;
	tv.tv_usec = 1000 * (tmo % 1000);

	return tvtohz(&tv);
}

/* Save TPM state on suspend. */
bool
tpm_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct tpm_softc *sc = device_private(dev);
	static const uint8_t command[] = {
	    0, 193,		/* TPM_TAG_RQU_COMMAND */
	    0, 0, 0, 10,	/* Length in bytes */
	    0, 0, 0, 156	/* TPM_ORD_SaveStates */
	};
	uint8_t scratch[sizeof(command)];

	/*
	 * Power down:  We have to issue the SaveStates command.
	 */
	(*sc->sc_write)(sc, &command, sizeof(command));
	(*sc->sc_read)(sc, &scratch, sizeof(scratch), NULL, TPM_HDRSIZE);
#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev, "%s: power down\n", __func__);
#endif
	return true;
}

/*
 * Handle resume event.  Actually nothing to do as the BIOS is supposed
 * to restore the previously saved state.
 */
bool
tpm_resume(device_t dev, const pmf_qual_t *qual)
{
#ifdef TPM_DEBUG
	struct tpm_softc *sc = device_private(dev);
	aprint_debug_dev(sc->sc_dev, "%s: resume\n", __func__);
#endif
	return true;
}

/* Wait for given status bits using polling. */
int
tpm_waitfor_poll(struct tpm_softc *sc, uint8_t mask, int tmo, void *c)
{
	int rv;

	/*
	 * Poll until either the requested condition or a time out is
	 * met.
	 */
	while (((sc->sc_stat = tpm_status(sc)) & mask) != mask && tmo--) {
		rv = tsleep(c, PRIBIO | PCATCH, "tpm_poll", 1);
		if (rv && rv != EWOULDBLOCK) {
#ifdef TPM_DEBUG
			aprint_debug_dev(sc->sc_dev,
			    "%s: interrupted %d\n", __func__, rv);
#endif
			return rv;
		}
	}

	return 0;
}

/* Wait for given status bits using interrupts. */
int
tpm_waitfor_int(struct tpm_softc *sc, uint8_t mask, int tmo, void *c,
    int inttype)
{
	int rv, to;

	/* Poll and return when condition is already met. */
	sc->sc_stat = tpm_status(sc);
	if ((sc->sc_stat & mask) == mask)
		return 0;

	/*
	 * Enable interrupt on tpm chip.  Note that interrupts on our
	 * level (SPL_TTY) are disabled (see tpm{read,write} et al) and
	 * will not be delivered to the cpu until we call tsleep(9) below.
	 */
	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE,
	    bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE) |
	    inttype);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE,
	    bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE) |
	    TPM_GLOBAL_INT_ENABLE);

	/*
	 * Poll once more to remedy the race between previous polling
	 * and enabling interrupts on the tpm chip.
	 */
	sc->sc_stat = tpm_status(sc);
	if ((sc->sc_stat & mask) == mask) {
		rv = 0;
		goto out;
	}

	to = tpm_tmotohz(tmo);
#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev,
	    "%s: sleeping for %d ticks on %p\n", __func__, to, c);
#endif
	/*
	 * tsleep(9) enables interrupts on the cpu and returns after
	 * wake up with interrupts disabled again.  Note that interrupts
	 * generated by the tpm chip while being at SPL_TTY are not lost
	 * but held and delivered as soon as the cpu goes below SPL_TTY.
	 */
	rv = tsleep(c, PRIBIO | PCATCH, "tpm_wait", to);

	sc->sc_stat = tpm_status(sc);
#ifdef TPM_DEBUG
	char buf[128];
	snprintb(buf, sizeof(buf), TPM_STS_BITS, sc->sc_stat);
	aprint_debug_dev(sc->sc_dev,
	    "%s: woke up with rv %d stat %s\n", __func__, rv, buf);
#endif
	if ((sc->sc_stat & mask) == mask)
		rv = 0;

	/* Disable interrupts on tpm chip again. */
out:	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE,
	    bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE) &
	    ~TPM_GLOBAL_INT_ENABLE);
	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE,
	    bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INTERRUPT_ENABLE) &
	    ~inttype);

	return rv;
}

/*
 * Wait on given status bits, uses interrupts where possible, otherwise polls.
 */
int
tpm_waitfor(struct tpm_softc *sc, uint8_t b0, int tmo, void *c)
{
	uint8_t b;
	int re, to, rv;

#ifdef TPM_DEBUG
	char buf[128];
	snprintb(buf, sizeof(buf), TPM_STS_BITS, sc->sc_stat);
	aprint_debug_dev(sc->sc_dev, "%s: b0 %s\n", __func__, buf);
#endif

	/*
	 * If possible, use interrupts, otherwise poll.
	 *
	 * We use interrupts for TPM_STS_VALID and TPM_STS_DATA_AVAIL (if
	 * the tpm chips supports them) as waiting for those can take
	 * really long.  The other TPM_STS* are not needed very often
	 * so we do not support them.
	 */
	if (sc->sc_vector != -1) {
		b = b0;

		/*
		 * Wait for data ready.  This interrupt only occures
		 * when both TPM_STS_VALID and TPM_STS_DATA_AVAIL are asserted.
		 * Thus we don't have to bother with TPM_STS_VALID
		 * separately and can just return.
		 *
		 * This only holds for interrupts!  When using polling
		 * both flags have to be waited for, see below.
		 */
		if ((b & TPM_STS_DATA_AVAIL) && (sc->sc_capabilities &
		    TPM_INTF_DATA_AVAIL_INT))
			return tpm_waitfor_int(sc, b, tmo, c,
			    TPM_DATA_AVAIL_INT);

		/* Wait for status valid bit. */
		if ((b & TPM_STS_VALID) && (sc->sc_capabilities &
		    TPM_INTF_STS_VALID_INT)) {
			rv = tpm_waitfor_int(sc, b, tmo, c, TPM_STS_VALID_INT);
			if (rv != 0)
				return rv;
			else
				b = b0 & ~TPM_STS_VALID;
		}

		/*
		 * When all flags are taken care of, return.  Otherwise
		 * use polling for eg. TPM_STS_CMD_READY.
		 */
		if (b == 0)
			return 0;
	}

	re = 3;
restart:
	/*
	 * If requested wait for TPM_STS_VALID before dealing with
	 * any other flag.  Eg. when both TPM_STS_DATA_AVAIL and TPM_STS_VALID
	 * are requested, wait for the latter first.
	 */
	b = b0;
	if (b0 & TPM_STS_VALID)
		b = TPM_STS_VALID;

	to = tpm_tmotohz(tmo);
again:
	if ((rv = tpm_waitfor_poll(sc, b, to, c)) != 0)
		return rv;

	if ((b & sc->sc_stat) == TPM_STS_VALID) {
		/* Now wait for other flags. */
		b = b0 & ~TPM_STS_VALID;
		to++;
		goto again;
	}

	if ((sc->sc_stat & b) != b) {
#ifdef TPM_DEBUG
		char bbuf[128], cbuf[128];
		snprintb(bbuf, sizeof(bbuf), TPM_STS_BITS, b);
		snprintb(cbuf, sizeof(cbuf), TPM_STS_BITS, sc->sc_stat);
		aprint_debug_dev(sc->sc_dev,
		    "%s: timeout: stat=%s b=%s\n", __func__, cbuf, bbuf);
#endif
		if (re-- && (b0 & TPM_STS_VALID)) {
			bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS,
			    TPM_STS_RESP_RETRY);
			goto restart;
		}
		return EIO;
	}

	return 0;
}

/* Start transaction. */
int
tpm_tis12_start(struct tpm_softc *sc, int flag)
{
	int rv;

	if (flag == UIO_READ) {
		rv = tpm_waitfor(sc, TPM_STS_DATA_AVAIL | TPM_STS_VALID,
		    TPM_READ_TMO, sc->sc_read);
		return rv;
	}

	/* Own our (0th) locality. */
	if ((rv = tpm_request_locality(sc, 0)) != 0)
		return rv;

	sc->sc_stat = tpm_status(sc);
	if (sc->sc_stat & TPM_STS_CMD_READY) {
#ifdef TPM_DEBUG
		char buf[128];
		snprintb(buf, sizeof(buf), TPM_STS_BITS, sc->sc_stat);
		aprint_debug_dev(sc->sc_dev, "%s: UIO_WRITE status %s\n",
		    __func__, buf);
#endif
		return 0;
	}

#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev,
	    "%s: UIO_WRITE readying chip\n", __func__);
#endif

	/* Abort previous and restart. */
	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS, TPM_STS_CMD_READY);
	if ((rv = tpm_waitfor(sc, TPM_STS_CMD_READY, TPM_READY_TMO,
	    sc->sc_write))) {
#ifdef TPM_DEBUG
		aprint_debug_dev(sc->sc_dev,
		    "%s: UIO_WRITE readying failed %d\n", __func__, rv);
#endif
		return rv;
	}

#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev,
	    "%s: UIO_WRITE readying done\n", __func__);
#endif

	return 0;
}

int
tpm_tis12_read(struct tpm_softc *sc, void *buf, size_t len, size_t *count,
    int flags)
{
	uint8_t *p = buf;
	size_t cnt;
	int rv, n, bcnt;

#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev, "%s: len %zu\n", __func__, len);
#endif
	cnt = 0;
	while (len > 0) {
		if ((rv = tpm_waitfor(sc, TPM_STS_DATA_AVAIL | TPM_STS_VALID,
		    TPM_READ_TMO, sc->sc_read)))
			return rv;

		bcnt = tpm_getburst(sc);
		n = MIN(len, bcnt);
#ifdef TPM_DEBUG
		aprint_debug_dev(sc->sc_dev,
		    "%s: fetching %d, burst is %d\n", __func__, n, bcnt);
#endif
		for (; n--; len--) {
			*p++ = bus_space_read_1(sc->sc_bt, sc->sc_bh, TPM_DATA);
			cnt++;
		}

		if ((flags & TPM_PARAM_SIZE) == 0 && cnt >= 6)
			break;
	}
#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev,
	    "%s: read %zu bytes, len %zu\n", __func__, cnt, len);
#endif

	if (count)
		*count = cnt;

	return 0;
}

int
tpm_tis12_write(struct tpm_softc *sc, const void *buf, size_t len)
{
	const uint8_t *p = buf;
	size_t cnt;
	int rv, r;

#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev,
	    "%s: sc %p buf %p len %zu\n", __func__, sc, buf, len);
#endif
	if (len == 0)
		return 0;

	if ((rv = tpm_request_locality(sc, 0)) != 0)
		return rv;

	cnt = 0;
	while (cnt < len - 1) {
		for (r = tpm_getburst(sc); r > 0 && cnt < len - 1; r--) {
			bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_DATA, *p++);
			cnt++;
		}
		if ((rv = tpm_waitfor(sc, TPM_STS_VALID, TPM_READ_TMO, sc))) {
#ifdef TPM_DEBUG
			aprint_debug_dev(sc->sc_dev,
			    "%s: failed burst rv %d\n", __func__, rv);
#endif
			return rv;
		}
		sc->sc_stat = tpm_status(sc);
		if (!(sc->sc_stat & TPM_STS_DATA_EXPECT)) {
#ifdef TPM_DEBUG
			char sbuf[128];
			snprintb(sbuf, sizeof(sbuf), TPM_STS_BITS, sc->sc_stat);
			aprint_debug_dev(sc->sc_dev,
			    "%s: failed rv %d stat=%s\n", __func__, rv, sbuf);
#endif
			return EIO;
		}
	}

	bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_DATA, *p++);
	cnt++;

	if ((rv = tpm_waitfor(sc, TPM_STS_VALID, TPM_READ_TMO, sc))) {
#ifdef TPM_DEBUG
		aprint_debug_dev(sc->sc_dev, "%s: failed last byte rv %d\n",
		    __func__, rv);
#endif
		return rv;
	}
	if ((sc->sc_stat & TPM_STS_DATA_EXPECT) != 0) {
#ifdef TPM_DEBUG
		char sbuf[128];
		snprintb(sbuf, sizeof(sbuf), TPM_STS_BITS, sc->sc_stat);
		aprint_debug_dev(sc->sc_dev,
		    "%s: failed rv %d stat=%s\n", __func__, rv, sbuf);
#endif
		return EIO;
	}

#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev, "%s: wrote %zu byte\n", __func__, cnt);
#endif

	return 0;
}

/* Finish transaction. */
int
tpm_tis12_end(struct tpm_softc *sc, int flag, int err)
{
	int rv = 0;

	if (flag == UIO_READ) {
		if ((rv = tpm_waitfor(sc, TPM_STS_VALID, TPM_READ_TMO,
		    sc->sc_read)))
			return rv;

		/* Still more data? */
		sc->sc_stat = tpm_status(sc);
		if (!err && ((sc->sc_stat & TPM_STS_DATA_AVAIL)
		    == TPM_STS_DATA_AVAIL)) {
#ifdef TPM_DEBUG
			char buf[128];
			snprintb(buf, sizeof(buf), TPM_STS_BITS, sc->sc_stat);
			aprint_debug_dev(sc->sc_dev,
			    "%s: read failed stat=%s\n", __func__, buf);
#endif
			rv = EIO;
		}

		bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS,
		    TPM_STS_CMD_READY);

		/* Release our (0th) locality. */
		bus_space_write_1(sc->sc_bt, sc->sc_bh,TPM_ACCESS,
		    TPM_ACCESS_ACTIVE_LOCALITY);
	} else {
		/* Hungry for more? */
		sc->sc_stat = tpm_status(sc);
		if (!err && (sc->sc_stat & TPM_STS_DATA_EXPECT)) {
#ifdef TPM_DEBUG
			char buf[128];
			snprintb(buf, sizeof(buf), TPM_STS_BITS, sc->sc_stat);
			aprint_debug_dev(sc->sc_dev,
			    "%s: write failed stat=%s\n", __func__, buf);
#endif
			rv = EIO;
		}

		bus_space_write_1(sc->sc_bt, sc->sc_bh, TPM_STS,
		    err ? TPM_STS_CMD_READY : TPM_STS_GO);
	}

	return rv;
}

int
tpm_intr(void *v)
{
	struct tpm_softc *sc = v;
	uint32_t r;
#ifdef TPM_DEBUG
	static int cnt = 0;
#endif

	r = bus_space_read_4(sc->sc_bt, sc->sc_bh, TPM_INT_STATUS);
#ifdef TPM_DEBUG
	if (r != 0) {
		char buf[128];
		snprintb(buf, sizeof(buf), TPM_INTERRUPT_ENABLE_BITS, r);
		aprint_debug_dev(sc->sc_dev, "%s: int=%s (%d)\n", __func__,
		    buf, cnt);
	} else
		cnt++;
#endif
	if (!(r & (TPM_CMD_READY_INT | TPM_LOCALITY_CHANGE_INT |
	    TPM_STS_VALID_INT | TPM_DATA_AVAIL_INT)))
#ifdef __FreeBSD__
		return;
#else
		return 0;
#endif
	if (r & TPM_STS_VALID_INT)
		wakeup(sc);

	if (r & TPM_CMD_READY_INT)
		wakeup(sc->sc_write);

	if (r & TPM_DATA_AVAIL_INT)
		wakeup(sc->sc_read);

	if (r & TPM_LOCALITY_CHANGE_INT)
		wakeup(sc->sc_init);

	bus_space_write_4(sc->sc_bt, sc->sc_bh, TPM_INT_STATUS, r);

	return 1;
}

/* Read single byte using legacy interface. */
static inline uint8_t
tpm_legacy_in(bus_space_tag_t iot, bus_space_handle_t ioh, int reg)
{
	bus_space_write_1(iot, ioh, 0, reg);
	return bus_space_read_1(iot, ioh, 1);
}

/* Probe for TPM using legacy interface. */
int
tpm_legacy_probe(bus_space_tag_t iot, bus_addr_t iobase)
{
	bus_space_handle_t ioh;
	uint8_t r, v;
	int i, rv = 0;
	char id[8];

	if (!tpm_enabled || iobase == -1)
		return 0;

	if (bus_space_map(iot, iobase, 2, 0, &ioh))
		return 0;

	v = bus_space_read_1(iot, ioh, 0);
	if (v == 0xff) {
		bus_space_unmap(iot, ioh, 2);
		return 0;
	}
	r = bus_space_read_1(iot, ioh, 1);

	for (i = sizeof(id); i--; )
		id[i] = tpm_legacy_in(iot, ioh, TPM_ID + i);

#ifdef TPM_DEBUG
	printf("tpm_legacy_probe %.4s %d.%d.%d.%d\n",
	    &id[4], id[0], id[1], id[2], id[3]);
#endif
	/*
	 * The only chips using the legacy interface we are aware of are
	 * by Atmel.  For other chips more signature would have to be added.
	 */
	if (!bcmp(&id[4], "ATML", 4))
		rv = 1;

	if (!rv) {
		bus_space_write_1(iot, ioh, r, 1);
		bus_space_write_1(iot, ioh, v, 0);
	}
	bus_space_unmap(iot, ioh, 2);

	return rv;
}

/* Setup TPM using legacy interface. */
int
tpm_legacy_init(struct tpm_softc *sc, int irq, const char *name)
{
	char id[8];
	int i;

	if ((i = bus_space_map(sc->sc_batm, tpm_enabled, 2, 0, &sc->sc_bahm))) {
		aprint_debug_dev(sc->sc_dev, "cannot map tpm registers (%d)\n",
		    i);
		tpm_enabled = 0;
		return 1;
	}

	for (i = sizeof(id); i--; )
		id[i] = tpm_legacy_in(sc->sc_bt, sc->sc_bh, TPM_ID + i);

	aprint_debug_dev(sc->sc_dev, "%.4s %d.%d @0x%x\n", &id[4], id[0],
	    id[1], tpm_enabled);
	tpm_enabled = 0;

	return 0;
}

/* Start transaction. */
int
tpm_legacy_start(struct tpm_softc *sc, int flag)
{
	struct timeval tv;
	uint8_t bits, r;
	int to, rv;

	bits = flag == UIO_READ ? TPM_LEGACY_DA : 0;
	tv.tv_sec = TPM_LEGACY_TMO;
	tv.tv_usec = 0;
	to = tvtohz(&tv) / TPM_LEGACY_SLEEP;
	while (((r = bus_space_read_1(sc->sc_batm, sc->sc_bahm, 1)) &
	    (TPM_LEGACY_BUSY|bits)) != bits && to--) {
		rv = tsleep(sc, PRIBIO | PCATCH, "legacy_tpm_start",
		    TPM_LEGACY_SLEEP);
		if (rv && rv != EWOULDBLOCK)
			return rv;
	}

#if defined(TPM_DEBUG) && !defined(__FreeBSD__)
	char buf[128];
	snprintb(buf, sizeof(buf), TPM_LEGACY_BITS, r);
	aprint_debug_dev(sc->sc_dev, "%s: bits %s\n", device_xname(sc->sc_dev),
	    buf);
#endif
	if ((r & (TPM_LEGACY_BUSY|bits)) != bits)
		return EIO;

	return 0;
}

int
tpm_legacy_read(struct tpm_softc *sc, void *buf, size_t len, size_t *count,
    int flags)
{
	uint8_t *p;
	size_t cnt;
	int to, rv;

	cnt = rv = 0;
	for (p = buf; !rv && len > 0; len--) {
		for (to = 1000;
		    !(bus_space_read_1(sc->sc_batm, sc->sc_bahm, 1) &
		    TPM_LEGACY_DA); DELAY(1))
			if (!to--)
				return EIO;

		DELAY(TPM_LEGACY_DELAY);
		*p++ = bus_space_read_1(sc->sc_batm, sc->sc_bahm, 0);
		cnt++;
	}

	*count = cnt;
	return 0;
}

int
tpm_legacy_write(struct tpm_softc *sc, const void *buf, size_t len)
{
	const uint8_t *p;
	size_t n;

	for (p = buf, n = len; n--; DELAY(TPM_LEGACY_DELAY)) {
		if (!n && len != TPM_BUFSIZ) {
			bus_space_write_1(sc->sc_batm, sc->sc_bahm, 1,
			    TPM_LEGACY_LAST);
			DELAY(TPM_LEGACY_DELAY);
		}
		bus_space_write_1(sc->sc_batm, sc->sc_bahm, 0, *p++);
	}

	return 0;
}

/* Finish transaction. */
int
tpm_legacy_end(struct tpm_softc *sc, int flag, int rv)
{
	struct timeval tv;
	uint8_t r;
	int to;

	if (rv || flag == UIO_READ)
		bus_space_write_1(sc->sc_batm, sc->sc_bahm, 1, TPM_LEGACY_ABRT);
	else {
		tv.tv_sec = TPM_LEGACY_TMO;
		tv.tv_usec = 0;
		to = tvtohz(&tv) / TPM_LEGACY_SLEEP;
		while(((r = bus_space_read_1(sc->sc_batm, sc->sc_bahm, 1)) &
		    TPM_LEGACY_BUSY) && to--) {
			rv = tsleep(sc, PRIBIO | PCATCH, "legacy_tpm_end",
			    TPM_LEGACY_SLEEP);
			if (rv && rv != EWOULDBLOCK)
				return rv;
		}

#if defined(TPM_DEBUG) && !defined(__FreeBSD__)
		char buf[128];
		snprintb(buf, sizeof(buf), TPM_LEGACY_BITS, r);
		aprint_debug_dev(sc->sc_dev, "%s: bits %s\n",
		    device_xname(sc->sc_dev), buf);
#endif
		if (r & TPM_LEGACY_BUSY)
			return EIO;

		if (r & TPM_LEGACY_RE)
			return EIO;	/* XXX Retry the loop? */
	}

	return rv;
}

int
tpmopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct tpm_softc *sc = device_lookup_private(&tpm_cd, TPMUNIT(dev));

	if (!sc)
		return ENXIO;

	if (sc->sc_flags & TPM_OPEN)
		return EBUSY;

	sc->sc_flags |= TPM_OPEN;

	return 0;
}

int
tpmclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct tpm_softc *sc = device_lookup_private(&tpm_cd, TPMUNIT(dev));

	if (!sc)
		return ENXIO;

	if (!(sc->sc_flags & TPM_OPEN))
		return EINVAL;

	sc->sc_flags &= ~TPM_OPEN;

	return 0;
}

int
tpmread(dev_t dev, struct uio *uio, int flags)
{
	struct tpm_softc *sc = device_lookup_private(&tpm_cd, TPMUNIT(dev));
	uint8_t buf[TPM_BUFSIZ], *p;
	size_t cnt, len, n;
	int  rv, s;

	if (!sc)
		return ENXIO;

	s = spltty();
	if ((rv = (*sc->sc_start)(sc, UIO_READ)))
		goto out;

#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev, "%s: getting header\n", __func__);
#endif
	if ((rv = (*sc->sc_read)(sc, buf, TPM_HDRSIZE, &cnt, 0))) {
		(*sc->sc_end)(sc, UIO_READ, rv);
		goto out;
	}

	len = (buf[2] << 24) | (buf[3] << 16) | (buf[4] << 8) | buf[5];
#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev, "%s: len %zu, io count %zu\n", __func__,
	    len, uio->uio_resid);
#endif
	if (len > uio->uio_resid) {
		rv = EIO;
		(*sc->sc_end)(sc, UIO_READ, rv);
#ifdef TPM_DEBUG
		aprint_debug_dev(sc->sc_dev,
		    "%s: bad residual io count 0x%zx\n", __func__,
		    uio->uio_resid);
#endif
		goto out;
	}

	/* Copy out header. */
	if ((rv = uiomove(buf, cnt, uio))) {
#ifdef TPM_DEBUG
		aprint_debug_dev(sc->sc_dev,
		    "%s: uiomove failed %d\n", __func__, rv);
#endif
		(*sc->sc_end)(sc, UIO_READ, rv);
		goto out;
	}

	/* Get remaining part of the answer (if anything is left). */
	for (len -= cnt, p = buf, n = sizeof(buf); len > 0; p = buf, len -= n,
	    n = sizeof(buf)) {
		n = MIN(n, len);
#ifdef TPM_DEBUG
		aprint_debug_dev(sc->sc_dev, "%s: n %zu len %zu\n", __func__,
		    n, len);
#endif
		if ((rv = (*sc->sc_read)(sc, p, n, NULL, TPM_PARAM_SIZE))) {
			(*sc->sc_end)(sc, UIO_READ, rv);
			goto out;
		}
		p += n;
		if ((rv = uiomove(buf, p - buf, uio))) {
#ifdef TPM_DEBUG
			aprint_debug_dev(sc->sc_dev,
			    "%s: uiomove failed %d\n", __func__, rv);
#endif
			(*sc->sc_end)(sc, UIO_READ, rv);
			goto out;
		}
	}

	rv = (*sc->sc_end)(sc, UIO_READ, rv);
out:
	splx(s);
	return rv;
}

int
tpmwrite(dev_t dev, struct uio *uio, int flags)
{
	struct tpm_softc *sc = device_lookup_private(&tpm_cd, TPMUNIT(dev));
	uint8_t buf[TPM_BUFSIZ];
	int n, rv, s;

	if (!sc)
		return ENXIO;

	s = spltty();

#ifdef TPM_DEBUG
	aprint_debug_dev(sc->sc_dev, "%s: io count %zu\n", __func__,
	    uio->uio_resid);
#endif

	n = MIN(sizeof(buf), uio->uio_resid);
	if ((rv = uiomove(buf, n, uio))) {
#ifdef TPM_DEBUG
		aprint_debug_dev(sc->sc_dev,
		    "%s: uiomove failed %d\n", __func__, rv);
#endif
		splx(s);
		return rv;
	}

	if ((rv = (*sc->sc_start)(sc, UIO_WRITE))) {
		splx(s);
		return rv;
	}

	if ((rv = (*sc->sc_write)(sc, buf, n))) {
		splx(s);
		return rv;
	}

	rv = (*sc->sc_end)(sc, UIO_WRITE, rv);
	splx(s);
	return rv;
}

int
tpmioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	return ENOTTY;
}
