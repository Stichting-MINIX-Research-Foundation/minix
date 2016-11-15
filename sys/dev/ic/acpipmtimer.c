/* $NetBSD: acpipmtimer.c,v 1.8 2009/08/18 17:47:46 dyoung Exp $ */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpipmtimer.c,v 1.8 2009/08/18 17:47:46 dyoung Exp $");

#include <sys/types.h>

#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <dev/ic/acpipmtimer.h>

#define ACPI_PM_TIMER_FREQUENCY 3579545

struct hwtc {
	struct timecounter tc;
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t off;
};

static u_int acpihwtimer_read_safe(struct timecounter *);
static u_int acpihwtimer_read_fast(struct timecounter *);

acpipmtimer_t
acpipmtimer_attach(device_t dev,
		   bus_space_tag_t t, bus_space_handle_t h, bus_size_t off,
		   int flags)
{
	struct hwtc *tc;

	tc = malloc(sizeof(struct hwtc), M_DEVBUF, M_WAITOK|M_ZERO);
	if (tc == NULL)
		return NULL;

	tc->tc.tc_name = device_xname(dev);
	tc->tc.tc_frequency = ACPI_PM_TIMER_FREQUENCY;
	if (flags & ACPIPMT_32BIT)
		tc->tc.tc_counter_mask = 0xffffffff;
	else
		tc->tc.tc_counter_mask = 0x00ffffff;
	if (flags & ACPIPMT_BADLATCH) {
		tc->tc.tc_get_timecount = acpihwtimer_read_safe;
		tc->tc.tc_quality = 900;
	} else {
		tc->tc.tc_get_timecount = acpihwtimer_read_fast;
		tc->tc.tc_quality = 1000;
	}

	tc->t = t;
	tc->h = h;
	tc->off = off;

	tc->tc.tc_priv = tc;
	tc_init(&tc->tc);
	aprint_normal("%s: %d-bit timer\n", tc->tc.tc_name,
		      (flags & ACPIPMT_32BIT ? 32 : 24));
	return tc;
}

int
acpipmtimer_detach(acpipmtimer_t timer, int flags)
{
	struct hwtc *tc = timer;

	return tc_detach(&tc->tc);
}

#define r(h) bus_space_read_4(h->t, h->h, h->off)

static u_int
acpihwtimer_read_safe(struct timecounter *tc)
{
	struct hwtc *h = tc->tc_priv;
	uint32_t t1, t2, t3;

	t2 = r(h);
	t3 = r(h);
	do {
		t1 = t2;
		t2 = t3;
		t3 = r(h);
	} while ((t1 > t2) || (t2 > t3));
	return (t2);
}

static u_int
acpihwtimer_read_fast(struct timecounter *tc)
{
	struct hwtc *h = tc->tc_priv;

	return r(h);
}
