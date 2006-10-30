/*
 * This file contains several functions and variables used for system
 * profiling.
 *
 * Statistical Profiling:
 *   The interrupt handler and control functions for the CMOS clock. 
 *
 * Call Profiling:
 *   The table used for profiling data and a function to get its size.
 *
 *   The function used by kernelspace processes to register the locations
 *   of their control struct and profiling table.
 *
 * Changes:
 *   14 Aug, 2006   Created, (Rogier Meurs)
 */

#include <minix/config.h>

#if SPROFILE || CPROFILE

#include <minix/profile.h>
#include "kernel.h"
#include "profile.h"
#include "proc.h"

#endif

#if SPROFILE

#include <string.h>
#include <ibm/cmos.h>

/* Function prototype for the CMOS clock handler. */ 
FORWARD _PROTOTYPE( int cmos_clock_handler, (irq_hook_t *hook) );

/* A hook for the CMOS clock interrupt handler. */
PRIVATE irq_hook_t cmos_clock_hook;

/*===========================================================================*
 *				init_cmos_clock				     *
 *===========================================================================*/
PUBLIC void init_cmos_clock(unsigned freq)
{
  int r;
  /* Register interrupt handler for statistical system profiling.
   * This uses the CMOS timer.
   */
  cmos_clock_hook.proc_nr_e = CLOCK;
  put_irq_handler(&cmos_clock_hook, CMOS_CLOCK_IRQ, cmos_clock_handler);
  enable_irq(&cmos_clock_hook);

  intr_disable();

  /* Set CMOS timer frequency. */
  outb(RTC_INDEX, RTC_REG_A);
  outb(RTC_IO, RTC_A_DV_OK | freq);
  /* Enable CMOS timer interrupts. */
  outb(RTC_INDEX, RTC_REG_B);
  r = inb(RTC_IO);
  outb(RTC_INDEX, RTC_REG_B);
  outb(RTC_IO, r | RTC_B_PIE);
  /* Mandatory read of CMOS register to enable timer interrupts. */
  outb(RTC_INDEX, RTC_REG_C);
  inb(RTC_IO);

  intr_enable();
}

/*===========================================================================*
 *				cmos_clock_stop				     *
 *===========================================================================*/
PUBLIC void stop_cmos_clock()
{
  int r;

  intr_disable();

  /* Disable CMOS timer interrupts. */
  outb(RTC_INDEX, RTC_REG_B);
  r = inb(RTC_IO);
  outb(RTC_INDEX, RTC_REG_B);
  outb(RTC_IO, r & !RTC_B_PIE);

  intr_enable();

  /* Unregister interrupt handler. */
  disable_irq(&cmos_clock_hook);
  rm_irq_handler(&cmos_clock_hook);
}

/*===========================================================================*
 *				cmos_clock_handler                           *
 *===========================================================================*/
PRIVATE int cmos_clock_handler(hook)
irq_hook_t *hook;
{
/* This executes on every tick of the CMOS timer. */

  /* Are we profiling, and profiling memory not full? */
  if (!sprofiling || sprof_info.mem_used == -1) return (1);

  /* Check if enough memory available before writing sample. */
  if (sprof_info.mem_used + sizeof(sprof_info) > sprof_mem_size) {
	sprof_info.mem_used = -1;
	return(1);
  }

  /* All is OK */

  /* Idle process? */
  if (priv(proc_ptr)->s_proc_nr == IDLE) {
	sprof_info.idle_samples++;
  } else
  /* Runnable system process? */
  if (priv(proc_ptr)->s_flags & SYS_PROC && !proc_ptr->p_rts_flags) {
	/* Note: k_reenter is always 0 here. */

	/* Store sample (process name and program counter). */
	phys_copy(vir2phys(proc_ptr->p_name),
		(phys_bytes) (sprof_data_addr + sprof_info.mem_used),
		(phys_bytes) strlen(proc_ptr->p_name));

	phys_copy(vir2phys(&proc_ptr->p_reg.pc),
		(phys_bytes) (sprof_data_addr+sprof_info.mem_used +
					sizeof(proc_ptr->p_name)),
		(phys_bytes) sizeof(proc_ptr->p_reg.pc));

	sprof_info.mem_used += sizeof(sprof_sample);

	sprof_info.system_samples++;
  } else {
	/* User process. */
	sprof_info.user_samples++;
  }
  
  sprof_info.total_samples++;

  /* Mandatory read of CMOS register to re-enable timer interrupts. */
  outb(RTC_INDEX, RTC_REG_C);
  inb(RTC_IO);

  return(1);                                    /* reenable interrupts */
}

#endif /* SPROFILE */


#if CPROFILE

/* 
 * The following variables and functions are used by the procentry/
 * procentry syslib functions when linked with kernelspace processes.
 * For userspace processes, the same variables and function are defined
 * elsewhere. This enables different functionality and variable sizes,
 * which is needed is a few cases.
 */

/* A small table is declared for the kernelspace processes. */
struct cprof_tbl_s cprof_tbl[CPROF_TABLE_SIZE_KERNEL];

/* Function that returns table size. */
PUBLIC int profile_get_tbl_size(void)
{
  return CPROF_TABLE_SIZE_KERNEL;
}

/* Function that returns on which execution of procentry to announce. */
PUBLIC int profile_get_announce(void)
{
  return CPROF_ACCOUNCE_KERNEL;
}

/*
 * The kernel "announces" its control struct and table locations
 * to itself through this function.
 */
PUBLIC void profile_register(ctl_ptr, tbl_ptr)
void *ctl_ptr;
void *tbl_ptr;
{
  int len, proc_nr;
  vir_bytes vir_dst;
  struct proc *rp;                          

  /* Store process name, control struct, table locations. */
  proc_nr = KERNEL;
  rp = proc_addr(proc_nr);

  cprof_proc_info[cprof_procs_no].endpt = rp->p_endpoint;
  cprof_proc_info[cprof_procs_no].name = rp->p_name;

  len = (phys_bytes) sizeof (void *);

  vir_dst = (vir_bytes) ctl_ptr;
  cprof_proc_info[cprof_procs_no].ctl =
	  numap_local(proc_nr, vir_dst, len);

  vir_dst = (vir_bytes) tbl_ptr;
  cprof_proc_info[cprof_procs_no].buf =
	  numap_local(proc_nr, vir_dst, len);

  cprof_procs_no++;
}

#endif /* CPROFILE */

