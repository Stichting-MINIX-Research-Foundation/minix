/*	$NetBSD: subr_prof.c,v 1.47 2014/07/10 21:13:52 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)subr_prof.c	8.4 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_prof.c,v 1.47 2014/07/10 21:13:52 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>

#include <sys/cpu.h>

#ifdef GPROF
#include <sys/malloc.h>
#include <sys/gmon.h>

MALLOC_DEFINE(M_GPROF, "gprof", "kernel profiling buffer");

/*
 * Froms is actually a bunch of unsigned shorts indexing tos
 */
struct gmonparam _gmonparam = { .state = GMON_PROF_OFF };

/* Actual start of the kernel text segment. */
extern char kernel_text[];

extern char etext[];


void
kmstartup(void)
{
	char *cp;
	struct gmonparam *p = &_gmonparam;
	/*
	 * Round lowpc and highpc to multiples of the density we're using
	 * so the rest of the scaling (here and in gprof) stays in ints.
	 */
	p->lowpc = rounddown(((u_long)kernel_text),
		HISTFRACTION * sizeof(HISTCOUNTER));
	p->highpc = roundup((u_long)etext,
		HISTFRACTION * sizeof(HISTCOUNTER));
	p->textsize = p->highpc - p->lowpc;
	printf("Profiling kernel, textsize=%ld [%lx..%lx]\n",
	       p->textsize, p->lowpc, p->highpc);
	p->kcountsize = p->textsize / HISTFRACTION;
	p->hashfraction = HASHFRACTION;
	p->fromssize = p->textsize / HASHFRACTION;
	p->tolimit = p->textsize * ARCDENSITY / 100;
	if (p->tolimit < MINARCS)
		p->tolimit = MINARCS;
	else if (p->tolimit > MAXARCS)
		p->tolimit = MAXARCS;
	p->tossize = p->tolimit * sizeof(struct tostruct);
	cp = malloc(p->kcountsize + p->fromssize + p->tossize,
	    M_GPROF, M_NOWAIT | M_ZERO);
	if (cp == 0) {
		printf("No memory for profiling.\n");
		return;
	}
	p->tos = (struct tostruct *)cp;
	cp += p->tossize;
	p->kcount = (u_short *)cp;
	cp += p->kcountsize;
	p->froms = (u_short *)cp;
}

/*
 * Return kernel profiling information.
 */
/*
 * sysctl helper routine for kern.profiling subtree.  enables/disables
 * kernel profiling and gives out copies of the profiling data.
 */
static int
sysctl_kern_profiling(SYSCTLFN_ARGS)
{
	struct gmonparam *gp = &_gmonparam;
	int error;
	struct sysctlnode node;

	node = *rnode;

	switch (node.sysctl_num) {
	case GPROF_STATE:
		node.sysctl_data = &gp->state;
		break;
	case GPROF_COUNT:
		node.sysctl_data = gp->kcount;
		node.sysctl_size = gp->kcountsize;
		break;
	case GPROF_FROMS:
		node.sysctl_data = gp->froms;
		node.sysctl_size = gp->fromssize;
		break;
	case GPROF_TOS:
		node.sysctl_data = gp->tos;
		node.sysctl_size = gp->tossize;
		break;
	case GPROF_GMONPARAM:
		node.sysctl_data = gp;
		node.sysctl_size = sizeof(*gp);
		break;
	default:
		return (EOPNOTSUPP);
	}

	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (node.sysctl_num == GPROF_STATE) {
		mutex_spin_enter(&proc0.p_stmutex);
		if (gp->state == GMON_PROF_OFF)
			stopprofclock(&proc0);
		else
			startprofclock(&proc0);
		mutex_spin_exit(&proc0.p_stmutex);
	}

	return (0);
}

SYSCTL_SETUP(sysctl_kern_gprof_setup, "sysctl kern.profiling subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "profiling",
		       SYSCTL_DESCR("Profiling information (available)"),
		       NULL, 0, NULL, 0,
		       CTL_KERN, KERN_PROF, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "state",
		       SYSCTL_DESCR("Profiling state"),
		       sysctl_kern_profiling, 0, NULL, 0,
		       CTL_KERN, KERN_PROF, GPROF_STATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRUCT, "count",
		       SYSCTL_DESCR("Array of statistical program counters"),
		       sysctl_kern_profiling, 0, NULL, 0,
		       CTL_KERN, KERN_PROF, GPROF_COUNT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRUCT, "froms",
		       SYSCTL_DESCR("Array indexed by program counter of "
				    "call-from points"),
		       sysctl_kern_profiling, 0, NULL, 0,
		       CTL_KERN, KERN_PROF, GPROF_FROMS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRUCT, "tos",
		       SYSCTL_DESCR("Array of structures describing "
				    "destination of calls and their counts"),
		       sysctl_kern_profiling, 0, NULL, 0,
		       CTL_KERN, KERN_PROF, GPROF_TOS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "gmonparam",
		       SYSCTL_DESCR("Structure giving the sizes of the above "
				    "arrays"),
		       sysctl_kern_profiling, 0, NULL, 0,
		       CTL_KERN, KERN_PROF, GPROF_GMONPARAM, CTL_EOL);
}
#endif /* GPROF */

/*
 * Profiling system call.
 *
 * The scale factor is a fixed point number with 16 bits of fraction, so that
 * 1.0 is represented as 0x10000.  A scale factor of 0 turns off profiling.
 */
/* ARGSUSED */
int
sys_profil(struct lwp *l, const struct sys_profil_args *uap, register_t *retval)
{
	/* {
		syscallarg(char *) samples;
		syscallarg(size_t) size;
		syscallarg(u_long) offset;
		syscallarg(u_int) scale;
	} */
	struct proc *p = l->l_proc;
	struct uprof *upp;

	if (SCARG(uap, scale) > (1 << 16))
		return (EINVAL);
	if (SCARG(uap, scale) == 0) {
		mutex_spin_enter(&p->p_stmutex);
		stopprofclock(p);
		mutex_spin_exit(&p->p_stmutex);
		return (0);
	}
	upp = &p->p_stats->p_prof;

	/* Block profile interrupts while changing state. */
	mutex_spin_enter(&p->p_stmutex);
	upp->pr_off = SCARG(uap, offset);
	upp->pr_scale = SCARG(uap, scale);
	upp->pr_base = SCARG(uap, samples);
	upp->pr_size = SCARG(uap, size);
	startprofclock(p);
	mutex_spin_exit(&p->p_stmutex);

	return (0);
}

/*
 * Scale is a fixed-point number with the binary point 16 bits
 * into the value, and is <= 1.0.  pc is at most 32 bits, so the
 * intermediate result is at most 48 bits.
 */
#define	PC_TO_INDEX(pc, prof) \
	((int)(((u_quad_t)((pc) - (prof)->pr_off) * \
	    (u_quad_t)((prof)->pr_scale)) >> 16) & ~1)

/*
 * Collect user-level profiling statistics; called on a profiling tick,
 * when a process is running in user-mode.  This routine may be called
 * from an interrupt context.  We try to update the user profiling buffers
 * cheaply with fuswintr() and suswintr().  If that fails, we revert to
 * an AST that will vector us to trap() with a context in which copyin
 * and copyout will work.  Trap will then call addupc_task().
 *
 * Note that we may (rarely) not get around to the AST soon enough, and
 * lose profile ticks when the next tick overwrites this one, but in this
 * case the system is overloaded and the profile is probably already
 * inaccurate.
 */
void
addupc_intr(struct lwp *l, u_long pc)
{
	struct uprof *prof;
	struct proc *p;
	void *addr;
	u_int i;
	int v;

	p = l->l_proc;

	KASSERT(mutex_owned(&p->p_stmutex));

	prof = &p->p_stats->p_prof;
	if (pc < prof->pr_off ||
	    (i = PC_TO_INDEX(pc, prof)) >= prof->pr_size)
		return;			/* out of range; ignore */

	addr = prof->pr_base + i;
	mutex_spin_exit(&p->p_stmutex);
	if ((v = fuswintr(addr)) == -1 || suswintr(addr, v + 1) == -1) {
		/* XXXSMP */
		prof->pr_addr = pc;
		prof->pr_ticks++;
		cpu_need_proftick(l);
	}
	mutex_spin_enter(&p->p_stmutex);
}

/*
 * Much like before, but we can afford to take faults here.  If the
 * update fails, we simply turn off profiling.
 */
void
addupc_task(struct lwp *l, u_long pc, u_int ticks)
{
	struct uprof *prof;
	struct proc *p;
	void *addr;
	int error;
	u_int i;
	u_short v;

	p = l->l_proc;

	if (ticks == 0)
		return;

	mutex_spin_enter(&p->p_stmutex);
	prof = &p->p_stats->p_prof;

	/* Testing P_PROFIL may be unnecessary, but is certainly safe. */
	if ((p->p_stflag & PST_PROFIL) == 0 || pc < prof->pr_off ||
	    (i = PC_TO_INDEX(pc, prof)) >= prof->pr_size) {
		mutex_spin_exit(&p->p_stmutex);
		return;
	}

	addr = prof->pr_base + i;
	mutex_spin_exit(&p->p_stmutex);
	if ((error = copyin(addr, (void *)&v, sizeof(v))) == 0) {
		v += ticks;
		error = copyout((void *)&v, addr, sizeof(v));
	}
	if (error != 0) {
		mutex_spin_enter(&p->p_stmutex);
		stopprofclock(p);
		mutex_spin_exit(&p->p_stmutex);
	}
}
