/*
 * This file contains the main function for the server. It waits for a request
 * and then send a response.
 */

#include "inc.h"
#include <minix/optset.h>

static struct optset optset_table[] = {
	{ "norock",	OPT_BOOL,   &opt.norock,	TRUE	},
	{ NULL,		0,	    NULL,		0	}
};

static int sef_cb_init_fresh(int __unused type,
	sef_init_info_t * __unused info)
{
	/* Initialize the iso9660fs server. */
	int i;

	/* Defaults */
	opt.norock = FALSE;

	/* If we have been given an options string, parse options here. */
	for (i = 1; i < env_argc - 1; i++)
		if (!strcmp(env_argv[i], "-o"))
			optset_parse(optset_table, env_argv[++i]);

	setenv("TZ","",1);              /* Used to calculate the time */

	lmfs_buf_pool(NR_BUFS);

	return OK;
}

static void sef_cb_signal_handler(int signo)
{
	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM) return;

	fsdriver_terminate();
}

static void sef_local_startup(void)
{
	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_restart(SEF_CB_INIT_RESTART_STATEFUL);

	/* Register signal callbacks. */
	sef_setcb_signal_handler(sef_cb_signal_handler);

	/* Let SEF perform startup. */
	sef_startup();
}

int main(int argc, char *argv[])
{
	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	fsdriver_task(&isofs_table);

	return 0;
}
