#include "utils.h"
#include <minix/config.h>
#include <timers.h>
#include "../../kernel/const.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h"

/*===========================================================================*
 *				get_proc_nr					     *
 *===========================================================================*/
PUBLIC int get_proc_nr(proc_nr, proc_name)
int *proc_nr;				/* store process number here */
char *proc_name;			/* lookup process by name */
{
  struct proc proc;
  message m;
  int s;
  if (proc_name != NULL) {		/* lookup by name */

  } else {				/* get own process number */
  	m.m_type = SYS_GETINFO;
  	m.I_REQUEST = GET_PROC;
  	m.I_PROC_NR = SELF;
  	m.I_KEY_LEN = SELF;
  	m.I_VAL_PTR = (char *) &proc;
  	if ((s=_taskcall(SYSTASK, SYS_GETINFO, &m)) != OK)
  		return(s);
  	*proc_nr = proc.p_nr;
  }
  return(OK);
}


