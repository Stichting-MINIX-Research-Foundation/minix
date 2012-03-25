/* ProcFS - util.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				procfs_getloadavg			     *
 *===========================================================================*/
int procfs_getloadavg(struct load *loadavg, int nelem)
{
	/* Retrieve system load average information.
	 */
	struct loadinfo loadinfo;
	u32_t system_hz, ticks_per_slot;
	int p, unfilled_ticks;
	int minutes[3] = { 1, 5, 15 };
	ssize_t l;

	if(nelem < 1) {
		errno = ENOSPC;
		return -1;
	}

	system_hz = sys_hz();

	if((l=sys_getloadinfo(&loadinfo)) != OK)
		return -1;
	if(nelem > 3)
		nelem = 3;

	/* How many ticks are missing from the newest-filled slot? */
	ticks_per_slot = _LOAD_UNIT_SECS * system_hz;
	unfilled_ticks =
		ticks_per_slot - (loadinfo.last_clock % ticks_per_slot);

	for(p = 0; p < nelem; p++) {
		int h, slots;
		int latest = loadinfo.proc_last_slot;
		slots = minutes[p] * 60 / _LOAD_UNIT_SECS;
		loadavg[p].proc_load = 0;

		/* Add up the total number of process ticks for this number
		 * of minutes (minutes[p]). Start with the newest slot, which
		 * is latest, and count back for the number of slots that
		 * correspond to the right number of minutes. Take wraparound
		 * into account by calculating the index modulo _LOAD_HISTORY,
		 * which is the number of slots of history kept.
		 */
		for(h = 0; h < slots; h++) {
			int slot;
			slot = (latest - h + _LOAD_HISTORY) % _LOAD_HISTORY;
			loadavg[p].proc_load +=
				 loadinfo.proc_load_history[slot];
			l += (double) loadinfo.proc_load_history[slot];
		}

		/* The load average over this number of minutes is the number
		 * of process-ticks divided by the number of ticks, not
		 * counting the number of ticks the last slot hasn't been
		 * around yet.
		 */
		loadavg[p].ticks = slots * ticks_per_slot - unfilled_ticks;
	}

	return nelem;
}
