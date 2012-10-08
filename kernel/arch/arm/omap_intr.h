#ifndef _OMAP_INTR_H
#define _OMAP_INTR_H

/* Interrupt controller memory map */
#define OMAP3_INTR_BASE 0x48200000 /* INTCPS physical address */

/* Interrupt controller registers */
#define OMAP3_INTCPS_REVISION     0x000 /* IP revision code */
#define OMAP3_INTCPS_SYSCONFIG    0x010 /* Controls params */
#define OMAP3_INTCPS_SYSSTATUS    0x014 /* Status */
#define OMAP3_INTCPS_SIR_IRQ      0x040 /* Active IRQ number */
#define OMAP3_INTCPS_SIR_FIQ      0x044 /* Active FIQ number */
#define OMAP3_INTCPS_CONTROL      0x048 /* New int agreement bits */
#define OMAP3_INTCPS_PROTECTION   0x04C /* Protection for other regs */
#define OMAP3_INTCPS_IDLE         0x050 /* Clock auto-idle/gating */
#define OMAP3_INTCPS_IRQ_PRIORITY 0x060 /* Active IRQ priority level */
#define OMAP3_INTCPS_FIQ_PRIORITY 0x064 /* Active FIQ priority level */
#define OMAP3_INTCPS_THRESHOLD    0x068 /* Priority threshold */
#define OMAP3_INTCPS_ITR0         0x080 /* Raw pre-masking interrupt status */
#define OMAP3_INTCPS_MIR0         0x084 /* Interrupt mask */
#define OMAP3_INTCPS_MIR_CLEAR0   0x088 /* Clear interrupt mask bits */
#define OMAP3_INTCPS_MIR_SET0     0x08C /* Set interrupt mask bits */
#define OMAP3_INTCPS_ISR_SET0     0x090 /* Set software int bits */
#define OMAP3_INTCPS_ISR_CLEAR0   0x094 /* Clear software int bits */
#define OMAP3_INTCPS_PENDING_IRQ0 0x098 /* IRQ status post-masking */
#define OMAP3_INTCPS_PENDING_FIQ0 0x09C /* FIQ status post-masking */
#define OMAP3_INTCPS_ILR0         0x100 /* Priority for interrupts */


#define OMAP3_INTR_REVISION     (OMAP3_INTR_BASE + OMAP3_INTCPS_REVISION)
#define OMAP3_INTR_SYSCONFIG    (OMAP3_INTR_BASE + OMAP3_INTCPS_SYSCONFIG)
#define OMAP3_INTR_SYSSTATUS    (OMAP3_INTR_BASE + OMAP3_INTCPS_SYSSTATUS)
#define OMAP3_INTR_SIR_IRQ      (OMAP3_INTR_BASE + OMAP3_INTCPS_SIR_IRQ)
#define OMAP3_INTR_SIR_FIQ      (OMAP3_INTR_BASE + OMAP3_INTCPS_SIR_FIQ)
#define OMAP3_INTR_CONTROL      (OMAP3_INTR_BASE + OMAP3_INTCPS_CONTROL)
#define OMAP3_INTR_PROTECTION   (OMAP3_INTR_BASE + OMAP3_INTCPS_PROTECTION)
#define OMAP3_INTR_IDLE         (OMAP3_INTR_BASE + OMAP3_INTCPS_IDLE)
#define OMAP3_INTR_IRQ_PRIORITY (OMAP3_INTR_BASE + OMAP3_INTCPS_IRQ_PRIORITY)
#define OMAP3_INTR_FIQ_PRIORITY (OMAP3_INTR_BASE + OMAP3_INTCPS_FIQ_PRIORITY)
#define OMAP3_INTR_THRESHOLD    (OMAP3_INTR_BASE + OMAP3_INTCPS_THRESHOLD)

#define OMAP3_INTR_ITR(n) \
    (OMAP3_INTR_BASE + OMAP3_INTCPS_ITR0 + 0x20 * (n))
#define OMAP3_INTR_MIR(n) \
    (OMAP3_INTR_BASE + OMAP3_INTCPS_MIR0 + 0x20 * (n))
#define OMAP3_INTR_MIR_CLEAR(n)	\
    (OMAP3_INTR_BASE + OMAP3_INTCPS_MIR_CLEAR0 + 0x20 * (n))
#define OMAP3_INTR_MIR_SET(n) \
    (OMAP3_INTR_BASE + OMAP3_INTCPS_MIR_SET0 + 0x20 * (n))
#define OMAP3_INTR_ISR_SET(n) \
    (OMAP3_INTR_BASE + OMAP3_INTCPS_ISR_SET0 + 0x20 * (n))
#define OMAP3_INTR_ISR_CLEAR(n) \
    (OMAP3_INTR_BASE + OMAP3_INTCPS_ISR_CLEAR0 + 0x20 * (n))
#define OMAP3_INTR_PENDING_IRQ(n) \
    (OMAP3_INTR_BASE + OMAP3_INTCPS_PENDING_IRQ0 + 0x20 * (n))
#define OMAP3_INTR_PENDING_FIQ(n) \
    (OMAP3_INTR_BASE + OMAP3_INTCPS_PENDING_FIQ0 + 0x20 * (n))
#define OMAP3_INTR_ILR(m) \
    (OMAP3_INTR_BASE + OMAP3_INTCPS_ILR0 + 0x4 * (m))

#define OMAP3_INTR_ACTIVEIRQ_MASK 0x7f /* Active IRQ mask for SIR_IRQ */
#define OMAP3_INTR_NEWIRQAGR      0x1  /* New IRQ Generation */

#define OMAP3_NR_IRQ_VECTORS    96

/* Interrupt mappings */
#define OMAP3_MCBSP2_ST_IRQ  4  /* Sidestone McBSP2 overflow */
#define OMAP3_MCBSP3_ST_IRQ  5  /* Sidestone McBSP3 overflow */
#define OMAP3_SYS_NIRQ       7  /* External source (active low) */
#define OMAP3_SMX_DBG_IRQ    9  /* L3 interconnect error for debug */
#define OMAP3_SMX_APP_IRQ   10  /* L3 interconnect error for application */
#define OMAP3_PRCM_IRQ      11  /* PRCM module */
#define OMAP3_SDMA0_IRQ     12  /* System DMA request 0 */
#define OMAP3_SDMA1_IRQ     13  /* System DMA request 1 */
#define OMAP3_SDMA2_IRQ     14  /* System DMA request 2 */
#define OMAP3_SDMA3_IRQ     15  /* System DMA request 3 */
#define OMAP3_MCBSP1_IRQ    16  /* McBSP module 1 */
#define OMAP3_MCBSP2_IRQ    17  /* McBSP module 2 */
#define OMAP3_GPMC_IRQ      20  /* General-purpose memory controller */
#define OMAP3_SGX_IRQ       21  /* 2D/3D graphics module */
#define OMAP3_MCBSP3_IRQ    22  /* McBSP module 3 */
#define OMAP3_MCBSP4_IRQ    23  /* McBSP module 4 */
#define OMAP3_CAM0_IRQ      24  /* Camera interface request 0 */
#define OMAP3_DSS_IRQ       25  /* Display subsystem module */
#define OMAP3_MAIL_U0_IRQ   26  /* Mailbox user 0 request */
#define OMAP3_MCBSP5_IRQ    27  /* McBSP module 5 */
#define OMAP3_IVA2_MMU_IRQ  28  /* IVA2 MMU */
#define OMAP3_GPIO1_IRQ     29  /* GPIO module 1 */
#define OMAP3_GPIO2_IRQ     30  /* GPIO module 2 */
#define OMAP3_GPIO3_IRQ     31  /* GPIO module 3 */
#define OMAP3_GPIO4_IRQ     32  /* GPIO module 4 */
#define OMAP3_GPIO5_IRQ     33  /* GPIO module 5 */
#define OMAP3_GPIO6_IRQ     34  /* GPIO module 6 */
#define OMAP3_WDT3_IRQ      36  /* Watchdog timer module 3 overflow */
#define OMAP3_GPT1_IRQ      37  /* General-purpose timer module 1 */
#define OMAP3_GPT2_IRQ      38  /* General-purpose timer module 2 */
#define OMAP3_GPT3_IRQ      39  /* General-purpose timer module 3 */
#define OMAP3_GPT4_IRQ      40  /* General-purpose timer module 4 */
#define OMAP3_GPT5_IRQ      41  /* General-purpose timer module 5 */
#define OMAP3_GPT6_IRQ      42  /* General-purpose timer module 6 */
#define OMAP3_GPT7_IRQ      43  /* General-purpose timer module 7 */
#define OMAP3_GPT8_IRQ      44  /* General-purpose timer module 8 */
#define OMAP3_GPT9_IRQ      45  /* General-purpose timer module 9 */
#define OMAP3_GPT10_IRQ     46  /* General-purpose timer module 10 */
#define OMAP3_GPT11_IRQ     47  /* General-purpose timer module 11 */
#define OMAP3_SPI4_IRQ      48  /* McSPI module 4 */
#define OMAP3_MCBSP4_TX_IRQ 54  /* McBSP module 4 transmit */
#define OMAP3_MCBSP4_RX_IRQ 55  /* McBSP module 4 receive */
#define OMAP3_I2C1_IRQ      56  /* I2C module 1 */
#define OMAP3_I2C2_IRQ      57  /* I2C module 2 */
#define OMAP3_HDQ_IRQ       58  /* HDQ/1-Wire */
#define OMAP3_MCBSP1_TX_IRQ 59  /* McBSP module 1 transmit */
#define OMAP3_MCBSP1_RX_IRQ 60  /* McBSP module 1 receive */
#define OMAP3_I2C3_IRQ      61  /* I2C module 3 */
#define OMAP3_MCBSP2_TX_IRQ 62  /* McBSP module 2 transmit */
#define OMAP3_MCBSP2_RX_IRQ 63  /* McBSP module 2 receive */
#define OMAP3_SPI1_IRQ      65  /* McSPI module 1 */
#define OMAP3_SPI2_IRQ      66  /* McSPI module 2 */
#define OMAP3_UART1_IRQ     72  /* UART module 1 */
#define OMAP3_UART2_IRQ     73  /* UART module 2 */
#define OMAP3_PBIAS_IRQ     75  /* Merged interrupt for PBIASlite 1/2 */
#define OMAP3_OHCI_IRQ      76  /* OHCI HSUSB MP Host Interrupt */
#define OMAP3_EHCI_IRQ      77  /* EHCI HSUSB MP Host Interrupt */
#define OMAP3_TLL_IRQ       78  /* HSUSB MP TLL Interrupt */
#define OMAP3_MCBSP5_TX_IRQ 81  /* McBSP module 5 transmit */
#define OMAP3_MCBSP5_RX_IRQ 82  /* McBSP module 5 receive */
#define OMAP3_MMC1_IRQ      83  /* MMC/SD module 1 */
#define OMAP3_MMC2_IRQ      86  /* MMC/SD module 2 */
#define OMAP3_ICR_IRQ       87  /* MPU ICR */
#define OMAP3_D2DFRINT_IRQ  88  /* 3G coproc (in stacked modem config) */
#define OMAP3_MCBSP3_TX_IRQ 89  /* McBSP module 3 transmit */
#define OMAP3_MCBSP3_RX_IRQ 90  /* McBSP module 3 receive */
#define OMAP3_SPI3_IRQ      91  /* McSPI module 3 */
#define OMAP3_HSUSB_MC_IRQ  92  /* High-speed USB OTG */
#define OMAP3_HSUSB_DMA_IRQ 93  /* High-speed USB OTG DMA */
#define OMAP3_MMC3_IRQ      94  /* MMC/SD module 3 */


#ifndef __ASSEMBLY__

void omap3_irq_unmask(int irq);
void omap3_irq_mask(int irq);

#endif /* __ASSEMBLY__ */

#endif /* _OMAP_INTR_H */
