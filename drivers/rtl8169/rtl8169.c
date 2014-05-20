/*
 * rtl8169.c
 *
 * This file contains a ethernet device driver for Realtek rtl8169 based
 * ethernet cards.
 *
 */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <minix/com.h>
#include <minix/ds.h>
#include <minix/syslib.h>
#include <minix/type.h>
#include <minix/sysutil.h>
#include <minix/endpoint.h>
#include <minix/timers.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <machine/pci.h>

#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include "kernel/const.h"
#include "kernel/config.h"
#include "kernel/type.h"

#define VERBOSE		0		/* display message during init */

#include "rtl8169.h"

#define IOVEC_NR	16		/* I/O vectors are handled IOVEC_NR entries at a time. */

#define RE_DTCC_VALUE	600		/* DTCC Update after every 10 minutes */

#define RX_CONFIG_MASK	0xff7e1880	/* Clears the bits supported by chip */

#define RE_INTR_MASK	(RL_IMR_TDU | RL_IMR_FOVW | RL_IMR_PUN | RL_IMR_RDU | RL_IMR_TER | RL_IMR_TOK | RL_IMR_RER | RL_IMR_ROK)

#define RL_ENVVAR	"RTLETH"	/* Configuration */

typedef struct re_desc
{
	u32_t status;		/* command/status */
	u32_t vlan;		/* VLAN */
	u32_t addr_low;		/* low 32-bits of physical buffer address */
	u32_t addr_high;	/* high 32-bits of physical buffer address */
} re_desc;

typedef struct re_dtcc
{
	u32_t	TxOk_low;	/* low 32-bits of Tx Ok packets */
	u32_t	TxOk_high;	/* high 32-bits of Tx Ok packets */
	u32_t	RxOk_low;	/* low 32-bits of Rx Ok packets */
	u32_t	RxOk_high;	/* high 32-bits of Rx Ok packets */
	u32_t	TxEr_low;	/* low 32-bits of Tx errors */
	u32_t	TxEr_high;	/* high 32-bits of Tx errors */
	u32_t	RxEr;		/* Rx errors */
	u16_t	MissPkt;	/* Missed packets */
	u16_t	FAE;		/* Frame Aignment Error packets (MII mode only) */
	u32_t	Tx1Col;		/* Tx Ok packets with only 1 collision happened before Tx Ok */
	u32_t	TxMCol;		/* Tx Ok packets with > 1 and < 16 collisions happened before Tx Ok */
	u32_t	RxOkPhy_low;	/* low 32-bits of Rx Ok packets with physical addr destination ID */
	u32_t	RxOkPhy_high;	/* high 32-bits of Rx Ok packets with physical addr destination ID */
	u32_t	RxOkBrd_low;	/* low 32-bits of Rx Ok packets with broadcast destination ID */
	u32_t	RxOkBrd_high;	/* high 32-bits of Rx Ok packets with broadcast destination ID */
	u32_t	RxOkMul;	/* Rx Ok Packets with multicast destination ID */
	u16_t	TxAbt;		/* Tx abort packets */
	u16_t	TxUndrn;	/* Tx underrun packets */
} re_dtcc;

typedef struct re {
	port_t re_base_port;
	int re_irq;
	int re_mode;
	int re_flags;
	endpoint_t re_client;
	int re_link_up;
	int re_got_int;
	int re_send_int;
	int re_report_link;
	int re_need_reset;
	int re_tx_alive;
	int setup;
	u32_t re_mac;
	char *re_model;

	/* Rx */
	int re_rx_head;
	struct {
		int ret_busy;
		phys_bytes ret_buf;
		char *v_ret_buf;
	} re_rx[N_RX_DESC];

	vir_bytes re_read_s;
	re_desc *re_rx_desc;	/* Rx descriptor buffer */
	phys_bytes p_rx_desc;	/* Rx descriptor buffer physical */

	/* Tx */
	int re_tx_head;
	struct {
		int ret_busy;
		phys_bytes ret_buf;
		char *v_ret_buf;
	} re_tx[N_TX_DESC];
	re_desc *re_tx_desc;	/* Tx descriptor buffer */
	phys_bytes p_tx_desc;	/* Tx descriptor buffer physical */

	/* PCI related */
	int re_seen;		/* TRUE iff device available */

	/* 'large' items */
	int re_hook_id;		/* IRQ hook id at kernel */
	eth_stat_t re_stat;
	phys_bytes dtcc_buf;	/* Dump Tally Counter buffer physical */
	re_dtcc *v_dtcc_buf;	/* Dump Tally Counter buffer */
	u32_t dtcc_counter;	/* DTCC update counter */
	ether_addr_t re_address;
	message re_rx_mess;
	message re_tx_mess;
	char re_name[sizeof("rtl8169#n")];
	iovec_t re_iovec[IOVEC_NR];
	iovec_s_t re_iovec_s[IOVEC_NR];
	u32_t interrupts;
}
re_t;

#define REM_DISABLED	0x0
#define REM_ENABLED	0x1

#define REF_PACK_SENT	0x001
#define REF_PACK_RECV	0x002
#define REF_SEND_AVAIL	0x004
#define REF_READING	0x010
#define REF_EMPTY	0x000
#define REF_PROMISC	0x040
#define REF_MULTI	0x080
#define REF_BROAD	0x100
#define REF_ENABLED	0x200

static re_t re_state;

static int re_instance;

static unsigned my_inb(u16_t port)
{
	u32_t value;
	int s;
	if ((s = sys_inb(port, &value)) != OK)
		printf("RTL8169: warning, sys_inb failed: %d\n", s);
	return value;
}
static unsigned my_inw(u16_t port)
{
	u32_t value;
	int s;
	if ((s = sys_inw(port, &value)) != OK)
		printf("RTL8169: warning, sys_inw failed: %d\n", s);
	return value;
}
static unsigned my_inl(u16_t port)
{
	u32_t value;
	int s;
	if ((s = sys_inl(port, &value)) != OK)
		printf("RTL8169: warning, sys_inl failed: %d\n", s);
	return value;
}
#define rl_inb(port, offset)	(my_inb((port) + (offset)))
#define rl_inw(port, offset)	(my_inw((port) + (offset)))
#define rl_inl(port, offset)	(my_inl((port) + (offset)))

static void my_outb(u16_t port, u8_t value)
{
	int s;

	if ((s = sys_outb(port, value)) != OK)
		printf("RTL8169: warning, sys_outb failed: %d\n", s);
}
static void my_outw(u16_t port, u16_t value)
{
	int s;

	if ((s = sys_outw(port, value)) != OK)
		printf("RTL8169: warning, sys_outw failed: %d\n", s);
}
static void my_outl(u16_t port, u32_t value)
{
	int s;

	if ((s = sys_outl(port, value)) != OK)
		printf("RTL8169: warning, sys_outl failed: %d\n", s);
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
static void rl_do_reset(re_t *rep);
static void rl_getstat_s(message *mp);
static void reply(re_t *rep);
static void mess_reply(message *req, message *reply);
static void check_int_events(void);
static void do_hard_int(void);
static void dump_phy(const re_t *rep);
static void rl_handler(re_t *rep);
static void rl_watchdog_f(minix_timer_t *tp);

/*
 * The message used in the main loop is made global, so that rl_watchdog_f()
 * can change its message type to fake an interrupt message.
 */
static message m;
static int int_event_check;		/* set to TRUE if events arrived */

u32_t system_hz;

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

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

	while (TRUE) {
		if ((r = netdriver_receive(ANY, &m, &ipc_status)) != OK)
			panic("netdriver_receive failed: %d", r);

		if (is_ipc_notify(ipc_status)) {
			switch (_ENDPOINT_P(m.m_source)) {
			case CLOCK:
				/*
				 * Under MINIX, synchronous alarms are used
				 * instead of watchdog functions.
				 * The approach is very different: MINIX VMD
				 * timeouts are handled within the kernel
				 * (the watchdog is executed by CLOCK), and
				 * notify() the driver in some cases. MINIX
				 * timeouts result in a SYN_ALARM message to
				 * the driver and thus are handled where they
				 * should be handled. Locally, watchdog
				 * functions are used again.
				 */
				rl_watchdog_f(NULL);
				break;
			case HARDWARE:
				do_hard_int();
				if (int_event_check) {
					check_int_events();
				}
				break ;
			default:
				panic("illegal notify from: %d",	m.m_type);
			}

			/* done, get nwe message */
			continue;
		}

		switch (m.m_type) {
		case DL_WRITEV_S:	rl_writev_s(&m, FALSE);	 break;
		case DL_READV_S:	rl_readv_s(&m, FALSE);	 break;
		case DL_CONF:		rl_init(&m);		 break;
		case DL_GETSTAT_S:	rl_getstat_s(&m);	 break;
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
/* Initialize the rtl8169 driver. */
	long v;

	system_hz = sys_hz();

	v = 0;
	(void) env_parse("instance", "d", 0, &v, 0, 255);
	re_instance = (int) v;

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
		rl_outb(rep->re_base_port, RL_CR, RL_CR_RST);

	exit(0);
}

static void mdio_write(u16_t port, int regaddr, int value)
{
	int i;

	rl_outl(port,  RL_PHYAR, 0x80000000 | (regaddr & 0x1F) << 16 | (value & 0xFFFF));

	for (i = 20; i > 0; i--) {
		/*
		 * Check if the RTL8169 has completed writing to the specified
		 * MII register
		 */
		if (!(rl_inl(port, RL_PHYAR) & 0x80000000))
			break;
		else
			micro_delay(50);
	}
}

static int mdio_read(u16_t port, int regaddr)
{
	int i, value = -1;

	rl_outl(port, RL_PHYAR, (regaddr & 0x1F) << 16);

	for (i = 20; i > 0; i--) {
		/*
		 * Check if the RTL8169 has completed retrieving data from
		 * the specified MII register
		 */
		if (rl_inl(port, RL_PHYAR) & 0x80000000) {
			value = (int)(rl_inl(port, RL_PHYAR) & 0xFFFF);
			break;
		} else
			micro_delay(50);
	}
	return value;
}

/*===========================================================================*
 *				check_int_events			     *
 *===========================================================================*/
static void check_int_events(void)
{
	re_t *rep;

	rep = &re_state;

	if (rep->re_mode != REM_ENABLED)
		return;
	if (!rep->re_got_int)
		return;
	rep->re_got_int = 0;
	assert(rep->re_flags & REF_ENABLED);
	rl_check_ints(rep);
}

static void rtl8169_update_stat(re_t *rep)
{
	port_t port;
	int i;

	port = rep->re_base_port;

	/* Fetch Missed Packets */
	rep->re_stat.ets_missedP += rl_inw(port, RL_MPC);
	rl_outw(port, RL_MPC, 0x00);

	/* Dump Tally Counter Command */
	rl_outl(port, RL_DTCCR_HI, 0);		/* 64 bits */
	rl_outl(port, RL_DTCCR_LO, rep->dtcc_buf | RL_DTCCR_CMD);
	for (i = 0; i < 1000; i++) {
		if (!(rl_inl(port, RL_DTCCR_LO) & RL_DTCCR_CMD))
			break;
		micro_delay(10);
	}

	/* Update counters */
	rep->re_stat.ets_frameAll = rep->v_dtcc_buf->FAE;
	rep->re_stat.ets_transDef = rep->v_dtcc_buf->TxUndrn;
	rep->re_stat.ets_transAb = rep->v_dtcc_buf->TxAbt;
	rep->re_stat.ets_collision =
		rep->v_dtcc_buf->Tx1Col + rep->v_dtcc_buf->TxMCol;
}

#if 0
/*===========================================================================*
 *				rtl8169_dump				     *
 *===========================================================================*/
static void rtl8169_dump(void)
{
	re_dtcc *dtcc;
	re_t *rep;

	rep = &re_state;

	printf("\n");
	if (rep->re_mode == REM_DISABLED)
		printf("Realtek RTL 8169 instance %d is disabled\n",
			re_instance);

	if (rep->re_mode != REM_ENABLED)
		return;

	rtl8169_update_stat(rep);

	printf("Realtek RTL 8169 statistics of instance %d:\n", re_instance);

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
	printf("OWC        :%8ld\n", rep->re_stat.ets_OWC);
	printf("interrupts :%8lu\n", rep->interrupts);

	printf("\nRealtek RTL 8169 Tally Counters:\n");

	dtcc = rep->v_dtcc_buf;

	if (dtcc->TxOk_high)
		printf("TxOk       :%8ld%08ld\t", dtcc->TxOk_high, dtcc->TxOk_low);
	else
		printf("TxOk       :%16lu\t", dtcc->TxOk_low);

	if (dtcc->RxOk_high)
		printf("RxOk       :%8ld%08ld\n", dtcc->RxOk_high, dtcc->RxOk_low);
	else
		printf("RxOk       :%16lu\n", dtcc->RxOk_low);

	if (dtcc->TxEr_high)
		printf("TxEr       :%8ld%08ld\t", dtcc->TxEr_high, dtcc->TxEr_low);
	else
		printf("TxEr       :%16ld\t", dtcc->TxEr_low);

	printf("RxEr       :%16ld\n", dtcc->RxEr);

	printf("Tx1Col     :%16ld\t", dtcc->Tx1Col);
	printf("TxMCol     :%16ld\n", dtcc->TxMCol);

	if (dtcc->RxOkPhy_high)
		printf("RxOkPhy    :%8ld%08ld\t", dtcc->RxOkPhy_high, dtcc->RxOkPhy_low);
	else
		printf("RxOkPhy    :%16ld\t", dtcc->RxOkPhy_low);

	if (dtcc->RxOkBrd_high)
		printf("RxOkBrd    :%8ld%08ld\n", dtcc->RxOkBrd_high, dtcc->RxOkBrd_low);
	else
		printf("RxOkBrd    :%16ld\n", dtcc->RxOkBrd_low);

	printf("RxOkMul    :%16ld\t", dtcc->RxOkMul);
	printf("MissPkt    :%16d\n", dtcc->MissPkt);

	printf("\nRealtek RTL 8169 Miscellaneous Info:\n");

	printf("re_flags   :      0x%08x\n", rep->re_flags);
	printf("tx_head    :%8d  busy %d\t",
		rep->re_tx_head, rep->re_tx[rep->re_tx_head].ret_busy);
}
#endif

/*===========================================================================*
 *				do_init					     *
 *===========================================================================*/
static void rl_init(mp)
message *mp;
{
	static int first_time = 1;

	re_t *rep;
	message reply_mess;

	if (first_time) {
		first_time = 0;
		rl_pci_conf();	/* Configure PCI devices. */

		/* Use a synchronous alarm instead of a watchdog timer. */
		sys_setalarm(system_hz, 0);
	}

	rep = &re_state;
	if (rep->re_mode == REM_DISABLED) {
		/* This is the default, try to (re)locate the device. */
		rl_conf_hw(rep);
		if (rep->re_mode == REM_DISABLED) {
			/* Probe failed, or the device is configured off. */
			reply_mess.m_type = DL_CONF_REPLY;
			reply_mess.m_netdrv_net_dl_conf.stat = ENXIO;
			mess_reply(mp, &reply_mess);
			return;
		}
		if (rep->re_mode == REM_ENABLED)
			rl_init_hw(rep);
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

	rep = &re_state;

	strlcpy(rep->re_name, "rtl8169#0", sizeof(rep->re_name));
	rep->re_name[8] += re_instance;
	rep->re_seen = FALSE;

	pci_init();

	if (rl_probe(rep, re_instance))
		rep->re_seen = TRUE;
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

	r = pci_first_dev(&devind, &vid, &did);
	if (r == 0)
		return 0;

	while (skip--) {
		r = pci_next_dev(&devind, &vid, &did);
		if (!r)
			return 0;
	}

#if VERBOSE
	dname = pci_dev_name(vid, did);
	if (!dname)
		dname = "unknown device";
	printf("%s: ", rep->re_name);
	printf("%s (%x/%x) at %s\n", dname, vid, did, pci_slot_name(devind));
#endif

	pci_reserve(devind);
	bar = pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;
	if (bar < 0x400) {
		panic("base address is not properly configured");
	}
	rep->re_base_port = bar;

	ilr = pci_attr_r8(devind, PCI_ILR);
	rep->re_irq = ilr;
#if VERBOSE
	printf("%s: using I/O address 0x%lx, IRQ %d\n",
		rep->re_name, (unsigned long)bar, ilr);
#endif

	return TRUE;
}

/*===========================================================================*
 *				rl_conf_hw				     *
 *===========================================================================*/
static void rl_conf_hw(rep)
re_t *rep;
{
	static eth_stat_t empty_stat = {0, 0, 0, 0, 0, 0 	/* ,... */ };

	rep->re_mode = REM_DISABLED;		/* Superfluous */

	if (rep->re_seen)
		rep->re_mode = REM_ENABLED;	/* PCI device is present */
	if (rep->re_mode != REM_ENABLED)
		return;

	rep->re_flags = REF_EMPTY;
	rep->re_link_up = 0;
	rep->re_got_int = 0;
	rep->re_send_int = 0;
	rep->re_report_link = 0;
	rep->re_need_reset = 0;
	rep->re_tx_alive = 0;
	rep->re_rx_head = 0;
	rep->re_read_s = 0;
	rep->re_tx_head = 0;
	rep->re_stat = empty_stat;
	rep->dtcc_counter = 0;
}

/*===========================================================================*
 *				rl_init_buf				     *
 *===========================================================================*/
static void rl_init_buf(rep)
re_t *rep;
{
	size_t rx_bufsize, tx_bufsize, rx_descsize, tx_descsize, tot_bufsize;
	struct re_desc *desc;
	phys_bytes buf;
	char *mallocbuf;
	int d;

	assert(!rep->setup);

	/* Allocate receive and transmit descriptors */
	rx_descsize = (N_RX_DESC * sizeof(struct re_desc));
	tx_descsize = (N_TX_DESC * sizeof(struct re_desc));

	/* Allocate receive and transmit buffers */
	tx_bufsize = ETH_MAX_PACK_SIZE_TAGGED;
	if (tx_bufsize % 4)
		tx_bufsize += 4-(tx_bufsize % 4);	/* Align */
	rx_bufsize = RX_BUFSIZE;
	tot_bufsize = rx_descsize + tx_descsize;
	tot_bufsize += (N_TX_DESC * tx_bufsize) + (N_RX_DESC * rx_bufsize);
	tot_bufsize += sizeof(struct re_dtcc);

	if (tot_bufsize % 4096)
		tot_bufsize += 4096 - (tot_bufsize % 4096);

	if (!(mallocbuf = alloc_contig(tot_bufsize, AC_ALIGN64K, &buf)))
		panic("Couldn't allocate kernel buffer");

	/* Rx Descriptor */
	rep->re_rx_desc = (re_desc *)mallocbuf;
	rep->p_rx_desc = buf;
	memset(mallocbuf, 0x00, rx_descsize);
	buf += rx_descsize;
	mallocbuf += rx_descsize;

	/* Tx Descriptor */
	rep->re_tx_desc = (re_desc *)mallocbuf;
	rep->p_tx_desc = buf;
	memset(mallocbuf, 0x00, tx_descsize);
	buf += tx_descsize;
	mallocbuf += tx_descsize;

	desc = rep->re_rx_desc;
	for (d = 0; d < N_RX_DESC; d++) {
		/* Setting Rx buffer */
		rep->re_rx[d].ret_buf = buf;
		rep->re_rx[d].v_ret_buf = mallocbuf;
		buf += rx_bufsize;
		mallocbuf += rx_bufsize;

		/* Setting Rx descriptor */
		if (d == (N_RX_DESC - 1)) /* Last descriptor? if so, set the EOR bit */
			desc->status =  DESC_EOR | DESC_OWN | (RX_BUFSIZE & DESC_RX_LENMASK);
		else
			desc->status =  DESC_OWN | (RX_BUFSIZE & DESC_RX_LENMASK);

		desc->addr_low =  rep->re_rx[d].ret_buf;
		desc++;
	}
	desc = rep->re_tx_desc;
	for (d = 0; d < N_TX_DESC; d++) {
		rep->re_tx[d].ret_busy = FALSE;
		rep->re_tx[d].ret_buf = buf;
		rep->re_tx[d].v_ret_buf = mallocbuf;
		buf += tx_bufsize;
		mallocbuf += tx_bufsize;

		/* Setting Tx descriptor */
		desc->addr_low =  rep->re_tx[d].ret_buf;
		desc++;
	}

	/* Dump Tally Counter buffer */
	rep->dtcc_buf = buf;
	rep->v_dtcc_buf = (re_dtcc *)mallocbuf;

	rep->setup = 1;
}

/*===========================================================================*
 *				rl_init_hw				     *
 *===========================================================================*/
static void rl_init_hw(rep)
re_t *rep;
{
	int s;
#if VERBOSE
	int i;
#endif

	rep->re_flags = REF_EMPTY;
	rep->re_flags |= REF_ENABLED;

	/*
	 * Set the interrupt handler. The policy is to only send HARD_INT
	 * notifications. Don't reenable interrupts automatically. The id
	 * that is passed back is the interrupt line number.
	 */
	rep->re_hook_id = rep->re_irq;
	if ((s = sys_irqsetpolicy(rep->re_irq, 0, &rep->re_hook_id)) != OK)
		printf("RTL8169: error, couldn't set IRQ policy: %d\n", s);

	rl_reset_hw(rep);

	if ((s = sys_irqenable(&rep->re_hook_id)) != OK)
		printf("RTL8169: error, couldn't enable interrupts: %d\n", s);

#if VERBOSE
	printf("%s: model: %s mac: 0x%08x\n",
		rep->re_name, rep->re_model, rep->re_mac);
#endif

	rl_confaddr(rep);
#if VERBOSE
	printf("%s: Ethernet address ", rep->re_name);
	for (i = 0; i < 6; i++) {
		printf("%x%c", rep->re_address.ea_addr[i],
			i < 5 ? ':' : '\n');
	}
#endif
}

static void rtl8169s_phy_config(port_t port)
{
	mdio_write(port, 0x1f, 0x0001);
	mdio_write(port, 0x06, 0x006e);
	mdio_write(port, 0x08, 0x0708);
	mdio_write(port, 0x15, 0x4000);
	mdio_write(port, 0x18, 0x65c7);

	mdio_write(port, 0x1f, 0x0001);
	mdio_write(port, 0x03, 0x00a1);
	mdio_write(port, 0x02, 0x0008);
	mdio_write(port, 0x01, 0x0120);
	mdio_write(port, 0x00, 0x1000);
	mdio_write(port, 0x04, 0x0800);
	mdio_write(port, 0x04, 0x0000);

	mdio_write(port, 0x03, 0xff41);
	mdio_write(port, 0x02, 0xdf60);
	mdio_write(port, 0x01, 0x0140);
	mdio_write(port, 0x00, 0x0077);
	mdio_write(port, 0x04, 0x7800);
	mdio_write(port, 0x04, 0x7000);

	mdio_write(port, 0x03, 0x802f);
	mdio_write(port, 0x02, 0x4f02);
	mdio_write(port, 0x01, 0x0409);
	mdio_write(port, 0x00, 0xf0f9);
	mdio_write(port, 0x04, 0x9800);
	mdio_write(port, 0x04, 0x9000);

	mdio_write(port, 0x03, 0xdf01);
	mdio_write(port, 0x02, 0xdf20);
	mdio_write(port, 0x01, 0xff95);
	mdio_write(port, 0x00, 0xba00);
	mdio_write(port, 0x04, 0xa800);
	mdio_write(port, 0x04, 0xa000);

	mdio_write(port, 0x03, 0xff41);
	mdio_write(port, 0x02, 0xdf20);
	mdio_write(port, 0x01, 0x0140);
	mdio_write(port, 0x00, 0x00bb);
	mdio_write(port, 0x04, 0xb800);
	mdio_write(port, 0x04, 0xb000);

	mdio_write(port, 0x03, 0xdf41);
	mdio_write(port, 0x02, 0xdc60);
	mdio_write(port, 0x01, 0x6340);
	mdio_write(port, 0x00, 0x007d);
	mdio_write(port, 0x04, 0xd800);
	mdio_write(port, 0x04, 0xd000);

	mdio_write(port, 0x03, 0xdf01);
	mdio_write(port, 0x02, 0xdf20);
	mdio_write(port, 0x01, 0x100a);
	mdio_write(port, 0x00, 0xa0ff);
	mdio_write(port, 0x04, 0xf800);
	mdio_write(port, 0x04, 0xf000);

	mdio_write(port, 0x1f, 0x0000);
	mdio_write(port, 0x0b, 0x0000);
	mdio_write(port, 0x00, 0x9200);
}

static void rtl8169scd_phy_config(port_t port)
{
	mdio_write(port, 0x1f, 0x0001);
	mdio_write(port, 0x04, 0x0000);
	mdio_write(port, 0x03, 0x00a1);
	mdio_write(port, 0x02, 0x0008);
	mdio_write(port, 0x01, 0x0120);
	mdio_write(port, 0x00, 0x1000);
	mdio_write(port, 0x04, 0x0800);
	mdio_write(port, 0x04, 0x9000);
	mdio_write(port, 0x03, 0x802f);
	mdio_write(port, 0x02, 0x4f02);
	mdio_write(port, 0x01, 0x0409);
	mdio_write(port, 0x00, 0xf099);
	mdio_write(port, 0x04, 0x9800);
	mdio_write(port, 0x04, 0xa000);
	mdio_write(port, 0x03, 0xdf01);
	mdio_write(port, 0x02, 0xdf20);
	mdio_write(port, 0x01, 0xff95);
	mdio_write(port, 0x00, 0xba00);
	mdio_write(port, 0x04, 0xa800);
	mdio_write(port, 0x04, 0xf000);
	mdio_write(port, 0x03, 0xdf01);
	mdio_write(port, 0x02, 0xdf20);
	mdio_write(port, 0x01, 0x101a);
	mdio_write(port, 0x00, 0xa0ff);
	mdio_write(port, 0x04, 0xf800);
	mdio_write(port, 0x04, 0x0000);
	mdio_write(port, 0x1f, 0x0000);

	mdio_write(port, 0x1f, 0x0001);
	mdio_write(port, 0x10, 0xf41b);
	mdio_write(port, 0x14, 0xfb54);
	mdio_write(port, 0x18, 0xf5c7);
	mdio_write(port, 0x1f, 0x0000);

	mdio_write(port, 0x1f, 0x0001);
	mdio_write(port, 0x17, 0x0cc0);
	mdio_write(port, 0x1f, 0x0000);
}

/*===========================================================================*
 *				rl_reset_hw				     *
 *===========================================================================*/
static void rl_reset_hw(rep)
re_t *rep;
{
	port_t port;
	u32_t t;
	int i;

	port = rep->re_base_port;

	rl_outw(port, RL_IMR, 0x0000);

	/* Reset the device */
	rl_outb(port, RL_CR, RL_CR_RST);
	SPIN_UNTIL(!(rl_inb(port, RL_CR) & RL_CR_RST), 1000000);
	if (rl_inb(port, RL_CR) & RL_CR_RST)
		printf("rtl8169: reset failed to complete");
	rl_outw(port, RL_ISR, 0xFFFF);

	/* Get Model and MAC info */
	t = rl_inl(port, RL_TCR);
	rep->re_mac = (t & (RL_TCR_HWVER_AM | RL_TCR_HWVER_BM));
	switch (rep->re_mac) {
	case RL_TCR_HWVER_RTL8169:
		rep->re_model = "RTL8169";

		rl_outw(port, RL_CCR_UNDOC, 0x01);
		break;
	case RL_TCR_HWVER_RTL8169S:
		rep->re_model = "RTL8169S";

		rtl8169s_phy_config(port);

		rl_outw(port, RL_CCR_UNDOC, 0x01);
		mdio_write(port, 0x0b, 0x0000);		/* w 0x0b 15 0 0 */
		break;
	case RL_TCR_HWVER_RTL8110S:
		rep->re_model = "RTL8110S";

		rtl8169s_phy_config(port);

		rl_outw(port, RL_CCR_UNDOC, 0x01);
		break;
	case RL_TCR_HWVER_RTL8169SB:
		rep->re_model = "RTL8169SB";

		mdio_write(port, 0x1f, 0x02);
		mdio_write(port, 0x01, 0x90d0);
		mdio_write(port, 0x1f, 0x00);

		rl_outw(port, RL_CCR_UNDOC, 0x01);
		break;
	case RL_TCR_HWVER_RTL8110SCd:
		rep->re_model = "RTL8110SCd";

		rtl8169scd_phy_config(port);

		rl_outw(port, RL_CCR_UNDOC, 0x01);
		break;
	case RL_TCR_HWVER_RTL8105E:
		rep->re_model = "RTL8105E";
		break;
	default:
		rep->re_model = "Unknown";
		rep->re_mac = t;
		break;
	}

	mdio_write(port, MII_CTRL, MII_CTRL_RST);
	for (i = 0; i < 1000; i++) {
		t = mdio_read(port, MII_CTRL);
		if (!(t & MII_CTRL_RST))
			break;
		else
			micro_delay(100);
	}

	t = mdio_read(port, MII_CTRL) | MII_CTRL_ANE | MII_CTRL_DM | MII_CTRL_SP_1000;
	mdio_write(port, MII_CTRL, t);

	t = mdio_read(port, MII_ANA);
	t |= MII_ANA_10THD | MII_ANA_10TFD | MII_ANA_100TXHD | MII_ANA_100TXFD;
	t |= MII_ANA_PAUSE_SYM | MII_ANA_PAUSE_ASYM;
	mdio_write(port, MII_ANA, t);

	t = mdio_read(port, MII_1000_CTRL) | 0x300;
	mdio_write(port, MII_1000_CTRL, t);

	/* Restart Auto-Negotiation Process */
	t = mdio_read(port, MII_CTRL) | MII_CTRL_ANE | MII_CTRL_RAN;
	mdio_write(port, MII_CTRL, t);

	rl_outw(port, RL_9346CR, RL_9346CR_EEM_CONFIG);	/* Unlock */

	switch (rep->re_mac) {
	case RL_TCR_HWVER_RTL8169S:
	case RL_TCR_HWVER_RTL8110S:
		/* Bit-3 and bit-14 of the C+CR register MUST be 1. */
		t = rl_inw(port, RL_CPLUSCMD);
		rl_outw(port, RL_CPLUSCMD, t | RL_CPLUS_MULRW | (1 << 14));
		break;
	case RL_TCR_HWVER_RTL8169:
	case RL_TCR_HWVER_RTL8169SB:
	case RL_TCR_HWVER_RTL8110SCd:
		t = rl_inw(port, RL_CPLUSCMD);
		rl_outw(port, RL_CPLUSCMD, t | RL_CPLUS_MULRW);
		break;
	}

	rl_outw(port, RL_INTRMITIGATE, 0x00);

	t = rl_inb(port, RL_CR);
	rl_outb(port, RL_CR, t | RL_CR_RE | RL_CR_TE);

	/* Initialize Rx */
	rl_outw(port, RL_RMS, RX_BUFSIZE);	/* Maximum rx packet size */
	t = rl_inl(port, RL_RCR) & RX_CONFIG_MASK;
	rl_outl(port, RL_RCR, RL_RCR_RXFTH_UNLIM | RL_RCR_MXDMA_1024 | t);
	rl_outl(port, RL_RDSAR_LO, rep->p_rx_desc);
	rl_outl(port, RL_RDSAR_HI, 0x00);	/* For 64 bit */

	/* Initialize Tx */
	rl_outw(port, RL_ETTHR, 0x3f);		/* No early transmit */
	rl_outl(port, RL_TCR, RL_TCR_MXDMA_2048 | RL_TCR_IFG_STD);
	rl_outl(port, RL_TNPDS_LO, rep->p_tx_desc);
	rl_outl(port, RL_TNPDS_HI, 0x00);	/* For 64 bit */

	rl_outw(port, RL_9346CR, RL_9346CR_EEM_NORMAL);	/* Lock */

	rl_outw(port, RL_MPC, 0x00);
	rl_outw(port, RL_MULINT, rl_inw(port, RL_MULINT) & 0xF000);
	rl_outw(port, RL_IMR, RE_INTR_MASK);
}

/*===========================================================================*
 *				rl_confaddr				     *
 *===========================================================================*/
static void rl_confaddr(rep)
re_t *rep;
{
	static char eakey[] = RL_ENVVAR "#_EA";
	static char eafmt[] = "x:x:x:x:x:x";

	int i;
	port_t port;
	u32_t w;
	long v;

	/* User defined ethernet address? */
	eakey[sizeof(RL_ENVVAR)-1] = '0' + re_instance;

	port = rep->re_base_port;

	for (i = 0; i < 6; i++) {
		if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
			break;
		rep->re_address.ea_addr[i] = v;
	}

	if (i != 0 && i != 6)
		env_panic(eakey);	/* It's all or nothing */

	/* Should update ethernet address in hardware */
	if (i == 6) {
		port = rep->re_base_port;
		rl_outb(port, RL_9346CR, RL_9346CR_EEM_CONFIG);
		w = 0;
		for (i = 0; i < 4; i++)
			w |= (rep->re_address.ea_addr[i] << (i * 8));
		rl_outl(port, RL_IDR, w);
		w = 0;
		for (i = 4; i < 6; i++)
			w |= (rep->re_address.ea_addr[i] << ((i-4) * 8));
		rl_outl(port, RL_IDR + 4, w);
		rl_outb(port, RL_9346CR, RL_9346CR_EEM_NORMAL);
	}

	/* Get ethernet address */
	for (i = 0; i < 6; i++)
		rep->re_address.ea_addr[i] = rl_inb(port, RL_IDR+i);
}

/*===========================================================================*
 *				rl_rec_mode				     *
 *===========================================================================*/
static void rl_rec_mode(rep)
re_t *rep;
{
	port_t port;
	u32_t rcr;
	u32_t mc_filter[2];		/* Multicast hash filter */

	port = rep->re_base_port;

	mc_filter[1] = mc_filter[0] = 0xffffffff;
	rl_outl(port, RL_MAR + 0, mc_filter[0]);
	rl_outl(port, RL_MAR + 4, mc_filter[1]);

	rcr = rl_inl(port, RL_RCR);
	rcr &= ~(RL_RCR_AB | RL_RCR_AM | RL_RCR_APM | RL_RCR_AAP);
	if (rep->re_flags & REF_PROMISC)
		rcr |= RL_RCR_AB | RL_RCR_AM | RL_RCR_AAP;
	if (rep->re_flags & REF_BROAD)
		rcr |= RL_RCR_AB;
	if (rep->re_flags & REF_MULTI)
		rcr |= RL_RCR_AM;
	rcr |= RL_RCR_APM;
	rl_outl(port, RL_RCR, RL_RCR_RXFTH_UNLIM | RL_RCR_MXDMA_1024 | rcr);
}

void transmittest(re_t *rep)
{
	int tx_head;
	int ipc_status;

	tx_head = rep->re_tx_head;

	if(rep->re_tx[tx_head].ret_busy) {
		do {
			message m;
			int r;
			if ((r = netdriver_receive(ANY, &m, &ipc_status)) != OK)
				panic("netdriver_receive failed: %d", r);
		} while(m.m_source != HARDWARE);
		assert(!(rep->re_flags & REF_SEND_AVAIL));
		rep->re_flags |= REF_SEND_AVAIL;
	}

	return;
}

/*===========================================================================*
 *				rl_readv_s				     *
 *===========================================================================*/
static void rl_readv_s(const message *mp, int from_int)
{
	int i, j, n, s, count, size, index;
	port_t port;
	unsigned totlen, packlen;
	re_desc *desc;
	u32_t rxstat = 0x12345678;
	re_t *rep;
	iovec_s_t *iovp;
	int cps;
	int iov_offset = 0;

	rep = &re_state;

	rep->re_client = mp->m_source;
	count = mp->m_net_netdrv_dl_readv_s.count;

	assert(rep->re_mode == REM_ENABLED);
	assert(rep->re_flags & REF_ENABLED);

	port = rep->re_base_port;

	/*
	 * Assume that the RL_CR_BUFE check was been done by rl_checks_ints
	 */
	if (!from_int && (rl_inb(port, RL_CR) & RL_CR_BUFE))
		goto suspend;		/* Receive buffer is empty, suspend */

	index = rep->re_rx_head;
	desc = rep->re_rx_desc;
	desc += index;
readvs_loop:
	rxstat = desc->status;

	if (rxstat & DESC_OWN)
		goto suspend;

	if (rxstat & DESC_RX_CRC)
		rep->re_stat.ets_CRCerr++;

	if ((rxstat & (DESC_FS | DESC_LS)) != (DESC_FS | DESC_LS)) {
#if VERBOSE
		printf("rl_readv_s: packet is fragmented\n");
#endif
		/* Fix the fragmented packet */
		if (index == N_RX_DESC - 1) {
			desc->status =  DESC_EOR | DESC_OWN | (RX_BUFSIZE & DESC_RX_LENMASK);
			index = 0;
			desc = rep->re_rx_desc;
		} else {
			desc->status =  DESC_OWN | (RX_BUFSIZE & DESC_RX_LENMASK);
			index++;
			desc++;
		}
		goto readvs_loop;	/* Loop until we get correct packet */
	}

	totlen = rxstat & DESC_RX_LENMASK;
	if (totlen < 8 || totlen > 2 * ETH_MAX_PACK_SIZE) {
		/* Someting went wrong */
		printf("rl_readv_s: bad length (%u) in status 0x%08x\n",
			totlen, rxstat);
		panic(NULL);
	}

	/* Should subtract the CRC */
	packlen = totlen - ETH_CRC_SIZE;

	size = 0;
	for (i = 0; i < count; i += IOVEC_NR,
		iov_offset += IOVEC_NR * sizeof(rep->re_iovec_s[0]))
	{
		n = IOVEC_NR;
		if (i + n > count)
			n = count-i;
		cps = sys_safecopyfrom(mp->m_source,
			mp->m_net_netdrv_dl_readv_s.grant, iov_offset,
			(vir_bytes) rep->re_iovec_s,
			n * sizeof(rep->re_iovec_s[0]));
		if (cps != OK) {
			panic("rl_readv_s: sys_safecopyfrom failed: %d", 				cps);
		}

		for (j = 0, iovp = rep->re_iovec_s; j < n; j++, iovp++) {
			s = iovp->iov_size;
			if (size + s > packlen) {
				assert(packlen > size);
				s = packlen-size;
			}

			cps = sys_safecopyto(mp->m_source, iovp->iov_grant, 0,
				(vir_bytes) rep->re_rx[index].v_ret_buf + size, s);
			if (cps != OK)
				panic("rl_readv_s: sys_safecopyto failed: %d", cps);

			size += s;
			if (size == packlen)
				break;
		}
		if (size == packlen)
			break;
	}
	if (size < packlen)
		assert(0);

	rep->re_stat.ets_packetR++;
	rep->re_read_s = packlen;
	if (index == N_RX_DESC - 1) {
		desc->status =  DESC_EOR | DESC_OWN | (RX_BUFSIZE & DESC_RX_LENMASK);
		index = 0;
	} else {
		desc->status =  DESC_OWN | (RX_BUFSIZE & DESC_RX_LENMASK);
		index++;
	}
	rep->re_rx_head = index;
	assert(rep->re_rx_head < N_RX_DESC);
	rep->re_flags = (rep->re_flags & ~REF_READING) | REF_PACK_RECV;

	if (!from_int)
		reply(rep);

	return;

suspend:
	if (from_int) {
		assert(rep->re_flags & REF_READING);

		/* No need to store any state */
		return;
	}

	rep->re_rx_mess = *mp;
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
	re_desc *desc;
	char *ret;
	int cps;
	int iov_offset = 0;

	rep = &re_state;

	rep->re_client = mp->m_source;
	count = mp->m_net_netdrv_dl_writev_s.count;
	assert(rep->setup);

	assert(rep->re_mode == REM_ENABLED);
	assert(rep->re_flags & REF_ENABLED);

	if (from_int) {
		assert(rep->re_flags & REF_SEND_AVAIL);
		rep->re_flags &= ~REF_SEND_AVAIL;
		rep->re_send_int = FALSE;
		rep->re_tx_alive = TRUE;
	}

	tx_head = rep->re_tx_head;

	desc = rep->re_tx_desc;
	desc += tx_head;

	if(!desc || !rep->re_tx_desc) {
		printf("desc %p, re_tx_desc %p, tx_head %d, setup %d\n",
			desc, rep->re_tx_desc, tx_head, rep->setup);
	}

	assert(rep->re_tx_desc);
	assert(rep->re_tx_head >= 0 && rep->re_tx_head < N_TX_DESC);

	assert(desc);

	if (rep->re_tx[tx_head].ret_busy) {
		assert(!(rep->re_flags & REF_SEND_AVAIL));
		rep->re_flags |= REF_SEND_AVAIL;
		if (rep->re_tx[tx_head].ret_busy)
			goto suspend;

		/*
		 * Race condition, the interrupt handler may clear re_busy
		 * before we got a chance to set REF_SEND_AVAIL. Checking
		 * ret_busy twice should be sufficient.
		 */
#if VERBOSE
		printf("rl_writev_s: race detected\n");
#endif
		rep->re_flags &= ~REF_SEND_AVAIL;
		rep->re_send_int = FALSE;
	}

	assert(!(rep->re_flags & REF_SEND_AVAIL));
	assert(!(rep->re_flags & REF_PACK_SENT));

	size = 0;
	ret = rep->re_tx[tx_head].v_ret_buf;
	for (i = 0; i < count; i += IOVEC_NR,
		iov_offset += IOVEC_NR * sizeof(rep->re_iovec_s[0]))
	{
		n = IOVEC_NR;
		if (i + n > count)
			n = count - i;
		cps = sys_safecopyfrom(mp->m_source,
			mp->m_net_netdrv_dl_writev_s.grant, iov_offset,
			(vir_bytes) rep->re_iovec_s,
			n * sizeof(rep->re_iovec_s[0]));
		if (cps != OK) {
			panic("rl_writev_s: sys_safecopyfrom failed: %d", 				cps);
		}

		for (j = 0, iovp = rep->re_iovec_s; j < n; j++, iovp++) {
			s = iovp->iov_size;
			if (size + s > ETH_MAX_PACK_SIZE_TAGGED)
				panic("invalid packet size");

			cps = sys_safecopyfrom(mp->m_source, iovp->iov_grant,
				0, (vir_bytes) ret, s);
			if (cps != OK) {
				panic("rl_writev_s: sys_safecopyfrom failed: %d", cps);
			}
			size += s;
			ret += s;
		}
	}
	assert(desc);
	if (size < ETH_MIN_PACK_SIZE)
		panic("invalid packet size: %d", size);

	rep->re_tx[tx_head].ret_busy = TRUE;

	if (tx_head == N_TX_DESC - 1) {
		desc->status =  DESC_EOR | DESC_OWN | DESC_FS | DESC_LS | size;
		tx_head = 0;
	} else {
		desc->status =  DESC_OWN | DESC_FS | DESC_LS | size;
		tx_head++;
	}

	assert(tx_head < N_TX_DESC);
	rep->re_tx_head = tx_head;

	rl_outl(rep->re_base_port, RL_TPPOLL, RL_TPPOLL_NPQ);
	rep->re_flags |= REF_PACK_SENT;

	/*
	 * If the interrupt handler called, don't send a reply. The reply
	 * will be sent after all interrupts are handled.
	 */
	if (from_int)
		return;
	reply(rep);
	return;

suspend:
	if (from_int)
		panic("should not be sending");

	rep->re_tx_mess = *mp;
	reply(rep);
}

/*===========================================================================*
 *				rl_check_ints				     *
 *===========================================================================*/
static void rl_check_ints(rep)
re_t *rep;
{
	int re_flags;

	re_flags = rep->re_flags;

	if ((re_flags & REF_READING) &&
		!(rl_inb(rep->re_base_port, RL_CR) & RL_CR_BUFE))
	{
		assert(rep->re_rx_mess.m_type == DL_READV_S);
		rl_readv_s(&rep->re_rx_mess, TRUE /* from int */);
	}

	if (rep->re_need_reset)
		rl_do_reset(rep);

	if (rep->re_send_int) {
		assert(rep->re_tx_mess.m_type == DL_WRITEV_S);
		rl_writev_s(&rep->re_tx_mess, TRUE /* from int */);
	}

	if (rep->re_report_link) {
		rep->re_report_link = FALSE;

		rl_report_link(rep);
	}

	if (rep->re_flags & (REF_PACK_SENT | REF_PACK_RECV))
		reply(rep);
}

/*===========================================================================*
 *				rl_report_link				     *
 *===========================================================================*/
static void rl_report_link(rep)
re_t *rep;
{
#if VERBOSE
	port_t port;
	u8_t mii_status;

	port = rep->re_base_port;

	mii_status = rl_inb(port, RL_PHYSTAT);

	if (mii_status & RL_STAT_LINK) {
		rep->re_link_up = 1;
		printf("%s: link up at ", rep->re_name);
	} else {
		rep->re_link_up = 0;
		printf("%s: link down\n", rep->re_name);
		return;
	}

	if (mii_status & RL_STAT_1000)
		printf("1000 Mbps");
	else if (mii_status & RL_STAT_100)
		printf("100 Mbps");
	else if (mii_status & RL_STAT_10)
		printf("10 Mbps");

	if (mii_status & RL_STAT_FULLDUP)
		printf(", full duplex");
	else
		printf(", half duplex");
	printf("\n");
#endif

	dump_phy(rep);
}

/*===========================================================================*
 *				rl_do_reset				     *
 *===========================================================================*/
static void rl_do_reset(rep)
re_t *rep;
{
	rep->re_need_reset = FALSE;
	rl_reset_hw(rep);
	rl_rec_mode(rep);

	rep->re_tx_head = 0;
	if (rep->re_flags & REF_SEND_AVAIL) {
		rep->re_tx[rep->re_tx_head].ret_busy = FALSE;
		rep->re_send_int = TRUE;
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

	rep = &re_state;

	assert(rep->re_mode == REM_ENABLED);
	assert(rep->re_flags & REF_ENABLED);

	stats = rep->re_stat;

	r = sys_safecopyto(mp->m_source, mp->m_net_netdrv_dl_getstat_s.grant,
		0, (vir_bytes) &stats, sizeof(stats));
	if (r != OK)
		panic("rl_getstat_s: sys_safecopyto failed: %d", r);

	mp->m_type = DL_STAT_REPLY;
	r = ipc_send(mp->m_source, mp);
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

	r = ipc_send(rep->re_client, &reply);

	if (r < 0) {
		printf("RTL8169 tried sending to %d, type %d\n",
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

static void dump_phy(const re_t *rep)
{
#if VERBOSE
	port_t port;
	u32_t t;

	port = rep->re_base_port;

	t = rl_inb(port, RL_CONFIG0);
	printf("CONFIG0\t\t:");
	t = t & RL_CFG0_ROM;
	if (t == RL_CFG0_ROM128K)
		printf(" 128K Boot ROM");
	else if (t == RL_CFG0_ROM64K)
		printf(" 64K Boot ROM");
	else if (t == RL_CFG0_ROM32K)
		printf(" 32K Boot ROM");
	else if (t == RL_CFG0_ROM16K)
		printf(" 16K Boot ROM");
	else if (t == RL_CFG0_ROM8K)
		printf(" 8K Boot ROM");
	else if (t == RL_CFG0_ROMNO)
		printf(" No Boot ROM");
	printf("\n");

	t = rl_inb(port, RL_CONFIG1);
	printf("CONFIG1\t\t:");
	if (t & RL_CFG1_LEDS1)
		printf(" LED1");
	if (t & RL_CFG1_LEDS0)
		printf(" LED0");
	if (t & RL_CFG1_DVRLOAD)
		printf(" Driver");
	if (t & RL_CFG1_LWACT)
		printf(" LWAKE");
	if (t & RL_CFG1_IOMAP)
		printf(" IOMAP");
	if (t & RL_CFG1_MEMMAP)
		printf(" MEMMAP");
	if (t & RL_CFG1_VPD)
		printf(" VPD");
	if (t & RL_CFG1_PME)
		printf(" PME");
	printf("\n");

	t = rl_inb(port, RL_CONFIG2);
	printf("CONFIG2\t\t:");
	if (t & RL_CFG2_AUX)
		printf(" AUX");
	if (t & RL_CFG2_PCIBW)
		printf(" PCI-64-Bit");
	else
		printf(" PCI-32-Bit");
	t = t & RL_CFG2_PCICLK;
	if (t == RL_CFG2_66MHZ)
		printf(" 66 MHz");
	else if (t == RL_CFG2_33MHZ)
		printf(" 33 MHz");
	printf("\n");

	t = mdio_read(port, MII_CTRL);
	printf("MII_CTRL\t:");
	if (t & MII_CTRL_RST)
		printf(" Reset");
	if (t & MII_CTRL_LB)
		printf(" Loopback");
	if (t & MII_CTRL_ANE)
		printf(" ANE");
	if (t & MII_CTRL_PD)
		printf(" Power-down");
	if (t & MII_CTRL_ISO)
		printf(" Isolate");
	if (t & MII_CTRL_RAN)
		printf(" RAN");
	if (t & MII_CTRL_DM)
		printf(" Full-duplex");
	if (t & MII_CTRL_CT)
		printf(" COL-signal");
	t = t & (MII_CTRL_SP_LSB | MII_CTRL_SP_MSB);
	if (t == MII_CTRL_SP_10)
		printf(" 10 Mb/s");
	else if (t == MII_CTRL_SP_100)
		printf(" 100 Mb/s");
	else if (t == MII_CTRL_SP_1000)
		printf(" 1000 Mb/s");
	printf("\n");

	t = mdio_read(port, MII_STATUS);
	printf("MII_STATUS\t:");
	if (t & MII_STATUS_100T4)
		printf(" 100Base-T4");
	if (t & MII_STATUS_100XFD)
		printf(" 100BaseX-FD");
	if (t & MII_STATUS_100XHD)
		printf(" 100BaseX-HD");
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
		printf(" res-0x%x", t & MII_STATUS_RES);
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

	t = mdio_read(port, MII_ANA);
	printf("MII_ANA\t\t: 0x%04x\n", t);

	t = mdio_read(port, MII_ANLPA);
	printf("MII_ANLPA\t: 0x%04x\n", t);

	t = mdio_read(port, MII_ANE);
	printf("MII_ANE\t\t:");
	if (t & MII_ANE_RES)
		printf(" res-0x%x", t & MII_ANE_RES);
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

	t = mdio_read(port, MII_1000_CTRL);
	printf("MII_1000_CTRL\t:");
	if (t & MII_1000C_FULL)
		printf(" 1000BaseT-FD");
	if (t & MII_1000C_HALF)
		printf(" 1000BaseT-HD");
	printf("\n");

	t = mdio_read(port, MII_1000_STATUS);
	if (t) {
		printf("MII_1000_STATUS\t:");
		if (t & MII_1000S_LRXOK)
			printf(" Local-Receiver");
		if (t & MII_1000S_RRXOK)
			printf(" Remote-Receiver");
		if (t & MII_1000S_HALF)
			printf(" 1000BaseT-HD");
		if (t & MII_1000S_FULL)
			printf(" 1000BaseT-FD");
		printf("\n");

		t = mdio_read(port, MII_EXT_STATUS);
		printf("MII_EXT_STATUS\t:");
		if (t & MII_ESTAT_1000XFD)
			printf(" 1000BaseX-FD");
		if (t & MII_ESTAT_1000XHD)
			printf(" 1000BaseX-HD");
		if (t & MII_ESTAT_1000TFD)
			printf(" 1000BaseT-FD");
		if (t & MII_ESTAT_1000THD)
			printf(" 1000BaseT-HD");
		printf("\n");
	}
#endif
}

static void do_hard_int(void)
{
	int s;

	/* Run interrupt handler at driver level. */
	rl_handler(&re_state);

	/* Reenable interrupts for this hook. */
	if ((s = sys_irqenable(&re_state.re_hook_id)) != OK)
		printf("RTL8169: error, couldn't enable interrupts: %d\n", s);
}

/*===========================================================================*
 *				rl_handler				     *
 *===========================================================================*/
static void rl_handler(re_t *rep)
{
	int i, port, tx_head, tx_tail, link_up;
	u16_t isr;
	re_desc *desc;
	int_event_check = FALSE;	/* disable check by default */

	port = rep->re_base_port;

	/* Ack interrupt */
	isr = rl_inw(port, RL_ISR);
	if(!isr)
		return;
	rl_outw(port, RL_ISR, isr);
	rep->interrupts++;

	if (isr & RL_IMR_FOVW) {
		isr &= ~RL_IMR_FOVW;
		/* Should do anything? */

		rep->re_stat.ets_fifoOver++;
	}
	if (isr & RL_IMR_PUN) {
		isr &= ~RL_IMR_PUN;

		/*
		 * Either the link status changed or there was a TX fifo
		 * underrun.
		 */
		link_up = !(!(rl_inb(port, RL_PHYSTAT) & RL_STAT_LINK));
		if (link_up != rep->re_link_up) {
			rep->re_report_link = TRUE;
			rep->re_got_int = TRUE;
			int_event_check = TRUE;
		}
	}

	if (isr & (RL_ISR_RDU | RL_ISR_RER | RL_ISR_ROK)) {
		if (isr & RL_ISR_RER)
			rep->re_stat.ets_recvErr++;
		isr &= ~(RL_ISR_RDU | RL_ISR_RER | RL_ISR_ROK);

		if (!rep->re_got_int && (rep->re_flags & REF_READING)) {
			rep->re_got_int = TRUE;
			int_event_check = TRUE;
		}
	}

	if ((isr & (RL_ISR_TDU | RL_ISR_TER | RL_ISR_TOK)) || 1) {
		if (isr & RL_ISR_TER)
			rep->re_stat.ets_sendErr++;
		isr &= ~(RL_ISR_TDU | RL_ISR_TER | RL_ISR_TOK);

		/* Transmit completed */
		tx_head = rep->re_tx_head;
		tx_tail = tx_head+1;
		if (tx_tail >= N_TX_DESC)
			tx_tail = 0;
		for (i = 0; i < 2 * N_TX_DESC; i++) {
			if (!rep->re_tx[tx_tail].ret_busy) {
				/* Strange, this buffer is not in-use.
				 * Increment tx_tail until tx_head is
				 * reached (or until we find a buffer that
				 * is in-use.
				 */
				if (tx_tail == tx_head)
					break;
				if (++tx_tail >= N_TX_DESC)
					tx_tail = 0;
				assert(tx_tail < N_TX_DESC);
				continue;
			}
			desc = rep->re_tx_desc;
			desc += tx_tail;
			if (desc->status & DESC_OWN) {
				/* Buffer is not yet ready */
				break;
			}

			rep->re_stat.ets_packetT++;
			rep->re_tx[tx_tail].ret_busy = FALSE;

			if (++tx_tail >= N_TX_DESC)
				tx_tail = 0;
			assert(tx_tail < N_TX_DESC);

			if (rep->re_flags & REF_SEND_AVAIL) {
				rep->re_send_int = TRUE;
				if (!rep->re_got_int) {
					rep->re_got_int = TRUE;
					int_event_check = TRUE;
				}
			}
		}
		assert(i < 2 * N_TX_DESC);
	}

	/* Ignore Reserved Interrupt */
	if (isr & RL_ISR_RES)
		isr &= ~RL_ISR_RES;

	if (isr)
		printf("rl_handler: unhandled interrupt isr = 0x%04x\n", isr);
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

	rep = &re_state;

	if (rep->re_mode != REM_ENABLED)
		return;

	/* Should collect statistics */
	if (!(++rep->dtcc_counter % RE_DTCC_VALUE))
		rtl8169_update_stat(rep);

	if (!(rep->re_flags & REF_SEND_AVAIL)) {
	/* Assume that an idle system is alive */
	rep->re_tx_alive = TRUE;
		return;
	}
	if (rep->re_tx_alive) {
		rep->re_tx_alive = FALSE;
		return;
	}
	printf("rl_watchdog_f: resetting instance %d mode 0x%x flags 0x%x\n",
		re_instance, rep->re_mode, rep->re_flags);
	printf("tx_head    :%8d  busy %d\t",
		rep->re_tx_head, rep->re_tx[rep->re_tx_head].ret_busy);
	rep->re_need_reset = TRUE;
	rep->re_got_int = TRUE;

	check_int_events();
}

