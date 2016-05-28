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
#define OMAP3_TIMER_TIDR      0x000 /* IP revision code */
#define OMAP3_TIMER_TIOCP_CFG 0x010 /* Controls params for GP timer L4 interface */
#define OMAP3_TIMER_TISTAT    0x014 /* Status (excl. interrupt status) */
#define OMAP3_TIMER_TISR      0x018 /* Pending interrupt status */
#define OMAP3_TIMER_TIER      0x01C /* Interrupt enable */
#define OMAP3_TIMER_TWER      0x020 /* Wakeup enable */
#define OMAP3_TIMER_TCLR      0x024 /* Controls optional features */
#define OMAP3_TIMER_TCRR      0x028 /* Internal counter value */
#define OMAP3_TIMER_TLDR      0x02C /* Timer load value */
#define OMAP3_TIMER_TTGR      0x030 /* Triggers counter reload */
#define OMAP3_TIMER_TWPS      0x034 /* Indicates if Write-Posted pending */
#define OMAP3_TIMER_TMAR      0x038 /* Value to be compared with counter */
#define OMAP3_TIMER_TCAR1     0x03C /* First captured value of counter register */
#define OMAP3_TIMER_TSICR     0x040 /* Control posted mode and functional SW reset */
#define OMAP3_TIMER_TCAR2     0x044 /* Second captured value of counter register */
#define OMAP3_TIMER_TPIR      0x048 /* Positive increment (1 ms tick) */
#define OMAP3_TIMER_TNIR      0x04C /* Negative increment (1 ms tick) */
#define OMAP3_TIMER_TCVR      0x050 /* Defines TCRR is sub/over-period (1 ms tick) */
#define OMAP3_TIMER_TOCR      0x054 /* Masks tick interrupt */
#define OMAP3_TIMER_TOWR      0x058 /* Number of masked overflow interrupts */

#define AM335X_DMTIMER0_BASE      0x44E05000  /* DMTimer0 Registers */
#define AM335X_DMTIMER1_1MS_BASE  0x44E31000 /* DMTimer1 1ms Registers (Accurate 1ms timer) */
#define AM335X_DMTIMER2_BASE      0x48040000 /*  DMTimer2 Registers */
#define AM335X_DMTIMER3_BASE      0x48042000 /*  DMTimer3 Registers */
#define AM335X_DMTIMER4_BASE      0x48044000 /* DMTimer4 Registers  */
#define AM335X_DMTIMER5_BASE      0x48046000 /* DMTimer5 Registers  */
#define AM335X_DMTIMER6_BASE      0x48048000 /*  DMTimer6 Registers */
#define AM335X_DMTIMER7_BASE      0x4804A000 /*  DMTimer7 Registers */

/* General-purpose timer registers  AM335x non 1MS timers have different offsets */
#define AM335X_TIMER_TIDR             0x000 /* IP revision code */
#define AM335X_TIMER_TIOCP_CFG        0x010 /* Controls params for GP timer L4 interface */
#define AM335X_TIMER_IRQSTATUS_RAW    0x024 /* Timer IRQSTATUS Raw Register */
#define AM335X_TIMER_IRQSTATUS        0x028 /* Timer IRQSTATUS Register */
#define AM335X_TIMER_IRQENABLE_SET    0x02C /* Timer IRQENABLE Set Register */
#define AM335X_TIMER_IRQENABLE_CLR    0x030 /* Timer IRQENABLE Clear Register */
#define AM335X_TIMER_IRQWAKEEN        0x034 /* Timer IRQ Wakeup Enable Register */
#define AM335X_TIMER_TCLR      0x038 /* Controls optional features */
#define AM335X_TIMER_TCRR      0x03C /* Internal counter value */
#define AM335X_TIMER_TLDR      0x040 /* Timer load value */
#define AM335X_TIMER_TTGR      0x044 /* Triggers counter reload */
#define AM335X_TIMER_TWPS      0x048 /* Indicates if Write-Posted pending */
#define AM335X_TIMER_TMAR      0x04C /* Value to be compared with counter */
#define AM335X_TIMER_TCAR1     0x050 /* First captured value of counter register */
#define AM335X_TIMER_TSICR     0x054 /* Control posted mode and functional SW reset */
#define AM335X_TIMER_TCAR2     0x058 /* Second captured value of counter register */

#define AM335X_WDT_BASE		0x44E35000	/* watchdog timer */
#define AM335X_WDT_WWPS		0x34		/* command posted status */
#define AM335X_WDT_WSPR		0x48		/* activate/deactivate sequence */

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
#define OMAP3_TCLR_PTV      2
#define OMAP3_TCLR_OVF_TRG  (1 << 10) /* Overflow trigger */


#define OMAP3_CM_CLKSEL_GFX		0x48004b40
#define OMAP3_CM_CLKEN_PLL		0x48004d00
#define OMAP3_CM_FCLKEN1_CORE	0x48004A00
#define OMAP3_CM_CLKSEL_CORE	0x48004A40 /* GPT10 src clock sel. */
#define OMAP3_CM_FCLKEN_PER		0x48005000
#define OMAP3_CM_CLKSEL_PER		0x48005040
#define OMAP3_CM_CLKSEL_WKUP    0x48004c40 /* GPT1 source clock selection */


#define CM_MODULEMODE_MASK (0x3 << 0)
#define CM_MODULEMODE_ENABLE      (0x2 << 0)
#define CM_MODULEMODE_DISABLED     (0x0 << 0)

#define CM_CLKCTRL_IDLEST         (0x3 << 16)
#define CM_CLKCTRL_IDLEST_FUNC    (0x0 << 16)
#define CM_CLKCTRL_IDLEST_TRANS   (0x1 << 16)
#define CM_CLKCTRL_IDLEST_IDLE    (0x2 << 16)
#define CM_CLKCTRL_IDLEST_DISABLE (0x3 << 16)

#define CM_WKUP_BASE 0x44E00400 /* Clock Module Wakeup Registers */

#define CM_WKUP_TIMER1_CLKCTRL	(CM_WKUP_BASE + 0xC4) /* This register manages the TIMER1 clocks. [Memory Mapped] */


#define CM_PER_BASE 0x44E00000 /* Clock Module Peripheral Registers */
#define CM_PER_TIMER7_CLKCTRL	(CM_PER_BASE + 0x7C) /* This register manages the TIMER7 clocks. [Memory Mapped] */



/* CM_DPLL registers */


#define CM_DPLL_BASE 	0x44E00500 /* Clock Module PLL Registers */

#define CLKSEL_TIMER1MS_CLK (CM_DPLL_BASE + 0x28)


#define CLKSEL_TIMER1MS_CLK_SEL_MASK (0x7 << 0)
#define CLKSEL_TIMER1MS_CLK_SEL_SEL1 (0x0 << 0) /* Select CLK_M_OSC clock */
#define CLKSEL_TIMER1MS_CLK_SEL_SEL2 (0x1 << 0) /* Select CLK_32KHZ clock */
#define CLKSEL_TIMER1MS_CLK_SEL_SEL3 (0x2 << 0) /* Select TCLKIN clock */
#define CLKSEL_TIMER1MS_CLK_SEL_SEL4 (0x3 << 0) /* Select CLK_RC32K clock */
#define CLKSEL_TIMER1MS_CLK_SEL_SEL5 (0x4 << 0) /* Selects the CLK_32768 from 32KHz Crystal Osc */

#define CLKSEL_TIMER7_CLK   (CM_DPLL_BASE + 0x04)
#define CLKSEL_TIMER7_CLK_SEL_MASK (0x3 << 0)
#define CLKSEL_TIMER7_CLK_SEL_SEL1 (0x0 << 0) /* Select TCLKIN clock */
#define CLKSEL_TIMER7_CLK_SEL_SEL2 (0x1 << 0) /* Select CLK_M_OSC clock */
#define CLKSEL_TIMER7_CLK_SEL_SEL3 (0x2 << 0) /* Select CLK_32KHZ clock */
#define CLKSEL_TIMER7_CLK_SEL_SEL4 (0x3 << 0) /* Reserved */




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
