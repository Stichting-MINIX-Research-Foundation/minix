#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <minix/board.h>
#include <sys/mman.h>
#include "assert.h"
#include "lan8710a.h"
#include "lan8710a_reg.h"

/* Local functions */
static int lan8710a_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks);
static void lan8710a_stop(void);
static ssize_t lan8710a_recv(struct netdriver_data *data, size_t max);
static int lan8710a_send(struct netdriver_data *data, size_t size);
static void lan8710a_intr(unsigned int mask);
static void lan8710a_tick(void);

static void lan8710a_enable_interrupt(int interrupt);
static void lan8710a_map_regs(void);
static void lan8710a_dma_config_tx(u8_t desc_idx);
static void lan8710a_dma_reset_init(void);
static void lan8710a_init_addr(netdriver_addr_t *addr, unsigned int instance);
static void lan8710a_init_desc(void);
static void lan8710a_init_mdio(void);
static int lan8710a_init_hw(netdriver_addr_t *addr, unsigned int instance);
static void lan8710a_reset_hw(void);

static void lan8710a_phy_write(u32_t reg, u32_t value);
static u32_t lan8710a_phy_read(u32_t reg);

static u32_t lan8710a_reg_read(volatile u32_t *reg);
static void lan8710a_reg_write(volatile u32_t *reg, u32_t value);
static void lan8710a_reg_set(volatile u32_t *reg, u32_t value);
static void lan8710a_reg_unset(volatile u32_t *reg, u32_t value);

/* Local variables */
static lan8710a_t lan8710a_state;

static const struct netdriver lan8710a_table = {
	.ndr_name	= "cpsw",
	.ndr_init	= lan8710a_init,
	.ndr_stop	= lan8710a_stop,
	.ndr_recv	= lan8710a_recv,
	.ndr_send	= lan8710a_send,
	.ndr_intr	= lan8710a_intr,
	.ndr_tick	= lan8710a_tick
};

/*============================================================================*
 *				main					      *
 *============================================================================*/
int
main(int argc, char *argv[])
{
	struct machine machine;

	env_setargs(argc, argv);

	sys_getmachine(&machine);
	if (BOARD_IS_BB(machine.board_id))
		netdriver_task(&lan8710a_table);

	return EXIT_SUCCESS;
}

/*============================================================================*
 *				lan8710a_init				      *
 *============================================================================*/
static int
lan8710a_init(unsigned int instance, netdriver_addr_t * addr, uint32_t * caps,
	unsigned int * ticks)
{
	/* Initialize the ethernet driver. */

	/* Clear state. */
	memset(&lan8710a_state, 0, sizeof(lan8710a_state));

	/* Initialize driver. */
	lan8710a_map_regs();

	lan8710a_init_hw(addr, instance);

	*caps = NDEV_CAP_MCAST | NDEV_CAP_BCAST;
	*ticks = sys_hz(); /* update statistics once a second */
	return OK;
}

/*============================================================================*
 *				lan8710a_enable_interrupt		      *
 *============================================================================*/
static void
lan8710a_enable_interrupt(int interrupt)
{
	int r;

	if (interrupt & RX_INT) {
		if ((r = sys_irqenable(&lan8710a_state.irq_rx_hook)) != OK) {
			panic("sys_irqenable failed: %d", r);
		}
	}
	if (interrupt & TX_INT) {
		if ((r = sys_irqenable(&lan8710a_state.irq_tx_hook)) != OK) {
			panic("sys_irqenable failed: %d", r);
		}
	}
}

/*============================================================================*
 *				lan8710a_intr				      *
 *============================================================================*/
static void
lan8710a_intr(unsigned int mask)
{
	u32_t dma_status;

	/* Check the card for interrupt reason(s). */
	u32_t rx_stat = lan8710a_reg_read(CPSW_WR_C0_RX_STAT);
	u32_t tx_stat = lan8710a_reg_read(CPSW_WR_C0_TX_STAT);
	u32_t cp;

	/* Handle interrupts. */
	if (rx_stat) {
		cp = lan8710a_reg_read(CPDMA_STRAM_RX_CP(0));

		netdriver_recv();

		lan8710a_reg_write(CPDMA_STRAM_RX_CP(0), cp);
		lan8710a_reg_write(CPDMA_EOI_VECTOR, RX_INT);
	}
	if (tx_stat) {
		cp = lan8710a_reg_read(CPDMA_STRAM_TX_CP(0));

		/* Disabling channels, where Tx interrupt occurred */
		lan8710a_reg_set(CPDMA_TX_INTMASK_CLEAR, tx_stat);

		netdriver_send();

		lan8710a_reg_write(CPDMA_STRAM_TX_CP(0), cp);
		lan8710a_reg_write(CPDMA_EOI_VECTOR, TX_INT);
	}

	dma_status = lan8710a_reg_read(CPDMA_STATUS);

	if (dma_status & CPDMA_ERROR) {
		LAN8710A_DEBUG_PRINT(("CPDMA error: 0x%X, reset", dma_status));
		lan8710a_dma_reset_init();
	}

	/* Re-enable Rx interrupt. */
	if (mask & (1 << RX_INT))
		lan8710a_enable_interrupt(RX_INT);

	/* Re-enable Tx interrupt. */
	if (mask & (1 << TX_INT))
		lan8710a_enable_interrupt(TX_INT);
}

/*============================================================================*
 *				lan8710a_init_addr			      *
 *============================================================================*/
static void
lan8710a_init_addr(netdriver_addr_t * addr, unsigned int instance)
{
	static char eakey[]= LAN8710A_ENVVAR "#_EA";
	static char eafmt[]= "x:x:x:x:x:x";
	int i;
	long v;

	/*
	 * Do we have a user defined ethernet address?
	 */
	eakey[sizeof(LAN8710A_ENVVAR)-1] = '0' + instance;

	for (i= 0; i < 6; i++) {
		if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
			break;
		else
			addr->na_addr[i] = v;
	}
	if (i == 6)
		return;

	/*
	 * No; get the address from the chip itself.
	 */
	addr->na_addr[0] = lan8710a_reg_read(CTRL_MAC_ID0_HI) & 0xFF;
	addr->na_addr[1] = (lan8710a_reg_read(CTRL_MAC_ID0_HI) >> 8) & 0xFF;
	addr->na_addr[2] = (lan8710a_reg_read(CTRL_MAC_ID0_HI) >> 16) & 0xFF;
	addr->na_addr[3] = (lan8710a_reg_read(CTRL_MAC_ID0_HI) >> 24) & 0xFF;
	addr->na_addr[4] = lan8710a_reg_read(CTRL_MAC_ID0_LO) & 0xFF;
	addr->na_addr[5] = (lan8710a_reg_read(CTRL_MAC_ID0_LO) >> 8) & 0xFF;
}

/*============================================================================*
 *				lan8710a_map_regs			      *
 *============================================================================*/
static void
lan8710a_map_regs(void)
{
	struct minix_mem_range mr;
	mr.mr_base = CM_PER_BASE_ADR;
	mr.mr_limit = CM_PER_BASE_ADR + MEMORY_LIMIT;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		panic("Unable to request permission to map memory");
	}
	lan8710a_state.regs_cp_per =
		(vir_bytes)vm_map_phys(SELF, (void *)CM_PER_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cp_per == MAP_FAILED) {
		panic("lan8710a_state.regs_cp_per: vm_map_phys failed");
	}
	lan8710a_state.regs_cpdma_stram =
		(vir_bytes)vm_map_phys(SELF, (void *)CPDMA_STRAM_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cpdma_stram == MAP_FAILED) {
		panic("lan8710a_state.regs_cpdma_stram: vm_map_phys failed");
	}
	lan8710a_state.regs_cpsw_cpdma =
		(vir_bytes)vm_map_phys(SELF, (void *)CPSW_CPDMA_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cpsw_cpdma == MAP_FAILED) {
		panic("lan8710a_state.regs_cpsw_cpdma: vm_map_phys failed");
	}
	lan8710a_state.regs_cpsw_ale =
		(vir_bytes)vm_map_phys(SELF, (void *)CPSW_ALE_BASE_ADR, 256);
	if ((void *)lan8710a_state.regs_cpsw_ale == MAP_FAILED) {
		panic("lan8710a_state.regs_cpsw_ale: vm_map_phys failed");
	}
	lan8710a_state.regs_cpsw_sl =
		(vir_bytes)vm_map_phys(SELF, (void *)CPSW_SL_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cpsw_sl == MAP_FAILED) {
		panic("lan8710a_state.regs_cpsw_sl: vm_map_phys failed");
	}
	lan8710a_state.regs_cpsw_ss =
		(vir_bytes)vm_map_phys(SELF, (void *)CPSW_SS_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cpsw_ss == MAP_FAILED) {
		panic("lan8710a_state.regs_cpsw_ss: vm_map_phys failed");
	}
	lan8710a_state.regs_cpsw_wr =
		(vir_bytes)vm_map_phys(SELF, (void *)CPSW_WR_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_cpsw_wr == MAP_FAILED) {
		panic("lan8710a_state.regs_cpsw_wr: vm_map_phys failed");
	}
	lan8710a_state.regs_ctrl_mod =
		(vir_bytes)vm_map_phys(SELF, (void *)CTRL_MOD_BASE_ADR, 2560);
	if ((void *)lan8710a_state.regs_ctrl_mod == MAP_FAILED) {
		panic("lan8710a_state.regs_ctrl_mod: vm_map_phys failed");
	}
	lan8710a_state.regs_intc =
		(vir_bytes)vm_map_phys(SELF, (void *)INTC_BASE_ADR, 512);
	if ((void *)lan8710a_state.regs_intc == MAP_FAILED) {
		panic("lan8710a_state.regs_intc: vm_map_phys failed");
	}
	lan8710a_state.regs_mdio =
		(vir_bytes)vm_map_phys(SELF, (void *)MDIO_BASE_ADDR, 512);
	if ((void *)lan8710a_state.regs_mdio == MAP_FAILED) {
		panic("lan8710a_state.regs_mdio: vm_map_phys failed");
	}

	mr.mr_base = BEGINNING_DESC_MEM;
	mr.mr_limit = BEGINNING_DESC_MEM + DESC_MEMORY_LIMIT;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		panic("Unable to request permission to map memory");
	}
	lan8710a_state.rx_desc_phy = BEGINNING_RX_DESC_MEM;
	lan8710a_state.tx_desc_phy = BEGINNING_TX_DESC_MEM;
	lan8710a_state.rx_desc = (lan8710a_desc_t *)vm_map_phys(SELF,
				(void *)lan8710a_state.rx_desc_phy, 1024);
	if ((void *)lan8710a_state.rx_desc == MAP_FAILED) {
		panic("lan8710a_state.rx_desc: vm_map_phys failed");
	}
	lan8710a_state.tx_desc = (lan8710a_desc_t *)vm_map_phys(SELF,
				(void *)lan8710a_state.tx_desc_phy, 1024);
	if ((void *)lan8710a_state.tx_desc == MAP_FAILED) {
		panic("lan8710a_state.tx_desc: vm_map_phys failed");
	}

	mr.mr_base = CPSW_STATS_BASE_ADR;
	mr.mr_limit = CPSW_STATS_BASE_ADR + CPSW_STATS_MEM_LIMIT;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		panic("Unable to request permission to map memory");
	}
	lan8710a_state.regs_cpsw_stats =
		(vir_bytes)vm_map_phys(SELF, (void *)CPSW_STATS_BASE_ADR, 256);
	if ((void *)lan8710a_state.regs_cpsw_stats == MAP_FAILED) {
		panic("lan8710a_state.regs_cpsw_stats: vm_map_phys failed");
	}
}

/*============================================================================*
 *				lan8710a_update_stats			      *
 *============================================================================*/
static void
lan8710a_update_stats(void)
{
	uint32_t val;

	/*
	 * AM335x Technical Reference (SPRUH73J) Sec. 14.3.2.20: statistics
	 * registers are decrement-on-write when any of the statistics port
	 * enable bits are set.
	 */
	val = lan8710a_reg_read(CPSW_STAT_RX_CRC_ERR);
	lan8710a_reg_write(CPSW_STAT_RX_CRC_ERR, val);
	netdriver_stat_ierror(val);

	val = lan8710a_reg_read(CPSW_STAT_RX_AGNCD_ERR);
	lan8710a_reg_write(CPSW_STAT_RX_AGNCD_ERR, val);
	netdriver_stat_ierror(val);

	val = lan8710a_reg_read(CPSW_STAT_RX_OVERSIZE);
	lan8710a_reg_write(CPSW_STAT_RX_OVERSIZE, val);
	netdriver_stat_ierror(val);

	val = lan8710a_reg_read(CPSW_STAT_COLLISIONS);
	lan8710a_reg_write(CPSW_STAT_COLLISIONS, val);
	netdriver_stat_coll(val);
}

/*============================================================================*
 *				lan8710a_tick				      *
 *============================================================================*/
static void
lan8710a_tick(void)
{

	/* Update statistics. */
	lan8710a_update_stats();
}

/*============================================================================*
 *				lan8710a_stop				      *
 *============================================================================*/
static void
lan8710a_stop(void)
{
	/* Reset hardware. */
	lan8710a_reset_hw();
}

/*============================================================================*
 *				lan8710a_dma_config_tx			      *
 *============================================================================*/
static void
lan8710a_dma_config_tx(u8_t desc_idx)
{
	phys_bytes phys_addr;
	int i;
	for (i = 0; i < TX_DMA_CHANNELS; ++i) {
		if (!lan8710a_reg_read(CPDMA_STRAM_TX_HDP(i))) break;
	}
	if (i == TX_DMA_CHANNELS) {
		panic("There are no free TX DMA channels.");
	}

	/* Enabling only one channel Tx interrupt */
	lan8710a_reg_write(CPDMA_TX_INTMASK_SET, 1 << i);
	/* Routing only one channel Tx int to TX_PULSE signal */
	lan8710a_reg_write(CPSW_WR_C0_TX_EN, 1 << i);

	/* Setting HDP */
	phys_addr = lan8710a_state.tx_desc_phy +
					(desc_idx * sizeof(lan8710a_desc_t));
	lan8710a_reg_write(CPDMA_STRAM_TX_HDP(i), (u32_t)phys_addr);
}

/*============================================================================*
 *				lan8710a_dma_reset_init			      *
 *============================================================================*/
static void
lan8710a_dma_reset_init(void)
{
	int i;
	lan8710a_reg_write(CPDMA_SOFT_RESET, SOFT_RESET);
	while ((lan8710a_reg_read(CPDMA_SOFT_RESET) & SOFT_RESET));

	/*
	 * Initialize the HDPs (Header Description Pointers) and
	 * CPs (Completion Pointers) to NULL.
	 */
	for (i = 0; i < DMA_MAX_CHANNELS; ++i) {
		lan8710a_reg_write(CPDMA_STRAM_TX_HDP(i), 0);
		lan8710a_reg_write(CPDMA_STRAM_RX_HDP(i), 0);
		lan8710a_reg_write(CPDMA_STRAM_TX_CP(i), 0);
		lan8710a_reg_write(CPDMA_STRAM_RX_CP(i), 0);
	}

	lan8710a_reg_write(CPDMA_RX_INTMASK_CLEAR, 0xFFFFFFFF);
	lan8710a_reg_write(CPDMA_TX_INTMASK_CLEAR, 0xFFFFFFFF);

	/* Configure the CPDMA controller. */
	lan8710a_reg_set(CPDMA_RX_CONTROL, CPDMA_RX_EN); /* RX Enabled */
	lan8710a_reg_set(CPDMA_TX_CONTROL, CPDMA_TX_EN); /* TX Enabled */

	/* Enabling first channel Rx interrupt */
	lan8710a_reg_set(CPDMA_RX_INTMASK_SET, CPDMA_FIRST_CHAN_INT);

	/*
	 * Writing the address of the first buffer descriptor in the queue
	 * (nonzero value)to the channel’s head descriptor pointer in the
	 * channel’s Rx DMA state.
	 */
	lan8710a_reg_write(CPDMA_STRAM_RX_HDP(0),
			  (u32_t)lan8710a_state.rx_desc_phy);

	lan8710a_state.rx_desc_idx = 0;
	lan8710a_state.tx_desc_idx = 0;
}

/*============================================================================*
 *				lan8710a_init_desc			      *
 *============================================================================*/
static void
lan8710a_init_desc(void)
{
	lan8710a_desc_t *p_rx_desc;
	lan8710a_desc_t *p_tx_desc;
	phys_bytes   buf_phys_addr;
	u8_t i;

	/* Attempt to allocate. */
	if ((lan8710a_state.p_rx_buf = alloc_contig((LAN8710A_NUM_RX_DESC
			* LAN8710A_IOBUF_SIZE), AC_ALIGN4K,
			&buf_phys_addr)) == NULL) {
		panic("failed to allocate RX buffers.");
	}
	for (i = 0; i < LAN8710A_NUM_RX_DESC; i++) {
		p_rx_desc = &(lan8710a_state.rx_desc[i]);
		memset(p_rx_desc, 0x0, sizeof(lan8710a_desc_t));
		p_rx_desc->pkt_len_flags = LAN8710A_DESC_FLAG_OWN;
		p_rx_desc->buffer_length_off = LAN8710A_IOBUF_SIZE;
		p_rx_desc->buffer_pointer = (u32_t)(buf_phys_addr +
						(i * LAN8710A_IOBUF_SIZE));

		p_rx_desc->next_pointer =
		   (u32_t)((i == (LAN8710A_NUM_RX_DESC - 1)) ?
			   (lan8710a_state.rx_desc_phy) :
			   (lan8710a_state.rx_desc_phy +
			     ((i + 1) * sizeof(lan8710a_desc_t))));
	}

	/* Attempt to allocate. */
	if ((lan8710a_state.p_tx_buf = alloc_contig((LAN8710A_NUM_TX_DESC
			* LAN8710A_IOBUF_SIZE), AC_ALIGN4K,
			&buf_phys_addr)) == NULL) {
		panic("failed to allocate TX buffers");
	}
	for (i = 0; i < LAN8710A_NUM_TX_DESC; i++) {
		p_tx_desc = &(lan8710a_state.tx_desc[i]);
		memset(p_tx_desc, 0x0, sizeof(lan8710a_desc_t));
		p_tx_desc->buffer_pointer = (u32_t)(buf_phys_addr +
				(i * LAN8710A_IOBUF_SIZE));
	}
	lan8710a_state.rx_desc_idx = 0;
	lan8710a_state.tx_desc_idx = 0;
}

/*============================================================================*
 *				lan8710a_init_hw			      *
 *============================================================================*/
static int
lan8710a_init_hw(netdriver_addr_t * addr, unsigned int instance)
{
	int r, i;

	/*
	 * Set the interrupt handler and policy. Do not automatically
	 * re-enable interrupts. Return the IRQ line number on interrupts.
	 */
	lan8710a_state.irq_rx_hook = RX_INT;
	if ((r = sys_irqsetpolicy(LAN8710A_RX_INTR, 0,
					&lan8710a_state.irq_rx_hook)) != OK) {
		panic("sys_irqsetpolicy failed: %d", r);
	}
	lan8710a_state.irq_tx_hook = TX_INT;
	if ((r = sys_irqsetpolicy(LAN8710A_TX_INTR, 0,
					&lan8710a_state.irq_tx_hook)) != OK) {
		panic("sys_irqsetpolicy failed: %d", r);
	}

	/* Reset hardware. */
	lan8710a_reset_hw();

	/*
	 * Select the Interface (GMII/RGMII/MII) Mode in the Control Module.
	 * Port1 GMII/MII Mode, Port2 not used.
	 */
	lan8710a_reg_write(GMII_SEL, (GMII2_SEL_BIT1 | GMII2_SEL_BIT0));

	/*
	 * Configure pads (PIN muxing) as per the Interface Selected using the
	 * appropriate pin muxing conf_xxx registers in the Control Module.
	 *
	 * CONF_MOD_SLEW_CTRL when 0 - Fast Mode, when 1 - Slow Mode
	 * CONF_MOD_RX_ACTIVE when 0 - Only output, when 1 - Also input
	 * CONF_MOD_PU_TYPESEL when 0 - Pull-down, when 1 - Pull-up
	 * CONF_MOD_PUDEN when 0 Pull* enabled, when 1 Pull* disabled
	 * CONF_MOD_MMODE_MII selects pin to work for MII interface
	 */
	lan8710a_reg_unset(CONF_MII1_COL, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MII1_COL, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_COL, CONF_MOD_PU_TYPESEL);
	lan8710a_reg_unset(CONF_MII1_COL, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_COL, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_CRS, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MII1_CRS, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_CRS, CONF_MOD_PU_TYPESEL);
	lan8710a_reg_unset(CONF_MII1_CRS, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_CRS, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_RX_ER, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MII1_RX_ER, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_RX_ER, CONF_MOD_PU_TYPESEL);
	lan8710a_reg_unset(CONF_MII1_RX_ER, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_RX_ER, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_TX_EN, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_unset(CONF_MII1_TX_EN, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_TX_EN, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_TX_EN, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_RX_DV, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MII1_RX_DV, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_RX_DV, CONF_MOD_PU_TYPESEL);
	lan8710a_reg_unset(CONF_MII1_RX_DV, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_RX_DV, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_TXD3, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_unset(CONF_MII1_TXD3, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_TXD3, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_TXD3, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_TXD2, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_unset(CONF_MII1_TXD2, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_TXD2, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_TXD2, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_TXD1, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_unset(CONF_MII1_TXD1, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_TXD1, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_TXD1, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_TXD0, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_unset(CONF_MII1_TXD0, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_TXD0, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_TXD0, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_TX_CLK, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MII1_TX_CLK, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_TX_CLK, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_TX_CLK, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_RX_CLK, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MII1_RX_CLK, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_RX_CLK, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_RX_CLK, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_RXD3, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MII1_RXD3, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_RXD3, CONF_MOD_PU_TYPESEL);
	lan8710a_reg_unset(CONF_MII1_RXD3, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_RXD3, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_RXD2, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MII1_RXD2, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_RXD2, CONF_MOD_PU_TYPESEL);
	lan8710a_reg_unset(CONF_MII1_RXD2, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_RXD2, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_RXD1, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MII1_RXD1, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_RXD1, CONF_MOD_PU_TYPESEL);
	lan8710a_reg_unset(CONF_MII1_RXD1, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_RXD1, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MII1_RXD0, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MII1_RXD0, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MII1_RXD0, CONF_MOD_PU_TYPESEL);
	lan8710a_reg_unset(CONF_MII1_RXD0, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MII1_RXD0, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MDIO, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_set(CONF_MDIO, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MDIO, CONF_MOD_PU_TYPESEL);
	lan8710a_reg_unset(CONF_MDIO, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MDIO, CONF_MOD_MMODE_MII);

	lan8710a_reg_unset(CONF_MDC, CONF_MOD_SLEW_CTRL);
	lan8710a_reg_unset(CONF_MDC, CONF_MOD_RX_ACTIVE);
	lan8710a_reg_set(CONF_MDC, CONF_MOD_PUDEN);
	lan8710a_reg_unset(CONF_MDC, CONF_MOD_MMODE_MII);

	/* Apply soft reset to 3PSW Subsytem, CPSW_3G, CPGMAC_SL, and CPDMA. */
	lan8710a_reg_write(CPSW_SS_SOFT_RESET, SOFT_RESET);
	lan8710a_reg_write(CPSW_SL_SOFT_RESET(1), SOFT_RESET);
	lan8710a_reg_write(CPSW_SL_SOFT_RESET(2), SOFT_RESET);

	/* Wait for software resets completion */
	while ((lan8710a_reg_read(CPSW_SS_SOFT_RESET) & SOFT_RESET) ||
		(lan8710a_reg_read(CPSW_SL_SOFT_RESET(1)) & SOFT_RESET) ||
		(lan8710a_reg_read(CPSW_SL_SOFT_RESET(2)) & SOFT_RESET));

	/* Configure the Statistics Port Enable register. */
	/* Enable port 0 and 1 statistics. */
	lan8710a_reg_write(CPSW_SS_STAT_PORT_EN, (CPSW_P1_STAT_EN |
							CPSW_P0_STAT_EN));

	/*
	 * Configure the ALE.
	 * Enabling Ale.
	 * All packets received on ports 1 are
	 * sent to the host (only to the host).
	 */
	lan8710a_reg_write(CPSW_ALE_CONTROL, (CPSW_ALE_ENABLE |
						CPSW_ALE_BYPASS));
	/* Port 0 (host) in forwarding mode. */
	lan8710a_reg_write(CPSW_ALE_PORTCTL0, CPSW_ALE_PORT_FWD);
	/* Port 1 in forwarding mode. */
	lan8710a_reg_write(CPSW_ALE_PORTCTL1, CPSW_ALE_PORT_FWD);

	/*
	 * Configure CPSW_SL Register
	 * Full duplex mode.
	 */
	lan8710a_reg_write(CPSW_SL_MACCONTROL(1), CPSW_SL_FULLDUPLEX);

	/* Initialize MDIO Protocol */
	lan8710a_init_mdio();

	/* Getting MAC Address */
	lan8710a_init_addr(addr, instance);

	/* Initialize descriptors */
	lan8710a_init_desc();

	/* Reset and initialize CPDMA */
	lan8710a_dma_reset_init();

	/*
	 * Configure the Interrupts.
	 * Routing all channel Rx int to RX_PULSE signal.
	 */
	lan8710a_reg_set(CPSW_WR_C0_RX_EN, CPSW_FIRST_CHAN_INT);

	/*
	 * Enabling LAN8710A Auto-negotiation
	 */
	lan8710a_phy_write(LAN8710A_CTRL_REG, LAN8710A_AUTO_NEG);

	/* Waiting for auto-negotiaion completion. */
	for (i = 0; !(lan8710a_phy_read(LAN8710A_STATUS_REG) &
					LAN8710A_AUTO_NEG_COMPL); ++i) {
		if (i == 100) {
			LAN8710A_DEBUG_PRINT(("Autonegotiation failed"));
			break;
		}
		micro_delay(1666666);
	}

	/* GMII RX and TX release from reset. */
	lan8710a_reg_set(CPSW_SL_MACCONTROL(1), CPSW_SL_GMII_EN);
	
	/* Enable interrupts. */
	lan8710a_enable_interrupt(RX_INT | TX_INT);

	return TRUE;
}

/*============================================================================*
 *				lan8710a_init_mdio			      *
 *============================================================================*/
static void
lan8710a_init_mdio(void)
{
	u16_t address = 0;
	u32_t r;

	/* Clearing MDIOCONTROL register */
	lan8710a_reg_write(MDIOCONTROL, 0);
	/* Configure the PREAMBLE and CLKDIV in the MDIO control register */
	lan8710a_reg_unset(MDIOCONTROL, MDIO_PREAMBLE); /* CLKDIV default */
	/* Enable sending MDIO frame preambles */
	lan8710a_reg_set(MDIOCONTROL, (MDCLK_DIVIDER | MDIO_ENABLE));
	/* Enable the MDIO module by setting the ENABLE bit in MDIOCONTROL */

	while (!(r = lan8710a_reg_read(MDIOALIVE)));

	/* Get PHY address */
	while (r >>= 1) {
		++address;
	}
	lan8710a_state.phy_address = address;

	/* Setup appropiate address in MDIOUSERPHYSEL0 */
	lan8710a_reg_set(MDIOUSERPHYSEL0, address);
}

/*============================================================================*
 *				lan8710a_send				      *
 *============================================================================*/
static int
lan8710a_send(struct netdriver_data * data, size_t size)
{
	lan8710a_t *e = &lan8710a_state;
	lan8710a_desc_t *p_tx_desc;
	u8_t *buf;

	/* setup descriptors */
	p_tx_desc = &(e->tx_desc[e->tx_desc_idx]);

	/*
	 * Check if descriptor is available for host and suspend if not.
	 */
	if (LAN8710A_DESC_FLAG_OWN & p_tx_desc->pkt_len_flags)
		return SUSPEND;

	/* Drop packets that exceed the size of our transmission buffer. */
	if (size > LAN8710A_IOBUF_SIZE) {
		printf("%s: dropping large packet (%zu)\n",
		    netdriver_name(), size);

		return OK;
	}

	/* virtual address of buffer */
	buf = e->p_tx_buf + e->tx_desc_idx * LAN8710A_IOBUF_SIZE;

	netdriver_copyin(data, 0, buf, size);

	/* set descriptor length */
	p_tx_desc->buffer_length_off = size;
	/* set flags */
	p_tx_desc->pkt_len_flags = (LAN8710A_DESC_FLAG_OWN |
					LAN8710A_DESC_FLAG_SOP |
					LAN8710A_DESC_FLAG_EOP |
					TX_DESC_TO_PORT1 |
					TX_DESC_TO_PORT_EN);
	p_tx_desc->pkt_len_flags |= size;

	/* setup DMA transfer */
	lan8710a_dma_config_tx(e->tx_desc_idx);

	e->tx_desc_idx++;
	if (LAN8710A_NUM_TX_DESC == e->tx_desc_idx)
		e->tx_desc_idx = 0;

	return OK;
}

/*============================================================================*
 *				lan8710a_recv				      *
 *============================================================================*/
static ssize_t
lan8710a_recv(struct netdriver_data * data, size_t max)
{
	lan8710a_t *e = &lan8710a_state;
	lan8710a_desc_t *p_rx_desc;
	u32_t flags;
	u8_t *buf;
	size_t off, size, chunk;

	/*
	 * Only handle one packet at a time.
	 */
	p_rx_desc = &(e->rx_desc[e->rx_desc_idx]);
	/* find next OWN descriptor with SOP flag */
	while ((0 == (LAN8710A_DESC_FLAG_SOP & p_rx_desc->pkt_len_flags)) &&
	    (0 == (LAN8710A_DESC_FLAG_OWN & p_rx_desc->pkt_len_flags))) {
		p_rx_desc->buffer_length_off = LAN8710A_IOBUF_SIZE;
		/* set ownership of current descriptor to EMAC */
		p_rx_desc->pkt_len_flags = LAN8710A_DESC_FLAG_OWN;

		e->rx_desc_idx++;
		if (LAN8710A_NUM_RX_DESC == e->rx_desc_idx)
			e->rx_desc_idx = 0;
		p_rx_desc = &(e->rx_desc[e->rx_desc_idx]);
	}

	if (0 == (LAN8710A_DESC_FLAG_SOP & p_rx_desc->pkt_len_flags)) {
		/* SOP was not found */
		return SUSPEND;
	}

	/*
	 * Copy data from descriptors, from SOP to EOP inclusive.
	 * TODO: make sure that the presence of a SOP slot implies the presence
	 * of an EOP slot, because we are not checking for ownership below..
	 */
	size = 0;
	off = 0;

	for (;;) {
		buf = e->p_rx_buf + e->rx_desc_idx * LAN8710A_IOBUF_SIZE + off;
		chunk = p_rx_desc->buffer_length_off & 0xFFFF;

		/* Truncate packets that are too large. */
		if (chunk > max - size)
			chunk = max - size;

		if (chunk > 0) {
			netdriver_copyout(data, size, buf, chunk);

			size += chunk;
		}

		flags = p_rx_desc->pkt_len_flags;

		/* Whole buffer move to the next descriptor */
		p_rx_desc->buffer_length_off = LAN8710A_IOBUF_SIZE;
		/* set ownership of current desc to EMAC */
		p_rx_desc->pkt_len_flags = LAN8710A_DESC_FLAG_OWN;

		e->rx_desc_idx++;
		if (LAN8710A_NUM_RX_DESC == e->rx_desc_idx)
			e->rx_desc_idx = 0;
		p_rx_desc = &(e->rx_desc[e->rx_desc_idx]);

		/* if EOP flag is set -> stop processing */
		if (flags & LAN8710A_DESC_FLAG_EOP)
			break;

		/*
		 * TODO: the upper 16 bits of buffer_length_off are used *only*
		 * for descriptors *after* the first one; I'm retaining this
		 * behavior because I don't have the chip's spec, but it may be
		 * better to simplify/correct this behavior. --David
		 */
		off = p_rx_desc->buffer_length_off >> 16;
	}

	return size;
}

/*============================================================================*
 *				lan8710a_phy_write			      *
 *============================================================================*/
static void
lan8710a_phy_write(u32_t reg, u32_t value)
{
	if (!(lan8710a_reg_read(MDIOUSERACCESS0) & MDIO_GO)) {
		/* Clearing MDIOUSERACCESS0 register */
		lan8710a_reg_write(MDIOUSERACCESS0, 0);
		/* Setting proper values in MDIOUSERACCESS0 */
		lan8710a_reg_set(MDIOUSERACCESS0, MDIO_WRITE);
		lan8710a_reg_set(MDIOUSERACCESS0, reg << MDIO_REGADR);
		lan8710a_reg_set(MDIOUSERACCESS0,
				lan8710a_state.phy_address << MDIO_PHYADR);
		/* Data written only 16 bits. */
		lan8710a_reg_set(MDIOUSERACCESS0,
		    (value & 0xFFFF) << MDIO_DATA);
		lan8710a_reg_set(MDIOUSERACCESS0, MDIO_GO);

		/* Waiting for writing completion */
		while (lan8710a_reg_read(MDIOUSERACCESS0) & MDIO_GO);
	}
}

/*============================================================================*
 *				lan8710a_phy_read			      *
 *============================================================================*/
static u32_t
lan8710a_phy_read(u32_t reg)
{
	u32_t value = 0xFFFFFFFF;

	if (!(lan8710a_reg_read(MDIOUSERACCESS0) & MDIO_GO)) {
		/* Clearing MDIOUSERACCESS0 register */
		lan8710a_reg_write(MDIOUSERACCESS0, 0);
		/* Setting proper values in MDIOUSERACCESS0 */
		lan8710a_reg_unset(MDIOUSERACCESS0, MDIO_WRITE);
		/* Reg number must be 5 bit long */
		lan8710a_reg_set(MDIOUSERACCESS0, (reg & 0x1F) << MDIO_REGADR);
		/* Addr must be 5 bit long */
		lan8710a_reg_set(MDIOUSERACCESS0,
			(lan8710a_state.phy_address & 0x1F) << MDIO_PHYADR);
		lan8710a_reg_set(MDIOUSERACCESS0, MDIO_GO);

		/* Waiting for reading completion */
		while ((lan8710a_reg_read(MDIOUSERACCESS0) & MDIO_GO)
			&& !(lan8710a_reg_read(MDIOUSERACCESS0) & MDIO_ACK));

		/* Reading data */
		value = lan8710a_reg_read(MDIOUSERACCESS0) & 0xFFFF;
	}
	return value;
}

/*============================================================================*
 *				lan8710a_reset_hw			      *
 *============================================================================*/
static void
lan8710a_reset_hw(void)
{
	/* Assert a Device Reset signal. */
	lan8710a_phy_write(LAN8710A_CTRL_REG, LAN8710A_SOFT_RESET);

	/* Waiting for reset completion. */
	while (lan8710a_phy_read(LAN8710A_CTRL_REG) & LAN8710A_SOFT_RESET);
}

/*============================================================================*
 *				lan8710a_reg_read			      *
 *============================================================================*/
static u32_t
lan8710a_reg_read(volatile u32_t *reg)
{
	u32_t value;

	/* Read from memory mapped register. */
	value = *reg;

	/* Return the result. */
	return value;
}

/*============================================================================*
 *				lan8710a_reg_write			      *
 *============================================================================*/
static void
lan8710a_reg_write(volatile u32_t *reg, u32_t value)
{
	/* Write to memory mapped register. */
	*reg = value;
}

/*============================================================================*
 *				lan8710a_reg_set			      *
 *============================================================================*/
static void
lan8710a_reg_set(volatile u32_t *reg, u32_t value)
{
	u32_t data;

	/* First read the current value. */
	data = lan8710a_reg_read(reg);

	/* Set value, and write back. */
	lan8710a_reg_write(reg, data | value);
}

/*============================================================================*
 *				lan8710a_reg_unset			      *
 *============================================================================*/
static void
lan8710a_reg_unset(volatile u32_t *reg, u32_t value)
{
	u32_t data;

	/* First read the current value. */
	data = lan8710a_reg_read(reg);

	/* Unset value, and write back. */
	lan8710a_reg_write(reg, data & ~value);
}
