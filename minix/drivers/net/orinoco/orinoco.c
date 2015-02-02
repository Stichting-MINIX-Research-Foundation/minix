/*
 * orinoco.c
 *
 * This file contains a wireless device driver for Prism based wireless
 * cards.
 *
 * Created by Stevens Le Blond <slblond@few.vu.nl>
 *        and Michael Valkering <mjvalker@cs.vu.nl>
 *
 */

#include 	<minix/drivers.h>
#include 	<minix/netdriver.h>
#include	<machine/pci.h>
#include	<machine/vmparam.h>
#include	<sys/mman.h>

#define		VERBOSE		1	/* display message during init */

#include	"assert.h"
#include	"hermes.h"
#include	"hermes_rid.h"
#include	"orinoco.h"

#define		OR_M_ENABLED 1
#define		OR_M_DISABLED 0
#define		OR_F_EMPTY 0
#define		OR_F_MULTI 1
#define		OR_F_BROAD (1<<1)
#define		OR_F_ENABLED (1<<2)
#define		OR_F_PROMISC (1<<3)
#define		OR_F_READING (1<<4)
#define		OR_F_SEND_AVAIL (1<<5)
#define		OR_F_PACK_SENT (1<<6)
#define		OR_F_PACK_RECV (1<<7)
#define 	ORINOCO_INTEN ( HERMES_EV_RX | HERMES_EV_ALLOC |\
					HERMES_EV_WTERR | HERMES_EV_TXEXC|\
					HERMES_EV_INFO | HERMES_EV_INFDROP|\
					HERMES_EV_TX)

#define		NO_FID (-1)
#define		ETH_ALEN 6
#define		USER_BAP 0
#define 	IRQ_BAP 1
#define		ETH_HLEN		14

static t_or or_state;

struct ethhdr {
	u8_t h_dest[ETH_ALEN];
	u8_t h_src[ETH_ALEN];
	u16_t h_proto;
};

struct header_struct {
	/* 802.3 */
	u8_t dest[ETH_ALEN];
	u8_t src[ETH_ALEN];
	u16_t len;
	/* 802.2 */
	u8_t dsap;
	u8_t ssap;
	u8_t ctrl;
	/* SNAP */
	u8_t oui[3];
	u16_t ethertype;
};

#define			RUP_EVEN(x)	(((x) + 1) & (~1))

u8_t encaps_hdr[] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
#define	ENCAPS_OVERHEAD	(sizeof (encaps_hdr) + 2)

/********************************************************************
 *              Data tables                                         *
 ********************************************************************/

/* The frequency of each channel in MHz */
static const long channel_frequency[] = {
	2412, 2417, 2422, 2427, 2432, 2437, 2442,
	2447, 2452, 2457, 2462, 2467, 2472, 2484
};

#define NUM_CHANNELS (sizeof(channel_frequency) / sizeof(channel_frequency[0]))

/* This tables gives the actual meanings of the bitrate IDs returned by the
 * firmware. Not used yet */
struct {
	int bitrate;		/* in 100s of kilobits */
	int automatic;
	u16_t txratectrl;
} bitrate_table[] =
{
	{110, 1, 15},		/* Entry 0 is the default */
	{10, 0, 1},
	{10, 1, 1},
	{20, 0, 2},
	{20, 1, 3},
	{55, 0, 4},
	{55, 1, 7},
	{110, 0, 8},};

#define BITRATE_TABLE_SIZE (sizeof(bitrate_table) / sizeof(bitrate_table[0]))

static void or_other(const message *m_ptr, int ipc_status);
static void or_stop(void);
static int or_probe(t_or *, unsigned int skip);
static void or_ev_info(t_or *);
static int or_init(unsigned int instance, ether_addr_t *addr);
static void or_init_struct(t_or *, unsigned int);
static void or_init_hw(t_or *, ether_addr_t *);
static void or_check_ints(t_or *);
static void or_writerids(hermes_t *, t_or *);
static void or_readrids(hermes_t *, t_or *, ether_addr_t *);
static u32_t or_get_bar(int devind, t_or * orp);
static void or_stat(eth_stat_t *stat);
static void print_linkstatus(t_or * orp, u16_t status);
static ssize_t or_recv(struct netdriver_data *data, size_t max);
static int or_get_recvd_packet(t_or *orp, u16_t rxfid, u8_t *databuf);
static void or_reset(void);
static void or_alarm(clock_t stamp);
static int or_send(struct netdriver_data *data, size_t size);
static void setup_wepkey(t_or *orp, char *wepkey0);
static void or_intr(unsigned int mask);
static void or_handler(t_or *orp);
static void or_dump(void);

static const struct netdriver or_table = {
	.ndr_init	= or_init,
	.ndr_stop	= or_stop,
	.ndr_recv	= or_recv,
	.ndr_send	= or_send,
	.ndr_stat	= or_stat,
	.ndr_intr	= or_intr,
	.ndr_alarm	= or_alarm,
	.ndr_other	= or_other
};

/*****************************************************************************
 *            main                                                           *
 *                                                                           *
 *                                                                           *
 * The main function of the driver, receiving and processing messages        *
 *****************************************************************************/
int main(int argc, char *argv[])
{
	env_setargs(argc, argv);

	netdriver_task(&or_table);

	return 0;
}

/*****************************************************************************
 *            or_other                                                       *
 *                                                                           *
 *                                                                           *
 * Process miscellaneous messages                                            *
 *****************************************************************************/
static void or_other(const message *m_ptr, int ipc_status)
{
	if (is_ipc_notify(ipc_status) && m_ptr->m_source == TTY_PROC_NR)
		or_dump();
}

/*****************************************************************************
 *                    or_stop                                                *
 *                                                                           *
 * Stop the card                                                             *
 *****************************************************************************/
static void or_stop(void)
{
	t_or *orp;

	orp = &or_state;

	/* TODO: send a signal to the card to shut it down */
}

/*****************************************************************************
 *                or_intr                                                    *
 *                                                                           *
 * Process the interrupts which the card generated                           *
 *****************************************************************************/
static void or_intr(unsigned int __unused mask)
{
	t_or *orp;
	int s;

	orp = &or_state;

	/* Run interrupt handler at driver level. */
	or_handler(orp);

	/* Reenable interrupts for this hook. */
	if ((s=sys_irqenable(&orp->or_hook_id)) != OK)
		printf("orinoco: error, couldn't enable interrupts: %d\n", s);

	/* Perform tasks based on the flagged conditions. */
	or_check_ints(orp);
}

/*****************************************************************************
 *                or_reset                                                   *
 *                                                                           *
 * Sometime the card gets screwed, behaving erratically. Solution: reset the *
 * card. This is actually largely redoing the initialization                 *
 *****************************************************************************/
static void or_reset(void)
{
	static clock_t last_reset, now;
	t_or *orp;
	int i, r;

	if (OK != (r = getticks(&now)))
		panic("orinoco: getuptime() failed: %d", r);

	if(now - last_reset < sys_hz() * 10) {
		printf("Resetting card too often. Going to reset driver\n");
		exit(1);
	}

	last_reset = now;

	orp = &or_state;

	orp->or_need_reset = FALSE;
	or_init_hw(orp, NULL);

	orp->rx_last = orp->rx_first = 0;
	for(i = 0; i < NR_RX_BUFS; i++) {
		orp->rx_length[0] = 0;
	}

	if (orp->or_tx.ret_busy)
		orp->or_tx_busy--;
	orp->or_tx.ret_busy = FALSE;
	orp->or_send_int = TRUE;
}

/*****************************************************************************
 *                or_dump                                                    *
 *                                                                           *
 * Dump interesting information about the card on F-key pressed.             *
 * Not implemented yet                                                       *
 *****************************************************************************/
static void or_dump(void)
{
	t_or *orp;
	int sfkeys;

	orp = &or_state;

	if(OK != fkey_events(NULL, &sfkeys)) {
		printf("Contacting the TTY failed\n");
	}

	if(bit_isset(sfkeys, 11)) {
		print_linkstatus(orp, orp->last_linkstatus);
	}
}

/*****************************************************************************
 *                or_init                                                    *
 *                                                                           *
 * The main initialization function, called at startup.                      *
 *****************************************************************************/
static int or_init(unsigned int instance, ether_addr_t *addr)
{
	int fkeys, sfkeys, r;
	t_or *orp;

	orp = &or_state;

	/* Initialize the orp structure */
	or_init_struct(orp, instance);

	/* Try to find out where the card is in the pci bus */
	if (!or_probe(orp, instance))
		return ENXIO;

	/* initialize card, hardware/firmware */
	or_init_hw(orp, addr);

	/* Use a synchronous alarm instead of a watchdog timer. */
	sys_setalarm(sys_hz(), 0);

	/* Observe some function key for debug dumps. */
	fkeys = sfkeys = 0; bit_set(sfkeys, 11);
	if ((r=fkey_map(&fkeys, &sfkeys)) != OK)
	    printf("Warning: orinoco couldn't observe F-key(s): %d\n",r);

	return OK;
}

/*****************************************************************************
 *                or_probe                                                   *
 *                                                                           *
 * Try to find the card based on information provided by pci and get irq and *
 * bar                                                                       *
 *****************************************************************************/
static int or_probe(t_or * orp, unsigned int skip)
{
	u8_t ilr;
	u32_t bar;
	char *dname;
	u16_t vid, did;
	int r, devind;

	pci_init();

	/* Start looking from the beginning */
	r = pci_first_dev(&devind, &vid, &did);
	if (r == 0)
		return FALSE;

	/* Skip as many instances as requested */
	while (skip--) {
		r = pci_next_dev(&devind, &vid, &did);
		if (!r)
			return FALSE;
	}

	/* Get the name as advertised by pci */
	dname = pci_dev_name(vid, did);
	if (!dname)
		dname = "unknown device";
	printf("%s: %s (%04x/%04x) at %s\n",
		orp->or_name, dname, vid, did, pci_slot_name(devind));

	pci_reserve(devind);

	/* Get the irq */
	ilr = pci_attr_r8(devind, PCI_ILR);
	orp->or_irq = ilr;

	/* Map registers into memory */
	bar = or_get_bar(devind, orp);

	orp->hw.locmem = vm_map_phys(SELF, (void *)bar, PAGE_SIZE);
	if (orp->hw.locmem == MAP_FAILED)
		panic("or_probe: vm_map_phys failed");

	return TRUE;
}

/*****************************************************************************
 *                or_get_bar                                                 *
 *                                                                           *
 * Get the base address from pci (from Base Address Register) and find out   *
 * whether the card is memory mapped or in I/O space. Currently, only        *
 * memmory mapped is supported.                                              *
 *****************************************************************************/
static u32_t or_get_bar(int devind, t_or * orp)
{
	u32_t bar;
	int is_iospace;
	hermes_t *hw = &(orp->hw);

	/* bit 1 off the PCI_BAR register indicates whether the cards registers
	 * are mapped in io-space or shared memory */
	is_iospace = pci_attr_r32(devind, PCI_BAR) & 1;

	if (is_iospace)	{
		/* read where the base address is in I/O space */
		bar = pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;

		if ((bar & 0x3ff) >= 0x100 - 32 || bar < 0x400)
			panic("base address isn't properly configured");

		/* In I/O space registers are 2 bytes wide, without any spacing
		 * in between */
		hermes_struct_init (hw, bar, is_iospace,
				    HERMES_16BIT_REGSPACING);

#if VERBOSE
		printf ("%s: using I/O space address 0x%x, IRQ %d\n",
			orp->or_name, bar, orp->or_irq);
#endif

		panic("Not implemented yet");
		/* Although we are able to find the desired bar and irq for an
		 * I/O spaced card, we haven't implemented the right register
 		 * accessing functions. This wouldn't be difficult, but we were
		 * not able to test them. Therefore, give an alert here */

		return bar;
	} else {
		/* read where the base address is in shared memory */
		bar = pci_attr_r32(devind, PCI_BAR) & 0xfffffff0;
		/* maybe some checking whether the address is legal... */

		/* Memory mapped registers are 2 bytes wide, aligned on 4
		 * bytes */
		hermes_struct_init (hw, bar, is_iospace,
				    HERMES_32BIT_REGSPACING);

#if VERBOSE
		printf ("%s: using shared memory address", orp->or_name);
		printf (" 0x%x, IRQ %d\n", bar, orp->or_irq);
#endif

		return bar;

	}
}

/*****************************************************************************
 *                or_init_struct                                             *
 *                                                                           *
 * Set the orinoco structure to default values                               *
 *****************************************************************************/
static void or_init_struct(t_or * orp, unsigned int instance)
{
	int i;

	memset(orp, 0, sizeof(*orp));

	strlcpy(orp->or_name, OR_NAME, sizeof(orp->or_name));
	orp->or_name[sizeof(OR_NAME) - 2] = instance + '0';

	orp->or_link_up = -1;

	orp->or_tx.or_txfid = NO_FID;

	for(i = 0; i < NR_RX_BUFS; i++) {
		orp->rxfid[i] = NO_FID;
		orp->rx_length[i] = 0;
	}

	/* Keep an administration in the driver whether the internal
	   buffer is in use. That's what ret_busy is for */
	orp->or_tx.ret_busy = FALSE;
	orp->or_tx_busy = 0;

	orp->or_nicbuf_size = IEEE802_11_FRAME_LEN + ETH_HLEN;
}

/*****************************************************************************
 *                or_init_hw                                                 *
 *                                                                           *
 * Initialize hardware and prepare for intercepting the interrupts. At the   *
 * end, the card is up and running. May be called multiple times.            *
 *****************************************************************************/
static void or_init_hw(t_or * orp, ether_addr_t * addr)
{
#if VERBOSE
	int i;
#endif
	int err, s;
	hermes_t *hw = &(orp->hw);
	static int first_time = TRUE;

	/* first step in starting the card */
	if (hermes_cor_reset(hw) != 0) {
		printf ("%s: Failed to start the card\n", orp->or_name);
	}

	/* here begins the real things, yeah! ;) */
	if ((err = hermes_init (hw)) != 0) {
		printf ("error value of hermes_init(): %d\n", err);
	}

	if (first_time) {
		/* Get the MAC address (which is a data item in the card)*/
		or_readrids(hw, orp, addr);
	}

	/* Write a few rids to the card, e.g. WEP key*/
	or_writerids (hw, orp);

#if VERBOSE
	printf ("%s: Ethernet address ", orp->or_name);
	for (i = 0; i < 6; i++)
		printf("%x%c", addr->ea_addr[i], i < 5 ? ':' : '\n');
#endif

	/* Prepare internal TX buffer in the card */
	err = hermes_allocate (hw,
				   orp->or_nicbuf_size,
				   &(orp->or_tx.or_txfid));

	if (err)
		printf ("%s:Error %d allocating Tx buffer\n",
			orp->or_name, err);

	/* Establish event handle */
	if(first_time) {
		orp->or_hook_id = orp->or_irq;
		if ((s=sys_irqsetpolicy(orp->or_irq, 0,
			&orp->or_hook_id)) != OK)
			printf("orinoco: couldn't set IRQ policy: %d\n", s);

		if ((s=sys_irqenable(&orp->or_hook_id)) != OK)
			printf("orinoco: couldn't enable interrupts: %d\n", s);
		first_time = FALSE;
	}

	/* Tell the card which events should raise an interrupt to the OS */
	hermes_set_irqmask (hw, ORINOCO_INTEN);

	/* Enable operation */
	err = hermes_docmd_wait (hw, HERMES_CMD_ENABLE, 0, NULL);
	if (err)
		printf("%s: Error %d enabling MAC port\n", orp->or_name, err);
}

/*****************************************************************************
 *                or_readrids                                                *
 *                                                                           *
 * Read some default rids from the card. A rid (resource identifier)         *
 * is a data item in the firmware, some configuration variable.              *
 * In our case, we are mostly interested in the MAC address for now          *
 *****************************************************************************/
static void or_readrids(hermes_t * hw, t_or * orp, ether_addr_t * addr)
{
	int err;

	assert(addr != NULL);

	/* Read the MAC address */
	err = hermes_read_ltv (hw, USER_BAP, HERMES_RID_CNFOWNMACADDR,
			       ETH_ALEN, NULL, addr);
	if (err)
		printf("%s: failed to read MAC address!\n", orp->or_name);
}

/*****************************************************************************
 *                or_writerids                                               *
 *                                                                           *
 * Write some default rids to the card. A rid (resource identifier)          *
 * is a data item in the firmware, some configuration variable, e.g. WEP key *
 *****************************************************************************/
static void or_writerids(hermes_t * hw, t_or * orp)
{
	int err;
	struct hermes_idstring idbuf;
	static char essid[IW_ESSID_MAX_SIZE + 1];
	static char wepkey0[LARGE_KEY_LENGTH + 1];

	/* Set the MAC port */
	err = hermes_write_wordrec(hw, USER_BAP, HERMES_RID_CNFPORTTYPE, 1);
	if (err) {
		printf ("%s: Error %d setting port type\n", orp->or_name, err);
		return;
	}

	if (OK != env_get_param("essid", essid, sizeof(essid))) {
		essid[0] = 0;
	}

	if(strlen(essid) == 0) {
		printf("%s: no essid provided in boot monitor!\n",
			orp->or_name);
		printf("Hope you'll connect to the right network... \n");
	}

	/* Set the desired ESSID */
	idbuf.len = strlen (essid);
	memcpy (&idbuf.val, essid, sizeof (idbuf.val));

	err = hermes_write_ltv (hw, USER_BAP, HERMES_RID_CNFDESIREDSSID,
				HERMES_BYTES_TO_RECLEN (strlen (essid) + 2),
                                &idbuf);
	if (err) {
		printf ("%s: Error %d setting DESIREDSSID\n",
				orp->or_name, err);
		return;
	}

	if (OK != env_get_param("wep", wepkey0, sizeof(wepkey0))) {
		wepkey0[0] = 0;
	}

	switch(strlen(wepkey0)) {
		case 0:
			/* No key found in monitor, using no encryption */
		break;
		case LARGE_KEY_LENGTH:
			setup_wepkey(orp, wepkey0);
		break;
		default:
			printf("Invalid key provided. Has to be 13 chars\n");
		break;
	}
}

/*****************************************************************************
 *                setup_wepkey                                               *
 *                                                                           *
 * If a wepkey is provided in the boot monitor, set the necessary rids so    *
 * that the card will decrypt received data and encrypt data to send by      *
 * by default with this key.                                                 *
 * It appears that there is a severe bug in setting up WEP. If the driver    *
 * doesnt function properly, please turn WEP off.                            *
 *****************************************************************************/
static void setup_wepkey(t_or *orp, char *wepkey0)
{
	int default_key = 0, err = 0;
	hermes_t *hw = &(orp->hw);

	err = hermes_write_wordrec (hw, USER_BAP,
					HERMES_RID_CNFWEPDEFAULTKEYID,
					default_key);
	if (err)
		printf ("%s: Error %d setting the default WEP-key entry\n",
				orp->or_name, err);

	err = hermes_write_ltv (hw, USER_BAP,
				HERMES_RID_CNFDEFAULTKEY0,
				HERMES_BYTES_TO_RECLEN(LARGE_KEY_LENGTH),
				wepkey0);
	if (err)
		printf ("%s: Error %d setting the WEP-key0\n",
				orp->or_name, err);

	err = hermes_write_wordrec (hw, USER_BAP,
					HERMES_RID_CNFAUTHENTICATION,
					HERMES_AUTH_OPEN);
	if (err)
		printf ("%s: Error %d setting the authentication flag\n",
			orp->or_name, err);

	err = hermes_write_wordrec (hw, USER_BAP,
					HERMES_RID_CNFWEPFLAGS_INTERSIL,
					HERMES_WEP_PRIVACY_INVOKED);
	if (err)
		printf ("%s: Error %d setting the master wep setting flag\n",
			orp->or_name, err);

}

/*****************************************************************************
 *                or_handler                                                 *
 *                                                                           *
 * The handler which is called when the card generated an interrupt. Events  *
 * like EV_INFO and EV_RX have to be handled before an acknowledgement for   *
 * the event is returned to the card. See also the documentation             *
 *****************************************************************************/
static void or_handler(t_or *orp)
{
	int length;
	u16_t evstat, events, fid;
	hermes_t *hw = &(orp->hw);

beginning:
	/* Retrieve which kind of event happened */
	evstat = hermes_read_reg (hw, HERMES_EVSTAT);
	events = evstat;

	/* There are plenty of events possible. The more interesting events
	   are actually implemented. Whether the following events actually
	   raise an interrupt depends on the value of ORINOCO_INTEN. For more
	   information about the events, see the specification in pdf */

	/* Occurs at each tick of the auxiliary time */
	if (events & HERMES_EV_TICK) {
		events &= ~HERMES_EV_TICK;
	}
	/* Occurs when a wait time-out error is detected */
	if (events & HERMES_EV_WTERR) {
		events &= ~HERMES_EV_WTERR;
	}

	/* Occurs when an info frame is dropped because there is not enough
	   buffer space available */
	if (events & HERMES_EV_INFDROP) {
		events &= ~(HERMES_EV_INFDROP);
	}

	/* This AP-only event will be asserted at the beacon interval prior to
	   the DTIM interval */
	if (events & HERMES_EV_DTIM) {
		events &= ~(HERMES_EV_DTIM);
	}

 	/* Occurs when a command execution is completed */
	if (events & HERMES_EV_CMD) {
		events &= ~(HERMES_EV_CMD);
	}

	/* Occurs when the asynchronous transmission process is unsuccessfully
	   completed */
	if (events & HERMES_EV_TXEXC) {
		/* What buffer generated the event? Represented by an fid */
		fid = hermes_read_reg(hw, HERMES_TXCOMPLFID);
		if(fid == 0xFFFF) {
			/* Illegal fid found */
			printf("unexpected txexc_fid interrupted\n");
		}

		if (orp->or_tx.ret_busy)
			orp->or_tx_busy--;
		orp->or_tx.ret_busy = FALSE;

		orp->or_send_int = TRUE;
		orp->or_got_int = TRUE;

		/* To detect illegal fids */
		hermes_write_reg(hw, HERMES_TXCOMPLFID, 0xFFFF);
		events &= ~(HERMES_EV_TXEXC);
		/* We don't do anything else yet.
		 * Could be used for statistics */
	}

	/* Occurs when the asynchronous transmission process is successfully
	   completed */
	if (events & HERMES_EV_TX) {
		events &= ~(HERMES_EV_TX);
		/* Which buffer was sent, represented by an fid */
		fid = hermes_read_reg (hw, HERMES_TXCOMPLFID);
		if(fid == 0xFFFF) {
			/* Illegal fid found */
			printf("unexpected tx_fid interrupted\n");
		}

		if (orp->or_tx.ret_busy)
			orp->or_tx_busy--;
		orp->or_tx.ret_busy = FALSE;

		orp->or_send_int = TRUE;
		orp->or_got_int = TRUE;
		orp->or_tx_alive = TRUE;

		/* To detect illegal fids */
		hermes_write_reg(hw, HERMES_TXCOMPLFID, 0xFFFF);
		/* We don't do anything else when such event happens */
	}

	/* Occurs when an info frame is available in the card */
	if (events & HERMES_EV_INFO) {
		events &= ~(HERMES_EV_INFO);
		/* Process the information, inside the handler (!) */
		or_ev_info(orp);
	}

	/* Occurs when a TX buffer is available again for usage */
	if (events & HERMES_EV_ALLOC) {
		/* Which frame is now marked as free? */
		fid = hermes_read_reg (hw, HERMES_ALLOCFID);
		if (fid == 0xFFFF){
			/* An illegal frame identifier is found. Ignore */
			printf("Allocate event on unexpected fid\n");
			goto next;
		}

		/* To be able to detect illegal fids */
		hermes_write_reg(hw, HERMES_ALLOCFID, 0xFFFF);

		events &= ~(HERMES_EV_ALLOC);
	}

	/* Occurs when a frame is received */
	if (events & HERMES_EV_RX) {
		events &= ~(HERMES_EV_RX);

		/* If the last buffer is still filled with data, then we don't
		 * have any buffers available to store the data */
		if(orp->rx_length[orp->rx_last] != 0) {
			/* indeed, we are going to overwrite information
			 * in a buffer
			 */
		}

		/* Which buffer is storing the data (represented by a fid) */
		orp->rxfid[orp->rx_last]
				 = hermes_read_reg (hw, HERMES_RXFID);

		/* Get the packet from the card and store it in
		 * orp->rx_buf[orp->rx_last]. The length is returned by this
		 * function
		 */
		length = or_get_recvd_packet(orp, orp->rxfid[orp->rx_last],
						(orp->rx_buf[orp->rx_last]));

		if(length < 0) {
			/* Error happened. */
			printf("length < 0\n");
			goto next;
		} else {
			orp->rx_length[orp->rx_last] = length;
		}

		/* The next buffer will be used the next time, circularly */
		orp->rx_last++;
 		orp->rx_last %= NR_RX_BUFS;

		orp->or_got_int = TRUE;
	}
next:
	if (events) {
		printf("Unknown event: 0x%x\n", events);
	}

	/* Acknowledge to the card that the events have been processed. After
	 * this the card will assume we have processed any buffer which were in
	 * use for this event. */
	hermes_write_reg (hw, HERMES_EVACK, evstat);

	evstat = hermes_read_reg (hw, HERMES_EVSTAT);
	if(evstat != 0 && !(evstat & HERMES_EV_TICK)) {
		goto beginning;
	}
}

/*****************************************************************************
 *                or_alarm                                                   *
 *                                                                           *
 * Will be called regularly to see whether the driver has crashed. If that   *
 * condition is detected, reset the driver and card                          *
 *****************************************************************************/
static void or_alarm(clock_t __unused stamp)
{
	t_or *orp;

	/* Use a synchronous alarm instead of a watchdog timer. */
	sys_setalarm(sys_hz(), 0);

	orp = &or_state;

	if (orp->or_tx_busy == 0) {
		/* Assume that an idle system is alive */
		orp->or_tx_alive= TRUE;
		return;
	}

	if (orp->connected == 0) {
		orp->or_tx_alive= TRUE;
		return;
	}
	if (orp->or_tx_alive) {
		orp->or_tx_alive= FALSE;
		return;
	}

	printf("or_alarm: resetting card\n");

	orp->or_need_reset= TRUE;
	orp->or_got_int= TRUE;
	or_check_ints(orp);
}

/*****************************************************************************
 *                or_send                                                    *
 *                                                                           *
 * Send a packet, if possible                                                *
 *****************************************************************************/
static int or_send(struct netdriver_data *data, size_t size)
{
	size_t p, data_len, data_off;
	int err;
	struct ethhdr *eh;
	t_or *orp;
	hermes_t *hw;
	struct hermes_tx_descriptor desc;
	struct header_struct hdr;
	u16_t txfid;

	/* We need space for the max packet size itself, plus an ethernet
	 * header, plus 2 bytes so we can align the IP header on a
	 * 32bit boundary, plus 1 byte so we can read in odd length
	 * packets from the card, which has an IO granularity of 16
	 * bits */
	static u8_t databuf[IEEE802_11_DATA_LEN + ETH_HLEN + 2 + 1];

	orp = &or_state;
	hw = &(orp->hw);

	/* Switch off interrupts. The card is accessable via 2 BAPs, one for
	 * reading and one for writing. In theory these BAPs should be
	 * independent, but in practice, the are not. By switching off the
	 * interrupts of the card, the chances of one interfering with the
	 * other should be less
	 */
	/* ..except that this is not happening at all here. */

	txfid = orp->or_tx.or_txfid;

	if (orp->or_tx.ret_busy || orp->connected == 0) {
		/* there is no buffer in the card available */
		return SUSPEND;
	}

	/* Copy the data to be sent from the vector to the databuf */
	netdriver_copyin(data, 0, databuf, size);

	/* Zero out the rest of the buffer */
	memset(&databuf[size], 0, sizeof(databuf) - size);

	memset(&desc, 0, sizeof(desc));
	/* Reclaim the tx buffer once the data is sent (OK), or it is clear
	 * that transmission failed (EX). Reclaiming means that we can reuse
	 * the buffer again for transmission
	 */
	desc.tx_control = HERMES_TXCTRL_TX_OK | HERMES_TXCTRL_TX_EX;
	/* Actually, this reclaim bit is the only thing which needs to be set
	 * in the descriptor
	 */
	err = hermes_bap_pwrite(hw, USER_BAP, &desc, sizeof(desc), txfid, 0);
	if (err) {
		printf("hermes_bap_pwrite() descriptor error:resetting card\n");
		/* When this happens, the card is quite confused: it will not
		 * recover. Reset it
		 */
		or_reset();
		return OK; /* pretend the packet was sent anyway.. */
	}

	eh = (struct ethhdr *) databuf;
	/* Encapsulate Ethernet-II frames */
	if (ntohs(eh->h_proto) > 1500)	{
		/* Ethernet-II frame */
		data_len = size - ETH_HLEN;
		data_off = HERMES_802_3_OFFSET + sizeof (hdr);

		/* 802.3 header */
		memcpy(hdr.dest, eh->h_dest, ETH_ALEN);
		memcpy(hdr.src, eh->h_src, ETH_ALEN);
		hdr.len = htons(data_len + ENCAPS_OVERHEAD);

		/* 802.2 header */
		memcpy(&hdr.dsap, &encaps_hdr, sizeof (encaps_hdr));
		hdr.ethertype = eh->h_proto;

		err = hermes_bap_pwrite(hw, USER_BAP, &hdr, sizeof(hdr),
					 txfid, HERMES_802_3_OFFSET);
		if (err) {
			printf("%s: Error %d writing packet header to BAP\n",
				orp->or_name, err);
			return OK; /* pretend the packet was sent anyway.. */
		}

		p = ETH_HLEN;
	} else {
		/* IEEE 802.3 frame */
		data_len = size + ETH_HLEN;
		data_off = HERMES_802_3_OFFSET;
		p = 0;
	}

	/* Round up for odd length packets */
	err = hermes_bap_pwrite(hw, USER_BAP, (void *) &(databuf[p]),
	    RUP_EVEN(data_len), txfid, data_off);
	if (err) {
		printf("hermes_bap_pwrite(data): error %d\n", err);
		return OK; /* pretend the packet was sent anyway.. */
	}

	/* this should be before the docmd_wait. Cause otherwise the bit can
	 * be cleared in the handler (if irq's not off) before it is set
	 * and then 1 reset (ret_busy=false) is lost
	 */
	orp->or_tx.ret_busy = TRUE;
	orp->or_tx_busy++;

	/* Send the packet which was constructed in txfid */
	err = hermes_docmd_wait (hw, HERMES_CMD_TX | HERMES_CMD_RECL,
				 txfid, NULL);
	if (err) {
		printf("hermes_docmd_wait(TX|RECL): error %d\n", err);
		/* Mark the buffer as available again */
		orp->or_tx.ret_busy = FALSE;
		orp->or_tx_busy--;
		return OK; /* pretend the packet was sent anyway.. */
	}

	return OK;
}

/*****************************************************************************
 *                or_ev_info                                                 *
 *                                                                           *
 * Process information which comes in from the card                          *
 *****************************************************************************/
static void or_ev_info(t_or * orp)
{
	struct hermes_tallies_frame tallies;
	struct hermes_linkstatus linkstatus;
	u16_t newstatus;
	u16_t infofid;
	int err, len, type;
	hermes_t *hw = &orp->hw;

	struct {
		u16_t len;
		u16_t type;
	} info;

	infofid = hermes_read_reg (hw, HERMES_INFOFID);
	err = hermes_bap_pread (hw, IRQ_BAP, &info, sizeof (info), infofid,
				0);
	if (err) {
		printf ("%s: error %d reading info frame.\n", orp->or_name,
			err);
		return;
	}

	len = HERMES_RECLEN_TO_BYTES (info.len);
	type = info.type;

	switch (type) {
	case HERMES_INQ_TALLIES:
		if (len > sizeof(tallies)) {
			printf("%s: Tallies frame too long ", orp->or_name);
			printf("(%d bytes)\n", len);
			len = sizeof (tallies);
		}
		hermes_read_words(hw, HERMES_DATA1, (void *)&tallies, len / 2);
		/* TODO: do something with the tallies structure */
		break;

	case HERMES_INQ_LINKSTATUS:
		if (len != sizeof(linkstatus))	{
			printf("%s: Unexpected size for linkstatus ",
				orp->or_name);
			printf("frame (%d bytes)\n", len);
		}

		hermes_read_words(hw, HERMES_DATA1, (void *)&linkstatus,
			len / 2);
		newstatus = linkstatus.linkstatus;

		if ((newstatus == HERMES_LINKSTATUS_CONNECTED)
		    || (newstatus == HERMES_LINKSTATUS_AP_CHANGE)
		    || (newstatus == HERMES_LINKSTATUS_AP_IN_RANGE)) {
			orp->connected = 1;
			orp->or_send_int = TRUE;
			orp->or_got_int = TRUE;
		}
		else if ((newstatus == HERMES_LINKSTATUS_NOT_CONNECTED)
		    || (newstatus == HERMES_LINKSTATUS_DISCONNECTED)
		    || (newstatus == HERMES_LINKSTATUS_AP_OUT_OF_RANGE)
		    || (newstatus == HERMES_LINKSTATUS_ASSOC_FAILED)) {
			orp->connected = 0;
		}

		if (newstatus != orp->last_linkstatus)
			print_linkstatus(orp, newstatus);

		orp->last_linkstatus = newstatus;

		break;
	default:
		printf("%s:Unknown information frame received (type %04x).\n",
			orp->or_name, type);
		break;
	}
}

/*****************************************************************************
 *                or_print_linkstatus                                        *
 *                                                                           *
 * Process information which comes in from the card                          *
 *****************************************************************************/
static void print_linkstatus(t_or * orp, u16_t status)
{
	int err;
	u16_t d;
	char *s;
	hermes_t *hw = &(orp->hw);

	switch (status) {
	case HERMES_LINKSTATUS_NOT_CONNECTED:
		s = "Not Connected";
		break;
	case HERMES_LINKSTATUS_CONNECTED:
		s = "Connected";
		break;
	case HERMES_LINKSTATUS_DISCONNECTED:
		s = "Disconnected";
		break;
	case HERMES_LINKSTATUS_AP_CHANGE:
		s = "AP Changed";
		break;
	case HERMES_LINKSTATUS_AP_OUT_OF_RANGE:
		s = "AP Out of Range";
		break;
	case HERMES_LINKSTATUS_AP_IN_RANGE:
		s = "AP In Range";
		break;
	case HERMES_LINKSTATUS_ASSOC_FAILED:
		s = "Association Failed";
		break;
	default:
		s = "UNKNOWN";
	}

	printf("%s: link status: %s, ", orp->or_name, s);

	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CURRENTCHANNEL, &d);
	if (err) {
		printf("error %d\n", err);
		return;
	}
	printf("channel: %d, freq: %ld MHz\n", d, (channel_frequency[d-1]));
}

/*****************************************************************************
 *                or_check_ints                                              *
 *                                                                           *
 * Process events which have been postponed in the interrupt handler         *
 *****************************************************************************/
static void or_check_ints(t_or * orp)
{
	if (!orp->or_got_int)
		return;
	orp->or_got_int = FALSE;

	if (orp->or_need_reset)
		or_reset();

	if (orp->rx_first != orp->rx_last)
		netdriver_recv();

	if (orp->or_send_int)
		netdriver_send();
}

/*****************************************************************************
 *                is_ethersnap                                               *
 *                                                                           *
 * is there an LLC and SNAP header in the ethernet packet? The inet task     *
 * isn't very interested in it...                                            *
 *****************************************************************************/
static int is_ethersnap(struct header_struct *hdr)
{
	/* We de-encapsulate all packets which, a) have SNAP headers
	 * (i.e. SSAP=DSAP=0xaa and CTRL=0x3 in the 802.2 LLC header
	 * and where b) the OUI of the SNAP header is 00:00:00 or
	 * 00:00:f8 - we need both because different APs appear to use
	 * different OUIs for some reason */
	return (memcmp(&hdr->dsap, &encaps_hdr, 5) == 0)
		&& ( (hdr->oui[2] == 0x00) || (hdr->oui[2] == 0xf8) );
}

/*****************************************************************************
 *                or_recv                                                    *
 *                                                                           *
 * Receive a packet, if one is available                                     *
 *****************************************************************************/
static ssize_t or_recv(struct netdriver_data *data, size_t max)
{
	t_or *orp;
	u8_t *databuf;
	size_t length;

	orp = &or_state;

	if (orp->rx_first == orp->rx_last)
		return SUSPEND;

	/* store the pointer to this data in databuf */
	databuf = &(orp->rx_buf[orp->rx_first][0]);
	length = orp->rx_length[orp->rx_first];

	orp->rxfid[orp->rx_first] = NO_FID;
	orp->rx_length[orp->rx_first] = 0;

	/* Next time, the next buffer with data will be retrieved */
	orp->rx_first++;
	orp->rx_first %= NR_RX_BUFS;

	if (length > max)
		length = max;

	netdriver_copyout(data, 0, databuf, length);

	orp->or_stat.ets_packetR++;

	return length;
}

/*****************************************************************************
 *            or_get_recvd_packet                                            *
 *                                                                           *
 * The card has received data. Retrieve the data from the card and put it    *
 * in a buffer in the driver (in the orp structure)                          *
 *****************************************************************************/
static int or_get_recvd_packet(t_or *orp, u16_t rxfid, u8_t *databuf)
{
	struct hermes_rx_descriptor desc;
	hermes_t *hw;
	struct header_struct hdr;
	int err, length, offset;
	u16_t status;

	memset(databuf, 0, IEEE802_11_FRAME_LEN);

	hw = &(orp->hw);

	/* Read the data from the buffer in the card which holds the data.
	 * First get the descriptor which will tell us whether the packet is
	 * healthy*/
	err = hermes_bap_pread (hw, IRQ_BAP, &desc, sizeof (desc), rxfid, 0);
	if (err) {
		printf("Orinoco: error %d reading Rx descriptor. "
			"Frame dropped\n", err);
		orp->or_stat.ets_recvErr++;
		return -1;
	}

	status = desc.status;

	if (status & HERMES_RXSTAT_ERR)	{
		if (status & HERMES_RXSTAT_UNDECRYPTABLE) {
			printf("Error reading Orinoco Rx descriptor.Dropped");
		} else {
			orp->or_stat.ets_CRCerr++;
			printf("Orinoco: Bad CRC on Rx. Frame dropped\n");
		}
		orp->or_stat.ets_recvErr++;
		return -1;
	}

	/* For now we ignore the 802.11 header completely, assuming
	   that the card's firmware has handled anything vital. The only
	   thing we want to know is the length of the received data */
	err = hermes_bap_pread (hw, IRQ_BAP, &hdr, sizeof (hdr),
				rxfid, HERMES_802_3_OFFSET);

	if (err) {
		printf("Orinoco: error %d reading frame header. "
			"Frame dropped\n", err);
		orp->or_stat.ets_recvErr++;
		return -1;
	}

	length = ntohs (hdr.len);

	/* Sanity checks */
	if (length < 3)	{
		/* No for even an 802.2 LLC header */
		printf("Orinoco: error in frame length: length = %d\n",
			length);
		/* orp->or_stat.ets_recvErr++; */
		return -1;
	}

	if (length > IEEE802_11_DATA_LEN) {
		printf("Orinoco: Oversized frame received (%d bytes)\n",
			length);
		orp->or_stat.ets_recvErr++;
		return -1;
	}

	length += sizeof (struct ethhdr);
	offset = HERMES_802_3_OFFSET;

	/* Read the interesting parts of the data to the drivers memory. This
	 * would be everything from the 802.3 layer and up */
	err = hermes_bap_pread (hw,
				IRQ_BAP, (void *) databuf, RUP_EVEN (length),
				rxfid, offset);

	if (err) {
		printf("Orinoco: error doing hermes_bap_pread()\n");
		orp->or_stat.ets_recvErr++;
		return -1;
	}

	/* Some types of firmware give us the SNAP and OUI headers. Remove
	 * these.
	 */
	if (is_ethersnap(&hdr))	{
		length -= 8;

		memcpy(databuf + ETH_ALEN * 2,
			databuf + sizeof(struct header_struct) - 2,
			length - ETH_ALEN * 2);
	}

	if(length<60) length=60;

	return length;
}

/*****************************************************************************
 *            or_stat                                                        *
 *                                                                           *
 * Return the statistics structure. The statistics aren't updated until now, *
 * so this won't return much interesting yet.                                *
 *****************************************************************************/
static void or_stat(eth_stat_t *stat)
{
	memcpy(stat, &or_state.or_stat, sizeof(*stat));
}
