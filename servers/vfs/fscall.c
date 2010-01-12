/* This file handles nested counter-request calls to VFS sent by file system
 * (FS) servers in response to VFS requests.
 *
 * The entry points into this file are
 *   nested_fs_call	perform a nested call from a file system server
 */

#include "fs.h"
#include "fproc.h"
#include <string.h>
#include <assert.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>

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
  char g_user_fullpath[PATH_MAX+1];	/* path to look up */
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
	return EPERM;

  globals[depth].g_fp = fp;
  globals[depth].g_m_in = m_in;
  globals[depth].g_m_out = m_out;
  globals[depth].g_who_e = who_e;
  globals[depth].g_who_p = who_p;
  globals[depth].g_call_nr = call_nr;
  globals[depth].g_super_user = super_user;

  /* XXX is it safe to strcpy this? */
  assert(sizeof(globals[0].g_user_fullpath) == sizeof(user_fullpath));
  memcpy(globals[depth].g_user_fullpath, user_fullpath, sizeof(user_fullpath));

  /* err_code is not used across blocking calls */

  depth++;

  return OK;
}

/*===========================================================================*
 *				pop_globals				     *
 *===========================================================================*/
PRIVATE void pop_globals()
{
/* Restore the global variables of a call from the globals stack.
 */

  if (depth == 0)
	panic("VFS", "Popping from empty globals stack!", NO_NUM);

  depth--;

  fp = globals[depth].g_fp;
  m_in = globals[depth].g_m_in;
  m_out = globals[depth].g_m_out;
  who_e = globals[depth].g_who_e;
  who_p = globals[depth].g_who_p;
  call_nr = globals[depth].g_call_nr;
  super_user = globals[depth].g_super_user;

  memcpy(user_fullpath, globals[depth].g_user_fullpath, sizeof(user_fullpath));
}

/*===========================================================================*
 *				set_globals				     *
 *===========================================================================*/
PRIVATE void set_globals(m)
message *m;				/* request message */
{
/* Initialize global variables based on a request message.
 */

  m_in = *m;
  who_e = m_in.m_source;
  who_p = _ENDPOINT_P(who_e);
  call_nr = m_in.m_type;
  fp = &fproc[who_p];
  super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);
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

	/* Perform the nested call */
	if (call_nr < 0 || call_nr >= NCALLS) {
		printf("VFS: invalid nested call %d from FS %d\n", call_nr,
			who_e);

		r = ENOSYS;
	} else {
#if ENABLE_SYSCALL_STATS
		calls_stats[call_nr]++;
#endif

		r = (*call_vec[call_nr])();
	}

	/* Store the result, and restore original global variables */
	*m = m_out;

	pop_globals();
  }

  m->m_type = r;
}
