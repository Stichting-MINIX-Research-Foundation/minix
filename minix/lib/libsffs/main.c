/* This file contains the SFFS initialization code and message loop.
 *
 * The entry points into this file are:
 *   sffs_init		initialization
 *   sffs_signal	signal handler
 *   sffs_loop		main message loop
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

/*===========================================================================*
 *				sffs_init				     *
 *===========================================================================*/
int sffs_init(char *name, const struct sffs_table *table,
  struct sffs_params *params)
{
/* Initialize this file server. Called at startup time.
 */
  int i;

  /* Make sure that the given path prefix doesn't end with a slash. */
  i = strlen(params->p_prefix);
  while (i > 0 && params->p_prefix[i - 1] == '/') i--;
  params->p_prefix[i] = 0;

  sffs_name = name;
  sffs_table = table;
  sffs_params = params;

  return OK;
}

/*===========================================================================*
 *				sffs_signal				     *
 *===========================================================================*/
void sffs_signal(int signo)
{

  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  dprintf(("%s: got SIGTERM\n", sffs_name));

  fsdriver_terminate();
}

/*===========================================================================*
 *				sffs_loop				     *
 *===========================================================================*/
void sffs_loop(void)
{
/* The main function of this file server. Libfsdriver does the real work.
 */

  fsdriver_task(&sffs_dtable);
}
