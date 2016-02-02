/* MIB service - vm.c - implementation of the CTL_VM subtree */

#include "mib.h"

#include <sys/resource.h>
#include <uvm/uvm_extern.h>

/*
 * Implementation of CTL_VM VM_LOADAVG.
 */
static ssize_t
mib_vm_loadavg(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	struct loadavg loadavg;
	struct loadinfo loadinfo;
	unsigned long proc_load;
	u32_t ticks_per_slot, ticks;
	unsigned int p;
	int unfilled_ticks;
	int h, slots, latest, slot;
	int minutes[3] = { 1, 5, 15 };

	assert(__arraycount(loadavg.ldavg) == __arraycount(minutes));

	if (sys_getloadinfo(&loadinfo) != OK)
		return EINVAL;

	memset(&loadavg, 0, sizeof(loadavg));

	/*
	 * The following code is inherited from the old MINIX libc.
	 */

	/* How many ticks are missing from the newest-filled slot? */
	ticks_per_slot = _LOAD_UNIT_SECS * sys_hz();
	unfilled_ticks =
	    ticks_per_slot - (loadinfo.last_clock % ticks_per_slot);

	for (p = 0; p < __arraycount(loadavg.ldavg); p++) {
		latest = loadinfo.proc_last_slot;
		slots = minutes[p] * 60 / _LOAD_UNIT_SECS;
		proc_load = 0;

		/*
		 * Add up the total number of process ticks for this number
		 * of minutes (minutes[p]).  Start with the newest slot, which
		 * is latest, and count back for the number of slots that
		 * correspond to the right number of minutes.  Take wraparound
		 * into account by calculating the index modulo _LOAD_HISTORY,
		 * which is the number of slots of history kept.
		 */
		for (h = 0; h < slots; h++) {
			slot = (latest - h + _LOAD_HISTORY) % _LOAD_HISTORY;
			proc_load += loadinfo.proc_load_history[slot];
		}

		/*
		 * The load average over this number of minutes is the number
		 * of process-ticks divided by the number of ticks, not
		 * counting the number of ticks the last slot hasn't been
		 * around yet.
		 */
		ticks = slots * ticks_per_slot - unfilled_ticks;

		loadavg.ldavg[p] = 100UL * proc_load / ticks;
	}

	loadavg.fscale = 100L;

	return mib_copyout(oldp, 0, &loadavg, sizeof(loadavg));
}

/*
 * Implementation of CTL_VM VM_UVMEXP2.
 */
static ssize_t
mib_vm_uvmexp2(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	struct vm_stats_info vsi;
	struct uvmexp_sysctl ues;
	unsigned int shift;

	if (vm_info_stats(&vsi) != OK)
		return EINVAL;

	memset(&ues, 0, sizeof(ues));

	/*
	 * TODO: by far most of the structure is not filled correctly yet,
	 * since the MINIX3 system does not provide much of the information
	 * exposed by NetBSD.  This will gradually have to be filled in.
	 * For now, we provide just some basic information used by top(1).
	 */
	ues.pagesize = vsi.vsi_pagesize;
	ues.pagemask = vsi.vsi_pagesize - 1;
	for (shift = 0; shift < CHAR_BIT * sizeof(void *); shift++)
		if ((1U << shift) == vsi.vsi_pagesize)
			break;
	if (shift < CHAR_BIT * sizeof(void *))
		ues.pageshift = shift;
	ues.npages = vsi.vsi_total;
	ues.free = vsi.vsi_free;
	ues.filepages = vsi.vsi_cached;
	/*
	 * We use one of the structure's unused fields to expose information
	 * not exposed by NetBSD, namely the largest area of physically
	 * contiguous memory.  If NetBSD repurposes this field, we have to find
	 * another home for it (or expose it through a separate node or so).
	 */
	ues.unused1 = vsi.vsi_largest;

	return mib_copyout(oldp, 0, &ues, sizeof(ues));
}

/* The CTL_VM nodes. */
static struct mib_node mib_vm_table[] = {
/* 1*/	/* VM_METER: not yet supported */
/* 2*/	[VM_LOADAVG]		= MIB_FUNC(_P | _RO | CTLTYPE_STRUCT,
				    sizeof(struct loadavg), mib_vm_loadavg,
				    "loadavg", "System load average history"),
/* 3*/	/* VM_UVMEXP: not yet supported */
/* 4*/	/* VM_NKMEMPAGES: not yet supported */
/* 5*/	[VM_UVMEXP2]		= MIB_FUNC(_P | _RO | CTLTYPE_STRUCT,
				    sizeof(struct uvmexp_sysctl),
				    mib_vm_uvmexp2, "uvmexp2",
				    "Detailed system-wide virtual memory "
				    "statistics (MI)"),
/* 6*/	/* VM_ANONMIN: not yet supported */
/* 7*/	/* VM_EXECMIN: not yet supported */
/* 8*/	/* VM_FILEMIN: not yet supported */
/* 9*/	[VM_MAXSLP]		= MIB_INT(_P | _RO, MAXSLP, "maxslp",
				    "Maximum process sleep time before being "
				    "swapped"),
/*10*/	[VM_USPACE]		= MIB_INT(_P | _RO, 0, "uspace", "Number of "
				    "bytes allocated for a kernel stack"),
				    /* MINIX3 processes don't have k-stacks */
/*11*/	/* VM_ANONMAX: not yet supported */
/*12*/	/* VM_EXECMAX: not yet supported */
/*13*/	/* VM_FILEMAX: not yet supported */
};

/*
 * Initialize the CTL_VM subtree.
 */
void
mib_vm_init(struct mib_node * node)
{

	MIB_INIT_ENODE(node, mib_vm_table);
}
