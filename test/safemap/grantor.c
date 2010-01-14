#include "inc.h"

char buf_buf[BUF_SIZE + CLICK_SIZE];

int fid_send, fid_get;

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );

/*===========================================================================*
 *				    main				     *
 *===========================================================================*/
int main(int argc, char **argv)
{
	endpoint_t ep_self, ep_requestor, ep_child;
	cp_grant_id_t gid;
	int i, r, pid;
	char *buf;
	int status;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	/* Prepare work. */
	buf = (char*) CLICK_CEIL(buf_buf);
	fid_send = open(FIFO_GRANTOR, O_WRONLY);
	fid_get = open(FIFO_REQUESTOR, O_RDONLY);
	if(fid_get < 0 || fid_send < 0) {
		printf("GRANTOR: can't open fifo files.\n");
		return 1;
	}

	/* Get the requestor's endpoint. */
	read(fid_get, &ep_requestor, sizeof(ep_requestor));
	dprint("GRANTOR: getting requestor's endpoint: %d\n", ep_requestor);

	/* Grant. */
	gid = cpf_grant_direct(ep_requestor, (long)buf, BUF_SIZE,
		CPF_READ | CPF_WRITE | CPF_MAP);
	ep_self = getprocnr();
	dprint("GRANTOR: sending my endpoint %d and gid %d\n", ep_self, gid);
	write(fid_send, &ep_self, sizeof(ep_self));
	write(fid_send, &gid, sizeof(gid));

	/* Test MAP. */
	buf[0] = BUF_START_GRANTOR;
	FIFO_NOTIFY(fid_send);
	FIFO_WAIT(fid_get);
	CHECK_TEST("GRANTOR", buf[0], BUF_START_REQUESTOR, "MAP");

	/* Test UNMAP. */
	buf[0] = BUF_START_GRANTOR;
	FIFO_NOTIFY(fid_send);
	FIFO_WAIT(fid_get);
	CHECK_TEST("GRANTOR", buf[0], BUF_START_GRANTOR, "UNMAP");

	/* Test REVOKE. */
	r = sys_saferevmap_gid(gid);
	if(r != OK) {
		printf("GRANTOR: error in sys_saferevmap_gid: %d\n", r);
		return 1;
	}
	buf[0] = BUF_START_GRANTOR+1;
	FIFO_NOTIFY(fid_send);
	FIFO_WAIT(fid_get);
	CHECK_TEST("GRANTOR", buf[0], BUF_START_GRANTOR+1, "REVOKE");

	/* Test SMAP_COW. */
	FIFO_NOTIFY(fid_send);
	FIFO_WAIT(fid_get);
	buf[0] = BUF_START_GRANTOR;
	FIFO_NOTIFY(fid_send);
	FIFO_WAIT(fid_get);
	CHECK_TEST("GRANTOR", buf[0], BUF_START_GRANTOR, "SMAP_COW");

	/* Test COW_SMAP. */
	r = sys_saferevmap_gid(gid);
	if(r != OK) {
		printf("GRANTOR: error in sys_saferevmap_gid: %d\n", r);
		return 1;
	}
	buf[0] = BUF_START_GRANTOR+1;
	pid = fork();
	if(pid < 0) {
		printf("GRANTOR: error in fork.\n");
		return 1;
	}
	if(pid == 0) {
		buf[0] = BUF_START_GRANTOR+2;
		exit(0);
	}
	FIFO_NOTIFY(fid_send);
	FIFO_WAIT(fid_get);
	ep_child = getnprocnr(pid);
	if ((r = sys_privctl(ep_child, SYS_PRIV_SET_USER, NULL)) != OK) {
		printf("GRANTOR: unable to set privileges: %d\n", r);
		return 1;
	}
	if ((r = sys_privctl(ep_child, SYS_PRIV_ALLOW, NULL)) != OK) {
		printf("GRANTOR: child process can't run: %d\n", r);
		return 1;
	}
	wait(&status);
	FIFO_NOTIFY(fid_send);
	CHECK_TEST("GRANTOR", buf[0], BUF_START_GRANTOR+1, "COW_SMAP");

	/* Test COW_SMAP2 (with COW safecopy). */
	r = sys_saferevmap_gid(gid);
	if(r != OK) {
		printf("GRANTOR: error in sys_saferevmap_gid: %d\n", r);
		return 1;
	}
	FIFO_NOTIFY(fid_send);
	FIFO_WAIT(fid_get);
	CHECK_TEST("GRANTOR", buf[0], BUF_START_REQUESTOR, "COW_SMAP2");

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

