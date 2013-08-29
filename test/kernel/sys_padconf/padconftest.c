/* Test for sys_padconf() */
#include <errno.h>
#include <stdio.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <minix/padconf.h>
#include <minix/drivers.h>
#include <assert.h>

static unsigned int failures = 0;

/*
 * padconf is only supported on ARM. On other systems sys_padconf() should
 * return -EBADREQUEST.
 */
static void test_badrequest(void)
{
#if !defined(__arm__)
	int r;

	r = sys_padconf(0xffffffff, 0xffffffff, 0xffffffff);
	if (r != -EBADREQUEST) {
		printf("Expected r=%d | Got r=%d\n", -EBADREQUEST, r);
		failures++;
	}
#endif
	return;
}

static void do_tests(void)
{
	test_badrequest();
}

static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	do_tests();

	/* The returned code will determine the outcome of the RS call, and
	 * thus the entire test. The actual error code does not matter.
	 */
	return (failures) ? EINVAL : 0;
}

static void sef_local_startup(void)
{
	sef_setcb_init_fresh(sef_cb_init_fresh);

	sef_startup();
}

int main(int argc, char **argv)
{
	env_setargs(argc, argv);

	sef_local_startup();

	return 0;
}
