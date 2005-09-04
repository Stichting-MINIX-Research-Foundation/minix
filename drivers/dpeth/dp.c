/*
**  File:	eth.c	Version 1.00,	Jan. 14, 1997
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  This file contains the ethernet device driver main task.
**  It has to be integrated with the board specific drivers.
**  It is a rewriting of Minix 2.0.0 ethernet driver (dp8390.c)
**  to remove bord specific code. It should operate (I hope)
**  with any board driver.
**
**  The valid messages and their parameters are:
**
**    m_type       DL_PORT   DL_PROC  DL_COUNT DL_MODE DL_ADDR
**  +------------+---------+---------+--------+-------+---------+
**  | HARD_INT   |         |         |        |       |         | 
**  +------------+---------+---------+--------+-------+---------+
**  | SYN_ALARM  |         |         |        |       |         | 
**  +------------+---------+---------+--------+-------+---------+
**  | HARD_STOP  |         |         |        |       |         | 
**  +------------+---------+---------+--------+-------+---------+
**  | FKEY_PRESSED         |         |        |       |         | (99)
**  +------------+---------+---------+--------+-------+---------+
**  | DL_WRITE   | port nr | proc nr | count  | mode  | address | (3)
**  +------------+---------+---------+--------+-------+---------+
**  | DL_WRITEV  | port nr | proc nr | count  | mode  | address | (4)
**  +------------+---------+---------+--------+-------+---------+
**  | DL_READ    | port nr | proc nr | count  |       | address | (5)
**  +------------+---------+---------+--------+-------+---------+
**  | DL_READV   | port nr | proc nr | count  |       | address | (6)
**  +------------+---------+---------+--------+-------+---------+
**  | DL_INIT    | port nr | proc nr |        | mode  | address | (7)
**  +------------+---------+---------+--------+-------+---------+
**  | DL_STOP    | port_nr |         |        |       |         | (8)
**  +------------+---------+---------+--------+-------+---------+
**  | DL_GETSTAT | port nr | proc nr |        |       | address | (9)
**  +------------+---------+---------+--------+-------+---------+
**
**  The messages sent are:
**
**    m-type       DL_PORT   DL_PROC  DL_COUNT  DL_STAT   DL_CLCK
**  +------------+---------+---------+--------+---------+---------+
**  |DL_TASK_REPL| port nr | proc nr |rd-count| err|stat| clock   | (21)
**  +------------+---------+---------+--------+---------+---------+
**
**    m_type       m3_i1     m3_i2      m3_ca1
**  +------------+---------+---------+---------------+
**  |DL_INIT_REPL| port nr |last port| ethernet addr | (20)
**  +------------+---------+---------+---------------+
**
**  $Id$
*/

#include "drivers.h"
#include <minix/keymap.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>

#include "dp.h"

/*
**  Local data
*/
extern int errno;
static dpeth_t de_table[DE_PORT_NR];

typedef struct dp_conf {	/* Configuration description structure */
  port_t dpc_port;
  int dpc_irq;
  phys_bytes dpc_mem;
  char *dpc_envvar;
} dp_conf_t;

/* Device default configuration */
static dp_conf_t dp_conf[DE_PORT_NR] = {
  /* I/O port, IRQ, Buff addr, Env. var, Buf. selector */
  {     0x300,   5,   0xC8000,	"DPETH0", },
  {     0x280,  10,   0xCC000,	"DPETH1", },
};

static const char CopyErrMsg[] = "unable to read/write user data";
static const char PortErrMsg[] = "illegal port";
static const char RecvErrMsg[] = "receive failed";
static const char SendErrMsg[] = "send failed";
static const char SizeErrMsg[] = "illegal packet size";
static const char TypeErrMsg[] = "illegal message type";
static const char DevName[] = "eth#?";

/*
**  Name:	void reply(dpeth_t *dep, int err)
**  Function:	Fills a DL_TASK_REPLY reply message and sends it.
*/
static void reply(dpeth_t * dep, int err)
{
  message reply;
  int status = FALSE;

  if (dep->de_flags & DEF_ACK_SEND) status |= DL_PACK_SEND;
  if (dep->de_flags & DEF_ACK_RECV) status |= DL_PACK_RECV;

  reply.m_type = DL_TASK_REPLY;
  reply.DL_PORT = dep - de_table;
  reply.DL_PROC = dep->de_client;
  reply.DL_STAT = status /* | ((u32_t) err << 16) */;
  reply.DL_COUNT = dep->de_read_s;
  getuptime(&reply.DL_CLCK);

  DEBUG(printf("\t reply %d (%ld)\n", reply.m_type, reply.DL_STAT));

  if ((status = send(dep->de_client, &reply)) == OK) {
	dep->de_read_s = 0;
	dep->de_flags &= NOT(DEF_ACK_SEND | DEF_ACK_RECV);

  } else if (status != ELOCKED || err == OK)
	panic(dep->de_name, SendErrMsg, status);

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

  strcpy(ea_key, dp_conf[dep - de_table].dpc_envvar);
  strcat(ea_key, "_EA");

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
static void update_conf(dpeth_t * dep, dp_conf_t * dcp)
{
  static char dpc_fmt[] = "x:d:x";
  long val;

  dep->de_mode = DEM_SINK;
  val = dcp->dpc_port;		/* Get I/O port address */
  switch (env_parse(dcp->dpc_envvar, dpc_fmt, 0, &val, 0x000L, 0x3FFL)) {
      case EP_OFF:	dep->de_mode = DEM_DISABLED;	break;
      case EP_ON:
      case EP_SET:	dep->de_mode = DEM_ENABLED;	break;
  }
  dep->de_base_port = val;

  val = dcp->dpc_irq | DEI_DEFAULT;	/* Get Interrupt line (IRQ) */
  env_parse(dcp->dpc_envvar, dpc_fmt, 1, &val, 0L, (long) NR_IRQ_VECTORS - 1);
  dep->de_irq = val;

  val = dcp->dpc_mem;		/* Get shared memory address */
  env_parse(dcp->dpc_envvar, dpc_fmt, 2, &val, 0L, LONG_MAX);
  dep->de_linmem = val;

  return;
}

/*
**  Name:	void do_dump(message *mp)
**  Function:	Displays statistics on screen (SFx key from console)
*/
static void do_dump(message *mp)
{
  dpeth_t *dep;
  int port;

  printf("\n\n");
  for (port = 0, dep = de_table; port < DE_PORT_NR; port += 1, dep += 1) {

	if (dep->de_mode == DEM_DISABLED) continue;

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
  }
  return;
}

/*
**  Name:	void get_userdata(int user_proc, vir_bytes user_addr, int count, void *loc_addr)
**  Function:	Copies data from user area.
*/
static void get_userdata(int user_proc, vir_bytes user_addr, int count, void *loc_addr)
{
  int rc;
  vir_bytes len;

  len = (count > IOVEC_NR ? IOVEC_NR : count) * sizeof(iovec_t);
  if ((rc = sys_datacopy(user_proc, user_addr, SELF, (vir_bytes)loc_addr, len)) != OK)
	panic(DevName, CopyErrMsg, rc);
  return;
}

/*
**  Name:	void do_first_init(dpeth_t *dep, dp_conf_t *dcp);
**  Function:	Init action to setup task
*/
static void do_first_init(dpeth_t *dep, dp_conf_t *dcp)
{

  if (dep->de_linmem != 0) {
	dep->de_memsegm = BIOS_SEG;
	/* phys2seg(&dep->de_memsegm, &dep->de_memoffs, dep->de_linmem); */
  } else
	dep->de_linmem = 0xFFFF0000;

  /* Make sure statisics are cleared */
  memset((void *) &(dep->de_stat), 0, sizeof(eth_stat_t));

  /* Device specific initialization */
  (*dep->de_initf) (dep);

  /* Set the interrupt handler policy. Request interrupts not to be reenabled
   * automatically. Return the IRQ line number when an interrupt occurs.
   */
  dep->de_hook = dep->de_irq;
  sys_irqsetpolicy(dep->de_irq, 0 /*IRQ_REENABLE*/, &dep->de_hook);
  dep->de_int_pending = FALSE;
  sys_irqenable(&dep->de_hook);

  return;
}

/*
**  Name:	void do_init(message *mp)
**  Function:	Checks for hardware presence.
**  		Provides initialization of hardware and data structures
*/
static void do_init(message * mp)
{
  int port;
  dpeth_t *dep;
  dp_conf_t *dcp;
  message reply_mess;

  port = mp->DL_PORT;
  if (port >= 0 && port < DE_PORT_NR) {

	dep = &de_table[port];
	dcp = &dp_conf[port];
	strcpy(dep->de_name, DevName);
	dep->de_name[4] = '0' + port;

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

	/* 'de_mode' may change if probe routines fail, test again */
	switch (dep->de_mode) {

	    case DEM_DISABLED:
		/* Device is configured OFF or hardware probe failed */
		port = ENXIO;
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
		dep->de_client = mp->m_source;
		break;

	    case DEM_SINK:
		/* Device not present (sink mode) */
		memset(dep->de_address.ea_addr, 0, sizeof(ether_addr_t));
		dp_confaddr(dep);	/* Station address from env. */
		break;

	    default:	break;
	}
	*(ether_addr_t *) reply_mess.m3_ca1 = dep->de_address;

  } else			/* Port number is out of range */
	port = ENXIO;

  reply_mess.m_type = DL_INIT_REPLY;
  reply_mess.m3_i1 = port;
  reply_mess.m3_i2 = DE_PORT_NR;
  DEBUG(printf("\t reply %d\n", reply_mess.m_type));
  if (send(mp->m_source, &reply_mess) != OK)	/* Can't send */
	panic(dep->de_name, SendErrMsg, mp->m_source);

  return;
}

/*
**  Name:	void dp_next_iovec(iovec_dat_t *iovp)
**  Function:	Retrieves data from next iovec element.
*/
PUBLIC void dp_next_iovec(iovec_dat_t * iovp)
{

  iovp->iod_iovec_s -= IOVEC_NR;
  iovp->iod_iovec_addr += IOVEC_NR * sizeof(iovec_t);
  get_userdata(iovp->iod_proc_nr, iovp->iod_iovec_addr,
	     iovp->iod_iovec_s, iovp->iod_iovec);
  return;
}

/*
**  Name:	int calc_iovec_size(iovec_dat_t *iovp)
**  Function:	Compute the size of a request.
*/
static int calc_iovec_size(iovec_dat_t * iovp)
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
**  Name:	void do_vwrite(message *mp, int vectored)
**  Function:
*/
static void do_vwrite(message * mp, int vectored)
{
  int port, size;
  dpeth_t *dep;

  port = mp->DL_PORT;
  if (port < 0 || port >= DE_PORT_NR)	/* Check for illegal port number */
	panic(dep->de_name, PortErrMsg, port);

  dep = &de_table[port];
  dep->de_client = mp->DL_PROC;

  if (dep->de_mode == DEM_ENABLED) {

	if (dep->de_flags & DEF_SENDING)	/* Is sending in progress? */
		panic(dep->de_name, "send already in progress ", NO_NUM);

	dep->de_write_iovec.iod_proc_nr = mp->DL_PROC;
	if (vectored) {
		get_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
		       mp->DL_COUNT, dep->de_write_iovec.iod_iovec);
		dep->de_write_iovec.iod_iovec_s = mp->DL_COUNT;
		dep->de_write_iovec.iod_iovec_addr = (vir_bytes) mp->DL_ADDR;
		size = calc_iovec_size(&dep->de_write_iovec);
	} else {
		dep->de_write_iovec.iod_iovec[0].iov_addr = (vir_bytes) mp->DL_ADDR;
		dep->de_write_iovec.iod_iovec[0].iov_size = size = mp->DL_COUNT;
		dep->de_write_iovec.iod_iovec_s = 1;
		dep->de_write_iovec.iod_iovec_addr = 0;
	}
	if (size < ETH_MIN_PACK_SIZE || size > ETH_MAX_PACK_SIZE)
		panic(dep->de_name, SizeErrMsg, size);

	dep->de_flags |= DEF_SENDING;
	(*dep->de_sendf) (dep, FALSE, size);

  } else if (dep->de_mode == DEM_SINK)
	dep->de_flags |= DEF_ACK_SEND;

  reply(dep, OK);
  return;
}

/*
**  Name:	void do_vread(message *mp, int vectored)
**  Function:
*/
static void do_vread(message * mp, int vectored)
{
  int port, size;
  dpeth_t *dep;

  port = mp->DL_PORT;
  if (port < 0 || port >= DE_PORT_NR)	/* Check for illegal port number */
	panic(dep->de_name, PortErrMsg, port);

  dep = &de_table[port];
  dep->de_client = mp->DL_PROC;

  if (dep->de_mode == DEM_ENABLED) {

	if (dep->de_flags & DEF_READING)	/* Reading in progress */
		panic(dep->de_name, "read already in progress", NO_NUM);

	dep->de_read_iovec.iod_proc_nr = mp->DL_PROC;
	if (vectored) {
		get_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
			mp->DL_COUNT, dep->de_read_iovec.iod_iovec);
		dep->de_read_iovec.iod_iovec_s = mp->DL_COUNT;
		dep->de_read_iovec.iod_iovec_addr = (vir_bytes) mp->DL_ADDR;
		size = calc_iovec_size(&dep->de_read_iovec);
	} else {
		dep->de_read_iovec.iod_iovec[0].iov_addr = (vir_bytes) mp->DL_ADDR;
		dep->de_read_iovec.iod_iovec[0].iov_size = size = mp->DL_COUNT;
		dep->de_read_iovec.iod_iovec_s = 1;
		dep->de_read_iovec.iod_iovec_addr = 0;
	}
	if (size < ETH_MAX_PACK_SIZE) panic(dep->de_name, SizeErrMsg, size);

	dep->de_flags |= DEF_READING;
	(*dep->de_recvf) (dep, FALSE, size);
#if 0
	if ((dep->de_flags & (DEF_READING | DEF_STOPPED)) == (DEF_READING | DEF_STOPPED))
		/* The chip is stopped, and all arrived packets delivered */
		(*dep->de_resetf) (dep);
	dep->de_flags &= NOT(DEF_STOPPED);
#endif
  }
  reply(dep, OK);
  return;
}

/*
**  Name:	void do_getstat(message *mp)
**  Function:	Reports device statistics.
*/
static void do_getstat(message * mp)
{
  int port, rc;
  dpeth_t *dep;

  port = mp->DL_PORT;
  if (port < 0 || port >= DE_PORT_NR)	/* Check for illegal port number */
	panic(dep->de_name, PortErrMsg, port);

  dep = &de_table[port];
  dep->de_client = mp->DL_PROC;

  if (dep->de_mode == DEM_ENABLED) (*dep->de_getstatsf) (dep);
  if ((rc = sys_datacopy(SELF, (vir_bytes)&dep->de_stat,
	                 mp->DL_PROC, (vir_bytes)mp->DL_ADDR,
			 (vir_bytes) sizeof(dep->de_stat))) != OK)
        panic(DevName, CopyErrMsg, rc);
  reply(dep, OK);
  return;
}

/*
**  Name:	void do_stop(message *mp)
**  Function:	Stops network interface.
*/
static void do_stop(message * mp)
{
  int port;
  dpeth_t *dep;

  port = mp->DL_PORT;
  if (port < 0 || port >= DE_PORT_NR)	/* Check for illegal port number */
	panic(dep->de_name, PortErrMsg, port);

  dep = &de_table[port];
  if (dep->de_mode == DEM_ENABLED && (dep->de_flags & DEF_ENABLED)) {

	/* Stop device */
	(dep->de_stopf) (dep);
	dep->de_flags = DEF_EMPTY;
	dep->de_mode = DEM_DISABLED;
  }
  return;
}

static void do_watchdog(void *message)
{

  DEBUG(printf("\t no reply"));
  return;
}

/*
**  Name:	int dpeth_task(void)
**  Function:	Main entry for dp task
*/
PUBLIC int main(int argc, char **argv)
{
  message m;
  dpeth_t *dep;
  int rc, fkeys, sfkeys;

  env_setargs(argc, argv);

  /* Request function key for debug dumps */
  fkeys = sfkeys = 0; bit_set(sfkeys, 8);
  if ((fkey_map(&fkeys, &sfkeys)) != OK) 
	printf("%s: couldn't program Shift+F8 key (%d)\n", DevName, errno);

#ifdef ETH_IGN_PROTO
  {
	static u16_t eth_ign_proto = 0;
	long val;
	val = 0xFFFF;
	env_parse("ETH_IGN_PROTO", "x", 0, &val, 0x0000L, 0xFFFFL);
	eth_ign_proto = htons((u16_t) val);
  }
#endif

  while (TRUE) {
	if ((rc = receive(ANY, &m)) != OK) panic(dep->de_name, RecvErrMsg, rc);

	DEBUG(printf("eth: got message %d, ", m.m_type));

	switch (m.m_type) {
	    case DL_WRITE:	/* Write message to device */
		do_vwrite(&m, FALSE);
		break;
	    case DL_WRITEV:	/* Write message to device */
		do_vwrite(&m, TRUE);
		break;
	    case DL_READ:	/* Read message from device */
		do_vread(&m, FALSE);
		break;
	    case DL_READV:	/* Read message from device */
		do_vread(&m, TRUE);
		break;
	    case DL_INIT:	/* Initialize device */
		do_init(&m);
		break;
	    case DL_GETSTAT:	/* Get device statistics */
		do_getstat(&m);
		break;
	    case SYN_ALARM:	/* to be defined */
		do_watchdog(&m);
		break;
	    case DL_STOP:	/* Stop device */
		do_stop(&m);
		break;
	    case SYS_SIG: {
	    	sigset_t sigset = m.NOTIFY_ARG;
	    	if (sigismember(&sigset, SIGKSTOP)) {	/* Shut down */
		    for (rc = 0; rc < DE_PORT_NR; rc += 1) {
			if (de_table[rc].de_mode == DEM_ENABLED) {
				m.m_type = DL_STOP;
				m.DL_PORT = rc;
				do_stop(&m);
			}
		    }
		}
		break;
	    }
	    case HARD_INT:	/* Interrupt from device */
		for (dep = de_table; dep < &de_table[DE_PORT_NR]; dep += 1) {
			/* If device is enabled and interrupt pending */
			if (dep->de_mode == DEM_ENABLED) {
				dep->de_int_pending = TRUE;
				(*dep->de_interruptf) (dep);
				if (dep->de_flags & (DEF_ACK_SEND | DEF_ACK_RECV))
					reply(dep, !OK);
				dep->de_int_pending = FALSE;
				sys_irqenable(&dep->de_hook);
			}
		}
		break;
	    case FKEY_PRESSED:	/* Function key pressed */
		do_dump(&m);
		break;
	    default:		/* Invalid message type */
		panic(DevName, TypeErrMsg, m.m_type);
		break;
	}
  }
  return OK;			/* Never reached, but keeps compiler happy */
}

/** dp.c **/
