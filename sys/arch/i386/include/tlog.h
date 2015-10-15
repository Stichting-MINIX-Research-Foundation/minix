/*	$NetBSD: tlog.h,v 1.4 2004/02/13 11:36:14 wiz Exp $	*/

/*
 * Trap log.  Per-CPU ring buffer containing a log of the last 2**N
 * traps.
 */

struct trec
{
	uint32_t	tr_sp;	/* stack pointer */
	uint32_t	tr_hpc;	/* handler pc */
	uint32_t	tr_ipc;	/* interrupted pc */
	uint32_t	tr_tsc;	/* timestamp counter */
	uint32_t	tr_lbf;	/* MSR_LAST{BRANCH,INT}{FROM,TO}IP */
	uint32_t	tr_lbt;
	uint32_t	tr_ibf;
	uint32_t	tr_ibt;
};

struct tlog
{
	struct trec	tl_recs[128];
};



