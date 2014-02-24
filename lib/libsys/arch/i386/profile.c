/* 
 * profile.c - library functions for call profiling
 *
 * For processes that were compiled using ACK with the -Rcem-p option,
 * procentry and procexit will be called on entry and exit of their
 * functions.  Procentry/procexit are implemented here as generic library
 * functions.
 *
 * Changes:
 *   14 Aug, 2006   Created (Rogier Meurs)
 */

#include <lib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <minix/profile.h>
#include <minix/sysutil.h>
#include <minix/u64.h>
#include <minix/minlib.h>

static char cpath[CPROF_CPATH_MAX_LEN];	/* current call path string */
static int cpath_len;				/* current call path len */
static struct cprof_tbl_s *cprof_slot;		/* slot of current function */
struct stack_s {			/* stack entry */
		int cpath_len;			/* call path len */
		struct cprof_tbl_s *slot;	/* table slot */
		u64_t start_1;			/* count @ begin of procentry */
		u64_t start_2;			/* count @ end of procentry */
		u64_t spent_deeper;		/* spent in called functions */
};
static struct stack_s cprof_stk[CPROF_STACK_SIZE];	/* stack */
static int cprof_stk_top;				/* top of stack */
EXTERN struct cprof_tbl_s cprof_tbl[];			/* hash table */
static int cprof_tbl_size;				/* nr of slots */
static struct cprof_tbl_s *idx[CPROF_INDEX_SIZE];	/* index to table */
static struct cprof_ctl_s control;		/* for comms with kernel */
static int cprof_announce;			/* announce on n-th execution
						 * of procentry */
static int cprof_locked;			/* for reentrancy */

static void cprof_init(void);
static void reset(void);
static void clear_tbl(void);


void procentry(char *name)
{
  static int init = 0;
  unsigned hash = 0, x = 0;
  int i = 0;
  struct cprof_tbl_s *last;
  char c;
  u64_t start;

  /* Procentry is not reentrant. */
  if (cprof_locked) return; else cprof_locked = 1;

  /* Read CPU cycle count into local variable. */
  read_tsc_64(&start);

  /* Run init code once after system boot. */
  if (init == 0) {
	cprof_init();
	init++;
  }

  /* Announce once. */
  if (init > -1 && init++ == cprof_announce) {
	/* Tell kernel about control structure and table locations.
	*
	* In userspace processes, the library function profile_register
	* will be used. This function does a kernel call (sys_profbuf) to
	* announce to the kernel the location of the control struct and
	* hash table. The control struct is used by the kernel to write
	* a flag if resetting of the table is requested. The location of
	* the table is needed to copy the information to the user process
	* that requests it.
	*
	* Kernelspace processes don't use the library function but have
	* their own implemention that executes logic similar to sys_profbuf.
	* The reason for this is that the kernel is non-reentrant, therefore
	* a kernelspace process is not able to do a kernel call itself since
	* this would cause a deadlock.
	*/
	profile_register((void *) &control, (void *) &cprof_tbl);
	init = -1;
  }

  /* Only continue if sane. */
  if (control.err) return;

  /* Check if kernel instructed to reset profiling data. */
  if (control.reset) reset();

  /* Increase stack. */
  if (++cprof_stk_top == CPROF_STACK_SIZE) {
	printf("CPROFILE error: stack overrun\n");
	control.err |= CPROF_STACK_OVERRUN;
	return;
  }

  /* Save initial cycle count on stack. */
  cprof_stk[cprof_stk_top].start_1 = start;

  /* Check available call path len. */
  if (cpath_len + strlen(name) + 1 > CPROF_CPATH_MAX_LEN) {
	printf("CPROFILE error: call path overrun\n");
	control.err |= CPROF_CPATH_OVERRUN;
	return;
  }

  /* Save previous call path length on stack. */
  cprof_stk[cprof_stk_top].cpath_len = cpath_len;

  /* Generate new call path string and length.*/
  if (cprof_stk_top > 0)			/* Path is space separated. */
	cpath[cpath_len++] = ' ';
  while ((c = *(name++)) != '\0')	/* Append function name. */
	cpath[cpath_len++] = c;
  cpath[cpath_len] = '\0';		/* Null-termination. */

  /* Calculate hash for call path string (algorithm: ELF). */
  for (i=0; i<cpath_len; i++) {
	 hash = (hash << 4) + cpath[i];
	 if ((x = hash & 0xF0000000L) != 0) {
		 hash ^= (x >> 24);
		 hash &= ~x;
	 }
  }
  hash %= CPROF_INDEX_SIZE;

  /* Look up the slot for this call path in the hash table. */
  for (cprof_slot = idx[hash]; cprof_slot != 0; cprof_slot = cprof_slot->next)
	if (strcmp(cprof_slot->cpath, cpath) == 0) break;

  if (cprof_slot)
	cprof_slot->calls++;	/* found slot: update call counter */
  else {
	/* Not found: insert path into hash table. */
	if (control.slots_used == cprof_tbl_size) {
		printf("CPROFILE error: table overrun\n");
		control.err |= CPROF_TABLE_OVERRUN;
		return;
	}
	/* Set values for new slot. */
	cprof_slot = &cprof_tbl[control.slots_used++];
	strlcpy(cprof_slot->cpath, cpath, sizeof(cprof_slot->cpath));
	cprof_slot->calls = 1;

	/* Update index. */
	if (idx[hash] == 0) {
		/* No collision: simple update. */
		idx[hash] = cprof_slot;
	} else {
		/* Collision: update last in chain. */
		for (last = idx[hash]; last->next != 0; last = last->next);
		last->next = cprof_slot;
	}
  }
  /* Save slot on stack. */
  cprof_stk[cprof_stk_top].slot = cprof_slot;

  /* Again save CPU cycle count on stack. */
  read_tsc_64(&cprof_stk[cprof_stk_top].start_2);
  cprof_locked = 0;
}


void procexit(char *UNUSED(name))
{
  u64_t stop, spent;
  u32_t tsc_lo, tsc_hi;

  /* Procexit is not reentrant. */
  if (cprof_locked) return; else cprof_locked = 1;

  /* First thing: read CPU cycle count into local variable. */
  read_tsc(&tsc_hi, &tsc_lo);
  stop = make64(tsc_lo, tsc_hi);

  /* Only continue if sane. */
  if (control.err) return;

  /* Update cycle count for this call path. Exclude time spent in procentry/
   * procexit by using measurements taken at end of procentry and begin of
   * procexit (the "small" difference). This way, only the call overhead for
   * the procentry/procexit functions will be attributed to this call path,
   * not the procentry/procexit cycles.
   */

  /* Calculate "small" difference. */
  spent = stop - cprof_stk[cprof_stk_top].start_2;
  cprof_stk[cprof_stk_top].slot->cycles +=
	spent - cprof_stk[cprof_stk_top].spent_deeper;

  /* Clear spent_deeper for call level we're leaving. */
  cprof_stk[cprof_stk_top].spent_deeper = ((u64_t)(0));

  /* Adjust call path string and stack. */
  cpath_len = cprof_stk[cprof_stk_top].cpath_len;
  cpath[cpath_len] = '\0';

  /* Update spent_deeper for call level below. Include time spent in
   * procentry/procexit by using measurements taken at begin of procentry
   * and end of procexit (the "big" difference). This way the time spent in
   * procentry/procexit will be included in spent_deeper and therefore, since
   * this value is substracted from the lower call level, it will not be
   * attributed to any call path. This way, pollution of the statistics
   * because of procentry/procexit is kept to a minimum.
   */

  /* Read CPU cycle count. */
  read_tsc(&tsc_hi, &tsc_lo);
  stop = make64(tsc_lo, tsc_hi);

  /* Calculate "big" difference. */
  spent = stop - cprof_stk[cprof_stk_top].start_1;
  cprof_stk_top--;					/* decrease stack */
  if (cprof_stk_top >= 0)	    /* don't update non-existent level -1 */
	cprof_stk[cprof_stk_top].spent_deeper += spent;
  cprof_locked = 0;
}


static void cprof_init(void)
{
  int i;

  cpath[0] = '\0';
  cpath_len = 0;
  cprof_stk_top = -1;
  control.reset = 0;
  control.err = 0;
  cprof_tbl_size = profile_get_tbl_size();
  cprof_announce = profile_get_announce();
  clear_tbl();

  for (i=0; i<CPROF_STACK_SIZE; i++) {
	cprof_stk[i].cpath_len = 0;
	cprof_stk[i].slot = 0;
	cprof_stk[i].start_1 = ((u64_t)(0));
	cprof_stk[i].start_2 = ((u64_t)(0));
	cprof_stk[i].spent_deeper = ((u64_t)(0));
  }
}


static void reset(void)
{
  clear_tbl();
  control.reset = 0;
}


static void clear_tbl(void)
{
  int i;

  /* Reset profiling table. */
  control.slots_used = 0;
  for (i=0; i<CPROF_INDEX_SIZE; i++) idx[i] = 0;	/* clear index */
  for (i=0; i<cprof_tbl_size; i++) {			/* clear table */
	memset(cprof_tbl[i].cpath, '\0', CPROF_CPATH_MAX_LEN);
	cprof_tbl[i].next = 0;
	cprof_tbl[i].calls = 0;
	cprof_tbl[i].cycles = make64(0, 0);
  }
}

