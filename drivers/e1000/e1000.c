/**
 * @file e1000.c
 *
 * @brief This file contains a device driver for Intel Pro/1000 
 *        Gigabit Ethernet Controllers.
 */

#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <stdlib.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <machine/pci.h>
#include <minix/ds.h>
#include <minix/vm.h>
#include <minix/timers.h>
#include <sys/mman.h>
#include "assert.h"
#include "e1000.h"
#include "e1000_hw.h"
#include "e1000_reg.h"
#include "e1000_pci.h"

static int e1000_instance;
static e1000_t e1000_state;

static void e1000_init(message *mp);
static void e1000_init_pci(void);
static int e1000_probe(e1000_t *e, int skip);
static int e1000_init_hw(e1000_t *e);
static void e1000_init_addr(e1000_t *e);
static void e1000_init_buf(e1000_t *e);
static void e1000_reset_hw(e1000_t *e);
static void e1000_writev_s(message *mp, int from_int);
static void e1000_readv_s(message *mp, int from_int);
static void e1000_getstat_s(message *mp);
static void e1000_interrupt(message *mp);
static int e1000_link_changed(e1000_t *e);
static void e1000_stop(e1000_t *e);
static uint32_t e1000_reg_read(e1000_t *e, uint32_t reg);
static void e1000_reg_write(e1000_t *e, uint32_t reg, uint32_t value);
static void e1000_reg_set(e1000_t *e, uint32_t reg, uint32_t value);
static void e1000_reg_unset(e1000_t *e, uint32_t reg, uint32_t value);
static u16_t eeprom_eerd(void *e, int reg);
static u16_t eeprom_ich(void *e, int reg);
static int eeprom_ich_init(e1000_t *e);
static int eeprom_ich_cycle(const e1000_t *e, u32_t timeout);
static void reply(e1000_t *e);
static void mess_reply(message *req, message *reply);

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

    /*
     * Enter the main driver loop.
     */
    while (TRUE)
    {
	if ((r= netdriver_receive(ANY, &m, &ipc_status)) != OK)
	{
	    panic("netdriver_receive failed: %d", r);
	}

	if (is_ipc_notify(ipc_status))
	{
	    switch (_ENDPOINT_P(m.m_source))
	    {
                case HARDWARE:
		    e1000_interrupt(&m);
		    break;

		case CLOCK:
                    break;
	    }
	    continue;
	}
	switch (m.m_type)
	{
	    case DL_WRITEV_S:   e1000_writev_s(&m, FALSE);	break;
	    case DL_READV_S:    e1000_readv_s(&m, FALSE);	break;
	    case DL_CONF:	e1000_init(&m);			break;
	    case DL_GETSTAT_S:  e1000_getstat_s(&m);		break;
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
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the e1000 driver. */
    long v;
    int r;

    v = 0;
    (void) env_parse("instance", "d", 0, &v, 0, 255);
    e1000_instance = (int) v;

    /* Clear state. */
    memset(&e1000_state, 0, sizeof(e1000_state));

    /* Perform calibration. */
    if((r = tsc_calibrate()) != OK)
    {
        panic("tsc_calibrate failed: %d", r);
    }

    /* Announce we are up! */
    netdriver_announce();

    return(OK);
}

/*===========================================================================*
 *			   sef_cb_signal_handler			     *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
    e1000_t *e;
    e = &e1000_state;

    E1000_DEBUG(3, ("%s: got signal\n", e->name));

    /* Only check for termination signal, ignore anything else. */
    if (signo != SIGTERM) return;

    e1000_stop(e);
}

/*===========================================================================*
 *				e1000_init				     *
 *===========================================================================*/
static void e1000_init(message *mp)
{
    static int first_time = 1;
    message reply_mess;
    e1000_t *e;

    E1000_DEBUG(3, ("e1000: init()\n"));

    /* Configure PCI devices, if needed. */
    if (first_time)
    {
	first_time = 0;
	e1000_init_pci(); 
    }
    e = &e1000_state;

    /* Initialize hardware, if needed. */
    if (!(e->status & E1000_ENABLED) && !(e1000_init_hw(e)))
    {
        reply_mess.m_type  = DL_CONF_REPLY;
        reply_mess.m_netdrv_net_dl_conf.stat = ENXIO;
        mess_reply(mp, &reply_mess);
        return;
    }
    /* Reply back to INET. */
    reply_mess.m_type  = DL_CONF_REPLY;
    reply_mess.m_netdrv_net_dl_conf.stat = OK;
    memcpy(reply_mess.m_netdrv_net_dl_conf.hw_addr, e->address.ea_addr,
	    sizeof(reply_mess.m_netdrv_net_dl_conf.hw_addr));
    mess_reply(mp, &reply_mess);
}

/*===========================================================================*
 *				e1000_int_pci				     *
 *===========================================================================*/
static void e1000_init_pci()
{
    e1000_t *e;

    /* Initialize the PCI bus. */
    pci_init();

    /* Try to detect e1000's. */
    e = &e1000_state;
    strlcpy(e->name, "e1000#0", sizeof(e->name));
    e->name[6] += e1000_instance;
    e1000_probe(e, e1000_instance);
}

/*===========================================================================*
 *				e1000_probe				     *
 *===========================================================================*/
static int e1000_probe(e1000_t *e, int skip)
{
    int r, devind, ioflag;
    u16_t vid, did, cr;
    u32_t status[2];
    u32_t base, size;
    u32_t gfpreg, sector_base_addr;
    char *dname;

    E1000_DEBUG(3, ("%s: probe()\n", e->name));

    /*
     * Attempt to iterate the PCI bus. Start at the beginning.
     */
    if ((r = pci_first_dev(&devind, &vid, &did)) == 0)
    {
	return FALSE;
    }
    /* Loop devices on the PCI bus. */
    while (skip--)
    {
	E1000_DEBUG(3, ("%s: probe() devind %d vid 0x%x did 0x%x\n",
				e->name, devind, vid, did));

	if (!(r = pci_next_dev(&devind, &vid, &did)))
	{
	    return FALSE;
	}
    }
    /*
     * Successfully detected an Intel Pro/1000 on the PCI bus.
     */
    e->status |= E1000_DETECTED;
    e->eeprom_read = eeprom_eerd;
    
    /*
     * Set card specific properties.
     */
    switch (did)
    {
        case E1000_DEV_ID_ICH10_D_BM_LM:
        case E1000_DEV_ID_ICH10_R_BM_LF:
            e->eeprom_read = eeprom_ich;
            break;

    	case E1000_DEV_ID_82540EM:
	case E1000_DEV_ID_82545EM:
	    e->eeprom_done_bit = (1 << 4);
	    e->eeprom_addr_off =  8;
	    break;

	default:
	    e->eeprom_done_bit = (1 << 1);
	    e->eeprom_addr_off =  2;
	    break;
    }

    /* Inform the user about the new card. */
    if (!(dname = pci_dev_name(vid, did)))
    {
        dname = "Intel Pro/1000 Gigabit Ethernet Card";
    }
    E1000_DEBUG(1, ("%s: %s (%04x/%04x/%02x) at %s\n",
		     e->name, dname, vid, did, e->revision, 
		     pci_slot_name(devind)));

    /* Reserve PCI resources found. */
    if ((r = pci_reserve_ok(devind)) != OK)
    {
        panic("failed to reserve PCI device: %d", r);
    }
    /* Read PCI configuration. */
    e->irq   = pci_attr_r8(devind, PCI_ILR);

    if ((r = pci_get_bar(devind, PCI_BAR, &base, &size, &ioflag)) != OK)
		panic("failed to get PCI BAR (%d)", r);
    if (ioflag) panic("PCI BAR is not for memory");

    e->regs  = vm_map_phys(SELF, (void *) base, size);
    if (e->regs == (u8_t *) -1) {
		panic("failed to map hardware registers from PCI");
    }

    /* FIXME: enable DMA bus mastering if necessary. This is disabled by
     * default on VMware. Eventually, the PCI driver should deal with this.
     */
    cr = pci_attr_r16(devind, PCI_CR);
    if (!(cr & PCI_CR_MAST_EN))
		pci_attr_w16(devind, PCI_CR, cr | PCI_CR_MAST_EN);

    /* Optionally map flash memory. */
    if (did != E1000_DEV_ID_82540EM &&
	did != E1000_DEV_ID_82545EM &&
	did != E1000_DEV_ID_82540EP &&
	pci_attr_r32(devind, PCI_BAR_2))
    {
        size_t flash_size;

        /* 82566/82567/82562V series support mapping 4kB of flash memory */
        switch(did)
        {
            case E1000_DEV_ID_ICH10_D_BM_LM:
            case E1000_DEV_ID_ICH10_R_BM_LF:
                flash_size = 0x1000;
                break;
            default:
                flash_size = 0x10000;
        }

        if ((e->flash = vm_map_phys(SELF,
                                    (void *) pci_attr_r32(devind, PCI_BAR_2),
                                    flash_size)) == MAP_FAILED) {
            panic("e1000: couldn't map in flash.");
        }

	gfpreg = E1000_READ_FLASH_REG(e, ICH_FLASH_GFPREG);
        /*
         * sector_base_addr is a "sector"-aligned address (4096 bytes)
         */
        sector_base_addr = gfpreg & FLASH_GFPREG_BASE_MASK;

        /* flash_base_addr is byte-aligned */
        e->flash_base_addr = sector_base_addr << FLASH_SECTOR_ADDR_SHIFT;
    }
    /*
     * Output debug information.
     */
    status[0] = e1000_reg_read(e, E1000_REG_STATUS);    
    E1000_DEBUG(3, ("%s: MEM at %p, IRQ %d\n",
		    e->name, e->regs, e->irq));
    E1000_DEBUG(3, ("%s: link %s, %s duplex\n",
		    e->name, status[0] & 3 ? "up"   : "down",
			     status[0] & 1 ? "full" : "half"));
    return TRUE;
}

/*===========================================================================*
 *				e1000_init_hw				     *
 *===========================================================================*/
static int e1000_init_hw(e)
e1000_t *e;
{
    int r, i;

    e->status  |= E1000_ENABLED;
    e->irq_hook = e->irq;

    /*
     * Set the interrupt handler and policy. Do not automatically
     * re-enable interrupts. Return the IRQ line number on interrupts.
     */
    if ((r = sys_irqsetpolicy(e->irq, 0, &e->irq_hook)) != OK)
    {
        panic("sys_irqsetpolicy failed: %d", r);
    }
    if ((r = sys_irqenable(&e->irq_hook)) != OK)
    {
	panic("sys_irqenable failed: %d", r);
    }
    /* Reset hardware. */
    e1000_reset_hw(e);

    /*
     * Initialize appropriately, according to section 14.3 General Configuration
     * of Intel's Gigabit Ethernet Controllers Software Developer's Manual.
     */
    e1000_reg_set(e,   E1000_REG_CTRL, E1000_REG_CTRL_ASDE | E1000_REG_CTRL_SLU);
    e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_LRST);
    e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_PHY_RST);
    e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_ILOS);
    e1000_reg_write(e, E1000_REG_FCAL, 0);
    e1000_reg_write(e, E1000_REG_FCAH, 0);
    e1000_reg_write(e, E1000_REG_FCT,  0);
    e1000_reg_write(e, E1000_REG_FCTTV, 0);
    e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_VME);

    /* Clear Multicast Table Array (MTA). */
    for (i = 0; i < 128; i++)
    {
	e1000_reg_write(e, E1000_REG_MTA + i, 0);
    }
    /* Initialize statistics registers. */
    for (i = 0; i < 64; i++)
    {
	e1000_reg_write(e, E1000_REG_CRCERRS + (i * 4), 0);
    }
    /*
     * Aquire MAC address and setup RX/TX buffers.
     */
    e1000_init_addr(e);
    e1000_init_buf(e);
    
    /* Enable interrupts. */
    e1000_reg_set(e,   E1000_REG_IMS, E1000_REG_IMS_LSC  |
				      E1000_REG_IMS_RXO  |
				      E1000_REG_IMS_RXT  |
				      E1000_REG_IMS_TXQE |
				      E1000_REG_IMS_TXDW);
    return TRUE;
}

/*===========================================================================*
 *				e1000_init_addr				     *
 *===========================================================================*/
static void e1000_init_addr(e)
e1000_t *e;
{
    static char eakey[]= E1000_ENVVAR "#_EA";
    static char eafmt[]= "x:x:x:x:x:x";
    u16_t word;
    int i;
    long v;

    /*
     * Do we have a user defined ethernet address?
     */
    eakey[sizeof(E1000_ENVVAR)-1] = '0' + e1000_instance;

    for (i= 0; i < 6; i++)
    {
        if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
            break;
	else
    	    e->address.ea_addr[i]= v;
    }
    /*
     * If that fails, read Ethernet Address from EEPROM.
     */
    if (i != 6)
    {
	for (i = 0; i < 3; i++)
	{
	    word = e->eeprom_read(e, i);
	    e->address.ea_addr[(i * 2)]     = (word & 0xff);
	    e->address.ea_addr[(i * 2) + 1] = (word & 0xff00) >> 8;
	}
    }
    /*
     * Set Receive Address.
     */
    e1000_reg_write(e, E1000_REG_RAL, *(u32_t *)(&e->address.ea_addr[0]));
    e1000_reg_write(e, E1000_REG_RAH, *(u16_t *)(&e->address.ea_addr[4]));
    e1000_reg_set(e,   E1000_REG_RAH,   E1000_REG_RAH_AV);
    e1000_reg_set(e,   E1000_REG_RCTL,  E1000_REG_RCTL_MPE);

    E1000_DEBUG(3, ("%s: Ethernet Address %x:%x:%x:%x:%x:%x\n", e->name,
		    e->address.ea_addr[0], e->address.ea_addr[1],
		    e->address.ea_addr[2], e->address.ea_addr[3],
		    e->address.ea_addr[4], e->address.ea_addr[5]));
}

/*===========================================================================*
 *				e1000_init_buf				     *
 *===========================================================================*/
static void e1000_init_buf(e)
e1000_t *e;
{
    phys_bytes rx_buff_p;
    phys_bytes tx_buff_p;
    int i;

    /* Number of descriptors. */
    e->rx_desc_count = E1000_RXDESC_NR;
    e->tx_desc_count = E1000_TXDESC_NR;

    /*
     * First, allocate the receive descriptors.
     */
    if (!e->rx_desc)
    {
    	if ((e->rx_desc = alloc_contig(sizeof(e1000_rx_desc_t) * 
				       e->rx_desc_count, AC_ALIGN4K, 
				      &e->rx_desc_p)) == NULL) {
		panic("failed to allocate RX descriptors");
    	}
    	memset(e->rx_desc, 0, sizeof(e1000_rx_desc_t) * e->rx_desc_count);

       /*
        * Allocate 2048-byte buffers.
        */
	e->rx_buffer_size = E1000_RXDESC_NR * E1000_IOBUF_SIZE;
    
	/* Attempt to allocate. */
	if ((e->rx_buffer = alloc_contig(e->rx_buffer_size,
					 AC_ALIGN4K, &rx_buff_p)) == NULL)
	{
	    panic("failed to allocate RX buffers");
	}
	/* Setup receive descriptors. */
	for (i = 0; i < E1000_RXDESC_NR; i++)
	{
	    e->rx_desc[i].buffer = rx_buff_p + (i * E1000_IOBUF_SIZE);
	}
    }
    /*
     * Then, allocate transmit descriptors.
     */
    if (!e->tx_desc)
    {
        if ((e->tx_desc = alloc_contig(sizeof(e1000_tx_desc_t) * 
				       e->tx_desc_count, AC_ALIGN4K, 
				      &e->tx_desc_p)) == NULL) {
		panic("failed to allocate TX descriptors");
    	}
    	memset(e->tx_desc, 0, sizeof(e1000_tx_desc_t) * e->tx_desc_count);

       /*
        * Allocate 2048-byte buffers.
        */
	e->tx_buffer_size = E1000_TXDESC_NR * E1000_IOBUF_SIZE;
    
	/* Attempt to allocate. */
	if ((e->tx_buffer = alloc_contig(e->tx_buffer_size,
					 AC_ALIGN4K, &tx_buff_p)) == NULL)
	{
	    panic("failed to allocate TX buffers");
	}
	/* Setup transmit descriptors. */
	for (i = 0; i < E1000_TXDESC_NR; i++)
	{
	    e->tx_desc[i].buffer = tx_buff_p + (i * E1000_IOBUF_SIZE);
	}
    }
    /*
     * Setup the receive ring registers.
     */
    e1000_reg_write(e, E1000_REG_RDBAL, e->rx_desc_p);
    e1000_reg_write(e, E1000_REG_RDBAH, 0);
    e1000_reg_write(e, E1000_REG_RDLEN, e->rx_desc_count *
					sizeof(e1000_rx_desc_t));
    e1000_reg_write(e, E1000_REG_RDH,   0);
    e1000_reg_write(e, E1000_REG_RDT,   e->rx_desc_count - 1);
    e1000_reg_unset(e, E1000_REG_RCTL,  E1000_REG_RCTL_BSIZE);
    e1000_reg_set(e,   E1000_REG_RCTL,  E1000_REG_RCTL_EN);
    
    /*
     * Setup the transmit ring registers.
     */
    e1000_reg_write(e, E1000_REG_TDBAL, e->tx_desc_p);
    e1000_reg_write(e, E1000_REG_TDBAH, 0);
    e1000_reg_write(e, E1000_REG_TDLEN, e->tx_desc_count *
					sizeof(e1000_tx_desc_t));
    e1000_reg_write(e, E1000_REG_TDH,   0);
    e1000_reg_write(e, E1000_REG_TDT,   0);
    e1000_reg_set(  e, E1000_REG_TCTL,  E1000_REG_TCTL_EN | E1000_REG_TCTL_PSP);
}

/*===========================================================================*
 *				e1000_reset_hw				     *
 *===========================================================================*/
static void e1000_reset_hw(e)
e1000_t *e;
{
    /* Assert a Device Reset signal. */
    e1000_reg_set(e, E1000_REG_CTRL, E1000_REG_CTRL_RST);

    /* Wait one microsecond. */
    tickdelay(1);
}

/*===========================================================================*
 *				e1000_writev_s				     *
 *===========================================================================*/
static void e1000_writev_s(mp, from_int)
message *mp;
int from_int;
{
    e1000_t *e = &e1000_state;
    e1000_tx_desc_t *desc;
    iovec_s_t iovec[E1000_IOVEC_NR];
    int r, head, tail, i, bytes = 0, size;

    E1000_DEBUG(3, ("e1000: writev_s(%p,%d)\n", mp, from_int));

    /* Are we called from the interrupt handler? */
    if (!from_int)
    {
	/* We cannot write twice simultaneously. 
	assert(!(e->status & E1000_WRITING)); */
    
	/* Copy write message. */
	e->tx_message = *mp;
	e->client = mp->m_source;
	e->status |= E1000_WRITING;

	/* Must be a sane vector count. */
	assert(e->tx_message.m_net_netdrv_dl_writev_s.count > 0);
	assert(e->tx_message.m_net_netdrv_dl_writev_s.count < E1000_IOVEC_NR);

	/*
	 * Copy the I/O vector table.
	 */
	if ((r = sys_safecopyfrom(e->tx_message.m_source,
				  e->tx_message.m_net_netdrv_dl_writev_s.grant, 0,
				  (vir_bytes) iovec,
				  e->tx_message.m_net_netdrv_dl_writev_s.count *
				  sizeof(iovec_s_t))) != OK)
	{
	    panic("sys_safecopyfrom() failed: %d", r);
	}
	/* Find the head, tail and current descriptors. */
	head =  e1000_reg_read(e, E1000_REG_TDH);
	tail =  e1000_reg_read(e, E1000_REG_TDT);
	desc = &e->tx_desc[tail];
	
	E1000_DEBUG(4, ("%s: head=%d, tail=%d\n",
	                 e->name, head, tail));

	/* Loop vector elements. */
	for (i = 0; i < e->tx_message.m_net_netdrv_dl_writev_s.count; i++)
	{
	    size = iovec[i].iov_size < (E1000_IOBUF_SIZE - bytes) ?
		   iovec[i].iov_size : (E1000_IOBUF_SIZE - bytes);

	    E1000_DEBUG(4, ("iovec[%d] = %d\n", i, size));

	    /* Copy bytes to TX queue buffers. */
	    if ((r = sys_safecopyfrom(e->tx_message.m_source,
				     iovec[i].iov_grant, 0,
				     (vir_bytes) e->tx_buffer +
				     (tail * E1000_IOBUF_SIZE),
				      size)) != OK)
	    {
		panic("sys_safecopyfrom() failed: %d", r);
	    }
	    /* Mark this descriptor ready. */
	    desc->status  = 0;
	    desc->command = 0;
	    desc->length  = size;

	    /* Marks End-of-Packet. */
	    if (i == e->tx_message.m_net_netdrv_dl_writev_s.count - 1)
	    {
		desc->command = E1000_TX_CMD_EOP |
			        E1000_TX_CMD_FCS |
				E1000_TX_CMD_RS;
	    }
	    /* Move to next descriptor. */
	    tail   = (tail + 1) % e->tx_desc_count;
	    bytes +=  size;
            desc   = &e->tx_desc[tail];
	}
	/* Increment tail. Start transmission. */
	e1000_reg_write(e, E1000_REG_TDT,  tail);

    	E1000_DEBUG(2, ("e1000: wrote %d byte packet\n", bytes));
    }
    else
    {
	e->status |= E1000_TRANSMIT;
    }
    reply(e);
}

/*===========================================================================*
 *				e1000_readv_s				     *
 *===========================================================================*/
static void e1000_readv_s(mp, from_int)
message *mp;
int from_int;
{
    e1000_t *e = &e1000_state;
    e1000_rx_desc_t *desc;
    iovec_s_t iovec[E1000_IOVEC_NR];
    int i, r, head, tail, cur, bytes = 0, size;

    E1000_DEBUG(3, ("e1000: readv_s(%p,%d)\n", mp, from_int));

    /* Are we called from the interrupt handler? */
    if (!from_int)
    {
	e->rx_message = *mp;
	e->client     = mp->m_source;
	e->status    |= E1000_READING;
	e->rx_size    = 0;
	
	assert(e->rx_message.m_net_netdrv_dl_readv_s.count > 0);
	assert(e->rx_message.m_net_netdrv_dl_readv_s.count < E1000_IOVEC_NR);
    }
    if (e->status & E1000_READING)
    {
	/*
	 * Copy the I/O vector table first.
	 */
	if ((r = sys_safecopyfrom(e->rx_message.m_source,
				  e->rx_message.m_net_netdrv_dl_readv_s.grant, 0,
				  (vir_bytes) iovec,
				  e->rx_message.m_net_netdrv_dl_readv_s.count *
				  sizeof(iovec_s_t))) != OK)
	{
	    panic("sys_safecopyfrom() failed: %d", r);
	}
	/* Find the head, tail and current descriptors. */
	head = e1000_reg_read(e, E1000_REG_RDH);
	tail = e1000_reg_read(e, E1000_REG_RDT);
	cur  = (tail + 1) % e->rx_desc_count;
	desc = &e->rx_desc[cur];
	
	/*
	 * Only handle one packet at a time.
	 */
	if (!(desc->status & E1000_RX_STATUS_EOP))
	{
    	    reply(e);
	    return;
	}
	E1000_DEBUG(4, ("%s: head=%x, tail=%d\n",
		          e->name, head, tail));

	/*
	 * Copy to vector elements.
	 */    
	for (i = 0; i < e->rx_message.m_net_netdrv_dl_readv_s.count &&
		bytes < desc->length; i++)
	{
	    size = iovec[i].iov_size < (desc->length - bytes) ?
		   iovec[i].iov_size : (desc->length - bytes);
	
	    E1000_DEBUG(4, ("iovec[%d] = %lu[%d]\n",
			  i, iovec[i].iov_size, size));

	    if ((r = sys_safecopyto(e->rx_message.m_source, iovec[i].iov_grant,
				   0, (vir_bytes) e->rx_buffer + bytes + 
				   (cur * E1000_IOBUF_SIZE),
				    size)) != OK)
	    {
		panic("sys_safecopyto() failed: %d", r);
	    }
	    bytes += size;
	}
	desc->status = 0;

	/*
	 * Update state.
	 */
	e->rx_size   = bytes;
	e->status   |= E1000_RECEIVED;
	E1000_DEBUG(2, ("e1000: got %d byte packet\n", e->rx_size));

	/* Increment tail. */
	e1000_reg_write(e, E1000_REG_RDT, (tail + 1) % e->rx_desc_count);
    }
    reply(e);
}

/*===========================================================================*
 *				e1000_getstat_s				     *
 *===========================================================================*/
static void e1000_getstat_s(mp)
message *mp;
{
    int r;
    eth_stat_t stats;
    e1000_t *e = &e1000_state;

    E1000_DEBUG(3, ("e1000: getstat_s()\n"));

    stats.ets_recvErr   = e1000_reg_read(e, E1000_REG_RXERRC);
    stats.ets_sendErr   = 0;
    stats.ets_OVW       = 0;
    stats.ets_CRCerr    = e1000_reg_read(e, E1000_REG_CRCERRS);
    stats.ets_frameAll  = 0;
    stats.ets_missedP   = e1000_reg_read(e, E1000_REG_MPC);
    stats.ets_packetR   = e1000_reg_read(e, E1000_REG_TPR);
    stats.ets_packetT   = e1000_reg_read(e, E1000_REG_TPT);
    stats.ets_collision = e1000_reg_read(e, E1000_REG_COLC);
    stats.ets_transAb   = 0;
    stats.ets_carrSense = 0;
    stats.ets_fifoUnder = 0;
    stats.ets_fifoOver  = 0;
    stats.ets_CDheartbeat = 0;
    stats.ets_OWC = 0;

    sys_safecopyto(mp->m_source, mp->m_net_netdrv_dl_getstat_s.grant, 0,
	    (vir_bytes)&stats, sizeof(stats));
    mp->m_type  = DL_STAT_REPLY;
    if((r=ipc_send(mp->m_source, mp)) != OK)
	panic("e1000_getstat: ipc_send() failed: %d", r);
}

/*===========================================================================*
 *				e1000_interrupt				     *
 *===========================================================================*/
static void e1000_interrupt(mp)
message *mp;
{
    e1000_t *e;
    u32_t cause;

    E1000_DEBUG(3, ("e1000: interrupt\n"));

    /*
     * Check the card for interrupt reason(s).
     */
    e = &e1000_state;
	
    /* Re-enable interrupts. */
    if (sys_irqenable(&e->irq_hook) != OK)
    {
	panic("failed to re-enable IRQ");
    }

    /* Read the Interrupt Cause Read register. */
    if ((cause = e1000_reg_read(e, E1000_REG_ICR)))
    {
	if (cause & E1000_REG_ICR_LSC)
	    e1000_link_changed(e);

	if (cause & (E1000_REG_ICR_RXO | E1000_REG_ICR_RXT))
	    e1000_readv_s(&e->rx_message, TRUE);
	
	if ((cause & E1000_REG_ICR_TXQE) ||
	    (cause & E1000_REG_ICR_TXDW))
	    e1000_writev_s(&e->tx_message, TRUE);
    }
}

/*===========================================================================*
 *				e1000_link_changed			     *
 *===========================================================================*/
static int e1000_link_changed(e)
e1000_t *e;
{
    E1000_DEBUG(4, ("%s: link_changed()\n", e->name));
    return FALSE;
}

/*===========================================================================*
 *				e1000_stop				     *
 *===========================================================================*/
static void e1000_stop(e)
e1000_t *e;
{
    E1000_DEBUG(3, ("%s: stop()\n", e->name));

    e1000_reset_hw(e);

    exit(EXIT_SUCCESS);
}

/*===========================================================================*
 *				e1000_reg_read				     *
 *===========================================================================*/
static uint32_t e1000_reg_read(e, reg)
e1000_t *e;
uint32_t reg;
{
    uint32_t value;

    /* Assume a sane register. */
    assert(reg < 0x1ffff);

    /* Read from memory mapped register. */
    value = *(volatile u32_t *)(e->regs + reg);

    /* Return the result. */    
    return value;
}

/*===========================================================================*
 *				e1000_reg_write				     *
 *===========================================================================*/
static void e1000_reg_write(e, reg, value)
e1000_t *e;
uint32_t reg;
uint32_t value;
{
    /* Assume a sane register. */
    assert(reg < 0x1ffff);
    
    /* Write to memory mapped register. */
    *(volatile u32_t *)(e->regs + reg) = value;
}

/*===========================================================================*
 *				e1000_reg_set				     *
 *===========================================================================*/
static void e1000_reg_set(e, reg, value)
e1000_t *e;
uint32_t reg;
uint32_t value;
{
    uint32_t data;

    /* First read the current value. */
    data = e1000_reg_read(e, reg);
    
    /* Set value, and write back. */
    e1000_reg_write(e, reg, data | value);
}

/*===========================================================================*
 *				e1000_reg_unset				     *
 *===========================================================================*/
static void e1000_reg_unset(e, reg, value)
e1000_t *e;
uint32_t reg;
uint32_t value;
{
    uint32_t data;

    /* First read the current value. */
    data = e1000_reg_read(e, reg);
    
    /* Unset value, and write back. */
    e1000_reg_write(e, reg, data & ~value);
}


/*===========================================================================*
 *				eeprom_eerd				     *
 *===========================================================================*/
static u16_t eeprom_eerd(v, reg)
void *v;
int reg;
{
    e1000_t *e = (e1000_t *) v;
    u32_t data;

    /* Request EEPROM read. */
    e1000_reg_write(e, E1000_REG_EERD,
		   (reg << e->eeprom_addr_off) | (E1000_REG_EERD_START));

    /* Wait until ready. */
    while (!((data = (e1000_reg_read(e, E1000_REG_EERD))) & e->eeprom_done_bit));

    return data >> 16;
}

/*===========================================================================* 
 *                              eeprom_ich_init                              * 
 *===========================================================================*/
static int eeprom_ich_init(e)
e1000_t *e;
{
    union ich8_hws_flash_status hsfsts;
    int ret_val = -1;
    int i = 0;

    hsfsts.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFSTS);

    /* Check if the flash descriptor is valid */
    if (hsfsts.hsf_status.fldesvalid == 0)
    {
        E1000_DEBUG(3, ("Flash descriptor invalid.  "
                	"SW Sequencing must be used."));
        goto out;                                                         
    }                                                                         
    /* Clear FCERR and DAEL in hw status by writing 1 */                      
    hsfsts.hsf_status.flcerr = 1;                                             
    hsfsts.hsf_status.dael   = 1;                                     
                                                                        
    E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFSTS, hsfsts.regval);   
                                                                        
    /*                                                              
     * Either we should have a hardware SPI cycle in progress       
     * bit to check against, in order to start a new cycle or       
     * FDONE bit should be changed in the hardware so that it 
     * is 1 after hardware reset, which can then be used as an 
     * indication whether a cycle is in progress or has been 
     * completed. 
     */                                                                
    if (hsfsts.hsf_status.flcinprog == 0)
    {
        /* 
         * There is no cycle running at present, 
         * so we can start a cycle. 
         * Begin by setting Flash Cycle Done. 
         */
        hsfsts.hsf_status.flcdone = 1;
        E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFSTS, hsfsts.regval);
	ret_val = 0;
    }
    else
    {
        /* 
         * Otherwise poll for sometime so the current 
         * cycle has a chance to end before giving up. 
         */
        for (i = 0; i < ICH_FLASH_READ_COMMAND_TIMEOUT; i++)
	{
            hsfsts.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFSTS);
            
	    if (hsfsts.hsf_status.flcinprog == 0)
	    {
                ret_val = 0;
                break;
            }
            tickdelay(1);
        }
        if (ret_val == 0)
	{
            /* 
             * Successful in waiting for previous cycle to timeout, 
             * now set the Flash Cycle Done. 
             */
            hsfsts.hsf_status.flcdone = 1;
            E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFSTS,
                                    hsfsts.regval);
        }
	else
	{
            E1000_DEBUG(3, ("Flash controller busy, cannot get access"));
        }
    }
out:    
    return ret_val;
}

/*===========================================================================* 
 *                              eeprom_ich_cycle                             * 
 *===========================================================================*/
static int eeprom_ich_cycle(const e1000_t *e, u32_t timeout)
{
    union ich8_hws_flash_ctrl hsflctl;
    union ich8_hws_flash_status hsfsts;
    int ret_val = -1;
    u32_t i = 0;

    E1000_DEBUG(3, ("e1000_flash_cycle_ich8lan"));

    /* Start a cycle by writing 1 in Flash Cycle Go in Hw Flash Control */
    hsflctl.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFCTL);
    hsflctl.hsf_ctrl.flcgo = 1;
    E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFCTL, hsflctl.regval);
                
    /* wait till FDONE bit is set to 1 */
    do
    {
        hsfsts.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFSTS);
        if (hsfsts.hsf_status.flcdone == 1)
            break;
        tickdelay(1);
    }
    while (i++ < timeout);
                                                              
    if (hsfsts.hsf_status.flcdone == 1 && hsfsts.hsf_status.flcerr == 0)
        ret_val = 0;
                                
    return ret_val;
}

/*===========================================================================*
 *				eeprom_ich				     *
 *===========================================================================*/
static u16_t eeprom_ich(v, reg)
void *v;
int reg;
{
    union ich8_hws_flash_status hsfsts;
    union ich8_hws_flash_ctrl hsflctl;
    u32_t flash_linear_addr;
    u32_t flash_data = 0;
    int ret_val = -1;
    u8_t count = 0;
    e1000_t *e = (e1000_t *) v;
    u16_t data = 0;
                        
    E1000_DEBUG(3, ("e1000_read_flash_data_ich8lan"));
                                         
    if (reg > ICH_FLASH_LINEAR_ADDR_MASK)
        goto out;

    reg *= sizeof(u16_t);                
    flash_linear_addr = (ICH_FLASH_LINEAR_ADDR_MASK & reg) +
                         e->flash_base_addr;

    do {
	tickdelay(1);
        
	/* Steps */
        ret_val = eeprom_ich_init(e);
        if (ret_val != 0)
            break;

        hsflctl.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFCTL);
        /* 0b/1b corresponds to 1 or 2 byte size, respectively. */
        hsflctl.hsf_ctrl.fldbcount = 1;
        hsflctl.hsf_ctrl.flcycle = ICH_CYCLE_READ;
        E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFCTL, hsflctl.regval);
        E1000_WRITE_FLASH_REG(e, ICH_FLASH_FADDR, flash_linear_addr);

        ret_val = eeprom_ich_cycle(v, ICH_FLASH_READ_COMMAND_TIMEOUT);
        
        /* 
         * Check if FCERR is set to 1, if set to 1, clear it 
         * and try the whole sequence a few more times, else 
         * read in (shift in) the Flash Data0, the order is 
         * least significant byte first msb to lsb 
         */
        if (ret_val == 0)
	{
            flash_data = E1000_READ_FLASH_REG(e, ICH_FLASH_FDATA0);
            data = (u16_t)(flash_data & 0x0000FFFF);
            break;
        }
	else
	{
            /* 
             * If we've gotten here, then things are probably 
             * completely hosed, but if the error condition is 
    	     * detected, it won't hurt to give it another try... 
             * ICH_FLASH_CYCLE_REPEAT_COUNT times. 
             */
            hsfsts.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFSTS);
            
	    if (hsfsts.hsf_status.flcerr == 1)
	    {
                /* Repeat for some time before giving up. */
                continue;
            }
	    else if (hsfsts.hsf_status.flcdone == 0)
	    {
                E1000_DEBUG(3, ("Timeout error - flash cycle "
                    		"did not complete."));
                break;
            }
        }
    } while (count++ < ICH_FLASH_CYCLE_REPEAT_COUNT);

out:
    return data;
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
static void reply(e)
e1000_t *e;
{
    message msg;
    int r;

    /* Only reply to client for read/write request. */
    if (!(e->status & E1000_READING ||
          e->status & E1000_WRITING))
    {
	return;
    }
    /* Construct reply message. */
    msg.m_type   = DL_TASK_REPLY;
    msg.m_netdrv_net_dl_task.flags = DL_NOFLAGS;
    msg.m_netdrv_net_dl_task.count = 0;

    /* Did we successfully receive packet(s)? */
    if (e->status & E1000_READING &&
	e->status & E1000_RECEIVED)
    {
	msg.m_netdrv_net_dl_task.flags |= DL_PACK_RECV;
	msg.m_netdrv_net_dl_task.count =
		e->rx_size >= ETH_MIN_PACK_SIZE ?
			e->rx_size  : ETH_MIN_PACK_SIZE;

        /* Clear flags. */
	e->status &= ~(E1000_READING | E1000_RECEIVED);
    }
    /* Did we successfully transmit packet(s)? */
    if (e->status & E1000_TRANSMIT &&
        e->status & E1000_WRITING)
    {
	msg.m_netdrv_net_dl_task.flags |= DL_PACK_SEND;
	
	/* Clear flags. */
	e->status &= ~(E1000_WRITING | E1000_TRANSMIT);
    }

    /* Acknowledge to INET. */
    if ((r = ipc_send(e->client, &msg)) != OK)
    {
        panic("ipc_send() failed: %d", r);
    }
}

/*===========================================================================*
 *				mess_reply				     *
 *===========================================================================*/
static void mess_reply(req, reply_mess)
message *req;
message *reply_mess;
{
    if (ipc_send(req->m_source, reply_mess) != OK)
    {
        panic("unable to send reply message");
    }
}
