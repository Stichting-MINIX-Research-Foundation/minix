/*
 * fxp.c
 *
 * This file contains an ethernet device driver for Intel 82557, 82558, 
 * 82559, 82550, and 82562 fast ethernet controllers.
 *
 * Created:	Nov 2004 by Philip Homburg <philip@f-mnx.phicoh.com>
 */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <stdlib.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <machine/pci.h>
#include <minix/ds.h>
#include <minix/endpoint.h>

#include <minix/timers.h>

#define debug			0
#define RAND_UPDATE		/**/
#define printW()		((void)0)

#include "assert.h"
#include "fxp.h"
#include "mii.h"

/* Number of receive buffers */
#define N_RX_BUF	40

/* Number of transmit buffers */
#define N_TX_BUF	4

/* I/O vectors are handled IOVEC_NR entries at a time. */
#define IOVEC_NR	16

/* Configuration */
#define FXP_ENVVAR	"FXPETH"

typedef int irq_hook_t;

/* ignore interrupt for the moment */
#define interrupt(x)	do { } while(0)

static union tmpbuf
{
	char pad[4096];
	struct cbl_conf cc;
	struct ias ias;
} *tmpbufp;

typedef struct fxp
{
	port_t fxp_base_port;
	int fxp_mode;
	int fxp_got_int;
	int fxp_send_int;
	int fxp_flags;
	int fxp_client;
	int fxp_features;		/* Needed? */
	int fxp_irq;
	int fxp_type;			/* What kind of hardware */
	int fxp_ms_regs;		/* Master/slave registers */
	int fxp_ee_addrlen;		/* #EEPROM address bits */
	int fxp_tx_alive;
	int fxp_need_reset;

	/* Rx */
	vir_bytes fxp_read_s;
	int fxp_rx_nbuf;
	int fxp_rx_bufsize;
	struct rfd *fxp_rx_buf;
	phys_bytes fxp_rx_busaddr;
	int fxp_rx_head;
	int fxp_rx_need_restart;
	int fxp_need_conf;		/* Re-configure after draining send
					 * queue
					 */

	/* Tx */
	int fxp_tx_nbuf;
	int fxp_tx_bufsize;
	struct tx *fxp_tx_buf;
	phys_bytes fxp_tx_busaddr;
	int fxp_tx_idle;
	int fxp_tx_head;
	int fxp_tx_tail;
	int fxp_tx_threshold;

	/* Link status */
	int fxp_report_link;
	int fxp_link_up;
	int fxp_mii_busy;
	u16_t fxp_mii_scr;

	/* PCI related */
	int fxp_seen;			/* TRUE iff device available */

	/* 'large' items */
	irq_hook_t fxp_hook;
	ether_addr_t fxp_address;
	message fxp_rx_mess;
	message fxp_tx_mess;
	struct sc fxp_stat;
	u8_t fxp_conf_bytes[CC_BYTES_NR];
	char fxp_name[sizeof("fxp#n")];
	iovec_t fxp_iovec[IOVEC_NR];
	iovec_s_t fxp_iovec_s[IOVEC_NR];
}
fxp_t;

/* fxp_mode */
#define FM_DISABLED	0x0
#define FM_ENABLED	0x1

/* fxp_flags */
#define FF_EMPTY	0x000
#define FF_PACK_SENT	0x001
#define FF_PACK_RECV	0x002
#define FF_SEND_AVAIL	0x004
#define FF_READING	0x010
#define FF_PROMISC	0x040
#define FF_MULTI	0x080
#define FF_BROAD	0x100
#define FF_ENABLED	0x200

/* fxp_features */
#define FFE_NONE	0x0

/* fxp_type */
#define FT_UNKNOWN	0x0
#define FT_82557	0x1
#define FT_82558A	0x2
#define FT_82559	0x4
#define FT_82801	0x8

static int fxp_instance;

static fxp_t *fxp_state;

static minix_timer_t fxp_watchdog;

static u32_t system_hz;

#define fxp_inb(port, offset)	(do_inb((port) + (offset)))
#define fxp_inl(port, offset)	(do_inl((port) + (offset)))
#define fxp_outb(port, offset, value)	(do_outb((port) + (offset), (value)))
#define fxp_outl(port, offset, value)	(do_outl((port) + (offset), (value)))

static void fxp_init(message *mp);
static void fxp_pci_conf(void);
static int fxp_probe(fxp_t *fp, int skip);
static void fxp_conf_hw(fxp_t *fp);
static void fxp_init_hw(fxp_t *fp);
static void fxp_init_buf(fxp_t *fp);
static void fxp_reset_hw(fxp_t *fp);
static void fxp_confaddr(fxp_t *fp);
static void fxp_rec_mode(fxp_t *fp);
static void fxp_writev_s(const message *mp, int from_int);
static void fxp_readv_s(message *mp, int from_int);
static void fxp_do_conf(fxp_t *fp);
static void fxp_cu_ptr_cmd(fxp_t *fp, int cmd, phys_bytes bus_addr, int
	check_idle);
static void fxp_ru_ptr_cmd(fxp_t *fp, int cmd, phys_bytes bus_addr, int
	check_idle);
static void fxp_restart_ru(fxp_t *fp);
static void fxp_getstat_s(message *mp);
static void fxp_handler(fxp_t *fp);
static void fxp_check_ints(fxp_t *fp);
static void fxp_watchdog_f(minix_timer_t *tp);
static int fxp_link_changed(fxp_t *fp);
static void fxp_report_link(fxp_t *fp);
static void reply(fxp_t *fp);
static void mess_reply(message *req, message *reply);
static u16_t eeprom_read(fxp_t *fp, int reg);
static void eeprom_addrsize(fxp_t *fp);
static u16_t mii_read(fxp_t *fp, int reg);
static u8_t do_inb(port_t port);
static u32_t do_inl(port_t port);
static void do_outb(port_t port, u8_t v);
static void do_outl(port_t port, u32_t v);
static void tell_dev(vir_bytes start, size_t size, int pci_bus, int
	pci_dev, int pci_func);

static void handle_hw_intr(void)
{
	int r;
	fxp_t *fp;

	fp= fxp_state;

	if (fp->fxp_mode != FM_ENABLED)
		return;
	fxp_handler(fp);

	r= sys_irqenable(&fp->fxp_hook);
	if (r != OK) {
		panic("unable enable interrupts: %d", r);
	}

	if (!fp->fxp_got_int)
		return;
	fp->fxp_got_int= 0;
	assert(fp->fxp_flags & FF_ENABLED);
	fxp_check_ints(fp);
}

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

/*===========================================================================*
 *				main					     *
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
			panic("netdriver_receive failed: %d", r);

		if (is_ipc_notify(ipc_status)) {
			switch (_ENDPOINT_P(m.m_source)) {
				case HARDWARE:
					handle_hw_intr();
					break;
				case CLOCK:
					expire_timers(m.m_notify.timestamp);
					break;
				default:
					panic(" illegal notify from: %d", m.m_source);
			}

			/* get new message */
			continue;
		}

		switch (m.m_type)
		{
		case DL_WRITEV_S: fxp_writev_s(&m, FALSE);	break;
		case DL_READV_S: fxp_readv_s(&m, FALSE);	break;
		case DL_CONF:	fxp_init(&m);			break;
		case DL_GETSTAT_S: fxp_getstat_s(&m);		break;
		default:
			panic(" illegal message: %d", m.m_type);
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
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the fxp driver. */
	long v;
	int r;
	vir_bytes ft;

	system_hz = sys_hz();

	v = 0;
	(void) env_parse("instance", "d", 0, &v, 0, 255);
	fxp_instance = (int) v;

	ft = sizeof(*fxp_state);

	if(!(fxp_state = alloc_contig(ft, 0, NULL)))
		panic("couldn't allocate table: %d", ENOMEM);

	memset(fxp_state, 0, ft);

	if((r=tsc_calibrate()) != OK)
		panic("tsc_calibrate failed: %d", r);

	/* Announce we are up! */
	netdriver_announce();

	return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
	port_t port;
	fxp_t *fp;

	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM) return;

	fp= fxp_state;

	if (fp->fxp_mode == FM_ENABLED && (fp->fxp_flags & FF_ENABLED)) {
		port= fp->fxp_base_port;

		/* Reset device */
		if (debug)
			printf("%s: resetting device\n", fp->fxp_name);
		fxp_outl(port, CSR_PORT, CP_CMD_SOFT_RESET);
	}

	exit(0);
}

/*===========================================================================*
 *				fxp_init				     *
 *===========================================================================*/
static void fxp_init(mp)
message *mp;
{
	static int first_time= 1;

	fxp_t *fp;
	message reply_mess;

	if (first_time)
	{
		first_time= 0;
		fxp_pci_conf(); /* Configure PCI devices. */

		init_timer(&fxp_watchdog);
		set_timer(&fxp_watchdog, system_hz, fxp_watchdog_f, 0);
	}

	fp= fxp_state;
	if (fp->fxp_mode == FM_DISABLED)
	{
		/* This is the default, try to (re)locate the device. */
		fxp_conf_hw(fp);
		if (fp->fxp_mode == FM_DISABLED)
		{
			/* Probe failed, or the device is configured off. */
			reply_mess.m_type= DL_CONF_REPLY;
			reply_mess.m_netdrv_net_dl_conf.stat= ENXIO;
			mess_reply(mp, &reply_mess);
			return;
		}
		if (fp->fxp_mode == FM_ENABLED)
			fxp_init_hw(fp);
		fxp_report_link(fp);
	}

	assert(fp->fxp_mode == FM_ENABLED);
	assert(fp->fxp_flags & FF_ENABLED);

	fp->fxp_flags &= ~(FF_PROMISC | FF_MULTI | FF_BROAD);

	if (mp->m_net_netdrv_dl_conf.mode & DL_PROMISC_REQ)
		fp->fxp_flags |= FF_PROMISC;
	if (mp->m_net_netdrv_dl_conf.mode & DL_MULTI_REQ)
		fp->fxp_flags |= FF_MULTI;
	if (mp->m_net_netdrv_dl_conf.mode & DL_BROAD_REQ)
		fp->fxp_flags |= FF_BROAD;

	fxp_rec_mode(fp);

	reply_mess.m_type = DL_CONF_REPLY;
	reply_mess.m_netdrv_net_dl_conf.stat = OK;
	memcpy(reply_mess.m_netdrv_net_dl_conf.hw_addr,
		fp->fxp_address.ea_addr,
		sizeof(reply_mess.m_netdrv_net_dl_conf.hw_addr));

	mess_reply(mp, &reply_mess);
}

/*===========================================================================*
 *				fxp_pci_conf				     *
 *===========================================================================*/
static void fxp_pci_conf()
{
	fxp_t *fp;

	fp= fxp_state;

	strlcpy(fp->fxp_name, "fxp#0", sizeof(fp->fxp_name));
	fp->fxp_name[4] += fxp_instance;
	fp->fxp_seen= FALSE;
	fp->fxp_features= FFE_NONE;

	pci_init();

	if (fxp_probe(fp, fxp_instance))
		fp->fxp_seen= TRUE;
}

/*===========================================================================*
 *				fxp_probe				     *
 *===========================================================================*/
static int fxp_probe(fxp_t *fp, int skip)
{
	int r, devind;
	u16_t vid, did;
	u32_t bar;
	u8_t ilr, rev;
	char *str;
#if VERBOSE
	char *dname;
#endif

	r= pci_first_dev(&devind, &vid, &did);
	if (r == 0)
		return FALSE;

	while (skip--)
	{
		r= pci_next_dev(&devind, &vid, &did);
		if (!r)
			return FALSE;
	}

#if VERBOSE
	dname= pci_dev_name(vid, did);
	if (!dname)
		dname= "unknown device";
	printf("%s: %s (%04x/%04x) at %s\n",
		fp->fxp_name, dname, vid, did, pci_slot_name(devind));
#endif
	pci_reserve(devind);

	bar= pci_attr_r32(devind, PCI_BAR_2) & 0xffffffe0;
	if (bar < 0x400) {
		panic("fxp_probe: base address is not properly configured");
	}
	fp->fxp_base_port= bar;

	ilr= pci_attr_r8(devind, PCI_ILR);
	fp->fxp_irq= ilr;
	if (debug)
	{
		printf("%s: using I/O address 0x%lx, IRQ %d\n",
			fp->fxp_name, (unsigned long)bar, ilr);
	}

	rev= pci_attr_r8(devind, PCI_REV);
	str= NULL;
	fp->fxp_type= FT_UNKNOWN;
	switch(rev)
	{
	case FXP_REV_82557A:	str= "82557A";			/* 0x01 */
				fp->fxp_type= FT_82557;
				break;
	case FXP_REV_82557B:	str= "82557B"; break;		/* 0x02 */
	case FXP_REV_82557C:	str= "82557C"; break;		/* 0x03 */
	case FXP_REV_82558A:	str= "82558A"; 			/* 0x04 */
				fp->fxp_type= FT_82558A;
				break;
	case FXP_REV_82558B:	str= "82558B"; 			/* 0x05 */
				fp->fxp_type= FT_82559;
				break;
	case FXP_REV_82559A:	str= "82559A"; break;		/* 0x06 */
	case FXP_REV_82559B:	str= "82559B"; break;		/* 0x07 */
	case FXP_REV_82559C:	str= "82559C";			/* 0x08 */
				fp->fxp_type= FT_82559;
				break;
	case FXP_REV_82559ERA:	str= "82559ER-A"; 		/* 0x09 */
				fp->fxp_type= FT_82559;
				break;
	case FXP_REV_82550_1:	str= "82550(1)"; 		/* 0x0C */
				fp->fxp_type= FT_82559;
				break;
	case FXP_REV_82550_2:	str= "82550(2)"; 		/* 0x0D */
				fp->fxp_type= FT_82559;
				break;
	case FXP_REV_82550_3:	str= "82550(3)"; 		/* 0x0E */
				fp->fxp_type= FT_82559;
				break;
	case FXP_REV_82551_1:	str= "82551(1)"; 		/* 0x0F */
				fp->fxp_type= FT_82559;
				break;
	case FXP_REV_82551_2:	str= "82551(2)"; 		/* 0x10 */
				fp->fxp_type= FT_82559;
				break;
	case FXP_REV_82801CAM:	str= "82801CAM"; 		/* 0x42 */
				fp->fxp_type= FT_82801;
				break;
	case FXP_REV_82801DB:	str= "82801DB"; 		/* 0x81 */
				fp->fxp_type= FT_82801;
				break;
	case FXP_REV_82550_4:	str= "82550(4)"; 		/* 0x83 */
				fp->fxp_type= FT_82559;
				break;
	}

#if VERBOSE
	if (str)
		printf("%s: device revision: %s\n", fp->fxp_name, str);
	else
		printf("%s: unknown revision: 0x%x\n", fp->fxp_name, rev);
#endif

	if (fp->fxp_type == FT_UNKNOWN)
	{
		printf("fxp_probe: device is not supported by this driver\n");
		return FALSE;
	}

	return TRUE;
}

/*===========================================================================*
 *				fxp_conf_hw				     *
 *===========================================================================*/
static void fxp_conf_hw(fxp_t *fp)
{
#if VERBOSE
	int i;
#endif

	fp->fxp_mode= FM_DISABLED;	/* Superfluous */

	if (!fp->fxp_seen)
		return;

	/* PCI device is present */
	fp->fxp_mode= FM_ENABLED;

	fp->fxp_flags= FF_EMPTY;
	fp->fxp_got_int= 0;
	fp->fxp_send_int= 0;
	fp->fxp_ee_addrlen= 0;	/* Unknown */
	fp->fxp_need_reset= 0;
	fp->fxp_report_link= 0;
	fp->fxp_link_up= -1;	/* Unknown */
	fp->fxp_mii_busy= 0;
	fp->fxp_read_s= 0;
	fp->fxp_rx_need_restart= 0;
	fp->fxp_need_conf= 0;
	fp->fxp_tx_head= 0;
	fp->fxp_tx_tail= 0;
	fp->fxp_tx_alive= 0;
	fp->fxp_tx_threshold= TXTT_MIN;

	/* Try to come up with a sensible configuration for the current
	 * device. Unfortunately every device is different, defaults are
	 * not always zero, and some fields are re-used with a completely
	 * different interpretation. We start out with a sensible default
	 * for all devices and then add device specific changes.
	 */
	fp->fxp_conf_bytes[0]= CC_BYTES_NR;
	fp->fxp_conf_bytes[1]= CTL_DEFAULT | CRL_DEFAULT;
	fp->fxp_conf_bytes[2]= CAI_DEFAULT;
	fp->fxp_conf_bytes[3]= 0;
	fp->fxp_conf_bytes[4]= 0;
	fp->fxp_conf_bytes[5]= 0;
	fp->fxp_conf_bytes[6]= CCB6_ESC | CCB6_ETCB | CCB6_RES;
	fp->fxp_conf_bytes[7]= CUR_1;
	fp->fxp_conf_bytes[8]= CCB8_503_MII;
	fp->fxp_conf_bytes[9]= 0;
	fp->fxp_conf_bytes[10]= CLB_NORMAL | CPAL_DEFAULT | CCB10_NSAI |
				CCB10_RES1;
	fp->fxp_conf_bytes[11]= 0;
	fp->fxp_conf_bytes[12]= CIS_DEFAULT;
	fp->fxp_conf_bytes[13]= CCB13_DEFAULT;
	fp->fxp_conf_bytes[14]= CCB14_DEFAULT;
	fp->fxp_conf_bytes[15]= CCB15_RES1 | CCB15_RES2;
	fp->fxp_conf_bytes[16]= CCB16_DEFAULT;
	fp->fxp_conf_bytes[17]= CCB17_DEFAULT;
	fp->fxp_conf_bytes[18]= CCB18_RES1 | CCB18_PFCT | CCB18_PE;
	fp->fxp_conf_bytes[19]= CCB19_FDPE;
	fp->fxp_conf_bytes[20]= CCB20_PFCL | CCB20_RES1;
	fp->fxp_conf_bytes[21]= CCB21_RES21;

#if VERBOSE
	for (i= 0; i<CC_BYTES_NR; i++)
		printf("%d: %0x, ", i, fp->fxp_conf_bytes[i]);
	printf("\n");
#endif

	switch(fp->fxp_type)
	{
	case FT_82557:
		break;
	case FT_82558A:
	case FT_82559:
	case FT_82801:
		fp->fxp_conf_bytes[18] |= CCB18_LROK;

		if (fp->fxp_type == FT_82801)
		{
			fp->fxp_conf_bytes[6] = 0xba; /* ctrl 1 */
			fp->fxp_conf_bytes[15] = 0x48; /* promiscuous */
			fp->fxp_conf_bytes[21] = 0x05; /* mc_all */
		}
		break;
	default:
		panic("fxp_conf_hw: bad device type: %d", fp->fxp_type);
	}

	/* Assume an 82555 (compatible) PHY. The should be changed for
	 * 82557 NICs with different PHYs
	 */
	fp->fxp_ms_regs = 0;	/* No master/slave registers. */

#if VERBOSE
	for (i= 0; i<CC_BYTES_NR; i++)
		printf("%d: %0x, ", i, fp->fxp_conf_bytes[i]);
	printf("\n");
#endif
}

/*===========================================================================*
 *				fxp_init_hw				     *
 *===========================================================================*/
static void fxp_init_hw(fp)
fxp_t *fp;
{
	int i, r, isr;
	port_t port;
	phys_bytes bus_addr;

	port= fp->fxp_base_port;

	fxp_init_buf(fp);

	fp->fxp_flags = FF_EMPTY;
	fp->fxp_flags |= FF_ENABLED;

	/* Set the interrupt handler and policy. Do not automatically 
	 * reenable interrupts. Return the IRQ line number on interrupts.
 	 */
 	fp->fxp_hook = fp->fxp_irq;
	r= sys_irqsetpolicy(fp->fxp_irq, 0, &fp->fxp_hook);
	if (r != OK)
		panic("sys_irqsetpolicy failed: %d", r);

	fxp_reset_hw(fp);

	r= sys_irqenable(&fp->fxp_hook);
	if (r != OK)
		panic("sys_irqenable failed: %d", r);

	/* Reset PHY? */

	fxp_do_conf(fp);

	/* Set pointer to statistical counters */
	r= sys_umap(SELF, VM_D, (vir_bytes)&fp->fxp_stat, sizeof(fp->fxp_stat),
		&bus_addr);
	if (r != OK)
		panic("sys_umap failed: %d", r);
	fxp_cu_ptr_cmd(fp, SC_CU_LOAD_DCA, bus_addr, TRUE /* check idle */);

	/* Ack previous interrupts */
	isr= fxp_inb(port, SCB_INT_STAT);
	fxp_outb(port, SCB_INT_STAT, isr);

	/* Enable interrupts */
	fxp_outb(port, SCB_INT_MASK, 0);

	fxp_ru_ptr_cmd(fp, SC_RU_START, fp->fxp_rx_busaddr,
		TRUE /* check idle */);

	fxp_confaddr(fp);
	if (debug)
	{
		printf("%s: Ethernet address ", fp->fxp_name);
		for (i= 0; i < 6; i++)
		{
			printf("%x%c", fp->fxp_address.ea_addr[i],
				i < 5 ? ':' : '\n');
		}
	}
}

/*===========================================================================*
 *				fxp_init_buf				     *
 *===========================================================================*/
static void fxp_init_buf(fp)
fxp_t *fp;
{
	size_t rx_totbufsize, tx_totbufsize, tot_bufsize, alloc_bufsize;
	char *alloc_buf;
	phys_bytes buf, bus_addr;
	int i, r;
	struct rfd *rfdp;
	struct tx *txp;
	phys_bytes ph;

	fp->fxp_rx_nbuf= N_RX_BUF;
	rx_totbufsize= fp->fxp_rx_nbuf * sizeof(struct rfd);
	fp->fxp_rx_bufsize= rx_totbufsize;

	fp->fxp_tx_nbuf= N_TX_BUF;
	tx_totbufsize= fp->fxp_tx_nbuf * sizeof(struct tx);
	fp->fxp_tx_bufsize= tx_totbufsize;

	tot_bufsize= sizeof(*tmpbufp) + tx_totbufsize + rx_totbufsize;
	if (tot_bufsize % 4096)
		tot_bufsize += 4096 - (tot_bufsize % 4096);
	alloc_bufsize= tot_bufsize;
	alloc_buf= alloc_contig(alloc_bufsize, AC_ALIGN4K, &ph);
	if (alloc_buf == NULL) {
		panic("fxp_init_buf: unable to alloc_contig size: %d", 			alloc_bufsize);
	}

	buf= (phys_bytes)alloc_buf;

	tell_dev((vir_bytes)buf, tot_bufsize, 0, 0, 0);

	tmpbufp= (union tmpbuf *)buf;

	fp->fxp_rx_buf= (struct rfd *)&tmpbufp[1];
	r= sys_umap(SELF, VM_D, (vir_bytes)fp->fxp_rx_buf, rx_totbufsize,
		&bus_addr);
	if (r != OK)
		panic("sys_umap failed: %d", r);
	fp->fxp_rx_busaddr= bus_addr;

#if 0
	printf("fxp_init_buf: got phys 0x%x for vir 0x%x\n",
		fp->fxp_rx_busaddr, fp->fxp_rx_buf);
#endif

	for (i= 0, rfdp= fp->fxp_rx_buf; i<fp->fxp_rx_nbuf; i++, rfdp++)
	{
		rfdp->rfd_status= 0;
		rfdp->rfd_command= 0;
		if (i != fp->fxp_rx_nbuf-1)
		{
			r= sys_umap(SELF, VM_D, (vir_bytes)&rfdp[1],
				sizeof(rfdp[1]), &bus_addr);
			if (r != OK)
				panic("sys_umap failed: %d", r);
			rfdp->rfd_linkaddr= bus_addr;
		}
		else
		{
			rfdp->rfd_linkaddr= fp->fxp_rx_busaddr;
			rfdp->rfd_command |= RFDC_EL;
		}
		rfdp->rfd_reserved= 0;
		rfdp->rfd_res= 0;
		rfdp->rfd_size= sizeof(rfdp->rfd_buf);

	}
	fp->fxp_rx_head= 0;

	fp->fxp_tx_buf= (struct tx *)((char *)fp->fxp_rx_buf+rx_totbufsize);
	r= sys_umap(SELF, VM_D, (vir_bytes)fp->fxp_tx_buf,
		(phys_bytes)tx_totbufsize, &fp->fxp_tx_busaddr);
	if (r != OK)
		panic("sys_umap failed: %d", r);

	for (i= 0, txp= fp->fxp_tx_buf; i<fp->fxp_tx_nbuf; i++, txp++)
	{
		txp->tx_status= 0;
		txp->tx_command= TXC_EL | CBL_NOP;	/* Just in case */
		if (i != fp->fxp_tx_nbuf-1)
		{
			r= sys_umap(SELF, VM_D, (vir_bytes)&txp[1],
				(phys_bytes)sizeof(txp[1]), &bus_addr);
			if (r != OK)
				panic("sys_umap failed: %d", r);
			txp->tx_linkaddr= bus_addr;
		}
		else
		{
			txp->tx_linkaddr= fp->fxp_tx_busaddr;
		}
		txp->tx_tbda= TX_TBDA_NIL;
		txp->tx_size= 0;
		txp->tx_tthresh= fp->fxp_tx_threshold;
		txp->tx_ntbd= 0;
	}
	fp->fxp_tx_idle= 1;
}

/*===========================================================================*
 *				fxp_reset_hw				     *
 *===========================================================================*/
static void fxp_reset_hw(fp)
fxp_t *fp;
{
/* Inline the function in init? */
	port_t port;

	port= fp->fxp_base_port;

	/* Reset device */
	fxp_outl(port, CSR_PORT, CP_CMD_SOFT_RESET);
	tickdelay(micros_to_ticks(CSR_PORT_RESET_DELAY));

	/* Disable interrupts */
	fxp_outb(port, SCB_INT_MASK, SIM_M);

	/* Set CU base to zero */
	fxp_cu_ptr_cmd(fp, SC_CU_LOAD_BASE, 0, TRUE /* check idle */);

	/* Set RU base to zero */
	fxp_ru_ptr_cmd(fp, SC_RU_LOAD_BASE, 0, TRUE /* check idle */);
}

/*===========================================================================*
 *				fxp_confaddr				     *
 *===========================================================================*/
static void fxp_confaddr(fxp_t *fp)
{
	static char eakey[]= FXP_ENVVAR "#_EA";
	static char eafmt[]= "x:x:x:x:x:x";
	int i, r;
	phys_bytes bus_addr;
	long v;

	/* User defined ethernet address? */
	eakey[sizeof(FXP_ENVVAR)-1]= '0' + fxp_instance;

	for (i= 0; i < 6; i++)
	{
		if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
			break;
		fp->fxp_address.ea_addr[i]= v;
	}

	if (i != 0 && i != 6) env_panic(eakey);	/* It's all or nothing */

	if (i == 0)
	{
		/* Get ethernet address from EEPROM */
		for (i= 0; i<3; i++)
		{
			v= eeprom_read(fp, i);
			fp->fxp_address.ea_addr[i*2]= (v & 0xff);
			fp->fxp_address.ea_addr[i*2+1]= ((v >> 8) & 0xff);
		}
	}

	/* Tell NIC about ethernet address */
	tmpbufp->ias.ias_status= 0;
	tmpbufp->ias.ias_command= CBL_C_EL | CBL_AIS;
	tmpbufp->ias.ias_linkaddr= 0;
	memcpy(tmpbufp->ias.ias_ethaddr, fp->fxp_address.ea_addr,
		sizeof(tmpbufp->ias.ias_ethaddr));
	r= sys_umap(SELF, VM_D, (vir_bytes)&tmpbufp->ias,
		(phys_bytes)sizeof(tmpbufp->ias), &bus_addr);
	if (r != OK)
		panic("sys_umap failed: %d", r);

	fxp_cu_ptr_cmd(fp, SC_CU_START, bus_addr, TRUE /* check idle */);

	/* Wait for CU command to complete */
	SPIN_UNTIL(tmpbufp->ias.ias_status & CBL_F_C, 1000);

	if (!(tmpbufp->ias.ias_status & CBL_F_C))
		panic("fxp_confaddr: CU command failed to complete");
	if (!(tmpbufp->ias.ias_status & CBL_F_OK))
		panic("fxp_confaddr: CU command failed");

#if VERBOSE
	printf("%s: hardware ethernet address: ", fp->fxp_name);
	for (i= 0; i<6; i++)
	{
		printf("%02x%s", fp->fxp_address.ea_addr[i], 
			i < 5 ? ":" : "");
	}
	printf("\n");
#endif
}

/*===========================================================================*
 *				fxp_rec_mode				     *
 *===========================================================================*/
static void fxp_rec_mode(fp)
fxp_t *fp;
{
	fp->fxp_conf_bytes[0]= CC_BYTES_NR;	/* Just to be sure */
	fp->fxp_conf_bytes[15] &= ~(CCB15_BD|CCB15_PM);
	fp->fxp_conf_bytes[21] &= ~CCB21_MA;

	if (fp->fxp_flags & FF_PROMISC)
		fp->fxp_conf_bytes[15] |= CCB15_PM;
	if (fp->fxp_flags & FF_MULTI)
		fp->fxp_conf_bytes[21] |= CCB21_MA;

	if (!(fp->fxp_flags & (FF_BROAD|FF_MULTI|FF_PROMISC)))
		fp->fxp_conf_bytes[15] |= CCB15_BD;

	/* Queue request if not idle */
	if (fp->fxp_tx_idle)
	{
		fxp_do_conf(fp);
	}
	else
	{
		printf("fxp_rec_mode: setting fxp_need_conf\n");
		fp->fxp_need_conf= TRUE;
	}
}

/*===========================================================================*
 *				fxp_writev_s				     *
 *===========================================================================*/
static void fxp_writev_s(const message *mp, int from_int)
{
	endpoint_t iov_endpt;
	cp_grant_id_t iov_grant;
	vir_bytes iov_offset;
	int i, j, n, o, r, s, count, size, prev_head;
	int fxp_tx_nbuf, fxp_tx_head;
	u16_t tx_command;
	fxp_t *fp;
	iovec_s_t *iovp;
	struct tx *txp, *prev_txp;

	fp= fxp_state;

	count = mp->m_net_netdrv_dl_writev_s.count;
	fp->fxp_client= mp->m_source;

	assert(fp->fxp_mode == FM_ENABLED);
	assert(fp->fxp_flags & FF_ENABLED);

	if (from_int)
	{
		assert(fp->fxp_flags & FF_SEND_AVAIL);
		fp->fxp_flags &= ~FF_SEND_AVAIL;
		fp->fxp_tx_alive= TRUE;
	}

	if (fp->fxp_tx_idle)
	{
		txp= fp->fxp_tx_buf;
		fxp_tx_head= 0;	/* lint */
		prev_txp= NULL;	/* lint */
	}
	else
	{	
		fxp_tx_nbuf= fp->fxp_tx_nbuf;
		prev_head= fp->fxp_tx_head;
		fxp_tx_head= prev_head+1;
		if (fxp_tx_head == fxp_tx_nbuf)
			fxp_tx_head= 0;
		assert(fxp_tx_head < fxp_tx_nbuf);

		if (fxp_tx_head == fp->fxp_tx_tail)
		{
			/* Send queue is full */
			assert(!(fp->fxp_flags & FF_SEND_AVAIL));
			fp->fxp_flags |= FF_SEND_AVAIL;
			goto suspend;
		}

		prev_txp= &fp->fxp_tx_buf[prev_head];
		txp= &fp->fxp_tx_buf[fxp_tx_head];
	}

	assert(!(fp->fxp_flags & FF_SEND_AVAIL));
	assert(!(fp->fxp_flags & FF_PACK_SENT));

	iov_endpt= mp->m_source;
	iov_grant= mp->m_net_netdrv_dl_writev_s.grant;

	size= 0;
	o= 0;
	iov_offset= 0;
	for (i= 0; i<count; i += IOVEC_NR,
		iov_offset += IOVEC_NR * sizeof(fp->fxp_iovec_s[0]))
	{
		n= IOVEC_NR;
		if (i+n > count)
			n= count-i;
		r= sys_safecopyfrom(iov_endpt, iov_grant, iov_offset,
			(vir_bytes)fp->fxp_iovec_s,
			n * sizeof(fp->fxp_iovec_s[0]));
		if (r != OK)
			panic("fxp_writev: sys_safecopyfrom failed: %d", r);

		for (j= 0, iovp= fp->fxp_iovec_s; j<n; j++, iovp++)
		{
			s= iovp->iov_size;
			if (size + s > ETH_MAX_PACK_SIZE_TAGGED) {
				panic("fxp_writev: invalid packet size: %d", size + s);
			}

			r= sys_safecopyfrom(iov_endpt, iovp->iov_grant,
				0, (vir_bytes)(txp->tx_buf+o), s);
			if (r != OK) {
				panic("fxp_writev_s: sys_safecopyfrom failed: %d", r);
			}
			size += s;
			o += s;
		}
	}
	if (size < ETH_MIN_PACK_SIZE)
		panic("fxp_writev: invalid packet size: %d", size);

	txp->tx_status= 0;
	txp->tx_command= TXC_EL | CBL_XMIT;
	txp->tx_tbda= TX_TBDA_NIL;
	txp->tx_size= TXSZ_EOF | size;
	txp->tx_tthresh= fp->fxp_tx_threshold;
	txp->tx_ntbd= 0;
	if (fp->fxp_tx_idle)
	{
		fp->fxp_tx_idle= 0;
		fp->fxp_tx_head= fp->fxp_tx_tail= 0;

		fxp_cu_ptr_cmd(fp, SC_CU_START, fp->fxp_tx_busaddr,
			TRUE /* check idle */);
	}
	else
	{
		/* Link new request in transmit list */
		tx_command= prev_txp->tx_command;
		assert(tx_command == (TXC_EL | CBL_XMIT));
		prev_txp->tx_command= CBL_XMIT;
		fp->fxp_tx_head= fxp_tx_head;
	}

	fp->fxp_flags |= FF_PACK_SENT;

	/* If the interrupt handler called, don't send a reply. The reply
	 * will be sent after all interrupts are handled. 
	 */
	if (from_int)
		return;
	reply(fp);
	return;

suspend:
	if (from_int)
		panic("fxp: should not be sending");

	fp->fxp_tx_mess= *mp;
	reply(fp);
}

/*===========================================================================*
 *				fxp_readv_s				     *
 *===========================================================================*/
static void fxp_readv_s(mp, from_int)
message *mp;
int from_int;
{
	int i, j, n, o, r, s, count, size, fxp_rx_head, fxp_rx_nbuf;
	endpoint_t iov_endpt;
	cp_grant_id_t iov_grant;
	port_t port;
	unsigned packlen;
	vir_bytes iov_offset;
	u16_t rfd_status;
	u16_t rfd_res;
	u8_t scb_status;
	fxp_t *fp;
	iovec_s_t *iovp;
	struct rfd *rfdp, *prev_rfdp;

	fp= fxp_state;

	count = mp->m_net_netdrv_dl_readv_s.count;
	fp->fxp_client= mp->m_source;

	assert(fp->fxp_mode == FM_ENABLED);
	assert(fp->fxp_flags & FF_ENABLED);

	port= fp->fxp_base_port;

	fxp_rx_head= fp->fxp_rx_head;
	rfdp= &fp->fxp_rx_buf[fxp_rx_head];

	rfd_status= rfdp->rfd_status;
	if (!(rfd_status & RFDS_C))
	{
		/* Receive buffer is empty, suspend */
		goto suspend;
	}

	if (!(rfd_status & RFDS_OK))
	{
		/* Not OK? What happened? */
		assert(0);
	}
	else
	{
		assert(!(rfd_status & (RFDS_CRCERR | RFDS_ALIGNERR |
			RFDS_OUTOFBUF | RFDS_DMAOVR | RFDS_TOOSHORT | 
			RFDS_RXERR)));
	}
	rfd_res= rfdp->rfd_res;
	assert(rfd_res & RFDR_EOF);
	assert(rfd_res & RFDR_F);

	packlen= rfd_res & RFDSZ_SIZE;

	iov_endpt = mp->m_source;
	iov_grant = mp->m_net_netdrv_dl_readv_s.grant;

	size= 0;
	o= 0;
	iov_offset= 0;
	for (i= 0; i<count; i += IOVEC_NR,
		iov_offset += IOVEC_NR * sizeof(fp->fxp_iovec_s[0]))
	{
		n= IOVEC_NR;
		if (i+n > count)
			n= count-i;
		r= sys_safecopyfrom(iov_endpt, iov_grant, iov_offset,
			(vir_bytes)fp->fxp_iovec_s,
			n * sizeof(fp->fxp_iovec_s[0]));
		if (r != OK)
			panic("fxp_readv_s: sys_safecopyfrom failed: %d", r);

		for (j= 0, iovp= fp->fxp_iovec_s; j<n; j++, iovp++)
		{
			s= iovp->iov_size;
			if (size + s > packlen)
			{
				assert(packlen > size);
				s= packlen-size;
			}

			r= sys_safecopyto(iov_endpt, iovp->iov_grant,
				0, (vir_bytes)(rfdp->rfd_buf+o), s);
			if (r != OK)
			{
				panic("fxp_readv: sys_safecopyto failed: %d", r);
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

	fp->fxp_read_s= packlen;
	fp->fxp_flags= (fp->fxp_flags & ~FF_READING) | FF_PACK_RECV;

	/* Re-init the current buffer */
	rfdp->rfd_status= 0;
	rfdp->rfd_command= RFDC_EL;
	rfdp->rfd_reserved= 0;
	rfdp->rfd_res= 0;
	rfdp->rfd_size= sizeof(rfdp->rfd_buf);

	fxp_rx_nbuf= fp->fxp_rx_nbuf;
	if (fxp_rx_head == 0)
	{
		prev_rfdp= &fp->fxp_rx_buf[fxp_rx_nbuf-1];
	}
	else
		prev_rfdp= &rfdp[-1];

	assert(prev_rfdp->rfd_command & RFDC_EL);
	prev_rfdp->rfd_command &= ~RFDC_EL;

	fxp_rx_head++;
	if (fxp_rx_head == fxp_rx_nbuf)
		fxp_rx_head= 0;
	assert(fxp_rx_head < fxp_rx_nbuf);
	fp->fxp_rx_head= fxp_rx_head;

	if (!from_int)
		reply(fp);

	return;

suspend:
	if (fp->fxp_rx_need_restart)
	{
		fp->fxp_rx_need_restart= 0;

		/* Check the status of the RU */
		scb_status= fxp_inb(port, SCB_STATUS);
		if ((scb_status & SS_RUS_MASK) != SS_RU_NORES)
		{
			/* Race condition? */
			printf("fxp_readv: restart race: 0x%x\n",
				scb_status);
			assert((scb_status & SS_RUS_MASK) == SS_RU_READY);
		}
		else
		{
			fxp_restart_ru(fp);
		}
	}
	if (from_int)
	{
		assert(fp->fxp_flags & FF_READING);

		/* No need to store any state */
		return;
	}

	fp->fxp_rx_mess= *mp;
	assert(!(fp->fxp_flags & FF_READING));
	fp->fxp_flags |= FF_READING;

	reply(fp);
}

/*===========================================================================*
 *				fxp_do_conf				     *
 *===========================================================================*/
static void fxp_do_conf(fp)
fxp_t *fp;
{
	int r;
	phys_bytes bus_addr;

	/* Configure device */
	tmpbufp->cc.cc_status= 0;
	tmpbufp->cc.cc_command= CBL_C_EL | CBL_CONF;
	tmpbufp->cc.cc_linkaddr= 0;
	memcpy(tmpbufp->cc.cc_bytes, fp->fxp_conf_bytes,
		sizeof(tmpbufp->cc.cc_bytes));

	r= sys_umap(SELF, VM_D, (vir_bytes)&tmpbufp->cc,
		(phys_bytes)sizeof(tmpbufp->cc), &bus_addr);
	if (r != OK)
		panic("sys_umap failed: %d", r);

	fxp_cu_ptr_cmd(fp, SC_CU_START, bus_addr, TRUE /* check idle */);

	/* Wait for CU command to complete */
	SPIN_UNTIL(tmpbufp->cc.cc_status & CBL_F_C, 100000);

	if (!(tmpbufp->cc.cc_status & CBL_F_C))
		panic("fxp_do_conf: CU command failed to complete");
	if (!(tmpbufp->cc.cc_status & CBL_F_OK))
		panic("fxp_do_conf: CU command failed");

}

/*===========================================================================*
 *				fxp_cu_ptr_cmd				     *
 *===========================================================================*/
static void fxp_cu_ptr_cmd(fp, cmd, bus_addr, check_idle)
fxp_t *fp;
int cmd;
phys_bytes bus_addr;
int check_idle;
{
	spin_t spin;
	port_t port;
	u8_t scb_cmd;

	port= fp->fxp_base_port;

	if (check_idle)
	{
		/* Consistency check. Make sure that CU is idle */
		if ((fxp_inb(port, SCB_STATUS) & SS_CUS_MASK) != SS_CU_IDLE)
			panic("fxp_cu_ptr_cmd: CU is not idle");
	}

	fxp_outl(port, SCB_POINTER, bus_addr);
	fxp_outb(port, SCB_CMD, cmd);

	/* What is a reasonable time-out? There is nothing in the
	 * documentation. 1 ms should be enough. We use 100 ms.
	 */
	spin_init(&spin, 100000);
	do {
		/* Wait for CU command to be accepted */
		scb_cmd= fxp_inb(port, SCB_CMD);
		if ((scb_cmd & SC_CUC_MASK) == SC_CU_NOP)
			break;
	} while (spin_check(&spin));

	if ((scb_cmd & SC_CUC_MASK) != SC_CU_NOP)
		panic("fxp_cu_ptr_cmd: CU does not accept command");
}

/*===========================================================================*
 *				fxp_ru_ptr_cmd				     *
 *===========================================================================*/
static void fxp_ru_ptr_cmd(fp, cmd, bus_addr, check_idle)
fxp_t *fp;
int cmd;
phys_bytes bus_addr;
int check_idle;
{
	spin_t spin;
	port_t port;
	u8_t scb_cmd;

	port= fp->fxp_base_port;

	if (check_idle)
	{
		/* Consistency check, make sure that RU is idle */
		if ((fxp_inb(port, SCB_STATUS) & SS_RUS_MASK) != SS_RU_IDLE)
			panic("fxp_ru_ptr_cmd: RU is not idle");
	}

	fxp_outl(port, SCB_POINTER, bus_addr);
	fxp_outb(port, SCB_CMD, cmd);

	spin_init(&spin, 1000);
	do {
		/* Wait for RU command to be accepted */
		scb_cmd= fxp_inb(port, SCB_CMD);
		if ((scb_cmd & SC_RUC_MASK) == SC_RU_NOP)
			break;
	} while (spin_check(&spin));

	if ((scb_cmd & SC_RUC_MASK) != SC_RU_NOP)
		panic("fxp_ru_ptr_cmd: RU does not accept command");
}

/*===========================================================================*
 *				fxp_restart_ru				     *
 *===========================================================================*/
static void fxp_restart_ru(fp)
fxp_t *fp;
{
	int i, fxp_rx_nbuf;
	port_t port;
	struct rfd *rfdp;

	port= fp->fxp_base_port;

	fxp_rx_nbuf= fp->fxp_rx_nbuf;
	for (i= 0, rfdp= fp->fxp_rx_buf; i<fxp_rx_nbuf; i++, rfdp++)
	{
		rfdp->rfd_status= 0;
		rfdp->rfd_command= 0;
		if (i == fp->fxp_rx_nbuf-1)
			rfdp->rfd_command= RFDC_EL;
		rfdp->rfd_reserved= 0;
		rfdp->rfd_res= 0;
		rfdp->rfd_size= sizeof(rfdp->rfd_buf);
	}
	fp->fxp_rx_head= 0;

	/* Make sure that RU is in the 'No resources' state */
	if ((fxp_inb(port, SCB_STATUS) & SS_RUS_MASK) != SS_RU_NORES)
		panic("fxp_restart_ru: RU is in an unexpected state");

	fxp_ru_ptr_cmd(fp, SC_RU_START, fp->fxp_rx_busaddr,
		FALSE /* do not check idle */);
}

/*===========================================================================*
 *				fxp_getstat_s				     *
 *===========================================================================*/
static void fxp_getstat_s(message *mp)
{
	int r;
	fxp_t *fp;
	u32_t *p;
	eth_stat_t stats;

	fp= fxp_state;

	assert(fp->fxp_mode == FM_ENABLED);
	assert(fp->fxp_flags & FF_ENABLED);

	p= &fp->fxp_stat.sc_tx_fcp;
	*p= 0;

	/* The dump commmand doesn't take a pointer. Setting a pointer
	 * doesn't hurt though.
	 */
	fxp_cu_ptr_cmd(fp, SC_CU_DUMP_SC, 0, FALSE /* do not check idle */);

	/* Wait for CU command to complete */
	SPIN_UNTIL(*p != 0, 1000);

	if (*p == 0)
		panic("fxp_getstat: CU command failed to complete");
	if (*p != SCM_DSC)
		panic("fxp_getstat: bad magic");

	stats.ets_recvErr=
		fp->fxp_stat.sc_rx_crc +
		fp->fxp_stat.sc_rx_align +
		fp->fxp_stat.sc_rx_resource +
		fp->fxp_stat.sc_rx_overrun +
		fp->fxp_stat.sc_rx_cd +
		fp->fxp_stat.sc_rx_short;
	stats.ets_sendErr=
		fp->fxp_stat.sc_tx_maxcol +
		fp->fxp_stat.sc_tx_latecol +
		fp->fxp_stat.sc_tx_crs;
	stats.ets_OVW= fp->fxp_stat.sc_rx_overrun;
	stats.ets_CRCerr= fp->fxp_stat.sc_rx_crc;
	stats.ets_frameAll= fp->fxp_stat.sc_rx_align;
	stats.ets_missedP= fp->fxp_stat.sc_rx_resource;
	stats.ets_packetR= fp->fxp_stat.sc_rx_good;
	stats.ets_packetT= fp->fxp_stat.sc_tx_good;
	stats.ets_transDef= fp->fxp_stat.sc_tx_defered;
	stats.ets_collision= fp->fxp_stat.sc_tx_totcol;
	stats.ets_transAb= fp->fxp_stat.sc_tx_maxcol;
	stats.ets_carrSense= fp->fxp_stat.sc_tx_crs;
	stats.ets_fifoUnder= fp->fxp_stat.sc_tx_underrun;
	stats.ets_fifoOver= fp->fxp_stat.sc_rx_overrun;
	stats.ets_CDheartbeat= 0;
	stats.ets_OWC= fp->fxp_stat.sc_tx_latecol;

	r= sys_safecopyto(mp->m_source, mp->m_net_netdrv_dl_getstat_s.grant, 0,
		(vir_bytes)&stats, sizeof(stats));
	if (r != OK)
		panic("fxp_getstat_s: sys_safecopyto failed: %d", r);

	mp->m_type= DL_STAT_REPLY;
	r= ipc_send(mp->m_source, mp);
	if (r != OK)
		panic("fxp_getstat_s: ipc_send failed: %d", r);
}

/*===========================================================================*
 *				fxp_handler				     *
 *===========================================================================*/
static void fxp_handler(fxp_t *fp)
{
	int port;
	u16_t isr;

	RAND_UPDATE

	port= fp->fxp_base_port;

	/* Ack interrupt */
	isr= fxp_inb(port, SCB_INT_STAT);
	fxp_outb(port, SCB_INT_STAT, isr);

	if (isr & SIS_FR)
	{
		isr &= ~SIS_FR;

		if (!fp->fxp_got_int && (fp->fxp_flags & FF_READING))
		{
			fp->fxp_got_int= TRUE;
			interrupt(fxp_tasknr);
		}
	}
	if (isr & SIS_CNA)
	{
		isr &= ~SIS_CNA;
		if (!fp->fxp_tx_idle)
		{
			fp->fxp_send_int= TRUE;
			if (!fp->fxp_got_int)
			{
				fp->fxp_got_int= TRUE;
				interrupt(fxp_tasknr);
			}
		}
	}
	if (isr & SIS_RNR)
	{
		isr &= ~SIS_RNR;

		/* Assume that receive buffer is full of packets. fxp_readv
		 * will restart the RU.
		 */
		fp->fxp_rx_need_restart= 1;
	}
	if (isr)
	{
		printf("fxp_handler: unhandled interrupt: isr = 0x%02x\n",
			isr);
	}
}

/*===========================================================================*
 *				fxp_check_ints				     *
 *===========================================================================*/
static void fxp_check_ints(fxp_t *fp)
{
	int n, fxp_flags, prev_tail;
	int fxp_tx_tail, fxp_tx_nbuf, fxp_tx_threshold;
	port_t port;
	u32_t busaddr;
	u16_t tx_status;
	u8_t scb_status;
	struct tx *txp;

	fxp_flags= fp->fxp_flags;

	if (fxp_flags & FF_READING)
	{
		if (!(fp->fxp_rx_buf[fp->fxp_rx_head].rfd_status & RFDS_C))
			; /* Nothing */
		else
		{
			fxp_readv_s(&fp->fxp_rx_mess, TRUE /* from int */);
		}
	}
	if (fp->fxp_tx_idle)
		;	/* Nothing to do */
	else if (fp->fxp_send_int)
	{
		fp->fxp_send_int= FALSE;
		fxp_tx_tail= fp->fxp_tx_tail;
		fxp_tx_nbuf= fp->fxp_tx_nbuf;
		n= 0;
		for (;;)
		{
			txp= &fp->fxp_tx_buf[fxp_tx_tail];
			tx_status= txp->tx_status;
			if (!(tx_status & TXS_C))
				break;

			n++;

			assert(tx_status & TXS_OK);
			if (tx_status & TXS_U)
			{
				fxp_tx_threshold= fp->fxp_tx_threshold;
				if (fxp_tx_threshold < TXTT_MAX)
				{
					fxp_tx_threshold++;
					fp->fxp_tx_threshold= fxp_tx_threshold;
				}
				printf(
			"fxp_check_ints: fxp_tx_threshold = 0x%x\n",
					fxp_tx_threshold);
			}

			if (txp->tx_command & TXC_EL)
			{
				fp->fxp_tx_idle= 1;
				break;
			}

			fxp_tx_tail++;
			if (fxp_tx_tail == fxp_tx_nbuf)
				fxp_tx_tail= 0;
			assert(fxp_tx_tail < fxp_tx_nbuf);
		}

		if (fp->fxp_need_conf)
		{
			/* Check the status of the CU */
			port= fp->fxp_base_port;
			scb_status= fxp_inb(port, SCB_STATUS);
			if ((scb_status & SS_CUS_MASK) != SS_CU_IDLE)
			{
				/* Nothing to do */
				printf("scb_status = 0x%x\n", scb_status);
			}
			else
			{
				printf("fxp_check_ints: fxp_need_conf\n");
				fp->fxp_need_conf= FALSE;
				fxp_do_conf(fp);
			}
		}

		if (n)
		{
			if (!fp->fxp_tx_idle)
			{
				fp->fxp_tx_tail= fxp_tx_tail;
				
				/* Check the status of the CU */
				port= fp->fxp_base_port;
				scb_status= fxp_inb(port, SCB_STATUS);
				if ((scb_status & SS_CUS_MASK) != SS_CU_IDLE)
				{
					/* Nothing to do */
					printf("scb_status = 0x%x\n",
						scb_status);

				}
				else
				{
					if (fxp_tx_tail == 0)
						prev_tail= fxp_tx_nbuf-1;
					else
						prev_tail= fxp_tx_tail-1;
					busaddr= fp->fxp_tx_buf[prev_tail].
						tx_linkaddr;

					fxp_cu_ptr_cmd(fp, SC_CU_START,
						busaddr, 1 /* check idle */);
				}
			}

			if (fp->fxp_flags & FF_SEND_AVAIL)
			{
				fxp_writev_s(&fp->fxp_tx_mess,
					TRUE /* from int */);
			}
		}
		
	}
	if (fp->fxp_report_link)
		fxp_report_link(fp);

	if (fp->fxp_flags & (FF_PACK_SENT | FF_PACK_RECV))
		reply(fp);
}

/*===========================================================================*
 *				fxp_watchdog_f				     *
 *===========================================================================*/
static void fxp_watchdog_f(tp)
minix_timer_t *tp;
{
	fxp_t *fp;

	set_timer(&fxp_watchdog, system_hz, fxp_watchdog_f, 0);

	fp= fxp_state;
	if (fp->fxp_mode != FM_ENABLED)
		return;

	/* Handle race condition, MII interface might be busy */
	if(!fp->fxp_mii_busy)
	{
		/* Check the link status. */
		if (fxp_link_changed(fp))
		{
#if VERBOSE
			printf("fxp_watchdog_f: link changed\n");
#endif
			fp->fxp_report_link= TRUE;
			fp->fxp_got_int= TRUE;
			interrupt(fxp_tasknr);
		}
	}
		
	if (!(fp->fxp_flags & FF_SEND_AVAIL))
	{
		/* Assume that an idle system is alive */
		fp->fxp_tx_alive= TRUE;
		return;
	}
	if (fp->fxp_tx_alive)
	{
		fp->fxp_tx_alive= FALSE;
		return;
	}

	fp->fxp_need_reset= TRUE;
	fp->fxp_got_int= TRUE;
	interrupt(fxp_tasknr);
}

/*===========================================================================*
 *				fxp_link_changed			     *
 *===========================================================================*/
static int fxp_link_changed(fp)
fxp_t *fp;
{
	u16_t scr;

	scr= mii_read(fp, MII_SCR);
	scr &= ~(MII_SCR_RES|MII_SCR_RES_1);

	return (fp->fxp_mii_scr != scr);
}

/*===========================================================================*
 *				fxp_report_link				     *
 *===========================================================================*/
static void fxp_report_link(fxp_t *fp)
{
	u16_t mii_ctrl, mii_status, mii_id1, mii_id2, 
		mii_ana, mii_anlpa, mii_ane, mii_extstat,
		mii_ms_ctrl, mii_ms_status, scr;
	u32_t oui;
	int model, rev;
	int f, link_up;

	fp->fxp_report_link= FALSE;

	scr= mii_read(fp, MII_SCR);
	scr &= ~(MII_SCR_RES|MII_SCR_RES_1);
	fp->fxp_mii_scr= scr;

	mii_ctrl= mii_read(fp, MII_CTRL);
	mii_read(fp, MII_STATUS); /* The status reg is latched, read twice */
	mii_status= mii_read(fp, MII_STATUS);
	mii_id1= mii_read(fp, MII_PHYID_H);
	mii_id2= mii_read(fp, MII_PHYID_L);
	mii_ana= mii_read(fp, MII_ANA);
	mii_anlpa= mii_read(fp, MII_ANLPA);
	mii_ane= mii_read(fp, MII_ANE);
	if (mii_status & MII_STATUS_EXT_STAT)
		mii_extstat= mii_read(fp, MII_EXT_STATUS);
	else
		mii_extstat= 0;
	if (fp->fxp_ms_regs)
	{
		mii_ms_ctrl= mii_read(fp, MII_MS_CTRL);
		mii_ms_status= mii_read(fp, MII_MS_STATUS);
	}
	else
	{
		mii_ms_ctrl= 0;
		mii_ms_status= 0;
	}

	/* How do we know about the link status? */
	link_up= !!(mii_status & MII_STATUS_LS);

	fp->fxp_link_up= link_up;
	if (!link_up)
	{
#if VERBOSE
		printf("%s: link down\n", fp->fxp_name);
#endif
		return;
	}

	oui= (mii_id1 << MII_PH_OUI_H_C_SHIFT) | 
		((mii_id2 & MII_PL_OUI_L_MASK) >> MII_PL_OUI_L_SHIFT);
	model= ((mii_id2 & MII_PL_MODEL_MASK) >> MII_PL_MODEL_SHIFT);
	rev= (mii_id2 & MII_PL_REV_MASK);

#if VERBOSE
	printf("OUI 0x%06lx, Model 0x%02x, Revision 0x%x\n", oui, model, rev);
#endif

	if (mii_ctrl & (MII_CTRL_LB|MII_CTRL_PD|MII_CTRL_ISO))
	{
		printf("%s: PHY: ", fp->fxp_name);
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
		printf("%s: manual config: ", fp->fxp_name);
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

	printf("%s: ", fp->fxp_name);
	mii_print_stat_speed(mii_status, mii_extstat);
	printf("\n");

	if (!(mii_status & MII_STATUS_ANC))
		printf("%s: auto-negotiation not complete\n", fp->fxp_name);
	if (mii_status & MII_STATUS_RF)
		printf("%s: remote fault detected\n", fp->fxp_name);
	if (!(mii_status & MII_STATUS_ANA))
	{
		printf("%s: local PHY has no auto-negotiation ability\n",
			fp->fxp_name);
	}
	if (!(mii_status & MII_STATUS_LS))
		printf("%s: link down\n", fp->fxp_name);
	if (mii_status & MII_STATUS_JD)
		printf("%s: jabber condition detected\n", fp->fxp_name);
	if (!(mii_status & MII_STATUS_EC))
	{
		printf("%s: no extended register set\n", fp->fxp_name);
		goto resspeed;
	}
	if (!(mii_status & MII_STATUS_ANC))
		goto resspeed;

	printf("%s: local cap.: ", fp->fxp_name);
	if (mii_ms_ctrl & (MII_MSC_1000T_FD | MII_MSC_1000T_HD))
	{
		printf("1000 Mbps: T-");
		switch(mii_ms_ctrl & (MII_MSC_1000T_FD | MII_MSC_1000T_HD))
		{
		case MII_MSC_1000T_FD:	printf("FD"); break;
		case MII_MSC_1000T_HD:	printf("HD"); break;
		default:		printf("FD/HD"); break;
		}
		if (mii_ana)
			printf(", ");
	}
	mii_print_techab(mii_ana);
	printf("\n");

	if (mii_ane & MII_ANE_PDF)
		printf("%s: parallel detection fault\n", fp->fxp_name);
	if (!(mii_ane & MII_ANE_LPANA))
	{
		printf("%s: link-partner does not support auto-negotiation\n",
			fp->fxp_name);
		goto resspeed;
	}

	printf("%s: remote cap.: ", fp->fxp_name);
	if (mii_ms_ctrl & (MII_MSC_1000T_FD | MII_MSC_1000T_HD))
	if (mii_ms_status & (MII_MSS_LP1000T_FD | MII_MSS_LP1000T_HD))
	{
		printf("1000 Mbps: T-");
		switch(mii_ms_status &
			(MII_MSS_LP1000T_FD | MII_MSS_LP1000T_HD))
		{
		case MII_MSS_LP1000T_FD:	printf("FD"); break;
		case MII_MSS_LP1000T_HD:	printf("HD"); break;
		default:			printf("FD/HD"); break;
		}
		if (mii_anlpa)
			printf(", ");
	}
	mii_print_techab(mii_anlpa);
	printf("\n");

	if (fp->fxp_ms_regs)
	{
		printf("%s: ", fp->fxp_name);
		if (mii_ms_ctrl & MII_MSC_MS_MANUAL)
		{
			printf("manual %s",
				(mii_ms_ctrl & MII_MSC_MS_VAL) ?
				"MASTER" : "SLAVE");
		}
		else
		{
			printf("%s device",
				(mii_ms_ctrl & MII_MSC_MULTIPORT) ?
				"multiport" : "single-port");
		}
		if (mii_ms_ctrl & MII_MSC_RES)
			printf(" reserved<0x%x>", mii_ms_ctrl & MII_MSC_RES);
		printf(": ");
		if (mii_ms_status & MII_MSS_FAULT)
			printf("M/S config fault");
		else if (mii_ms_status & MII_MSS_MASTER)
			printf("MASTER");
		else
			printf("SLAVE");
		printf("\n");
	}

	if (mii_ms_status & (MII_MSS_LP1000T_FD|MII_MSS_LP1000T_HD))
	{
		if (!(mii_ms_status & MII_MSS_LOCREC))
		{
			printf("%s: local receiver not OK\n",
				fp->fxp_name);
		}
		if (!(mii_ms_status & MII_MSS_REMREC))
		{
			printf("%s: remote receiver not OK\n",
				fp->fxp_name);
		}
	}
	if (mii_ms_status & (MII_MSS_RES|MII_MSS_IDLE_ERR))
	{
		printf("%s", fp->fxp_name);
		if (mii_ms_status & MII_MSS_RES)
			printf(" reserved<0x%x>", mii_ms_status & MII_MSS_RES);
		if (mii_ms_status & MII_MSS_IDLE_ERR)
		{
			printf(" idle error %d",
				mii_ms_status & MII_MSS_IDLE_ERR);
		}
		printf("\n");
	}

resspeed:
#if VERBOSE
	printf("%s: link up, %d Mbps, %s duplex\n",
		fp->fxp_name, (scr & MII_SCR_100) ? 100 : 10,
		(scr & MII_SCR_FD) ? "full" : "half");
#endif
	;
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
static void reply(fp)
fxp_t *fp;
{
	message reply;
	int flags;
	int r;

	flags = DL_NOFLAGS;
	if (fp->fxp_flags & FF_PACK_SENT)
		flags |= DL_PACK_SEND;
	if (fp->fxp_flags & FF_PACK_RECV)
		flags |= DL_PACK_RECV;

	reply.m_type = DL_TASK_REPLY;
	reply.m_netdrv_net_dl_task.flags = flags;
	reply.m_netdrv_net_dl_task.count = fp->fxp_read_s;

	r= ipc_send(fp->fxp_client, &reply);

	if (r < 0)
		panic("fxp: ipc_send failed: %d", r);
	
	fp->fxp_read_s = 0;
	fp->fxp_flags &= ~(FF_PACK_SENT | FF_PACK_RECV);
}

/*===========================================================================*
 *				mess_reply				     *
 *===========================================================================*/
static void mess_reply(req, reply_mess)
message *req;
message *reply_mess;
{
	if (ipc_send(req->m_source, reply_mess) != OK)
		panic("fxp: unable to mess_reply");
}

/*===========================================================================*
 *				eeprom_read				     *
 *===========================================================================*/
static u16_t eeprom_read(fp, reg)
fxp_t *fp;
int reg;
{
	port_t port;
	u16_t v;
	int b, i, alen;

	alen= fp->fxp_ee_addrlen;
	if (!alen)
	{
		eeprom_addrsize(fp);
		alen= fp->fxp_ee_addrlen;
		assert(alen == 6 || alen == 8);
	}

	port= fp->fxp_base_port;

	fxp_outb(port, CSR_EEPROM, CE_EECS);	/* Enable EEPROM */
	v= EEPROM_READ_PREFIX;
	for (i= EEPROM_PREFIX_LEN-1; i >= 0; i--)
	{
		b= ((v & (1 << i)) ? CE_EEDI : 0);
		fxp_outb(port, CSR_EEPROM, CE_EECS | b);	/* bit */
		fxp_outb(port, CSR_EEPROM, CE_EECS | b | CE_EESK); /* Clock */
		micro_delay(EESK_PERIOD/2+1);
		fxp_outb(port, CSR_EEPROM, CE_EECS | b);		
		micro_delay(EESK_PERIOD/2+1);
	}
	
	v= reg;
	for (i= alen-1; i >= 0; i--)
	{
		b= ((v & (1 << i)) ? CE_EEDI : 0);
		fxp_outb(port, CSR_EEPROM, CE_EECS | b);	/* bit */
		fxp_outb(port, CSR_EEPROM, CE_EECS | b | CE_EESK); /* Clock */
		micro_delay(EESK_PERIOD/2+1);
		fxp_outb(port, CSR_EEPROM, CE_EECS | b);		
		micro_delay(EESK_PERIOD/2+1);
	}

	v= 0;
	for (i= 0; i<16; i++)
	{
		fxp_outb(port, CSR_EEPROM, CE_EECS | CE_EESK); /* Clock */
		micro_delay(EESK_PERIOD/2+1);
		b= !!(fxp_inb(port, CSR_EEPROM) & CE_EEDO);
		v= (v << 1) | b;
		fxp_outb(port, CSR_EEPROM, CE_EECS );		
		micro_delay(EESK_PERIOD/2+1);
	}
	fxp_outb(port, CSR_EEPROM, 0);	/* Disable EEPROM */
	micro_delay(EECS_DELAY);

	return v;
}

/*===========================================================================*
 *				eeprom_addrsize				     *
 *===========================================================================*/
static void eeprom_addrsize(fp)
fxp_t *fp;
{
	port_t port;
	u16_t v;
	int b, i;

	port= fp->fxp_base_port;

	/* Try to find out the size of the EEPROM */
	fxp_outb(port, CSR_EEPROM, CE_EECS);	/* Enable EEPROM */
	v= EEPROM_READ_PREFIX;
	for (i= EEPROM_PREFIX_LEN-1; i >= 0; i--)
	{
		b= ((v & (1 << i)) ? CE_EEDI : 0);
		fxp_outb(port, CSR_EEPROM, CE_EECS | b);	/* bit */
		fxp_outb(port, CSR_EEPROM, CE_EECS | b | CE_EESK); /* Clock */
		micro_delay(EESK_PERIOD/2+1);
		fxp_outb(port, CSR_EEPROM, CE_EECS | b);		
		micro_delay(EESK_PERIOD/2+1);
	}

	for (i= 0; i<32; i++)
	{
		b= 0;
		fxp_outb(port, CSR_EEPROM, CE_EECS | b);	/* bit */
		fxp_outb(port, CSR_EEPROM, CE_EECS | b | CE_EESK); /* Clock */
		micro_delay(EESK_PERIOD/2+1);
		fxp_outb(port, CSR_EEPROM, CE_EECS | b);		
		micro_delay(EESK_PERIOD/2+1);
		v= fxp_inb(port, CSR_EEPROM);
		if (!(v & CE_EEDO))
			break;
	}
	if (i >= 32)
		panic("eeprom_addrsize: failed");
	fp->fxp_ee_addrlen= i+1;

	/* Discard 16 data bits */
	for (i= 0; i<16; i++)
	{
		fxp_outb(port, CSR_EEPROM, CE_EECS | CE_EESK); /* Clock */
		micro_delay(EESK_PERIOD/2+1);
		fxp_outb(port, CSR_EEPROM, CE_EECS );		
		micro_delay(EESK_PERIOD/2+1);
	}
	fxp_outb(port, CSR_EEPROM, 0);	/* Disable EEPROM */
	micro_delay(EECS_DELAY);

#if VERBOSE
	printf("%s EEPROM address length: %d\n",
		fp->fxp_name, fp->fxp_ee_addrlen);
#endif
}

/*===========================================================================*
 *				mii_read				     *
 *===========================================================================*/
static u16_t mii_read(fp, reg)
fxp_t *fp;
int reg;
{
	spin_t spin;
	port_t port;
	u32_t v;

	port= fp->fxp_base_port;

	assert(!fp->fxp_mii_busy);
	fp->fxp_mii_busy++;

	if (!(fxp_inl(port, CSR_MDI_CTL) & CM_READY))
		panic("mii_read: MDI not ready");
	fxp_outl(port, CSR_MDI_CTL, CM_READ | (1 << CM_PHYADDR_SHIFT) |
		(reg << CM_REG_SHIFT));

	spin_init(&spin, 100000);
	do {
		v= fxp_inl(port, CSR_MDI_CTL);
		if (v & CM_READY)
			break;
	} while (spin_check(&spin));

	if (!(v & CM_READY))
		panic("mii_read: MDI not ready after command");

	fp->fxp_mii_busy--;
	assert(!fp->fxp_mii_busy);

	return v & CM_DATA_MASK;
}

static u8_t do_inb(port_t port)
{
	int r;
	u32_t value;

	r= sys_inb(port, &value);
	if (r != OK)
		panic("sys_inb failed: %d", r);
	return value;
}

static u32_t do_inl(port_t port)
{
	int r;
	u32_t value;

	r= sys_inl(port, &value);
	if (r != OK)
		panic("sys_inl failed: %d", r);
	return value;
}

static void do_outb(port_t port, u8_t value)
{
	int r;

	r= sys_outb(port, value);
	if (r != OK)
		panic("sys_outb failed: %d", r);
}

static void do_outl(port_t port, u32_t value)
{
	int r;

	r= sys_outl(port, value);
	if (r != OK)
		panic("sys_outl failed: %d", r);
}

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
		"fxp`tell_dev: ds_retrieve_label_endpt failed for 'amddev': %d\n",
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
		printf("fxp`tell_dev: ipc_sendrec to %d failed: %d\n",
			dev_e, r);
		return;
	}
	if (m.m_type != OK)
	{
		printf("fxp`tell_dev: dma map request failed: %d\n",
			m.m_type);
		return;
	}
}

/*
 * $PchId: fxp.c,v 1.4 2005/01/31 22:10:37 philip Exp $
 */

