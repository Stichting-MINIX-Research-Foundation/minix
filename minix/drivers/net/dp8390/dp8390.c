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

#include <net/hton.h>
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

static int do_init(unsigned int instance, ether_addr_t *addr);
static void pci_conf(void);
static int do_send(struct netdriver_data *data, size_t size);
static ssize_t do_recv(struct netdriver_data *data, size_t max);
static void do_mode(unsigned int mode);
static void do_stat(eth_stat_t *stat);
static void do_stop(void);
static void dp_init(dpeth_t *dep);
static void dp_confaddr(dpeth_t *dep);
static void dp_reset(dpeth_t *dep);
static void do_intr(unsigned int mask);
static void dp_getblock(dpeth_t *dep, int page, size_t offset, size_t
	size, void *dst);
static void dp_pio8_getblock(dpeth_t *dep, int page, size_t offset,
	size_t size, void *dst);
static void dp_pio16_getblock(dpeth_t *dep, int page, size_t offset,
	size_t size, void *dst);
static void dp_pkt2user_s(dpeth_t *dep, struct netdriver_data *data, int page,
	size_t length);
static void dp_user2nic_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count);
static void dp_pio8_user2nic_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count);
static void dp_pio16_user2nic_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count);
static void dp_nic2user_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count);
static void dp_pio8_nic2user_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count);
static void dp_pio16_nic2user_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count);
static void conf_hw(dpeth_t *dep);
static void update_conf(dpeth_t *dep, dp_conf_t *dcp);
static void map_hw_buffer(dpeth_t *dep);
static void insb(port_t port, void *buf, size_t size);
static void insw(port_t port, void *buf, size_t size);

static const struct netdriver dp_table = {
	.ndr_init	= do_init,
	.ndr_stop	= do_stop,
	.ndr_mode	= do_mode,
	.ndr_recv	= do_recv,
	.ndr_send	= do_send,
	.ndr_stat	= do_stat,
	.ndr_intr	= do_intr
};

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char *argv[])
{
	env_setargs(argc, argv);

	netdriver_task(&dp_table);

	return 0;
}

/*===========================================================================*
 *				do_init					     *
 *===========================================================================*/
static int do_init(unsigned int instance, ether_addr_t *addr)
{
/* Initialize the dp8390 driver. */
	dpeth_t *dep;

	system_hz = sys_hz();

	de_instance = instance;

	dep = &de_state;
	memset(dep, 0, sizeof(*dep));

	strlcpy(dep->de_name, "dp8390#0", sizeof(dep->de_name));
	dep->de_name[7] += de_instance;

	pci_conf(); /* Configure PCI devices. */

	/* This is the default, try to (re)locate the device. */
	conf_hw(dep);

	dp_init(dep);

	memcpy(addr, dep->de_address.ea_addr, sizeof(*addr));

	return OK;
}

#if 0
/*===========================================================================*
 *				dp8390_dump				     *
 *===========================================================================*/
void dp8390_dump(void)
{
	dpeth_t *dep;
	int isr;

	dep = &de_state;

	printf("\n");
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

/*===========================================================================*
 *				pci_conf				     *
 *===========================================================================*/
static void pci_conf(void)
{
	char envvar[16];
	struct dpeth *dep;
	int i, pci_instance;

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
		panic("no matching PCI device found");
}

/*===========================================================================*
 *				do_send					     *
 *===========================================================================*/
static int do_send(struct netdriver_data *data, size_t size)
{
	int sendq_head;
	dpeth_t *dep;

	dep= &de_state;

	sendq_head= dep->de_sendq_head;
	if (dep->de_sendq[sendq_head].sq_filled)
		return SUSPEND;

	(dep->de_user2nicf_s)(dep, data,
		dep->de_sendq[sendq_head].sq_sendpage * DP_PAGESIZE, 0, size);

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

	return OK;
}

/*===========================================================================*
 *				do_mode					     *
 *===========================================================================*/
static void do_mode(unsigned int mode)
{
	dpeth_t *dep;
	int dp_rcr_reg;

	dep = &de_state;

	outb_reg0(dep, DP_CR, CR_PS_P0 | CR_EXTRA);

	dp_rcr_reg = 0;
	if (mode & NDEV_PROMISC)
		dp_rcr_reg |= RCR_AB | RCR_PRO | RCR_AM;
	if (mode & NDEV_BROAD)
		dp_rcr_reg |= RCR_AB;
	if (mode & NDEV_MULTI)
		dp_rcr_reg |= RCR_AM;
	outb_reg0(dep, DP_RCR, dp_rcr_reg);
}

/*===========================================================================*
 *				do_stat					     *
 *===========================================================================*/
static void do_stat(eth_stat_t *stat)
{
	dpeth_t *dep;

	dep= &de_state;

	dep->de_stat.ets_CRCerr += inb_reg0(dep, DP_CNTR0);
	dep->de_stat.ets_frameAll += inb_reg0(dep, DP_CNTR1);
	dep->de_stat.ets_missedP += inb_reg0(dep, DP_CNTR2);

	memcpy(stat, &dep->de_stat, sizeof(*stat));
}

/*===========================================================================*
 *				do_stop					     *
 *===========================================================================*/
static void do_stop(void)
{
	dpeth_t *dep;

	dep = &de_state;

	outb_reg0(dep, DP_CR, CR_STP | CR_DM_ABORT);
	(dep->de_stopf)(dep);
}

/*===========================================================================*
 *				dp_init					     *
 *===========================================================================*/
static void dp_init(dpeth_t *dep)
{
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
	outb_reg0(dep, DP_RCR, 0);
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
		panic("unable to enable interrupts: %d", r);
}

/*===========================================================================*
 *				dp_confaddr				     *
 *===========================================================================*/
static void dp_confaddr(dpeth_t *dep)
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
 *				dp_reset				     *
 *===========================================================================*/
static void dp_reset(dpeth_t *dep)
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
	netdriver_send();
	dep->de_flags &= ~DEF_STOPPED;
}

/*===========================================================================*
 *				do_intr					     *
 *===========================================================================*/
static void do_intr(unsigned int __unused mask)
{
	dpeth_t *dep;
	int isr, tsr;
	int r, size, sendq_tail;

	dep = &de_state;

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
			netdriver_send();
		}

		if (isr & ISR_PRX)
			netdriver_recv();

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
			netdriver_recv(); /* see if we can reset right now */
			break;
		}
	}

	if ((r = sys_irqenable(&dep->de_hook)) != OK)
		panic("unable enable interrupts: %d", r);
}

/*===========================================================================*
 *				do_recv					     *
 *===========================================================================*/
static ssize_t do_recv(struct netdriver_data *data, size_t max)
{
	dpeth_t *dep;
	dp_rcvhdr_t header;
	unsigned pageno, curr, next;
	size_t length;
	int packet_processed;
	u16_t eth_type;

	dep = &de_state;

	packet_processed = FALSE;
	pageno = inb_reg0(dep, DP_BNRY) + 1;
	if (pageno == dep->de_stoppage) pageno = dep->de_startpage;

	do
	{
		outb_reg0(dep, DP_CR, CR_PS_P1 | CR_EXTRA);
		curr = inb_reg1(dep, DP_CURR);
		outb_reg0(dep, DP_CR, CR_PS_P0 | CR_EXTRA);

		if (curr == pageno) {
			if (dep->de_flags & DEF_STOPPED) {
				/* The chip is stopped, and all arrived packets
				 * are delivered.
				 */
				dp_reset(dep);
			}

			return SUSPEND;
		}

		(dep->de_getblockf)(dep, pageno, (size_t)0, sizeof(header),
			&header);
		(dep->de_getblockf)(dep, pageno, sizeof(header) +
			2*sizeof(ether_addr_t), sizeof(eth_type), &eth_type);

		length = (header.dr_rbcl | (header.dr_rbch << 8)) -
			sizeof(dp_rcvhdr_t);
		next = header.dr_next;
		if (length < ETH_MIN_PACK_SIZE || length > max)
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
		else if (header.dr_status & RSR_PRX)
		{
			dp_pkt2user_s(dep, data, pageno, length);

			packet_processed = TRUE;
			dep->de_stat.ets_packetR++;
		}
		if (next == dep->de_startpage)
			outb_reg0(dep, DP_BNRY, dep->de_stoppage - 1);
		else
			outb_reg0(dep, DP_BNRY, next - 1);

		pageno = next;
	} while (!packet_processed);

	return length;
}

/*===========================================================================*
 *				dp_getblock				     *
 *===========================================================================*/
static void dp_getblock(dpeth_t *dep, int page, size_t offset, size_t size,
	void *dst)
{
	offset = page * DP_PAGESIZE + offset;

	memcpy(dst, dep->de_locmem + offset, size);
}

/*===========================================================================*
 *				dp_pio8_getblock			     *
 *===========================================================================*/
static void dp_pio8_getblock(dpeth_t *dep, int page, size_t offset,
	size_t size, void *dst)
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
static void dp_pio16_getblock(dpeth_t *dep, int page, size_t offset,
	size_t size, void *dst)
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
static void dp_pkt2user_s(dpeth_t *dep, struct netdriver_data *data, int page,
	size_t length)
{
	int last, count;

	last = page + (length - 1) / DP_PAGESIZE;
	if (last >= dep->de_stoppage)
	{
		count = (dep->de_stoppage - page) * DP_PAGESIZE -
			sizeof(dp_rcvhdr_t);

		(dep->de_nic2userf_s)(dep, data,
		    page * DP_PAGESIZE + sizeof(dp_rcvhdr_t), 0, count);
		(dep->de_nic2userf_s)(dep, data,
		    dep->de_startpage * DP_PAGESIZE, count, length - count);
	}
	else
	{
		(dep->de_nic2userf_s)(dep, data,
		    page * DP_PAGESIZE + sizeof(dp_rcvhdr_t), 0, length);
	}
}

/*===========================================================================*
 *				dp_user2nic_s				     *
 *===========================================================================*/
static void dp_user2nic_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count)
{
	netdriver_copyin(data, offset, dep->de_locmem + nic_addr, count);
}

/*===========================================================================*
 *				dp_pio8_user2nic_s			     *
 *===========================================================================*/
static void dp_pio8_user2nic_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count)
{
	int i;

	outb_reg0(dep, DP_ISR, ISR_RDC);

	outb_reg0(dep, DP_RBCR0, count & 0xFF);
	outb_reg0(dep, DP_RBCR1, count >> 8);
	outb_reg0(dep, DP_RSAR0, nic_addr & 0xFF);
	outb_reg0(dep, DP_RSAR1, nic_addr >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RW | CR_PS_P0 | CR_STA);

	netdriver_portoutb(data, offset, dep->de_data_port, count);

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
static void dp_pio16_user2nic_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count)
{
	size_t ecount;
	int i;

	ecount= (count+1) & ~1;

	outb_reg0(dep, DP_ISR, ISR_RDC);
	outb_reg0(dep, DP_RBCR0, ecount & 0xFF);
	outb_reg0(dep, DP_RBCR1, ecount >> 8);
	outb_reg0(dep, DP_RSAR0, nic_addr & 0xFF);
	outb_reg0(dep, DP_RSAR1, nic_addr >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RW | CR_PS_P0 | CR_STA);

	netdriver_portoutw(data, offset, dep->de_data_port, count);

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
static void dp_nic2user_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count)
{
	netdriver_copyout(data, offset, dep->de_locmem + nic_addr, count);
}

/*===========================================================================*
 *				dp_pio8_nic2user_s			     *
 *===========================================================================*/
static void dp_pio8_nic2user_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count)
{
	outb_reg0(dep, DP_RBCR0, count & 0xFF);
	outb_reg0(dep, DP_RBCR1, count >> 8);
	outb_reg0(dep, DP_RSAR0, nic_addr & 0xFF);
	outb_reg0(dep, DP_RSAR1, nic_addr >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RR | CR_PS_P0 | CR_STA);

	netdriver_portinb(data, offset, dep->de_data_port, count);
}

/*===========================================================================*
 *				dp_pio16_nic2user_s			     *
 *===========================================================================*/
static void dp_pio16_nic2user_s(dpeth_t *dep, struct netdriver_data *data,
	int nic_addr, size_t offset, size_t count)
{
	size_t ecount;

	ecount= (count+1) & ~1;

	outb_reg0(dep, DP_RBCR0, ecount & 0xFF);
	outb_reg0(dep, DP_RBCR1, ecount >> 8);
	outb_reg0(dep, DP_RSAR0, nic_addr & 0xFF);
	outb_reg0(dep, DP_RSAR1, nic_addr >> 8);
	outb_reg0(dep, DP_CR, CR_DM_RR | CR_PS_P0 | CR_STA);

	netdriver_portinw(data, offset, dep->de_data_port, count);
}

/*===========================================================================*
 *				conf_hw					     *
 *===========================================================================*/
static void conf_hw(dpeth_t *dep)
{
	int confnr;
	dp_conf_t *dcp;

	/* Pick a default configuration for this instance. */
	confnr= MIN(de_instance, DP_CONF_NR-1);

	dcp= &dp_conf[confnr];
	update_conf(dep, dcp);
	if (!wdeth_probe(dep) && !ne_probe(dep) && !el2_probe(dep))
		panic("no ethernet card found at 0x%x\n", dep->de_base_port);

/* XXX */ if (dep->de_linmem == 0) dep->de_linmem= 0xFFFF0000;
}

/*===========================================================================*
 *				update_conf				     *
 *===========================================================================*/
static void update_conf(dpeth_t *dep, dp_conf_t *dcp)
{
	long v;
	static char dpc_fmt[] = "x:d:x:x";
	char eckey[16];

	if (dep->de_pci)
	{
		/* PCI device is present */
		return;		/* Already configured */
	}

	strlcpy(eckey, "DPETH0", sizeof(eckey));
	eckey[5] += de_instance;

	/* Get the default settings and modify them from the environment. */
	v= dcp->dpc_port;
	(void) env_parse(eckey, dpc_fmt, 0, &v, 0x0000L, 0xFFFFL);
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
static void map_hw_buffer(dpeth_t *dep)
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
	int r;

	r= sys_insb(port, SELF, buf, size);
	if (r != OK)
		panic("sys_sdevio failed: %d", r);
}

static void insw(port_t port, void *buf, size_t size)
{
	int r;

	r= sys_insw(port, SELF, buf, size);
	if (r != OK)
		panic("sys_sdevio failed: %d", r);
}

/*
 * $PchId: dp8390.c,v 1.25 2005/02/10 17:32:07 philip Exp $
 */
