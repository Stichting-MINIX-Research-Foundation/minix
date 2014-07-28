/* This file contains the implementation of the VBFS file system server. */
/*
 * The architecture of VBFS can be sketched as follows:
 *
 *     +-------------+
 *     |    VBFS     |		This file
 *     +-------------+
 *            |
 *     +-------------+
 *     |   libsffs   |		Shared Folder File System library
 *     +-------------+
 *            |
 *     +-------------+
 *     |  libvboxfs  |		VirtualBox File System library
 *     +-------------+
 *            |
 *     +-------------+
 *     | libsys/vbox |		VBOX driver interfacing library
 *     +-------------+
 *  --------  |  -------- 		(process boundary)
 *     +-------------+
 *     | VBOX driver |		VirtualBox backdoor driver
 *     +-------------+
 *  ========  |  ======== 		(system boundary)
 *     +-------------+
 *     | VirtualBox  |		The host system
 *     +-------------+
 *
 * The interfaces between the layers are defined in the following header files:
 *   minix/sffs.h:	shared between VBFS, libsffs, and libvboxfs
 *   minix/vboxfs.h:	shared between VBFS and libvboxfs
 *   minix/vbox.h:	shared between libvboxfs and libsys/vbox
 *   minix/vboxtype.h:	shared between libvboxfs, libsys/vbox, and VBOX
 *   minix/vboxif.h:	shared between libsys/vbox and VBOX
 */

#include <minix/drivers.h>
#include <minix/sffs.h>
#include <minix/vboxfs.h>
#include <minix/optset.h>

static char share[PATH_MAX];
static struct sffs_params params;

static struct optset optset_table[] = {
	{ "share",  OPT_STRING, share,               sizeof(share)           },
	{ "prefix", OPT_STRING, params.p_prefix,     sizeof(params.p_prefix) },
	{ "uid",    OPT_INT,    &params.p_uid,       10                      },
	{ "gid",    OPT_INT,    &params.p_gid,       10                      },
	{ "fmask",  OPT_INT,    &params.p_file_mask, 8                       },
	{ "dmask",  OPT_INT,    &params.p_dir_mask,  8                       },
	{ NULL,     0,          NULL,                0                       }
};

/*
 * Initialize this file server. Called at startup time.
 */
static int
init(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	const struct sffs_table *table;
	int i, r, roflag;

	/* Set defaults. */
	share[0] = 0;
	params.p_prefix[0] = 0;
	params.p_uid = 0;
	params.p_gid = 0;
	params.p_file_mask = 0755;
	params.p_dir_mask = 0755;
	params.p_case_insens = FALSE;

	/* We must have been given an options string. Parse the options. */
	for (i = 1; i < env_argc - 1; i++)
		if (!strcmp(env_argv[i], "-o"))
			optset_parse(optset_table, env_argv[++i]);

	/* A share name is required. */
	if (!share[0]) {
		printf("VBFS: no shared folder share name specified\n");

		return EINVAL;
	}

	/* Initialize the VBOXFS library. If this fails, exit immediately. */
	r = vboxfs_init(share, &table, &params.p_case_insens, &roflag);

	if (r != OK) {
		if (r == ENOENT)
			printf("VBFS: the given share does not exist\n");
		else
			printf("VBFS: unable to initialize VBOXFS (%d)\n", r);

		return r;
	}

	/* Now initialize the SFFS library. */
	if ((r = sffs_init("VBFS", table, &params)) != OK) {
		vboxfs_cleanup();

		return r;
	}

	return OK;
}

/*
 * Local SEF initialization.
 */
static void
sef_local_startup(void)
{

	/* Register initialization callback. */
	sef_setcb_init_fresh(init);

	/* Register signal callback. SFFS handles this. */
	sef_setcb_signal_handler(sffs_signal);

	sef_startup();
}

/*
 * The main function of this file server.
 */
int
main(int argc, char **argv)
{

	/* Start up. */
	env_setargs(argc, argv);
	sef_local_startup();

	/* Let SFFS do the actual work. */
	sffs_loop();

	/* Clean up. */
	vboxfs_cleanup();

	return EXIT_SUCCESS;
}
