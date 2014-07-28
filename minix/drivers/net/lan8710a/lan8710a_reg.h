#ifndef LAN8710A_REG_H_
#define LAN8710A_REG_H_

/* How much memory we should map */
#define MEMORY_LIMIT		(0x5302000)
#define BEGINNING_DESC_MEM	(0x4A102000)
#define DESC_MEMORY_LIMIT	(0x2000)
#define BEGINNING_RX_DESC_MEM	(0x4A102000)
#define BEGINNING_TX_DESC_MEM	(0x4A103000)

/* MDIO Registers */
#define MDIO_BASE_ADDR		(0x4A101000)
#define MDIOVER			((volatile u32_t *)( lan8710a_state.regs_mdio + 0x00))
#define MDIOCONTROL		((volatile u32_t *)( lan8710a_state.regs_mdio + 0x04))
#define MDIOALIVE		((volatile u32_t *)( lan8710a_state.regs_mdio + 0x08))
#define MDIOLINK		((volatile u32_t *)( lan8710a_state.regs_mdio + 0x0C))
#define MDIOLINKINTRAW		((volatile u32_t *)( lan8710a_state.regs_mdio + 0x10))
#define MDIOLINKINTMASKED	((volatile u32_t *)( lan8710a_state.regs_mdio + 0x14))
#define MDIOUSERINTRAW		((volatile u32_t *)( lan8710a_state.regs_mdio + 0x20))
#define MDIOUSERINTMASKED	((volatile u32_t *)( lan8710a_state.regs_mdio + 0x24))
#define MDIOUSERINTMASKSET	((volatile u32_t *)( lan8710a_state.regs_mdio + 0x28))
#define MDIOUSERINTMASKCLR	((volatile u32_t *)( lan8710a_state.regs_mdio + 0x2C))
#define MDIOUSERACCESS0		((volatile u32_t *)( lan8710a_state.regs_mdio + 0x80))
#define MDIOUSERPHYSEL0		((volatile u32_t *)( lan8710a_state.regs_mdio + 0x84))
#define MDIOUSERACCESS1		((volatile u32_t *)( lan8710a_state.regs_mdio + 0x88))
#define MDIOUSERPHYSEL1		((volatile u32_t *)( lan8710a_state.regs_mdio + 0x8C))

#define MDIO_PREAMBLE		(1 << 20)
#define MDCLK_DIVIDER		(0x255)
#define MDIO_ENABLE		(1 << 30)
#define MDIO_GO			(1 << 31)
#define MDIO_WRITE		(1 << 30)
#define MDIO_ACK		(1 << 29)

#define MDIO_REGADR		(21)
#define MDIO_PHYADR		(16)
#define MDIO_DATA		(0)

/* CONTROL MODULE Registers */
#define CTRL_MOD_BASE_ADR	(0x44E10000)
#define CTRL_MAC_ID0_LO		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x630))
#define CTRL_MAC_ID0_HI		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x634))
#define GMII_SEL		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x650))
#define CONF_MII1_COL		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x908))
#define CONF_MII1_CRS		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x90C))
#define CONF_MII1_RX_ER		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x910))
#define CONF_MII1_TX_EN		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x914))
#define CONF_MII1_RX_DV		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x918))
#define CONF_MII1_TXD3		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x91C))
#define CONF_MII1_TXD2		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x920))
#define CONF_MII1_TXD1		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x924))
#define CONF_MII1_TXD0		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x928))
#define CONF_MII1_TX_CLK	((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x92C))
#define CONF_MII1_RX_CLK	((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x930))
#define CONF_MII1_RXD3		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x934))
#define CONF_MII1_RXD2		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x938))
#define CONF_MII1_RXD1		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x93C))
#define CONF_MII1_RXD0		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x940))
#define CONF_MDIO		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x948))
#define CONF_MDC		((volatile u32_t *)( lan8710a_state.regs_ctrl_mod + 0x94C))

#define CONF_MOD_SLEW_CTRL	(1 << 6)
#define CONF_MOD_RX_ACTIVE	(1 << 5)
#define CONF_MOD_PU_TYPESEL	(1 << 4)
#define CONF_MOD_PUDEN		(1 << 3)
#define CONF_MOD_MMODE_MII	(7 << 0)
#define RMII1_IO_CLK_EN		(1 << 6)
#define RGMII1_IDMODE		(1 << 4)
#define GMII2_SEL_BIT1		(1 << 3)
#define GMII2_SEL_BIT0		(1 << 2)
#define GMII1_SEL_BIT1		(1 << 1)
#define GMII1_SEL_BIT0		(1 << 0)

/* CLOCK MODULE Registers */
#define CM_PER_BASE_ADR		(0x44E00000)
#define CM_PER_CPSW_CLKSTCTRL	((volatile u32_t *)( lan8710a_state.regs_cp_per + 0x144))

#define CM_PER_CPSW_CLKSTCTRL_BIT1	(1 << 1)
#define CM_PER_CPSW_CLKSTCTRL_BIT0	(1 << 0)

/* CPSW_ALE Registers */
#define CPSW_ALE_BASE_ADR	(0x4A100D00)
#define CPSW_ALE_CONTROL	((volatile u32_t *)( lan8710a_state.regs_cpsw_ale + 0x08))
#define CPSW_ALE_PORTCTL0	((volatile u32_t *)( lan8710a_state.regs_cpsw_ale + 0x40))
#define CPSW_ALE_PORTCTL1	((volatile u32_t *)( lan8710a_state.regs_cpsw_ale + 0x44))

#define CPSW_ALE_ENABLE		(1 << 31)
#define CPSW_ALE_BYPASS		(1 << 4)
#define CPSW_ALE_PORT_FWD	(3 << 0)

/* CPSW_SL Registers */
#define CPSW_SL_BASE_ADR	(0x4A100D80)
#define CPSW_SL_MACCONTROL(x)	((volatile u32_t *)( lan8710a_state.regs_cpsw_sl + ((x)-1)*64 + 0x04))
#define CPSW_SL_SOFT_RESET(x)	((volatile u32_t *)( lan8710a_state.regs_cpsw_sl + ((x)-1)*64 + 0x0C))
#define CPSW_SL_RX_MAXLEN(x)	((volatile u32_t *)( lan8710a_state.regs_cpsw_sl + ((x)-1)*64 + 0x10))
#define CPSW_SL_BOFFTEST(x)	((volatile u32_t *)( lan8710a_state.regs_cpsw_sl + ((x)-1)*64 + 0x14))
#define CPSW_SL_EMCONTROL(x)	((volatile u32_t *)( lan8710a_state.regs_cpsw_sl + ((x)-1)*64 + 0x20))
#define CPSW_SL_RX_PRI_MAP(x)	((volatile u32_t *)( lan8710a_state.regs_cpsw_sl + ((x)-1)*64 + 0x24))
#define CPSW_SL_TX_GAP(x)	((volatile u32_t *)( lan8710a_state.regs_cpsw_sl + ((x)-1)*64 + 0x28))

#define CPSW_SL_GMII_EN		(1 << 5)
#define CPSW_SL_FULLDUPLEX	(1 << 0)
#define SOFT_RESET		(1 << 0)

/* CPSW_STATS Registers */
#define CPSW_STATS_BASE_ADR	(0x4A100900)
#define CPSW_STATS_MEM_LIMIT	(0x90)
#define CPSW_STAT_RX_GOOD	((volatile u32_t *)( lan8710a_state.regs_cpsw_stats + 0x00))
#define CPSW_STAT_RX_CRC_ERR	((volatile u32_t *)( lan8710a_state.regs_cpsw_stats + 0x10))
#define CPSW_STAT_RX_AGNCD_ERR	((volatile u32_t *)( lan8710a_state.regs_cpsw_stats + 0x14))
#define CPSW_STAT_RX_OVERSIZE	((volatile u32_t *)( lan8710a_state.regs_cpsw_stats + 0x18))
#define CPSW_STAT_TX_GOOD	((volatile u32_t *)( lan8710a_state.regs_cpsw_stats + 0x34))
#define CPSW_STAT_COLLISIONS	((volatile u32_t *)( lan8710a_state.regs_cpsw_stats + 0x48))
#define CPSW_STAT_TX_UNDERRUN	((volatile u32_t *)( lan8710a_state.regs_cpsw_stats + 0x5C))
#define CPSW_STAT_CARR_SENS_ERR	((volatile u32_t *)( lan8710a_state.regs_cpsw_stats + 0x60))
#define CPSW_STAT_RX_OVERRUN	((volatile u32_t *)( lan8710a_state.regs_cpsw_stats + 0x8C))

/* CPSW_CPDMA Registers */
#define CPSW_CPDMA_BASE_ADR	(0x4A100800)
#define CPDMA_SOFT_RESET	((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0x1C))
#define CPDMA_TX_CONTROL	((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0x04))
#define CPDMA_RX_CONTROL	((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0x14))
#define CPDMA_CONTROL		((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0x20))
#define CPDMA_STATUS		((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0x24))
#define CPDMA_RX_BUFFER_OFFSET	((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0x28))
#define CPDMA_EMCONTROL		((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0x2C))
#define CPDMA_TX_INTMASK_SET	((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0x88))
#define CPDMA_TX_INTMASK_CLEAR	((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0x8C))
#define CPDMA_EOI_VECTOR	((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0x94))
#define CPDMA_RX_INTMASK_SET	((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0xA8))
#define CPDMA_RX_INTMASK_CLEAR	((volatile u32_t *)( lan8710a_state.regs_cpsw_cpdma + 0xAC))

#define CPDMA_IDLE		(1 << 31)
#define CPDMA_TX_RLIM		(0xFF << 8)
#define CPDMA_NO_OFFSET		(0xFFFF << 0)
#define CPDMA_RX_CEF		(1 << 4)
#define CPDMA_CMD_IDLE		(1 << 3)
#define RX_OFFLEN_BLOCK		(1 << 2)
#define RX_OWNERSHIP		(1 << 1)
#define TX_PTYPE		(1 << 0)
#define CPDMA_TX_EN		(1 << 0)
#define CPDMA_RX_EN		(1 << 0)
#define CPDMA_FIRST_CHAN_INT	(1 << 0)
#define CPDMA_ALL_CHAN_INT	(0xFF << 0)
#define CPDMA_TX_PTYPE		(1 << 0)
#define CPDMA_ERROR		(0x00F7F700)

/* CPSW_SS Registers */
#define CPSW_SS_BASE_ADR	(0x4A100000)
#define CPSW_SS_SOFT_RESET	((volatile u32_t *)( lan8710a_state.regs_cpsw_ss + 0x08))
#define CPSW_SS_STAT_PORT_EN	((volatile u32_t *)( lan8710a_state.regs_cpsw_ss + 0x0C))
#define CPSW_SS_TX_START_WDS	((volatile u32_t *)( lan8710a_state.regs_cpsw_ss + 0x20))

#define CPSW_P2_STAT_EN		(1 << 2)
#define CPSW_P1_STAT_EN		(1 << 1)
#define CPSW_P0_STAT_EN		(1 << 0)

/* CPSW_WR Registers */
#define CPSW_WR_BASE_ADR	(0x4A101200)
#define CPSW_WR_INT_CONTROL	((volatile u32_t *)( lan8710a_state.regs_cpsw_wr + 0x0C))
#define CPSW_WR_C0_RX_EN	((volatile u32_t *)( lan8710a_state.regs_cpsw_wr + 0x14))
#define CPSW_WR_C1_RX_EN	((volatile u32_t *)( lan8710a_state.regs_cpsw_wr + 0x24))
#define CPSW_WR_C2_RX_EN	((volatile u32_t *)( lan8710a_state.regs_cpsw_wr + 0x34))
#define CPSW_WR_C0_RX_STAT	((volatile u32_t *)( lan8710a_state.regs_cpsw_wr + 0x44))
#define CPSW_WR_C0_TX_EN	((volatile u32_t *)( lan8710a_state.regs_cpsw_wr + 0x18))
#define CPSW_WR_C0_TX_STAT	((volatile u32_t *)( lan8710a_state.regs_cpsw_wr + 0x48))

#define CPSW_FIRST_CHAN_INT	(1 << 0)
#define CPSW_ALL_CHAN_INT	(0xFF << 0)

/* INTERRUPTION CONTROLLER Registers */
#define INTC_BASE_ADR		(0x48200000)
#define INTC_SYSCONFIG		((volatile u32_t *)( lan8710a_state.regs_intc + 0x10))
#define INTC_IDLE		((volatile u32_t *)( lan8710a_state.regs_intc + 0x50))
#define INTC_MIR_CLEAR1		((volatile u32_t *)( lan8710a_state.regs_intc + 0xA8))
#define INTC_ILR(x)		((volatile u32_t *)( lan8710a_state.regs_intc + 0x100 + 4*(x)))

#define INTC_AUTOIDLE		(1 << 0)
#define INTC_FUNCIDLE		(1 << 0)
#define INTC_TURBO		(1 << 1)
#define INTC_FIQnIRQ		(1 << 0)
#define INTC_RX_MASK		(1 << 9)
#define INTC_TX_MASK		(1 << 10)

/* DMA STATERAM Registers */
#define CPDMA_STRAM_BASE_ADR	(0x4A100A00)
#define CPDMA_STRAM_TX_HDP(x)	((volatile u32_t *)( lan8710a_state.regs_cpdma_stram + 4*(x)))
#define CPDMA_STRAM_RX_HDP(x)	((volatile u32_t *)( lan8710a_state.regs_cpdma_stram + 0x20 + 4*(x)))
#define CPDMA_STRAM_TX_CP(x)	((volatile u32_t *)( lan8710a_state.regs_cpdma_stram + 0x40 + 4*(x)))
#define CPDMA_STRAM_RX_CP(x)	((volatile u32_t *)( lan8710a_state.regs_cpdma_stram + 0x60 + 4*(x)))

#define ALL_BITS		(0xFFFFFFFF)

/* LAN8710A Registers */
#define PHY_REGISTERS		(31)
#define LAN8710A_CTRL_REG	(0)
#define LAN8710A_STATUS_REG	(1)

#define LAN8710A_SOFT_RESET	(1 << 15)
#define LAN8710A_AUTO_NEG	(1 << 12)
#define LAN8710A_AUTO_NEG_COMPL	(1 << 5)

#endif /* LAN8710A_REG_H_ */
