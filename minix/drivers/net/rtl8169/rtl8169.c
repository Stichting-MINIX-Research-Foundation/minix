/*
 * rtl8169.c
 *
 * This file contains a ethernet device driver for Realtek rtl8169 based
 * ethernet cards.
 *
 */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <machine/pci.h>
#include <assert.h>

#include "rtl8169.h"

#define VERBOSE		0		/* display message during init */

#define RE_DTCC_VALUE	600		/* DTCC Update once every 10 minutes */

#define RX_CONFIG_MASK	0xff7e1880	/* Clears the bits supported by chip */

#define RE_INTR_MASK	(RL_IMR_TDU | RL_IMR_FOVW | RL_IMR_PUN | RL_IMR_RDU | \
			 RL_IMR_TER | RL_IMR_TOK | RL_IMR_RER | RL_IMR_ROK)

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
	u16_t	FAE;		/* Frame Alignment Error packets (MII only) */
	u32_t	Tx1Col;		/* Tx Ok packets with 1 collision before Tx */
	u32_t	TxMCol;		/* Tx Ok packets with 2..15 collisions */
	u32_t	RxOkPhy_low;	/* low 32-bits of Rx Ok packets for us */
	u32_t	RxOkPhy_high;	/* high 32-bits of Rx Ok packets for us */
	u32_t	RxOkBrd_low;	/* low 32-bits of Rx Ok broadcast packets */
	u32_t	RxOkBrd_high;	/* high 32-bits of Rx Ok broadcast packets */
	u32_t	RxOkMul;	/* Rx Ok multicast packets */
	u16_t	TxAbt;		/* Tx abort packets */
	u16_t	TxUndrn;	/* Tx underrun packets */
} re_dtcc;

typedef struct re {
	port_t re_base_port;
	int re_irq;
	int re_mode;
	int re_link_up;
	int re_got_int;
	int re_send_int;
	int re_report_link;
	int re_need_reset;
	int re_tx_alive;
	u32_t re_mac;
	const char *re_model;

	/* Rx */
	int re_rx_head;
	struct {
		phys_bytes ret_buf;
		char *v_ret_buf;
	} re_rx[N_RX_DESC];

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
	int re_tx_busy;		/* how many Tx descriptors are busy? */

	int re_hook_id;		/* IRQ hook id at kernel */
	phys_bytes dtcc_buf;	/* Dump Tally Counter buffer physical */
	re_dtcc *v_dtcc_buf;	/* Dump Tally Counter buffer */
	u32_t dtcc_counter;	/* DTCC update counter */
	u32_t interrupts;
} re_t;

static re_t re_state;

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

static int rl_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks);
static int rl_probe(re_t *rep, unsigned int skip);
static void rl_init_buf(re_t *rep);
static void rl_init_hw(re_t *rep, netdriver_addr_t *addr,
	unsigned int instance);
static void rl_reset_hw(re_t *rep);
static void rl_confaddr(re_t *rep, netdriver_addr_t *addr,
	unsigned int instance);
static void rl_set_hwaddr(const netdriver_addr_t *addr);
static void rl_stop(void);
static void rl_rec_mode(re_t *rep);
static void rl_set_mode(unsigned int mode, const netdriver_addr_t *mcast_list,
	unsigned int mcast_count);
static ssize_t rl_recv(struct netdriver_data *data, size_t max);
static int rl_send(struct netdriver_data *data, size_t size);
static unsigned int rl_get_link(uint32_t *media);
static void rl_intr(unsigned int mask);
static void rl_check_ints(re_t *rep);
static void rl_do_reset(re_t *rep);
#if VERBOSE
static void rl_report_link(re_t *rep);
static void dump_phy(const re_t *rep);
#endif
static void rl_handler(re_t *rep);
static void rl_tick(void);

static const struct netdriver rl_table = {
	.ndr_name	= "re",
	.ndr_init	= rl_init,
	.ndr_stop	= rl_stop,
	.ndr_set_mode	= rl_set_mode,
	.ndr_set_hwaddr	= rl_set_hwaddr,
	.ndr_recv	= rl_recv,
	.ndr_send	= rl_send,
	.ndr_get_link	= rl_get_link,
	.ndr_intr	= rl_intr,
	.ndr_tick	= rl_tick
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
 *				rl_init					     *
 *===========================================================================*/
static int rl_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks)
{
/* Initialize the rtl8169 driver. */
	re_t *rep;

	/* Initialize driver state. */
	rep = &re_state;
	memset(rep, 0, sizeof(*rep));

	/* Try to find a matching device. */
	if (!rl_probe(rep, instance))
		return ENXIO;

	/* Claim buffer memory now. */
	rl_init_buf(&re_state);

	/* Initialize the device we found. */
	rl_init_hw(rep, addr, instance);

	*caps = NDEV_CAP_MCAST | NDEV_CAP_BCAST | NDEV_CAP_HWADDR;
	*ticks = sys_hz();
	return OK;
}

/*===========================================================================*
 *				rl_stop					     *
 *===========================================================================*/
static void rl_stop(void)
{
	re_t *rep;

	rep = &re_state;

	rl_outb(rep->re_base_port, RL_CR, RL_CR_RST);
}

static void mdio_write(u16_t port, int regaddr, int value)
{
	int i;

	rl_outl(port, RL_PHYAR,
	    0x80000000 | (regaddr & 0x1F) << 16 | (value & 0xFFFF));

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

static void rtl8169_update_stat(re_t *rep)
{
	static u64_t last_miss = 0, last_coll = 0;
	u64_t miss, coll;
	port_t port;
	int i;

	port = rep->re_base_port;

	/* Dump Tally Counter Command */
	rl_outl(port, RL_DTCCR_HI, 0);		/* 64 bits */
	rl_outl(port, RL_DTCCR_LO, rep->dtcc_buf | RL_DTCCR_CMD);
	for (i = 0; i < 1000; i++) {
		if (!(rl_inl(port, RL_DTCCR_LO) & RL_DTCCR_CMD))
			break;
		micro_delay(10);
	}

	/* Update counters */
	miss = rep->v_dtcc_buf->MissPkt;
	netdriver_stat_ierror(miss - last_miss);
	last_miss = miss;

	coll = rep->v_dtcc_buf->Tx1Col + rep->v_dtcc_buf->TxMCol;
	netdriver_stat_coll(coll - last_coll);
	last_coll = coll;
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

	rtl8169_update_stat(rep);

	printf("Realtek RTL 8169 driver %s:\n", netdriver_name());

	printf("interrupts :%8u\n", rep->interrupts);

	printf("\nRealtek RTL 8169 Tally Counters:\n");

	dtcc = rep->v_dtcc_buf;

	if (dtcc->TxOk_high)
		printf("TxOk       :%8u%08u\t",
		    dtcc->TxOk_high, dtcc->TxOk_low);
	else
		printf("TxOk       :%16u\t", dtcc->TxOk_low);

	if (dtcc->RxOk_high)
		printf("RxOk       :%8u%08u\n",
		    dtcc->RxOk_high, dtcc->RxOk_low);
	else
		printf("RxOk       :%16u\n", dtcc->RxOk_low);

	if (dtcc->TxEr_high)
		printf("TxEr       :%8u%08u\t",
		    dtcc->TxEr_high, dtcc->TxEr_low);
	else
		printf("TxEr       :%16u\t", dtcc->TxEr_low);

	printf("RxEr       :%16u\n", dtcc->RxEr);

	printf("Tx1Col     :%16u\t", dtcc->Tx1Col);
	printf("TxMCol     :%16u\n", dtcc->TxMCol);

	if (dtcc->RxOkPhy_high)
		printf("RxOkPhy    :%8u%08u\t",
		    dtcc->RxOkPhy_high, dtcc->RxOkPhy_low);
	else
		printf("RxOkPhy    :%16u\t", dtcc->RxOkPhy_low);

	if (dtcc->RxOkBrd_high)
		printf("RxOkBrd    :%8u%08u\n",
		    dtcc->RxOkBrd_high, dtcc->RxOkBrd_low);
	else
		printf("RxOkBrd    :%16u\n", dtcc->RxOkBrd_low);

	printf("RxOkMul    :%16u\t", dtcc->RxOkMul);
	printf("MissPkt    :%16d\n", dtcc->MissPkt);

	printf("\nRealtek RTL 8169 Miscellaneous Info:\n");

	printf("tx_head    :%8d  busy %d\t",
		rep->re_tx_head, rep->re_tx[rep->re_tx_head].ret_busy);
}
#endif

/*===========================================================================*
 *				rl_set_mode				     *
 *===========================================================================*/
static void rl_set_mode(unsigned int mode,
	const netdriver_addr_t * mcast_list __unused,
	unsigned int mcast_count __unused)
{
	re_t *rep;

	rep = &re_state;

	rep->re_mode = mode;

	rl_rec_mode(rep);
}

/*===========================================================================*
 *				rl_probe				     *
 *===========================================================================*/
static int rl_probe(re_t *rep, unsigned int skip)
{
	int r, devind;
	u16_t vid, did;
	u32_t bar;
	u8_t ilr;
#if VERBOSE
	const char *dname;
#endif

	pci_init();

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
	printf("%s: ", netdriver_name());
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
		netdriver_name(), (unsigned long)bar, ilr);
#endif

	return TRUE;
}

/*===========================================================================*
 *				rl_init_buf				     *
 *===========================================================================*/
static void rl_init_buf(re_t *rep)
{
	size_t rx_bufsize, tx_bufsize, rx_descsize, tx_descsize, tot_bufsize;
	struct re_desc *desc;
	phys_bytes buf;
	char *mallocbuf;
	int d;

	/* Allocate receive and transmit descriptors */
	rx_descsize = (N_RX_DESC * sizeof(struct re_desc));
	tx_descsize = (N_TX_DESC * sizeof(struct re_desc));

	/* Allocate receive and transmit buffers */
	tx_bufsize = NDEV_ETH_PACKET_MAX_TAGGED;
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
		if (d == (N_RX_DESC - 1)) /* Last descriptor: set EOR bit */
			desc->status = DESC_EOR | DESC_OWN |
			    (RX_BUFSIZE & DESC_RX_LENMASK);
		else
			desc->status = DESC_OWN |
			    (RX_BUFSIZE & DESC_RX_LENMASK);

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
	rep->re_tx_busy = 0;

	/* Dump Tally Counter buffer */
	rep->dtcc_buf = buf;
	rep->v_dtcc_buf = (re_dtcc *)mallocbuf;
}

/*===========================================================================*
 *				rl_init_hw				     *
 *===========================================================================*/
static void rl_init_hw(re_t *rep, netdriver_addr_t *addr,
	unsigned int instance)
{
	int s;
#if VERBOSE
	int i;
#endif

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
		netdriver_name(), rep->re_model, rep->re_mac);
#endif

	rl_confaddr(rep, addr, instance);

#if VERBOSE
	printf("%s: Ethernet address ", netdriver_name());
	for (i = 0; i < 6; i++) {
		printf("%x%c", addr->na_addr[i],
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
static void rl_reset_hw(re_t *rep)
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

	t = mdio_read(port, MII_CTRL);
	t |= MII_CTRL_ANE | MII_CTRL_DM | MII_CTRL_SP_1000;
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
static void rl_confaddr(re_t *rep, netdriver_addr_t *addr,
	unsigned int instance)
{
	static char eakey[] = RL_ENVVAR "#_EA";
	static char eafmt[] = "x:x:x:x:x:x";
	int i;
	port_t port;
	long v;

	/* User defined ethernet address? */
	eakey[sizeof(RL_ENVVAR)-1] = '0' + instance;

	port = rep->re_base_port;

	for (i = 0; i < 6; i++) {
		if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
			break;
		addr->na_addr[i] = v;
	}

	if (i != 0 && i != 6)
		env_panic(eakey);	/* It's all or nothing */

	/* Should update ethernet address in hardware */
	if (i == 6)
		rl_set_hwaddr(addr);

	/* Get ethernet address */
	for (i = 0; i < 6; i++)
		addr->na_addr[i] = rl_inb(port, RL_IDR+i);
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

	port = rep->re_base_port;
	rl_outb(port, RL_9346CR, RL_9346CR_EEM_CONFIG);
	w = 0;
	for (i = 0; i < 4; i++)
		w |= (addr->na_addr[i] << (i * 8));
	rl_outl(port, RL_IDR, w);
	w = 0;
	for (i = 4; i < 6; i++)
		w |= (addr->na_addr[i] << ((i-4) * 8));
	rl_outl(port, RL_IDR + 4, w);
	rl_outb(port, RL_9346CR, RL_9346CR_EEM_NORMAL);
}

/*===========================================================================*
 *				rl_rec_mode				     *
 *===========================================================================*/
static void rl_rec_mode(re_t *rep)
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
	if (rep->re_mode & NDEV_MODE_PROMISC)
		rcr |= RL_RCR_AB | RL_RCR_AM | RL_RCR_AAP;
	if (rep->re_mode & NDEV_MODE_BCAST)
		rcr |= RL_RCR_AB;
	if (rep->re_mode & (NDEV_MODE_MCAST_LIST | NDEV_MODE_MCAST_ALL))
		rcr |= RL_RCR_AM;
	rcr |= RL_RCR_APM;
	rl_outl(port, RL_RCR, RL_RCR_RXFTH_UNLIM | RL_RCR_MXDMA_1024 | rcr);
}

/*===========================================================================*
 *				rl_recv					     *
 *===========================================================================*/
static ssize_t rl_recv(struct netdriver_data *data, size_t max)
{
	int index;
	port_t port;
	unsigned totlen, packlen;
	re_desc *desc;
	u32_t rxstat;
	re_t *rep;

	rep = &re_state;

	port = rep->re_base_port;

	if (rl_inb(port, RL_CR) & RL_CR_BUFE)
		return SUSPEND; /* Receive buffer is empty, suspend */

	index = rep->re_rx_head;
	desc = rep->re_rx_desc;
	desc += index;

	for (;;) {
		rxstat = desc->status;

		if (rxstat & DESC_OWN)
			return SUSPEND;

		if (rxstat & DESC_RX_CRC)
			netdriver_stat_ierror(1);

		if ((rxstat & (DESC_FS | DESC_LS)) == (DESC_FS | DESC_LS))
			break;

#if VERBOSE
		printf("rl_recv: packet is fragmented\n");
#endif
		/* Fix the fragmented packet */
		if (index == N_RX_DESC - 1) {
			desc->status = DESC_EOR | DESC_OWN |
			    (RX_BUFSIZE & DESC_RX_LENMASK);
			index = 0;
			desc = rep->re_rx_desc;
		} else {
			desc->status = DESC_OWN |
			    (RX_BUFSIZE & DESC_RX_LENMASK);
			index++;
			desc++;
		}
		/* Loop until we get correct packet */
	}

	totlen = rxstat & DESC_RX_LENMASK;
	if (totlen < 8 || totlen > 2 * NDEV_ETH_PACKET_MAX) {
		/* Someting went wrong */
		printf("rl_recv: bad length (%u) in status 0x%08x\n",
			totlen, rxstat);
		panic(NULL);
	}

	/* Should subtract the CRC */
	packlen = totlen - NDEV_ETH_PACKET_CRC;
	if (packlen > max)
		packlen = max;

	netdriver_copyout(data, 0, rep->re_rx[index].v_ret_buf, packlen);

	if (index == N_RX_DESC - 1) {
		desc->status = DESC_EOR | DESC_OWN |
		    (RX_BUFSIZE & DESC_RX_LENMASK);
		index = 0;
	} else {
		desc->status = DESC_OWN | (RX_BUFSIZE & DESC_RX_LENMASK);
		index++;
	}
	rep->re_rx_head = index;
	assert(rep->re_rx_head < N_RX_DESC);

	return packlen;
}

/*===========================================================================*
 *				rl_send					     *
 *===========================================================================*/
static int rl_send(struct netdriver_data *data, size_t size)
{
	int tx_head;
	re_t *rep;
	re_desc *desc;

	rep = &re_state;

	tx_head = rep->re_tx_head;

	desc = rep->re_tx_desc;
	desc += tx_head;

	assert(desc);
	assert(rep->re_tx_desc);
	assert(rep->re_tx_head >= 0 && rep->re_tx_head < N_TX_DESC);

	if (rep->re_tx[tx_head].ret_busy)
		return SUSPEND;

	netdriver_copyin(data, 0, rep->re_tx[tx_head].v_ret_buf, size);

	rep->re_tx[tx_head].ret_busy = TRUE;
	rep->re_tx_busy++;

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

	return OK;
}

/*===========================================================================*
 *				rl_check_ints				     *
 *===========================================================================*/
static void rl_check_ints(re_t *rep)
{
	if (!rep->re_got_int)
		return;
	rep->re_got_int = FALSE;

	netdriver_recv();

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
	re_t *rep;
	u8_t mii_status;

	rep = &re_state;

	mii_status = rl_inb(rep->re_base_port, RL_PHYSTAT);

	if (!(mii_status & RL_STAT_LINK))
		return NDEV_LINK_DOWN;

	if (mii_status & RL_STAT_1000)
		*media = IFM_ETHER | IFM_1000_T;
	else if (mii_status & RL_STAT_100)
		*media = IFM_ETHER | IFM_100_TX;
	else if (mii_status & RL_STAT_10)
		*media = IFM_ETHER | IFM_10_T;

	if (mii_status & RL_STAT_FULLDUP)
		*media |= IFM_FDX;
	else
		*media |= IFM_HDX;

	return NDEV_LINK_UP;
}

/*===========================================================================*
 *				rl_report_link				     *
 *===========================================================================*/
#if VERBOSE
static void rl_report_link(re_t *rep)
{
	port_t port;
	u8_t mii_status;

	port = rep->re_base_port;

	mii_status = rl_inb(port, RL_PHYSTAT);

	if (mii_status & RL_STAT_LINK) {
		rep->re_link_up = 1;
		printf("%s: link up at ", netdriver_name());
	} else {
		rep->re_link_up = 0;
		printf("%s: link down\n", netdriver_name());
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

	dump_phy(rep);
}
#endif

/*===========================================================================*
 *				rl_do_reset				     *
 *===========================================================================*/
static void rl_do_reset(re_t *rep)
{
	rep->re_need_reset = FALSE;
	rl_reset_hw(rep);
	rl_rec_mode(rep);

	rep->re_tx_head = 0;
	if (rep->re_tx[rep->re_tx_head].ret_busy)
		rep->re_tx_busy--;
	rep->re_tx[rep->re_tx_head].ret_busy = FALSE;
	rep->re_send_int = TRUE;
}

#if VERBOSE
static void dump_phy(const re_t *rep)
{
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
}
#endif

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
		printf("RTL8169: error, couldn't enable interrupts: %d\n", s);

	/* Perform tasks based on the flagged conditions. */
	rl_check_ints(rep);
}

/*===========================================================================*
 *				rl_handler				     *
 *===========================================================================*/
static void rl_handler(re_t *rep)
{
	int i, port, tx_head, tx_tail, link_up;
	u16_t isr;
	re_desc *desc;

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
		}
	}

	if (isr & (RL_ISR_RDU | RL_ISR_RER | RL_ISR_ROK)) {
		if (isr & RL_ISR_RER)
			netdriver_stat_ierror(1);
		isr &= ~(RL_ISR_RDU | RL_ISR_RER | RL_ISR_ROK);

		rep->re_got_int = TRUE;
	}

	if ((isr & (RL_ISR_TDU | RL_ISR_TER | RL_ISR_TOK)) || 1) {
		if (isr & RL_ISR_TER)
			netdriver_stat_oerror(1);
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

			rep->re_tx[tx_tail].ret_busy = FALSE;
			rep->re_tx_busy--;

			if (++tx_tail >= N_TX_DESC)
				tx_tail = 0;
			assert(tx_tail < N_TX_DESC);

			rep->re_send_int = TRUE;
			rep->re_got_int = TRUE;
			rep->re_tx_alive = TRUE;
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
 *				rl_tick					     *
 *===========================================================================*/
static void rl_tick(void)
{
	re_t *rep;

	rep = &re_state;

	/* Should collect statistics */
	if (!(++rep->dtcc_counter % RE_DTCC_VALUE))
		rtl8169_update_stat(rep);

	assert(rep->re_tx_busy >= 0 && rep->re_tx_busy <= N_TX_DESC);
	if (rep->re_tx_busy == 0) {
		/* Assume that an idle system is alive */
		rep->re_tx_alive = TRUE;
		return;
	}
	if (rep->re_tx_alive) {
		rep->re_tx_alive = FALSE;
		return;
	}
	printf("%s: TX timeout, resetting\n", netdriver_name());
	printf("tx_head    :%8d  busy %d\t",
		rep->re_tx_head, rep->re_tx[rep->re_tx_head].ret_busy);
	rep->re_need_reset = TRUE;
	rep->re_got_int = TRUE;

	rl_check_ints(rep);
}
