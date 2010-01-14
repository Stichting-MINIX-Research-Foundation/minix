#include "inc.h"

char buf_buf[BUF_SIZE + CLICK_SIZE];

endpoint_t ep_granter;
cp_grant_id_t gid;
char *buf;
int fid_send, fid_get;

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );

/*===========================================================================*
 *				    main				     *
 *===========================================================================*/
int main(int argc, char **argv)
{
	endpoint_t ep_self, ep_child;
	size_t size = BUF_SIZE;
	int i, r, pid;
	int status;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	/* Prepare work. */
	buf = (char*) CLICK_CEIL(buf_buf);
	fid_get = open(FIFO_GRANTOR, O_RDONLY);
	fid_send = open(FIFO_REQUESTOR, O_WRONLY);
	if(fid_get < 0 || fid_send < 0) {
		printf("REQUESTOR: can't open fifo files.\n");
		return 1;
	}

	/* Send the endpoint to the granter, in order to let him to
	 * create the grant.
	 */
	ep_self = getprocnr();
	write(fid_send, &ep_self, sizeof(ep_self));
	dprint("REQUESTOR: sending my endpoint: %d\n", ep_self);

	/* Get the granter's endpoint and gid. */
	read(fid_get, &ep_granter, sizeof(ep_granter));
	read(fid_get, &gid, sizeof(gid));
	dprint("REQUESTOR: getting granter's endpoint %d and gid %d\n",
		ep_granter, gid);

	/* Test MAP. */
	FIFO_WAIT(fid_get);
	r = sys_safemap(ep_granter, gid, 0, (long)buf, size, D, 1);
	if(r != OK) {
		printf("REQUESTOR: error in sys_safemap: %d\n", r);
		return 1;
	}
	CHECK_TEST("REQUESTOR", buf[0], BUF_START_GRANTOR, "MAP");
	buf[0] = BUF_START_REQUESTOR;
	r = sys_safeunmap(D, (long)buf);
	if(r != OK) {
		printf("REQUESTOR: error in sys_safeunmap: %d\n", r);
		return 1;
	}
	FIFO_NOTIFY(fid_send);

	/* Test UNMAP. */
	FIFO_WAIT(fid_get);
	CHECK_TEST("REQUESTOR", buf[0], BUF_START_REQUESTOR, "UNMAP");
	r = sys_safemap(ep_granter, gid, 0, (long)buf, size, D, 1);
	if(r != 0) {
		printf("REQUESTOR: error in sys_safemap: %d\n", r);
		return 1;
	}
	FIFO_NOTIFY(fid_send);

	/* Test REVOKE. */
	FIFO_WAIT(fid_get);
	CHECK_TEST("REQUESTOR", buf[0], BUF_START_GRANTOR, "REVOKE");
	buf[0] = BUF_START_REQUESTOR;
	FIFO_NOTIFY(fid_send);

	/* Test SMAP_COW. */
	FIFO_WAIT(fid_get);
	r = sys_safemap(ep_granter, gid, 0, (long)buf, size, D, 1);
	if(r != OK) {
		printf("REQUESTOR: error in sys_safemap: %d\n", r);
		return 1;
	}
	buf[0] = BUF_START_REQUESTOR;
	pid = fork();
	if(pid < 0) {
		printf("REQUESTOR: error in fork\n");
		return 1;
	}
	if(pid == 0) {
		exit(buf[0] != BUF_START_REQUESTOR);
	}
	FIFO_NOTIFY(fid_send);
	FIFO_WAIT(fid_get);
	ep_child = getnprocnr(pid);
	if ((r = sys_privctl(ep_child, SYS_PRIV_SET_USER, NULL)) != OK) {
		printf("REQUESTOR: unable to set privileges: %d\n", r);
		return 1;
	}
	if ((r = sys_privctl(ep_child, SYS_PRIV_ALLOW, NULL)) != OK) {
		printf("REQUESTOR: child process can't run: %d\n", r);
		return 1;
	}
	wait(&status);
	FIFO_NOTIFY(fid_send);
	CHECK_TEST("REQUESTOR", buf[0], BUF_START_GRANTOR, "SMAP_COW");
	CHECK_TEST("REQUESTOR", 1, WIFEXITED(status)
		&& (WEXITSTATUS(status) == 0), "SMAP_COW child");

	/* Test COW_SMAP. */
	FIFO_WAIT(fid_get);
	buf[0] = BUF_START_REQUESTOR;
	r = sys_safemap(ep_granter, gid, 0, (long)buf, size, D, 1);
	if(r != OK) {
		printf("REQUESTOR: error in sys_safemap: %d\n", r);
		return 1;
	}
	FIFO_NOTIFY(fid_send);
	FIFO_WAIT(fid_get);
	CHECK_TEST("REQUESTOR", buf[0], BUF_START_GRANTOR+1, "COW_SMAP");

	/* Test COW_SMAP2 (with COW safecopy). */
	FIFO_WAIT(fid_get);
	buf[0] = BUF_START_REQUESTOR;
	r = sys_safecopyto(ep_granter, gid, 0, (long)buf, size, D);
	if(r != OK) {
		printf("REQUESTOR: error in sys_safecopyto: %d\n", r);
		return 1;
	}
	r = sys_safemap(ep_granter, gid, 0, (long)buf, size, D, 1);
	if(r != OK) {
		printf("REQUESTOR: error in sys_safemap: %d\n", r);
		return 1;
	}
	FIFO_NOTIFY(fid_send);
	CHECK_TEST("REQUESTOR", buf[0], BUF_START_REQUESTOR, "COW_SMAP2");

	return 0;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Let SEF perform startup. */
  sef_startup();
}

