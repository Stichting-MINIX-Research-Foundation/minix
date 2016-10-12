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
#include <sys/mman.h>
#include <assert.h>

#include "dp.h"

/*
**  Local data
*/
static dpeth_t de_state;

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

static int do_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks);
static void do_stop(void);
static void do_set_mode(unsigned int mode, const netdriver_addr_t *mcast_list,
	unsigned int mcast_count);
static int do_send(struct netdriver_data *data, size_t size);
static ssize_t do_recv(struct netdriver_data *data, size_t max);
static void do_intr(unsigned int mask);
static void do_tick(void);
static void do_other(const message *m_ptr, int ipc_status);

static const struct netdriver dp_table = {
	.ndr_name	= "dpe",
	.ndr_init	= do_init,
	.ndr_stop	= do_stop,
	.ndr_set_mode	= do_set_mode,
	.ndr_recv	= do_recv,
	.ndr_send	= do_send,
	.ndr_intr	= do_intr,
	.ndr_tick	= do_tick,
	.ndr_other	= do_other
};

/*
**  Name:	update_conf
**  Function:	Gets the default settings from 'dp_conf' table and
**  		modifies them from the environment.
*/
static void update_conf(dpeth_t * dep, const dp_conf_t * dcp,
	unsigned int instance)
{
  static char dpc_fmt[] = "x:d:x";
  char ec_key[16];
  long val;

  strlcpy(ec_key, "DPETH0", sizeof(ec_key));
  ec_key[5] += instance;

  val = dcp->dpc_port;		/* Get I/O port address */
  env_parse(ec_key, dpc_fmt, 0, &val, 0x000L, 0x3FFL);
  dep->de_base_port = val;

  val = dcp->dpc_irq | DEI_DEFAULT;	/* Get Interrupt line (IRQ) */
  env_parse(ec_key, dpc_fmt, 1, &val, 0L, (long) NR_IRQ_VECTORS - 1);
  dep->de_irq = val;

  val = dcp->dpc_mem;		/* Get shared memory address */
  env_parse(ec_key, dpc_fmt, 2, &val, 0L, LONG_MAX);
  dep->de_linmem = val;
}

/*
**  Name:	do_dump
**  Function:	Displays statistics on screen (SFx key from console)
*/
static void do_dump(void)
{
  dpeth_t *dep;

  dep = &de_state;

  printf("\n\n");

  printf("%s statistics:\t\t", netdriver_name());

  /* Network interface status  */
  printf("Status: 0x%04x\n\n", dep->de_flags);

  (*dep->de_dumpstatsf)(dep);

  /* Transmitted/received bytes */
  printf("Tx bytes:%10ld\t", dep->bytes_Tx);
  printf("Rx bytes:%10ld\n", dep->bytes_Rx);
}

/*
**  Name:	do_first_init
**  Function:	Init action to setup task
*/
static void do_first_init(dpeth_t *dep, const dp_conf_t *dcp)
{

  dep->de_linmem = 0xFFFF0000; /* FIXME: this overrides update_conf, why? */

  /* Device specific initialization */
  (*dep->de_initf)(dep);

  /* Map memory if requested */
  if (dep->de_linmem != 0) {
	assert(dep->de_ramsize > 0);
	dep->de_locmem =
	    vm_map_phys(SELF, (void *)dep->de_linmem, dep->de_ramsize);
	if (dep->de_locmem == MAP_FAILED)
		panic("unable to map memory");
  }

  /* Set the interrupt handler policy. Request interrupts not to be reenabled
   * automatically. Return the IRQ line number when an interrupt occurs.
   */
  dep->de_hook = dep->de_irq;
  if (sys_irqsetpolicy(dep->de_irq, 0 /*IRQ_REENABLE*/, &dep->de_hook) != OK)
	panic("unable to set IRQ policy");
  sys_irqenable(&dep->de_hook);
}

/*
**  Name:	do_init
**  Function:	Checks for hardware presence.
**  		Initialize hardware and data structures.
**		Return status and ethernet address.
*/
static int do_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks)
{
  dpeth_t *dep;
  dp_conf_t *dcp;
  int confnr, fkeys, sfkeys;

  dep = &de_state;

  /* Pick a default configuration for this instance. */
  confnr = MIN(instance, DP_CONF_NR-1);

  dcp = &dp_conf[confnr];

  update_conf(dep, dcp, instance);

  if (!el1_probe(dep) &&	/* Probe for 3c501  */
    !wdeth_probe(dep) &&	/* Probe for WD80x3 */
    !ne_probe(dep) &&		/* Probe for NEx000 */
    !el2_probe(dep) &&		/* Probe for 3c503  */
    !el3_probe(dep)) {		/* Probe for 3c509  */
	printf("%s: warning no ethernet card found at 0x%04X\n",
	       netdriver_name(), dep->de_base_port);
	return ENXIO;
  }

  do_first_init(dep, dcp);

  /* Request function key for debug dumps */
  fkeys = sfkeys = 0; bit_set(sfkeys, 7);
  if (fkey_map(&fkeys, &sfkeys) != OK)
	printf("%s: couldn't bind Shift+F7 key (%d)\n",
	    netdriver_name(), errno);

  memcpy(addr, dep->de_address.na_addr, sizeof(*addr));
  *caps = NDEV_CAP_MCAST | NDEV_CAP_BCAST; /* ..is this even accurate? */
  *ticks = sys_hz(); /* update statistics once a second */
  return OK;
}

/*
**  Name:	de_set_mode
**  Function:	Sets packet receipt mode.
*/
static void do_set_mode(unsigned int mode,
	const netdriver_addr_t * mcast_list __unused,
	unsigned int mcast_count __unused)
{
  dpeth_t *dep;

  dep = &de_state;

  dep->de_flags &= NOT(DEF_PROMISC | DEF_MULTI | DEF_BROAD);
  if (mode & NDEV_MODE_PROMISC)
	dep->de_flags |= DEF_PROMISC | DEF_MULTI | DEF_BROAD;
  if (mode & (NDEV_MODE_MCAST_LIST | NDEV_MODE_MCAST_ALL))
	dep->de_flags |= DEF_MULTI;
  if (mode & NDEV_MODE_BCAST)
	dep->de_flags |= DEF_BROAD;
  (*dep->de_flagsf)(dep);
}

/*
**  Name:	do_send
**  Function:	Send a packet, if possible.
*/
static int do_send(struct netdriver_data *data, size_t size)
{
  dpeth_t *dep;

  dep = &de_state;

  return (*dep->de_sendf)(dep, data, size);
}

/*
**  Name:	do_recv
**  Function:	Receive a packet, if possible.
*/
static ssize_t do_recv(struct netdriver_data *data, size_t max)
{
  dpeth_t *dep;

  dep = &de_state;

  return (*dep->de_recvf)(dep, data, max);
}

/*
**  Name:	do_stop
**  Function:	Stops network interface.
*/
static void do_stop(void)
{
  dpeth_t *dep;

  dep = &de_state;

  /* Stop device */
  (dep->de_stopf)(dep);
}

/*
**  Name:	do_intr
**  Function;	Handles interrupts.
*/
static void do_intr(unsigned int __unused mask)
{
	dpeth_t *dep;

	dep = &de_state;

	/* If device is enabled and interrupt pending */
	(*dep->de_interruptf)(dep);
	sys_irqenable(&dep->de_hook);
}

/*
**  Name:	do_tick
**  Function:	perform regular processing.
*/
static void do_tick(void)
{
	dpeth_t *dep;

	dep = &de_state;

	if (dep->de_getstatsf != NULL)
		(*dep->de_getstatsf)(dep);
}

/*
**  Name:	do_other
**  Function:	Processes miscellaneous messages.
*/
static void do_other(const message *m_ptr, int ipc_status)
{

  if (is_ipc_notify(ipc_status) && m_ptr->m_source == TTY_PROC_NR)
	do_dump();
}

/*
**  Name:	main
**  Function:	Main entry for dp task
*/
int main(int argc, char **argv)
{

  env_setargs(argc, argv);

  netdriver_task(&dp_table);

  return 0;
}
