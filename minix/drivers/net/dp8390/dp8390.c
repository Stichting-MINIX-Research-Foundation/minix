/*
 * dp8390.c
 *
 * This file contains a ethernet device driver for NS dp8390 based ethernet
 * cards.
 *
 * Created:	before Dec 28, 1992 by Philip Homburg <philip@f-mnx.phicoh.com>
 *
 * Modified Mar 10 1994 by Philip Homburg
 *	Become a generic dp8390 driver.
 *
 * Modified Dec 20 1996 by G. Falzoni <falzoni@marina.scn.de>
 *	Added support for 3c503 boards.
 */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <stdlib.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <minix/ds.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <machine/vm.h>
#include <sys/mman.h>
#include "assert.h"

#include "local.h"
#include "dp8390.h"

static dpeth_t de_state;
static int de_instance;

u32_t system_hz;

/* Configuration */
typedef struct dp_conf
{
	port_t dpc_port;
	int dpc_irq;
	phys_bytes dpc_mem;
} dp_conf_t;

#define DP_CONF_NR 4
static dp_conf_t dp_conf[DP_CONF_NR]=	/* Card addresses */
{
	/* I/O port, IRQ,  Buffer address. */
	{  0x280,     3,    0xD0000,       },
	{  0x300,     5,    0xC8000,       },
	{  0x380,    10,    0xD8000,       },
	{  0x000,     0,    0x00000,       },
};

/* Card inits configured out? */
#if !ENABLE_WDETH
#define wdeth_probe(dep)	(0)
#endif
#if !ENABLE_NE2000
#define ne_probe(dep)		(0)
#endif
#if !ENABLE_3C503
#define el2_probe(dep)		(0)
#endif

/* Some clones of the dp8390 and the PC emulator 'Bochs' require the CR_STA
 * on writes to the CR register. Additional CR_STAs do not appear to hurt
 * genuine dp8390s
 */
#define CR_EXTRA	CR_STA

#if ENABLE_PCI
static void pci_conf(void);
#endif
static void do_vwrite_s(message *mp, int from_int);
static void do_vread_s(message *mp);
static void do_init(message *mp);
static void do_int(dpeth_t *dep);
static void do_getstat_s(message *mp);
static void dp_stop(dpeth_t *dep);
static void dp_init(dpeth_t *dep);
static void dp_confaddr(dpeth_t *dep);
static void dp_reinit(dpeth_t *dep);
static void dp_reset(dpeth_t *dep);
static void dp_check_ints(dpeth_t *dep);
static void dp_recv(dpeth_t *dep);
static void dp_send(dpeth_t *dep);
static void dp_getblock(dpeth_t *dep, int page, size_t offset, size_t
	size, void *dst);
static void dp_pio8_getblock(dpeth_t *dep, int page, size_t offset,
	size_t size, void *dst);
static void dp_pio16_getblock(dpeth_t *dep, int page, size_t offset,
	size_t size, void *dst);
static int dp_pkt2user_s(dpeth_t *dep, int page, vir_bytes length);
static void dp_user2nic_s(dpeth_t *dep, iovec_dat_s_t *iovp, vir_bytes
	offset, int nic_addr, vir_bytes count);
static void dp_pio8_user2nic_s(dpeth_t *dep, iovec_dat_s_t *iovp,
	vir_bytes offset, int nic_addr, vir_bytes count);
static void dp_pio16_user2nic_s(dpeth_t *dep, iovec_dat_s_t *iovp,
	vir_bytes offset, int nic_addr, vir_bytes count);
static void dp_nic2user_s(dpeth_t *dep, int nic_addr, iovec_dat_s_t
	*iovp, vir_bytes offset, vir_bytes count);
static void dp_pio8_nic2user_s(dpeth_t *dep, int nic_addr, iovec_dat_s_t
	*iovp, vir_bytes offset, vir_bytes count);
static void dp_pio16_nic2user_s(dpeth_t *dep, int nic_addr,
	iovec_dat_s_t *iovp, vir_bytes offset, vir_bytes count);
static void dp_next_iovec_s(iovec_dat_s_t *iovp);
static void conf_hw(dpeth_t *dep);
static void update_conf(dpeth_t *dep, dp_conf_t *dcp);
static void map_hw_buffer(dpeth_t *dep);
static int calc_iovec_size_s(iovec_dat_s_t *iovp);
static void reply(dpeth_t *dep);
static void mess_reply(message *req, message *reply);
static void get_userdata_s(int user_proc, cp_grant_id_t grant, vir_bytes
	offset, vir_bytes count, void *loc_addr);
static void put_userdata_s(int user_proc, cp_grant_id_t grant, size_t
	count, void *loc_addr);
static void insb(port_t port, void *buf, size_t size);
static void insw(port_t port, void *buf, size_t size);
static void do_vir_insb(port_t port, int proc, vir_bytes buf, size_t
	size);
static void do_vir_insw(port_t port, int proc, vir_bytes buf, size_t
	size);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

static void handle_hw_intr(void)
{
	int r, irq;
	dpeth_t *dep;

	dep = &de_state;

	if (dep->de_mode != DEM_ENABLED)
		return;
	assert(dep->de_flags & DEF_ENABLED);
	irq= dep->de_irq;
	assert(irq >= 0 && irq < NR_IRQ_VECTORS);
	if (dep->de_int_pending || 1)
	{
		dep->de_int_pending= 0;
		dp_check_ints(dep);
		do_int(dep);
		r= sys_irqenable(&dep->de_hook);
		if (r != OK) {
			panic("unable enable interrupts: %d", r);
		}
	}
}

/*===========================================================================*
 *				dpeth_task				     *
 *===========================================================================*/
int main(int argc, char *argv[])
{
	message m;
	int ipc_status;
	int r;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE)
	{
		if ((r= netdriver_receive(ANY, &m, &ipc_status)) != OK)
			panic("dp8390: netdriver_receive failed: %d", r);

		if (is_ipc_notify(ipc_status)) {
			switch (_ENDPOINT_P(m.m_source)) {
				case HARDWARE:
					handle_hw_intr();
					break;
				case CLOCK:
					printf("dp8390: notify from CLOCK\n");
					break;
				default:
					panic("dp8390: illegal notify from: %d",
						m.m_source);
			}

			/* done, get a new message */
			continue;
		}

		switch (m.m_type)
		{
		case DL_WRITEV_S: do_vwrite_s(&m, FALSE);	break;
		case DL_READV_S: do_vread_s(&m);		break;
		case DL_CONF:	do_init(&m);			break;
		case DL_GETSTAT_S: do_getstat_s(&m);		break;
		default:
			panic("dp8390: illegal message: %d", m.m_type);
		}
	}
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
/* Initialize the dp8390 driver. */
	dpeth_t *dep;
	long v;

	system_hz = sys_hz();

	if (env_argc < 1) {
		panic("A head which at this time has no name");
	}

	v = 0;
	(void) env_parse("instance", "d", 0, &v, 0, 255);
	de_instance = (int) v;

	dep = &de_state;

	strlcpy(dep->de_name, "dp8390#0", sizeof(dep->de_name));
	dep->de_name[7] += de_instance;

	/* Announce we are up! */
	netdriver_announce();

	return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM) return;

	if (de_state.de_mode == DEM_ENABLED)
		dp_stop(&de_state);
}

#if 0
/*===========================================================================*
 *				dp8390_dump				     *
 *===========================================================================*/
void dp8390_dump()
{
	dpeth_t *dep;
	int isr;

	dep = &de_state;

	printf("\n");
#if XXX
	if (dep->de_mode == DEM_DISABLED)
		printf("dp8390 instance %d is disabled\n", de_instance);
	else if (dep->de_mode == DEM_SINK)
		printf("dp8390 instance %d is in sink mode\n", de_instance);
#endif

	if (dep->de_mode != DEM_ENABLED)
		return;

	printf("dp8390 statistics of instance %d:\n", de_instance);

	printf("recvErr    :%8ld\t", dep->de_stat.ets_recvErr);
	printf("sendErr    :%8ld\t", dep->de_stat.ets_sendErr);
	printf("OVW        :%8ld\n", dep->de_stat.ets_OVW);

	printf("CRCerr     :%8ld\t", dep->de_stat.ets_CRCerr);
	printf("frameAll   :%8ld\t", dep->de_stat.ets_frameAll);
	printf("missedP    :%8ld\n", dep->de_stat.ets_missedP);

	printf("packetR    :%8ld\t", dep->de_stat.ets_packetR);
	printf("packetT    :%8ld\t", dep->de_stat.ets_packetT);
	printf("transDef   :%8ld\n", dep->de_stat.ets_transDef);

	printf("collision  :%8ld\t", dep->de_stat.ets_collision);
	printf("transAb    :%8ld\t", dep->de_stat.ets_transAb);
	printf("carrSense  :%8ld\n", dep->de_stat.ets_carrSense);

	printf("fifoUnder  :%8ld\t", dep->de_stat.ets_fifoUnder);
	printf("fifoOver   :%8ld\t", dep->de_stat.ets_fifoOver);
	printf("CDheartbeat:%8ld\n", dep->de_stat.ets_CDheartbeat);

	printf("OWC        :%8ld\t", dep->de_stat.ets_OWC);

	isr= inb_reg0(dep, DP_ISR);
	printf("dp_isr = 0x%x + 0x%x, de_flags = 0x%x\n", isr,
				inb_reg0(dep, DP_ISR), dep->de_flags);
}
#endif

#if ENABLE_PCI
/*===========================================================================*
 *				pci_conf				     *
 *===========================================================================*/
static void pci_conf()
{
	char envvar[16];
	struct dpeth *dep;
	int i, pci_instance;
	static int first_time= 1;

	if (!first_time)
		return;
	first_time= 0;

	dep= &de_state;

	strlcpy(envvar, "DPETH0", sizeof(envvar));
	envvar[5] += de_instance;
	if (!(dep->de_pci= env_prefix(envvar, "pci")))
		return;	/* no PCI config */

	/* Count the number of dp instances before this one that are configured
	 * for PCI, so that we can skip that many when enumerating PCI devices.
	 */
	pci_instance= 0;
	for (i= 0; i < de_instance; i++) {
		envvar[5]= i;
		if (env_prefix(envvar, "pci"))
			pci_instance++;
	}

	if (!rtl_probe(dep, pci_instance))
		dep->de_pci= -1;
}
#endif /* ENABLE_PCI */

/*===========================================================================*
 *				do_vwrite_s				     *
 *===========================================================================*/
static void do_vwrite_s(mp, from_int)
message *mp;
int from_int;
{
	int count, size;
	int sendq_head;
	dpeth_t *dep;

	dep= &de_state;

	count = mp->m_net_netdrv_dl_writev_s.count;
	dep->de_client= mp->m_source;

	if (dep->de_mode == DEM_SINK)
	{
		assert(!from_int);
		dep->de_flags |= DEF_PACK_SEND;
		reply(dep);
		return;
	}
	assert(dep->de_mode == DEM_ENABLED);
	assert(dep->de_flags & DEF_ENABLED);
	if (dep->de_flags & DEF_SEND_AVAIL)
		panic("dp8390: send already in progress");

	sendq_head= dep->de_sendq_head;
	if (dep->de_sendq[sendq_head].sq_filled)
	{
		if (from_int)
			panic("dp8390: should not be sending");
		dep->de_sendmsg= *mp;
		dep->de_flags |= DEF_SEND_AVAIL;
		reply(dep);
		return;
	}
	assert(!(dep->de_flags & DEF_PACK_SEND));

	get_userdata_s(mp->m_source, mp->m_net_netdrv_dl_writev_s.grant, 0,
		(count > IOVEC_NR ? IOVEC_NR : count) *
		sizeof(dep->de_write_iovec_s.iod_iovec[0]),
		dep->de_write_iovec_s.iod_iovec);
	dep->de_write_iovec_s.iod_iovec_s = count;
	dep->de_write_iovec_s.iod_proc_nr = mp->m_source;
	dep->de_write_iovec_s.iod_grant = mp->m_net_netdrv_dl_writev_s.grant;
	dep->de_write_iovec_s.iod_iovec_offset = 0;

	dep->de_tmp_iovec_s = dep->de_write_iovec_s;
	size = calc_iovec_size_s(&dep->de_tmp_iovec_s);

	if (size < ETH_MIN_PACK_SIZE || size > ETH_MAX_PACK_SIZE_TAGGED)
	{
		panic("dp8390: invalid packet size: %d", size);
	}
	(dep->de_user2nicf_s)(dep, &dep->de_write_iovec_s, 0,
		dep->de_sendq[sendq_head].sq_sendpage * DP_PAGESIZE,
		size);
	dep->de_sendq[sendq_head].sq_filled= TRUE;
	if (dep->de_sendq_tail == sendq_head)
	{
		outb_reg0(dep, DP_TPSR, dep->de_sendq[sendq_head].sq_sendpage);
		outb_reg0(dep, DP_TBCR1, size >> 8);
		outb_reg0(dep, DP_TBCR0, size & 0xff);
		outb_reg0(dep, DP_CR, CR_TXP | CR_EXTRA);/* there it goes.. */
	}
	else
		dep->de_sendq[sendq_head].sq_size= size;
	
	if (++sendq_head == dep->de_sendq_nr)
		sendq_head= 0;
	assert(sendq_head < SENDQ_NR);
	dep->de_sendq_head= sendq_head;

	dep->de_flags |= DEF_PACK_SEND;

	/* If the interrupt handler called, don't send a reply. The reply
	 * will be sent after all interrupts are handled. 
	 */
	if (from_int)
		return;
	reply(dep);

	assert(dep->de_mode == DEM_ENABLED);
	assert(dep->de_flags & DEF_ENABLED);
}

/*===========================================================================*
 *				do_vread_s				     *
 *===========================================================================*/
static void do_vread_s(mp)
message *mp;
{
	int count;
	int size;
	dpeth_t *dep;

	dep= &de_state;

	count = mp->m_net_netdrv_dl_readv_s.count;
	dep->de_client= mp->m_source;
	if (dep->de_mode == DEM_SINK)
	{
		reply(dep);
		return;
	}
	assert(dep->de_mode == DEM_ENABLED);
	assert(dep->de_flags & DEF_ENABLED);

	if(dep->de_flags & DEF_READING)
		panic("dp8390: read already in progress");

	get_userdata_s(mp->m_source, mp->m_net_netdrv_dl_readv_s.grant, 0,
		(count > IOVEC_NR ? IOVEC_NR : count) *
		sizeof(dep->de_read_iovec_s.iod_iovec[0]),
		dep->de_read_iovec_s.iod_iovec);
	dep->de_read_iovec_s.iod_iovec_s = count;
	dep->de_read_iovec_s.iod_proc_nr = mp->m_source;
	dep->de_read_iovec_s.iod_grant = mp->m_net_netdrv_dl_readv_s.grant;
	dep->de_read_iovec_s.iod_iovec_offset = 0;

	dep->de_tmp_iovec_s = dep->de_read_iovec_s;
	size= calc_iovec_size_s(&dep->de_tmp_iovec_s);

	if (size < ETH_MAX_PACK_SIZE_TAGGED)
		panic("dp8390: wrong packet size: %d", size);
	dep->de_flags |= DEF_READING;

	dp_recv(dep);

	if ((dep->de_flags & (DEF_READING|DEF_STOPPED)) ==
		(DEF_READING|DEF_STOPPED))
	{
		/* The chip is stopped, and all arrived packets are 
		 * delivered.
		 */
		dp_reset(dep);
	}
	reply(dep);
}

/*===========================================================================*
 *				do_init					     *
 *===========================================================================*/
static void do_init(message *mp)
{
	dpeth_t *dep;
	message reply_mess;

#if ENABLE_PCI
	pci_conf(); /* Configure PCI devices. */
#endif

	dep= &de_state;

	if (dep->de_mode == DEM_DISABLED)
	{
		/* This is the default, try to (re)locate the device. */
		conf_hw(dep);
		if (dep->de_mode == DEM_DISABLED)
		{
			/* Probe failed, or the device is configured off. */
			reply_mess.m_type = DL_CONF_REPLY;
			reply_mess.m_netdrv_net_dl_conf.stat = ENXIO;
			mess_reply(mp, &reply_mess);
			return;
		}
		if (dep->de_mode == DEM_ENABLED)
			dp_init(dep);
	}

	if (dep->de_mode == DEM_SINK)
	{
		strncpy((char *) dep->de_address.ea_addr, "ZDP", 6);
		dep->de_address.ea_addr[5] = de_instance;
		dp_confaddr(dep);
		reply_mess.m_type = DL_CONF_REPLY;
		reply_mess.m_netdrv_net_dl_conf.stat = OK;
		memcpy(reply_mess.m_netdrv_net_dl_conf.hw_addr,
			dep->de_address.ea_addr,
			sizeof(reply_mess.m_netdrv_net_dl_conf.hw_addr));
		mess_reply(mp, &reply_mess);
		return;
	}
	assert(dep->de_mode == DEM_ENABLED);
	assert(dep->de_flags & DEF_ENABLED);

	dep->de_flags &= ~(DEF_PROMISC | DEF_MULTI | DEF_BROAD);

	if (mp->m_net_netdrv_dl_conf.mode & DL_PROMISC_REQ)
		dep->de_flags |= DEF_PROMISC | DEF_MULTI | DEF_BROAD;
	if (mp->m_net_netdrv_dl_conf.mode & DL_MULTI_REQ)
		dep->de_flags |= DEF_MULTI;
	if (mp->m_net_netdrv_dl_conf.mode & DL_BROAD_REQ)
		dep->de_flags |= DEF_BROAD;

	dp_reinit(dep);

	reply_mess.m_type = DL_CONF_REPLY;
	reply_mess.m_netdrv_net_dl_conf.stat = OK;

	memcpy(reply_mess.m_netdrv_net_dl_conf.hw_addr, dep->de_address.ea_addr,
		sizeof(reply_mess.m_netdrv_net_dl_conf.hw_addr));

	mess_reply(mp, &reply_mess);
}

/*===========================================================================*
 *				do_int					     *
 *===========================================================================*/
static void do_int(dep)
dpeth_t *dep;
{
	if (dep->de_flags & (DEF_PACK_SEND | DEF_PACK_RECV))
		reply(dep);
}

/*===========================================================================*
 *				do_getstat_s				     *
 *===========================================================================*/
static void do_getstat_s(mp)
message *mp;
{
	int r;
	dpeth_t *dep;

	dep= &de_state;

	if (dep->de_mode == DEM_SINK)
	{
		put_userdata_s(mp->m_source,
			mp->m_net_netdrv_dl_getstat_s.grant,
			(vir_bytes) sizeof(dep->de_stat), &dep->de_stat);

		mp->m_type= DL_STAT_REPLY;
		r= ipc_send(mp->m_source, mp);
		if (r != OK)
			panic("do_getstat: ipc_send failed: %d", r);
		return;
	}
	assert(dep->de_mode == DEM_ENABLED);
	assert(dep->de_flags & DEF_ENABLED);

	dep->de_stat.ets_CRCerr += inb_reg0(dep, DP_CNTR0);
	dep->de_stat.ets_frameAll += inb_reg0(dep, DP_CNTR1);
	dep->de_stat.ets_missedP += inb_reg0(dep, DP_CNTR2);

	put_userdata_s(mp->m_source, mp->m_net_netdrv_dl_getstat_s.grant,
		sizeof(dep->de_stat), &dep->de_stat);

	mp->m_type= DL_STAT_REPLY;
	r= ipc_send(mp->m_source, mp);
	if (r != OK)
		panic("do_getstat: ipc_send failed: %d", r);
}

/*===========================================================================*
 *				dp_stop					     *
 *===========================================================================*/
static void dp_stop(dep)
dpeth_t *dep;
{

	if (dep->de_mode == DEM_SINK)
		return;
	assert(dep->de_mode == DEM_ENABLED);

	if (!(dep->de_flags & DEF_ENABLED))
		return;

	outb_reg0(dep, DP_CR, CR_STP | CR_DM_ABORT);
	(dep->de_stopf)(dep);

	dep->de_flags= DEF_EMPTY;
}

/*===========================================================================*
 *				dp_init					     *
 *===========================================================================*/
static void dp_init(dep)
dpeth_t *dep;
{
	int dp_rcr_reg;
	int i, r;

	/* General initialization */
	dep->de_flags = DEF_EMPTY;
	(*dep->de_initf)(dep);

	dp_confaddr(dep);

	if (debug)
	{
		printf("%s: Ethernet address ", dep->de_name);
		for (i= 0; i < 6; i++)
			printf("%x%c", dep->de_address.ea_addr[i],
							i < 5 ? ':' : '\n');
	}

	/* Map buffer */
	map_hw_buffer(dep);

	/* Initialization of the dp8390 following the mandatory procedure
	 * in reference manual ("DP8390D/NS32490D NIC Network Interface
	 * Controller", National Semiconductor, July 1995, Page 29).
	 */
	/* Step 1: */
	outb_reg0(dep, DP_CR, CR_PS_P0 | CR_STP | CR_DM_ABORT);
	/* Step 2: */
	if (dep->de_16bit)
		outb_reg0(dep, DP_DCR, DCR_WORDWIDE | DCR_8BYTES | DCR_BMS);
	else
		outb_reg0(dep, DP_DCR, DCR_BYTEWIDE | DCR_8BYTES | DCR_BMS);
	/* Step 3: */
	outb_reg0(dep, DP_RBCR0, 0);
	outb_reg0(dep, DP_RBCR1, 0);
	/* Step 4: */
	dp_rcr_reg = 0;
	if (dep->de_flags & DEF_PROMISC)
		dp_rcr_reg |= RCR_AB | RCR_PRO | RCR_AM;
	if (dep->de_flags & DEF_BROAD)
		dp_rcr_reg |= RCR_AB;
	if (dep->de_flags & DEF_MULTI)
		dp_rcr_reg |= RCR_AM;
	outb_reg0(dep, DP_RCR, dp_rcr_reg);
	/* Step 5: */
	outb_reg0(dep, DP_TCR, TCR_INTERNAL);
	/* Step 6: */
	outb_reg0(dep, DP_BNRY, dep->de_startpage);
	outb_reg0(dep, DP_PSTART, dep->de_startpage);
	outb_reg0(dep, DP_PSTOP, dep->de_stoppage);
	/* Step 7: */
	outb_reg0(dep, DP_ISR, 0xFF);
	/* Step 8: */
	outb_reg0(dep, DP_IMR, IMR_PRXE | IMR_PTXE | IMR_RXEE | IMR_TXEE |
		IMR_OVWE | IMR_CNTE);
	/* Step 9: */
	outb_reg0(dep, DP_CR, CR_PS_P1 | CR_DM_ABORT | CR_STP);

	outb_reg1(dep, DP_PAR0, dep->de_address.ea_addr[0]);
	outb_reg1(dep, DP_PAR1, dep->de_address.ea_addr[1]);
	outb_reg1(dep, DP_PAR2, dep->de_address.ea_addr[2]);
	outb_reg1(dep, DP_PAR3, dep->de_address.ea_addr[3]);
	outb_reg1(dep, DP_PAR4, dep->de_address.ea_addr[4]);
	outb_reg1(dep, DP_PAR5, dep->de_address.ea_addr[5]);

	outb_reg1(dep, DP_MAR0, 0xff);
	outb_reg1(dep, DP_MAR1, 0xff);
	outb_reg1(dep, DP_MAR2, 0xff);
	outb_reg1(dep, DP_MAR3, 0xff);
	outb_reg1(dep, DP_MAR4, 0xff);
	outb_reg1(dep, DP_MAR5, 0xff);
	outb_reg1(dep, DP_MAR6, 0xff);
	outb_reg1(dep, DP_MAR7, 0xff);

	outb_reg1(dep, DP_CURR, dep->de_startpage + 1);
	/* Step 10: */
	outb_reg0(dep, DP_CR, CR_DM_ABORT | CR_STA);
	/* Step 11: */
	outb_reg0(dep, DP_TCR, TCR_NORMAL);

	inb_reg0(dep, DP_CNTR0);		/* reset counters by reading */
	inb_reg0(dep, DP_CNTR1);
	inb_reg0(dep, DP_CNTR2);

	/* Finish the initialization. */
	dep->de_flags |= DEF_ENABLED;
	for (i= 0; i<dep->de_sendq_nr; i++)
		dep->de_sendq[i].sq_filled= 0;
	dep->de_sendq_head= 0;
	dep->de_sendq_tail= 0;
	if (!dep->de_prog_IO)
	{
		dep->de_user2nicf_s= dp_user2nic_s;
		dep->de_nic2userf_s= dp_nic2user_s;
		dep->de_getblockf= dp_getblock;
	}
	else if (dep->de_16bit)
	{
		dep->de_user2nicf_s= dp_pio16_user2nic_s;
		dep->de_nic2userf_s= dp_pio16_nic2user_s;
		dep->de_getblockf= dp_pio16_getblock;
	}
	else
	{
		dep->de_user2nicf_s= dp_pio8_user2nic_s;
		dep->de_nic2userf_s= dp_pio8_nic2user_s;
		dep->de_getblockf= dp_pio8_getblock;
	}

	/* Set the interrupt handler and policy. Do not automatically 
	 * reenable interrupts. Return the IRQ line number on interrupts.
 	 */
 	dep->de_hook = dep->de_irq;
	r= sys_irqsetpolicy(dep->de_irq, 0, &dep->de_hook);
	if (r != OK)
		panic("sys_irqsetpolicy failed: %d", r);

	r= sys_irqenable(&dep->de_hook);
	if (r != OK)
	{
		panic("unable enable interrupts: %d", r);
	}
}

/*===========================================================================*
 *				dp_confaddr				     *
 *===========================================================================*/
static void dp_confaddr(dep)
dpeth_t *dep;
{
	int i;
	char eakey[16];
	static char eafmt[]= "x:x:x:x:x:x";
	long v;

	/* User defined ethernet address? */
	strlcpy(eakey, "DPETH0_EA", sizeof(eakey));
	eakey[5] += de_instance;

	for (i= 0; i < 6; i++)
	{
		v= dep->de_address.ea_addr[i];
		if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
		{
			break;
		}
		dep->de_address.ea_addr[i]= v;
	}

	if (i != 0 && i != 6) env_panic(eakey);	/* It's all or nothing */
}

/*===========================================================================*
 *				dp_reinit				     *
 *===========================================================================*/
static void dp_reinit(dep)
dpeth_t *dep;
{
	int dp_rcr_reg;

	outb_reg0(dep, DP_CR, CR_PS_P0 | CR_EXTRA);

	dp_rcr_reg = 0;
	if (dep->de_flags & DEF_PROMISC)
		dp_rcr_reg |= RCR_AB | RCR_PRO | RCR_AM;
	if (dep->de_flags & DEF_BROAD)
		dp_rcr_reg |= RCR_AB;
	if (dep->de_flags & DEF_MULTI)
		dp_rcr_reg |= RCR_AM;
	outb_reg0(dep, DP_RCR, dp_rcr_reg);
}

/*===========================================================================*
 *				dp_reset				     *
 *===========================================================================*/
static void dp_reset(dep)
dpeth_t *dep;
{
	int i;

	/* Stop chip */
	outb_reg0(dep, DP_CR, CR_STP | CR_DM_ABORT);
	outb_reg0(dep, DP_RBCR0, 0);
	outb_reg0(dep, DP_RBCR1, 0);
	for (i= 0; i < 0x1000 && ((inb_reg0(dep, DP_ISR) & ISR_RST) == 0); i++)
		; /* Do nothing */
	outb_reg0(dep, DP_TCR, TCR_1EXTERNAL|TCR_OFST);
	outb_reg0(dep, DP_CR, CR_STA|CR_DM_ABORT);
	outb_reg0(dep, DP_TCR, TCR_NORMAL);

	/* Acknowledge the ISR_RDC (remote dma) interrupt. */
	for (i= 0; i < 0x1000 && ((inb_reg0(dep, DP_ISR) & ISR_RDC) == 0); i++)
		; /* Do nothing */
	outb_reg0(dep, DP_ISR, inb_reg0(dep, DP_ISR) & ~ISR_RDC);

	/* Reset the transmit ring. If we were transmitting a packet, we
	 * pretend that the packet is processed. Higher layers will
	 * retransmit if the packet wasn't actually sent.
	 */
	dep->de_sendq_head= dep->de_sendq_tail= 0;
	for (i= 0; i<dep->de_sendq_nr; i++)
		dep->de_sendq[i].sq_filled= 0;
	dp_send(dep);
	dep->de_flags &= ~DEF_STOPPED;
}

/*===========================================================================*
 *				dp_check_ints				     *
 *===========================================================================*/
static void dp_check_ints(dep)
dpeth_t *dep;
{
	int isr, tsr;
	int size, sendq_tail;

	if (!(dep->de_flags & DEF_ENABLED))
		panic("dp8390: got premature interrupt");

	for(;;)
	{
		isr = inb_reg0(dep, DP_ISR);
		if (!isr)
			break;
		outb_reg0(dep, DP_ISR, isr);
		if (isr & (ISR_PTX|ISR_TXE))
		{
			if (isr & ISR_TXE)
			{
#if DEBUG
 { printf("%s: got send Error\n", dep->de_name); }
#endif
				dep->de_stat.ets_sendErr++;
			}
			else
			{
				tsr = inb_reg0(dep, DP_TSR);

				if (tsr & TSR_PTX) dep->de_stat.ets_packetT++;
#if 0	/* Reserved in later manuals, should be ignored */
				if (!(tsr & TSR_DFR))
				{
					/* In most (all?) implementations of
					 * the dp8390, this bit is set
					 * when the packet is not deferred
					 */
					dep->de_stat.ets_transDef++;
				}
#endif
				if (tsr & TSR_COL) dep->de_stat.ets_collision++;
				if (tsr & TSR_ABT) dep->de_stat.ets_transAb++;
				if (tsr & TSR_CRS) dep->de_stat.ets_carrSense++;
				if (tsr & TSR_FU
					&& ++dep->de_stat.ets_fifoUnder <= 10)
				{
					printf("%s: fifo underrun\n",
						dep->de_name);
				}
				if (tsr & TSR_CDH
					&& ++dep->de_stat.ets_CDheartbeat <= 10)
				{
					printf("%s: CD heart beat failure\n",
						dep->de_name);
				}
				if (tsr & TSR_OWC) dep->de_stat.ets_OWC++;
			}
			sendq_tail= dep->de_sendq_tail;

			if (!(dep->de_sendq[sendq_tail].sq_filled))
			{
				/* Software bug? */
				assert(!debug);

				/* Or hardware bug? */
				printf(
				"%s: transmit interrupt, but not sending\n",
					dep->de_name);
				continue;
			}
			dep->de_sendq[sendq_tail].sq_filled= 0;
			if (++sendq_tail == dep->de_sendq_nr)
				sendq_tail= 0;
			dep->de_sendq_tail= sendq_tail;
			if (dep->de_sendq[sendq_tail].sq_filled)
			{
				size= dep->de_sendq[sendq_tail].sq_size;
				outb_reg0(dep, DP_TPSR,
					dep->de_sendq[sendq_tail].sq_sendpage);
				outb_reg0(dep, DP_TBCR1, size >> 8);
				outb_reg0(dep, DP_TBCR0, size & 0xff);
				outb_reg0(dep, DP_CR, CR_TXP | CR_EXTRA);
			}
			if (dep->de_flags & DEF_SEND_AVAIL)
				dp_send(dep);
		}

		if (isr & ISR_PRX)
		{
			/* Only call dp_recv if there is a read request */
			if (dep->de_flags & DEF_READING)
				dp_recv(dep);
		}
		
		if (isr & ISR_RXE) dep->de_stat.ets_recvErr++;
		if (isr & ISR_CNT)
		{
			dep->de_stat.ets_CRCerr += inb_reg0(dep, DP_CNTR0);
			dep->de_stat.ets_frameAll += inb_reg0(dep, DP_CNTR1);
			dep->de_stat.ets_missedP += inb_reg0(dep, DP_CNTR2);
		}
		if (isr & ISR_OVW)
		{
			dep->de_stat.ets_OVW++;
#if 0
			{ printW(); printf(
				"%s: got overwrite warning\n", dep->de_name); }
#endif
			if (dep->de_flags & DEF_READING)
			{
				printf(
"dp_check_ints: strange: overwrite warning and pending read request\n");
				dp_recv(dep);
			}
		}
		if (isr & ISR_RDC)
		{
			/* Nothing to do */
		}
		if (isr & ISR_RST)
		{
			/* this means we got an interrupt but the ethernet 
			 * chip is shutdown. We set the flag DEF_STOPPED,
			 * and continue processing arrived packets. When the
			 * receive buffer is empty, we reset the dp8390.
			 */
#if 0
			 { printW(); printf(
				"%s: NIC stopped\n", dep->de_name); }
#endif
			dep->de_flags |= DEF_STOPPED;
			break;
		}
	}
	if ((dep->de_flags & (DEF_READING|DEF_STOPPED)) == 
						(DEF_READING|DEF_STOPPED))
	{
		/* The chip is stopped, and all arrived packets are 
		 * delivered.
		 */
		dp_reset(dep);
	}
}

/*===========================================================================*
 *				dp_recv					     *
 *===========================================================================*/
static void dp_recv(dep)
dpeth_t *dep;
{
	dp_rcvhdr_t header;
	unsigned pageno, curr, next;
	vir_bytes length;
	int packet_processed, r;
	u16_t eth_type;

	packet_processed = FALSE;
	pageno = inb_reg0(dep, DP_BNRY) + 1;
	if (pageno == dep->de_stoppage) pageno = dep->de_startpage;

	do
	{
		outb_reg0(dep, DP_CR, CR_PS_P1 | CR_EXTRA);
		curr = inb_reg1(dep, DP_CURR);
		outb_reg0(dep, DP_CR, CR_PS_P0 | CR_EXTRA);

		if (curr == pageno) break;

		(dep->de_getblockf)(dep, pageno, (size_t)0, sizeof(header),
			&header);
		(dep->de_getblockf)(dep, pageno, sizeof(header) +
			2*sizeof(ether_addr_t), sizeof(eth_type), &eth_type);

		length = (header.dr_rbcl | (header.dr_rbch << 8)) -
			sizeof(dp_rcvhdr_t);
		next = header.dr_next;
		if (length < ETH_MIN_PACK_SIZE ||
			length > ETH_MAX_PACK_SIZE_TAGGED)
		{
			printf("%s: packet with strange length arrived: %d\n",
				dep->de_name, (int) length);
			next= curr;
		}
		else if (next < dep->de_startpage || next >= dep->de_stoppage)
		{
			printf("%s: strange next page\n", dep->de_name);
			next= curr;
		}
		else if (header.dr_status & RSR_FO)
		{
			/* This is very serious, so we issue a warning and
			 * reset the buffers */
			printf("%s: fifo overrun, resetting receive buffer\n",
				dep->de_name);
			dep->de_stat.ets_fifoOver++;
			next = curr;
		}
		else if ((header.dr_status & RSR_PRX) &&
					   (dep->de_flags & DEF_ENABLED))
		{
			r = dp_pkt2user_s(dep, pageno, length);
			if (r != OK)
				return;

			packet_processed = TRUE;
			dep->de_stat.ets_packetR++;
		}
		if (next == dep->de_startpage)
			outb_reg0(dep, DP_BNRY, dep->de_stoppage - 1);
		else
			outb_reg0(dep, DP_BNRY, next - 1);

		pageno = next;
	}
	while (!packet_processed);
}

/*===========================================================================*
 *				dp_send					     *
 *===========================================================================*/
static void dp_send(dep)
dpeth_t *dep;
{
	if (!(dep->de_flags & DEF_SEND_AVAIL))
		return;

	dep->de_flags &= ~DEF_SEND_AVAIL;
	do_vwrite_s(&dep->de_sendmsg, TRUE);
}

/*===========================================================================*
 *				dp_getblock				     *
 *===========================================================================*/
static void dp_getblock(dep, page, offset, size, dst)
dpeth_t *dep;
int page;
size_t offset;
size_t size;
void *dst;
{
	offset = page * DP_PAGESIZE + offset;

	memcpy(dst, dep->de_locmem + offset, size);
}

/*===========================================================================*
 *				dp_pio8_getblock			     *
 *===========================================================================*/
static void dp_pio8_getblock(dep, page, offset, size, dst)
dpeth_t *dep;
int page;
size_t offset;
size_t size;
void *dst;
{
	offset = page * DP_PAGESIZE + offset;
	outb_reg0(dep, DP_RBCR0, size & 0xFF);
	outb_reg0(dep, DP_RBCR1, size >> 8);
	outb_reg0(dep, DP_RSAR0, offset & 0xFF);
	outb_reg0(dep, DP_RSAR1, offset >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RR | CR_PS_P0 | CR_STA);

	insb(dep->de_data_port, dst, size);
}

/*===========================================================================*
 *				dp_pio16_getblock			     *
 *===========================================================================*/
static void dp_pio16_getblock(dep, page, offset, size, dst)
dpeth_t *dep;
int page;
size_t offset;
size_t size;
void *dst;
{
	offset = page * DP_PAGESIZE + offset;
	outb_reg0(dep, DP_RBCR0, size & 0xFF);
	outb_reg0(dep, DP_RBCR1, size >> 8);
	outb_reg0(dep, DP_RSAR0, offset & 0xFF);
	outb_reg0(dep, DP_RSAR1, offset >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RR | CR_PS_P0 | CR_STA);

	assert (!(size & 1));
	insw(dep->de_data_port, dst, size);
}

/*===========================================================================*
 *				dp_pkt2user_s				     *
 *===========================================================================*/
static int dp_pkt2user_s(dpeth_t *dep, int page, vir_bytes length)
{
	int last, count;

	if (!(dep->de_flags & DEF_READING))
		return EGENERIC;

	last = page + (length - 1) / DP_PAGESIZE;
	if (last >= dep->de_stoppage)
	{
		count = (dep->de_stoppage - page) * DP_PAGESIZE -
			sizeof(dp_rcvhdr_t);

		/* Save read_iovec since we need it twice. */
		dep->de_tmp_iovec_s = dep->de_read_iovec_s;
		(dep->de_nic2userf_s)(dep, page * DP_PAGESIZE +
			sizeof(dp_rcvhdr_t), &dep->de_tmp_iovec_s, 0, count);
		(dep->de_nic2userf_s)(dep, dep->de_startpage * DP_PAGESIZE, 
				&dep->de_read_iovec_s, count, length - count);
	}
	else
	{
		(dep->de_nic2userf_s)(dep, page * DP_PAGESIZE +
			sizeof(dp_rcvhdr_t), &dep->de_read_iovec_s, 0, length);
	}

	dep->de_read_s = length;
	dep->de_flags |= DEF_PACK_RECV;
	dep->de_flags &= ~DEF_READING;

	return OK;
}

/*===========================================================================*
 *				dp_user2nic_s				     *
 *===========================================================================*/
static void dp_user2nic_s(dep, iovp, offset, nic_addr, count)
dpeth_t *dep;
iovec_dat_s_t *iovp;
vir_bytes offset;
int nic_addr;
vir_bytes count;
{
	vir_bytes vir_hw;
	int bytes, i, r;

	vir_hw = (vir_bytes)dep->de_locmem + nic_addr;

	i= 0;
	while (count > 0)
	{
		if (i >= IOVEC_NR)
		{
			dp_next_iovec_s(iovp);
			i= 0;
			continue;
		}
		assert(i < iovp->iod_iovec_s);
		if (offset >= iovp->iod_iovec[i].iov_size)
		{
			offset -= iovp->iod_iovec[i].iov_size;
			i++;
			continue;
		}
		bytes = iovp->iod_iovec[i].iov_size - offset;
		if (bytes > count)
			bytes = count;

		r= sys_safecopyfrom(iovp->iod_proc_nr,
			iovp->iod_iovec[i].iov_grant, offset,
			vir_hw, bytes);
		if (r != OK) {
			panic("dp_user2nic_s: sys_safecopyfrom failed: %d", r);
		}

		count -= bytes;
		vir_hw += bytes;
		offset += bytes;
	}
	assert(count == 0);
}

/*===========================================================================*
 *				dp_pio8_user2nic_s			     *
 *===========================================================================*/
static void dp_pio8_user2nic_s(dep, iovp, offset, nic_addr, count)
dpeth_t *dep;
iovec_dat_s_t *iovp;
vir_bytes offset;
int nic_addr;
vir_bytes count;
{
	int bytes, i, r;

	outb_reg0(dep, DP_ISR, ISR_RDC);

	outb_reg0(dep, DP_RBCR0, count & 0xFF);
	outb_reg0(dep, DP_RBCR1, count >> 8);
	outb_reg0(dep, DP_RSAR0, nic_addr & 0xFF);
	outb_reg0(dep, DP_RSAR1, nic_addr >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RW | CR_PS_P0 | CR_STA);

	i= 0;
	while (count > 0)
	{
		if (i >= IOVEC_NR)
		{
			dp_next_iovec_s(iovp);
			i= 0;
			continue;
		}
		assert(i < iovp->iod_iovec_s);
		if (offset >= iovp->iod_iovec[i].iov_size)
		{
			offset -= iovp->iod_iovec[i].iov_size;
			i++;
			continue;
		}
		bytes = iovp->iod_iovec[i].iov_size - offset;
		if (bytes > count)
			bytes = count;

		r= sys_safe_outsb(dep->de_data_port, iovp->iod_proc_nr,
			iovp->iod_iovec[i].iov_grant, offset, bytes);
		if (r != OK) {
				panic("dp_pio8_user2nic_s: sys_safe_outsb failed: %d",
					r);
		}
		count -= bytes;
		offset += bytes;
	}
	assert(count == 0);

	for (i= 0; i<100; i++)
	{
		if (inb_reg0(dep, DP_ISR) & ISR_RDC)
			break;
	}
	if (i == 100)
	{
		panic("dp8390: remote dma failed to complete");
	}
}

/*===========================================================================*
 *				dp_pio16_user2nic_s			     *
 *===========================================================================*/
static void dp_pio16_user2nic_s(dep, iovp, offset, nic_addr, count)
dpeth_t *dep;
iovec_dat_s_t *iovp;
vir_bytes offset;
int nic_addr;
vir_bytes count;
{
	vir_bytes ecount;
	cp_grant_id_t gid;
	int i, r, bytes, user_proc;
	u8_t two_bytes[2];
	int odd_byte;

	ecount= (count+1) & ~1;
	odd_byte= 0;

	outb_reg0(dep, DP_ISR, ISR_RDC);
	outb_reg0(dep, DP_RBCR0, ecount & 0xFF);
	outb_reg0(dep, DP_RBCR1, ecount >> 8);
	outb_reg0(dep, DP_RSAR0, nic_addr & 0xFF);
	outb_reg0(dep, DP_RSAR1, nic_addr >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RW | CR_PS_P0 | CR_STA);

	i= 0;
	while (count > 0)
	{
		if (i >= IOVEC_NR)
		{
			dp_next_iovec_s(iovp);
			i= 0;
			continue;
		}
		assert(i < iovp->iod_iovec_s);
		if (offset >= iovp->iod_iovec[i].iov_size)
		{
			offset -= iovp->iod_iovec[i].iov_size;
			i++;
			continue;
		}
		bytes = iovp->iod_iovec[i].iov_size - offset;
		if (bytes > count)
			bytes = count;

		user_proc= iovp->iod_proc_nr;
		gid= iovp->iod_iovec[i].iov_grant;
		if (odd_byte)
		{
			r= sys_safecopyfrom(user_proc, gid, offset, 
				(vir_bytes)&two_bytes[1], 1);
			if (r != OK) { 
				panic("dp_pio16_user2nic: sys_safecopyfrom failed: %d", r);
			}
			outw(dep->de_data_port, *(u16_t *)two_bytes);
			count--;
			offset++;
			bytes--;
			odd_byte= 0;
			if (!bytes)
				continue;
		}
		ecount= bytes & ~1;
		if (ecount != 0)
		{
			r= sys_safe_outsw(dep->de_data_port, user_proc,
				gid, offset, ecount);
			if (r != OK) {
				panic("dp_pio16_user2nic: sys_safe_outsw failed: %d", r);
			}
			count -= ecount;
			offset += ecount;
			bytes -= ecount;
		}
		if (bytes)
		{
			assert(bytes == 1);
			r= sys_safecopyfrom(user_proc, gid, offset,
				(vir_bytes)&two_bytes[0], 1);
			if (r != OK) {
				panic("dp_pio16_user2nic: sys_safecopyfrom failed: %d", r);
			}
			count--;
			offset++;
			bytes--;
			odd_byte= 1;
		}
	}
	assert(count == 0);

	if (odd_byte)
		outw(dep->de_data_port, *(u16_t *)two_bytes);

	for (i= 0; i<100; i++)
	{
		if (inb_reg0(dep, DP_ISR) & ISR_RDC)
			break;
	}
	if (i == 100)
	{
		panic("dp8390: remote dma failed to complete");
	}
}

/*===========================================================================*
 *				dp_nic2user_s				     *
 *===========================================================================*/
static void dp_nic2user_s(dep, nic_addr, iovp, offset, count)
dpeth_t *dep;
int nic_addr;
iovec_dat_s_t *iovp;
vir_bytes offset;
vir_bytes count;
{
	vir_bytes vir_hw;
	int bytes, i, r;

	vir_hw = (vir_bytes)dep->de_locmem + nic_addr;

	i= 0;
	while (count > 0)
	{
		if (i >= IOVEC_NR)
		{
			dp_next_iovec_s(iovp);
			i= 0;
			continue;
		}
		assert(i < iovp->iod_iovec_s);
		if (offset >= iovp->iod_iovec[i].iov_size)
		{
			offset -= iovp->iod_iovec[i].iov_size;
			i++;
			continue;
		}
		bytes = iovp->iod_iovec[i].iov_size - offset;
		if (bytes > count)
			bytes = count;

		r= sys_safecopyto(iovp->iod_proc_nr,
			iovp->iod_iovec[i].iov_grant, offset,
			vir_hw, bytes);
		if (r != OK)
			panic("dp_nic2user_s: sys_safecopyto failed: %d", r);

		count -= bytes;
		vir_hw += bytes;
		offset += bytes;
	}
	assert(count == 0);
}

/*===========================================================================*
 *				dp_pio8_nic2user_s			     *
 *===========================================================================*/
static void dp_pio8_nic2user_s(dep, nic_addr, iovp, offset, count)
dpeth_t *dep;
int nic_addr;
iovec_dat_s_t *iovp;
vir_bytes offset;
vir_bytes count;
{
	int bytes, i, r;

	outb_reg0(dep, DP_RBCR0, count & 0xFF);
	outb_reg0(dep, DP_RBCR1, count >> 8);
	outb_reg0(dep, DP_RSAR0, nic_addr & 0xFF);
	outb_reg0(dep, DP_RSAR1, nic_addr >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RR | CR_PS_P0 | CR_STA);

	i= 0;
	while (count > 0)
	{
		if (i >= IOVEC_NR)
		{
			dp_next_iovec_s(iovp);
			i= 0;
			continue;
		}
		assert(i < iovp->iod_iovec_s);
		if (offset >= iovp->iod_iovec[i].iov_size)
		{
			offset -= iovp->iod_iovec[i].iov_size;
			i++;
			continue;
		}
		bytes = iovp->iod_iovec[i].iov_size - offset;
		if (bytes > count)
			bytes = count;

		r= sys_safe_insb(dep->de_data_port, iovp->iod_proc_nr,
			iovp->iod_iovec[i].iov_grant, offset, bytes);
		if (r != OK) {
			panic("dp_pio8_nic2user_s: sys_safe_insb failed: %d", r);
		}
		count -= bytes;
		offset += bytes;
	}
	assert(count == 0);
}

/*===========================================================================*
 *				dp_pio16_nic2user_s			     *
 *===========================================================================*/
static void dp_pio16_nic2user_s(dep, nic_addr, iovp, offset, count)
dpeth_t *dep;
int nic_addr;
iovec_dat_s_t *iovp;
vir_bytes offset;
vir_bytes count;
{
	vir_bytes ecount;
	cp_grant_id_t gid;
	int i, r, bytes, user_proc;
	u8_t two_bytes[2];
	int odd_byte;

	ecount= (count+1) & ~1;
	odd_byte= 0;

	outb_reg0(dep, DP_RBCR0, ecount & 0xFF);
	outb_reg0(dep, DP_RBCR1, ecount >> 8);
	outb_reg0(dep, DP_RSAR0, nic_addr & 0xFF);
	outb_reg0(dep, DP_RSAR1, nic_addr >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RR | CR_PS_P0 | CR_STA);

	i= 0;
	while (count > 0)
	{
		if (i >= IOVEC_NR)
		{
			dp_next_iovec_s(iovp);
			i= 0;
			continue;
		}
		assert(i < iovp->iod_iovec_s);
		if (offset >= iovp->iod_iovec[i].iov_size)
		{
			offset -= iovp->iod_iovec[i].iov_size;
			i++;
			continue;
		}
		bytes = iovp->iod_iovec[i].iov_size - offset;
		if (bytes > count)
			bytes = count;

		user_proc= iovp->iod_proc_nr;
		gid= iovp->iod_iovec[i].iov_grant;
		if (odd_byte)
		{
			r= sys_safecopyto(user_proc, gid, offset,
				(vir_bytes)&two_bytes[1], 1);
			if (r != OK) {
				panic("dp_pio16_nic2user: sys_safecopyto failed: %d", r);
			}
			count--;
			offset++;
			bytes--;
			odd_byte= 0;
			if (!bytes)
				continue;
		}
		ecount= bytes & ~1;
		if (ecount != 0)
		{
			r= sys_safe_insw(dep->de_data_port, user_proc, gid,
				offset, ecount);
			if (r != OK) {
				panic("dp_pio16_nic2user: sys_safe_insw failed: %d",
				r);
			}
			count -= ecount;
			offset += ecount;
			bytes -= ecount;
		}
		if (bytes)
		{
			assert(bytes == 1);
			*(u16_t *)two_bytes= inw(dep->de_data_port);
			r= sys_safecopyto(user_proc, gid, offset,
				(vir_bytes)&two_bytes[0], 1);
			if (r != OK)
			{
				panic("dp_pio16_nic2user: sys_safecopyto failed: %d",
					r);
			}
			count--;
			offset++;
			bytes--;
			odd_byte= 1;
		}
	}
	assert(count == 0);
}

/*===========================================================================*
 *				dp_next_iovec_s				     *
 *===========================================================================*/
static void dp_next_iovec_s(iovp)
iovec_dat_s_t *iovp;
{
	assert(iovp->iod_iovec_s > IOVEC_NR);

	iovp->iod_iovec_s -= IOVEC_NR;

	iovp->iod_iovec_offset += IOVEC_NR * sizeof(iovec_t);

	get_userdata_s(iovp->iod_proc_nr, iovp->iod_grant,
		iovp->iod_iovec_offset, 
		(iovp->iod_iovec_s > IOVEC_NR ? IOVEC_NR : iovp->iod_iovec_s) *
		sizeof(iovp->iod_iovec[0]), iovp->iod_iovec); 
}

/*===========================================================================*
 *				conf_hw					     *
 *===========================================================================*/
static void conf_hw(dep)
dpeth_t *dep;
{
	static eth_stat_t empty_stat = {0, 0, 0, 0, 0, 0 	/* ,... */ };

	int confnr;
	dp_conf_t *dcp;

	dep->de_mode= DEM_DISABLED;	/* Superfluous */

	/* Pick a default configuration for this instance. */
	confnr= MIN(de_instance, DP_CONF_NR-1);

	dcp= &dp_conf[confnr];
	update_conf(dep, dcp);
	if (dep->de_mode != DEM_ENABLED)
		return;
	if (!wdeth_probe(dep) && !ne_probe(dep) && !el2_probe(dep))
	{
		printf("%s: No ethernet card found at 0x%x\n", 
			dep->de_name, dep->de_base_port);
		dep->de_mode= DEM_DISABLED;
		return;
	}

/* XXX */ if (dep->de_linmem == 0) dep->de_linmem= 0xFFFF0000;

	dep->de_flags = DEF_EMPTY;
	dep->de_stat = empty_stat;
}

/*===========================================================================*
 *				update_conf				     *
 *===========================================================================*/
static void update_conf(dep, dcp)
dpeth_t *dep;
dp_conf_t *dcp;
{
	long v;
	static char dpc_fmt[] = "x:d:x:x";
	char eckey[16];

#if ENABLE_PCI
	if (dep->de_pci)
	{
		if (dep->de_pci == 1)
		{
			/* PCI device is present */
			dep->de_mode= DEM_ENABLED;
		}
		return;		/* Already configured */
	}
#endif

	strlcpy(eckey, "DPETH0", sizeof(eckey));
	eckey[5] += de_instance;

	/* Get the default settings and modify them from the environment. */
	dep->de_mode= DEM_SINK;
	v= dcp->dpc_port;
	switch (env_parse(eckey, dpc_fmt, 0, &v, 0x0000L, 0xFFFFL)) {
	case EP_OFF:
		dep->de_mode= DEM_DISABLED;
		break;
	case EP_ON:
	case EP_SET:
		dep->de_mode= DEM_ENABLED;	/* Might become disabled if 
						 * all probes fail */
		break;
	}
	dep->de_base_port= v;

	v= dcp->dpc_irq | DEI_DEFAULT;
	(void) env_parse(eckey, dpc_fmt, 1, &v, 0L, (long) NR_IRQ_VECTORS - 1);
	dep->de_irq= v;

	v= dcp->dpc_mem;
	(void) env_parse(eckey, dpc_fmt, 2, &v, 0L, 0xFFFFFL);
	dep->de_linmem= v;

	v= 0;
	(void) env_parse(eckey, dpc_fmt, 3, &v, 0x2000L, 0x8000L);
	dep->de_ramsize= v;
}

/*===========================================================================*
 *				map_hw_buffer				     *
 *===========================================================================*/
static void map_hw_buffer(dep)
dpeth_t *dep;
{

	if (dep->de_prog_IO)
	{
#if 0
		printf(
		"map_hw_buffer: programmed I/O, no need to map buffer\n");
#endif
		dep->de_locmem = (char *)-dep->de_ramsize; /* trap errors */
		return;
	}

	dep->de_locmem=
		vm_map_phys(SELF, (void *) dep->de_linmem, dep->de_ramsize);
	if (dep->de_locmem == MAP_FAILED)
		panic("map_hw_buffer: vm_map_phys failed");
}

/*===========================================================================*
 *				calc_iovec_size_s			     *
 *===========================================================================*/
static int calc_iovec_size_s(iovp)
iovec_dat_s_t *iovp;
{
	/* Calculate the size of a request. Note that the iovec_dat
	 * structure will be unusable after calc_iovec_size_s.
	 */
	int size;
	int i;

	size= 0;
	i= 0;
	while (i < iovp->iod_iovec_s)
	{
		if (i >= IOVEC_NR)
		{
			dp_next_iovec_s(iovp);
			i= 0;
			continue;
		}
		size += iovp->iod_iovec[i].iov_size;
		i++;
	}
	return size;
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
static void reply(dep)
dpeth_t *dep;
{
	message reply;
	int flags;
	int r;

	flags = DL_NOFLAGS;
	if (dep->de_flags & DEF_PACK_SEND)
		flags |= DL_PACK_SEND;
	if (dep->de_flags & DEF_PACK_RECV)
		flags |= DL_PACK_RECV;

	reply.m_type = DL_TASK_REPLY;
	reply.m_netdrv_net_dl_task.flags = flags;
	reply.m_netdrv_net_dl_task.count = dep->de_read_s;
	r= ipc_send(dep->de_client, &reply);

	if (r < 0)
		panic("dp8390: ipc_send failed: %d", r);
	
	dep->de_read_s = 0;
	dep->de_flags &= ~(DEF_PACK_SEND | DEF_PACK_RECV);
}

/*===========================================================================*
 *				mess_reply				     *
 *===========================================================================*/
static void mess_reply(req, reply_mess)
message *req;
message *reply_mess;
{
	if (ipc_send(req->m_source, reply_mess) != OK)
		panic("dp8390: unable to mess_reply");
}

/*===========================================================================*
 *				get_userdata_s				     *
 *===========================================================================*/
static void get_userdata_s(user_proc, grant, offset, count, loc_addr)
int user_proc;
cp_grant_id_t grant;
vir_bytes offset;
vir_bytes count;
void *loc_addr;
{
	int r;

	r= sys_safecopyfrom(user_proc, grant, offset,
		(vir_bytes)loc_addr, count);
	if (r != OK)
		panic("get_userdata: sys_safecopyfrom failed: %d", r);
}

/*===========================================================================*
 *				put_userdata_s				     *
 *===========================================================================*/
static void put_userdata_s(user_proc, grant, count, loc_addr)
int user_proc;
cp_grant_id_t grant;
size_t count;
void *loc_addr;
{
	int r;

	r= sys_safecopyto(user_proc, grant, 0, (vir_bytes)loc_addr, 
		count);
	if (r != OK)
		panic("put_userdata: sys_safecopyto failed: %d", r);
}

u8_t inb(port_t port)
{
	int r;
	u32_t value;

	r= sys_inb(port, &value);
	if (r != OK)
	{
		printf("inb failed for port 0x%x\n", port);
		panic("sys_inb failed: %d", r);
	}
	return value;
}

u16_t inw(port_t port)
{
	int r;
	u32_t value;

	r= sys_inw(port, &value);
	if (r != OK)
		panic("sys_inw failed: %d", r);
	return (u16_t) value;
}

void outb(port_t port, u8_t value)
{
	int r;

	r= sys_outb(port, value);
	if (r != OK)
		panic("sys_outb failed: %d", r);
}

void outw(port_t port, u16_t value)
{
	int r;

	r= sys_outw(port, value);
	if (r != OK)
		panic("sys_outw failed: %d", r);
}

static void insb(port_t port, void *buf, size_t size)
{
	do_vir_insb(port, SELF, (vir_bytes)buf, size);
}

static void insw(port_t port, void *buf, size_t size)
{
	do_vir_insw(port, SELF, (vir_bytes)buf, size);
}

static void do_vir_insb(port_t port, int proc, vir_bytes buf, size_t size)
{
	int r;

	r= sys_insb(port, proc, (void *) buf, size);
	if (r != OK)
		panic("sys_sdevio failed: %d", r);
}

static void do_vir_insw(port_t port, int proc, vir_bytes buf, size_t size)
{
	int r;

	r= sys_insw(port, proc, (void *) buf, size);
	if (r != OK)
		panic("sys_sdevio failed: %d", r);
}

/*
 * $PchId: dp8390.c,v 1.25 2005/02/10 17:32:07 philip Exp $
 */
