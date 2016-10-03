/* Debugging dump procedures for the VM server. */

#include "inc.h"
#include <sys/mman.h>
#include <minix/vm.h>
#include <minix/timers.h>
#include "kernel/proc.h"

#define LINES 24

static void print_region(struct vm_region_info *vri, int *n)
{
  static int vri_count, vri_prev_set;
  static struct vm_region_info vri_prev;
  int is_repeat;

  /* part of a contiguous identical run? */
  is_repeat =
	vri &&
  	vri_prev_set &&
	vri->vri_prot == vri_prev.vri_prot &&
	vri->vri_flags == vri_prev.vri_flags &&
	vri->vri_length == vri_prev.vri_length &&
	vri->vri_addr == vri_prev.vri_addr + vri_prev.vri_length;
  if (vri) {
  	vri_prev_set = 1;
	vri_prev = *vri;
  } else {
	vri_prev_set = 0;
  }
  if (is_repeat) {
	vri_count++;
	return;
  }

  if (vri_count > 0) {
	printf("  (contiguously repeated %d more times)\n", vri_count);
	(*n)++;
	vri_count = 0;
  }

  /* NULL indicates the end of a list of mappings, nothing else to do */
  if (!vri) return;

  printf("  %08lx-%08lx %c%c%c (%lu kB)\n", vri->vri_addr,
	vri->vri_addr + vri->vri_length,
	(vri->vri_prot & PROT_READ) ? 'r' : '-',
	(vri->vri_prot & PROT_WRITE) ? 'w' : '-',
	(vri->vri_prot & PROT_EXEC) ? 'x' : '-',
	vri->vri_length / 1024L);
  (*n)++;
}

void
vm_dmp(void)
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
		printf("IS: warning: couldn't talk to VM: %d\n", r);
		return;
	}

	printf("Total %lu kB, free %lu kB, largest free %lu kB, cached %lu kB\n",
		vsi.vsi_total * (vsi.vsi_pagesize / 1024),
		vsi.vsi_free * (vsi.vsi_pagesize / 1024),
		vsi.vsi_largest * (vsi.vsi_pagesize / 1024),
		vsi.vsi_cached * (vsi.vsi_pagesize / 1024));
	n++;
	printf("\n");
	n++;

  	prev_i++;
  }

  if ((r = sys_getproctab(proc)) != OK) {
	printf("IS: warning: couldn't get copy of process table: %d\n", r);
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

	while (r > 0) {
		for (j = 0; j < r; j++) {
			print_region(&vri[j], &n);
		}

		if (LINES - n - 1 <= 0) break;
		r = vm_info_region(proc[i].p_endpoint, vri, LINES - n - 1,
			&prev_base);

		if (r < 0) {
			printf("Process %d (%s): error %d\n",
				proc[i].p_endpoint, proc[i].p_name, r);
			n++;
		}
	}
	print_region(NULL, &n);

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

