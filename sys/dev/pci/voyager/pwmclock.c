/*	$NetBSD: pwmclock.c,v 1.10 2013/05/14 09:19:36 macallan Exp $	*/

/*
 * Copyright (c) 2011 Michael Lorenz
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pwmclock.c,v 1.10 2013/05/14 09:19:36 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/cpu.h>
#include <sys/timetc.h>
#include <sys/sysctl.h>

#include <dev/pci/voyagervar.h>
#include <dev/ic/sm502reg.h>

#include <mips/mips3_clock.h>
#include <mips/locore.h>
#include <mips/bonito/bonitoreg.h>
#include <mips/bonito/bonitovar.h>

#include "opt_pwmclock.h"

#ifdef PWMCLOCK_DEBUG
#define DPRINTF aprint_error
#else
#define DPRINTF while (0) printf
#endif

int pwmclock_intr(void *);

struct pwmclock_softc {
	device_t sc_dev;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_regh;
	uint32_t sc_reg, sc_last;
	uint32_t sc_scale[8];
	uint32_t sc_count;	/* should probably be 64 bit */
	int sc_step;
	int sc_step_wanted;
	void *sc_shutdown_cookie;
};

static int	pwmclock_match(device_t, cfdata_t, void *);
static void	pwmclock_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(pwmclock, sizeof(struct pwmclock_softc),
    pwmclock_match, pwmclock_attach, NULL, NULL);

static void pwmclock_start(void);
static u_int get_pwmclock_timecount(struct timecounter *);

struct pwmclock_softc *pwmclock;
extern void (*initclocks_ptr)(void);
extern struct clockframe cf;

/* 0, 1/4, 3/8, 1/2, 5/8, 3/4, 7/8, 1 */
static int scale_m[] = {1, 1, 3, 1, 5, 3, 7, 1};
static int scale_d[] = {0, 4, 8, 2, 8, 4, 8, 1};

#define scale(x, f) (x * scale_d[f] / scale_m[f])

void pwmclock_set_speed(struct pwmclock_softc *, int);
static int  pwmclock_cpuspeed_temp(SYSCTLFN_ARGS);
static int  pwmclock_cpuspeed_cur(SYSCTLFN_ARGS);
static int  pwmclock_cpuspeed_available(SYSCTLFN_ARGS);

static void pwmclock_shutdown(void *);

static struct timecounter pwmclock_timecounter = {
	get_pwmclock_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffffff,		/* counter_mask */
	0,			/* frequency */
	"pwm",			/* name */
	100,			/* quality */
	NULL,			/* tc_priv */
	NULL			/* tc_next */
};

static int
pwmclock_match(device_t parent, cfdata_t match, void *aux)
{
	struct voyager_attach_args *vaa = (struct voyager_attach_args *)aux;

	if (strcmp(vaa->vaa_name, "pwmclock") == 0) return 100;
	return 0;
}

static uint32_t
pwmclock_wait_edge(struct pwmclock_softc *sc)
{
	/* clear interrupt */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PWM1, sc->sc_reg);
	while ((bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_PWM1) &
	    SM502_PWM_INTR_PENDING) == 0);
	return mips3_cp0_count_read();
}

static void
pwmclock_attach(device_t parent, device_t self, void *aux)
{
	struct pwmclock_softc *sc = device_private(self);
	struct voyager_attach_args *vaa = aux;
	const struct sysctlnode *sysctl_node, *me, *freq;
	uint32_t reg, last, curr, diff, acc;
	int i, clk;

	sc->sc_dev = self;
	sc->sc_memt = vaa->vaa_tag;
	sc->sc_regh = vaa->vaa_regh;

	aprint_normal("\n");

	voyager_establish_intr(parent, 22, pwmclock_intr, sc);
	reg = voyager_set_pwm(100, 100); /* 100Hz, 10% duty cycle */
	reg |= SM502_PWM_ENABLE | SM502_PWM_ENABLE_INTR |
	       SM502_PWM_INTR_PENDING;
	sc->sc_reg = reg;
	pwmclock = sc;
	initclocks_ptr = pwmclock_start;

	/*
	 * Establish a hook so on shutdown we can set the CPU clock back to
	 * full speed. This is necessary because PMON doesn't change the 
	 * clock scale register on a warm boot, the MIPS clock code gets
	 * confused if we're too slow and the loongson-specific bits run
	 * too late in the boot process
	 */
	sc->sc_shutdown_cookie = shutdownhook_establish(pwmclock_shutdown, sc);

	/* ok, let's see how far the cycle counter gets between interrupts */
	DPRINTF("calibrating CPU timer...\n");
	for (clk = 1; clk < 8; clk++) {

		REGVAL(LS2F_CHIPCFG0) =
		    (REGVAL(LS2F_CHIPCFG0) & ~LS2FCFG_FREQSCALE_MASK) | clk;
		bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PWM1,
		    sc->sc_reg);
		acc = 0;
		last = pwmclock_wait_edge(sc);
		for (i = 0; i < 16; i++) {
			curr = pwmclock_wait_edge(sc);
			diff = curr - last;
			acc += diff;
			last = curr;
		}
		sc->sc_scale[clk] = (acc >> 4) / 5000;
	}
#ifdef PWMCLOCK_DEBUG
	for (clk = 1; clk < 8; clk++) {
		aprint_normal_dev(sc->sc_dev, "%d/8: %d\n", clk + 1,
		    sc->sc_scale[clk]);
	}
#endif
	sc->sc_step = 7;
	sc->sc_step_wanted = 7;

	/* now setup sysctl */
	if (sysctl_createv(NULL, 0, NULL, 
	    &me, 
	    CTLFLAG_READWRITE, CTLTYPE_NODE, "loongson", NULL, NULL,
	    0, NULL, 0, CTL_MACHDEP, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(sc->sc_dev,
		    "couldn't create 'loongson' node\n");

	if (sysctl_createv(NULL, 0, NULL, 
	    &freq, 
	    CTLFLAG_READWRITE, CTLTYPE_NODE, "frequency", NULL, NULL, 0, NULL,
	    0, CTL_MACHDEP, me->sysctl_num, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(sc->sc_dev,
		    "couldn't create 'frequency' node\n");

	if (sysctl_createv(NULL, 0, NULL, 
	    &sysctl_node, 
	    CTLFLAG_READWRITE | CTLFLAG_OWNDESC,
	    CTLTYPE_INT, "target", "CPU speed", pwmclock_cpuspeed_temp, 
	    0, (void *)sc, 0, CTL_MACHDEP, me->sysctl_num, freq->sysctl_num, 
	    CTL_CREATE, CTL_EOL) == 0) {
	} else
		aprint_error_dev(sc->sc_dev,
		    "couldn't create 'target' node\n");

	if (sysctl_createv(NULL, 0, NULL, 
	    &sysctl_node, 
	    CTLFLAG_READWRITE,
	    CTLTYPE_INT, "current", NULL, pwmclock_cpuspeed_cur, 
	    1, (void *)sc, 0, CTL_MACHDEP, me->sysctl_num, freq->sysctl_num, 
	    CTL_CREATE, CTL_EOL) == 0) {
	} else
		aprint_error_dev(sc->sc_dev,
		    "couldn't create 'current' node\n");

	if (sysctl_createv(NULL, 0, NULL, 
	    &sysctl_node, 
	    CTLFLAG_READWRITE,
	    CTLTYPE_STRING, "available", NULL, pwmclock_cpuspeed_available, 
	    2, (void *)sc, 0, CTL_MACHDEP, me->sysctl_num, freq->sysctl_num, 
	    CTL_CREATE, CTL_EOL) == 0) {
	} else
		aprint_error_dev(sc->sc_dev,
		    "couldn't create 'available' node\n");
}

static void
pwmclock_shutdown(void *cookie)
{
	struct pwmclock_softc *sc = cookie;

	/* just in case the interrupt handler runs again after this */
	sc->sc_step_wanted = 7;
	/* set the clock to full speed */
	REGVAL(LS2F_CHIPCFG0) =
	    (REGVAL(LS2F_CHIPCFG0) & ~LS2FCFG_FREQSCALE_MASK) | 7;
}

void
pwmclock_set_speed(struct pwmclock_softc *sc, int speed)
{

	if ((speed < 1) || (speed > 7))
		return;
	sc->sc_step_wanted = speed;
	DPRINTF("%s: %d\n", __func__, speed);
}

/*
 * the PWM interrupt handler
 * we don't have a CPU clock independent, high resolution counter so we're
 * stuck with a PWM that can't count and a CP0 counter that slows down or
 * speeds up with the actual CPU speed. In order to still get halfway
 * accurate time we do the following:
 * - only change CPU speed in the timer interrupt
 * - each timer interrupt we measure how many CP0 cycles passed since last
 *   time, adjust for CPU speed since we can be sure it didn't change, use
 *   that to update a separate counter
 * - when reading the time counter we take the number of CP0 ticks since 
 *   the last timer interrupt, scale it to CPU clock, return that plus the
 *   interrupt updated counter mentioned above to get something close to
 *   CP0 running at full speed 
 * - when changing CPU speed do it as close to taking the time from CP0 as
 *   possible to keep the period of time we spend with CP0 running at the
 *   wrong frequency as short as possible - hopefully short enough to stay
 *   insignificant compared to other noise since switching speeds isn't
 *   going to happen all that often
 */

int
pwmclock_intr(void *cookie)
{
	struct pwmclock_softc *sc = cookie;
	uint32_t reg, now, diff;

	/* is it us? */
	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_PWM1);
	if ((reg & SM502_PWM_INTR_PENDING) == 0)
		return 0;

	/* yes, it's us, so clear the interrupt */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PWM1, sc->sc_reg);

	/*
	 * this looks kinda funny but what we want here is this:
	 * - reading the counter and changing the CPU clock should be as
	 *   close together as possible in order to remain halfway accurate
	 * - we need to use the previous sc_step in order to scale the
	 *   interval passed since the last clock interrupt correctly, so
	 *   we only change sc_step after doing that
	 */
	if (sc->sc_step_wanted != sc->sc_step) {

		REGVAL(LS2F_CHIPCFG0) =
		    (REGVAL(LS2F_CHIPCFG0) & ~LS2FCFG_FREQSCALE_MASK) |
		     sc->sc_step_wanted;
	}

	now = mips3_cp0_count_read();		
	diff = now - sc->sc_last;
	sc->sc_count += scale(diff, sc->sc_step);
	sc->sc_last = now;
	if (sc->sc_step_wanted != sc->sc_step) {
		sc->sc_step = sc->sc_step_wanted;
	}
		 
	hardclock(&cf);

	return 1;
}

static void
pwmclock_start(void)
{
	struct pwmclock_softc *sc = pwmclock;
	sc->sc_count = 0;
	sc->sc_last = mips3_cp0_count_read();
	pwmclock_timecounter.tc_frequency = curcpu()->ci_cpu_freq / 2;
	tc_init(&pwmclock_timecounter);
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_PWM1, sc->sc_reg);
}

static u_int
get_pwmclock_timecount(struct timecounter *tc)
{
	struct pwmclock_softc *sc = pwmclock;
	uint32_t now, diff;

	now = mips3_cp0_count_read();
	diff = now - sc->sc_last;
	return sc->sc_count + scale(diff, sc->sc_step);
}

static int
pwmclock_cpuspeed_temp(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	struct pwmclock_softc *sc = node.sysctl_data;
	int mhz, i;

	mhz = sc->sc_scale[sc->sc_step_wanted];

	node.sysctl_data = &mhz;
	if (sysctl_lookup(SYSCTLFN_CALL(&node)) == 0) {
		int new_reg;

		new_reg = *(int *)node.sysctl_data;
		i = 1;
		while ((i < 8) && (sc->sc_scale[i] != new_reg))
			i++;
		if (i > 7)
			return EINVAL;
		pwmclock_set_speed(sc, i);
		return 0;
	}
	return EINVAL;
}

static int
pwmclock_cpuspeed_cur(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	struct pwmclock_softc *sc = node.sysctl_data;
	int mhz;

	mhz = sc->sc_scale[sc->sc_step];
	node.sysctl_data = &mhz;
	return sysctl_lookup(SYSCTLFN_CALL(&node));
}

static int
pwmclock_cpuspeed_available(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	struct pwmclock_softc *sc = node.sysctl_data;
	char buf[128];

	snprintf(buf, 128, "%d %d %d %d %d %d %d", sc->sc_scale[1],
	    sc->sc_scale[2], sc->sc_scale[3], sc->sc_scale[4],
	    sc->sc_scale[5], sc->sc_scale[6], sc->sc_scale[7]);
	node.sysctl_data = buf;
	return(sysctl_lookup(SYSCTLFN_CALL(&node)));
}

SYSCTL_SETUP(sysctl_ams_setup, "sysctl obio subtree setup")
{

	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "machdep", NULL,
		       NULL, 0, NULL, 0,
		       CTL_MACHDEP, CTL_EOL);
}
