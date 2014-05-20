/*
 * rtl8139.c
 *
 * This file contains a ethernet device driver for Realtek rtl8139 based
 * ethernet cards.
 *
 * Created:	Aug 2003 by Philip Homburg <philip@cs.vu.nl>
 * Changes:
 *   Aug 15, 2004   sync alarms replace watchdogs timers  (Jorrit N. Herder)
 *   May 02, 2004   flag alarms replace micro_elapsed()  (Jorrit N. Herder)
 *
 */

#define VERBOSE 0 /* Verbose debugging output */
#define RTL8139_FKEY 0 /* Use function key to dump RTL8139 status */

#include "rtl8139.h"

re_t re_state;

static int re_instance;

static unsigned my_inb(u16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inb(port, &value)) !=OK)
		printf("RTL8139: warning, sys_inb failed: %d\n", s);
	return value;
}
static unsigned my_inw(u16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inw(port, &value)) !=OK)
		printf("RTL8139: warning, sys_inw failed: %d\n", s);
	return value;
}
static unsigned my_inl(u16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inl(port, &value)) !=OK)
		printf("RTL8139: warning, sys_inl failed: %d\n", s);
	return value;
}
#define rl_inb(port, offset)	(my_inb((port) + (offset)))
#define rl_inw(port, offset)	(my_inw((port) + (offset)))
#define rl_inl(port, offset)	(my_inl((port) + (offset)))

static void my_outb(u16_t port, u8_t value) {
	int s;
	if ((s=sys_outb(port, value)) !=OK)
		printf("RTL8139: warning, sys_outb failed: %d\n", s);
}
static void my_outw(u16_t port, u16_t value) {
	int s;
	if ((s=sys_outw(port, value)) !=OK)
		printf("RTL8139: warning, sys_outw failed: %d\n", s);
}
static void my_outl(u16_t port, u32_t value) {
	int s;
	if ((s=sys_outl(port, value)) !=OK)
		printf("RTL8139: warning, sys_outl failed: %d\n", s);
}
#define rl_outb(port, offset, value)	(my_outb((port) + (offset), (value)))
#define rl_outw(port, offset, value)	(my_outw((port) + (offset), (value)))
#define rl_outl(port, offset, value)	(my_outl((port) + (offset), (value)))

static void rl_init(message *mp);
static void rl_pci_conf(void);
static int rl_probe(re_t *rep, int skip);
static void rl_conf_hw(re_t *rep);
static void rl_init_buf(re_t *rep);
static void rl_init_hw(re_t *rep);
static void rl_reset_hw(re_t *rep);
static void rl_confaddr(re_t *rep);
static void rl_rec_mode(re_t *rep);
static void rl_readv_s(const message *mp, int from_int);
static void rl_writev_s(const message *mp, int from_int);
static void rl_check_ints(re_t *rep);
static void rl_report_link(re_t *rep);
static void mii_print_techab(u16_t techab);
static void mii_print_stat_speed(u16_t stat, u16_t extstat);
static void rl_clear_rx(re_t *rep);
static void rl_do_reset(re_t *rep);
static void rl_getstat_s(message *mp);
static void reply(re_t *rep);
static void mess_reply(message *req, message *reply);
static void check_int_events(void);
static void do_hard_int(void);
static void rtl8139_dump(message *m);
#if 0
static void dump_phy(re_t *rep);
#endif
static int rl_handler(re_t *rep);
static void rl_watchdog_f(minix_timer_t *tp);
static void tell_dev(vir_bytes start, size_t size, int pci_bus, int
	pci_dev, int pci_func);

/* The message used in the main loop is made global, so that rl_watchdog_f()
 * can change its message type to fake an interrupt message.
 */
static message m;
static int int_event_check;		/* set to TRUE if events arrived */

static u32_t system_hz;

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);
EXTERN int sef_cb_lu_prepare(int state);
EXTERN int sef_cb_lu_state_isvalid(int state);
EXTERN void sef_cb_lu_state_dump(int state);

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char *argv[])
{
	int r;
	int ipc_status;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE)
	{
		if ((r= netdriver_receive(ANY, &m, &ipc_status)) != OK)
			panic("netdriver_receive failed: %d", r);

		if (is_ipc_notify(ipc_status)) {
			switch (_ENDPOINT_P(m.m_source)) {
				case CLOCK:
					/* 
					 * Under MINIX, synchronous alarms are
					 * used instead of watchdog functions.
					 * The approach is very different: MINIX
					 * VMD timeouts are handled within the
					 * kernel (the watchdog is executed by
					 * CLOCK), and notify() the driver in
					 * some cases.  MINIX timeouts result in
					 * a SYN_ALARM message to the driver and
					 * thus are handled where they should be
					 * handled. Locally, watchdog functions
					 * are used again. 
					 */
					rl_watchdog_f(NULL);     
					break;		 
				case HARDWARE:
					do_hard_int();
					if (int_event_check)
						check_int_events();
					break ;
				case TTY_PROC_NR:
					rtl8139_dump(&m);
					break;
				default:
					panic("illegal notify from: %d",
					m.m_source);
			}

			/* done, get nwe message */
			continue;
		}

		switch (m.m_type)
		{
		case DL_WRITEV_S: rl_writev_s(&m, FALSE);	break;
		case DL_READV_S: rl_readv_s(&m, FALSE);		break;
		case DL_CONF:	rl_init(&m);			break;
		case DL_GETSTAT_S: rl_getstat_s(&m);		break;
		default:
			panic("illegal message: %d", m.m_type);
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
  sef_setcb_lu_prepare(sef_cb_lu_prepare);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid);
  sef_setcb_lu_state_dump(sef_cb_lu_state_dump);

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
/* Initialize the rtl8139 driver. */
	long v;
#if RTL8139_FKEY
	int r, fkeys, sfkeys;
#endif

	system_hz = sys_hz();

	v = 0;
	(void) env_parse("instance", "d", 0, &v, 0, 255);
	re_instance = (int) v;

#if RTL8139_FKEY
	/* Observe some function key for debug dumps. */
	fkeys = sfkeys = 0; bit_set(sfkeys, 9);
	if ((r=fkey_map(&fkeys, &sfkeys)) != OK) 
	    printf("Warning: RTL8139 couldn't observe Shift+F9 key: %d\n",r);
#endif

	/* Claim buffer memory now. */
	rl_init_buf(&re_state);

	/* Announce we are up! */
	netdriver_announce();

	return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
	re_t *rep;

	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM) return;

	rep = &re_state;
	if (rep->re_mode == REM_ENABLED)
		rl_outb(rep->re_base_port, RL_CR, 0);

	exit(0);
}

/*===========================================================================*
 *				check_int_events			     *
 *===========================================================================*/
static void check_int_events(void) 
{
	re_t *rep;

	rep= &re_state;

	if (rep->re_mode != REM_ENABLED)
		return;
	if (!rep->re_got_int)
		return;
	rep->re_got_int= 0;
	assert(rep->re_flags & REF_ENABLED);
	rl_check_ints(rep);
}

/*===========================================================================*
 *				rtl8139_dump				     *
 *===========================================================================*/
static void rtl8139_dump(m)
message *m;			/* pointer to request message */
{
	re_t *rep;

	rep= &re_state;

	printf("\n");
	if (rep->re_mode == REM_DISABLED)
		printf("Realtek RTL 8139 instance %d is disabled\n",
			re_instance);

	if (rep->re_mode != REM_ENABLED)
		return;

	printf("Realtek RTL 8139 statistics of instance %d:\n", re_instance);

	printf("recvErr    :%8ld\t", rep->re_stat.ets_recvErr);
	printf("sendErr    :%8ld\t", rep->re_stat.ets_sendErr);
	printf("OVW        :%8ld\n", rep->re_stat.ets_OVW);

	printf("CRCerr     :%8ld\t", rep->re_stat.ets_CRCerr);
	printf("frameAll   :%8ld\t", rep->re_stat.ets_frameAll);
	printf("missedP    :%8ld\n", rep->re_stat.ets_missedP);

	printf("packetR    :%8ld\t", rep->re_stat.ets_packetR);
	printf("packetT    :%8ld\t", rep->re_stat.ets_packetT);
	printf("transDef   :%8ld\n", rep->re_stat.ets_transDef);

	printf("collision  :%8ld\t", rep->re_stat.ets_collision);
	printf("transAb    :%8ld\t", rep->re_stat.ets_transAb);
	printf("carrSense  :%8ld\n", rep->re_stat.ets_carrSense);

	printf("fifoUnder  :%8ld\t", rep->re_stat.ets_fifoUnder);
	printf("fifoOver   :%8ld\t", rep->re_stat.ets_fifoOver);
	printf("CDheartbeat:%8ld\n", rep->re_stat.ets_CDheartbeat);

	printf("OWC        :%8ld\t", rep->re_stat.ets_OWC);

	printf("re_flags = 0x%x\n", rep->re_flags);

	printf("TSAD: 0x%04x, TSD: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		rl_inw(rep->re_base_port, RL_TSAD),
		rl_inl(rep->re_base_port, RL_TSD0+0*4),
		rl_inl(rep->re_base_port, RL_TSD0+1*4),
		rl_inl(rep->re_base_port, RL_TSD0+2*4),
		rl_inl(rep->re_base_port, RL_TSD0+3*4));
	printf("tx_head %d, tx_tail %d, busy: %d %d %d %d\n",
		rep->re_tx_head, rep->re_tx_tail,
		rep->re_tx[0].ret_busy, rep->re_tx[1].ret_busy,
		rep->re_tx[2].ret_busy, rep->re_tx[3].ret_busy);
}

/*===========================================================================*
 *				rl_init					     *
 *===========================================================================*/
static void rl_init(mp)
message *mp;
{
	static int first_time= 1;

	re_t *rep;
	message reply_mess;

	if (first_time)
	{
		first_time= 0;
		rl_pci_conf(); /* Configure PCI devices. */

		/* Use a synchronous alarm instead of a watchdog timer. */
		sys_setalarm(system_hz, 0);
	}

	rep= &re_state;
	if (rep->re_mode == REM_DISABLED)
	{
		/* This is the default, try to (re)locate the device. */
		rl_conf_hw(rep);
		if (rep->re_mode == REM_DISABLED)
		{
			/* Probe failed, or the device is configured off. */
			reply_mess.m_type= DL_CONF_REPLY;
			reply_mess.m_netdrv_net_dl_conf.stat= ENXIO;
			mess_reply(mp, &reply_mess);
			return;
		}
		if (rep->re_mode == REM_ENABLED)
			rl_init_hw(rep);
#if VERBOSE	/* load silently ... can always check status later */
		rl_report_link(rep);
#endif
	}

	assert(rep->re_mode == REM_ENABLED);
	assert(rep->re_flags & REF_ENABLED);

	rep->re_flags &= ~(REF_PROMISC | REF_MULTI | REF_BROAD);

	if (mp->m_net_netdrv_dl_conf.mode & DL_PROMISC_REQ)
		rep->re_flags |= REF_PROMISC;
	if (mp->m_net_netdrv_dl_conf.mode & DL_MULTI_REQ)
		rep->re_flags |= REF_MULTI;
	if (mp->m_net_netdrv_dl_conf.mode & DL_BROAD_REQ)
		rep->re_flags |= REF_BROAD;

	rl_rec_mode(rep);

	reply_mess.m_type = DL_CONF_REPLY;
	reply_mess.m_netdrv_net_dl_conf.stat = OK;
	memcpy(reply_mess.m_netdrv_net_dl_conf.hw_addr,
		rep->re_address.ea_addr,
		sizeof(reply_mess.m_netdrv_net_dl_conf.hw_addr));

	mess_reply(mp, &reply_mess);
}

/*===========================================================================*
 *				rl_pci_conf				     *
 *===========================================================================*/
static void rl_pci_conf()
{
	re_t *rep;

	rep= &re_state;

	strlcpy(rep->re_name, "rtl8139#0", sizeof(rep->re_name));
	rep->re_name[8] += re_instance;
	rep->re_seen= FALSE;

	pci_init();

	if (rl_probe(rep, re_instance))
		rep->re_seen= TRUE;
}

/*===========================================================================*
 *				rl_probe				     *
 *===========================================================================*/
static int rl_probe(rep, skip)
re_t *rep;
int skip;
{
	int r, devind;
	u16_t vid, did;
	u32_t bar;
	u8_t ilr;
#if VERBOSE
	char *dname;
#endif

	r= pci_first_dev(&devind, &vid, &did);
	if (r == 0)
		return 0;

	while (skip--)
	{
		r= pci_next_dev(&devind, &vid, &did);
		if (!r)
			return 0;
	}

#if VERBOSE	/* stay silent at startup, can always get status later */
	dname= pci_dev_name(vid, did);
	if (!dname)
		dname= "unknown device";
	printf("%s: ", rep->re_name);
	printf("%s (%x/%x) at %s\n", dname, vid, did, pci_slot_name(devind));
#endif
	pci_reserve(devind);
	/* printf("cr = 0x%x\n", pci_attr_r16(devind, PCI_CR)); */
	bar= pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;
	if (bar < 0x400) {
		panic("base address is not properly configured");
	}
	rep->re_base_port= bar;

	ilr= pci_attr_r8(devind, PCI_ILR);
	rep->re_irq= ilr;
	if (debug)
	{
		printf("%s: using I/O address 0x%lx, IRQ %d\n",
			rep->re_name, (unsigned long)bar, ilr);
	}

	return TRUE;
}

/*===========================================================================*
 *				rl_conf_hw				     *
 *===========================================================================*/
static void rl_conf_hw(rep)
re_t *rep;
{
	static eth_stat_t empty_stat = {0, 0, 0, 0, 0, 0 	/* ,... */ };

	rep->re_mode= REM_DISABLED;	/* Superfluous */

	if (rep->re_seen)
	{
		/* PCI device is present */
		rep->re_mode= REM_ENABLED;
	}
	if (rep->re_mode != REM_ENABLED)
		return;

	rep->re_flags= REF_EMPTY;
	rep->re_link_up= -1;	/* Unknown */
	rep->re_got_int= 0;
	rep->re_send_int= 0;
	rep->re_report_link= 0;
	rep->re_clear_rx= 0;
	rep->re_need_reset= 0;
	rep->re_tx_alive= 0;
	rep->re_read_s= 0;
	rep->re_tx_head= 0;
	rep->re_tx_tail= 0;
	rep->re_ertxth= RL_TSD_ERTXTH_8;
	rep->re_stat= empty_stat;
}

/*===========================================================================*
 *				rl_init_buf				     *
 *===========================================================================*/
static void rl_init_buf(rep)
re_t *rep;
{
	size_t rx_bufsize, tx_bufsize, tot_bufsize;
	phys_bytes buf;
	char *mallocbuf;
	int i, off;

	/* Allocate receive and transmit buffers */
	tx_bufsize= ETH_MAX_PACK_SIZE_TAGGED;
	if (tx_bufsize % 4)
		tx_bufsize += 4-(tx_bufsize % 4);	/* Align */
	rx_bufsize= RX_BUFSIZE;
	tot_bufsize= N_TX_BUF*tx_bufsize + rx_bufsize;

	if (tot_bufsize % 4096)
		tot_bufsize += 4096-(tot_bufsize % 4096);

#define BUF_ALIGNMENT (64*1024)

	if(!(mallocbuf = alloc_contig(BUF_ALIGNMENT + tot_bufsize, 0, &buf))) {
	    panic("Couldn't allocate kernel buffer");
	}

	/* click-align mallocced buffer. this is what we used to get
	 * from kmalloc() too.
	 */
	if((off = buf % BUF_ALIGNMENT)) {
		mallocbuf += BUF_ALIGNMENT - off;
		buf += BUF_ALIGNMENT - off;
	}

	tell_dev((vir_bytes)mallocbuf, tot_bufsize, 0, 0, 0);

	for (i= 0; i<N_TX_BUF; i++)
	{
		rep->re_tx[i].ret_buf= buf;
		rep->re_tx[i].v_ret_buf= mallocbuf;
		buf += tx_bufsize;
		mallocbuf += tx_bufsize;
	}
	rep->re_rx_buf= buf;
	rep->v_re_rx_buf= mallocbuf;
}

/*===========================================================================*
 *				rl_init_hw				     *
 *===========================================================================*/
static void rl_init_hw(rep)
re_t *rep;
{
	int s, i;

	rep->re_flags = REF_EMPTY;
	rep->re_flags |= REF_ENABLED;

	/* Set the interrupt handler. The policy is to only send HARD_INT 
	 * notifications. Don't reenable interrupts automatically. The id
	 * that is passed back is the interrupt line number.
	 */
	rep->re_hook_id = rep->re_irq;	
	if ((s=sys_irqsetpolicy(rep->re_irq, 0, &rep->re_hook_id)) != OK)
		printf("RTL8139: error, couldn't set IRQ policy: %d\n", s);

	rl_reset_hw(rep);

	if ((s=sys_irqenable(&rep->re_hook_id)) != OK)
		printf("RTL8139: error, couldn't enable interrupts: %d\n", s);

#if VERBOSE	/* stay silent during startup, can always get status later */
	if (rep->re_model) {
		printf("%s: model %s\n", rep->re_name, rep->re_model);
	} else
	{
		printf("%s: unknown model 0x%08x\n",
			rep->re_name,
			rl_inl(rep->re_base_port, RL_TCR) &
			(RL_TCR_HWVER_AM | RL_TCR_HWVER_BM));
	}
#endif

	rl_confaddr(rep);
	if (debug)
	{
		printf("%s: Ethernet address ", rep->re_name);
		for (i= 0; i < 6; i++)
		{
			printf("%x%c", rep->re_address.ea_addr[i],
				i < 5 ? ':' : '\n');
		}
	}
}

/*===========================================================================*
 *				rl_reset_hw				     *
 *===========================================================================*/
static void rl_reset_hw(rep)
re_t *rep;
{
	port_t port;
	u32_t t;
	phys_bytes bus_buf;
	int i;

	port= rep->re_base_port;

#if 0
	/* Reset the PHY */
	rl_outb(port, RL_BMCR, MII_CTRL_RST);
	SPIN_UNTIL(!(rl_inb(port, RL_BMCR) & MII_CTRL_RST), 1000000);
	if (rl_inb(port, RL_BMCR) & MII_CTRL_RST)
		panic("reset PHY failed to complete");
#endif

	/* Reset the device */
#if VERBOSE
	printf("rl_reset_hw: (before reset) port = 0x%x, RL_CR = 0x%x\n",
		port, rl_inb(port, RL_CR));
#endif
	rl_outb(port, RL_CR, RL_CR_RST);
	SPIN_UNTIL(!(rl_inb(port, RL_CR) & RL_CR_RST), 1000000);
#if VERBOSE
	printf("rl_reset_hw: (after reset) port = 0x%x, RL_CR = 0x%x\n",
		port, rl_inb(port, RL_CR));
#endif
	if (rl_inb(port, RL_CR) & RL_CR_RST)
		printf("rtl8139: reset failed to complete");

	t= rl_inl(port, RL_TCR);
	switch(t & (RL_TCR_HWVER_AM | RL_TCR_HWVER_BM))
	{
	case RL_TCR_HWVER_RTL8139: rep->re_model= "RTL8139"; break;
	case RL_TCR_HWVER_RTL8139A: rep->re_model= "RTL8139A"; break;
	case RL_TCR_HWVER_RTL8139AG:
		rep->re_model= "RTL8139A-G / RTL8139C";
		break;
	case RL_TCR_HWVER_RTL8139B:
		rep->re_model= "RTL8139B / RTL8130";
		break;
	case RL_TCR_HWVER_RTL8100: rep->re_model= "RTL8100"; break;
	case RL_TCR_HWVER_RTL8100B:
		rep->re_model= "RTL8100B/RTL8139D";
		break;
	case RL_TCR_HWVER_RTL8139CP: rep->re_model= "RTL8139C+"; break;
	case RL_TCR_HWVER_RTL8101: rep->re_model= "RTL8101"; break;
	default:
		rep->re_model= NULL;
		break;
	}

#if 0
	printf("REVID: 0x%02x\n", rl_inb(port, RL_REVID));
#endif

	/* Intialize Rx */

	/* Should init multicast mask */
#if 0
08-0f	R/W	MAR[0-7]	multicast
#endif
	bus_buf= vm_1phys2bus(rep->re_rx_buf);
	rl_outl(port, RL_RBSTART, bus_buf);

	/* Initialize Tx */ 
	for (i= 0; i<N_TX_BUF; i++)
	{
		rep->re_tx[i].ret_busy= FALSE;
		bus_buf= vm_1phys2bus(rep->re_tx[i].ret_buf);
		rl_outl(port, RL_TSAD0+i*4, bus_buf);
		t= rl_inl(port, RL_TSD0+i*4);
		assert(t & RL_TSD_OWN);
	}

#if 0
	dump_phy(rep);
#endif

	t= rl_inw(port, RL_IMR);
	rl_outw(port, RL_IMR, t | (RL_IMR_SERR | RL_IMR_TIMEOUT |
		RL_IMR_LENCHG));

	t= rl_inw(port, RL_IMR);
	rl_outw(port, RL_IMR, t | (RL_IMR_FOVW | RL_IMR_PUN |
		RL_IMR_RXOVW | RL_IMR_RER | RL_IMR_ROK));

	t= rl_inw(port, RL_IMR);
	rl_outw(port, RL_IMR, t | (RL_IMR_TER | RL_IMR_TOK));

	t= rl_inb(port, RL_CR);
	rl_outb(port, RL_CR, t | RL_CR_RE);

	t= rl_inb(port, RL_CR);
	rl_outb(port, RL_CR, t | RL_CR_TE);

	rl_outl(port, RL_RCR, RX_BUFBITS);

	t= rl_inl(port, RL_TCR);
	rl_outl(port, RL_TCR, t | RL_TCR_IFG_STD);
}

/*===========================================================================*
 *				rl_confaddr				     *
 *===========================================================================*/
static void rl_confaddr(rep)
re_t *rep;
{
	static char eakey[]= RL_ENVVAR "#_EA";
	static char eafmt[]= "x:x:x:x:x:x";

	int i;
	port_t port;
	u32_t w;
	long v;

	/* User defined ethernet address? */
	eakey[sizeof(RL_ENVVAR)-1]= '0' + re_instance;

	port= rep->re_base_port;

	for (i= 0; i < 6; i++)
	{
		if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
			break;
		rep->re_address.ea_addr[i]= v;
	}

	if (i != 0 && i != 6) env_panic(eakey);	/* It's all or nothing */

	/* Should update ethernet address in hardware */
	if (i == 6)
	{
		port= rep->re_base_port;
		rl_outb(port, RL_9346CR, RL_9346CR_EEM_CONFIG);
		w= 0;
		for (i= 0; i<4; i++)
			w |= (rep->re_address.ea_addr[i] << (i*8));
		rl_outl(port, RL_IDR, w);
		w= 0;
		for (i= 4; i<6; i++)
			w |= (rep->re_address.ea_addr[i] << ((i-4)*8));
		rl_outl(port, RL_IDR+4, w);
		rl_outb(port, RL_9346CR, RL_9346CR_EEM_NORMAL);
	}

	/* Get ethernet address */
	for (i= 0; i<6; i++)
		rep->re_address.ea_addr[i]= rl_inb(port, RL_IDR+i);
}

/*===========================================================================*
 *				rl_rec_mode				     *
 *===========================================================================*/
static void rl_rec_mode(rep)
re_t *rep;
{
	port_t port;
	u32_t rcr;

	port= rep->re_base_port;
	rcr= rl_inl(port, RL_RCR);
	rcr &= ~(RL_RCR_AB|RL_RCR_AM|RL_RCR_APM|RL_RCR_AAP);
	if (rep->re_flags & REF_PROMISC)
		rcr |= RL_RCR_AB | RL_RCR_AM | RL_RCR_AAP;
	if (rep->re_flags & REF_BROAD)
		rcr |= RL_RCR_AB;
	if (rep->re_flags & REF_MULTI)
		rcr |= RL_RCR_AM;
	rcr |= RL_RCR_APM;

	rl_outl(port, RL_RCR, rcr);
}

/*===========================================================================*
 *				rl_readv_s				     *
 *===========================================================================*/
static void rl_readv_s(const message *mp, int from_int)
{
	int i, j, n, o, s, s1, count, size;
	port_t port;
	unsigned amount, totlen, packlen;
	u16_t d_start, d_end;
	u32_t l, rxstat = 0x12345678;
	re_t *rep;
	iovec_s_t *iovp;
	int cps;
	int iov_offset = 0;

	rep= &re_state;

	rep->re_client= mp->m_source;
	count = mp->m_net_netdrv_dl_readv_s.count;

	if (rep->re_clear_rx)
		goto suspend;	/* Buffer overflow */

	assert(rep->re_mode == REM_ENABLED);
	assert(rep->re_flags & REF_ENABLED);

	port= rep->re_base_port;

	/* Assume that the RL_CR_BUFE check was been done by rl_checks_ints
	 */
	if (!from_int && (rl_inb(port, RL_CR) & RL_CR_BUFE))
	{
		/* Receive buffer is empty, suspend */
		goto suspend;
	}

	d_start= rl_inw(port, RL_CAPR) + RL_CAPR_DATA_OFF;
	d_end= rl_inw(port, RL_CBR) % RX_BUFSIZE;

#if RX_BUFSIZE <= USHRT_MAX
	if (d_start >= RX_BUFSIZE)
	{
		printf("rl_readv: strange value in RL_CAPR: 0x%x\n",
			rl_inw(port, RL_CAPR));
		d_start %= RX_BUFSIZE;
	}
#endif

	if (d_end > d_start)
		amount= d_end-d_start;
	else
		amount= d_end+RX_BUFSIZE - d_start;

	rxstat = *(u32_t *) (rep->v_re_rx_buf + d_start);

	if (rep->re_clear_rx)
	{
#if 0
		printf("rl_readv: late buffer overflow\n");
#endif
		goto suspend;	/* Buffer overflow */
	}

	/* Should convert from little endian to host byte order */

	if (!(rxstat & RL_RXS_ROK))
	{
		printf("rxstat = 0x%08x\n", rxstat);
		printf("d_start: 0x%x, d_end: 0x%x, rxstat: 0x%x\n",
			d_start, d_end, rxstat);
		panic("received packet not OK");
	}
	totlen= (rxstat >> RL_RXS_LEN_S);
	if (totlen < 8 || totlen > 2*ETH_MAX_PACK_SIZE)
	{
		/* Someting went wrong */
		printf(
		"rl_readv: bad length (%u) in status 0x%08x at offset 0x%x\n",
			totlen, rxstat, d_start);
		printf(
		"d_start: 0x%x, d_end: 0x%x, totlen: %d, rxstat: 0x%x\n",
			d_start, d_end, totlen, rxstat);
		panic(NULL);
	}

#if 0
	printf("d_start: 0x%x, d_end: 0x%x, totlen: %d, rxstat: 0x%x\n",
		d_start, d_end, totlen, rxstat);
#endif

	if (totlen+4 > amount)
	{
		printf("rl_readv: packet not yet ready\n");
		goto suspend;
	}

	/* Should subtract the CRC */
	packlen= totlen - ETH_CRC_SIZE;

	size= 0;
	o= d_start+4;
	for (i= 0; i<count; i += IOVEC_NR,
		iov_offset += IOVEC_NR * sizeof(rep->re_iovec_s[0]))
	{
		n= IOVEC_NR;
		if (i+n > count)
			n= count-i;

		cps = sys_safecopyfrom(mp->m_source,
			mp->m_net_netdrv_dl_readv_s.grant, iov_offset,
			(vir_bytes) rep->re_iovec_s,
			n * sizeof(rep->re_iovec_s[0]));
		if (cps != OK) {
			panic("rl_readv_s: sys_safecopyfrom failed: %d",
				cps);
		}

		for (j= 0, iovp= rep->re_iovec_s; j<n; j++, iovp++)
		{
			s= iovp->iov_size;
			if (size + s > packlen)
			{
				assert(packlen > size);
				s= packlen-size;
			}

#if 0
			if (sys_umap(mp->m_source, D, iovp->iov_addr, s, &dst_phys) != OK)
			  panic("umap_local failed");
#endif

			if (o >= RX_BUFSIZE)
			{
				o -= RX_BUFSIZE;
				assert(o < RX_BUFSIZE);
			}

			if (o+s > RX_BUFSIZE)
			{
				assert(o<RX_BUFSIZE);
				s1= RX_BUFSIZE-o;

				cps = sys_safecopyto(mp->m_source,
					iovp->iov_grant, 0, 
					(vir_bytes) rep->v_re_rx_buf+o, s1);
				if (cps != OK) { 
					panic("rl_readv_s: sys_safecopyto failed: %d",
					cps);
				}
				cps = sys_safecopyto(mp->m_source,
					iovp->iov_grant, s1, 
					(vir_bytes) rep->v_re_rx_buf, s-s1);
				if (cps != OK) {
					panic("rl_readv_s: sys_safecopyto failed: %d", cps);
				}
			}
			else
			{
				cps = sys_safecopyto(mp->m_source,
					iovp->iov_grant, 0,
					(vir_bytes) rep->v_re_rx_buf+o, s);
				if (cps != OK)
					panic("rl_readv_s: sys_safecopyto failed: %d", cps);
			}

			size += s;
			if (size == packlen)
				break;
			o += s;
		}
		if (size == packlen)
			break;
	}
	if (size < packlen)
	{
		assert(0);
	}

	if (rep->re_clear_rx)
	{
		/* For some reason the receiver FIFO is not stopped when
		 * the buffer is full.
		 */
#if 0
		printf("rl_readv: later buffer overflow\n");
#endif
		goto suspend;	/* Buffer overflow */
	}

	rep->re_stat.ets_packetR++;
	rep->re_read_s= packlen;
	rep->re_flags= (rep->re_flags & ~REF_READING) | REF_PACK_RECV;

	/* Avoid overflow in 16-bit computations */
	l= d_start;
	l += totlen+4;
	l= (l+3) & ~3;	/* align */
	if (l >= RX_BUFSIZE)
	{
		l -= RX_BUFSIZE;
		assert(l < RX_BUFSIZE);
	}
	rl_outw(port, RL_CAPR, l-RL_CAPR_DATA_OFF);

	if (!from_int)
		reply(rep);

	return;

suspend:
	if (from_int)
	{
		assert(rep->re_flags & REF_READING);

		/* No need to store any state */
		return;
	}

	rep->re_rx_mess= *mp;
	assert(!(rep->re_flags & REF_READING));
	rep->re_flags |= REF_READING;

	reply(rep);
}

/*===========================================================================*
 *				rl_writev_s				     *
 *===========================================================================*/
static void rl_writev_s(const message *mp, int from_int)
{
	int i, j, n, s, count, size;
	int tx_head;
	re_t *rep;
	iovec_s_t *iovp;
	char *ret;
	int cps;
	int iov_offset = 0;

	rep= &re_state;

	rep->re_client= mp->m_source;
	count = mp->m_net_netdrv_dl_writev_s.count;

	assert(rep->re_mode == REM_ENABLED);
	assert(rep->re_flags & REF_ENABLED);

	if (from_int)
	{
		assert(rep->re_flags & REF_SEND_AVAIL);
		rep->re_flags &= ~REF_SEND_AVAIL;
		rep->re_send_int= FALSE;
		rep->re_tx_alive= TRUE;
	}

	tx_head= rep->re_tx_head;
	if (rep->re_tx[tx_head].ret_busy)
	{
		assert(!(rep->re_flags & REF_SEND_AVAIL));
		rep->re_flags |= REF_SEND_AVAIL;
		goto suspend;
	}

	assert(!(rep->re_flags & REF_SEND_AVAIL));
	assert(!(rep->re_flags & REF_PACK_SENT));

	size= 0;
	ret = rep->re_tx[tx_head].v_ret_buf;
	for (i= 0; i<count; i += IOVEC_NR,
		iov_offset += IOVEC_NR * sizeof(rep->re_iovec_s[0]))
	{
		n= IOVEC_NR;
		if (i+n > count)
			n= count-i;
		cps = sys_safecopyfrom(mp->m_source,
			mp->m_net_netdrv_dl_writev_s.grant, iov_offset,
			(vir_bytes) rep->re_iovec_s,
			n * sizeof(rep->re_iovec_s[0]));
		if (cps != OK) {
			panic("rl_writev_s: sys_safecopyfrom failed: %d", cps);
		}

		for (j= 0, iovp= rep->re_iovec_s; j<n; j++, iovp++)
		{
			s= iovp->iov_size;
			if (size + s > ETH_MAX_PACK_SIZE_TAGGED) {
				panic("invalid packet size");
			}
			cps = sys_safecopyfrom(mp->m_source, iovp->iov_grant,
				0, (vir_bytes) ret, s);
			if (cps != OK) { 
				panic("rl_writev_s: sys_safecopyfrom failed: %d",	cps);
			}
			size += s;
			ret += s;
		}
	}
	if (size < ETH_MIN_PACK_SIZE)
		panic("invalid packet size: %d", size);

	rl_outl(rep->re_base_port, RL_TSD0+tx_head*4, 
		rep->re_ertxth | size);
	rep->re_tx[tx_head].ret_busy= TRUE;

	if (++tx_head == N_TX_BUF)
		tx_head= 0;
	assert(tx_head < RL_N_TX);
	rep->re_tx_head= tx_head;

	rep->re_flags |= REF_PACK_SENT;

	/* If the interrupt handler called, don't send a reply. The reply
	 * will be sent after all interrupts are handled. 
	 */
	if (from_int)
		return;
	reply(rep);
	return;

suspend:
#if 0
		printf("rl_writev: head %d, tail %d, busy: %d %d %d %d\n",
			tx_head, rep->re_tx_tail,
			rep->re_tx[0].ret_busy, rep->re_tx[1].ret_busy,
			rep->re_tx[2].ret_busy, rep->re_tx[3].ret_busy);
		printf("rl_writev: TSD: 0x%x, 0x%x, 0x%x, 0x%x\n",
			rl_inl(rep->re_base_port, RL_TSD0+0*4),
			rl_inl(rep->re_base_port, RL_TSD0+1*4),
			rl_inl(rep->re_base_port, RL_TSD0+2*4),
			rl_inl(rep->re_base_port, RL_TSD0+3*4));
#endif

	if (from_int)
		panic("should not be sending");

	rep->re_tx_mess= *mp;
	reply(rep);
}

/*===========================================================================*
 *				rl_check_ints				     *
 *===========================================================================*/
static void rl_check_ints(rep)
re_t *rep;
{
#if 0
10-1f	R/W	TSD[0-3]	Transmit Status of Descriptor [0-3]
	31	R	CRS	Carrier Sense Lost
	30	R	TABT	Transmit Abort
	29	R	OWC	Out of Window Collision
	27-24	R	NCC[3-0] Number of Collision Count
	23-22			reserved
	21-16	R/W	ERTXH[5-0] Early Tx Threshold
	15	R	TOK	Transmit OK
	14	R	TUN	Transmit FIFO Underrun
	13	R/W	OWN	OWN
	12-0	R/W	SIZE	Descriptor Size
3e-3f	R/W	ISR		Interrupt Status Register
	6	R/W	FOVW	Fx FIFO Overflow Interrupt
	5	R/W	PUN/LinkChg Packet Underrun / Link Change Interrupt
	3	R/W	TER	Transmit Error Interrupt
	2	R/W	TOK	Transmit OK Interrupt
3e-3f	R/W	ISR		Interrupt Status Register
	15	R/W	SERR	System Error Interrupt
	14	R/W	TimeOut	Time Out Interrupt
	13	R/W	LenChg	Cable Length Change Interrupt
3e-3f	R/W	ISR		Interrupt Status Register
	4	R/W	RXOVW	Rx Buffer Overflow Interrupt
	1	R/W	RER	Receive Error Interrupt
	0	R/W	ROK	Receive OK Interrupt
4c-4f	R/W	MPC		Missed Packet Counter
60-61	R	TSAD		Transmit Status of All Descriptors
	15-12	R	TOK[3-0] TOK bit of Descriptor [3-0]
	11-8	R	TUN[3-0] TUN bit of Descriptor [3-0]
	7-4	R	TABT[3-0] TABT bit of Descriptor [3-0]
	3-0     R       OWN[3-0] OWN bit of Descriptor [3-0]
6c-6d	R	DIS		Disconnect Counter
	15-0	R	DCNT	Disconnect Counter
6e-6f	R	FCSC		False Carrier Sense Counter
	15-0	R	FCSCNT	False Carrier event counter
72-73	R	REC		RX_ER Counter
	15-0	R	RXERCNT	Received packet counter
#endif

	int re_flags;

	re_flags= rep->re_flags;

	if ((re_flags & REF_READING) &&
		!(rl_inb(rep->re_base_port, RL_CR) & RL_CR_BUFE))
	{
		rl_readv_s(&rep->re_rx_mess, TRUE /* from int */);
	}
	if (rep->re_clear_rx)
		rl_clear_rx(rep);

	if (rep->re_need_reset)
		rl_do_reset(rep);

	if (rep->re_send_int)
	{
		rl_writev_s(&rep->re_tx_mess, TRUE /* from int */);
	}

	if (rep->re_report_link)
		rl_report_link(rep);

	if (rep->re_flags & (REF_PACK_SENT | REF_PACK_RECV))
		reply(rep);
}

/*===========================================================================*
 *				rl_report_link				     *
 *===========================================================================*/
static void rl_report_link(rep)
re_t *rep;
{
	port_t port;
	u16_t mii_ctrl, mii_status, mii_ana, mii_anlpa, mii_ane, mii_extstat;
	u8_t msr;
	int f, link_up;

	rep->re_report_link= FALSE;
	port= rep->re_base_port;
	msr= rl_inb(port, RL_MSR);
	link_up= !(msr & RL_MSR_LINKB);
	rep->re_link_up= link_up;
	if (!link_up)
	{
		printf("%s: link down\n", rep->re_name);
		return;
	}

	mii_ctrl= rl_inw(port, RL_BMCR);
	mii_status= rl_inw(port, RL_BMSR);
	mii_ana= rl_inw(port, RL_ANAR);
	mii_anlpa= rl_inw(port, RL_ANLPAR);
	mii_ane= rl_inw(port, RL_ANER);
	mii_extstat= 0;

	if (mii_ctrl & (MII_CTRL_LB|MII_CTRL_PD|MII_CTRL_ISO))
	{
		printf("%s: PHY: ", rep->re_name);
		f= 1;
		if (mii_ctrl & MII_CTRL_LB)
		{
			printf("loopback mode");
			f= 0;
		}
		if (mii_ctrl & MII_CTRL_PD)
		{
			if (!f) printf(", ");
			f= 0;
			printf("powered down");
		}
		if (mii_ctrl & MII_CTRL_ISO)
		{
			if (!f) printf(", ");
			f= 0;
			printf("isolated");
		}
		printf("\n");
		return;
	}
	if (!(mii_ctrl & MII_CTRL_ANE))
	{
		printf("%s: manual config: ", rep->re_name);
		switch(mii_ctrl & (MII_CTRL_SP_LSB|MII_CTRL_SP_MSB))
		{
		case MII_CTRL_SP_10:	printf("10 Mbps"); break;
		case MII_CTRL_SP_100:	printf("100 Mbps"); break;
		case MII_CTRL_SP_1000:	printf("1000 Mbps"); break;
		case MII_CTRL_SP_RES:	printf("reserved speed"); break;
		}
		if (mii_ctrl & MII_CTRL_DM)
			printf(", full duplex");
		else
			printf(", half duplex");
		printf("\n");
		return;
	}

	if (!debug) goto resspeed;

	printf("%s: ", rep->re_name);
	mii_print_stat_speed(mii_status, mii_extstat);
	printf("\n");

	if (!(mii_status & MII_STATUS_ANC))
		printf("%s: auto-negotiation not complete\n", rep->re_name);
	if (mii_status & MII_STATUS_RF)
		printf("%s: remote fault detected\n", rep->re_name);
	if (!(mii_status & MII_STATUS_ANA))
	{
		printf("%s: local PHY has no auto-negotiation ability\n",
			rep->re_name);
	}
	if (!(mii_status & MII_STATUS_LS))
		printf("%s: link down\n", rep->re_name);
	if (mii_status & MII_STATUS_JD)
		printf("%s: jabber condition detected\n", rep->re_name);
	if (!(mii_status & MII_STATUS_EC))
	{
		printf("%s: no extended register set\n", rep->re_name);
		goto resspeed;
	}
	if (!(mii_status & MII_STATUS_ANC))
		goto resspeed;

	printf("%s: local cap.: ", rep->re_name);
	mii_print_techab(mii_ana);
	printf("\n");

	if (mii_ane & MII_ANE_PDF)
		printf("%s: parallel detection fault\n", rep->re_name);
	if (!(mii_ane & MII_ANE_LPANA))
	{
		printf("%s: link-partner does not support auto-negotiation\n",
			rep->re_name);
		goto resspeed;
	}

	printf("%s: remote cap.: ", rep->re_name);
	mii_print_techab(mii_anlpa);
	printf("\n");

resspeed:
	printf("%s: ", rep->re_name);
	printf("link up at %d Mbps, ", (msr & RL_MSR_SPEED_10) ? 10 : 100);
	printf("%s duplex\n", ((mii_ctrl & MII_CTRL_DM) ? "full" : "half"));

}

static void mii_print_techab(u16_t techab)
{
	int fs, ft;
	if ((techab & MII_ANA_SEL_M) != MII_ANA_SEL_802_3)
	{
		printf("strange selector 0x%x, value 0x%x",
			techab & MII_ANA_SEL_M,
			(techab & MII_ANA_TAF_M) >> MII_ANA_TAF_S);
		return;
	}
	fs= 1;
	if (techab & (MII_ANA_100T4 | MII_ANA_100TXFD | MII_ANA_100TXHD))
	{
		printf("100 Mbps: ");
		fs= 0;
		ft= 1;
		if (techab & MII_ANA_100T4)
		{
			printf("T4");
			ft= 0;
		}
		if (techab & (MII_ANA_100TXFD | MII_ANA_100TXHD))
		{
			if (!ft)
				printf(", ");
			ft= 0;
			printf("TX-");
			switch(techab & (MII_ANA_100TXFD|MII_ANA_100TXHD))
			{
			case MII_ANA_100TXFD:	printf("FD"); break;
			case MII_ANA_100TXHD:	printf("HD"); break;
			default:		printf("FD/HD"); break;
			}
		}
	}
	if (techab & (MII_ANA_10TFD | MII_ANA_10THD))
	{
		if (!fs)
			printf(", ");
		printf("10 Mbps: ");
		fs= 0;
		printf("T-");
		switch(techab & (MII_ANA_10TFD|MII_ANA_10THD))
		{
		case MII_ANA_10TFD:	printf("FD"); break;
		case MII_ANA_10THD:	printf("HD"); break;
		default:		printf("FD/HD"); break;
		}
	}
	if (techab & MII_ANA_PAUSE_SYM)
	{
		if (!fs)
			printf(", ");
		fs= 0;
		printf("pause(SYM)");
	}
	if (techab & MII_ANA_PAUSE_ASYM)
	{
		if (!fs)
			printf(", ");
		fs= 0;
		printf("pause(ASYM)");
	}
	if (techab & MII_ANA_TAF_RES)
	{
		if (!fs)
			printf(", ");
		fs= 0;
		printf("0x%x", (techab & MII_ANA_TAF_RES) >> MII_ANA_TAF_S);
	}
}

static void mii_print_stat_speed(u16_t stat, u16_t extstat)
{
	int fs, ft;
	fs= 1;
	if (stat & MII_STATUS_EXT_STAT)
	{
		if (extstat & (MII_ESTAT_1000XFD | MII_ESTAT_1000XHD |
			MII_ESTAT_1000TFD | MII_ESTAT_1000THD))
		{
			printf("1000 Mbps: ");
			fs= 0;
			ft= 1;
			if (extstat & (MII_ESTAT_1000XFD | MII_ESTAT_1000XHD))
			{
				ft= 0;
				printf("X-");
				switch(extstat &
					(MII_ESTAT_1000XFD|MII_ESTAT_1000XHD))
				{
				case MII_ESTAT_1000XFD:	printf("FD"); break;
				case MII_ESTAT_1000XHD:	printf("HD"); break;
				default:		printf("FD/HD"); break;
				}
			}
			if (extstat & (MII_ESTAT_1000TFD | MII_ESTAT_1000THD))
			{
				if (!ft)
					printf(", ");
				ft= 0;
				printf("T-");
				switch(extstat &
					(MII_ESTAT_1000TFD|MII_ESTAT_1000THD))
				{
				case MII_ESTAT_1000TFD:	printf("FD"); break;
				case MII_ESTAT_1000THD:	printf("HD"); break;
				default:		printf("FD/HD"); break;
				}
			}
		}
	}
	if (stat & (MII_STATUS_100T4 |
		MII_STATUS_100XFD | MII_STATUS_100XHD |
		MII_STATUS_100T2FD | MII_STATUS_100T2HD))
	{
		if (!fs)
			printf(", ");
		fs= 0;
		printf("100 Mbps: ");
		ft= 1;
		if (stat & MII_STATUS_100T4)
		{
			printf("T4");
			ft= 0;
		}
		if (stat & (MII_STATUS_100XFD | MII_STATUS_100XHD))
		{
			if (!ft)
				printf(", ");
			ft= 0;
			printf("TX-");
			switch(stat & (MII_STATUS_100XFD|MII_STATUS_100XHD))
			{
			case MII_STATUS_100XFD:	printf("FD"); break;
			case MII_STATUS_100XHD:	printf("HD"); break;
			default:		printf("FD/HD"); break;
			}
		}
		if (stat & (MII_STATUS_100T2FD | MII_STATUS_100T2HD))
		{
			if (!ft)
				printf(", ");
			ft= 0;
			printf("T2-");
			switch(stat & (MII_STATUS_100T2FD|MII_STATUS_100T2HD))
			{
			case MII_STATUS_100T2FD:	printf("FD"); break;
			case MII_STATUS_100T2HD:	printf("HD"); break;
			default:		printf("FD/HD"); break;
			}
		}
	}
	if (stat & (MII_STATUS_10FD | MII_STATUS_10HD))
	{
		if (!fs)
			printf(", ");
		printf("10 Mbps: ");
		fs= 0;
		printf("T-");
		switch(stat & (MII_STATUS_10FD|MII_STATUS_10HD))
		{
		case MII_STATUS_10FD:	printf("FD"); break;
		case MII_STATUS_10HD:	printf("HD"); break;
		default:		printf("FD/HD"); break;
		}
	}
}

/*===========================================================================*
 *				rl_clear_rx				     *
 *===========================================================================*/
static void rl_clear_rx(re_t *rep)
{
	port_t port;
	u8_t cr;

	rep->re_clear_rx= FALSE;
	port= rep->re_base_port;

	/* Reset the receiver */
	cr= rl_inb(port, RL_CR);
	cr &= ~RL_CR_RE;
	rl_outb(port, RL_CR, cr);
	SPIN_UNTIL(!(rl_inb(port, RL_CR) & RL_CR_RE), 1000000);
	if (rl_inb(port, RL_CR) & RL_CR_RE)
		panic("cannot disable receiver");

#if 0
	printf("RBSTART = 0x%08x\n", rl_inl(port, RL_RBSTART));
	printf("CAPR = 0x%04x\n", rl_inw(port, RL_CAPR));
	printf("CBR = 0x%04x\n", rl_inw(port, RL_CBR));
	printf("RCR = 0x%08x\n", rl_inl(port, RL_RCR));
#endif

	rl_outb(port, RL_CR, cr | RL_CR_RE);

	rl_outl(port, RL_RCR, RX_BUFBITS);

	rl_rec_mode(rep);

	rep->re_stat.ets_missedP++;
}

/*===========================================================================*
 *				rl_do_reset				     *
 *===========================================================================*/
static void rl_do_reset(rep)
re_t *rep;
{
	rep->re_need_reset= FALSE;
	rl_reset_hw(rep);
	rl_rec_mode(rep);

	rep->re_tx_head= 0;
	if (rep->re_flags & REF_SEND_AVAIL)
	{
		rep->re_tx[rep->re_tx_head].ret_busy= FALSE;
		rep->re_send_int= TRUE;
	}
}

/*===========================================================================*
 *				rl_getstat_s				     *
 *===========================================================================*/
static void rl_getstat_s(mp)
message *mp;
{
	int r;
	eth_stat_t stats;
	re_t *rep;

	rep= &re_state;

	assert(rep->re_mode == REM_ENABLED);
	assert(rep->re_flags & REF_ENABLED);

	stats= rep->re_stat;

	r = sys_safecopyto(mp->m_source, mp->m_net_netdrv_dl_getstat_s.grant,
		0, (vir_bytes) &stats, sizeof(stats));
	if (r != OK)
		panic("rl_getstat_s: sys_safecopyto failed: %d", r);

	mp->m_type= DL_STAT_REPLY;
	r= ipc_send(mp->m_source, mp);
	if (r != OK)
		panic("rl_getstat_s: ipc_send failed: %d", r);
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
static void reply(rep)
re_t *rep;
{
	message reply;
	int flags;
	int r;

	flags = DL_NOFLAGS;
	if (rep->re_flags & REF_PACK_SENT)
		flags |= DL_PACK_SEND;
	if (rep->re_flags & REF_PACK_RECV)
		flags |= DL_PACK_RECV;

	reply.m_type = DL_TASK_REPLY;
	reply.m_netdrv_net_dl_task.flags = flags;
	reply.m_netdrv_net_dl_task.count = rep->re_read_s;

	r= ipc_send(rep->re_client, &reply);

	if (r < 0) {
		printf("RTL8139 tried sending to %d, type %d\n",
			rep->re_client, reply.m_type);
		panic("ipc_send failed: %d", r);
	}
	
	rep->re_read_s = 0;
	rep->re_flags &= ~(REF_PACK_SENT | REF_PACK_RECV);
}

/*===========================================================================*
 *				mess_reply				     *
 *===========================================================================*/
static void mess_reply(req, reply_mess)
message *req;
message *reply_mess;
{
	if (ipc_send(req->m_source, reply_mess) != OK)
		panic("unable to mess_reply");
}

#if 0
/*===========================================================================*
 *				dump_phy				     *
 *===========================================================================*/
static void dump_phy(rep)
re_t *rep;
{
	port_t port;
	u32_t t;

	port= rep->re_base_port;

	t= rl_inb(port, RL_MSR);
	printf("MSR: 0x%02lx\n", t);
	if (t & RL_MSR_SPEED_10)
		printf("\t10 Mbps\n");
	if (t & RL_MSR_LINKB)
		printf("\tLink failed\n");

	t= rl_inb(port, RL_CONFIG1);
	printf("CONFIG1: 0x%02lx\n", t);

	t= rl_inb(port, RL_CONFIG3);
	printf("CONFIG3: 0x%02lx\n", t);

	t= rl_inb(port, RL_CONFIG4);
	printf("CONFIG4: 0x%02lx\n", t);

	t= rl_inw(port, RL_BMCR);
	printf("BMCR (MII_CTRL): 0x%04lx\n", t);

	t= rl_inw(port, RL_BMSR);
	printf("BMSR:");
	if (t & MII_STATUS_100T4)
		printf(" 100Base-T4");
	if (t & MII_STATUS_100XFD)
		printf(" 100Base-X-FD");
	if (t & MII_STATUS_100XHD)
		printf(" 100Base-X-HD");
	if (t & MII_STATUS_10FD)
		printf(" 10Mbps-FD");
	if (t & MII_STATUS_10HD)
		printf(" 10Mbps-HD");
	if (t & MII_STATUS_100T2FD)
		printf(" 100Base-T2-FD");
	if (t & MII_STATUS_100T2HD)
		printf(" 100Base-T2-HD");
	if (t & MII_STATUS_EXT_STAT)
		printf(" Ext-stat");
	if (t & MII_STATUS_RES)
		printf(" res-0x%lx", t & MII_STATUS_RES);
	if (t & MII_STATUS_MFPS)
		printf(" MFPS");
	if (t & MII_STATUS_ANC)
		printf(" ANC");
	if (t & MII_STATUS_RF)
		printf(" remote-fault");
	if (t & MII_STATUS_ANA)
		printf(" ANA");
	if (t & MII_STATUS_LS)
		printf(" Link");
	if (t & MII_STATUS_JD)
		printf(" Jabber");
	if (t & MII_STATUS_EC)
		printf(" Extended-capability");
	printf("\n");

	t= rl_inw(port, RL_ANAR);
	printf("ANAR (MII_ANA): 0x%04lx\n", t);

	t= rl_inw(port, RL_ANLPAR);
	printf("ANLPAR: 0x%04lx\n", t);

	t= rl_inw(port, RL_ANER);
	printf("ANER (MII_ANE): ");
	if (t & MII_ANE_RES)
		printf(" res-0x%lx", t & MII_ANE_RES);
	if (t & MII_ANE_PDF)
		printf(" Par-Detect-Fault");
	if (t & MII_ANE_LPNPA)
		printf(" LP-Next-Page-Able");
	if (t & MII_ANE_NPA)
		printf(" Loc-Next-Page-Able");
	if (t & MII_ANE_PR)
		printf(" Page-Received");
	if (t & MII_ANE_LPANA)
		printf(" LP-Auto-Neg-Able");
	printf("\n");

	t= rl_inw(port, RL_NWAYTR);
	printf("NWAYTR: 0x%04lx\n", t);
	t= rl_inw(port, RL_CSCR);
	printf("CSCR: 0x%04lx\n", t);

	t= rl_inb(port, RL_CONFIG5);
	printf("CONFIG5: 0x%02lx\n", t);
}
#endif

/*===========================================================================*
 *				do_hard_int				     *
 *===========================================================================*/
static void do_hard_int(void)
{
	int s;

	/* Run interrupt handler at driver level. */
	rl_handler(&re_state);

	/* Reenable interrupts for this hook. */
	if ((s=sys_irqenable(&re_state.re_hook_id)) != OK)
		printf("RTL8139: error, couldn't enable interrupts: %d\n", s);
}

/*===========================================================================*
 *				rl_handler				     *
 *===========================================================================*/
static int rl_handler(re_t *rep)
{
	int i, port, tx_head, tx_tail, link_up;
	u16_t isr, tsad;
	u32_t tsd, tcr, ertxth;
#if 0
	u8_t cr;
#endif
	int_event_check = FALSE;	/* disable check by default */

	port= rep->re_base_port;

	/* Ack interrupt */
	isr= rl_inw(port, RL_ISR);
	rl_outw(port, RL_ISR, isr);

	if (isr & RL_IMR_FOVW)
	{
		isr &= ~RL_IMR_FOVW;
		/* Should do anything? */

		rep->re_stat.ets_fifoOver++;
	}
	if (isr & RL_IMR_PUN)
	{
		isr &= ~RL_IMR_PUN;

		/* Either the link status changed or there was a TX fifo
		 * underrun.
		 */
		link_up= !(rl_inb(port, RL_MSR) & RL_MSR_LINKB);
		if (link_up != rep->re_link_up)
		{
			rep->re_report_link= TRUE;
			rep->re_got_int= TRUE;
			int_event_check = TRUE;
		}
	}
	if (isr & RL_IMR_RXOVW)
	{
		isr &= ~RL_IMR_RXOVW;

		/* Clear the receive buffer */
		rep->re_clear_rx= TRUE;
		rep->re_got_int= TRUE;
		int_event_check = TRUE;
	}

	if (isr & (RL_ISR_RER | RL_ISR_ROK))
	{
		isr &= ~(RL_ISR_RER | RL_ISR_ROK);

		if (!rep->re_got_int && (rep->re_flags & REF_READING))
		{
			rep->re_got_int= TRUE;
			int_event_check = TRUE;
		}
	}
#if 0
	if ((isr & (RL_ISR_TER | RL_ISR_TOK)) &&
		(rep->re_flags & REF_SEND_AVAIL) &&
		(rep->re_tx[0].ret_busy || rep->re_tx[1].ret_busy ||
		rep->re_tx[2].ret_busy || rep->re_tx[3].ret_busy))
		
	{
		printf(
	"rl_handler, SEND_AVAIL: tx_head %d, tx_tail %d, busy: %d %d %d %d\n",
			rep->re_tx_head, rep->re_tx_tail,
			rep->re_tx[0].ret_busy, rep->re_tx[1].ret_busy,
			rep->re_tx[2].ret_busy, rep->re_tx[3].ret_busy);
		printf(
	"rl_handler: TSAD: 0x%04x, TSD: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			rl_inw(port, RL_TSAD),
			rl_inl(port, RL_TSD0+0*4),
			rl_inl(port, RL_TSD0+1*4),
			rl_inl(port, RL_TSD0+2*4),
			rl_inl(port, RL_TSD0+3*4));
	}
#endif
	if ((isr & (RL_ISR_TER | RL_ISR_TOK)) || 1)
	{
		isr &= ~(RL_ISR_TER | RL_ISR_TOK);

		tsad= rl_inw(port, RL_TSAD);
		if (tsad & (RL_TSAD_TABT0|RL_TSAD_TABT1|
			RL_TSAD_TABT2|RL_TSAD_TABT3))
		{
#if 0
			/* Do we need a watch dog? */
			/* Just reset the whole chip */
			rep->re_need_reset= TRUE;
			rep->re_got_int= TRUE;
			int_event_check = TRUE;
#elif 0
			/* Reset transmitter */
			rep->re_stat.ets_transAb++;

			cr= rl_inb(port, RL_CR);
			cr &= ~RL_CR_TE;
			rl_outb(port, RL_CR, cr);
			SPIN_UNTIL(!(rl_inb(port, RL_CR) & RL_CR_TE), 1000000);
			if (rl_inb(port, RL_CR) & RL_CR_TE) {
				panic("cannot disable transmitter");
			}
			rl_outb(port, RL_CR, cr | RL_CR_TE);

			tcr= rl_inl(port, RL_TCR);
			rl_outl(port, RL_TCR, tcr | RL_TCR_IFG_STD);

			printf("rl_handler: reset after abort\n");

			if (rep->re_flags & REF_SEND_AVAIL)
			{
				printf("rl_handler: REF_SEND_AVAIL\n");
				rep->re_send_int= TRUE;
				rep->re_got_int= TRUE;
				int_event_check = TRUE;
			}
			for (i= 0; i< N_TX_BUF; i++)
				rep->re_tx[i].ret_busy= FALSE;
			rep->re_tx_head= 0;
#else
			printf("rl_handler, TABT, tasd = 0x%04x\n",
				tsad);

			/* Find the aborted transmit request */
			for (i= 0; i< N_TX_BUF; i++)
			{
				tsd= rl_inl(port, RL_TSD0+i*4);
				if (tsd & RL_TSD_TABT)
					break;
			}
			if (i >= N_TX_BUF)
			{
				printf(
				"rl_handler: can't find aborted TX req.\n");
			}
			else
			{
				printf("TSD%d = 0x%04x\n", i, tsd);

				/* Set head and tail to this buffer */
				rep->re_tx_head= rep->re_tx_tail= i;
			}

			/* Aborted transmission, just kick the device
			 * and be done with it.
			 */
			rep->re_stat.ets_transAb++;
			tcr= rl_inl(port, RL_TCR);
			rl_outl(port, RL_TCR, tcr | RL_TCR_CLRABT);
#endif
		}

		/* Transmit completed */
		tx_head= rep->re_tx_head;
		tx_tail= rep->re_tx_tail;
		for (i= 0; i< 2*N_TX_BUF; i++)
		{
			if (!rep->re_tx[tx_tail].ret_busy)
			{
				/* Strange, this buffer is not in-use.
				 * Increment tx_tail until tx_head is
				 * reached (or until we find a buffer that
				 * is in-use.
				 */
				if (tx_tail == tx_head)
					break;
				if (++tx_tail >= N_TX_BUF)
					tx_tail= 0;
				assert(tx_tail < RL_N_TX);
				rep->re_tx_tail= tx_tail;
				continue;
			}
			tsd= rl_inl(port, RL_TSD0+tx_tail*4);
			if (!(tsd & RL_TSD_OWN))
			{
				/* Buffer is not yet ready */
				break;
			}

			/* Should collect statistics */
			if (tsd & RL_TSD_CRS)
				rep->re_stat.ets_carrSense++;
			if (tsd & RL_TSD_TABT)
			{
				printf("rl_handler, TABT, TSD%d = 0x%04x\n",
					tx_tail, tsd);
				assert(0);	/* CLRABT is not all that
						 * effective, why not?
						 */
				rep->re_stat.ets_transAb++;
				tcr= rl_inl(port, RL_TCR);
				rl_outl(port, RL_TCR, tcr | RL_TCR_CLRABT);
			}
			if (tsd & RL_TSD_OWC)
				rep->re_stat.ets_OWC++;
			if (tsd & RL_TSD_CDH)
				rep->re_stat.ets_CDheartbeat++;

			/* What about collisions? */
			if (tsd & RL_TSD_TOK)
				rep->re_stat.ets_packetT++;
			else
				rep->re_stat.ets_sendErr++;
			if (tsd & RL_TSD_TUN)
			{
				rep->re_stat.ets_fifoUnder++;

				/* Increase ERTXTH */
				ertxth= tsd + (1 << RL_TSD_ERTXTH_S);
				ertxth &= RL_TSD_ERTXTH_M;
				if (debug && ertxth > rep->re_ertxth)
				{
					printf("%s: new ertxth: %d bytes\n",
						rep->re_name,
						(ertxth >> RL_TSD_ERTXTH_S) *
						32);
					rep->re_ertxth= ertxth;
				}
			}
			rep->re_tx[tx_tail].ret_busy= FALSE;

#if 0
			if (rep->re_flags & REF_SEND_AVAIL)
			{
			printf("TSD%d: %08lx\n", tx_tail, tsd);
			printf(
			"rl_handler: head %d, tail %d, busy: %d %d %d %d\n", 
				tx_head, tx_tail,
				rep->re_tx[0].ret_busy, rep->re_tx[1].ret_busy, 
				rep->re_tx[2].ret_busy, rep->re_tx[3].ret_busy);
			}
#endif

			if (++tx_tail >= N_TX_BUF)
				tx_tail= 0;
			assert(tx_tail < RL_N_TX);
			rep->re_tx_tail= tx_tail;

			if (rep->re_flags & REF_SEND_AVAIL)
			{
#if 0
				printf("rl_handler: REF_SEND_AVAIL\n");
#endif
				rep->re_send_int= TRUE;
				if (!rep->re_got_int)
				{
					rep->re_got_int= TRUE;
					int_event_check = TRUE;
				}
			}
		}
		assert(i < 2*N_TX_BUF);
	}
	if (isr)
	{
		printf("rl_handler: unhandled interrupt: isr = 0x%04x\n",
			isr);
	}

	return 1;
}

/*===========================================================================*
 *				rl_watchdog_f				     *
 *===========================================================================*/
static void rl_watchdog_f(tp)
minix_timer_t *tp;
{
	re_t *rep;
	/* Use a synchronous alarm instead of a watchdog timer. */
	sys_setalarm(system_hz, 0);

	rep= &re_state;

	if (rep->re_mode != REM_ENABLED)
		return;
	if (!(rep->re_flags & REF_SEND_AVAIL))
	{
		/* Assume that an idle system is alive */
		rep->re_tx_alive= TRUE;
		return;
	}
	if (rep->re_tx_alive)
	{
		rep->re_tx_alive= FALSE;
		return;
	}
	printf("rl_watchdog_f: resetting instance %d\n", re_instance);
	printf("TSAD: 0x%04x, TSD: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		rl_inw(rep->re_base_port, RL_TSAD),
		rl_inl(rep->re_base_port, RL_TSD0+0*4),
		rl_inl(rep->re_base_port, RL_TSD0+1*4),
		rl_inl(rep->re_base_port, RL_TSD0+2*4),
		rl_inl(rep->re_base_port, RL_TSD0+3*4));
	printf("tx_head %d, tx_tail %d, busy: %d %d %d %d\n",
		rep->re_tx_head, rep->re_tx_tail,
		rep->re_tx[0].ret_busy, rep->re_tx[1].ret_busy,
		rep->re_tx[2].ret_busy, rep->re_tx[3].ret_busy);
	rep->re_need_reset= TRUE;
	rep->re_got_int= TRUE;
			
	check_int_events();
}

#if 0

static void rtl_init(struct dpeth *dep);
static u16_t get_ee_word(dpeth_t *dep, int a);
static void ee_wen(dpeth_t *dep);
static void set_ee_word(dpeth_t *dep, int a, u16_t w);
static void ee_wds(dpeth_t *dep);

static void rtl_init(dep)
dpeth_t *dep;
{
	u8_t reg_a, reg_b, cr, config0, config2, config3;
	int i;
	char val[128];

	printf("rtl_init called\n");
	ne_init(dep);

	/* ID */
	outb_reg0(dep, DP_CR, CR_PS_P0);
	reg_a = inb_reg0(dep, DP_DUM1);
	reg_b = inb_reg0(dep, DP_DUM2);

	printf("rtl_init: '%c', '%c'\n", reg_a, reg_b);

	outb_reg0(dep, DP_CR, CR_PS_P3);
	config0 = inb_reg3(dep, 3);
	config2 = inb_reg3(dep, 5);
	config3 = inb_reg3(dep, 6);
	outb_reg0(dep, DP_CR, CR_PS_P0);

	printf("rtl_init: config 0/2/3 = %x/%x/%x\n",
		config0, config2, config3);

	if (0 == sys_getkenv("RTL8029FD",9+1, val, sizeof(val)))
	{
		printf("rtl_init: setting full-duplex mode\n");
		outb_reg0(dep, DP_CR, CR_PS_P3);

		cr= inb_reg3(dep, 1);
		outb_reg3(dep, 1, cr | 0xc0);

		outb_reg3(dep, 6, config3 | 0x40);
		config3 = inb_reg3(dep, 6);

		config2= inb_reg3(dep, 5);
		outb_reg3(dep, 5, config2 | 0x20);
		config2= inb_reg3(dep, 5);

		outb_reg3(dep, 1, cr);

		outb_reg0(dep, DP_CR, CR_PS_P0);

		printf("rtl_init: config 2 = %x\n", config2);
		printf("rtl_init: config 3 = %x\n", config3);
	}

	for (i= 0; i<64; i++)
		printf("%x ", get_ee_word(dep, i));
	printf("\n");

	if (0 == sys_getkenv("RTL8029MN",9+1, val, sizeof(val)))
	{
		ee_wen(dep);

		set_ee_word(dep, 0x78/2, 0x10ec);
		set_ee_word(dep, 0x7A/2, 0x8029);
		set_ee_word(dep, 0x7C/2, 0x10ec);
		set_ee_word(dep, 0x7E/2, 0x8029);

		ee_wds(dep);

		assert(get_ee_word(dep, 0x78/2) == 0x10ec);
		assert(get_ee_word(dep, 0x7A/2) == 0x8029);
		assert(get_ee_word(dep, 0x7C/2) == 0x10ec);
		assert(get_ee_word(dep, 0x7E/2) == 0x8029);
	}

	if (0 == sys_getkenv("RTL8029XXX",10+1, val, sizeof(val)))
	{
		ee_wen(dep);

		set_ee_word(dep, 0x76/2, 0x8029);

		ee_wds(dep);

		assert(get_ee_word(dep, 0x76/2) == 0x8029);
	}
}

static u16_t get_ee_word(dep, a)
dpeth_t *dep;
int a;
{
	int b, i, cmd;
	u16_t w;

	outb_reg0(dep, DP_CR, CR_PS_P3);	/* Bank 3 */

	/* Switch to 9346 mode and enable CS */
	outb_reg3(dep, 1, 0x80 | 0x8);

	cmd= 0x180 | (a & 0x3f);	/* 1 1 0 a5 a4 a3 a2 a1 a0 */
	for (i= 8; i >= 0; i--)
	{
		b= (cmd & (1 << i));
		b= (b ? 2 : 0);

		/* Cmd goes out on the rising edge of the clock */
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4 | b);
	}
	outb_reg3(dep, 1, 0x80 | 0x8);	/* End of cmd */

	w= 0;
	for (i= 0; i<16; i++)
	{
		w <<= 1;

		/* Data is shifted out on the rising edge. Read at the
		 * falling edge.
		 */
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4);
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		b= inb_reg3(dep, 1);
		w |= (b & 1);
	}

	outb_reg3(dep, 1, 0x80);		/* drop CS */
	outb_reg3(dep, 1, 0x00);		/* back to normal */
	outb_reg0(dep, DP_CR, CR_PS_P0);	/* back to bank 0 */

	return w;
}

static void ee_wen(dep)
dpeth_t *dep;
{
	int b, i, cmd;
	u16_t w;

	outb_reg0(dep, DP_CR, CR_PS_P3);	/* Bank 3 */

	/* Switch to 9346 mode and enable CS */
	outb_reg3(dep, 1, 0x80 | 0x8);

	cmd= 0x130;		/* 1 0 0 1 1 x x x x */
	for (i= 8; i >= 0; i--)
	{
		b= (cmd & (1 << i));
		b= (b ? 2 : 0);

		/* Cmd goes out on the rising edge of the clock */
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4 | b);
	}
	outb_reg3(dep, 1, 0x80 | 0x8);	/* End of cmd */
	outb_reg3(dep, 1, 0x80);	/* Drop CS */
	/* micro_delay(1); */			/* Is this required? */
}

static void set_ee_word(dpeth_t *dep, int a, u16_t w)
dpeth_t *dep;
int a;
u16_t w;
{
	int b, i, cmd;

	outb_reg3(dep, 1, 0x80 | 0x8);		/* Set CS */

	cmd= 0x140 | (a & 0x3f);		/* 1 0 1 a5 a4 a3 a2 a1 a0 */
	for (i= 8; i >= 0; i--)
	{
		b= (cmd & (1 << i));
		b= (b ? 2 : 0);

		/* Cmd goes out on the rising edge of the clock */
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4 | b);
	}
	for (i= 15; i >= 0; i--)
	{
		b= (w & (1 << i));
		b= (b ? 2 : 0);

		/* Cmd goes out on the rising edge of the clock */
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4 | b);
	}
	outb_reg3(dep, 1, 0x80 | 0x8);	/* End of data */
	outb_reg3(dep, 1, 0x80);	/* Drop CS */
	/* micro_delay(1); */			/* Is this required? */
	outb_reg3(dep, 1, 0x80 | 0x8);		/* Set CS */
	SPIN_UNTIL(inb_reg3(dep, 1) & 1, 10000);
	if (!(inb_reg3(dep, 1) & 1))
		panic("device remains busy");
}

static void ee_wds(dep)
dpeth_t *dep;
{
	int b, i, cmd;
	u16_t w;

	outb_reg0(dep, DP_CR, CR_PS_P3);	/* Bank 3 */

	/* Switch to 9346 mode and enable CS */
	outb_reg3(dep, 1, 0x80 | 0x8);

	cmd= 0x100;		/* 1 0 0 0 0 x x x x */
	for (i= 8; i >= 0; i--)
	{
		b= (cmd & (1 << i));
		b= (b ? 2 : 0);

		/* Cmd goes out on the rising edge of the clock */
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4 | b);
	}
	outb_reg3(dep, 1, 0x80 | 0x8);	/* End of cmd */
	outb_reg3(dep, 1, 0x80);	/* Drop CS */
	outb_reg3(dep, 1, 0x00);		/* back to normal */
	outb_reg0(dep, DP_CR, CR_PS_P0);	/* back to bank 0 */
}
#endif

static void tell_dev(buf, size, pci_bus, pci_dev, pci_func)
vir_bytes buf;
size_t size;
int pci_bus;
int pci_dev;
int pci_func;
{
	int r;
	endpoint_t dev_e;
	message m;

	r= ds_retrieve_label_endpt("amddev", &dev_e);
	if (r != OK)
	{
#if 0
		printf(
		"rtl8139`tell_dev: ds_retrieve_label_endpt failed for 'amddev': %d\n",
			r);
#endif
		return;
	}

	m.m_type= IOMMU_MAP;
	m.m2_i1= pci_bus;
	m.m2_i2= pci_dev;
	m.m2_i3= pci_func;
	m.m2_l1= buf;
	m.m2_l2= size;

	r= ipc_sendrec(dev_e, &m);
	if (r != OK)
	{
		printf("rtl8139`tell_dev: ipc_sendrec to %d failed: %d\n",
			dev_e, r);
		return;
	}
	if (m.m_type != OK)
	{
		printf("rtl8139`tell_dev: dma map request failed: %d\n",
			m.m_type);
		return;
	}
}

/*
 * $PchId: rtl8139.c,v 1.3 2003/09/11 14:15:15 philip Exp $
 */
