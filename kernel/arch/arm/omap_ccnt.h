#ifndef _OMAP_CCNT_H
#define _OMAP_CCNT_H

/* ARM ARM B4.1.116 */
#define OMAP_PMCNTENSET_C	(1 << 31)  /* Enable PMCCNTR cycle counter */

/* ARM ARM B4.1.117 PMCR */
#define OMAP_PMCR_DP		(1 << 5) /* Disable when ev. cnt. prohibited */
#define OMAP_PMCR_X		(1 << 4) /* Export enable */
#define OMAP_PMCR_D		(1 << 3) /* Clock divider */
#define OMAP_PMCR_C		(1 << 2) /* Cycle counter reset */
#define OMAP_PMCR_P		(1 << 1) /* Event counter reset */
#define OMAP_PMCR_E		(1 << 0) /* Enable event counters */

/* ARM ARM B4.1.119 PMINTENSET */
#define OMAP_PMINTENSET_C	(1 << 31) /* PMCCNTR overflow int req. enable*/

/* ARM ARM B4.1.124 PMUSERENR */
#define OMAP_PMUSERENR_EN	(1 << 0) /* User mode access enable bit */

#endif /* _OMAP_CCNT_H */
