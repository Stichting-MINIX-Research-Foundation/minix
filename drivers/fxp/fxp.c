/*
 * fxp.c
 *
 * This file contains an ethernet device driver for Intel 82557, 82558, 
 * 82559, 82550, and 82562 fast ethernet controllers.
 *
 * The valid messages and their parameters are:
 *
 *   m_type	  DL_PORT    DL_PROC   DL_COUNT   DL_MODE   DL_ADDR
 * |------------+----------+---------+----------+---------+---------|
 * | HARDINT	|          |         |          |         |         |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_WRITE	| port nr  | proc nr | count    | mode    | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_WRITEV	| port nr  | proc nr | count    | mode    | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_READ	| port nr  | proc nr | count    |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_READV	| port nr  | proc nr | count    |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_INIT	| port nr  | proc nr | mode     |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_GETSTAT	| port nr  | proc nr |          |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_STOP	| port_nr  |         |          |         |	    |
 * |------------|----------|---------|----------|---------|---------|
 *
 * The messages sent are:
 *
 *   m-type	   DL_PORT    DL_PROC   DL_COUNT   DL_STAT   DL_CLCK
 * |-------------+----------+---------+----------+---------+---------|
 * |DL_TASK_REPLY| port nr  | proc nr | rd-count | err|stat| clock   |
 * |-------------+----------+---------+----------+---------+---------|
 *
 *   m_type	   m3_i1     m3_i2       m3_ca1
 * |-------------+---------+-----------+---------------|
 * |DL_INIT_REPLY| port nr | last port | ethernet addr |
 * |-------------+---------+-----------+---------------|
 *
 * Created:	Nov 2004 by Philip Homburg <philip@f-mnx.phicoh.com>
 */

#include "../drivers.h"

#include <stdlib.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>

#include <timers.h>

#define tmra_ut			timer_t
#define tmra_inittimer(tp)	tmr_inittimer(tp)
#define Proc_number(p)		proc_number(p)
#define debug			0
#define RAND_UPDATE		/**/
#define printW()		((void)0)
#define vm_1phys2bus(p)		(p)

#include "assert.h"
#include "../libpci/pci.h"
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

struct pcitab
{
	u16_t vid;
	u16_t did;
	int checkclass;
};

PRIVATE struct pcitab pcitab_fxp[]=
{
	{ 0x8086, 0x1229, 0 },		/* Intel 82557, etc. */
	{ 0x8086, 0x2449, 0 },		/* Intel 82801BA/BAM/CA/CAM */

	{ 0x0000, 0x0000, 0 }
};

#define FXP_PORT_NR	1		/* Minix */

typedef int irq_hook_t;

/* Translate a pointer to a field in a structure to a pointer to the structure
 * itself.  So it translates '&struct_ptr->field' back to 'struct_ptr'.
 */
#define structof(type, field, ptr) \
	((type *) (((char *) (ptr)) - offsetof(type, field)))

#define MICROS_TO_TICKS(m)  (((m)*HZ/1000000)+1)

static timer_t *fxp_timers= NULL;
static clock_t fxp_next_timeout= 0;

static void micro_delay(unsigned long usecs);

/* ignore interrupt for the moment */
#define interrupt(x)	0

char buffer[70*1024];

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
	u8_t fxp_pcibus;	
	u8_t fxp_pcidev;	
	u8_t fxp_pcifunc;	

	/* 'large' items */
	irq_hook_t fxp_hook;
	ether_addr_t fxp_address;
	message fxp_rx_mess;
	message fxp_tx_mess;
	struct sc fxp_stat;
	u8_t fxp_conf_bytes[CC_BYTES_NR];
	char fxp_name[sizeof("fxp#n")];
	iovec_t fxp_iovec[IOVEC_NR];
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

static fxp_t fxp_table[FXP_PORT_NR];

static int fxp_tasknr= ANY;
static u16_t eth_ign_proto;
static tmra_ut fxp_watchdog;

extern int errno;

#define fxp_inb(port, offset)	(do_inb((port) + (offset)))
#define fxp_inw(port, offset)	(do_inw((port) + (offset)))
#define fxp_inl(port, offset)	(do_inl((port) + (offset)))
#define fxp_outb(port, offset, value)	(do_outb((port) + (offset), (value)))
#define fxp_outw(port, offset, value)	(do_outw((port) + (offset), (value)))
#define fxp_outl(port, offset, value)	(do_outl((port) + (offset), (value)))

_PROTOTYPE( static void fxp_init, (message *mp)				);
_PROTOTYPE( static void fxp_pci_conf, (void)				);
_PROTOTYPE( static int fxp_probe, (fxp_t *fp)				);
_PROTOTYPE( static void fxp_conf_hw, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_init_hw, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_init_buf, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_reset_hw, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_confaddr, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_rec_mode, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_writev, (message *mp, int from_int,
							int vectored)	);
_PROTOTYPE( static void fxp_readv, (message *mp, int from_int, 
							int vectored)	);
_PROTOTYPE( static void fxp_do_conf, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_cu_ptr_cmd, (fxp_t *fp, int cmd,
				phys_bytes bus_addr, int check_idle)	);
_PROTOTYPE( static void fxp_ru_ptr_cmd, (fxp_t *fp, int cmd,
				phys_bytes bus_addr, int check_idle)	);
_PROTOTYPE( static void fxp_restart_ru, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_getstat, (message *mp)			);
_PROTOTYPE( static int fxp_handler, (fxp_t *fp)				);
_PROTOTYPE( static void fxp_check_ints, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_watchdog_f, (timer_t *tp)			);
_PROTOTYPE( static int fxp_link_changed, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_report_link, (fxp_t *fp)			);
_PROTOTYPE( static void fxp_stop, (void));
_PROTOTYPE( static void reply, (fxp_t *fp, int err, int may_block)	);
_PROTOTYPE( static void mess_reply, (message *req, message *reply)	);
_PROTOTYPE( static void put_userdata, (int user_proc,
		vir_bytes user_addr, vir_bytes count, void *loc_addr)	);
_PROTOTYPE( static u16_t eeprom_read, (fxp_t *fp, int reg)		);
_PROTOTYPE( static void eeprom_addrsize, (fxp_t *fp)			);
_PROTOTYPE( static u16_t mii_read, (fxp_t *fp, int reg)			);
_PROTOTYPE( static void fxp_set_timer,(timer_t *tp, clock_t delta,
						tmr_func_t watchdog)	);
_PROTOTYPE( static void fxp_expire_timers,(void)			);
_PROTOTYPE( static u8_t do_inb, (port_t port)				);
_PROTOTYPE( static u32_t do_inl, (port_t port)				);
_PROTOTYPE( static void do_outb, (port_t port, u8_t v)			);
_PROTOTYPE( static void do_outl, (port_t port, u32_t v)			);

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(void)
{
	message m;
	int i, r;
	fxp_t *fp;
	long v;

	if ((fxp_tasknr= getprocnr())<0)
		panic("FXP", "couldn't get proc nr", errno);

	v= 0;
#if 0
	(void) env_parse("ETH_IGN_PROTO", "x", 0, &v, 0x0000L, 0xFFFFL);
#endif
	eth_ign_proto= htons((u16_t) v);

#if 0	/* What about memory allocation? */
	/* Claim buffer memory now under Minix, before MM takes it all. */
	for (fp= &fxp_table[0]; fp < fxp_table+FXP_PORT_NR; fp++)
		fxp_init_buf(fp);
#endif

	while (TRUE)
	{
		if ((r= receive(ANY, &m)) != OK)
			panic("FXP","receive failed", r);

		switch (m.m_type)
		{
		case DL_WRITEV:	fxp_writev(&m, FALSE, TRUE);	break;
		case DL_WRITE:	fxp_writev(&m, FALSE, FALSE);	break;
#if 0
		case DL_READ:	fxp_vread(&m, FALSE);		break;
#endif
		case DL_READV:	fxp_readv(&m, FALSE, TRUE);	break;
		case DL_INIT:	fxp_init(&m);			break;
		case DL_GETSTAT: fxp_getstat(&m);		break;
		case HARD_INT:
			for (i= 0, fp= &fxp_table[0]; i<FXP_PORT_NR; i++, fp++)
			{
				if (fp->fxp_mode != FM_ENABLED)
					continue;
				fxp_handler(fp);

				r= sys_irqenable(&fp->fxp_hook);
				if (r != OK)
					panic("FXP","unable enable interrupts", r);

				if (!fp->fxp_got_int)
					continue;
				fp->fxp_got_int= 0;
				assert(fp->fxp_flags & FF_ENABLED);
				fxp_check_ints(fp);
			}
			break;
		case SYS_SIG:	{
			sigset_t sigset = m.NOTIFY_ARG;
			if (sigismember(&sigset, SIGKSTOP)) fxp_stop();
			break;
		}
		case SYN_ALARM:	fxp_expire_timers();		break;
		default:
			panic("FXP"," illegal message", m.m_type);
		}
	}
}

/*===========================================================================*
 *				fxp_init				     *
 *===========================================================================*/
static void fxp_init(mp)
message *mp;
{
	static int first_time= 1;

	int port;
	fxp_t *fp;
	message reply_mess;

	if (first_time)
	{
		first_time= 0;
		fxp_pci_conf(); /* Configure PCI devices. */

		tmra_inittimer(&fxp_watchdog);
		tmr_arg(&fxp_watchdog)->ta_int= 0;
		fxp_set_timer(&fxp_watchdog, HZ, fxp_watchdog_f);
	}

	port = mp->DL_PORT;
	if (port < 0 || port >= FXP_PORT_NR)
	{
		reply_mess.m_type= DL_INIT_REPLY;
		reply_mess.m3_i1= ENXIO;
		mess_reply(mp, &reply_mess);
		return;
	}
	fp= &fxp_table[port];
	if (fp->fxp_mode == FM_DISABLED)
	{
		/* This is the default, try to (re)locate the device. */
		fxp_conf_hw(fp);
		if (fp->fxp_mode == FM_DISABLED)
		{
			/* Probe failed, or the device is configured off. */
			reply_mess.m_type= DL_INIT_REPLY;
			reply_mess.m3_i1= ENXIO;
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

	if (mp->DL_MODE & DL_PROMISC_REQ)
		fp->fxp_flags |= FF_PROMISC;
	if (mp->DL_MODE & DL_MULTI_REQ)
		fp->fxp_flags |= FF_MULTI;
	if (mp->DL_MODE & DL_BROAD_REQ)
		fp->fxp_flags |= FF_BROAD;

	fp->fxp_client = mp->m_source;
	fxp_rec_mode(fp);

	reply_mess.m_type = DL_INIT_REPLY;
	reply_mess.m3_i1 = mp->DL_PORT;
	reply_mess.m3_i2 = FXP_PORT_NR;
	*(ether_addr_t *) reply_mess.m3_ca1 = fp->fxp_address;

	mess_reply(mp, &reply_mess);
}

/*===========================================================================*
 *				fxp_pci_conf				     *
 *===========================================================================*/
static void fxp_pci_conf()
{
	static char envvar[] = FXP_ENVVAR "#";
	static char envfmt[] = "*:d.d.d";

	int i, h;
	fxp_t *fp;
	long v;

	for (i= 0, fp= fxp_table; i<FXP_PORT_NR; i++, fp++)
	{
		strcpy(fp->fxp_name, "fxp#0");
		fp->fxp_name[4] += i;
		fp->fxp_seen= FALSE;
		fp->fxp_features= FFE_NONE;
		envvar[sizeof(FXP_ENVVAR)-1]= '0'+i;
#if 0
		if (getenv(envvar) != NULL)
		{
			if (strcmp(getenv(envvar), "off") == 0)
			{
				fp->fxp_pcibus= 255;
				continue;
			}
			if (!env_prefix(envvar, "pci"))
				env_panic(envvar);
		}
#endif

		v= 0;
#if 0
		(void) env_parse(envvar, envfmt, 1, &v, 0, 255);
#endif
		fp->fxp_pcibus= v;
		v= 0;
#if 0
		(void) env_parse(envvar, envfmt, 2, &v, 0, 255);
#endif
		fp->fxp_pcidev= v;
		v= 0;
#if 0
		(void) env_parse(envvar, envfmt, 3, &v, 0, 255);
#endif
		fp->fxp_pcifunc= v;
	}

	pci_init();

	for (h= 1; h >= 0; h--) {
		for (i= 0, fp= fxp_table; i<FXP_PORT_NR; i++, fp++)
		{
			if (fp->fxp_pcibus == 255)
				continue;
			if (((fp->fxp_pcibus | fp->fxp_pcidev |
				fp->fxp_pcifunc) != 0) != h)
			{
				continue;
			}
			if (fxp_probe(fp))
				fp->fxp_seen= TRUE;
		}
	}
}

/*===========================================================================*
 *				fxp_probe				     *
 *===========================================================================*/
static int fxp_probe(fp)
fxp_t *fp;
{
	int i, r, devind, just_one;
	u16_t vid, did;
	u32_t bar;
	u8_t ilr, rev;
	char *dname, *str;

	if ((fp->fxp_pcibus | fp->fxp_pcidev | fp->fxp_pcifunc) != 0)
	{
		/* Look for specific PCI device */
		r= pci_find_dev(fp->fxp_pcibus, fp->fxp_pcidev,
			fp->fxp_pcifunc, &devind);
		if (r == 0)
		{
			printf("%s: no PCI device found at %d.%d.%d\n",
				fp->fxp_name, fp->fxp_pcibus,
				fp->fxp_pcidev, fp->fxp_pcifunc);
			return FALSE;
		}
		pci_ids(devind, &vid, &did);
		just_one= TRUE;
	}
	else
	{
		r= pci_first_dev(&devind, &vid, &did);
		if (r == 0)
			return FALSE;
		just_one= FALSE;
	}

	for(;;)
	{
		for (i= 0; pcitab_fxp[i].vid != 0; i++)
		{
			if (pcitab_fxp[i].vid != vid)
				continue;
			if (pcitab_fxp[i].did != did)
				continue;
			if (pcitab_fxp[i].checkclass)
			{
				panic("FXP","fxp_probe: class check not implemented",
					NO_NUM);
			}
			break;
		}
		if (pcitab_fxp[i].vid != 0)
			break;

		if (just_one)
		{
			printf(
		"%s: wrong PCI device (%04x/%04x) found at %d.%d.%d\n",
				fp->fxp_name, vid, did,
				fp->fxp_pcibus,
				fp->fxp_pcidev, fp->fxp_pcifunc);
			return FALSE;
		}

		r= pci_next_dev(&devind, &vid, &did);
		if (!r)
			return FALSE;
	}

	dname= pci_dev_name(vid, did);
#if VERBOSE
	if (!dname)
		dname= "unknown device";
	printf("%s: %s (%04x/%04x) at %s\n",
		fp->fxp_name, dname, vid, did, pci_slot_name(devind));
#endif
	pci_reserve(devind);

	bar= pci_attr_r32(devind, PCI_BAR_2) & 0xffffffe0;
	if ((bar & 0x3ff) >= 0x100-32 || bar < 0x400)
	{
		panic("FXP","fxp_probe: base address is not properly configured",
			NO_NUM);
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
	case FXP_REV_82558B:	str= "82558B"; break;		/* 0x05 */
	case FXP_REV_82559A:	str= "82559A"; break;		/* 0x06 */
	case FXP_REV_82559B:	str= "82559B"; break;		/* 0x07 */
	case FXP_REV_82559C:	str= "82559C";			/* 0x08 */
				fp->fxp_type= FT_82559;
				break;
	case FXP_REV_82559ERA:	str= "82559ER-A"; break;	/* 0x09 */
	case FXP_REV_82550_1:	str= "82550(1)"; break;		/* 0x0C */
	case FXP_REV_82550_2:	str= "82550(2)"; break;		/* 0x0D */
	case FXP_REV_82550_3:	str= "82550(3)"; break;		/* 0x0E */
	case FXP_REV_82551_1:	str= "82551(1)"; break;		/* 0x0F */
	case FXP_REV_82551_2:	str= "82551(2)"; break;		/* 0x10 */
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
static void fxp_conf_hw(fp)
fxp_t *fp;
{
	int i;
	int mwi, ext_stat1, ext_stat2, lim_fifo, i82503, fc;

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

	mwi= 0;		/* Do we want "Memory Write and Invalidate"? */
	ext_stat1= 0;	/* Do we want extended statistical counters? */
	ext_stat2= 0;	/* Do we want even more statistical counters? */
	lim_fifo= 0;	/* Limit number of frame in TX FIFO */
	i82503= 0;	/* Older 10 Mbps interface on the 82557 */
	fc= 0;		/* Flow control */

	switch(fp->fxp_type)
	{
	case FT_82557:
		if (i82503)
		{
			fp->fxp_conf_bytes[8] &= ~CCB8_503_MII;
			fp->fxp_conf_bytes[15] |= CCB15_CRSCDT;
		}
		break;
	case FT_82558A:
	case FT_82559:
		if (mwi)
			fp->fxp_conf_bytes[3] |= CCB3_MWIE;
		if (ext_stat1)
			fp->fxp_conf_bytes[6] &= ~CCB6_ESC;
		if (ext_stat2)
			fp->fxp_conf_bytes[6] &= ~CCB6_TCOSC;
		if (lim_fifo)
			fp->fxp_conf_bytes[7] |= CCB7_2FFIFO;
		if (fc)
		{
			/* From FreeBSD driver */
			fp->fxp_conf_bytes[16]= 0x1f;
			fp->fxp_conf_bytes[17]= 0x01;

			fp->fxp_conf_bytes[19] |= CCB19_FDRSTAFC |
				CCB19_FDRSTOFC;
		}

		fp->fxp_conf_bytes[18] |= CCB18_LROK;
		break;
	default:
		panic("FXP","fxp_conf_hw: bad device type", fp->fxp_type);
	}

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
	u32_t bus_addr;

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
		panic("FXP","sys_irqsetpolicy failed", r);

	fxp_reset_hw(fp);

	r= sys_irqenable(&fp->fxp_hook);
	if (r != OK)
		panic("FXP","sys_irqenable failed", r);

	/* Reset PHY? */

	fxp_do_conf(fp);

	/* Set pointer to statistical counters */
	r= sys_umap(SELF, D, (vir_bytes)&fp->fxp_stat, sizeof(fp->fxp_stat),
		&bus_addr);
	if (r != OK)
		panic("FXP","sys_umap failed", r);
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
	size_t rx_totbufsize, tx_totbufsize, tot_bufsize;
	phys_bytes buf;
	int i, r;
	struct rfd *rfdp;
	struct tx *txp;

	fp->fxp_rx_nbuf= N_RX_BUF;
	rx_totbufsize= fp->fxp_rx_nbuf * sizeof(struct rfd);
	fp->fxp_rx_bufsize= rx_totbufsize;

	fp->fxp_tx_nbuf= N_TX_BUF;
	tx_totbufsize= fp->fxp_tx_nbuf * sizeof(struct tx);
	fp->fxp_tx_bufsize= tx_totbufsize;

	tot_bufsize= tx_totbufsize + rx_totbufsize;

	/* What about memory allocation? */
	{
		static int first_time= 1;

		assert(first_time);
		first_time= 0;

#define BUFALIGN	4096
		assert(tot_bufsize <= sizeof(buffer)-BUFALIGN); 
		buf= (phys_bytes)buffer;
		buf += BUFALIGN - (buf % BUFALIGN);
	}

	fp->fxp_rx_buf= (struct rfd *)buf;
	r= sys_umap(SELF, D, (vir_bytes)buf, rx_totbufsize,
		&fp->fxp_rx_busaddr);
	if (r != OK)
		panic("FXP","sys_umap failed", r);
	for (i= 0, rfdp= fp->fxp_rx_buf; i<fp->fxp_rx_nbuf; i++, rfdp++)
	{
		rfdp->rfd_status= 0;
		rfdp->rfd_command= 0;
		if (i != fp->fxp_rx_nbuf-1)
		{
			r= sys_umap(SELF, D, (vir_bytes)&rfdp[1],
				sizeof(rfdp[1]), &rfdp->rfd_linkaddr);
			if (r != OK)
				panic("FXP","sys_umap failed", r);
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

	fp->fxp_tx_buf= (struct tx *)(buf+rx_totbufsize);
	r= sys_umap(SELF, D, (vir_bytes)fp->fxp_tx_buf,
		(phys_bytes)tx_totbufsize, &fp->fxp_tx_busaddr);
	if (r != OK)
		panic("FXP","sys_umap failed", r);

	for (i= 0, txp= fp->fxp_tx_buf; i<fp->fxp_tx_nbuf; i++, txp++)
	{
		txp->tx_status= 0;
		txp->tx_command= TXC_EL | CBL_NOP;	/* Just in case */
		if (i != fp->fxp_tx_nbuf-1)
		{
			r= sys_umap(SELF, D, (vir_bytes)&txp[1],
				(phys_bytes)sizeof(txp[1]),
				&txp->tx_linkaddr);
			if (r != OK)
				panic("FXP","sys_umap failed", r);
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
	tickdelay(MICROS_TO_TICKS(CSR_PORT_RESET_DELAY));

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
static void fxp_confaddr(fp)
fxp_t *fp;
{
	static char eakey[]= FXP_ENVVAR "#_EA";
	static char eafmt[]= "x:x:x:x:x:x";
	clock_t t0,t1;
	int i, r;
	port_t port;
	u32_t bus_addr;
	long v;
	struct ias ias;

	port= fp->fxp_base_port;

	/* User defined ethernet address? */
	eakey[sizeof(FXP_ENVVAR)-1]= '0' + (fp-fxp_table);

#if 0
	for (i= 0; i < 6; i++)
	{
		if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
			break;
		fp->fxp_address.ea_addr[i]= v;
	}
#else
	i= 0;
#endif

#if 0
	if (i != 0 && i != 6) env_panic(eakey);	/* It's all or nothing */
#endif

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
	ias.ias_status= 0;
	ias.ias_command= CBL_C_EL | CBL_AIS;
	ias.ias_linkaddr= 0;
	memcpy(ias.ias_ethaddr, fp->fxp_address.ea_addr,
		sizeof(ias.ias_ethaddr));
	r= sys_umap(SELF, D, (vir_bytes)&ias, (phys_bytes)sizeof(ias),
		&bus_addr);
	if (r != OK)
		panic("FXP","sys_umap failed", r);

	fxp_cu_ptr_cmd(fp, SC_CU_START, bus_addr, TRUE /* check idle */);

	getuptime(&t0);
	do {
		/* Wait for CU command to complete */
		if (ias.ias_status & CBL_F_C)
			break;
	} while (getuptime(&t1)==OK && (t1-t0) < MICROS_TO_TICKS(1000));

	if (!(ias.ias_status & CBL_F_C))
		panic("FXP","fxp_confaddr: CU command failed to complete", NO_NUM);
	if (!(ias.ias_status & CBL_F_OK))
		panic("FXP","fxp_confaddr: CU command failed", NO_NUM);

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
 *				fxp_writev				     *
 *===========================================================================*/
static void fxp_writev(mp, from_int, vectored)
message *mp;
int from_int;
int vectored;
{
	vir_bytes iov_src;
	int i, j, n, o, r, s, dl_port, count, size, prev_head;
	int fxp_client, fxp_tx_nbuf, fxp_tx_head;
	u16_t tx_command;
	fxp_t *fp;
	iovec_t *iovp;
	struct tx *txp, *prev_txp;

	dl_port = mp->DL_PORT;
	count = mp->DL_COUNT;
	if (dl_port < 0 || dl_port >= FXP_PORT_NR)
		panic("FXP","fxp_writev: illegal port", dl_port);
	fp= &fxp_table[dl_port];
	fxp_client= mp->DL_PROC;
	fp->fxp_client= fxp_client;

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

	if (vectored)
	{

		iov_src = (vir_bytes)mp->DL_ADDR;

		size= 0;
		o= 0;
		for (i= 0; i<count; i += IOVEC_NR,
			iov_src += IOVEC_NR * sizeof(fp->fxp_iovec[0]))
		{
			n= IOVEC_NR;
			if (i+n > count)
				n= count-i;
			r= sys_vircopy(fxp_client, D, iov_src, 
				SELF, D, (vir_bytes)fp->fxp_iovec,
				n * sizeof(fp->fxp_iovec[0]));
			if (r != OK)
				panic("FXP","fxp_writev: sys_vircopy failed", r);

			for (j= 0, iovp= fp->fxp_iovec; j<n; j++, iovp++)
			{
				s= iovp->iov_size;
				if (size + s > ETH_MAX_PACK_SIZE_TAGGED)
				{
					panic("FXP","fxp_writev: invalid packet size",
						NO_NUM);
				}

				r= sys_vircopy(fxp_client, D, iovp->iov_addr, 
					SELF, D, (vir_bytes)(txp->tx_buf+o),
					s);
				if (r != OK)
				{
					panic("FXP","fxp_writev: sys_vircopy failed",
						r);
				}
				size += s;
				o += s;
			}
		}
		if (size < ETH_MIN_PACK_SIZE)
			panic("FXP","fxp_writev: invalid packet size", size);
	}
	else
	{  
		size= mp->DL_COUNT;
		if (size < ETH_MIN_PACK_SIZE || size > ETH_MAX_PACK_SIZE_TAGGED)
			panic("FXP","fxp_writev: invalid packet size", size);

		r= sys_vircopy(fxp_client, D, (vir_bytes)mp->DL_ADDR, 
			SELF, D, (vir_bytes)txp->tx_buf, size);
		if (r != OK)
			panic("FXP","fxp_writev: sys_vircopy failed", r);
	}

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
	reply(fp, OK, FALSE);
	return;

suspend:
	if (from_int)
		panic("FXP","fxp: should not be sending\n", NO_NUM);

	fp->fxp_tx_mess= *mp;
	reply(fp, OK, FALSE);
}

/*===========================================================================*
 *				fxp_readv				     *
 *===========================================================================*/
static void fxp_readv(mp, from_int, vectored)
message *mp;
int from_int;
int vectored;
{
	int i, j, n, o, r, s, dl_port, fxp_client, count, size,
		fxp_rx_head, fxp_rx_nbuf;
	port_t port;
	unsigned packlen;
	vir_bytes iov_src;
	u16_t rfd_status;
	u16_t rfd_res;
	u8_t scb_status;
	fxp_t *fp;
	iovec_t *iovp;
	struct rfd *rfdp, *prev_rfdp;

	dl_port = mp->DL_PORT;
	count = mp->DL_COUNT;
	if (dl_port < 0 || dl_port >= FXP_PORT_NR)
		panic("FXP","fxp_readv: illegal port", dl_port);
	fp= &fxp_table[dl_port];
	fxp_client= mp->DL_PROC;
	fp->fxp_client= fxp_client;

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

	if (!rfd_status & RFDS_OK)
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

	if (vectored)
	{
		iov_src = (vir_bytes)mp->DL_ADDR;

		size= 0;
		o= 0;
		for (i= 0; i<count; i += IOVEC_NR,
			iov_src += IOVEC_NR * sizeof(fp->fxp_iovec[0]))
		{
			n= IOVEC_NR;
			if (i+n > count)
				n= count-i;
			r= sys_vircopy(fxp_client, D, iov_src, 
				SELF, D, (vir_bytes)fp->fxp_iovec,
				n * sizeof(fp->fxp_iovec[0]));
			if (r != OK)
				panic("FXP","fxp_readv: sys_vircopy failed", r);

			for (j= 0, iovp= fp->fxp_iovec; j<n; j++, iovp++)
			{
				s= iovp->iov_size;
				if (size + s > packlen)
				{
					assert(packlen > size);
					s= packlen-size;
				}

				r= sys_vircopy(SELF, D,
					(vir_bytes)(rfdp->rfd_buf+o),
					fxp_client, D, iovp->iov_addr, s);
				if (r != OK)
				{
					panic("FXP","fxp_readv: sys_vircopy failed",
						r);
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
	}
	else
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
		reply(fp, OK, FALSE);

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

	reply(fp, OK, FALSE);
}

/*===========================================================================*
 *				fxp_do_conf				     *
 *===========================================================================*/
static void fxp_do_conf(fp)
fxp_t *fp;
{
	int r;
	u32_t bus_addr;
	struct cbl_conf cc;
	clock_t t0,t1;

	/* Configure device */
	cc.cc_status= 0;
	cc.cc_command= CBL_C_EL | CBL_CONF;
	cc.cc_linkaddr= 0;
	memcpy(cc.cc_bytes, fp->fxp_conf_bytes, sizeof(cc.cc_bytes));

	r= sys_umap(SELF, D, (vir_bytes)&cc, (phys_bytes)sizeof(cc),
		&bus_addr);
	if (r != OK)
		panic("FXP","sys_umap failed", r);

	fxp_cu_ptr_cmd(fp, SC_CU_START, bus_addr, TRUE /* check idle */);

	getuptime(&t0);
	do {
		/* Wait for CU command to complete */
		if (cc.cc_status & CBL_F_C)
			break;
	} while (getuptime(&t1)==OK && (t1-t0) < MICROS_TO_TICKS(100000));

	if (!(cc.cc_status & CBL_F_C))
		panic("FXP","fxp_do_conf: CU command failed to complete", NO_NUM);
	if (!(cc.cc_status & CBL_F_OK))
		panic("FXP","fxp_do_conf: CU command failed", NO_NUM);

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
	clock_t t0,t1;
	port_t port;
	u8_t scb_cmd;

	port= fp->fxp_base_port;

	if (check_idle)
	{
		/* Consistency check. Make sure that CU is idle */
		if ((fxp_inb(port, SCB_STATUS) & SS_CUS_MASK) != SS_CU_IDLE)
			panic("FXP","fxp_cu_ptr_cmd: CU is not idle", NO_NUM);
	}

	fxp_outl(port, SCB_POINTER, bus_addr);
	fxp_outb(port, SCB_CMD, cmd);

	/* What is a reasonable time-out? There is nothing in the
	 * documentation. 1 ms should be enough.
	 */
	getuptime(&t0);
	do {
		/* Wait for CU command to be accepted */
		scb_cmd= fxp_inb(port, SCB_CMD);
		if ((scb_cmd & SC_CUC_MASK) == SC_CU_NOP)
			break;
	} while (getuptime(&t1)==OK && (t1-t0) < MICROS_TO_TICKS(100000));

	if ((scb_cmd & SC_CUC_MASK) != SC_CU_NOP)
		panic("FXP","fxp_cu_ptr_cmd: CU does not accept command", NO_NUM);
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
	clock_t t0,t1;
	port_t port;
	u8_t scb_cmd;

	port= fp->fxp_base_port;

	if (check_idle)
	{
		/* Consistency check, make sure that RU is idle */
		if ((fxp_inb(port, SCB_STATUS) & SS_RUS_MASK) != SS_RU_IDLE)
			panic("FXP","fxp_ru_ptr_cmd: RU is not idle", NO_NUM);
	}

	fxp_outl(port, SCB_POINTER, bus_addr);
	fxp_outb(port, SCB_CMD, cmd);

	getuptime(&t0);
	do {
		/* Wait for RU command to be accepted */
		scb_cmd= fxp_inb(port, SCB_CMD);
		if ((scb_cmd & SC_RUC_MASK) == SC_RU_NOP)
			break;
	} while (getuptime(&t1)==OK && (t1-t0) < MICROS_TO_TICKS(1000));

	if ((scb_cmd & SC_RUC_MASK) != SC_RU_NOP)
		panic("FXP","fxp_ru_ptr_cmd: RU does not accept command", NO_NUM);
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
		panic("FXP","fxp_restart_ru: RU is in an unexpected state", NO_NUM);

	fxp_ru_ptr_cmd(fp, SC_RU_START, fp->fxp_rx_busaddr,
		FALSE /* do not check idle */);
}

/*===========================================================================*
 *				fxp_getstat				     *
 *===========================================================================*/
static void fxp_getstat(mp)
message *mp;
{
	clock_t t0,t1;
	int dl_port;
	port_t port;
	fxp_t *fp;
	u32_t *p;
	eth_stat_t stats;

	dl_port = mp->DL_PORT;
	if (dl_port < 0 || dl_port >= FXP_PORT_NR)
		panic("FXP","fxp_getstat: illegal port", dl_port);
	fp= &fxp_table[dl_port];
	fp->fxp_client= mp->DL_PROC;

	assert(fp->fxp_mode == FM_ENABLED);
	assert(fp->fxp_flags & FF_ENABLED);

	port= fp->fxp_base_port;

	p= &fp->fxp_stat.sc_tx_fcp;
	*p= 0;

	/* The dump commmand doesn't take a pointer. Setting a pointer
	 * doesn't hard though.
	 */
	fxp_cu_ptr_cmd(fp, SC_CU_DUMP_SC, 0, FALSE /* do not check idle */);

	getuptime(&t0);
	do {
		/* Wait for CU command to complete */
		if (*p != 0)
			break;
	} while (getuptime(&t1)==OK && (t1-t0) < MICROS_TO_TICKS(1000));

	if (*p == 0)
		panic("FXP","fxp_getstat: CU command failed to complete", NO_NUM);
	if (*p != SCM_DSC)
		panic("FXP","fxp_getstat: bad magic", NO_NUM);

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

	put_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
		(vir_bytes) sizeof(stats), &stats);
	reply(fp, OK, FALSE);
}

/*===========================================================================*
 *				fxp_handler				     *
 *===========================================================================*/
static int fxp_handler(fp)
fxp_t *fp;
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

	return 1;
}

/*===========================================================================*
 *				fxp_check_ints				     *
 *===========================================================================*/
static void fxp_check_ints(fp)
fxp_t *fp;
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
		else if (fp->fxp_rx_mess.m_type == DL_READV)
		{
			fxp_readv(&fp->fxp_rx_mess, TRUE /* from int */,
				TRUE /* vectored */);
		}
		else
		{
			assert(fp->fxp_rx_mess.m_type == DL_READ);
			fxp_readv(&fp->fxp_rx_mess, TRUE /* from int */,
				FALSE /* !vectored */);
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
				if (fp->fxp_tx_mess.m_type == DL_WRITEV)
				{
					fxp_writev(&fp->fxp_tx_mess,
						TRUE /* from int */,
						TRUE /* vectored */);
				}
				else
				{
					assert(fp->fxp_tx_mess.m_type ==
						DL_WRITE);
					fxp_writev(&fp->fxp_tx_mess,
						TRUE /* from int */,
						FALSE /* !vectored */);
				}
			}
		}
		
	}
	if (fp->fxp_report_link)
		fxp_report_link(fp);

	if (fp->fxp_flags & (FF_PACK_SENT | FF_PACK_RECV))
		reply(fp, OK, TRUE);
}

/*===========================================================================*
 *				fxp_watchdog_f				     *
 *===========================================================================*/
static void fxp_watchdog_f(tp)
timer_t *tp;
{
	int i;
	fxp_t *fp;

	tmr_arg(&fxp_watchdog)->ta_int= 0;
	fxp_set_timer(&fxp_watchdog, HZ, fxp_watchdog_f);

	for (i= 0, fp = &fxp_table[0]; i<FXP_PORT_NR; i++, fp++)
	{
		if (fp->fxp_mode != FM_ENABLED)
			continue;

		/* Handle race condition, MII interface mgith be busy */
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
			continue;
		}
		if (fp->fxp_tx_alive)
		{
			fp->fxp_tx_alive= FALSE;
			continue;
		}

		fp->fxp_need_reset= TRUE;
		fp->fxp_got_int= TRUE;
		interrupt(fxp_tasknr);
	}
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
static void fxp_report_link(fp)
fxp_t *fp;
{
	port_t port;
	u16_t mii_ctrl, mii_status, mii_id1, mii_id2, 
		mii_ana, mii_anlpa, mii_ane, mii_extstat,
		mii_ms_ctrl, mii_ms_status, scr;
	u32_t oui;
	int model, rev;
	int f, link_up, ms_regs;

	/* Assume an 82555 (compatible) PHY. The should be changed for
	 * 82557 NICs with different PHYs
	 */
	ms_regs= 0;	/* No master/slave registers. */

	fp->fxp_report_link= FALSE;
	port= fp->fxp_base_port;

	scr= mii_read(fp, MII_SCR);
	scr &= ~(MII_SCR_RES|MII_SCR_RES_1);
	fp->fxp_mii_scr= scr;

	mii_ctrl= mii_read(fp, MII_CTRL);
	mii_read(fp, MII_STATUS); /* Read the status register twice, why? */
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
	if (ms_regs)
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

	if (ms_regs)
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
 *				fxp_stop				     *
 *===========================================================================*/
static void fxp_stop()
{
	int i;
	port_t port;
	fxp_t *fp;

	for (i= 0, fp= &fxp_table[0]; i<FXP_PORT_NR; i++, fp++)
	{
		if (fp->fxp_mode != FM_ENABLED)
			continue;
		if (!(fp->fxp_flags & FF_ENABLED))
			continue;
		port= fp->fxp_base_port;

		/* Reset device */
		if (debug)
			printf("%s: resetting device\n", fp->fxp_name);
		fxp_outl(port, CSR_PORT, CP_CMD_SOFT_RESET);
	}
	sys_exit(0);
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
static void reply(fp, err, may_block)
fxp_t *fp;
int err;
int may_block;
{
	message reply;
	int status;
	int r;

	status = 0;
	if (fp->fxp_flags & FF_PACK_SENT)
		status |= DL_PACK_SEND;
	if (fp->fxp_flags & FF_PACK_RECV)
		status |= DL_PACK_RECV;

	reply.m_type = DL_TASK_REPLY;
	reply.DL_PORT = fp - fxp_table;
	reply.DL_PROC = fp->fxp_client;
	reply.DL_STAT = status | ((u32_t) err << 16);
	reply.DL_COUNT = fp->fxp_read_s;
#if 0
	reply.DL_CLCK = get_uptime();
#else
	reply.DL_CLCK = 0;
#endif

	r= send(fp->fxp_client, &reply);

	if (r == ELOCKED && may_block)
	{
#if 0
		printW(); printf("send locked\n");
#endif
		return;
	}

	if (r < 0)
		panic("FXP","fxp: send failed:", r);
	
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
	if (send(req->m_source, reply_mess) != OK)
		panic("FXP","fxp: unable to mess_reply", NO_NUM);
}

/*===========================================================================*
 *				put_userdata				     *
 *===========================================================================*/
static void put_userdata(user_proc, user_addr, count, loc_addr)
int user_proc;
vir_bytes user_addr;
vir_bytes count;
void *loc_addr;
{
	int r;

	r= sys_vircopy(SELF, D, (vir_bytes)loc_addr,
		user_proc, D, user_addr, count);
	if (r != OK)
		panic("FXP","put_userdata: sys_vircopy failed", r);
}

/*===========================================================================*
 *				eeprom_read				     *
 *===========================================================================*/
PRIVATE u16_t eeprom_read(fp, reg)
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
PRIVATE void eeprom_addrsize(fp)
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
		panic("FXP","eeprom_addrsize: failed", NO_NUM);
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
PRIVATE u16_t mii_read(fp, reg)
fxp_t *fp;
int reg;
{
	clock_t t0,t1;
	port_t port;
	u32_t v;

	port= fp->fxp_base_port;

	assert(!fp->fxp_mii_busy);
	fp->fxp_mii_busy++;

	if (!(fxp_inl(port, CSR_MDI_CTL) & CM_READY))
		panic("FXP","mii_read: MDI not ready", NO_NUM);
	fxp_outl(port, CSR_MDI_CTL, CM_READ | (1 << CM_PHYADDR_SHIFT) |
		(reg << CM_REG_SHIFT));

	getuptime(&t0);
	do {
		v= fxp_inl(port, CSR_MDI_CTL);
		if (v & CM_READY)
			break;
	} while (getuptime(&t1)==OK && (t1-t0) < MICROS_TO_TICKS(100000));

	if (!(v & CM_READY))
		panic("FXP","mii_read: MDI not ready after command", NO_NUM);

	fp->fxp_mii_busy--;
	assert(!fp->fxp_mii_busy);

	return v & CM_DATA_MASK;
}

/*===========================================================================*
 *				fxp_set_timer				     *
 *===========================================================================*/
PRIVATE void fxp_set_timer(tp, delta, watchdog)
timer_t *tp;				/* timer to be set */
clock_t delta;				/* in how many ticks */
tmr_func_t watchdog;			/* watchdog function to be called */
{
	clock_t now;				/* current time */
	int r;

	/* Get the current time. */
	r= getuptime(&now);
	if (r != OK)
		panic("FXP","unable to get uptime from clock", r);

	/* Add the timer to the local timer queue. */
	tmrs_settimer(&fxp_timers, tp, now + delta, watchdog, NULL);

	/* Possibly reschedule an alarm call. This happens when a new timer
	 * is added in front. 
	 */
	if (fxp_next_timeout == 0 || 
		fxp_timers->tmr_exp_time < fxp_next_timeout)
	{
		fxp_next_timeout= fxp_timers->tmr_exp_time; 
#if VERBOSE
		printf("fxp_set_timer: calling sys_setalarm for %d (now+%d)\n",
			fxp_next_timeout, fxp_next_timeout-now);
#endif
		r= sys_setalarm(fxp_next_timeout, 1);
		if (r != OK)
			panic("FXP","unable to set synchronous alarm", r);
	}
}

/*===========================================================================*
 *				fxp_expire_tmrs				     *
 *===========================================================================*/
PRIVATE void fxp_expire_timers()
{
/* A synchronous alarm message was received. Check if there are any expired 
 * timers. Possibly reschedule the next alarm.  
 */
  clock_t now;				/* current time */
  timer_t *tp;
  int r;

  /* Get the current time to compare the timers against. */
  r= getuptime(&now);
  if (r != OK)
 	panic("FXP","Unable to get uptime from clock.", r);

  /* Scan the timers queue for expired timers. Dispatch the watchdog function
   * for each expired timers. Possibly a new alarm call must be scheduled.
   */
  tmrs_exptimers(&fxp_timers, now, NULL);
  if (fxp_timers == NULL)
  	fxp_next_timeout= TMR_NEVER;
  else
  {  					  /* set new alarm */
  	fxp_next_timeout = fxp_timers->tmr_exp_time;
  	r= sys_setalarm(fxp_next_timeout, 1);
  	if (r != OK)
 		panic("FXP","Unable to set synchronous alarm.", r);
  }
}

static void micro_delay(unsigned long usecs)
{
	tickdelay(MICROS_TO_TICKS(usecs));
}

static u8_t do_inb(port_t port)
{
	int r;
	u8_t value;

	r= sys_inb(port, &value);
	if (r != OK)
		panic("FXP","sys_inb failed", r);
	return value;
}

static u32_t do_inl(port_t port)
{
	int r;
	u32_t value;

	r= sys_inl(port, &value);
	if (r != OK)
		panic("FXP","sys_inl failed", r);
	return value;
}

static void do_outb(port_t port, u8_t value)
{
	int r;

	r= sys_outb(port, value);
	if (r != OK)
		panic("FXP","sys_outb failed", r);
}

static void do_outl(port_t port, u32_t value)
{
	int r;

	r= sys_outl(port, value);
	if (r != OK)
		panic("FXP","sys_outl failed", r);
}

/*
 * $PchId: fxp.c,v 1.4 2005/01/31 22:10:37 philip Exp $
 */

