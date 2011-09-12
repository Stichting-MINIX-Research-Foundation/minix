/* This file handles nested counter-request calls to VFS sent by file system
 * (FS) servers in response to VFS requests.
 *
 * The entry points into this file are
 *   nested_fs_call	perform a nested call from a file system server
 *   nested_dev_call	perform a nested call from a device driver server
 *
 */

#include "fs.h"
#include "fproc.h"
#include <string.h>
#include <assert.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/vfsif.h>

/* maximum nested call stack depth */
#define MAX_DEPTH 1

/* global variables stack */
PRIVATE struct {
  struct fproc *g_fp;			/* pointer to caller process */
  message g_m_in;			/* request message */
  message g_m_out;			/* reply message */
  int g_who_e;				/* endpoint of caller process */
  int g_who_p;				/* slot number of caller process */
  int g_call_nr;			/* call number */
  int g_super_user;			/* is the caller root? */
  char g_user_fullpath[PATH_MAX];	/* path to look up */
} globals[MAX_DEPTH];

PRIVATE int depth = 0;			/* current globals stack level */

#if ENABLE_SYSCALL_STATS
EXTERN unsigned long calls_stats[NCALLS];
#endif

FORWARD _PROTOTYPE( int push_globals, (void)				);
FORWARD _PROTOTYPE( void pop_globals, (void)				);
FORWARD _PROTOTYPE( void set_globals, (message *m)			);

/*===========================================================================*
 *				push_globals				     *
 *===========================================================================*/
PRIVATE int push_globals()
{
/* Save the global variables of the current call onto the globals stack.
 */

  if (depth == MAX_DEPTH)
	return(EPERM);

  globals[depth].g_fp = fp;
  globals[depth].g_m_in = m_in;
  globals[depth].g_m_out = m_out;
  globals[depth].g_super_user = super_user;

  /* err_code is not used across blocking calls */
  depth++;
  return(OK);
}

/*===========================================================================*
 *				pop_globals				     *
 *===========================================================================*/
PRIVATE void pop_globals()
{
/* Restore the global variables of a call from the globals stack.
 */

  if (depth == 0)
	panic("Popping from empty globals stack!");

  depth--;

  fp = globals[depth].g_fp;
  m_in = globals[depth].g_m_in;
  m_out = globals[depth].g_m_out;

}

/*===========================================================================*
 *				set_globals				     *
 *===========================================================================*/
PRIVATE void set_globals(m)
message *m;				/* request message */
{
/* Initialize global variables based on a request message.
 */
  int proc_p;

  m_in = *m;

  proc_p = _ENDPOINT_P(m_in.m_source);
  fp = &fproc[proc_p];

  /* the rest need not be initialized */
}

/*===========================================================================*
 *				nested_fs_call				     *
 *===========================================================================*/
PUBLIC void nested_fs_call(m)
message *m;				/* request/reply message pointer */
{
/* Handle a nested call from a file system server.
 */
  int r;

  /* Save global variables of the current call */
  if ((r = push_globals()) != OK) {
	printf("VFS: error saving global variables in call %d from FS %d\n",
		m->m_type, m->m_source);
  } else {
	/* Initialize global variables for the nested call */
	set_globals(m);

	/* Perform the nested call - only getsysinfo() is allowed right now */
	if (call_nr == COMMON_GETSYSINFO) {
		r = do_getsysinfo();
	} else {
		printf("VFS: invalid nested call %d from FS %d\n", call_nr,
			who_e);

		r = ENOSYS;
	}

	/* Store the result, and restore original global variables */
	*m = m_out;

	pop_globals();
  }

  m->m_type = r;
}
