/* Filter driver - top layer - block interface */

/* This is a filter driver, which lays above disk driver, and forwards
 * messages between disk driver and its callers. The filter can detect
 * corrupted data (toggled by USE_CHECKSUM) and recover it (toggled
 * by USE_MIRROR). These two functions are independent from each other. 
 * The mirroring function requires two disks, on separate disk drivers.
 */

#include "inc.h"

#define _POSIX_SOURCE 1
#include <signal.h>

/* Global settings. */
int USE_CHECKSUM = 0;	/* enable checksumming */
int USE_MIRROR = 0;	/* enable mirroring */

int BAD_SUM_ERROR = 1;	/* bad checksums are considered a driver error */

int USE_SUM_LAYOUT = 0;	/* use checksumming layout on disk */
int NR_SUM_SEC = 8;	/* number of checksums per checksum sector */

int SUM_TYPE = ST_CRC;	/* use NIL, XOR, CRC, or MD5 */
int SUM_SIZE = 0;	/* size of the stored checksum */

int NR_RETRIES = 3;	/* number of times the request will be retried (N) */
int NR_RESTARTS = 3;	/* number of times a driver will be restarted (M) */
int DRIVER_TIMEOUT = 5;	/* timeout in seconds to declare a driver dead (T) */

int CHUNK_SIZE = 0;	/* driver requests will be vectorized at this size */

char MAIN_LABEL[LABEL_SIZE] = "";		/* main disk driver label */
char BACKUP_LABEL[LABEL_SIZE] = "";		/* backup disk driver label */
int MAIN_MINOR = -1;				/* main partition minor nr */
int BACKUP_MINOR = -1;				/* backup partition minor nr */

static struct optset optset_table[] = {
  { "label0",	OPT_STRING,	MAIN_LABEL,		LABEL_SIZE	},
  { "label1",	OPT_STRING,	BACKUP_LABEL,		LABEL_SIZE	},
  { "minor0",	OPT_INT,	&MAIN_MINOR,		10		},
  { "minor1",	OPT_INT,	&BACKUP_MINOR,		10		},
  { "sum_sec",	OPT_INT,	&NR_SUM_SEC,		10		},
  { "layout",	OPT_BOOL,	&USE_SUM_LAYOUT,	1		},
  { "nolayout",	OPT_BOOL,	&USE_SUM_LAYOUT,	0		},
  { "sum",	OPT_BOOL,	&USE_CHECKSUM,		1		},
  { "nosum",	OPT_BOOL,	&USE_CHECKSUM,		0		},
  { "mirror",	OPT_BOOL,	&USE_MIRROR,		1		},
  { "nomirror",	OPT_BOOL,	&USE_MIRROR,		0		},
  { "nil",	OPT_BOOL,	&SUM_TYPE,		ST_NIL		},
  { "xor",	OPT_BOOL,	&SUM_TYPE,		ST_XOR		},
  { "crc",	OPT_BOOL,	&SUM_TYPE,		ST_CRC		},
  { "md5",	OPT_BOOL,	&SUM_TYPE,		ST_MD5		},
  { "sumerr",	OPT_BOOL,	&BAD_SUM_ERROR,		1		},
  { "nosumerr",	OPT_BOOL,	&BAD_SUM_ERROR,		0		},
  { "retries",	OPT_INT,	&NR_RETRIES,		10		},
  { "N",	OPT_INT,	&NR_RETRIES,		10		},
  { "restarts",	OPT_INT,	&NR_RESTARTS,		10		},
  { "M",	OPT_INT,	&NR_RESTARTS,		10		},
  { "timeout",	OPT_INT,	&DRIVER_TIMEOUT,	10		},
  { "T",	OPT_INT,	&DRIVER_TIMEOUT,	10		},
  { "chunk",	OPT_INT,	&CHUNK_SIZE,		10		},
  { NULL,	0,		NULL,			0		}
};

/* Request message. */
static message m_in;
static endpoint_t who_e;			/* m_source */
static long req_id;				/* BDEV_ID */
static cp_grant_id_t grant_id;			/* BDEV_GRANT */

/* Data buffers. */
static char *buf_array, *buffer;		/* contiguous buffer */

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

/*===========================================================================*
 *				carry					     *
 *===========================================================================*/
static int carry(size_t size, int flag_rw)
{
	/* Carry data between caller proc and filter.
	 */

	if (flag_rw == FLT_WRITE)
		return sys_safecopyfrom(who_e, grant_id, 0,
			(vir_bytes) buffer, size);
	else
		return sys_safecopyto(who_e, grant_id, 0,
			(vir_bytes) buffer, size);
}

/*===========================================================================*
 *				vcarry					     *
 *===========================================================================*/
static int vcarry(int grants, iovec_t *iov, int flag_rw, size_t size)
{
	/* Carry data between caller proc and filter, through grant-vector.
	 */
	char *bufp;
	int i, r;
	size_t bytes;

	bufp = buffer;
	for(i = 0; i < grants && size > 0; i++) {
		bytes = MIN(size, iov[i].iov_size);

		if (flag_rw == FLT_WRITE)
			r = sys_safecopyfrom(who_e,
				(vir_bytes) iov[i].iov_addr, 0,
				(vir_bytes) bufp, bytes);
		else
			r = sys_safecopyto(who_e,
				(vir_bytes) iov[i].iov_addr, 0,
				(vir_bytes) bufp, bytes);

		if(r != OK)
			return r;

		bufp += bytes;
		size -= bytes;
	}

	return OK;
}

/*===========================================================================*
 *				do_rdwt					     *
 *===========================================================================*/
static int do_rdwt(int flag_rw)
{
	size_t size, size_ret;
	u64_t pos;
	int r;

	pos = make64(m_in.BDEV_POS_LO, m_in.BDEV_POS_HI);
	size = m_in.BDEV_COUNT;

	if (rem64u(pos, SECTOR_SIZE) != 0 || size % SECTOR_SIZE != 0) {
		printf("Filter: unaligned request from caller!\n");

		return EINVAL;
	}

	buffer = flt_malloc(size, buf_array, BUF_SIZE);

	if(flag_rw == FLT_WRITE)
		carry(size, flag_rw);

	reset_kills();

	for (;;) {
		size_ret = size;
		r = transfer(pos, buffer, &size_ret, flag_rw);
		if(r != RET_REDO)
			break;

#if DEBUG
		printf("Filter: transfer yielded RET_REDO, checking drivers\n");
#endif
		if((r = check_driver(DRIVER_MAIN)) != OK) break;
		if((r = check_driver(DRIVER_BACKUP)) != OK) break;
	}

	if(r == OK && flag_rw == FLT_READ)
		carry(size_ret, flag_rw);

	flt_free(buffer, size, buf_array);

	if (r != OK)
		return r;

	return size_ret;
}

/*===========================================================================*
 *				do_vrdwt				     *
 *===========================================================================*/
static int do_vrdwt(int flag_rw)
{
	size_t size, size_ret;
	int grants;
	int r, i;
	u64_t pos;
	iovec_t iov_proc[NR_IOREQS];

	/* Extract informations. */
	grants = m_in.BDEV_COUNT;
	if((r = sys_safecopyfrom(who_e, grant_id, 0, (vir_bytes) iov_proc,
		grants * sizeof(iovec_t))) != OK) {
		panic("copying in grant vector failed: %d", r);
	}

	pos = make64(m_in.BDEV_POS_LO, m_in.BDEV_POS_HI);
	for(size = 0, i = 0; i < grants; i++)
		size += iov_proc[i].iov_size;

	if (rem64u(pos, SECTOR_SIZE) != 0 || size % SECTOR_SIZE != 0) {
		printf("Filter: unaligned request from caller!\n");
		return EINVAL;
	}

	buffer = flt_malloc(size, buf_array, BUF_SIZE);

	if(flag_rw == FLT_WRITE)
		vcarry(grants, iov_proc, flag_rw, size);

	reset_kills();

	for (;;) {
		size_ret = size;
		r = transfer(pos, buffer, &size_ret, flag_rw);
		if(r != RET_REDO)
			break;

#if DEBUG
		printf("Filter: transfer yielded RET_REDO, checking drivers\n");
#endif
		if((r = check_driver(DRIVER_MAIN)) != OK) break;
		if((r = check_driver(DRIVER_BACKUP)) != OK) break;
	}

	if(r != OK) {
		flt_free(buffer, size, buf_array);
		return r;
	}

	if(flag_rw == FLT_READ)
		vcarry(grants, iov_proc, flag_rw, size_ret);

	flt_free(buffer, size, buf_array);

	return size_ret;
}

/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
static int do_ioctl(message *m)
{
	struct part_geom sizepart;

	switch(m->BDEV_REQUEST) {
	case DIOCSETP:
	case DIOCTIMEOUT:
	case DIOCOPENCT:
		/* These do not make sense for us. */
		return EINVAL;

	case DIOCGETP:
		memset(&sizepart, 0, sizeof(sizepart));

		/* The presented disk size is the raw partition size,
		 * corrected for space needed for checksums.
		 */
		sizepart.size = convert(get_raw_size());

		if(sys_safecopyto(who_e, (vir_bytes) grant_id, 0,
				(vir_bytes) &sizepart,
				sizeof(struct part_geom)) != OK) {
			printf("Filter: DIOCGETP safecopyto failed\n");
			return EIO;
		}
		break;

	default:
		printf("Filter: unknown ioctl request: %d!\n",
			m->BDEV_REQUEST);
		return EINVAL;
	}

	return OK;
}

/*===========================================================================*
 *				parse_arguments				     *
 *===========================================================================*/
static int parse_arguments(int argc, char *argv[])
{

	if(argc != 2)
		return EINVAL;

	optset_parse(optset_table, argv[1]);

	if (MAIN_LABEL[0] == 0 || MAIN_MINOR < 0 || MAIN_MINOR > 255)
		return EINVAL;
	if (USE_MIRROR && (BACKUP_LABEL[0] == 0 ||
			BACKUP_MINOR < 0 || BACKUP_MINOR > 255))
		return EINVAL;

	/* Checksumming implies a checksum layout. */
	if (USE_CHECKSUM)
		USE_SUM_LAYOUT = 1;

	/* Determine the checksum size for the chosen checksum type. */
	switch (SUM_TYPE) {
	case ST_NIL:
		SUM_SIZE = 4;	/* for the sector number */
		break;
	case ST_XOR:
		SUM_SIZE = 16;	/* compatibility */
		break;
	case ST_CRC:
		SUM_SIZE = 4;
		break;
	case ST_MD5:
		SUM_SIZE = 16;
		break;
	default:
		return EINVAL;
	}

	if (NR_SUM_SEC <= 0 || SUM_SIZE * NR_SUM_SEC > SECTOR_SIZE)
		return EINVAL;

#if DEBUG
	printf("Filter starting. Configuration:\n");
	printf("  USE_CHECKSUM :   %3s ", USE_CHECKSUM ? "yes" : "no");
	printf("  USE_MIRROR : %3s\n", USE_MIRROR ? "yes" : "no");

	if (USE_CHECKSUM) {
		printf("  BAD_SUM_ERROR :  %3s ",
			BAD_SUM_ERROR ? "yes" : "no");
		printf("  NR_SUM_SEC : %3d\n", NR_SUM_SEC);

		printf("  SUM_TYPE :       ");

		switch (SUM_TYPE) {
		case ST_NIL: printf("nil"); break;
		case ST_XOR: printf("xor"); break;
		case ST_CRC: printf("crc"); break;
		case ST_MD5: printf("md5"); break;
		}

		printf("   SUM_SIZE :   %3d\n", SUM_SIZE);
	}
	else printf("  USE_SUM_LAYOUT : %3s\n", USE_SUM_LAYOUT ? "yes" : "no");

	printf("  N : %3dx       M : %3dx        T : %3ds\n",
		NR_RETRIES, NR_RESTARTS, DRIVER_TIMEOUT);

	printf("  MAIN_LABEL / MAIN_MINOR : %19s / %d\n",
		MAIN_LABEL, MAIN_MINOR);
	if (USE_MIRROR) {
		printf("  BACKUP_LABEL / BACKUP_MINOR : %15s / %d\n",	
			BACKUP_LABEL, BACKUP_MINOR);
	}

#endif

	/* Convert timeout seconds to ticks. */
	DRIVER_TIMEOUT *= sys_hz();

	return OK;
}

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char *argv[])
{
	message m_out;
	int r, ipc_status;
	size_t size;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	for (;;) {
		/* Wait for request. */
		if(driver_receive(ANY, &m_in, &ipc_status) != OK) {
			panic("driver_receive failed");
		}

#if DEBUG2
		printf("Filter: got request %d from %d\n",
			m_in.m_type, m_in.m_source);
#endif

		if(m_in.m_source == DS_PROC_NR && is_ipc_notify(ipc_status)) {
			ds_event();
			continue;
		}

		who_e = m_in.m_source;
		req_id = m_in.BDEV_ID;
		grant_id = m_in.BDEV_GRANT;
		size = 0;

		/* Forword the request message to the drivers. */
		switch(m_in.m_type) {
		case BDEV_OPEN:		/* open/close is a noop for filter. */
		case BDEV_CLOSE:	r = OK;				break;
		case BDEV_READ:		r = do_rdwt(FLT_READ);		break;
		case BDEV_WRITE:	r = do_rdwt(FLT_WRITE);		break;
		case BDEV_GATHER:	r = do_vrdwt(FLT_READ);		break;
		case BDEV_SCATTER:	r = do_vrdwt(FLT_WRITE);	break;
		case BDEV_IOCTL:	r = do_ioctl(&m_in);		break;

		default:
			printf("Filter: ignoring unknown request %d from %d\n", 
				m_in.m_type, m_in.m_source);
			continue;
		}

#if DEBUG2
		printf("Filter: replying with code %d\n", r);
#endif

		/* Send back reply message. */
		m_out.m_type = BDEV_REPLY;
		m_out.BDEV_ID = req_id;
		m_out.BDEV_STATUS = r;
		send(who_e, &m_out);
	}

	return 0;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_restart(sef_cb_init_fresh);

	/* No live update support for now. */

	/* Register signal callbacks. */
	sef_setcb_signal_handler(sef_cb_signal_handler);

	/* Let SEF perform startup. */
	sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *UNUSED(info))
{
	/* Initialize the filter driver. */
	int r;

	r = parse_arguments(env_argc, env_argv);
	if(r != OK) {
		printf("Filter: wrong argument!\n");
		return 1;
	}

	if ((buf_array = flt_malloc(BUF_SIZE, NULL, 0)) == NULL)
		panic("no memory available");

	sum_init();

	driver_init();

	/* Subscribe to block driver events. */
	r = ds_subscribe("drv\\.blk\\..*", DSF_INITIAL | DSF_OVERWRITE);
	if(r != OK) {
		panic("Filter: can't subscribe to driver events");
	}

	/* Announce we are up! */
	blockdriver_announce(type);

	return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM) return;

	/* If so, shut down this driver. */
#if DEBUG
	printf("Filter: shutdown...\n");
#endif

	driver_shutdown();

	exit(0);
}

