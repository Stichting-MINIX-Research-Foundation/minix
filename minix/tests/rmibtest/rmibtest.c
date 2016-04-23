/* Remote MIB (RMIB) test service - by D.C. van Moolenbroek */
/*
 * This test is a good start, but not an exhaustive coverage test for all
 * possible failure cases.  The reason for that is mainly that there are
 * various scenarios that we cannot generate without implementing our own local
 * bogus RMIB code.  Adding that is something for later - TODO.
 */
#include <minix/drivers.h>
#include <minix/sysctl.h>
#include <minix/rmib.h>

static int running;

/* The following is a copy of the minix.test subtree in the MIB service. */
static char test_string[16], test_struct[12];

static struct rmib_node minix_test_secret_table[] = {
/* 0*/	[SECRET_VALUE]		= RMIB_INT(RMIB_RO, 12345, "value",
				    "The combination to my luggage"),
};

static struct rmib_node minix_test_table[] = {
/* 0*/	[TEST_INT]		= RMIB_INT(RMIB_RO | CTLFLAG_HEX, 0x01020304,
				    "int", "Value test field"),
/* 1*/	[TEST_BOOL]		= RMIB_BOOL(RMIB_RW, 0, "bool",
				    "Boolean test field"),
/* 2*/	[TEST_QUAD]		= RMIB_QUAD(RMIB_RW, 0, "quad",
				    "Quad test field"),
/* 3*/	[TEST_STRING]		= RMIB_STRING(RMIB_RW, test_string, "string",
				    "String test field"),
/* 4*/	[TEST_STRUCT]		= RMIB_STRUCT(RMIB_RW, sizeof(test_struct),
				    test_struct, "struct",
				    "Structure test field"),
/* 5*/	[TEST_PRIVATE]		= RMIB_INT(RMIB_RW | CTLFLAG_PRIVATE, -5375,
				    "private", "Private test field"),
/* 6*/	[TEST_ANYWRITE]		= RMIB_INT(RMIB_RW | CTLFLAG_ANYWRITE, 0,
				    "anywrite", "AnyWrite test field"),
/* 7*/	[TEST_DYNAMIC]		= RMIB_INT(RMIB_RO, 0, "deleteme",
				    "This node will be destroyed"),
/* 8*/	[TEST_SECRET]		= RMIB_NODE(RMIB_RO | CTLFLAG_PRIVATE,
				    minix_test_secret_table, "secret",
				    "Private subtree"),
/* 9*/	[TEST_PERM]		= RMIB_INT(RMIB_RO, 1, "permanent", NULL),
/*10*/	[TEST_DESTROY1]		= RMIB_INT(RMIB_RO, 123, "destroy1", NULL),
/*11*/	[TEST_DESTROY2]		= RMIB_INT(RMIB_RO, 456, "destroy2",
				    "This node will be destroyed"),
};

static struct rmib_node minix_test = RMIB_NODE(RMIB_RW | CTLFLAG_HIDDEN,
    minix_test_table, "test", "Test87 testing ground");
/* Here ends the copy of the minix.test subtree in the MIB service. */

static struct rmib_node test_table[] = {
};

static struct rmib_node test_rnode = RMIB_NODE(RMIB_RO, test_table, "test",
    "Test node");

static int value = 5375123;

static ssize_t test_func(struct rmib_call *, struct rmib_node *,
    struct rmib_oldp *, struct rmib_newp *);

/* No defined constants because userland will access these by name anyway. */
static struct rmib_node minix_rtest_table[] = {
	[1]			= RMIB_INTPTR(RMIB_RW, &value, "int",
				    "Test description"),
	[2]			= RMIB_FUNC(CTLTYPE_INT | RMIB_RW, sizeof(int),
				    test_func, "func", "Test function"),
};

static struct rmib_node minix_rtest = RMIB_NODE(RMIB_RO, minix_rtest_table,
    "rtest", "Remote test subtree");

/*
 * Test function that deflects reads and writes to its sibling node.  Not a
 * super useful thing to do, but a decent test of functionality regardless.
 */
static ssize_t
test_func(struct rmib_call * call, struct rmib_node * node,
	struct rmib_oldp * oldp, struct rmib_newp * newp)
{

	return rmib_readwrite(call, &minix_rtest_table[1], oldp, newp);
}

/*
 * Attempt to perform registrations that should be rejected locally, and thus
 * result in failure immediately.  Unfortunately, we cannot verify that the MIB
 * service also verifies these aspects remotely, at least without talking to it
 * directly.
 */
static void
test_local_failures(void)
{
	int r, mib[CTL_SHORTNAME + 1];

	memset(mib, 0, sizeof(mib));

	/* Test an empty path. */
	if ((r = rmib_register(mib, 0, &test_rnode)) != EINVAL)
		panic("registering remote MIB subtree yielded: %d", r);

	/* Test a path that is too long. */
	if ((r = rmib_register(mib, CTL_SHORTNAME + 1, &test_rnode)) != EINVAL)
		panic("registering remote MIB subtree yielded: %d", r);

	/* Test a mount point that is not a node-type (parent) node. */
	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = TEST_INT;
	if ((r = rmib_register(mib, 3, &minix_test_table[TEST_INT])) != EINVAL)
		panic("registering remote MIB subtree yielded: %d", r);
}

/*
 * Perform a number of registrations that will not be accepted by the MIB
 * service.  We will never know, but the userland test script can verify the
 * difference by comparing the number of remotes before and after.
 */
static void
test_remote_failures(void)
{
	int r, mib[CTL_SHORTNAME];

	/* Test an existing one-node path. */
	mib[0] = CTL_KERN;
	if ((r = rmib_register(mib, 1, &test_rnode)) != OK)
		panic("unable to register remote MIB subtree: %d", r);
	rmib_reset();

	/* Test a path in which a non-final component does not exist. */
	mib[1] = CREATE_BASE - 1; /* probably as safe as it gets.. */
	mib[2] = 0;
	if ((r = rmib_register(mib, 3, &test_rnode)) != OK)
		panic("unable to register remote MIB subtree: %d", r);
	rmib_reset();

	/* Test a path in which a non-final component is not a parent node. */
	mib[1] = KERN_OSTYPE;
	if ((r = rmib_register(mib, 3, &test_rnode)) != OK)
		panic("unable to register remote MIB subtree: %d", r);
	rmib_reset();

	/* Test a path in which a non-final component is a meta-identifier. */
	mib[1] = CTL_QUERY;
	if ((r = rmib_register(mib, 3, &test_rnode)) != OK)
		panic("unable to register remote MIB subtree: %d", r);
	rmib_reset();

	/* Test a path in which the final component is a meta-identifier. */
	if ((r = rmib_register(mib, 2, &test_rnode)) != OK)
		panic("unable to register remote MIB subtree: %d", r);
	rmib_reset();

	/* Test a path in which the final component identifies a non-parent. */
	mib[1] = KERN_OSTYPE;
	if ((r = rmib_register(mib, 2, &test_rnode)) != OK)
		panic("unable to register remote MIB subtree: %d", r);
	rmib_reset();

	/* Test a path with unacceptable flags for the final component. */
	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = TEST_SECRET;
	if ((r = rmib_register(mib, 3, &test_rnode)) != OK)
		panic("unable to register remote MIB subtree: %d", r);
	rmib_reset();

	/* Test a path of which the name, but not the ID, already exists. */
	mib[1] = CREATE_BASE - 1;
	if ((r = rmib_register(mib, 2, &test_rnode)) != OK)
		panic("unable to register remote MIB subtree: %d", r);
	/*
	 * Do NOT call rmib_reset() anymore now: we want to let the MIB service
	 * get the name from us.
	 */
}

static int
init(int type __unused, sef_init_info_t * info __unused)
{
	const int new_mib[] = { CTL_MINIX, CREATE_BASE - 2 };
	const int shadow_mib[] = { CTL_MINIX, MINIX_TEST };
	int r;

	test_local_failures();

	test_remote_failures();

	/*
	 * We must now register our new test tree before shadowing minix.test,
	 * because if any of the previous requests actually did succeed, the
	 * next registration will be rejected (ID 0 already in use) and no
	 * difference would be detected because of "successful" shadowing.
	 */
	r = rmib_register(new_mib, __arraycount(new_mib), &minix_rtest);
	if (r != OK)
		panic("unable to register remote MIB subtree: %d", r);

	r = rmib_register(shadow_mib, __arraycount(shadow_mib), &minix_test);
	if (r != OK)
		panic("unable to register remote MIB subtree: %d", r);

	running = TRUE;

	return OK;
}

static void
cleanup(void)
{
	int r;

	if ((r = rmib_deregister(&minix_rtest)) != OK)
		panic("unable to deregister: %d", r);
	if ((r = rmib_deregister(&minix_test)) != OK)
		panic("unable to deregister: %d", r);

	/*
	 * TODO: the fact that the MIB service can currently not detect the
	 * death of other services is creating somewhat of a problem here: if
	 * we deregister shortly before exiting, the asynchronous deregister
	 * requests may not be delivered before we actually exit (and take our
	 * asynsend table with us), and leave around the remote subtrees until
	 * a user process tries accessing them.  We work around this here by
	 * delaying the exit by half a second - shorter than RS's timeout, but
	 * long enough to allow deregistration.
	 */
	sys_setalarm(sys_hz() / 2, 0);

	running = FALSE;
}

static void
got_signal(int sig)
{

	if (sig == SIGTERM && running)
		cleanup();
}

int
main(void)
{
	message m;
	int r, ipc_status;

	sef_setcb_init_fresh(init);
	sef_setcb_signal_handler(got_signal);

	sef_startup();

	for (;;) {
		r = sef_receive_status(ANY, &m, &ipc_status);

		if (r != OK)
			panic("sef_receive_status failed: %d", r);

		if (m.m_source == CLOCK && is_ipc_notify(ipc_status))
			break; /* the intended exit path; see above */
		if (m.m_source == MIB_PROC_NR)
			rmib_process(&m, ipc_status);
	}

	return EXIT_SUCCESS;
}
