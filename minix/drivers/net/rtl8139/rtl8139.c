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

static re_t re_state;

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

static int rl_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks);
static int rl_probe(re_t *rep, unsigned int skip);
static void rl_init_buf(re_t *rep);
static void rl_init_hw(re_t *rep, netdriver_addr_t *addr,
	unsigned int instance);
static void rl_reset_hw(re_t *rep);
static void rl_set_hwaddr(const netdriver_addr_t *addr);
static void rl_confaddr(re_t *rep, netdriver_addr_t *addr,
	unsigned int instance);
static void rl_stop(void);
static void rl_rec_mode(re_t *rep);
static void rl_set_mode(unsigned int mode, const netdriver_addr_t *mcast_list,
	unsigned int mcast_count);
static ssize_t rl_recv(struct netdriver_data *data, size_t max);
static int rl_send(struct netdriver_data *data, size_t size);
static unsigned int rl_get_link(uint32_t *media);
static void rl_intr(unsigned int mask);
static void rl_check_ints(re_t *rep);
#if VERBOSE
static void rl_report_link(re_t *rep);
static void mii_print_techab(u16_t techab);
static void mii_print_stat_speed(u16_t stat, u16_t extstat);
#endif
static void rl_clear_rx(re_t *rep);
static void rl_do_reset(re_t *rep);
static void rl_other(const message *m_ptr, int ipc_status);
static void rl_dump(void);
#if 0
static void dump_phy(re_t *rep);
#endif
static int rl_handler(re_t *rep);
static void rl_tick(void);
static void tell_iommu(vir_bytes start, size_t size, int pci_bus, int
	pci_dev, int pci_func);

static const struct netdriver rl_table = {
	.ndr_name	= "rl",
	.ndr_init	= rl_init,
	.ndr_stop	= rl_stop,
	.ndr_set_mode	= rl_set_mode,
	.ndr_set_hwaddr	= rl_set_hwaddr,
	.ndr_recv	= rl_recv,
	.ndr_send	= rl_send,
	.ndr_get_link	= rl_get_link,
	.ndr_intr	= rl_intr,
	.ndr_tick	= rl_tick,
	.ndr_other	= rl_other,
};

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char *argv[])
{

	env_setargs(argc, argv);

	netdriver_task(&rl_table);

	return 0;
}

/*===========================================================================*
 *				rl_intr					     *
 *===========================================================================*/
static void rl_intr(unsigned int __unused mask)
{
	re_t *rep;
	int s;

	rep = &re_state;

	/* Run interrupt handler at driver level. */
	rl_handler(rep);

	/* Reenable interrupts for this hook. */
	if ((s = sys_irqenable(&rep->re_hook_id)) != OK)
		printf("RTL8139: error, couldn't enable interrupts: %d\n", s);

	/* Perform tasks based on the flagged conditions. */
	rl_check_ints(rep);
}

/*===========================================================================*
 *				rl_other				     *
 *===========================================================================*/
static void rl_other(const message *m_ptr, int ipc_status)
{
	if (is_ipc_notify(ipc_status) && m_ptr->m_source == TTY_PROC_NR)
		rl_dump();
}

/*===========================================================================*
 *				rl_stop					     *
 *===========================================================================*/
static void rl_stop(void)
{
	re_t *rep;

	rep = &re_state;

	rl_outb(rep->re_base_port, RL_CR, 0);
}

/*===========================================================================*
 *				rl_dump					     *
 *===========================================================================*/
static void rl_dump(void)
{
	re_t *rep;

	rep= &re_state;

	printf("\n");
	printf("Realtek RTL 8139 device %s:\n", netdriver_name());

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
 *				rl_set_mode				     *
 *===========================================================================*/
static void rl_set_mode(unsigned int mode, const netdriver_addr_t *mcast_list,
	unsigned int mcast_count)
{
	re_t *rep;

	rep= &re_state;

	rep->re_mode = mode;

	rl_rec_mode(rep);
}

/*===========================================================================*
 *				rl_init					     *
 *===========================================================================*/
static int rl_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks)
{
/* Initialize the rtl8139 driver. */
	re_t *rep;
#if RTL8139_FKEY
	int r, fkeys, sfkeys;
#endif

	/* Initialize driver state. */
	rep= &re_state;
	memset(rep, 0, sizeof(*rep));

	rep->re_link_up= -1;	/* Unknown */
	rep->re_ertxth= RL_TSD_ERTXTH_8;

	/* Try to find a matching device. */
	if (!rl_probe(rep, instance))
		return ENXIO;

	/* Claim buffer memory. */
	rl_init_buf(rep);

	/* Initialize the device we found. */
	rl_init_hw(rep, addr, instance);

#if VERBOSE
	/* Report initial link status. */
	rl_report_link(rep);
#endif

#if RTL8139_FKEY
	/* Observe some function key for debug dumps. */
	fkeys = sfkeys = 0; bit_set(sfkeys, 9);
	if ((r = fkey_map(&fkeys, &sfkeys)) != OK)
	    printf("Warning: RTL8139 couldn't observe Shift+F9 key: %d\n",r);
#endif

	*caps = NDEV_CAP_MCAST | NDEV_CAP_BCAST | NDEV_CAP_HWADDR;
	*ticks = sys_hz();
	return OK;
}

/*===========================================================================*
 *				rl_probe				     *
 *===========================================================================*/
static int rl_probe(re_t *rep, unsigned int skip)
{
	int r, devind;
	u16_t cr, vid, did;
	u32_t bar;
	u8_t ilr;
#if VERBOSE
	const char *dname;
#endif

	pci_init();

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
	printf("%s: ", netdriver_name());
	printf("%s (%x/%x) at %s\n", dname, vid, did, pci_slot_name(devind));
#endif
	pci_reserve(devind);

	/* Enable bus mastering if necessary. */
	cr = pci_attr_r16(devind, PCI_CR);
	/* printf("cr = 0x%x\n", cr); */
	if (!(cr & PCI_CR_MAST_EN))
		pci_attr_w16(devind, PCI_CR, cr | PCI_CR_MAST_EN);

	bar= pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;
	if (bar < 0x400) {
		panic("base address is not properly configured");
	}
	rep->re_base_port= bar;

	ilr= pci_attr_r8(devind, PCI_ILR);
	rep->re_irq= ilr;
#if VERBOSE
	printf("%s: using I/O address 0x%lx, IRQ %d\n",
		netdriver_name(), (unsigned long)bar, ilr);
#endif

	return TRUE;
}

/*===========================================================================*
 *				rl_init_buf				     *
 *===========================================================================*/
static void rl_init_buf(re_t *rep)
{
	size_t rx_bufsize, tx_bufsize, tot_bufsize;
	phys_bytes buf;
	char *mallocbuf;
	int i, off;

	/* Allocate receive and transmit buffers */
	tx_bufsize= NDEV_ETH_PACKET_MAX_TAGGED;
	if (tx_bufsize % 4)
		tx_bufsize += 4-(tx_bufsize % 4);	/* Align */
	rx_bufsize= RX_BUFSIZE;
	tot_bufsize= N_TX_BUF*tx_bufsize + rx_bufsize;

	if (tot_bufsize % 4096)
		tot_bufsize += 4096-(tot_bufsize % 4096);

#define BUF_ALIGNMENT (64*1024)

	if (!(mallocbuf = alloc_contig(BUF_ALIGNMENT + tot_bufsize, 0, &buf)))
		panic("Couldn't allocate kernel buffer");

	/* click-align mallocced buffer. this is what we used to get
	 * from kmalloc() too.
	 */
	if((off = buf % BUF_ALIGNMENT)) {
		mallocbuf += BUF_ALIGNMENT - off;
		buf += BUF_ALIGNMENT - off;
	}

	tell_iommu((vir_bytes)mallocbuf, tot_bufsize, 0, 0, 0);

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
static void rl_init_hw(re_t *rep, netdriver_addr_t *addr,
	unsigned int instance)
{
#if VERBOSE
	int i;
#endif
	int s;

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
		printf("%s: model %s\n", netdriver_name(), rep->re_model);
	} else
	{
		printf("%s: unknown model 0x%08x\n",
			netdriver_name(),
			rl_inl(rep->re_base_port, RL_TCR) &
			(RL_TCR_HWVER_AM | RL_TCR_HWVER_BM));
	}
#endif

	rl_confaddr(rep, addr, instance);

#if VERBOSE
	printf("%s: Ethernet address ", netdriver_name());
	for (i= 0; i < 6; i++)
		printf("%x%c", addr->na_addr[i], i < 5 ? ':' : '\n');
#endif
}

/*===========================================================================*
 *				rl_reset_hw				     *
 *===========================================================================*/
static void rl_reset_hw(re_t *rep)
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

	rep->re_tx_busy = 0;

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
 *				rl_set_hwaddr				     *
 *===========================================================================*/
static void rl_set_hwaddr(const netdriver_addr_t *addr)
{
	re_t *rep;
	port_t port;
	u32_t w;
	int i;

	rep = &re_state;

	port= rep->re_base_port;
	rl_outb(port, RL_9346CR, RL_9346CR_EEM_CONFIG);
	w= 0;
	for (i= 0; i<4; i++)
		w |= (addr->na_addr[i] << (i*8));
	rl_outl(port, RL_IDR, w);
	w= 0;
	for (i= 4; i<6; i++)
		w |= (addr->na_addr[i] << ((i-4)*8));
	rl_outl(port, RL_IDR+4, w);
	rl_outb(port, RL_9346CR, RL_9346CR_EEM_NORMAL);
}

/*===========================================================================*
 *				rl_confaddr				     *
 *===========================================================================*/
static void rl_confaddr(re_t *rep, netdriver_addr_t *addr,
	unsigned int instance)
{
	static char eakey[]= RL_ENVVAR "#_EA";
	static char eafmt[]= "x:x:x:x:x:x";
	port_t port;
	int i;
	long v;

	/* User defined ethernet address? */
	eakey[sizeof(RL_ENVVAR)-1]= '0' + instance;

	for (i= 0; i < 6; i++)
	{
		if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
			break;
		addr->na_addr[i]= v;
	}

	if (i != 0 && i != 6) env_panic(eakey);	/* It's all or nothing */

	/* Should update ethernet address in hardware */
	if (i == 6)
		rl_set_hwaddr(addr);

	/* Get ethernet address */
	port= rep->re_base_port;

	for (i= 0; i<6; i++)
		addr->na_addr[i]= rl_inb(port, RL_IDR+i);
}

/*===========================================================================*
 *				rl_rec_mode				     *
 *===========================================================================*/
static void rl_rec_mode(re_t *rep)
{
	port_t port;
	u32_t rcr;

	port= rep->re_base_port;
	rcr= rl_inl(port, RL_RCR);
	rcr &= ~(RL_RCR_AB|RL_RCR_AM|RL_RCR_APM|RL_RCR_AAP);
	if (rep->re_mode & NDEV_MODE_PROMISC)
		rcr |= RL_RCR_AB | RL_RCR_AM | RL_RCR_AAP;
	if (rep->re_mode & NDEV_MODE_BCAST)
		rcr |= RL_RCR_AB;
	if (rep->re_mode & (NDEV_MODE_MCAST_LIST | NDEV_MODE_MCAST_ALL))
		rcr |= RL_RCR_AM;
	rcr |= RL_RCR_APM;

	rl_outl(port, RL_RCR, rcr);
}

/*===========================================================================*
 *				rl_recv					     *
 *===========================================================================*/
static ssize_t rl_recv(struct netdriver_data *data, size_t max)
{
	int o, s;
	port_t port;
	unsigned amount, totlen, packlen;
	u16_t d_start, d_end;
	u32_t l, rxstat;
	re_t *rep;

	rep= &re_state;

	if (rep->re_clear_rx)
		return SUSPEND;	/* Buffer overflow */

	port= rep->re_base_port;

	if (rl_inb(port, RL_CR) & RL_CR_BUFE)
	{
		/* Receive buffer is empty, suspend */
		return SUSPEND;
	}

	d_start= rl_inw(port, RL_CAPR) + RL_CAPR_DATA_OFF;
	d_end= rl_inw(port, RL_CBR) % RX_BUFSIZE;

#if RX_BUFSIZE <= USHRT_MAX
	if (d_start >= RX_BUFSIZE)
	{
		printf("rl_recv: strange value in RL_CAPR: 0x%x\n",
			rl_inw(port, RL_CAPR));
		d_start %= RX_BUFSIZE;
	}
#endif

	if (d_end > d_start)
		amount= d_end-d_start;
	else
		amount= d_end+RX_BUFSIZE - d_start;

	rxstat = *(u32_t *) (rep->v_re_rx_buf + d_start);

	/* Should convert from little endian to host byte order */

	if (!(rxstat & RL_RXS_ROK))
	{
		printf("rxstat = 0x%08x\n", rxstat);
		printf("d_start: 0x%x, d_end: 0x%x, rxstat: 0x%x\n",
			d_start, d_end, rxstat);
		panic("received packet not OK");
	}
	totlen= (rxstat >> RL_RXS_LEN_S);
	if (totlen < 8 || totlen > 2*NDEV_ETH_PACKET_MAX)
	{
		/* Someting went wrong */
		printf(
		"rl_recv: bad length (%u) in status 0x%08x at offset 0x%x\n",
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
		printf("rl_recv: packet not yet ready\n");
		return SUSPEND;
	}

	/* Should subtract the CRC */
	packlen = MIN(totlen - NDEV_ETH_PACKET_CRC, max);

	/* Copy out the data.  The packet may wrap in the receive buffer. */
	o = (d_start+4) % RX_BUFSIZE;
	s = MIN(RX_BUFSIZE - o, (int)packlen);

	netdriver_copyout(data, 0, rep->v_re_rx_buf + o, s);
	if (s < (int)packlen)
		netdriver_copyout(data, s, rep->v_re_rx_buf, packlen - s);

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

	return packlen;
}

/*===========================================================================*
 *				rl_send					     *
 *===========================================================================*/
static int rl_send(struct netdriver_data *data, size_t size)
{
	int tx_head;
	re_t *rep;

	rep= &re_state;

	tx_head= rep->re_tx_head;
	if (rep->re_tx[tx_head].ret_busy)
		return SUSPEND;

	netdriver_copyin(data, 0, rep->re_tx[tx_head].v_ret_buf, size);

	rl_outl(rep->re_base_port, RL_TSD0+tx_head*4, rep->re_ertxth | size);
	rep->re_tx[tx_head].ret_busy= TRUE;
	rep->re_tx_busy++;

	if (++tx_head == N_TX_BUF)
		tx_head= 0;
	assert(tx_head < RL_N_TX);
	rep->re_tx_head= tx_head;

	return OK;
}

/*===========================================================================*
 *				rl_check_ints				     *
 *===========================================================================*/
static void rl_check_ints(re_t *rep)
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

	if (!rep->re_got_int)
		return;
	rep->re_got_int = FALSE;

	netdriver_recv();

	if (rep->re_clear_rx)
		rl_clear_rx(rep);

	if (rep->re_need_reset)
		rl_do_reset(rep);

	if (rep->re_send_int) {
		rep->re_send_int = FALSE;

		netdriver_send();
	}

	if (rep->re_report_link) {
		rep->re_report_link = FALSE;

		netdriver_link();
#if VERBOSE
		rl_report_link(rep);
#endif
	}
}

/*===========================================================================*
 *				rl_get_link				     *
 *===========================================================================*/
static unsigned int rl_get_link(uint32_t *media)
{
	port_t port;
	u8_t msr;
	u16_t mii_ctrl;
	re_t *rep;

	rep = &re_state;

	port= rep->re_base_port;
	msr= rl_inb(port, RL_MSR);

	if (msr & RL_MSR_LINKB)
		return NDEV_LINK_DOWN;

	if (msr & RL_MSR_SPEED_10)
		*media = IFM_ETHER | IFM_10_T;
	else
		*media = IFM_ETHER | IFM_100_TX;

	mii_ctrl= rl_inw(port, RL_BMCR);
	if (mii_ctrl & MII_CTRL_DM)
		*media |= IFM_FDX;
	else
		*media |= IFM_HDX;

	return NDEV_LINK_UP;
}

#if VERBOSE
/*===========================================================================*
 *				rl_report_link				     *
 *===========================================================================*/
static void rl_report_link(re_t *rep)
{
	port_t port;
	u16_t mii_ctrl, mii_status, mii_ana, mii_anlpa, mii_ane, mii_extstat;
	u8_t msr;
	int f, link_up;

	port= rep->re_base_port;
	msr= rl_inb(port, RL_MSR);
	link_up= !(msr & RL_MSR_LINKB);
	rep->re_link_up= link_up;
	if (!link_up)
	{
		printf("%s: link down\n", netdriver_name());
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
		printf("%s: PHY: ", netdriver_name());
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
		printf("%s: manual config: ", netdriver_name());
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

#if VERBOSE
	printf("%s: ", netdriver_name());
	mii_print_stat_speed(mii_status, mii_extstat);
	printf("\n");

	if (!(mii_status & MII_STATUS_ANC))
		printf("%s: auto-negotiation not complete\n",
		    netdriver_name());
	if (mii_status & MII_STATUS_RF)
		printf("%s: remote fault detected\n", netdriver_name());
	if (!(mii_status & MII_STATUS_ANA))
	{
		printf("%s: local PHY has no auto-negotiation ability\n",
			netdriver_name());
	}
	if (!(mii_status & MII_STATUS_LS))
		printf("%s: link down\n", netdriver_name());
	if (mii_status & MII_STATUS_JD)
		printf("%s: jabber condition detected\n",
		    netdriver_name());
	if (!(mii_status & MII_STATUS_EC))
	{
		printf("%s: no extended register set\n", netdriver_name());
		goto resspeed;
	}
	if (!(mii_status & MII_STATUS_ANC))
		goto resspeed;

	printf("%s: local cap.: ", netdriver_name());
	mii_print_techab(mii_ana);
	printf("\n");

	if (mii_ane & MII_ANE_PDF)
		printf("%s: parallel detection fault\n", netdriver_name());
	if (!(mii_ane & MII_ANE_LPANA))
	{
		printf("%s: link-partner does not support auto-negotiation\n",
			netdriver_name());
		goto resspeed;
	}

	printf("%s: remote cap.: ", netdriver_name());
	mii_print_techab(mii_anlpa);
	printf("\n");
resspeed:
#endif

	printf("%s: ", netdriver_name());
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
#endif /* VERBOSE */

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

	netdriver_stat_ierror(1);
}

/*===========================================================================*
 *				rl_do_reset				     *
 *===========================================================================*/
static void rl_do_reset(re_t *rep)
{
	rep->re_need_reset= FALSE;
	rl_reset_hw(rep);
	rl_rec_mode(rep);

	rep->re_tx_head= 0;
	if (rep->re_tx[rep->re_tx_head].ret_busy)
		rep->re_tx_busy--;
	rep->re_tx[rep->re_tx_head].ret_busy= FALSE;
	rep->re_send_int= TRUE;
}

#if 0
/*===========================================================================*
 *				dump_phy				     *
 *===========================================================================*/
static void dump_phy(re_t *rep)
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
 *				rl_handler				     *
 *===========================================================================*/
static int rl_handler(re_t *rep)
{
	int i, port, tx_head, tx_tail, link_up;
	u16_t isr, tsad;
	u32_t tsd, tcr, ertxth;

	port= rep->re_base_port;

	/* Ack interrupt */
	isr= rl_inw(port, RL_ISR);
	rl_outw(port, RL_ISR, isr);

	if (isr & RL_IMR_FOVW)
	{
		isr &= ~RL_IMR_FOVW;
		/* Should do anything? */
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
		}
	}
	if (isr & RL_IMR_RXOVW)
	{
		isr &= ~RL_IMR_RXOVW;

		/* Clear the receive buffer */
		rep->re_clear_rx= TRUE;
		rep->re_got_int= TRUE;
	}

	if (isr & (RL_ISR_RER | RL_ISR_ROK))
	{
		isr &= ~(RL_ISR_RER | RL_ISR_ROK);

		rep->re_got_int= TRUE;
	}
	if ((isr & (RL_ISR_TER | RL_ISR_TOK)) || 1)
	{
		isr &= ~(RL_ISR_TER | RL_ISR_TOK);

		tsad= rl_inw(port, RL_TSAD);
		if (tsad & (RL_TSAD_TABT0|RL_TSAD_TABT1|
			RL_TSAD_TABT2|RL_TSAD_TABT3))
		{
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
			netdriver_stat_oerror(1);

			tcr= rl_inl(port, RL_TCR);
			rl_outl(port, RL_TCR, tcr | RL_TCR_CLRABT);
		}

		/* Transmit completed */
		tx_head= rep->re_tx_head;
		tx_tail= rep->re_tx_tail;
		for (i= 0; i< 2*N_TX_BUF; i++)
		{
			if (rep->re_tx_busy == 0)
				break;
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
			if (!(tsd & (RL_TSD_TABT | RL_TSD_TOK | RL_TSD_TUN)))
			{
				/* Buffer is not yet ready */
				break;
			}

			/* Should collect statistics */
			if (tsd & RL_TSD_TABT)
			{
				printf("rl_handler, TABT, TSD%d = 0x%04x\n",
					tx_tail, tsd);
				panic("TX abort"); /* CLRABT is not all that
						    * that effective, why not?
						    */
				tcr= rl_inl(port, RL_TCR);
				rl_outl(port, RL_TCR, tcr | RL_TCR_CLRABT);
			}

			/* What about collisions? */
			if (!(tsd & RL_TSD_TOK))
				netdriver_stat_oerror(1);
			if (tsd & RL_TSD_TUN)
			{
				/* Increase ERTXTH */
				ertxth= tsd + (1 << RL_TSD_ERTXTH_S);
				ertxth &= RL_TSD_ERTXTH_M;
#if VERBOSE
				if (ertxth > rep->re_ertxth)
				{
					printf("%s: new ertxth: %d bytes\n",
						netdriver_name(),
						(ertxth >> RL_TSD_ERTXTH_S) *
						32);
					rep->re_ertxth= ertxth;
				}
#endif
			}
			rep->re_tx[tx_tail].ret_busy= FALSE;
			rep->re_tx_busy--;

#if 0
			printf("TSD%d: %08lx\n", tx_tail, tsd);
			printf(
			"rl_handler: head %d, tail %d, busy: %d %d %d %d\n",
				tx_head, tx_tail,
				rep->re_tx[0].ret_busy, rep->re_tx[1].ret_busy,
				rep->re_tx[2].ret_busy, rep->re_tx[3].ret_busy);
#endif

			if (++tx_tail >= N_TX_BUF)
				tx_tail= 0;
			assert(tx_tail < RL_N_TX);
			rep->re_tx_tail= tx_tail;

			rep->re_send_int= TRUE;
			rep->re_got_int= TRUE;
			rep->re_tx_alive= TRUE;
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
 *				rl_tick					     *
 *===========================================================================*/
static void rl_tick(void)
{
	re_t *rep;

	rep= &re_state;

	assert(rep->re_tx_busy >= 0 && rep->re_tx_busy <= N_TX_BUF);
	if (rep->re_tx_busy == 0)
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
	printf("%s: TX timeout, resetting\n", netdriver_name());
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

	rl_check_ints(rep);
}

/* TODO: obviously this needs a lot of work. */
static void tell_iommu(vir_bytes buf, size_t size, int pci_bus, int pci_dev,
	int pci_func)
{
	int r;
	endpoint_t dev_e;
	message m;

	r= ds_retrieve_label_endpt("amddev", &dev_e);
	if (r != OK)
	{
#if 0
		printf("rtl8139`tell_dev: ds_retrieve_label_endpt failed "
		    "for 'amddev': %d\n", r);
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
