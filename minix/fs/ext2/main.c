#include "fs.h"
#include "buf.h"
#include "inode.h"
#include <string.h>
#include <minix/optset.h>

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

EXTERN int env_argc;
EXTERN char **env_argv;

static struct optset optset_table[] = {
  { "sb",		OPT_INT,    &opt.block_with_super,	0	},
  { "orlov",		OPT_BOOL,   &opt.use_orlov,		TRUE    },
  { "oldalloc",		OPT_BOOL,   &opt.use_orlov,		FALSE   },
  { "mfsalloc",		OPT_BOOL,   &opt.mfsalloc,		TRUE    },
  { "reserved",		OPT_BOOL,   &opt.use_reserved_blocks,	TRUE    },
  { "prealloc",		OPT_BOOL,   &opt.use_prealloc, 		TRUE	},
  { "noprealloc",	OPT_BOOL,   &opt.use_prealloc, 		FALSE	},
  { NULL,		0,	    NULL,			0	}
};

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
int main(int argc, char *argv[])
{
/* This is the main routine of this service. */
  unsigned short test_endian = 1;

  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();

  le_CPU = (*(unsigned char *) &test_endian == 0 ? 0 : 1);

  /* Server isn't tested on big endian CPU */
  ASSERT(le_CPU == 1);

  /* The fsdriver library does the actual work here. */
  fsdriver_task(&ext2_table);

  return 0;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the Minix file server. */
  int i;

  /* Defaults */
  opt.use_orlov = TRUE;
  opt.mfsalloc = FALSE;
  opt.use_reserved_blocks = FALSE;
  opt.block_with_super = 0;
  opt.use_prealloc = FALSE;

  /* If we have been given an options string, parse options from there. */
  for (i = 1; i < env_argc - 1; i++)
	if (!strcmp(env_argv[i], "-o"))
		optset_parse(optset_table, env_argv[++i]);

  lmfs_may_use_vmcache(1);

  /* Init inode table */
  for (i = 0; i < NR_INODES; ++i) {
	inode[i].i_count = 0;
	cch[i] = 0;
  }

  init_inode_cache();

  /* just a small number before we find out the block size at mount time */
  lmfs_buf_pool(10);

  return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  fs_sync();

  fsdriver_terminate();
}
