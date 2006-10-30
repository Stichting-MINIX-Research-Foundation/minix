/* The kernel call that is implemented in this file:
 *   m_type:    SYS_CPROFILE
 *
 * The parameters for this kernel call are:
 *    m7_i1:    PROF_ACTION       (get/reset profiling data)
 *    m7_i2:    PROF_MEM_SIZE     (available memory for data)
 *    m7_i4:    PROF_ENDPT        (endpoint of caller)
 *    m7_p1:    PROF_CTL_PTR      (location of info struct)
 *    m7_p2:    PROF_MEM_PTR      (location of memory for data)
 *
 * Changes:
 *   14 Aug, 2006   Created (Rogier Meurs)
 */

#include "../system.h"

#if CPROFILE

#include <string.h>

/*===========================================================================*
 *				do_cprofile				     *
 *===========================================================================*/
PUBLIC int do_cprofile(m_ptr)
register message *m_ptr;    /* pointer to request message */
{
  int proc_nr, i, err = 0, k = 0;
  vir_bytes vir_dst;
  phys_bytes phys_src, phys_dst, len;

  switch (m_ptr->PROF_ACTION) {

  case PROF_RESET:

	/* Reset profiling tables. */

	cprof_ctl_inst.reset = 1;

	kprintf("CPROFILE notice: resetting tables:");

	for (i=0; i<cprof_procs_no; i++) {

		kprintf(" %s", cprof_proc_info[i].name);

		/* Test whether proc still alive. */
		if (!isokendpt(cprof_proc_info[i].endpt, &proc_nr)) {
			kprintf("endpt not valid %u (%s)\n",
			cprof_proc_info[i].endpt, cprof_proc_info[i].name);
			continue;
		}

		/* Set reset flag. */
		phys_src = vir2phys((vir_bytes) &cprof_ctl_inst.reset);
		phys_dst = (phys_bytes) cprof_proc_info[i].ctl;
		len = (phys_bytes) sizeof(cprof_ctl_inst.reset);
		phys_copy(phys_src, phys_dst, len);
	}
	kprintf("\n");
	
	return OK;

  case PROF_GET:

	/* Get profiling data.
	 *
	 * Calculate physical addresses of user pointers.  Copy to user
	 * program the info struct.  Copy to user program the profiling
	 * tables of the profiled processes.
	 */

	if(!isokendpt(m_ptr->PROF_ENDPT, &proc_nr))
		return EINVAL;

	vir_dst = (vir_bytes) m_ptr->PROF_CTL_PTR;
	len = (phys_bytes) sizeof (int *);
	cprof_info_addr = numap_local(proc_nr, vir_dst, len);

	vir_dst = (vir_bytes) m_ptr->PROF_MEM_PTR;
	len = (phys_bytes) sizeof (char *);
	cprof_data_addr = numap_local(proc_nr, vir_dst, len);

	cprof_mem_size = m_ptr->PROF_MEM_SIZE;

	kprintf("CPROFILE notice: getting tables:");

	/* Copy control structs of profiled processes to calculate total
	 * nr of bytes to be copied to user program and find out if any
	 * errors happened. */
	cprof_info.mem_used = 0;
	cprof_info.err = 0;

	for (i=0; i<cprof_procs_no; i++) {

		kprintf(" %s", cprof_proc_info[i].name);

		/* Test whether proc still alive. */
		if (!isokendpt(cprof_proc_info[i].endpt, &proc_nr)) {
			kprintf("endpt not valid %u (%s)\n",
			cprof_proc_info[i].endpt, cprof_proc_info[i].name);
			continue;
		}

		/* Copy control struct from proc to local variable. */
		phys_src = cprof_proc_info[i].ctl;
		phys_dst = vir2phys((vir_bytes) &cprof_ctl_inst);
		len = (phys_bytes) sizeof(cprof_ctl_inst);
		phys_copy(phys_src, phys_dst, len);

	       	/* Calculate memory used. */
		cprof_proc_info[i].slots_used = cprof_ctl_inst.slots_used;
		cprof_info.mem_used += CPROF_PROCNAME_LEN;
		cprof_info.mem_used += sizeof(cprof_proc_info_inst.slots_used);
		cprof_info.mem_used += cprof_proc_info[i].slots_used *
						sizeof(cprof_tbl_inst);
		/* Collect errors. */
		cprof_info.err |= cprof_ctl_inst.err;
	}
	kprintf("\n");

	/* Do we have the space available? */
	if (cprof_mem_size < cprof_info.mem_used) cprof_info.mem_used = -1;

	/* Copy the info struct to the user process. */
	phys_copy(vir2phys((vir_bytes) &cprof_info), cprof_info_addr,
					(phys_bytes) sizeof(cprof_info));

	/* If there is no space or errors occurred, don't bother copying. */
	if (cprof_info.mem_used == -1 || cprof_info.err) return OK;

	/* For each profiled process, copy its name, slots_used and profiling
	 * table to the user process. */
	phys_dst = cprof_data_addr;
	for (i=0; i<cprof_procs_no; i++) {
		phys_src = vir2phys((vir_bytes) cprof_proc_info[i].name);
		len = (phys_bytes) strlen(cprof_proc_info[i].name);
		phys_copy(phys_src, phys_dst, len);
		phys_dst += CPROF_PROCNAME_LEN;

		phys_src = cprof_proc_info[i].ctl +
						sizeof(cprof_ctl_inst.reset);
		len = (phys_bytes) sizeof(cprof_ctl_inst.slots_used);
		phys_copy(phys_src, phys_dst, len);
		phys_dst += len;

		phys_src = cprof_proc_info[i].buf;
		len = (phys_bytes)
		(sizeof(cprof_tbl_inst) * cprof_proc_info[i].slots_used);
		phys_copy(phys_src, phys_dst, len);
		phys_dst += len;
	}

	return OK;

  default:
	return EINVAL;
  }
}

#endif /* CPROFILE */

