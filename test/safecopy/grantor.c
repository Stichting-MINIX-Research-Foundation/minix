#include "inc.h"

char buf_buf[BUF_SIZE + CLICK_SIZE];

/* SEF functions and variables. */
static void sef_local_startup(void);

/*===========================================================================*
 *				    main				     *
 *===========================================================================*/
int main(int argc, char **argv)
{
	endpoint_t ep_self, ep_requestor;
	int fid_send, fid_get;
	cp_grant_id_t gid;
	char *buf;
	int i;

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
	buf[0] = BUF_START;

	/* Get the requestor's endpoint. */
	read(fid_get, &ep_requestor, sizeof(ep_requestor));
	dprint("GRANTOR: getting requestor's endpoint: %d\n", ep_requestor);

	/* Grant. */
	gid = cpf_grant_direct(ep_requestor, (long)buf, BUF_SIZE,
		CPF_READ | CPF_WRITE);
	ep_self = getprocnr();
	dprint("GRANTOR: sending my endpoint %d and gid %d\n", ep_self, gid);
	write(fid_send, &ep_self, sizeof(ep_self));
	write(fid_send, &gid, sizeof(gid));

	/* Wait till requestor is done. */
	FIFO_WAIT(fid_get);

	return 0;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Let SEF perform startup. */
  sef_startup();
}

