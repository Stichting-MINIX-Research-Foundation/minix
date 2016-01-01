/*
 * This file contains several functions and variables used for statistical
 * system profiling, in particular the interrupt handler for profiling clock.
 *
 * Changes:
 *   14 Aug, 2006   Created, (Rogier Meurs)
 */

#include <minix/config.h>

#include "kernel/kernel.h"

#include <minix/profile.h>
#include <minix/portio.h>

#if SPROFILE

#include <string.h>
#include "watchdog.h"

char sprof_sample_buffer[SAMPLE_BUFFER_SIZE];

/* Function prototype for the profiling clock handler. */ 
static int profile_clock_handler(irq_hook_t *hook);

/* A hook for the profiling clock interrupt handler. */
static irq_hook_t profile_clock_hook;

/*===========================================================================*
 *			init_profile_clock				     *
 *===========================================================================*/
void init_profile_clock(u32_t freq)
{
  int irq;

  if((irq = arch_init_profile_clock(freq)) >= 0) {
	/* Register interrupt handler for statistical system profiling.  */
	profile_clock_hook.proc_nr_e = CLOCK;
	put_irq_handler(&profile_clock_hook, irq, profile_clock_handler);
	enable_irq(&profile_clock_hook);
  }
}

/*===========================================================================*
 *			profile_clock_stop				     *
 *===========================================================================*/
void stop_profile_clock(void)
{
  arch_stop_profile_clock();

  /* Unregister interrupt handler. */
  disable_irq(&profile_clock_hook);
  rm_irq_handler(&profile_clock_hook);
}

static void sprof_save_sample(struct proc * p, void * pc)
{
	struct sprof_sample *s;

	s = (struct sprof_sample *) (sprof_sample_buffer + sprof_info.mem_used);

	s->proc = p->p_endpoint;
	s->pc = pc;

	sprof_info.mem_used += sizeof(struct sprof_sample);
}

static void sprof_save_proc(struct proc * p)
{
	struct sprof_proc * s;

	s = (struct sprof_proc *) (sprof_sample_buffer + sprof_info.mem_used);

	s->proc = p->p_endpoint;
	strcpy(s->name, p->p_name);

	sprof_info.mem_used += sizeof(struct sprof_proc);
}

static void profile_sample(struct proc * p, void * pc)
{
/* This executes on every tick of the CMOS timer. */

  /* Are we profiling, and profiling memory not full? */
  if (!sprofiling || sprof_info.mem_used == -1)
	  return;

  /* Check if enough memory available before writing sample. */
  if (sprof_info.mem_used + sizeof(sprof_info) +
		  2*sizeof(struct sprof_sample) +
		  2*sizeof(struct sprof_sample) > sprof_mem_size) {
	sprof_info.mem_used = -1;
	return;
  }

  /* Runnable system process? */
  if (p->p_endpoint == IDLE)
	sprof_info.idle_samples++;
  else if (p->p_endpoint == KERNEL ||
		(priv(p)->s_flags & SYS_PROC && proc_is_runnable(p))) {

	if (!(p->p_misc_flags & MF_SPROF_SEEN)) {
		p->p_misc_flags |= MF_SPROF_SEEN;
		sprof_save_proc(p);
	}

	sprof_save_sample(p, pc);
	sprof_info.system_samples++;
  } else {
	/* User process. */
	sprof_info.user_samples++;
  }
  
  sprof_info.total_samples++;
}

/*===========================================================================*
 *			profile_clock_handler                           *
 *===========================================================================*/
static int profile_clock_handler(irq_hook_t *hook)
{
  struct proc * p;
  p = get_cpulocal_var(proc_ptr);

  profile_sample(p, (void *) p->p_reg.pc);

  /* Acknowledge interrupt if necessary. */
  arch_ack_profile_clock();

  return(1);                                    /* reenable interrupts */
}

void nmi_sprofile_handler(struct nmi_frame * frame)
{
	struct proc * p = get_cpulocal_var(proc_ptr);
	/*
	 * test if the kernel was interrupted. If so, save first a sample fo
	 * kernel and than for the current process, otherwise save just the
	 * process
	 */
	if (nmi_in_kernel(frame)) {
		struct proc *kern;

		/*
		 * if we sample kernel, check if IDLE is scheduled. If so,
		 * account for idle time rather than taking kernel sample
		 */
		if (p->p_endpoint == IDLE) {
			sprof_info.idle_samples++;
			sprof_info.total_samples++;
			return;
		}

		kern = proc_addr(KERNEL);

		profile_sample(kern, (void *) frame->pc);
	}
	else
		profile_sample(p, (void *) frame->pc);
}

#endif /* SPROFILE */
