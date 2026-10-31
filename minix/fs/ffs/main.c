#include "fs.h"
#include "inode.h"

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

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

  /* This server only supports native-little-endian images (UFS1 and UFS2). */
  ASSERT(le_CPU == 1);

  /* The fsdriver library does the actual work here. */
  fsdriver_task(&ffs_table);

  return 0;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
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
/* Initialize the file server. */

  /* Whether the VM second-level (inode-keyed) cache can be used is
   * decided at mount time, when the block (fragment) size is known;
   * see fs_mount(). */

  init_inode_cache();

  /* Just a small number before we find out the block size at mount time. */
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
