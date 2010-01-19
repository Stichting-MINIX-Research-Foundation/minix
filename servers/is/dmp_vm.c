/* Debugging dump procedures for the VM server. */

#include "inc.h"
#include <sys/mman.h>
#include <minix/vm.h>
#include <timers.h>
#include "../../kernel/proc.h"

#define LINES 24

PRIVATE void print_region(struct vm_region_info *vri)
{
  char c;

  switch (vri->vri_seg) {
  case T: c = 'T'; break;
  case D: c = 'D'; break;
  default: c = '?';
  }

  printf("  %c %08lx-%08lx %c%c%c %c (%lu kB)\n", c, vri->vri_addr,
	vri->vri_addr + vri->vri_length,
	(vri->vri_prot & PROT_READ) ? 'r' : '-',
	(vri->vri_prot & PROT_WRITE) ? 'w' : '-',
	(vri->vri_prot & PROT_EXEC) ? 'x' : '-',
	(vri->vri_flags & MAP_SHARED) ? 's' : 'p',
	vri->vri_length / 1024L);
}

PUBLIC void vm_dmp()
{
  static struct proc proc[NR_TASKS + NR_PROCS];
  static struct vm_region_info vri[LINES];
  struct vm_stats_info vsi;
  struct vm_usage_info vui;
  static int prev_i = -1;
  static vir_bytes prev_base = 0;
  int r, r2, i, j, first, n = 0;

  if (prev_i == -1) {
	if ((r = vm_info_stats(&vsi)) != OK) {
		report("IS", "warning: couldn't talk to VM", r);
		return;
	}

	printf("Total %u kB, free %u kB, largest free area %u kB\n",	
		vsi.vsi_total * (vsi.vsi_pagesize / 1024),
		vsi.vsi_free * (vsi.vsi_pagesize / 1024),
		vsi.vsi_largest * (vsi.vsi_pagesize / 1024));
	n++;
	printf("\n");
	n++;

  	prev_i++;
  }

  if ((r = sys_getproctab(proc)) != OK) {
	report("IS", "warning: couldn't get copy of process table", r);
	return;
  }

  for (i = prev_i; i < NR_TASKS + NR_PROCS && n < LINES; i++, prev_base = 0) {
	if (i < NR_TASKS || isemptyp(&proc[i])) continue;

	/* The first batch dump for each process contains a header line. */
	first = prev_base == 0;

	r = vm_info_region(proc[i].p_endpoint, vri, LINES - first, &prev_base);

	if (r < 0) {
		printf("Process %d (%s): error %d\n",
			proc[i].p_endpoint, proc[i].p_name, r);
		n++;
		continue;
	}

	if (first) {
		/* The entire batch should fit on the screen. */
		if (n + 1 + r > LINES) {
			prev_base = 0;	/* restart on next page */
			break;
		}

		if ((r2 = vm_info_usage(proc[i].p_endpoint, &vui)) != OK) {
			printf("Process %d (%s): error %d\n",
				proc[i].p_endpoint, proc[i].p_name, r2);
			n++;
			continue;
		}

		printf("Process %d (%s): total %lu kB, common %lu kB, "
			"shared %lu kB\n",
			proc[i].p_endpoint, proc[i].p_name,
			vui.vui_total / 1024L, vui.vui_common / 1024L,
			vui.vui_shared / 1024L);
		n++;
	}

	for (j = 0; j < r; j++) {
		print_region(&vri[j]);
		n++;
	}

	if (n > LINES) printf("IS: internal error\n");
	if (n == LINES) break;

	/* This may have to wipe out the "--more--" from below. */
	printf("        \n");
	n++;
  }

  if (i >= NR_TASKS + NR_PROCS) {
	i = -1;
	prev_base = 0;
  }
  else printf("--more--\r");
  prev_i = i;
}

