/*	$NetBSD: spkr.c,v 1.36 2015/05/17 05:20:37 pgoyette Exp $	*/

/*
 * Copyright (c) 1990 Eric S. Raymond (esr@snark.thyrsus.com)
 * Copyright (c) 1990 Andrew A. Chernov (ache@astral.msk.su)
 * Copyright (c) 1990 Lennart Augustsson (lennart@augustsson.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Eric S. Raymond
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * spkr.c -- device driver for console speaker on 80386
 *
 * v1.1 by Eric S. Raymond (esr@snark.thyrsus.com) Feb 1990
 *      modified for 386bsd by Andrew A. Chernov <ache@astral.msk.su>
 *      386bsd only clean version, all SYSV stuff removed
 *      use hz value from param.c
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: spkr.c,v 1.36 2015/05/17 05:20:37 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/conf.h>

#include <sys/bus.h>

#include <dev/isa/pcppivar.h>

#include <dev/isa/spkrio.h>

int spkrprobe(device_t, cfdata_t, void *);
void spkrattach(device_t, device_t, void *);
int spkrdetach(device_t, int);

#include "ioconf.h"

MODULE(MODULE_CLASS_DRIVER, spkr, NULL /* "pcppi" */);

#ifdef _MODULE
#include "ioconf.c"
#endif


CFATTACH_DECL_NEW(spkr, 0,
    spkrprobe, spkrattach, spkrdetach, NULL);

dev_type_open(spkropen);
dev_type_close(spkrclose);
dev_type_write(spkrwrite);
dev_type_ioctl(spkrioctl);

const struct cdevsw spkr_cdevsw = {
	.d_open = spkropen,
	.d_close = spkrclose,
	.d_read = noread,
	.d_write = spkrwrite,
	.d_ioctl = spkrioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

static pcppi_tag_t ppicookie;

#define SPKRPRI (PZERO - 1)

static void tone(u_int, u_int);
static void rest(int);
static void playinit(void);
static void playtone(int, int, int);
static void playstring(char *, int);

static void
tone(u_int xhz, u_int ticks)
/* emit tone of frequency hz for given number of ticks */
{
	pcppi_bell(ppicookie, xhz, ticks, PCPPI_BELL_SLEEP);
}

static void
rest(int ticks)
/* rest for given number of ticks */
{
    /*
     * Set timeout to endrest function, then give up the timeslice.
     * This is so other processes can execute while the rest is being
     * waited out.
     */
#ifdef SPKRDEBUG
    printf("rest: %d\n", ticks);
#endif /* SPKRDEBUG */
    if (ticks > 0)
	    tsleep(rest, SPKRPRI | PCATCH, "rest", ticks);
}

/**************** PLAY STRING INTERPRETER BEGINS HERE **********************
 *
 * Play string interpretation is modelled on IBM BASIC 2.0's PLAY statement;
 * M[LNS] are missing and the ~ synonym and octave-tracking facility is added.
 * Requires tone(), rest(), and endtone(). String play is not interruptible
 * except possibly at physical block boundaries.
 */

#define dtoi(c)		((c) - '0')

static int octave;	/* currently selected octave */
static int whole;	/* whole-note time at current tempo, in ticks */
static int value;	/* whole divisor for note time, quarter note = 1 */
static int fill;	/* controls spacing of notes */
static bool octtrack;	/* octave-tracking on? */
static bool octprefix;	/* override current octave-tracking state? */

/*
 * Magic number avoidance...
 */
#define SECS_PER_MIN	60	/* seconds per minute */
#define WHOLE_NOTE	4	/* quarter notes per whole note */
#define MIN_VALUE	64	/* the most we can divide a note by */
#define DFLT_VALUE	4	/* default value (quarter-note) */
#define FILLTIME	8	/* for articulation, break note in parts */
#define STACCATO	6	/* 6/8 = 3/4 of note is filled */
#define NORMAL		7	/* 7/8ths of note interval is filled */
#define LEGATO		8	/* all of note interval is filled */
#define DFLT_OCTAVE	4	/* default octave */
#define MIN_TEMPO	32	/* minimum tempo */
#define DFLT_TEMPO	120	/* default tempo */
#define MAX_TEMPO	255	/* max tempo */
#define NUM_MULT	3	/* numerator of dot multiplier */
#define DENOM_MULT	2	/* denominator of dot multiplier */

/* letter to half-tone:  A   B  C  D  E  F  G */
static const int notetab[8] = {9, 11, 0, 2, 4, 5, 7};

/*
 * This is the American Standard A440 Equal-Tempered scale with frequencies
 * rounded to nearest integer. Thank Goddess for the good ol' CRC Handbook...
 * our octave 0 is standard octave 2.
 */
#define OCTAVE_NOTES	12	/* semitones per octave */
static const int pitchtab[] =
{
/*        C     C#    D     D#    E     F     F#    G     G#    A     A#    B*/
/* 0 */   65,   69,   73,   78,   82,   87,   93,   98,  103,  110,  117,  123,
/* 1 */  131,  139,  147,  156,  165,  175,  185,  196,  208,  220,  233,  247,
/* 2 */  262,  277,  294,  311,  330,  349,  370,  392,  415,  440,  466,  494,
/* 3 */  523,  554,  587,  622,  659,  698,  740,  784,  831,  880,  932,  988,
/* 4 */ 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1975,
/* 5 */ 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951,
/* 6 */ 4186, 4435, 4698, 4978, 5274, 5588, 5920, 6272, 6644, 7040, 7459, 7902,
};
#define NOCTAVES (int)(__arraycount(pitchtab) / OCTAVE_NOTES)

static void
playinit(void)
{
    octave = DFLT_OCTAVE;
    whole = (hz * SECS_PER_MIN * WHOLE_NOTE) / DFLT_TEMPO;
    fill = NORMAL;
    value = DFLT_VALUE;
    octtrack = false;
    octprefix = true;	/* act as though there was an initial O(n) */
}

static void
playtone(int pitch, int val, int sustain)
/* play tone of proper duration for current rhythm signature */
{
    int	sound, silence, snum = 1, sdenom = 1;

    /* this weirdness avoids floating-point arithmetic */
    for (; sustain; sustain--)
    {
	snum *= NUM_MULT;
	sdenom *= DENOM_MULT;
    }

    if (pitch == -1)
	rest(whole * snum / (val * sdenom));
    else
    {
	sound = (whole * snum) / (val * sdenom)
		- (whole * (FILLTIME - fill)) / (val * FILLTIME);
	silence = whole * (FILLTIME-fill) * snum / (FILLTIME * val * sdenom);

#ifdef SPKRDEBUG
	printf("playtone: pitch %d for %d ticks, rest for %d ticks\n",
	    pitch, sound, silence);
#endif /* SPKRDEBUG */

	tone(pitchtab[pitch], sound);
	if (fill != LEGATO)
	    rest(silence);
    }
}

static void
playstring(char *cp, int slen)
/* interpret and play an item from a notation string */
{
    int		pitch, lastpitch = OCTAVE_NOTES * DFLT_OCTAVE;

#define GETNUM(cp, v)	for(v=0; slen > 0 && isdigit(cp[1]); ) \
				{v = v * 10 + (*++cp - '0'); slen--;}
    for (; slen--; cp++)
    {
	int		sustain, timeval, tempo;
	char	c = toupper(*cp);

#ifdef SPKRDEBUG
	printf("playstring: %c (%x)\n", c, c);
#endif /* SPKRDEBUG */

	switch (c)
	{
	case 'A':  case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':

	    /* compute pitch */
	    pitch = notetab[c - 'A'] + octave * OCTAVE_NOTES;

	    /* this may be followed by an accidental sign */
	    if (slen > 0 && (cp[1] == '#' || cp[1] == '+'))
	    {
		++pitch;
		++cp;
		slen--;
	    }
	    else if (slen > 0 && cp[1] == '-')
	    {
		--pitch;
		++cp;
		slen--;
	    }

	    /*
	     * If octave-tracking mode is on, and there has been no octave-
	     * setting prefix, find the version of the current letter note
	     * closest to the last regardless of octave.
	     */
	    if (octtrack && !octprefix)
	    {
		if (abs(pitch-lastpitch) > abs(pitch+OCTAVE_NOTES-lastpitch))
		{
		    if (octave < NOCTAVES - 1) {
			++octave;
			pitch += OCTAVE_NOTES;
		    }
		}

		if (abs(pitch-lastpitch) > abs((pitch-OCTAVE_NOTES)-lastpitch))
		{
		    if (octave > 0) {
			--octave;
			pitch -= OCTAVE_NOTES;
		    }
		}
	    }
	    octprefix = false;
	    lastpitch = pitch;

	    /* ...which may in turn be followed by an override time value */
	    GETNUM(cp, timeval);
	    if (timeval <= 0 || timeval > MIN_VALUE)
		timeval = value;

	    /* ...and/or sustain dots */
	    for (sustain = 0; slen > 0 && cp[1] == '.'; cp++)
	    {
		slen--;
		sustain++;
	    }

	    /* time to emit the actual tone */
	    playtone(pitch, timeval, sustain);
	    break;

	case 'O':
	    if (slen > 0 && (cp[1] == 'N' || cp[1] == 'n'))
	    {
		octprefix = octtrack = false;
		++cp;
		slen--;
	    }
	    else if (slen > 0 && (cp[1] == 'L' || cp[1] == 'l'))
	    {
		octtrack = true;
		++cp;
		slen--;
	    }
	    else
	    {
		GETNUM(cp, octave);
		if (octave >= NOCTAVES)
		    octave = DFLT_OCTAVE;
		octprefix = true;
	    }
	    break;

	case '>':
	    if (octave < NOCTAVES - 1)
		octave++;
	    octprefix = true;
	    break;

	case '<':
	    if (octave > 0)
		octave--;
	    octprefix = true;
	    break;

	case 'N':
	    GETNUM(cp, pitch);
	    for (sustain = 0; slen > 0 && cp[1] == '.'; cp++)
	    {
		slen--;
		sustain++;
	    }
	    playtone(pitch - 1, value, sustain);
	    break;

	case 'L':
	    GETNUM(cp, value);
	    if (value <= 0 || value > MIN_VALUE)
		value = DFLT_VALUE;
	    break;

	case 'P':
	case '~':
	    /* this may be followed by an override time value */
	    GETNUM(cp, timeval);
	    if (timeval <= 0 || timeval > MIN_VALUE)
		timeval = value;
	    for (sustain = 0; slen > 0 && cp[1] == '.'; cp++)
	    {
		slen--;
		sustain++;
	    }
	    playtone(-1, timeval, sustain);
	    break;

	case 'T':
	    GETNUM(cp, tempo);
	    if (tempo < MIN_TEMPO || tempo > MAX_TEMPO)
		tempo = DFLT_TEMPO;
	    whole = (hz * SECS_PER_MIN * WHOLE_NOTE) / tempo;
	    break;

	case 'M':
	    if (slen > 0 && (cp[1] == 'N' || cp[1] == 'n'))
	    {
		fill = NORMAL;
		++cp;
		slen--;
	    }
	    else if (slen > 0 && (cp[1] == 'L' || cp[1] == 'l'))
	    {
		fill = LEGATO;
		++cp;
		slen--;
	    }
	    else if (slen > 0 && (cp[1] == 'S' || cp[1] == 's'))
	    {
		fill = STACCATO;
		++cp;
		slen--;
	    }
	    break;
	}
    }
}

/******************* UNIX DRIVER HOOKS BEGIN HERE **************************
 *
 * This section implements driver hooks to run playstring() and the tone(),
 * endtone(), and rest() functions defined above.
 */

static int spkr_active;	/* exclusion flag */
static void *spkr_inbuf;

static int spkr_attached = 0;

int
spkrprobe(device_t parent, cfdata_t match, void *aux)
{
	return (!spkr_attached);
}

void
spkrattach(device_t parent, device_t self, void *aux)
{
	printf("\n");
	ppicookie = ((struct pcppi_attach_args *)aux)->pa_cookie;
	spkr_attached = 1;
	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n"); 
}

int
spkrdetach(device_t self, int flags)
{

	pmf_device_deregister(self);
	spkr_attached = 0;
	ppicookie = NULL;

	return 0;
}

int
spkropen(dev_t dev, int	flags, int mode, struct lwp *l)
{
#ifdef SPKRDEBUG
    printf("spkropen: entering with dev = %"PRIx64"\n", dev);
#endif /* SPKRDEBUG */

    if (minor(dev) != 0 || !spkr_attached)
	return(ENXIO);
    else if (spkr_active)
	return(EBUSY);
    else
    {
	playinit();
	spkr_inbuf = malloc(DEV_BSIZE, M_DEVBUF, M_WAITOK);
	spkr_active = 1;
    }
    return(0);
}

int
spkrwrite(dev_t dev, struct uio *uio, int flags)
{
    int n;
    int error;
#ifdef SPKRDEBUG
    printf("spkrwrite: entering with dev = %"PRIx64", count = %zu\n",
		dev, uio->uio_resid);
#endif /* SPKRDEBUG */

    if (minor(dev) != 0)
	return(ENXIO);
    else
    {
	n = min(DEV_BSIZE, uio->uio_resid);
	error = uiomove(spkr_inbuf, n, uio);
	if (!error)
		playstring((char *)spkr_inbuf, n);
	return(error);
    }
}

int
spkrclose(dev_t dev, int flags, int mode, struct lwp *l)
{
#ifdef SPKRDEBUG
    printf("spkrclose: entering with dev = %"PRIx64"\n", dev);
#endif /* SPKRDEBUG */

    if (minor(dev) != 0)
	return(ENXIO);
    else
    {
	tone(0, 0);
	free(spkr_inbuf, M_DEVBUF);
	spkr_active = 0;
    }
    return(0);
}

int
spkrioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
#ifdef SPKRDEBUG
    printf("spkrioctl: entering with dev = %"PRIx64", cmd = %lx\n", dev, cmd);
#endif /* SPKRDEBUG */

    if (minor(dev) != 0)
	return(ENXIO);
    else if (cmd == SPKRTONE)
    {
	tone_t	*tp = (tone_t *)data;

	if (tp->frequency == 0)
	    rest(tp->duration);
	else
	    tone(tp->frequency, tp->duration);
    }
    else if (cmd == SPKRTUNE)
    {
	tone_t  *tp = (tone_t *)(*(void **)data);
	tone_t ttp;
	int error;

	for (; ; tp++) {
	    error = copyin(tp, &ttp, sizeof(tone_t));
	    if (error)
		    return(error);
	    if (ttp.duration == 0)
		    break;
	    if (ttp.frequency == 0)
		rest(ttp.duration);
	    else
		tone(ttp.frequency, ttp.duration);
	}
    }
    else
	return(EINVAL);
    return(0);
}

static int
spkr_modcmd(modcmd_t cmd, void *arg)
{
#ifdef _MODULE
	devmajor_t bmajor, cmajor;
#endif
	int error = 0;

#ifdef _MODULE
	switch(cmd) {
	case MODULE_CMD_INIT:
		bmajor = cmajor = -1;
		error = devsw_attach(spkr_cd.cd_name, NULL, &bmajor,
		    &spkr_cdevsw, &cmajor);
		if (error)
			break;

		error = config_init_component(cfdriver_ioconf_spkr,
			cfattach_ioconf_spkr, cfdata_ioconf_spkr);
		if (error) {
			devsw_detach(NULL, &spkr_cdevsw);
		}
		break;

	case MODULE_CMD_FINI:
		if (spkr_active)
			return EBUSY;
		error = config_fini_component(cfdriver_ioconf_spkr,
			cfattach_ioconf_spkr, cfdata_ioconf_spkr);
		devsw_detach(NULL, &spkr_cdevsw);
		break;
	default:
		error = ENOTTY;
		break;
	}
#endif

	return error;
}
/* spkr.c ends here */
