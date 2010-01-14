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

	/* Test safemap. */
	buf[0] = 0;
	FIFO_NOTIFY(fid_send);
	FIFO_WAIT(fid_get);

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

