#include "inc.h"

char buf_buf[BUF_SIZE + CLICK_SIZE];

endpoint_t ep_granter;
cp_grant_id_t gid;
char *buf;
int fid_send, fid_get;

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );

/*===========================================================================*
 *			      read_write_buff				     *
 *===========================================================================*/
void read_write_buff(char* buff, int size, int is_write)
{
	int i;
	char c;

	if(size % CLICK_SIZE != 0) {
		panic("buff_size not page aligned");
	}
	if(is_write) {
	    for(i=0;i<size;i+=CLICK_SIZE) buff[i] = 1;
	}
	else {
	    for(i=0;i<size;i+=CLICK_SIZE) c = buff[i];
	}
}

/*===========================================================================*
 *				 exit_usage				     *
 *===========================================================================*/
void exit_usage(void)
{
	printf("Usage: requestor pages=<nr_pages> map=<0|1> write=<0|1>\n");
	exit(1);
}

/*===========================================================================*
 *				    main				     *
 *===========================================================================*/
int main(int argc, char **argv)
{
	endpoint_t ep_self, ep_child;
	size_t size = BUF_SIZE;
	int i, r, pid;
	int status;
	u64_t start, end, diff;
	double micros;
	char nr_pages_str[10], is_map_str[2], is_write_str[2];
	int nr_pages, is_map, is_write;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	/* Parse the command line. */
	r = env_get_param("pages", nr_pages_str, sizeof(nr_pages_str));
	errno = 0;
	nr_pages = atoi(nr_pages_str);
	if (r != OK || errno || nr_pages <=0) {
		exit_usage();
	}
	if(nr_pages > TEST_PAGE_NUM) {
		printf("REQUESTOR: too many pages. Max allowed: %d\n",
			TEST_PAGE_NUM);
		exit_usage();
	}
	r = env_get_param("map", is_map_str, sizeof(is_map_str));
	errno = 0;
	is_map = atoi(is_map_str);
	if (r != OK || errno || (is_map!=0 && is_map!=1)) {
		exit_usage();
	}
	r = env_get_param("write", is_write_str, sizeof(is_write_str));
	errno = 0;
	is_write = atoi(is_write_str);
	if (r != OK || errno || (is_write!=0 && is_write!=1)) {
		exit_usage();
	}
	printf("REQUESTOR: Running tests with pages=%d map=%d write=%d...\n",
		nr_pages, is_map, is_write);

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

	FIFO_WAIT(fid_get);
	diff = make64(0, 0);

	if(is_map) {
		/* Test safemap. */
		for(i=0;i<NR_TEST_ITERATIONS;i++) {
			read_tsc_64(&start);
			r = sys_safemap(ep_granter, gid, 0, (long)buf,
				nr_pages*CLICK_SIZE, D, 1);
			if(r != OK) {
				printf("REQUESTOR: safemap error: %d\n", r);
				return 1;
			}
			read_write_buff(buf, nr_pages*CLICK_SIZE, is_write);
			read_tsc_64(&end);
			diff = add64(diff, (sub64(end, start)));
			r = sys_safeunmap(D, (long)buf);
			if(r != OK) {
				printf("REQUESTOR: safeunmap error: %d\n", r);
				return 1;
			}
		}
		micros = ((double)tsc_64_to_micros(diff))
			/ (NR_TEST_ITERATIONS*nr_pages);
		REPORT_TEST("REQUESTOR", "SAFEMAP", micros);
	}
	else {
		/* Test safecopy. */
		for(i=0;i<NR_TEST_ITERATIONS;i++) {
			read_tsc_64(&start);
			r = sys_safecopyfrom(ep_granter, gid, 0, (long)buf,
				nr_pages*CLICK_SIZE, D);
			if(r != OK) {
				printf("REQUESTOR: safecopy error: %d\n", r);
				return 1;
			}
			read_write_buff(buf, nr_pages*CLICK_SIZE, is_write);
			read_tsc_64(&end);
			diff = add64(diff, (sub64(end, start)));
		}
		micros = ((double)tsc_64_to_micros(diff))
			/ (NR_TEST_ITERATIONS*nr_pages);
		REPORT_TEST("REQUESTOR", "SAFECOPY", micros);
	}

	FIFO_NOTIFY(fid_send);

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

