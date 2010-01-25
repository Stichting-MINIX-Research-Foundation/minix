/**
 * @file e1000.c
 *
 * @brief This file contains a device driver for Intel Pro/1000 
 *        Gigabit Ethernet Controllers.
 */

#include "../drivers.h"
#include <stdlib.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <ibm/pci.h>
#include <minix/ds.h>
#include <minix/vm.h>
#include <timers.h>
#include "assert.h"
#include "e1000.h"
#include "e1000_hw.h"
#include "e1000_reg.h"
#include "e1000_pci.h"

PRIVATE u16_t pcitab_e1000[] =
{
    E1000_DEV_ID_82540EM,
    E1000_DEV_ID_82541GI_LF,
    E1000_DEV_ID_ICH10_R_BM_LF,
    0,
};

PRIVATE char *progname;
PRIVATE e1000_t e1000_table[E1000_PORT_NR];

_PROTOTYPE( PRIVATE void e1000_init, (message *mp)			);
_PROTOTYPE( PRIVATE void e1000_init_pci, (void)				);
_PROTOTYPE( PRIVATE int  e1000_probe, (e1000_t *e)			);
_PROTOTYPE( PRIVATE int  e1000_init_hw, (e1000_t *e)			);
_PROTOTYPE( PRIVATE void e1000_init_addr, (e1000_t *e)		        );
_PROTOTYPE( PRIVATE void e1000_init_buf,  (e1000_t *e)			);
_PROTOTYPE( PRIVATE void e1000_reset_hw, (e1000_t *e)			);
_PROTOTYPE( PRIVATE void e1000_writev_s, (message *mp, int from_int)	);
_PROTOTYPE( PRIVATE void e1000_readv_s, (message *mp, int from_int)	);
_PROTOTYPE( PRIVATE void e1000_getstat_s, (message *mp)			);
_PROTOTYPE( PRIVATE void e1000_getname, (message *mp)			);
_PROTOTYPE( PRIVATE void e1000_interrupt, (message *mp)			);
_PROTOTYPE( PRIVATE void e1000_signal, (void)                           );
_PROTOTYPE( PRIVATE int  e1000_link_changed, (e1000_t *e)		);
_PROTOTYPE( PRIVATE void e1000_report_link, (e1000_t *e)		);
_PROTOTYPE( PRIVATE void e1000_stop, (void)                             );
_PROTOTYPE( PRIVATE e1000_t * e1000_port, (int port)                    );
_PROTOTYPE( PRIVATE uint32_t e1000_reg_read, (e1000_t *e, uint32_t reg) );
_PROTOTYPE( PRIVATE void e1000_reg_write, (e1000_t *e, uint32_t reg,
					  uint32_t value)               );					  
_PROTOTYPE( PRIVATE void e1000_reg_set,   (e1000_t *e, uint32_t reg,
					  uint32_t value)               );
_PROTOTYPE( PRIVATE void e1000_reg_unset, (e1000_t *e, uint32_t reg,
					   uint32_t value)              );
_PROTOTYPE( PRIVATE u16_t eeprom_eerd, (void *e, int reg)		);
_PROTOTYPE( PRIVATE u16_t eeprom_ich,  (void *e, int reg)		);
_PROTOTYPE( PRIVATE int eeprom_ich_init, (e1000_t *e)		        );
_PROTOTYPE( PRIVATE int eeprom_ich_cycle, (e1000_t *e, u32_t timeout)   );
_PROTOTYPE( PRIVATE void reply, (e1000_t *e, int err, int may_block)	);
_PROTOTYPE( PRIVATE void mess_reply, (message *req, message *reply)	);

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );
EXTERN int env_argc;
EXTERN char **env_argv;

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char *argv[])
{
    message m;
    int r;

    /* SEF local startup. */
    env_setargs(argc, argv);
    sef_local_startup();

    /*
     * Enter the main driver loop.
     */
    while (TRUE)
    {
	if ((r= sef_receive(ANY, &m)) != OK)
	{
	    panic("e1000", "sef_receive failed", r);
	}
	if (is_notify(m.m_type))
	{
	    switch (_ENDPOINT_P(m.m_source))
	    {
                case HARDWARE:
		    e1000_interrupt(&m);
		    break;
                
		case PM_PROC_NR:
		    e1000_signal();
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
	    case DL_STOP:       e1000_stop();                   break;
	    case DL_GETSTAT_S:  e1000_getstat_s(&m);		break;
	    case DL_GETNAME:    e1000_getname(&m);		break;
	    default:
		panic("e1000", "illegal message", m.m_type);
	}
    }
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* No live update support for now. */

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the e1000 driver. */
    int r;
    u32_t tasknr;

    /* Verify command-line arguments. */
    if (env_argc < 1)
    {
	panic("e1000", "no program name given in argc/argv", NO_NUM);
    }
    else
	(progname = strrchr(env_argv[0],'/')) ? progname++
		: (progname = env_argv[0]);

    /* Clear state. */
    memset(e1000_table, 0, sizeof(e1000_table));

    /* Perform calibration. */
    if((r = tsc_calibrate()) != OK)
    {
        panic("e1000", "tsc_calibrate failed", r);
    }
    /* Try to notify inet that we are present (again) */
    if ((r = ds_retrieve_label_num("inet", &tasknr)) == OK)
    {
        notify(tasknr);
    }
    else if (r != ESRCH)
    {
        printf("e1000: ds_retrieve_label_num failed for 'inet': %d\n", r);
    }

    return(OK);
}

/*===========================================================================*
 *				e1000_init				     *
 *===========================================================================*/
PRIVATE void e1000_init(message *mp)
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
    /* Retrieve e1000 pointer. */
    e = e1000_port(mp->DL_PORT);
    e->client = mp->DL_PROC;

    /* Initialize hardware, if needed. */
    if (!(e->status & E1000_ENABLED) && !(e1000_init_hw(e)))
    {
        reply_mess.m_type = DL_CONF_REPLY;
        reply_mess.m3_i1  = ENXIO;
        mess_reply(mp, &reply_mess);
        return;
    }
    /* Reply back to INET. */
    reply_mess.m_type = DL_CONF_REPLY;
    reply_mess.m3_i1  = mp->DL_PORT;
    reply_mess.m3_i2  = E1000_PORT_NR;
    *(ether_addr_t *) reply_mess.m3_ca1 = e->address;
    mess_reply(mp, &reply_mess);
}

/*===========================================================================*
 *				e1000_int_pci				     *
 *===========================================================================*/
PRIVATE void e1000_init_pci()
{
    e1000_t *e;
    int i;

    /* Initialize the PCI bus. */
    pci_init();

    /* Try to detect e1000's. */
    for (i = 0, e = &e1000_table[i]; i < E1000_PORT_NR; i++, e++)
    {
	strcpy(e->name, "e1000#0");
	e->name[6] += i;	
	e1000_probe(e);
    }
}

/*===========================================================================*
 *				e1000_probe				     *
 *===========================================================================*/
PRIVATE int e1000_probe(e1000_t *e)
{
    int i, r, devind;
    u16_t vid, did;
    u32_t status[2];
    u32_t gfpreg, sector_base_addr, sector_end_addr;
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
    for(;;)
    {
	for (i = 0; pcitab_e1000[i] != 0; i++)
	{
	    if (vid != 0x8086)
		continue;
	
	    if (did != pcitab_e1000[i])
		continue;
	    else
		break;
	}
	if (pcitab_e1000[i] != 0)
	    break;

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
        case E1000_DEV_ID_ICH10_R_BM_LF:
            e->eeprom_read = eeprom_ich;
            break;

	case E1000_DEV_ID_82541GI_LF:
	    e->eeprom_done_bit = (1 << 1);
	    e->eeprom_addr_off =  2;
	    break;

	default:
	    e->eeprom_done_bit = (1 << 4);
	    e->eeprom_addr_off =  8;
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
        panic("e1000", "failed to reserve PCI device", r);
    }
    /* Read PCI configuration. */
    e->irq   = pci_attr_r8(devind, PCI_ILR);
    e->regs  = vm_map_phys(SELF, (void *) pci_attr_r32(devind, PCI_BAR), 
			   0x20000);
			   
    /* Verify mapped registers. */
    if (e->regs == (u8_t *) -1)
    {
	panic("e1000", "failed to map hardware registers from PCI\n", 
			NO_NUM);
    }
    /* Optionally map flash memory. */
    if (pci_attr_r32(devind, PCI_BAR_3))
    {
	e->flash = vm_map_phys(SELF, (void *) pci_attr_r32(devind, PCI_BAR_2),
			       0x10000);

	gfpreg = E1000_READ_FLASH_REG(e, ICH_FLASH_GFPREG);
        /*
         * sector_X_addr is a "sector"-aligned address (4096 bytes)
         * Add 1 to sector_end_addr since this sector is included in
         * the overall size.
         */
        sector_base_addr = gfpreg & FLASH_GFPREG_BASE_MASK;
        sector_end_addr = ((gfpreg >> 16) & FLASH_GFPREG_BASE_MASK) + 1;

        /* flash_base_addr is byte-aligned */
        e->flash_base_addr = sector_base_addr << FLASH_SECTOR_ADDR_SHIFT;
    }
    /*
     * Output debug information.
     */
    status[0] = e1000_reg_read(e, E1000_REG_STATUS);    
    E1000_DEBUG(3, ("%s: MEM at 0x%lx, IRQ %d\n",
		    e->name, e->regs, e->irq));
    E1000_DEBUG(3, ("%s: link %s, %s duplex\n",
		    e->name, status[0] & 3 ? "up"   : "down",
			     status[0] & 1 ? "full" : "half"));
    return TRUE;
}

/*===========================================================================*
 *				e1000_init_hw				     *
 *===========================================================================*/
PRIVATE int e1000_init_hw(e)
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
        panic(e->name, "sys_irqsetpolicy failed", r);
    }
    if ((r = sys_irqenable(&e->irq_hook)) != OK)
    {
	panic(e->name, "sys_irqenable failed", r);
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
PRIVATE void e1000_init_addr(e)
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
    eakey[sizeof(E1000_ENVVAR)-1] = '0' + (e-e1000_table);

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
    if ((i != 0 && i != 6) || i == 0)
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
PRIVATE void e1000_init_buf(e)
e1000_t *e;
{
    phys_bytes rx_desc_p, rx_buff_p;
    phys_bytes tx_desc_p, tx_buff_p;
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
				      &rx_desc_p)) == NULL)
    	{
        	panic(e->name, "failed to allocate RX descriptors", 
				NO_NUM);
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
	    panic(e->name, "failed to allocate RX buffers", NO_NUM);
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
				      &tx_desc_p)) == NULL)
    	{
        	panic(e->name, "failed to allocate TX descriptors", 
				NO_NUM);
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
	    panic(e->name, "failed to allocate TX buffers", NO_NUM);
	}
	/* Setup transmit descriptors. */
	for (i = 0; i < E1000_RXDESC_NR; i++)
	{
	    e->tx_desc[i].buffer = tx_buff_p + (i * E1000_IOBUF_SIZE);
	}
    }
    /*
     * Setup the receive ring registers.
     */
    e1000_reg_write(e, E1000_REG_RDBAL, rx_desc_p);
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
    e1000_reg_write(e, E1000_REG_TDBAL, tx_desc_p);
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
PRIVATE void e1000_reset_hw(e)
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
PRIVATE void e1000_writev_s(mp, from_int)
message *mp;
int from_int;
{
    e1000_t *e = e1000_port(mp->DL_PORT);
    e1000_tx_desc_t *desc;
    iovec_s_t iovec[E1000_IOVEC_NR];
    int r, head, tail, i, bytes = 0, size;

    E1000_DEBUG(3, ("e1000: writev_s(%x,%d)\n", mp, from_int));

    /* Are we called from the interrupt handler? */
    if (!from_int)
    {
	/* We cannot write twice simultaneously. 
	assert(!(e->status & E1000_WRITING)); */
    
	/* Copy write message. */
	e->tx_message = *mp;
	e->status |= E1000_WRITING;

	/* Must be a sane vector count. */
	assert(e->tx_message.DL_COUNT > 0);
	assert(e->tx_message.DL_COUNT < E1000_IOVEC_NR);

	/*
	 * Copy the I/O vector table.
	 */
	if ((r = sys_safecopyfrom(e->client, e->tx_message.DL_GRANT, 0,
				  (vir_bytes) iovec, e->tx_message.DL_COUNT *
				  sizeof(iovec_s_t), D)) != OK)
	{
	    panic(e->name, "sys_safecopyfrom() failed", r);
	}
	/* Find the head, tail and current descriptors. */
	head =  e1000_reg_read(e, E1000_REG_TDH);
	tail =  e1000_reg_read(e, E1000_REG_TDT);
	desc = &e->tx_desc[tail];
	
	E1000_DEBUG(4, ("%s: head=%d, tail=%d\n",
	                 e->name, head, tail));

	/* Loop vector elements. */
	for (i = 0; i < e->tx_message.DL_COUNT; i++)
	{
	    size = iovec[i].iov_size < (E1000_IOBUF_SIZE - bytes) ?
		   iovec[i].iov_size : (E1000_IOBUF_SIZE - bytes);

	    E1000_DEBUG(4, ("iovec[%d] = %d\n", i, size));

	    /* Copy bytes to TX queue buffers. */
	    if ((r = sys_safecopyfrom(e->client, iovec[i].iov_grant, 0,
				     (vir_bytes) e->tx_buffer +
				     (tail * E1000_IOBUF_SIZE),
				      size, D)) != OK)
	    {
		panic(e->name, "sys_safecopyfrom() failed", r);
	    }
	    /* Mark this descriptor ready. */
	    desc->status  = 0;
	    desc->command = 0;
	    desc->length  = size;

	    /* Marks End-of-Packet. */
	    if (i == e->tx_message.DL_COUNT - 1)
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
    reply(e, OK, FALSE);
}

/*===========================================================================*
 *				e1000_readv_s				     *
 *===========================================================================*/
PRIVATE void e1000_readv_s(mp, from_int)
message *mp;
int from_int;
{
    e1000_t *e = e1000_port(mp->DL_PORT);
    e1000_rx_desc_t *desc;
    iovec_s_t iovec[E1000_IOVEC_NR];
    int i, r, head, tail, cur, bytes = 0, size;

    E1000_DEBUG(3, ("e1000: readv_s(%x,%d)\n", mp, from_int));

    /* Are we called from the interrupt handler? */
    if (!from_int)
    {
	e->rx_message = *mp;
	e->status    |= E1000_READING;
	e->rx_size    = 0;
	
	assert(e->rx_message.DL_COUNT > 0);
	assert(e->rx_message.DL_COUNT < E1000_IOVEC_NR);
    }
    if (e->status & E1000_READING)
    {
	/*
	 * Copy the I/O vector table first.
	 */
	if ((r = sys_safecopyfrom(e->client, e->rx_message.DL_GRANT, 0,
				  (vir_bytes) iovec, e->rx_message.DL_COUNT *
				  sizeof(iovec_s_t), D)) != OK)
	{
	    panic(e->name, "sys_safecopyfrom() failed", r);
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
    	    reply(e, OK, FALSE);
	    return;
	}
	E1000_DEBUG(4, ("%s: head=%x, tail=%d\n",
		          e->name, head, tail));

	/*
	 * Copy to vector elements.
	 */    
	for (i = 0; i < e->rx_message.DL_COUNT && bytes < desc->length; i++)
	{
	    size = iovec[i].iov_size < (desc->length - bytes) ?
		   iovec[i].iov_size : (desc->length - bytes);
	
	    E1000_DEBUG(4, ("iovec[%d] = %d[%d]\n",
			  i, iovec[i].iov_size, size));

	    if ((r = sys_safecopyto(e->client, iovec[i].iov_grant, 0,
				   (vir_bytes) e->rx_buffer + bytes + 
				   (cur * E1000_IOBUF_SIZE),
				    size, D)) != OK)
	    {
		panic(e->name, "sys_safecopyto() failed", r);
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
    reply(e, OK, FALSE);
}

/*===========================================================================*
 *				e1000_getstat_s				     *
 *===========================================================================*/
PRIVATE void e1000_getstat_s(mp)
message *mp;
{
    int r;
    eth_stat_t stats;
    e1000_t *e = e1000_port(mp->DL_PORT);

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

    sys_safecopyto(mp->DL_PROC, mp->DL_GRANT, 0, (vir_bytes)&stats,
                   sizeof(stats), D);
    mp->m_type  = DL_STAT_REPLY;
    mp->DL_PORT = mp->DL_PORT;
    mp->DL_STAT = OK;
    if((r=send(mp->m_source, mp)) != OK)
	panic("e1000", "e1000_getstat: send() failed", r);
}

/*===========================================================================*
 *				e1000_getname				     *
 *===========================================================================*/
PRIVATE void e1000_getname(mp)
message *mp;
{
    int r;

    E1000_DEBUG(3, ("e1000: getname()\n"));

    /* Copy our program name. */
    strncpy(mp->DL_NAME, progname, sizeof(mp->DL_NAME));
    mp->DL_NAME[ sizeof(mp->DL_NAME) - 1 ] = 0;

    /* Acknowledge the name request. */
    mp->m_type = DL_NAME_REPLY;
    if ((r = send(mp->m_source, mp)) != OK)
    {
	panic("e1000", "e1000_getname: send() failed", r);
    }
}

/*===========================================================================*
 *				e1000_interrupt				     *
 *===========================================================================*/
PRIVATE void e1000_interrupt(mp)
message *mp;
{
    e1000_t *e;
    u32_t cause;
    unsigned int i;

    E1000_DEBUG(3, ("e1000: interrupt\n"));

    /*
     * Loop all cards. Check for interrupt reason(s).
     */
    for (i = 0; i < E1000_PORT_NR; i++)
    {
	e = e1000_port(i);
	
        /* Re-enable interrupts. */
        if (sys_irqenable(&e->irq_hook) != OK)
        {
	    panic("e1000", "failed to re-enable IRQ", NO_NUM);
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
}

/*===========================================================================*
 *				e1000_signal				     *
 *===========================================================================*/
PRIVATE void e1000_signal(void)
{
    sigset_t sigset;

    E1000_DEBUG(3, ("e1000: signal()\n"));

    /* Try to obtain signal set from PM. */
    if (getsigset(&sigset) != 0)
    {
        return;
    }
    /* Check for known signals. */
    if (sigismember(&sigset, SIGTERM))
    {
	e1000_stop();
    }
}

/*===========================================================================*
 *				e1000_link_changed			     *
 *===========================================================================*/
PRIVATE int e1000_link_changed(e)
e1000_t *e;
{
    E1000_DEBUG(4, ("%s: link_changed()\n", e->name));
    return FALSE;
}

/*===========================================================================*
 *				e1000_report_link			     *
 *===========================================================================*/
PRIVATE void e1000_report_link(e)
e1000_t *e;
{
    E1000_DEBUG(4, ("%s: report_link()\n", e->name));
}

/*===========================================================================*
 *				e1000_stop				     *
 *===========================================================================*/
PRIVATE void e1000_stop()
{
    E1000_DEBUG(3, ("e1000: stop()\n"));
    exit(EXIT_SUCCESS);
}

/*===========================================================================*
 *				e1000_port				     *
 *===========================================================================*/
PRIVATE e1000_t * e1000_port(num)
int num;
{
    /*
     * Is the given port number within the allowed range?
     */
    if (num < 0 || num >= E1000_PORT_NR)
    {
	panic("e1000", "invalid port number given", num);
    }

    /*
     * The card must be active.
     */
    if (!(e1000_table[num].status & E1000_DETECTED))
    {
	panic("e1000", "inactive port number given", num);
    }
    return &e1000_table[num];
}


/*===========================================================================*
 *				e1000_reg_read				     *
 *===========================================================================*/
PRIVATE uint32_t e1000_reg_read(e, reg)
e1000_t *e;
uint32_t reg;
{
    uint32_t value;

    /* Assume a sane register. */
    assert(reg < 0x1ffff);

    /* Read from memory mapped register. */
    value = *(u32_t *)(e->regs + reg);

    /* Return the result. */    
    return value;
}

/*===========================================================================*
 *				e1000_reg_write				     *
 *===========================================================================*/
PRIVATE void e1000_reg_write(e, reg, value)
e1000_t *e;
uint32_t reg;
uint32_t value;
{
    /* Assume a sane register. */
    assert(reg < 0x1ffff);
    
    /* Write to memory mapped register. */
    *(u32_t *)(e->regs + reg) = value;
}

/*===========================================================================*
 *				e1000_reg_set				     *
 *===========================================================================*/
PRIVATE void e1000_reg_set(e, reg, value)
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
PRIVATE void e1000_reg_unset(e, reg, value)
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
PRIVATE u16_t eeprom_eerd(v, reg)
void *v;
int reg;
{
    e1000_t *e = (e1000_t *) v;
    u16_t data;

    /* Request EEPROM read. */
    e1000_reg_write(e, E1000_REG_EERD,
		   (reg << e->eeprom_addr_off) | (E1000_REG_EERD_START));

    /* Wait until ready. */
    while (!(e1000_reg_read(e, E1000_REG_EERD) &
			       e->eeprom_done_bit));

    /* Fetch data. */
    data = (e1000_reg_read(e, E1000_REG_EERD) &
			      E1000_REG_EERD_DATA) >> 16;
    return data;
}

/*===========================================================================* 
 *                              eeprom_ich_init                              * 
 *===========================================================================*/
PRIVATE int eeprom_ich_init(e)
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
PRIVATE int eeprom_ich_cycle(e1000_t *e, u32_t timeout)
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
PRIVATE u16_t eeprom_ich(v, reg)
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
    u16_t data;
                        
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
PRIVATE void reply(e, err, may_block)
e1000_t *e;
int err;
int may_block;
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
    msg.DL_PORT  = e - e1000_table;
    msg.DL_PROC  = e->client;
    msg.DL_COUNT = 0;
    msg.DL_STAT  = 0;
    msg.DL_CLCK  = 0;

    /* Did we successfully receive packet(s)? */
    if (e->status & E1000_READING &&
	e->status & E1000_RECEIVED)
    {
	msg.DL_STAT  = DL_PACK_RECV;
	msg.DL_COUNT = e->rx_size >= ETH_MIN_PACK_SIZE ?
		       e->rx_size  : ETH_MIN_PACK_SIZE;

        /* Clear flags. */
	e->status &= ~(E1000_READING | E1000_RECEIVED);
    }
    /* Did we successfully transmit packet(s)? */
    if (e->status & E1000_TRANSMIT &&
        e->status & E1000_WRITING)
    {
	msg.DL_STAT  = DL_PACK_SEND;
	msg.DL_COUNT = 0;
	
	/* Clear flags. */
	e->status &= ~(E1000_WRITING | E1000_TRANSMIT);
    }

    /* Acknowledge to INET. */
    if ((r = send(e->client, &msg)) != OK)
    {
        panic("e1000", "send() failed", r);
    }
}

/*===========================================================================*
 *				mess_reply				     *
 *===========================================================================*/
PRIVATE void mess_reply(req, reply_mess)
message *req;
message *reply_mess;
{
    if (send(req->m_source, reply_mess) != OK)
    {
        panic("e1000", "unable to send reply message", NO_NUM);
    }
}
