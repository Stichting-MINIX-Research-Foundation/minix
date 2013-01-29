#ifndef _OMAP_TIMER_REGISTERS_H
#define _OMAP_TIMER_REGISTERS_H

/* General-purpose timer register map */
#define OMAP3_GPTIMER1_BASE  0x48318000 /* GPTIMER1 physical address */
#define OMAP3_GPTIMER2_BASE  0x49032000 /* GPTIMER2 physical address */
#define OMAP3_GPTIMER3_BASE  0x49034000 /* GPTIMER3 physical address */
#define OMAP3_GPTIMER4_BASE  0x49036000 /* GPTIMER4 physical address */
#define OMAP3_GPTIMER5_BASE  0x49038000 /* GPTIMER5 physical address */
#define OMAP3_GPTIMER6_BASE  0x4903A000 /* GPTIMER6 physical address */
#define OMAP3_GPTIMER7_BASE  0x4903C000 /* GPTIMER7 physical address */
#define OMAP3_GPTIMER8_BASE  0x4903E000 /* GPTIMER8 physical address */
#define OMAP3_GPTIMER9_BASE  0x49040000 /* GPTIMER9 physical address */
#define OMAP3_GPTIMER10_BASE 0x48086000 /* GPTIMER10 physical address */
#define OMAP3_GPTIMER11_BASE 0x48088000 /* GPTIMER11 physical address */

/* General-purpose timer registers */
#define OMAP3_TIDR      0x000 /* IP revision code */
#define OMAP3_TIOCP_CFG 0x010 /* Controls params for GP timer L4 interface */
#define OMAP3_TISTAT    0x014 /* Status (excl. interrupt status) */
#define OMAP3_TISR      0x018 /* Pending interrupt status */
#define OMAP3_TIER      0x01C /* Interrupt enable */
#define OMAP3_TWER      0x020 /* Wakeup enable */
#define OMAP3_TCLR      0x024 /* Controls optional features */
#define OMAP3_TCRR      0x028 /* Internal counter value */
#define OMAP3_TLDR      0x02C /* Timer load value */
#define OMAP3_TTGR      0x030 /* Triggers counter reload */
#define OMAP3_TWPS      0x034 /* Indicates if Write-Posted pending */
#define OMAP3_TMAR      0x038 /* Value to be compared with counter */
#define OMAP3_TCAR1     0x03C /* First captured value of counter register */
#define OMAP3_TSICR     0x040 /* Control posted mode and functional SW reset */
#define OMAP3_TCAR2     0x044 /* Second captured value of counter register */
#define OMAP3_TPIR      0x048 /* Positive increment (1 ms tick) */
#define OMAP3_TNIR      0x04C /* Negative increment (1 ms tick) */
#define OMAP3_TCVR      0x050 /* Defines TCRR is sub/over-period (1 ms tick) */
#define OMAP3_TOCR      0x054 /* Masks tick interrupt */
#define OMAP3_TOWR      0x058 /* Number of masked overflow interrupts */

/* Interrupt status register fields */
#define OMAP3_TISR_MAT_IT_FLAG  (1 << 0) /* Pending match interrupt status */
#define OMAP3_TISR_OVF_IT_FLAG  (1 << 1) /* Pending overflow interrupt status */
#define OMAP3_TISR_TCAR_IT_FLAG (1 << 2) /* Pending capture interrupt status */

/* Interrupt enable register fields */
#define OMAP3_TIER_MAT_IT_ENA  (1 << 0) /* Enable match interrupt */
#define OMAP3_TIER_OVF_IT_ENA  (1 << 1) /* Enable overflow interrupt */
#define OMAP3_TIER_TCAR_IT_ENA (1 << 2) /* Enable capture interrupt */

/* Timer control fields */
#define OMAP3_TCLR_ST       (1 << 0)  /* Start/stop timer */
#define OMAP3_TCLR_AR       (1 << 1)  /* Autoreload or one-shot mode */
#define OMAP3_TCLR_PRE      (1 << 5)  /* Prescaler on */
#define OMAP3_TCLR_PTV            2
#define OMAP3_TCLR_OVF_TRG  (1 << 10) /* Overflow trigger */

#define OMAP3_GPTIMER1_TIDR      (OMAP3_GPTIMER1_BASE + OMAP3_TIDR)
#define OMAP3_GPTIMER1_TIOCP_CFG (OMAP3_GPTIMER1_BASE + OMAP3_TIOCP_CFG)
#define OMAP3_GPTIMER1_TISTAT    (OMAP3_GPTIMER1_BASE + OMAP3_TISTAT)
#define OMAP3_GPTIMER1_TISR      (OMAP3_GPTIMER1_BASE + OMAP3_TISR)
#define OMAP3_GPTIMER1_TIER      (OMAP3_GPTIMER1_BASE + OMAP3_TIER)
#define OMAP3_GPTIMER1_TWER      (OMAP3_GPTIMER1_BASE + OMAP3_TWER)
#define OMAP3_GPTIMER1_TCLR      (OMAP3_GPTIMER1_BASE + OMAP3_TCLR)
#define OMAP3_GPTIMER1_TCRR      (OMAP3_GPTIMER1_BASE + OMAP3_TCRR)
#define OMAP3_GPTIMER1_TLDR      (OMAP3_GPTIMER1_BASE + OMAP3_TLDR)
#define OMAP3_GPTIMER1_TTGR      (OMAP3_GPTIMER1_BASE + OMAP3_TTGR)
#define OMAP3_GPTIMER1_TWPS      (OMAP3_GPTIMER1_BASE + OMAP3_TWPS)
#define OMAP3_GPTIMER1_TMAR      (OMAP3_GPTIMER1_BASE + OMAP3_TMAR)
#define OMAP3_GPTIMER1_TCAR1     (OMAP3_GPTIMER1_BASE + OMAP3_TCAR1)
#define OMAP3_GPTIMER1_TSICR     (OMAP3_GPTIMER1_BASE + OMAP3_TSICR)
#define OMAP3_GPTIMER1_TCAR2     (OMAP3_GPTIMER1_BASE + OMAP3_TCAR2)
#define OMAP3_GPTIMER1_TPIR      (OMAP3_GPTIMER1_BASE + OMAP3_TPIR)
#define OMAP3_GPTIMER1_TNIR      (OMAP3_GPTIMER1_BASE + OMAP3_TNIR)
#define OMAP3_GPTIMER1_TCVR      (OMAP3_GPTIMER1_BASE + OMAP3_TCVR)
#define OMAP3_GPTIMER1_TOCR      (OMAP3_GPTIMER1_BASE + OMAP3_TOCR)
#define OMAP3_GPTIMER1_TOWR      (OMAP3_GPTIMER1_BASE + OMAP3_TOWR)

#define OMAP3_GPTIMER10_TIDR      (OMAP3_GPTIMER10_BASE + OMAP3_TIDR)
#define OMAP3_GPTIMER10_TIOCP_CFG (OMAP3_GPTIMER10_BASE + OMAP3_TIOCP_CFG)
#define OMAP3_GPTIMER10_TISTAT    (OMAP3_GPTIMER10_BASE + OMAP3_TISTAT)
#define OMAP3_GPTIMER10_TISR      (OMAP3_GPTIMER10_BASE + OMAP3_TISR)
#define OMAP3_GPTIMER10_TIER      (OMAP3_GPTIMER10_BASE + OMAP3_TIER)
#define OMAP3_GPTIMER10_TWER      (OMAP3_GPTIMER10_BASE + OMAP3_TWER)
#define OMAP3_GPTIMER10_TCLR      (OMAP3_GPTIMER10_BASE + OMAP3_TCLR)
#define OMAP3_GPTIMER10_TCRR      (OMAP3_GPTIMER10_BASE + OMAP3_TCRR)
#define OMAP3_GPTIMER10_TLDR      (OMAP3_GPTIMER10_BASE + OMAP3_TLDR)
#define OMAP3_GPTIMER10_TTGR      (OMAP3_GPTIMER10_BASE + OMAP3_TTGR)
#define OMAP3_GPTIMER10_TWPS      (OMAP3_GPTIMER10_BASE + OMAP3_TWPS)
#define OMAP3_GPTIMER10_TMAR      (OMAP3_GPTIMER10_BASE + OMAP3_TMAR)
#define OMAP3_GPTIMER10_TCAR1     (OMAP3_GPTIMER10_BASE + OMAP3_TCAR1)
#define OMAP3_GPTIMER10_TSICR     (OMAP3_GPTIMER10_BASE + OMAP3_TSICR)
#define OMAP3_GPTIMER10_TCAR2     (OMAP3_GPTIMER10_BASE + OMAP3_TCAR2)
#define OMAP3_GPTIMER10_TPIR      (OMAP3_GPTIMER10_BASE + OMAP3_TPIR)
#define OMAP3_GPTIMER10_TNIR      (OMAP3_GPTIMER10_BASE + OMAP3_TNIR)
#define OMAP3_GPTIMER10_TCVR      (OMAP3_GPTIMER10_BASE + OMAP3_TCVR)
#define OMAP3_GPTIMER10_TOCR      (OMAP3_GPTIMER10_BASE + OMAP3_TOCR)
#define OMAP3_GPTIMER10_TOWR      (OMAP3_GPTIMER10_BASE + OMAP3_TOWR)

#define OMAP3_CM_CLKSEL_GFX		0x48004b40
#define OMAP3_CM_CLKEN_PLL		0x48004d00
#define OMAP3_CM_FCLKEN1_CORE		0x48004A00
#define OMAP3_CM_CLKSEL_CORE		0x48004A40 /* GPT10 src clock sel. */
#define OMAP3_CM_FCLKEN_PER		0x48005000
#define OMAP3_CM_CLKSEL_PER		0x48005040
#define OMAP3_CM_CLKSEL_WKUP 0x48004c40 /* GPT1 source clock selection */

#define OMAP3_CLKSEL_GPT1    (1 << 0)   /* Selects GPTIMER 1 source
					 * clock:
					 *
					 *  0: use 32KHz clock
					 *  1: sys clock)
					 */
#define OMAP3_CLKSEL_GPT10    (1 << 6)
#define OMAP3_CLKSEL_GPT11    (1 << 7)


#define TIMER_FREQ  1000    /* clock frequency for OMAP timer (1ms) */
#define TIMER_COUNT(freq) (TIMER_FREQ/(freq)) /* initial value for counter*/

#endif /* _OMAP_TIMER_REGISTERS_H */
