
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <lib.h>

/* Retrieve system load average information. */
int getloadavg(double *loadavg, int nelem)
{
  struct loadinfo loadinfo;
  int h, p, unfilled_ticks;
#define PERIODS 3
  int minutes[3] = { 1, 5, 15 };
  if(nelem < 1) {
	errno = ENOSPC;
	return -1;
  }

  if(getsysinfo(PM_PROC_NR, SI_LOADINFO, &loadinfo) < 0)
	return -1;
  if(nelem > PERIODS)
	nelem = PERIODS;

  /* How many ticks are missing from the newest-filled slot? */
#define TICKSPERSLOT (_LOAD_UNIT_SECS * HZ)
  unfilled_ticks = TICKSPERSLOT - (loadinfo.last_clock % TICKSPERSLOT);

  for(p = 0; p < nelem; p++) {
    int h, offset, slots;
    double l = 0.0;
    int latest = loadinfo.proc_last_slot;
    slots = minutes[p] * 60 / _LOAD_UNIT_SECS;

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
       l += (double) loadinfo.proc_load_history[slot];
    }

    /* The load average over this number of minutes is the number of
     * process-ticks divided by the number of ticks, not counting the
     * number of ticks the last slot hasn't been around yet.
     */
    loadavg[p] = l / (slots * TICKSPERSLOT - unfilled_ticks);
  }

  return nelem;
}

