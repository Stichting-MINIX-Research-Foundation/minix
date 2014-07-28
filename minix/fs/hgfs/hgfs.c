/* This file contains the implementation of the HGFS file system server.
 * The file system side is handled by libsffs, whereas the host communication
 * is handled by libhgfs. This file mainly contains the glue between them.
 *
 * The entry points into this file are:
 *   main		main program function
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include <minix/drivers.h>
#include <minix/sffs.h>
#include <minix/hgfs.h>
#include <minix/optset.h>

static struct sffs_params params;

static struct optset optset_table[] = {
  { "prefix",   OPT_STRING, params.p_prefix,       sizeof(params.p_prefix) },
  { "uid",      OPT_INT,    &params.p_uid,         10                      },
  { "gid",      OPT_INT,    &params.p_gid,         10                      },
  { "fmask",    OPT_INT,    &params.p_file_mask,   8                       },
  { "dmask",    OPT_INT,    &params.p_dir_mask,    8                       },
  { "icase",    OPT_BOOL,   &params.p_case_insens, TRUE                    },
  { "noicase",  OPT_BOOL,   &params.p_case_insens, FALSE                   },
  { NULL,       0,          NULL,                  0                       }
};

/*===========================================================================*
 *				sef_cb_init_fresh			     *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize this file server. Called at startup time.
 */
  const struct sffs_table *table;
  int i, r;

  /* Defaults */
  params.p_prefix[0] = 0;
  params.p_uid = 0;
  params.p_gid = 0;
  params.p_file_mask = 0755;
  params.p_dir_mask = 0755;
  params.p_case_insens = FALSE;

  /* If we have been given an options string, parse options from there. */
  for (i = 1; i < env_argc - 1; i++)
	if (!strcmp(env_argv[i], "-o"))
		optset_parse(optset_table, env_argv[++i]);

  /* Initialize the HGFS library. If this fails, exit immediately. */
  if ((r = hgfs_init(&table)) != OK) {
	if (r == EAGAIN)
		printf("HGFS: shared folders are disabled\n");
	else
		printf("HGFS: unable to initialize HGFS library (%d)\n", r);

	return r;
  }

  /* Now initialize the SFFS library. */
  if ((r = sffs_init("HGFS", table, &params)) != OK) {
	hgfs_cleanup();

	return r;
  }

  return OK;
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
/* Local SEF initialization.
 */

  /* Register init callback. */
  sef_setcb_init_fresh(sef_cb_init_fresh);

  /* Register signal callback. SFFS handles this. */
  sef_setcb_signal_handler(sffs_signal);

  sef_startup();
}

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char **argv)
{
/* The main function of this file server.
 */

  env_setargs(argc, argv);
  sef_local_startup();

  sffs_loop();

  hgfs_cleanup();

  return 0;
}
