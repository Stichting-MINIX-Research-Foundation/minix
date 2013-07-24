#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <minix/sysutil.h>
#include "assert.h"
#include "lan8710a.h"
#include "lan8710a_reg.h"

/* Local functions */
#ifdef AM335X
static void lan8710a_readv_s(message *m, int from_int);
static void lan8710a_writev_s(message *m, int from_int);
static void lan8710a_init(message *m);
static void lan8710a_getstat(message *m);

static void lan8710a_interrupt(message *m);
static void lan8710a_map_regs(void);
static void lan8710a_stop(void);
static void lan8710a_dma_config_tx(u8_t desc_idx);
static void lan8710a_dma_reset_init(void);
static void lan8710a_init_addr(void);
static void lan8710a_init_desc(void);
static void lan8710a_init_mdio(void);
static int lan8710a_init_hw(void);
static void lan8710a_reset_hw();

static void lan8710a_phy_write(u32_t reg, u32_t value);
static u32_t lan8710a_phy_read(u32_t reg);

static u32_t lan8710a_reg_read(volatile u32_t *reg);
static void lan8710a_reg_write(volatile u32_t *reg, u32_t value);
static void lan8710a_reg_set(volatile u32_t *reg, u32_t value);
static void lan8710a_reg_unset(volatile u32_t *reg, u32_t value);

static void mess_reply(message *req, message *reply);
static void reply(lan8710a_t *e);

/* Local variables */
static lan8710a_t lan8710a_state;

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signal);
#endif /* AM335X */

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char *argv[])
{
#ifdef AM335X
	/* Local variables */
	message m;
	int r;
	int ipc_status;

	/* SEF local startup */
	env_setargs(argc, argv);
	sef_local_startup();

	/* Main driver loop */
	while (TRUE)
	{
		r = netdriver_receive(ANY, &m, &ipc_status);
		if (r != OK)
		{
			panic("netdriver_receive failed: %d", r);
		}

		if (is_ipc_notify(ipc_status))
		{
			switch (_ENDPOINT_P(m.m_source))
			{
			case HARDWARE:
				lan8710a_interrupt(&m);
				break;
			}
		}
		else
		{
			switch (m.m_type)
			{
			case DL_WRITEV_S:
				lan8710a_writev_s(&m, 0);
				break;
			case DL_READV_S:
				lan8710a_readv_s(&m, 0);
				break;
			case DL_CONF:
				lan8710a_init(&m);
				break;
			case DL_GETSTAT_S:
				lan8710a_getstat(&m);
				break;
			default:
				panic("Illegal message: %d", m.m_type);
			}
		}
	}
#endif /* AM335X */
	return EXIT_SUCCESS;
}
#ifdef AM335X
/*===========================================================================*
 * 				sef_local_startup			     *
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
 * 				sef_cb_init_fresh			     *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED( type), sef_init_info_t *UNUSED( info))
{
	/* Initialize the ethernet driver. */
	long v = 0;

	/* Clear state. */
	memset(&lan8710a_state, 0, sizeof(lan8710a_state));

	/* Get instance of ethernet device */
	env_parse("instance", "d", 0, &v, 0, 255);
	lan8710a_state.instance = (int) v;

	/* Announce we are up! */
	netdriver_announce();

	return OK;
}

/*===========================================================================*
 * 			sef_cb_signal_handler				     *
 *===========================================================================*/
static void sef_cb_signal_handler(int signal)
{
	/* Only check for termination signal, ignore anything else. */
	if (signal != SIGTERM)
		return;

	lan8710a_stop();
}

/*===========================================================================*
 * 				lan8710a_interrupt			     *
 *===========================================================================*/
static void lan8710a_interrupt(message *m)
{
	lan8710a_t *e = &lan8710a_state;
	int r;
	u32_t dma_status;

	/* Check the card for interrupt reason(s). */
	u32_t rx_stat = lan8710a_reg_read(CPSW_WR_C0_RX_STAT);
	u32_t tx_stat = lan8710a_reg_read(CPSW_WR_C0_TX_STAT);
	u32_t cp;

	/* Handle interrupts. */
	if(rx_stat)
	{
		cp = lan8710a_reg_read(CPDMA_STATERAM_RX_CP(0));

		lan8710a_readv_s(&(e->rx_message), TRUE);

		lan8710a_reg_write(CPDMA_STATERAM_RX_CP(0), cp);
		lan8710a_reg_write(CPDMA_EOI_VECTOR, END_OF_RX_INT);
	}
	if(tx_stat)
	{
		cp = lan8710a_reg_read(CPDMA_STATERAM_TX_CP(0));
		lan8710a_reg_set(CPDMA_TX_INTMASK_CLEAR, tx_stat); /* Disabling channels, where Tx interrupt ocurred */

		lan8710a_writev_s(&(e->tx_message), TRUE);

		lan8710a_reg_write(CPDMA_STATERAM_TX_CP(0), cp);
		lan8710a_reg_write(CPDMA_EOI_VECTOR, END_OF_TX_INT);
	}

	dma_status = lan8710a_reg_read(CPDMA_STATUS);

	if(dma_status & CPDMA_ERROR)
	{
		LAN8710A_DEBUG_PRINT(("CPDMA failed, error: 0x%X, CPDMA reset", dma_status));
		lan8710a_dma_reset_init();
	}
	/* Re-enable Rx interrupt. */
	if ((r = sys_irqenable(&lan8710a_state.irq_rx_hook)) != OK)
	{
		panic("sys_irqenable failed: %d", r);
	}
	/* Re-enable Tx interrupt. */
	if ((r = sys_irqenable(&lan8710a_state.irq_tx_hook)) != OK)
	{
		panic("sys_irqenable failed: %d", r);
	}
}

/*===========================================================================*
 * 				lan8710a_init				     *
 *===========================================================================*/
static void lan8710a_init(m)
message *m;
{
	message reply;
	static int first_time = 1;

	/* Configure driver first time */
	if (first_time)
	{
		lan8710a_map_regs();
		first_time = 0;
		strlcpy(lan8710a_state.name, "lan8710a#0", sizeof(lan8710a_state.name));
		lan8710a_state.name[9] += lan8710a_state.instance;
		lan8710a_state.status |= LAN8710A_DETECTED;
	}

	if (!(lan8710a_state.status & LAN8710A_ENABLED) && !(lan8710a_init_hw()))
	{
		reply.m_type = DL_CONF_REPLY;
		reply.DL_STAT = ENXIO;
		mess_reply(m, &reply);
		return;
	}
	/* Reply back to INET. */
	reply.m_type = DL_CONF_REPLY;
	reply.DL_STAT = OK;
	*(ether_addr_t *) reply.DL_HWADDR = lan8710a_state.address;
	mess_reply(m, &reply);
}

/*===========================================================================*
 * 				lan8710a_init_addr			     *
 *===========================================================================*/
static void lan8710a_init_addr(void)
{
	static char eakey[]= LAN8710A_ENVVAR "#_EA";
	static char eafmt[]= "x:x:x:x:x:x";
	int i;
	long v;

	/*
	 * Do we have a user defined ethernet address?
	 */
	eakey[sizeof(LAN8710A_ENVVAR)-1] = '0' + lan8710a_state.instance;

	for (i= 0; i < 6; i++)
	{
		if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
			break;
		else
			lan8710a_state.address.ea_addr[i]= v;
	}
	if(i != 6)
	{
		lan8710a_state.address.ea_addr[0] = (lan8710a_reg_read(CTRL_MAC_ID0_HI) & 0xFF);
		lan8710a_state.address.ea_addr[1] = ((lan8710a_reg_read(CTRL_MAC_ID0_HI) & 0xFF00) >> 8);
		lan8710a_state.address.ea_addr[2] = ((lan8710a_reg_read(CTRL_MAC_ID0_HI) & 0xFF0000) >> 16);
		lan8710a_state.address.ea_addr[3] = ((lan8710a_reg_read(CTRL_MAC_ID0_HI) & 0xFF000000) >> 24);
		lan8710a_state.address.ea_addr[4] = (lan8710a_reg_read(CTRL_MAC_ID0_LO) & 0xFF);
		lan8710a_state.address.ea_addr[5] = ((lan8710a_reg_read(CTRL_MAC_ID0_LO) & 0xFF00) >> 8);
	}
}

/*===========================================================================*
 * 			lan8710a_map_regs				     *
 *===========================================================================*/
static void lan8710a_map_regs(void)
{
	struct minix_mem_range mr;
	mr.mr_base = CM_PER_BASE_ADR;
	mr.mr_limit = CM_PER_BASE_ADR + MEMORY_LIMIT;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0)
	{
		panic("Unable to request permission to map memory");
	}
	lan8710a_state.regs_cp_per = (vir_bytes)vm_map_phys(SELF, (void *)CM_PER_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cp_per == MAP_FAILED)
	{
		panic("lan8710a_state.regs_cp_per: vm_map_phys failed");
	}
	lan8710a_state.regs_cpdma_stateram = (vir_bytes)vm_map_phys(SELF, (void *)CPDMA_STATERAM_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cpdma_stateram == MAP_FAILED)
	{
		panic("lan8710a_state.regs_cpdma_stateram: vm_map_phys failed");
	}
	lan8710a_state.regs_cpsw_cpdma = (vir_bytes)vm_map_phys(SELF, (void *)CPSW_CPDMA_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cpsw_cpdma == MAP_FAILED)
	{
		panic("lan8710a_state.regs_cpsw_cpdma: vm_map_phys failed");
	}
	lan8710a_state.regs_cpsw_ale = (vir_bytes)vm_map_phys(SELF, (void *)CPSW_ALE_BASE_ADR, 256);
	if ((void *)lan8710a_state.regs_cpsw_ale == MAP_FAILED)
	{
		panic("lan8710a_state.regs_cpsw_ale: vm_map_phys failed");
	}
	lan8710a_state.regs_cpsw_sl = (vir_bytes)vm_map_phys(SELF, (void *)CPSW_SL_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cpsw_sl == MAP_FAILED)
	{
		panic("lan8710a_state.regs_cpsw_sl: vm_map_phys failed");
	}
	lan8710a_state.regs_cpsw_ss = (vir_bytes)vm_map_phys(SELF, (void *)CPSW_SS_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cpsw_ss == MAP_FAILED)
	{
		panic("lan8710a_state.regs_cpsw_ss: vm_map_phys failed");
	}
	lan8710a_state.regs_cpsw_wr = (vir_bytes)vm_map_phys(SELF, (void *)CPSW_WR_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cpsw_wr == MAP_FAILED)
	{
		panic("lan8710a_state.regs_cpsw_wr: vm_map_phys failed");
	}
	lan8710a_state.regs_ctrl_mod = (vir_bytes)vm_map_phys(SELF, (void *)CTRL_MOD_BASE_ADR, 2560);
	if ((void *)lan8710a_state.regs_ctrl_mod == MAP_FAILED)
	{
		panic("lan8710a_state.regs_ctrl_mod: vm_map_phys failed");
	}
	lan8710a_state.regs_intc = (vir_bytes)vm_map_phys(SELF, (void *)INTC_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_intc == MAP_FAILED)
	{
		panic("lan8710a_state.regs_intc: vm_map_phys failed");
	}
	lan8710a_state.regs_mdio = (vir_bytes)vm_map_phys(SELF, (void *)MDIO_BASE_ADDR, 512);
	if ((void *)lan8710a_state.regs_mdio == MAP_FAILED)
	{
		panic("lan8710a_state.regs_mdio: vm_map_phys failed");
	}

	mr.mr_base = BEGINNING_DESC_MEM;
	mr.mr_limit = BEGINNING_DESC_MEM + DESC_MEMORY_LIMIT;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0)
	{
		panic("Unable to request permission to map memory");
	}
	lan8710a_state.rx_desc_phy = BEGINNING_RX_DESC_MEM;
	lan8710a_state.tx_desc_phy = BEGINNING_TX_DESC_MEM;
	lan8710a_state.rx_desc = (lan8710a_desc_t *)vm_map_phys(SELF, (void *)lan8710a_state.rx_desc_phy, 1024);
	if ((void *)lan8710a_state.rx_desc == MAP_FAILED)
	{
		panic("lan8710a_state.rx_desc: vm_map_phys failed");
	}
	lan8710a_state.tx_desc = (lan8710a_desc_t *)vm_map_phys(SELF, (void *)lan8710a_state.tx_desc_phy, 1024);
	if ((void *)lan8710a_state.tx_desc == MAP_FAILED)
	{
		panic("lan8710a_state.tx_desc: vm_map_phys failed");
	}

	mr.mr_base = CPSW_STATS_BASE_ADR;
	mr.mr_limit = CPSW_STATS_BASE_ADR + CPSW_STATS_MEM_LIMIT;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0)
	{
		panic("Unable to request permission to map memory");
	}
	lan8710a_state.regs_cpsw_stats = (vir_bytes)vm_map_phys(SELF, (void *)CPSW_STATS_BASE_ADR, 256);
	if ((void *)lan8710a_state.regs_cpsw_stats == MAP_FAILED)
	{
		panic("lan8710a_state.regs_cpsw_stats: vm_map_phys failed");
	}
}

/*===========================================================================*
 * 			lan8710a_getstat				     *
 *===========================================================================*/
static void lan8710a_getstat(mp)
message *mp;
{
	int r;
	eth_stat_t stats;

	stats.ets_recvErr   = lan8710a_reg_read(CPSW_STAT_RX_CRC_ERR)
				+ lan8710a_reg_read(CPSW_STAT_RX_AGNCD_ERR)
				+ lan8710a_reg_read(CPSW_STAT_RX_OVERSIZE);
	stats.ets_sendErr   = 0;
	stats.ets_OVW       = 0;
	stats.ets_CRCerr    = lan8710a_reg_read(CPSW_STAT_RX_CRC_ERR);
	stats.ets_frameAll  = lan8710a_reg_read(CPSW_STAT_RX_AGNCD_ERR);
	stats.ets_missedP   = 0;
	stats.ets_packetR   = lan8710a_reg_read(CPSW_STAT_RX_GOOD);
	stats.ets_packetT   = lan8710a_reg_read(CPSW_STAT_TX_GOOD);
	stats.ets_collision = lan8710a_reg_read(CPSW_STAT_COLLISIONS);
	stats.ets_transAb   = 0;
	stats.ets_carrSense = lan8710a_reg_read(CPSW_STAT_CARR_SENS_ERR);
	stats.ets_fifoUnder = lan8710a_reg_read(CPSW_STAT_TX_UNDERRUN);
	stats.ets_fifoOver  = lan8710a_reg_read(CPSW_STAT_RX_OVERRUN);
	stats.ets_CDheartbeat = 0;
	stats.ets_OWC = 0;

	sys_safecopyto(mp->m_source, mp->DL_GRANT, 0, (vir_bytes)&stats,
				   sizeof(stats));
	mp->m_type  = DL_STAT_REPLY;

	if((r=send(mp->m_source, mp)) != OK)
	{
		panic("lan8710a_getstat: send() failed: %d", r);
	}
}

/*===========================================================================*
 * 			lan8710a_stop 					     *
 *===========================================================================*/
static void lan8710a_stop(void)
{
	/* Reset hardware. */
	lan8710a_reset_hw();

	/* Exit driver. */
	exit(EXIT_SUCCESS);
}

/*===========================================================================*
 * 			lan8710a_dma_config_tx				     *
 *===========================================================================*/
static void lan8710a_dma_config_tx(desc_idx)
u8_t desc_idx;
{
	phys_bytes phys_addr;
	int i;
	for(i = 0; i < TX_DMA_CHANNELS; ++i)
	{
		if(!lan8710a_reg_read(CPDMA_STATERAM_TX_HDP(i))) break;
	}
	if(i == TX_DMA_CHANNELS)
	{
		panic("There are not free TX DMA channels.");
	}

	lan8710a_reg_write(CPDMA_TX_INTMASK_SET, 1 << i); /* Enabling only one channels Tx interrupt */
	lan8710a_reg_write(CPSW_WR_C0_TX_EN, 1 << i); /* Routing only one channels Tx int to TX_PULSE signal */

	/* Setting HDP */
	phys_addr = lan8710a_state.tx_desc_phy + (desc_idx * sizeof(lan8710a_desc_t));
	lan8710a_reg_write(CPDMA_STATERAM_TX_HDP(i), (u32_t)phys_addr);
}

/*===========================================================================*
 * 			lan8710a_dma_reset_init				     *
 *===========================================================================*/
static void lan8710a_dma_reset_init(void)
{
	int i;
	lan8710a_reg_write(CPDMA_SOFT_RESET, SOFT_RESET);
	while((lan8710a_reg_read(CPDMA_SOFT_RESET) & SOFT_RESET));

	/* Initialize the HDPs (Header Description Pointer) and CPs (Completion Pointer) to NULL */
	for(i = 0; i < DMA_MAX_CHANNELS; ++i)
	{
		lan8710a_reg_write(CPDMA_STATERAM_TX_HDP(i), 0);
		lan8710a_reg_write(CPDMA_STATERAM_RX_HDP(i), 0);
		lan8710a_reg_write(CPDMA_STATERAM_TX_CP(i) , 0);
		lan8710a_reg_write(CPDMA_STATERAM_RX_CP(i) , 0);
	}

	lan8710a_reg_write(CPDMA_RX_INTMASK_CLEAR, 0xFFFFFFFF);
	lan8710a_reg_write(CPDMA_TX_INTMASK_CLEAR, 0xFFFFFFFF);

	/* Configure the CPDMA controller. */
	lan8710a_reg_set(CPDMA_RX_CONTROL, CPDMA_RX_EN); /* RX Enabled */
	lan8710a_reg_set(CPDMA_TX_CONTROL, CPDMA_TX_EN); /* TX Enabled */

	lan8710a_reg_set(CPDMA_RX_INTMASK_SET, CPDMA_FIRST_CHAN_INT); /* Enabling first channel Rx interrupt */

	/* Writing the address of the first buffer descriptor in the queue (nonzero value)
	   to the channel’s head descriptor pointer in the channel’s Rx DMA state. */
	lan8710a_reg_write(CPDMA_STATERAM_RX_HDP(0), (u32_t)lan8710a_state.rx_desc_phy);

	lan8710a_state.rx_desc_idx = 0;
	lan8710a_state.tx_desc_idx = 0;
}

/*===========================================================================*
 * 			lan8710a_init_desc				     *
 *===========================================================================*/
void lan8710a_init_desc(void)
{
	lan8710a_desc_t *p_rx_desc;
	lan8710a_desc_t *p_tx_desc;
	phys_bytes   buf_phys_addr;
	u8_t *p_buf;
	u8_t i;

	/* Attempt to allocate. */
	if ((lan8710a_state.p_rx_buf = alloc_contig((LAN8710A_NUM_RX_DESC
			* LAN8710A_IOBUF_SIZE), AC_ALIGN4K, &buf_phys_addr)) == NULL)
	{
		panic("failed to allocate RX buffers.");
	}
	p_buf = lan8710a_state.p_rx_buf;
	for (i = 0; i < LAN8710A_NUM_RX_DESC; i++)
	{
		p_rx_desc = &(lan8710a_state.rx_desc[i]);
		memset(p_rx_desc, 0x0, sizeof(lan8710a_desc_t));
		p_rx_desc->pkt_len_flags = LAN8710A_DESC_FLAG_OWN;
		p_rx_desc->buffer_length_off = LAN8710A_IOBUF_SIZE;
		p_rx_desc->buffer_pointer = (u32_t)(buf_phys_addr + (i * LAN8710A_IOBUF_SIZE));

		p_rx_desc->next_pointer = (u32_t)((i == (LAN8710A_NUM_RX_DESC - 1)) ? (lan8710a_state.rx_desc_phy)
							: (lan8710a_state.rx_desc_phy + (i + 1) * sizeof(lan8710a_desc_t)));
	}

	/* Attempt to allocate. */
	if ((lan8710a_state.p_tx_buf = alloc_contig((LAN8710A_NUM_TX_DESC
			* LAN8710A_IOBUF_SIZE), AC_ALIGN4K, &buf_phys_addr)) == NULL)
	{
		panic("failed to allocate TX buffers");
	}
	p_buf = lan8710a_state.p_tx_buf;
	for (i = 0; i < LAN8710A_NUM_TX_DESC; i++)
	{
		p_tx_desc = &(lan8710a_state.tx_desc[i]);
		memset(p_tx_desc, 0x0, sizeof(lan8710a_desc_t));
		p_tx_desc->buffer_pointer = (u32_t)(buf_phys_addr + (i * LAN8710A_IOBUF_SIZE));
	}
	lan8710a_state.rx_desc_idx = 0;
	lan8710a_state.tx_desc_idx = 0;
}

/*===========================================================================*
 * 			lan8710a_init_hw				     *
 *===========================================================================*/
static int lan8710a_init_hw(void)
{
	int r, i;

	lan8710a_state.status |= LAN8710A_ENABLED;

	/* Reset hardware. */
	lan8710a_reset_hw();

	/* Select the Interface (GMII/RGMII/MII) Mode in the Control Module. */
	lan8710a_reg_write(GMII_SEL, (GMII2_SEL_BIT1 | GMII2_SEL_BIT0)); /* Port1 GMII/MII Mode, Port2 not used */

	/* Configure pads (PIN muxing) as per the Interface Selected using the
	 * appropriate pin muxing conf_xxx registers in the Control Module.
	 */
	lan8710a_reg_unset(CONF_MII1_COL, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MII1_COL, CONF_MOD_RX_ACTIVE); /* Input */
	lan8710a_reg_set(CONF_MII1_COL, CONF_MOD_PU_TYPESEL); /* Pull-up selected */
	lan8710a_reg_unset(CONF_MII1_COL, CONF_MOD_PUDEN); /* Pull-up enabled */
	lan8710a_reg_unset(CONF_MII1_COL, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_CRS, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MII1_CRS, CONF_MOD_RX_ACTIVE); /* Input */
	lan8710a_reg_set(CONF_MII1_CRS, CONF_MOD_PU_TYPESEL); /* Pull-up selected */
	lan8710a_reg_unset(CONF_MII1_CRS, CONF_MOD_PUDEN); /* Pull-up enabled */
	lan8710a_reg_unset(CONF_MII1_CRS, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_RX_ER, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MII1_RX_ER, CONF_MOD_RX_ACTIVE); /* Input */
	lan8710a_reg_set(CONF_MII1_RX_ER, CONF_MOD_PU_TYPESEL); /* Pull-up selected */
	lan8710a_reg_unset(CONF_MII1_RX_ER, CONF_MOD_PUDEN); /* Pull-up enabled */
	lan8710a_reg_unset(CONF_MII1_RX_ER, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_TX_EN, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_unset(CONF_MII1_TX_EN, CONF_MOD_RX_ACTIVE); /* Output */
	lan8710a_reg_set(CONF_MII1_TX_EN, CONF_MOD_PUDEN); /* Pull-up/Pull-down disabled */
	lan8710a_reg_unset(CONF_MII1_TX_EN, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_RX_DV, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MII1_RX_DV, CONF_MOD_RX_ACTIVE); /* Input */
	lan8710a_reg_set(CONF_MII1_RX_DV, CONF_MOD_PU_TYPESEL); /* Pull-up selected */
	lan8710a_reg_unset(CONF_MII1_RX_DV, CONF_MOD_PUDEN);  /* Pull-up enabled */
	lan8710a_reg_unset(CONF_MII1_RX_DV, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_TXD3, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_unset(CONF_MII1_TXD3, CONF_MOD_RX_ACTIVE); /* Output */
	lan8710a_reg_set(CONF_MII1_TXD3, CONF_MOD_PUDEN); /* Pull-up/Pull-down disabled */
	lan8710a_reg_unset(CONF_MII1_TXD3, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_TXD2, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_unset(CONF_MII1_TXD2, CONF_MOD_RX_ACTIVE); /* Output */
	lan8710a_reg_set(CONF_MII1_TXD2, CONF_MOD_PUDEN); /* Pull-up/Pull-down disabled */
	lan8710a_reg_unset(CONF_MII1_TXD2, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_TXD1, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_unset(CONF_MII1_TXD1, CONF_MOD_RX_ACTIVE); /* Output */
	lan8710a_reg_set(CONF_MII1_TXD1, CONF_MOD_PUDEN); /* Pull-up/Pull-down disabled */
	lan8710a_reg_unset(CONF_MII1_TXD1, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_TXD0, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_unset(CONF_MII1_TXD0, CONF_MOD_RX_ACTIVE); /* Output */
	lan8710a_reg_set(CONF_MII1_TXD0, CONF_MOD_PUDEN); /* Pull-up/Pull-down disabled */
	lan8710a_reg_unset(CONF_MII1_TXD0, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_TX_CLK, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MII1_TX_CLK, CONF_MOD_RX_ACTIVE); /* Input */
	lan8710a_reg_set(CONF_MII1_TX_CLK, CONF_MOD_PUDEN); /* Pull-up disabled */
	lan8710a_reg_unset(CONF_MII1_TX_CLK, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_RX_CLK, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MII1_RX_CLK, CONF_MOD_RX_ACTIVE); /* Input */
	lan8710a_reg_set(CONF_MII1_RX_CLK, CONF_MOD_PUDEN); /* Pull-up disabled */
	lan8710a_reg_unset(CONF_MII1_RX_CLK, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_RXD3, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MII1_RXD3, CONF_MOD_RX_ACTIVE); /* Input */
	lan8710a_reg_set(CONF_MII1_RXD3, CONF_MOD_PU_TYPESEL); /* Pull-up selected */
	lan8710a_reg_unset(CONF_MII1_RXD3, CONF_MOD_PUDEN);   /* Pull-up enabled */
	lan8710a_reg_unset(CONF_MII1_RXD3, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_RXD2, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MII1_RXD2, CONF_MOD_RX_ACTIVE); /* Input */
	lan8710a_reg_set(CONF_MII1_RXD2, CONF_MOD_PU_TYPESEL); /* Pull-up selected */
	lan8710a_reg_unset(CONF_MII1_RXD2, CONF_MOD_PUDEN);  /* Pull-up enabled */
	lan8710a_reg_unset(CONF_MII1_RXD2, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_RXD1, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MII1_RXD1, CONF_MOD_RX_ACTIVE); /* Input */
	lan8710a_reg_set(CONF_MII1_RXD1, CONF_MOD_PU_TYPESEL); /* Pull-up selected */
	lan8710a_reg_unset(CONF_MII1_RXD1, CONF_MOD_PUDEN);  /* Pull-up enabled */
	lan8710a_reg_unset(CONF_MII1_RXD1, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MII1_RXD0, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MII1_RXD0, CONF_MOD_RX_ACTIVE); /* Input */
	lan8710a_reg_set(CONF_MII1_RXD0, CONF_MOD_PU_TYPESEL); /* Pull-up selected */
	lan8710a_reg_unset(CONF_MII1_RXD0, CONF_MOD_PUDEN);  /* Pull-up enabled */
	lan8710a_reg_unset(CONF_MII1_RXD0, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MDIO, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_set(CONF_MDIO, CONF_MOD_RX_ACTIVE); /* Input/Output */
	lan8710a_reg_set(CONF_MDIO, CONF_MOD_PU_TYPESEL); /* Pull-up selected */
	lan8710a_reg_unset(CONF_MDIO, CONF_MOD_PUDEN);  /* Pull-up enabled */
	lan8710a_reg_unset(CONF_MDIO, CONF_MOD_MMODE_MII); /* MII Mode */

	lan8710a_reg_unset(CONF_MDC, CONF_MOD_SLEW_CTRL); /* Fast Mode */
	lan8710a_reg_unset(CONF_MDC, CONF_MOD_RX_ACTIVE); /* Output */
	lan8710a_reg_set(CONF_MDC, CONF_MOD_PUDEN); /* Pull-up/Pull-down disabled */
	lan8710a_reg_unset(CONF_MDC, CONF_MOD_MMODE_MII); /* MII Mode */

	/* Apply soft reset to 3PSW Subsytem, CPSW_3G, CPGMAC_SL1/2, and CPDMA. */
	lan8710a_reg_write(CPSW_SS_SOFT_RESET, SOFT_RESET);
	lan8710a_reg_write(CPSW_SL_SOFT_RESET(1), SOFT_RESET);
	lan8710a_reg_write(CPSW_SL_SOFT_RESET(2), SOFT_RESET);

	/* Wait for software resets completion */
	while((lan8710a_reg_read(CPSW_SS_SOFT_RESET) & SOFT_RESET)
			|| (lan8710a_reg_read(CPSW_SL_SOFT_RESET(1)) & SOFT_RESET)
			|| (lan8710a_reg_read(CPSW_SL_SOFT_RESET(2)) & SOFT_RESET));

	/* Configure the Statistics Port Enable register. */
	/* Enable port 0 and 1 statistics. */
	lan8710a_reg_write(CPSW_SS_STAT_PORT_EN, (CPSW_P1_STAT_EN | CPSW_P0_STAT_EN));

	/* Configure the ALE. */
	lan8710a_reg_write(CPSW_ALE_CONTROL, (CPSW_ALE_ENABLE | CPSW_ALE_BYPASS)); /* Enable Ale */
										   /* All packets received on ports 1 are
										    * sent to the host (only to the host). */
	lan8710a_reg_write(CPSW_ALE_PORTCTL0, CPSW_ALE_PORT_FWD); /* Port 0 (host) in forwarding mode. */
	lan8710a_reg_write(CPSW_ALE_PORTCTL1, CPSW_ALE_PORT_FWD); /* Port 1 in forwarding mode. */

	/* Configure CPSW_SL Register */
	lan8710a_reg_write(CPSW_SL_MACCONTROL(1), CPSW_SL_FULLDUPLEX); /* Full duplex mode. */

	/*
	 * Set the interrupt handler and policy. Do not automatically
	 * re-enable interrupts. Return the IRQ line number on interrupts.
	 */
	lan8710a_state.irq_rx_hook = 1;
	if ((r = sys_irqsetpolicy(LAN8710A_RX_INTR, 0, &lan8710a_state.irq_rx_hook)) != OK)
	{
		panic("sys_irqsetpolicy failed: %d", r);
	}
	if ((r = sys_irqenable(&lan8710a_state.irq_rx_hook)) != OK)
	{
		panic("sys_irqenable failed: %d", r);
	}
	lan8710a_state.irq_tx_hook = 2;
	if ((r = sys_irqsetpolicy(LAN8710A_TX_INTR, 0, &lan8710a_state.irq_tx_hook)) != OK)
	{
		panic("sys_irqsetpolicy failed: %d", r);
	}
	if ((r = sys_irqenable(&lan8710a_state.irq_tx_hook)) != OK)
	{
		panic("sys_irqenable failed: %d", r);
	}

	/* Initialize MDIO Protocol */
	lan8710a_init_mdio();

	/* Getting MAC Address */
	lan8710a_init_addr();

	/* Initialize descriptors */
	lan8710a_init_desc();

	/* Reset and initialize CPDMA */
	lan8710a_dma_reset_init();

	/* Configure the Interrupts.  */
	lan8710a_reg_set(CPSW_WR_C0_RX_EN, CPSW_FIRST_CHAN_INT); /* Routing all channel Rx int to RX_PULSE signal */\

	/*
	 * Enabling LAN8710A Auto-negotiation
	 */
	lan8710a_phy_write(LAN8710A_CTRL_REG, LAN8710A_AUTO_NEG);

	/* Waiting for auto-negotiaion completion. */
	for(i = 0; !(lan8710a_phy_read(LAN8710A_STATUS_REG) & LAN8710A_AUTO_NEG_COMPL); ++i)
	{
		if(i == 100)
		{
			LAN8710A_DEBUG_PRINT(("Autonegotiation failed"));
			break;
		}
		tickdelay(100);
	}

	lan8710a_reg_set(CPSW_SL_MACCONTROL(1), CPSW_SL_GMII_EN); /* GMII RX and TX release from reset. */

	return TRUE;
}

/*===========================================================================*
 * 			lan8710a_init_mdio				     *
 *===========================================================================*/
static void lan8710a_init_mdio(void)
{
	u16_t address = 0;
	u32_t r;

	/* Clearing MDIOCONTROL register */
	lan8710a_reg_write(MDIOCONTROL, 0);
	/* Configure the PREAMBLE and CLKDIV bits in the MDIO control register */
	lan8710a_reg_unset(MDIOCONTROL, MDIO_PREAMBLE);
	/* Enable sending MDIO frame preambles */
	lan8710a_reg_set(MDIOCONTROL, (MDCLK_DIVIDER | MDIO_ENABLE));
	/* Enable the MDIO module by setting the ENABLE bit in MDIOCONTROL */

	while(!(r = lan8710a_reg_read(MDIOALIVE)));

	/* Get PHY address */
	while (r >>= 1) 
	{
		++address;
	}
	lan8710a_state.phy_address = address;

	/* Setup appropiate address in MDIOUSERPHYSEL0 */
	lan8710a_reg_set(MDIOUSERPHYSEL0, address);
}

/*===========================================================================*
 *			lan8710a_writev_s				     *
 *===========================================================================*/
static void lan8710a_writev_s(mp, from_int)
message *mp;
int from_int;
{
	iovec_s_t iovec[LAN8710A_IOVEC_NR];
	lan8710a_t *e = &lan8710a_state;
	lan8710a_desc_t *p_tx_desc;
	u8_t *p_buf;
	int r, size, buf_data_len, i;

	/* Are we called from the interrupt handler? */
	if (!from_int)
	{
		/* We cannot write twice simultaneously. */
		assert(!(e->status & LAN8710A_WRITING));

		/* Copy write message. */
		e->tx_message = *mp;
		e->client = mp->m_source;
		e->status |= LAN8710A_WRITING;

		/* verify vector count */
		assert(mp->DL_COUNT > 0);
		assert(mp->DL_COUNT < LAN8710A_IOVEC_NR);

		/*
		 * Copy the I/O vector table.
		 */
		if ((r = sys_safecopyfrom(mp->m_source, mp->DL_GRANT, 0,
				(vir_bytes) iovec, mp->DL_COUNT * sizeof(iovec_s_t))) != OK)
		{
			panic("sys_safecopyfrom() failed: %d", r);
		}
		/* setup descriptors */
		p_tx_desc = &(e->tx_desc[e->tx_desc_idx]);

		/* Check if descriptor is available for host and drop the packet if not*/
		if(LAN8710A_DESC_FLAG_OWN & p_tx_desc->pkt_len_flags)
		{
			panic("No available transmit descriptor.");
		}

		/* virtual address of buffer */
		p_buf = e->p_tx_buf + e->tx_desc_idx * LAN8710A_IOBUF_SIZE;
		buf_data_len = 0;
		for (i = 0; i < mp->DL_COUNT; i++)
		{
			if ((buf_data_len + iovec[i].iov_size) > LAN8710A_IOBUF_SIZE)
			{
			  panic("packet too long");
			}

			/* copy data to buffer */
			size = iovec[i].iov_size < (LAN8710A_IOBUF_SIZE - buf_data_len) ? iovec[i].iov_size
							: (LAN8710A_IOBUF_SIZE - buf_data_len);

			/* Copy bytes to TX queue buffers. */
			if ((r = sys_safecopyfrom(mp->m_source, iovec[i].iov_grant, 0,
					(vir_bytes) p_buf, size)) != OK)
			{
				panic("sys_safecopyfrom() failed: %d", r);
			}
			p_buf += size;
			buf_data_len += size;
		}

		/* set descriptor length */
		p_tx_desc->buffer_length_off = buf_data_len;
		/* set flags */
		p_tx_desc->pkt_len_flags = (LAN8710A_DESC_FLAG_OWN | LAN8710A_DESC_FLAG_SOP | LAN8710A_DESC_FLAG_EOP
						   | TX_DESC_TO_PORT1 | TX_DESC_TO_PORT_EN);
		p_tx_desc->pkt_len_flags |= buf_data_len;

		/* setup DMA transfer */
		lan8710a_dma_config_tx(e->tx_desc_idx);

		e->tx_desc_idx++;
		if (LAN8710A_NUM_TX_DESC == e->tx_desc_idx)
		{
			e->tx_desc_idx = 0;
		}
	}
	else
	{
		e->status |= LAN8710A_TRANSMIT;
	}
	reply(e);
}

/*===========================================================================*
 *			lan8710a_readv_s	 			     *
 *===========================================================================*/
static void lan8710a_readv_s(mp, from_int)
message *mp;
int from_int;
{
	iovec_s_t iovec[LAN8710A_IOVEC_NR];
	lan8710a_t *e = &lan8710a_state;
	lan8710a_desc_t *p_rx_desc;
	u32_t  flags;
	u8_t *p_buf;
	u16_t pkt_data_len;
	u16_t buf_bytes, buf_len;
	int i, r, size;

	/* Are we called from the interrupt handler? */
	if (!from_int)
	{
		e->rx_message = *mp;
		e->client = mp->m_source;
		e->status |= LAN8710A_READING;
		e->rx_size = 0;

		assert(e->rx_message.DL_COUNT > 0);
		assert(e->rx_message.DL_COUNT < LAN8710A_IOVEC_NR);
	}
	if (e->status & LAN8710A_READING)
	{
		/*
		 * Copy the I/O vector table first.
		 */
		if ((r = sys_safecopyfrom(e->rx_message.m_source,
				e->rx_message.DL_GRANT, 0, (vir_bytes) iovec,
				e->rx_message.DL_COUNT * sizeof(iovec_s_t))) != OK)
		{
			panic("sys_safecopyfrom() failed: %d", r);
		}

		/*
		 * Only handle one packet at a time.
		 */
		p_rx_desc = &(e->rx_desc[e->rx_desc_idx]);
		/* find next OWN descriptor with SOP flag */
		while ((0 == (LAN8710A_DESC_FLAG_SOP & p_rx_desc->pkt_len_flags))
			&& (0 == (LAN8710A_DESC_FLAG_OWN & p_rx_desc->pkt_len_flags)) )
		{
			p_rx_desc->buffer_length_off = LAN8710A_IOBUF_SIZE;
			p_rx_desc->pkt_len_flags = LAN8710A_DESC_FLAG_OWN; /* set ownership of current descriptor to EMAC*/

			e->rx_desc_idx++;
			if (LAN8710A_NUM_RX_DESC == e->rx_desc_idx)
				e->rx_desc_idx = 0;
			p_rx_desc = &(e->rx_desc[e->rx_desc_idx]);
		}
		if (0 == (LAN8710A_DESC_FLAG_SOP & p_rx_desc->pkt_len_flags))
		{  /* SOP was not found */
			reply(e);
			return;
		}

		/*
		 * Copy to vector elements.
		 */
		pkt_data_len = 0;
		buf_bytes = 0;
		p_buf = e->p_rx_buf + e->rx_desc_idx * LAN8710A_IOBUF_SIZE;
		for (i = 0; i < e->rx_message.DL_COUNT; i++)
		{
			buf_len = p_rx_desc->buffer_length_off & 0xFFFF;
			if (buf_bytes == buf_len)
			{ /* whole buffer copied -> move to the next descriptor */
				p_rx_desc->buffer_length_off = LAN8710A_IOBUF_SIZE;
				p_rx_desc->pkt_len_flags = LAN8710A_DESC_FLAG_OWN; /* set ownership of current descriptor to EMAC*/
				buf_bytes = 0;

				e->rx_desc_idx++;
				if (LAN8710A_NUM_RX_DESC == e->rx_desc_idx)
					e->rx_desc_idx = 0;
				p_rx_desc = &(e->rx_desc[e->rx_desc_idx]);
				p_buf = e->p_rx_buf + e->rx_desc_idx * LAN8710A_IOBUF_SIZE
						+ (p_rx_desc->buffer_length_off >> 16);
				buf_len = p_rx_desc->buffer_length_off & 0xFFFF;
			}
			size = iovec[i].iov_size < (buf_len - buf_bytes)
							? iovec[i].iov_size : (buf_len - buf_bytes);

			if ((r = sys_safecopyto(e->rx_message.m_source, iovec[i].iov_grant,
					0, (vir_bytes) p_buf, size)) != OK)
			{
				panic("sys_safecopyto() failed: %d", r);
			}
			p_buf += size;
			buf_bytes += size;
			pkt_data_len += size;

			/* if EOP flag is set -> stop processing */
			if ((LAN8710A_DESC_FLAG_EOP & p_rx_desc->pkt_len_flags)
				&& (buf_bytes == buf_len))/* end of packet */
			{
				break;
			}
		}
		do
		{	/* reset owned descriptors up to EOP flag */
			flags = p_rx_desc->pkt_len_flags;
			p_rx_desc->buffer_length_off = LAN8710A_IOBUF_SIZE;
			p_rx_desc->pkt_len_flags = LAN8710A_DESC_FLAG_OWN; /* set ownership of current descriptor to EMAC*/

			e->rx_desc_idx++;
			if (LAN8710A_NUM_RX_DESC == e->rx_desc_idx) 
				e->rx_desc_idx = 0;
				
			p_rx_desc = &(e->rx_desc[e->rx_desc_idx]);
		}
		while (0 == (flags & LAN8710A_DESC_FLAG_EOP));

		/*
		 * Update state.
		 */
		e->status |= LAN8710A_RECEIVED;
		e->rx_size = pkt_data_len;

	}
	reply(e);
}

/*===========================================================================*
 * 			lan8710a_phy_write				     *
 *===========================================================================*/
static void lan8710a_phy_write(reg, value)
u32_t reg;
u32_t value;
{
	if (!(lan8710a_reg_read(MDIOUSERACCESS0) & MDIO_GO))
	{
		/* Clearing MDIOUSERACCESS0 register */
		lan8710a_reg_write(MDIOUSERACCESS0, 0);
		/* Setting proper values in MDIOUSERACCESS0 */
		lan8710a_reg_set(MDIOUSERACCESS0, MDIO_WRITE);
		lan8710a_reg_set(MDIOUSERACCESS0, reg << MDIO_REGADR);
		lan8710a_reg_set(MDIOUSERACCESS0, lan8710a_state.phy_address << MDIO_PHYADR);
		lan8710a_reg_set(MDIOUSERACCESS0, (value & 0xFFFF) << MDIO_DATA); /* Data written only 16 bits. */
		lan8710a_reg_set(MDIOUSERACCESS0, MDIO_GO);

		/* Waiting for writing completion */
		while (lan8710a_reg_read(MDIOUSERACCESS0) & MDIO_GO);
	}
}

/*===========================================================================*
 * 			lan8710a_phy_read				     *
 *===========================================================================*/
static u32_t lan8710a_phy_read(reg)
u32_t reg;
{
	u32_t value = 0xFFFFFFFF;

	if (!(lan8710a_reg_read(MDIOUSERACCESS0) & MDIO_GO))
	{
		/* Clearing MDIOUSERACCESS0 register */
		lan8710a_reg_write(MDIOUSERACCESS0, 0);
		/* Setting proper values in MDIOUSERACCESS0 */
		lan8710a_reg_unset(MDIOUSERACCESS0, MDIO_WRITE);
		lan8710a_reg_set(MDIOUSERACCESS0, (reg & 0x1F) << MDIO_REGADR); /* Reg number must be 5 bit long */
		lan8710a_reg_set(MDIOUSERACCESS0, (lan8710a_state.phy_address & 0x1F) << MDIO_PHYADR); /* Addr must be 5 bit long */
		lan8710a_reg_set(MDIOUSERACCESS0, MDIO_GO);

		/* Waiting for reading completion */
		while ((lan8710a_reg_read(MDIOUSERACCESS0) & MDIO_GO)
				&& !(lan8710a_reg_read(MDIOUSERACCESS0) & MDIO_ACK));

		/* Reading data */
		value = lan8710a_reg_read(MDIOUSERACCESS0) & 0xFFFF;
	}
	return value;
}

/*===========================================================================*
 * 			lan8710a_reset_hw 				     *
 *===========================================================================*/
static void lan8710a_reset_hw()
{
	/* Assert a Device Reset signal. */
	lan8710a_phy_write(LAN8710A_CTRL_REG, LAN8710A_SOFT_RESET);

	/* Waiting for reset completion. */
	while(lan8710a_phy_read(LAN8710A_CTRL_REG) & LAN8710A_SOFT_RESET);
}

/*===========================================================================*
 * 			lan8710a_reg_read 				     *
 *===========================================================================*/
static u32_t lan8710a_reg_read(reg)
volatile u32_t *reg;
{
	u32_t value;

	/* Read from memory mapped register. */
	value = *reg;

	/* Return the result. */
	return value;
}

/*===========================================================================*
 * 			lan8710a_reg_write 				     *
 *===========================================================================*/
static void lan8710a_reg_write(reg, value)
volatile u32_t *reg;
u32_t value;
{
	/* Write to memory mapped register. */
	*reg = value;
}

/*===========================================================================*
 * 			lan8710a_reg_set 				     *
 *===========================================================================*/
static void lan8710a_reg_set(reg, value)
volatile u32_t *reg;
u32_t value;
{
	u32_t data;

	/* First read the current value. */
	data = lan8710a_reg_read(reg);

	/* Set value, and write back. */
	lan8710a_reg_write(reg, data | value);
}

/*===========================================================================*
 * 			lan8710a_reg_unset				     *
 *===========================================================================*/
static void lan8710a_reg_unset(reg, value)
volatile u32_t *reg;
u32_t value;
{
	u32_t data;

	/* First read the current value. */
	data = lan8710a_reg_read(reg);

	/* Unset value, and write back. */
	lan8710a_reg_write(reg, data & ~value);
}

/*===========================================================================*
 * 			mess_reply 					     *
 *===========================================================================*/
static void mess_reply(req, reply)
message *req;message *reply;
{
	if (send(req->m_source, reply) != OK)
	{
		panic("unable to send reply message");
	}
}

/*===========================================================================*
 * 			reply	 					     *
 *===========================================================================*/
static void reply(e)
lan8710a_t *e;
{
	message msg;
	int r;

	/* Only reply to client for read/write request. */
	if (!(e->status & LAN8710A_READING ||
		  e->status & LAN8710A_WRITING))
	{
		return;
	}
	/* Construct reply message. */
	msg.m_type   = DL_TASK_REPLY;
	msg.DL_FLAGS = DL_NOFLAGS;
	msg.DL_COUNT = 0;

	/* Did we successfully receive packet(s)? */
	if (e->status & LAN8710A_READING &&
	e->status & LAN8710A_RECEIVED)
	{
		msg.DL_FLAGS |= DL_PACK_RECV;
		msg.DL_COUNT = e->rx_size >= ETH_MIN_PACK_SIZE ?
			   e->rx_size  : ETH_MIN_PACK_SIZE;

		/* Clear flags. */
		e->status &= ~(LAN8710A_READING | LAN8710A_RECEIVED);
	}
	/* Did we successfully transmit packet(s)? */
	if (e->status & LAN8710A_TRANSMIT &&
		e->status & LAN8710A_WRITING)
	{
		msg.DL_FLAGS |= DL_PACK_SEND;

		/* Clear flags. */
		e->status &= ~(LAN8710A_WRITING | LAN8710A_TRANSMIT);
	}

	/* Acknowledge to INET. */
	if ((r = send(e->client, &msg) != OK))
	{
		panic("send() failed: %d", r);
	}
}
#endif /* AM335X */
