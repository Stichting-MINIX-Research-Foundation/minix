#include "inc.h"

char buf_buf[BUF_SIZE + CLICK_SIZE];

endpoint_t ep_granter;
int gid;
char *buf;

/*===========================================================================*
 *				    test				     *
 *===========================================================================*/
int test(size_t size)
{
	u32_t low1, high1;
	u32_t low2, high2;
	int r;

	/* Timing. */
	read_tsc(&high1, &low1);
	r = sys_safecopyfrom(ep_granter, gid, 0, (long)buf, size);
	read_tsc(&high2, &low2);
	if(r != OK) {
		printf("REQUESTOR: error in safecopy: %d\n", r);
		return r;
	}
	printf("REQUESTOR: SAFECOPY 0x%-8x - %d\n", size, low2 -  low1);

	/* Test. */
	if(buf[0] != BUF_START) {
		printf("REQUESTOR: error in safecopy!\n");
		printf("	size, value: %d, %d\n", size, buf[0]);
		return r;
	}

	return OK;
}

/* SEF functions and variables. */
static void sef_local_startup(void);

/*===========================================================================*
 *				    main				     *
 *===========================================================================*/
int main(int argc, char **argv)
{
	endpoint_t ep_self;
	int fid_send, fid_get;
	int i;

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

	/* Sending the endpoint to the granter, in order to let him
	 * create the grant.
	 */
	ep_self = getprocnr();
	write(fid_send, &ep_self, sizeof(ep_self));
	dprint("REQUESTOR: sending my endpoint: %d\n", ep_self);

	/* Getting the granter's endpoint and gid. */
	read(fid_get, &ep_granter, sizeof(ep_granter));
	read(fid_get, &gid, sizeof(gid));
	dprint("REQUESTOR: getting granter's endpoint %d and gid %d\n",
		ep_granter, gid);

	/* Test SAFECOPY. */
	for(i = 0; i <= TEST_PAGE_SHIFT; i++) {
		if(test(1 << i) != OK)
			break;
	}

	/* Notify grantor we are done. */
	FIFO_NOTIFY(fid_send);

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

