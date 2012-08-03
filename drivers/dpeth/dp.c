/*
**  File:	dp.c	Version 1.01,	Oct. 17, 2007
**  Original:	eth.c	Version 1.00,	Jan. 14, 1997
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  This file contains the ethernet device driver main task.
**  It has to be integrated with the board specific drivers.
**  It is a rewriting of Minix 2.0.0 ethernet driver (dp8390.c)
**  to remove bord specific code. It should operate (I hope)
**  with any board driver.
*/

#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <minix/endpoint.h>
#include <minix/ds.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>

#include "dp.h"

/*
**  Local data
*/
static dpeth_t de_state;
static int de_instance;

typedef struct dp_conf {	/* Configuration description structure */
  port_t dpc_port;
  int dpc_irq;
  phys_bytes dpc_mem;
} dp_conf_t;

/* Device default configuration */
#define DP_CONF_NR 3
static dp_conf_t dp_conf[DP_CONF_NR] = {
  /* I/O port, IRQ, Buff addr, Env. var */
  {     0x300,   5,   0xC8000,  },
  {     0x280,  10,   0xCC000,  },
  {     0x000,   0,   0x00000,  },
};

static char CopyErrMsg[] = "unable to read/write user data";
static char RecvErrMsg[] = "netdriver_receive failed";
static char SendErrMsg[] = "send failed";
static char SizeErrMsg[] = "illegal packet size";
static char TypeErrMsg[] = "illegal message type";
static char DevName[] = "eth#?";

/*
**  Name:	void reply(dpeth_t *dep, int err, int m_type)
**  Function:	Fills a reply message and sends it.
*/
static void reply(dpeth_t * dep)
{
  message reply;
  int r, flags;

  flags = DL_NOFLAGS;
  if (dep->de_flags & DEF_ACK_SEND) flags |= DL_PACK_SEND;
  if (dep->de_flags & DEF_ACK_RECV) flags |= DL_PACK_RECV;

  reply.m_type = DL_TASK_REPLY;
  reply.DL_FLAGS = flags;
  reply.DL_COUNT = dep->de_read_s;

  DEBUG(printf("\t reply %d (%lx)\n", reply.m_type, reply.DL_FLAGS));

  if ((r = send(dep->de_client, &reply)) != OK)
	panic(SendErrMsg, r);

  dep->de_read_s = 0;
  dep->de_flags &= NOT(DEF_ACK_SEND | DEF_ACK_RECV);

  return;
}

/*
**  Name:	void dp_confaddr(dpeth_t *dep)
**  Function:	Checks environment for a User defined ethernet address.
*/
static void dp_confaddr(dpeth_t * dep)
{
  static char ea_fmt[] = "x:x:x:x:x:x";
  char ea_key[16];
  int ix;
  long val;

  strlcpy(ea_key, "DPETH0_EA", sizeof(ea_key));
  ea_key[5] += de_instance;

  for (ix = 0; ix < SA_ADDR_LEN; ix++) {
	val = dep->de_address.ea_addr[ix];
	if (env_parse(ea_key, ea_fmt, ix, &val, 0x00L, 0xFFL) != EP_SET)
		break;
	dep->de_address.ea_addr[ix] = val;
  }

  if (ix != 0 && ix != SA_ADDR_LEN)
	/* It's all or nothing, force a panic */
	env_parse(ea_key, "?", 0, &val, 0L, 0L);
  return;
}

/*
**  Name:	void update_conf(dpeth_t *dep, dp_conf_t *dcp)
**  Function:	Gets the default settings from 'dp_conf' table and
**  		modifies them from the environment.
*/
static void update_conf(dpeth_t * dep, const dp_conf_t * dcp)
{
  static char dpc_fmt[] = "x:d:x";
  char ec_key[16];
  long val;

  strlcpy(ec_key, "DPETH0", sizeof(ec_key));
  ec_key[5] += de_instance;

  dep->de_mode = DEM_SINK;
  val = dcp->dpc_port;		/* Get I/O port address */
  switch (env_parse(ec_key, dpc_fmt, 0, &val, 0x000L, 0x3FFL)) {
      case EP_OFF:	dep->de_mode = DEM_DISABLED;	break;
      case EP_ON:
      case EP_SET:	dep->de_mode = DEM_ENABLED;	break;
  }
  dep->de_base_port = val;

  val = dcp->dpc_irq | DEI_DEFAULT;	/* Get Interrupt line (IRQ) */
  env_parse(ec_key, dpc_fmt, 1, &val, 0L, (long) NR_IRQ_VECTORS - 1);
  dep->de_irq = val;

  val = dcp->dpc_mem;		/* Get shared memory address */
  env_parse(ec_key, dpc_fmt, 2, &val, 0L, LONG_MAX);
  dep->de_linmem = val;

  return;
}

/*
**  Name:	void do_dump(message *mp)
**  Function:	Displays statistics on screen (SFx key from console)
*/
static void do_dump(const message *mp)
{
  dpeth_t *dep;

  dep = &de_state;

  printf("\n\n");

  if (dep->de_mode == DEM_DISABLED) return;

  printf("%s statistics:\t\t", dep->de_name);

  /* Network interface status  */
  printf("Status: 0x%04x (%d)\n\n", dep->de_flags, dep->de_int_pending);

  (*dep->de_dumpstatsf) (dep);

  /* Transmitted/received bytes */
  printf("Tx bytes:%10ld\t", dep->bytes_Tx);
  printf("Rx bytes:%10ld\n", dep->bytes_Rx);

  /* Transmitted/received packets */
  printf("Tx OK:     %8ld\t", dep->de_stat.ets_packetT);
  printf("Rx OK:     %8ld\n", dep->de_stat.ets_packetR);

  /* Transmit/receive errors */
  printf("Tx Err:    %8ld\t", dep->de_stat.ets_sendErr);
  printf("Rx Err:    %8ld\n", dep->de_stat.ets_recvErr);

  /* Transmit unnerruns/receive overrruns */
  printf("Tx Und:    %8ld\t", dep->de_stat.ets_fifoUnder);
  printf("Rx Ovr:    %8ld\n", dep->de_stat.ets_fifoOver);

  /* Transmit collisions/receive CRC errors */
  printf("Tx Coll:   %8ld\t", dep->de_stat.ets_collision);
  printf("Rx CRC:    %8ld\n", dep->de_stat.ets_CRCerr);

  return;
}

/*
**  Name:	void get_userdata_s(int user_proc, vir_bytes user_addr, int count, void *loc_addr)
**  Function:	Copies data from user area.
*/
static void get_userdata_s(int user_proc, cp_grant_id_t grant,
	vir_bytes offset, int count, void *loc_addr)
{
  int rc;
  vir_bytes len;

  len = (count > IOVEC_NR ? IOVEC_NR : count) * sizeof(iovec_t);
  if ((rc = sys_safecopyfrom(user_proc, grant, 0, (vir_bytes)loc_addr, len)) != OK)
	panic(CopyErrMsg, rc);
  return;
}

/*
**  Name:	void do_first_init(dpeth_t *dep, dp_conf_t *dcp);
**  Function:	Init action to setup task
*/
static void do_first_init(dpeth_t *dep, const dp_conf_t *dcp)
{

  dep->de_linmem = 0xFFFF0000;

  /* Make sure statisics are cleared */
  memset((void *) &(dep->de_stat), 0, sizeof(eth_stat_t));

  /* Device specific initialization */
  (*dep->de_initf) (dep);

  /* Set the interrupt handler policy. Request interrupts not to be reenabled
   * automatically. Return the IRQ line number when an interrupt occurs.
   */
  dep->de_hook = dep->de_irq;
  if (sys_irqsetpolicy(dep->de_irq, 0 /*IRQ_REENABLE*/, &dep->de_hook) != OK)
	panic("unable to set IRQ policy");
  dep->de_int_pending = FALSE;
  sys_irqenable(&dep->de_hook);

  return;
}

/*
**  Name:	void do_init(message *mp)
**  Function:	Checks for hardware presence.
**  		Provides initialization of hardware and data structures
*/
static void do_init(const message * mp)
{
  dpeth_t *dep;
  dp_conf_t *dcp;
  message reply_mess;
  int r, confnr;

  dep = &de_state;

  /* Pick a default configuration for this instance. */
  confnr = MIN(de_instance, DP_CONF_NR-1);

  dcp = &dp_conf[confnr];
  strlcpy(dep->de_name, DevName, sizeof(dep->de_name));
  dep->de_name[4] = '0' + de_instance;

  if (dep->de_mode == DEM_DISABLED) {

	update_conf(dep, dcp);	/* First time thru */
	if (dep->de_mode == DEM_ENABLED &&
	    !el1_probe(dep) &&	/* Probe for 3c501  */
	    !wdeth_probe(dep) &&	/* Probe for WD80x3 */
	    !ne_probe(dep) &&	/* Probe for NEx000 */
	    !el2_probe(dep) &&	/* Probe for 3c503  */
	    !el3_probe(dep)) {	/* Probe for 3c509  */
		printf("%s: warning no ethernet card found at 0x%04X\n",
		       dep->de_name, dep->de_base_port);
		dep->de_mode = DEM_DISABLED;
	}
  }

  r = OK;

  /* 'de_mode' may change if probe routines fail, test again */
  switch (dep->de_mode) {

    case DEM_DISABLED:
	/* Device is configured OFF or hardware probe failed */
	r = ENXIO;
	break;

    case DEM_ENABLED:
	/* Device is present and probed */
	if (dep->de_flags == DEF_EMPTY) {
		/* These actions only the first time */
		do_first_init(dep, dcp);
		dep->de_flags |= DEF_ENABLED;
	}
	dep->de_flags &= NOT(DEF_PROMISC | DEF_MULTI | DEF_BROAD);
	if (mp->DL_MODE & DL_PROMISC_REQ)
		dep->de_flags |= DEF_PROMISC | DEF_MULTI | DEF_BROAD;
	if (mp->DL_MODE & DL_MULTI_REQ) dep->de_flags |= DEF_MULTI;
	if (mp->DL_MODE & DL_BROAD_REQ) dep->de_flags |= DEF_BROAD;
	(*dep->de_flagsf) (dep);
	break;

    case DEM_SINK:
	/* Device not present (sink mode) */
	memset(dep->de_address.ea_addr, 0, sizeof(ether_addr_t));
	dp_confaddr(dep);	/* Station address from env. */
	break;

    default:	break;
  }

  reply_mess.m_type = DL_CONF_REPLY;
  reply_mess.DL_STAT = r;
  if (r == OK)
	*(ether_addr_t *) reply_mess.DL_HWADDR = dep->de_address;
  DEBUG(printf("\t reply %d\n", reply_mess.m_type));
  if (send(mp->m_source, &reply_mess) != OK)	/* Can't send */
	panic(SendErrMsg, mp->m_source);

  return;
}

/*
**  Name:	void dp_next_iovec(iovec_dat_t *iovp)
**  Function:	Retrieves data from next iovec element.
*/
void dp_next_iovec(iovec_dat_s_t * iovp)
{

  iovp->iod_iovec_s -= IOVEC_NR;
  iovp->iod_iovec_offset += IOVEC_NR * sizeof(iovec_t);
  get_userdata_s(iovp->iod_proc_nr, iovp->iod_grant, iovp->iod_iovec_offset,
	     iovp->iod_iovec_s, iovp->iod_iovec);
  return;
}

/*
**  Name:	int calc_iovec_size(iovec_dat_t *iovp)
**  Function:	Compute the size of a request.
*/
static int calc_iovec_size(iovec_dat_s_t * iovp)
{
  int size, ix;

  size = ix = 0;
  do {
	size += iovp->iod_iovec[ix].iov_size;
	if (++ix >= IOVEC_NR) {
		dp_next_iovec(iovp);
		ix = 0;
	}

	/* Till all vectors added */
  } while (ix < iovp->iod_iovec_s);
  return size;
}

/*
**  Name:	void do_vwrite_s(message *mp)
**  Function:
*/
static void do_vwrite_s(const message * mp)
{
  int size;
  dpeth_t *dep;

  dep = &de_state;

  dep->de_client = mp->m_source;

  if (dep->de_mode == DEM_ENABLED) {

	if (dep->de_flags & DEF_SENDING)	/* Is sending in progress? */
		panic("send already in progress ");

	dep->de_write_iovec.iod_proc_nr = mp->m_source;
	get_userdata_s(mp->m_source, mp->DL_GRANT, 0,
	       mp->DL_COUNT, dep->de_write_iovec.iod_iovec);
	dep->de_write_iovec.iod_iovec_s = mp->DL_COUNT;
	dep->de_write_iovec.iod_grant = (cp_grant_id_t) mp->DL_GRANT;
	dep->de_write_iovec.iod_iovec_offset = 0;
	size = calc_iovec_size(&dep->de_write_iovec);
	if (size < ETH_MIN_PACK_SIZE || size > ETH_MAX_PACK_SIZE)
		panic(SizeErrMsg, size);

	dep->de_flags |= DEF_SENDING;
	(*dep->de_sendf) (dep, FALSE, size);

  } else if (dep->de_mode == DEM_SINK)
	dep->de_flags |= DEF_ACK_SEND;

  reply(dep);
  return;
}

/*
**  Name:	void do_vread_s(message *mp, int vectored)
**  Function:
*/
static void do_vread_s(const message * mp)
{
  int size;
  dpeth_t *dep;

  dep = &de_state;

  dep->de_client = mp->m_source;

  if (dep->de_mode == DEM_ENABLED) {

	if (dep->de_flags & DEF_READING)	/* Reading in progress */
		panic("read already in progress");

	dep->de_read_iovec.iod_proc_nr = mp->m_source;
	get_userdata_s(mp->m_source, (cp_grant_id_t) mp->DL_GRANT, 0,
		mp->DL_COUNT, dep->de_read_iovec.iod_iovec);
	dep->de_read_iovec.iod_iovec_s = mp->DL_COUNT;
	dep->de_read_iovec.iod_grant = (cp_grant_id_t) mp->DL_GRANT;
	dep->de_read_iovec.iod_iovec_offset = 0;
	size = calc_iovec_size(&dep->de_read_iovec);
	if (size < ETH_MAX_PACK_SIZE) panic(SizeErrMsg, size);

	dep->de_flags |= DEF_READING;
	(*dep->de_recvf) (dep, FALSE, size);
#if 0
	if ((dep->de_flags & (DEF_READING | DEF_STOPPED)) == (DEF_READING | DEF_STOPPED))
		/* The chip is stopped, and all arrived packets delivered */
		(*dep->de_resetf) (dep);
	dep->de_flags &= NOT(DEF_STOPPED);
#endif
  }
  reply(dep);
  return;
}

/*
**  Name:	void do_getstat_s(message *mp)
**  Function:	Reports device statistics.
*/
static void do_getstat_s(const message * mp)
{
  int rc;
  dpeth_t *dep;
  message reply_mess;

  dep = &de_state;

  if (dep->de_mode == DEM_ENABLED) (*dep->de_getstatsf) (dep);
  if ((rc = sys_safecopyto(mp->m_source, mp->DL_GRANT, 0,
			(vir_bytes)&dep->de_stat,
			(vir_bytes) sizeof(dep->de_stat))) != OK)
        panic(CopyErrMsg, rc);

  reply_mess.m_type = DL_STAT_REPLY;
  rc= send(mp->m_source, &reply_mess);
  if (rc != OK)
	panic("do_getname: send failed: %d", rc);
  return;
}

/*
**  Name:	void dp_stop(dpeth_t *dep)
**  Function:	Stops network interface.
*/
static void dp_stop(dpeth_t * dep)
{

  if (dep->de_mode == DEM_ENABLED && (dep->de_flags & DEF_ENABLED)) {

	/* Stop device */
	(dep->de_stopf) (dep);
	dep->de_flags = DEF_EMPTY;
	dep->de_mode = DEM_DISABLED;
  }
  return;
}

static void do_watchdog(const void *UNUSED(message))
{

  DEBUG(printf("\t no reply"));
  return;
}

static void handle_hw_intr(void)
{
	dpeth_t *dep;

	dep = &de_state;

	/* If device is enabled and interrupt pending */
	if (dep->de_mode == DEM_ENABLED) {
		dep->de_int_pending = TRUE;
		(*dep->de_interruptf) (dep);
		if (dep->de_flags & (DEF_ACK_SEND | DEF_ACK_RECV))
			reply(dep);
		dep->de_int_pending = FALSE;
		sys_irqenable(&dep->de_hook);
	}
}

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

/*
**  Name:	int dpeth_task(void)
**  Function:	Main entry for dp task
*/
int main(int argc, char **argv)
{
  message m;
  int ipc_status;
  int rc;

  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();

  while (TRUE) {
	if ((rc = netdriver_receive(ANY, &m, &ipc_status)) != OK){
		panic(RecvErrMsg, rc);
	}

	DEBUG(printf("eth: got message %d, ", m.m_type));

	if (is_ipc_notify(ipc_status)) {
		switch(_ENDPOINT_P(m.m_source)) {
			case CLOCK:
				/* to be defined */
				do_watchdog(&m);
				break;
			case HARDWARE:
				/* Interrupt from device */
				handle_hw_intr();
				break;
			case TTY_PROC_NR:
				/* Function key pressed */
				do_dump(&m);
				break;
			default:	
				/* Invalid message type */
				panic(TypeErrMsg, m.m_type);
				break;
		}
		/* message processed, get another one */
		continue;
	}

	switch (m.m_type) {
	    case DL_WRITEV_S:	/* Write message to device */
		do_vwrite_s(&m);
		break;
	    case DL_READV_S:	/* Read message from device */
		do_vread_s(&m);
		break;
	    case DL_CONF:	/* Initialize device */
		do_init(&m);
		break;
	    case DL_GETSTAT_S:	/* Get device statistics */
		do_getstat_s(&m);
		break;
	    default:		/* Invalid message type */
		panic(TypeErrMsg, m.m_type);
		break;
	}
  }
  return OK;			/* Never reached, but keeps compiler happy */
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* Register live update callbacks. */
  sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_workfree);

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
/* Initialize the dpeth driver. */
  int fkeys, sfkeys;
  long v;

  /* Request function key for debug dumps */
  fkeys = sfkeys = 0; bit_set(sfkeys, 8);
  if ((fkey_map(&fkeys, &sfkeys)) != OK) 
	printf("%s: couldn't program Shift+F8 key (%d)\n", DevName, errno);

  v = 0;
  (void) env_parse("instance", "d", 0, &v, 0, 255);
  de_instance = (int) v;

  /* Announce we are up! */
  netdriver_announce();

  return(OK);
}

/*===========================================================================*
 *		            sef_cb_signal_handler                            *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  if (de_state.de_mode == DEM_ENABLED)
	dp_stop(&de_state);

  exit(0);
}

/** dp.c **/
