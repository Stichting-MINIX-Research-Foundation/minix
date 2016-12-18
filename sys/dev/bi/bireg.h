/*	$NetBSD: bireg.h,v 1.10 2005/12/11 12:21:15 christos Exp $	*/
/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)bireg.h	7.3 (Berkeley) 6/28/90
 */

/*
 * VAXBI node definitions.
 */

/*
 * BI node addresses
 */
#define	BI_NODESIZE	0x2000	/* Size of one BI node */
#define	BI_NODE(node)	(BI_NODESIZE * (node))
#define	BI_BASE(bi,nod)	((0x20000000 + (bi) * 0x2000000) + BI_NODE(nod))
#define	MAXNBI		16	/* Spec says there can be 16 anyway */
#define	NNODEBI		16	/* 16 nodes per BI */

#define	BI_PROBE	0x80000	/* CPU on 8200, NBIA on 8800 */
/*
 * BI nodes all start with BI interface registers (those on the BIIC chip).
 * These are followed with interface-specific registers.
 *
 * NB: This structure does NOT include the four GPRs (not anymore!)
 *
 * 990712: The structs not used anymore due to conversion to bus.h.
 */
#ifdef notdef
struct biiregs {
	u_short	bi_dtype;	/* device type */
	u_short	bi_revs;	/* revisions */
	u_long	bi_csr;		/* control and status register */
	u_long	bi_ber;		/* bus error register */
	u_long	bi_eintrcsr;	/* error interrupt control register */
	u_long	bi_intrdes;	/* interrupt destination register */
				/* the rest are not required for all nodes */
	u_long	bi_ipintrmsk;	/* IP interrupt mask register */
	u_long	bi_fipsdes;	/* Force-Bit IPINTR/STOP destination reg */
	u_long	bi_ipintrsrc;	/* IPINTR source register */
	u_long	bi_sadr;	/* starting address register */
	u_long	bi_eadr;	/* ending address register */
	u_long	bi_bcicsr;	/* BCI control and status register */
	u_long	bi_wstat;	/* write status register */
	u_long	bi_fipscmd;	/* Force-Bit IPINTR/STOP command reg */
	u_long	bi_xxx1[3];	/* unused */
	u_long	bi_uintrcsr;	/* user interface interrupt control reg */
	u_long	bi_xxx2[43];	/* unused */
/* although these are on the BIIC, their interpretation varies */
/*	u_long	bi_gpr[4]; */	/* general purpose registers */
};

/*
 * A generic BI node.
 */
struct bi_node {
	struct	biiregs biic;	/* interface */
	u_long	bi_xxx[1988];	/* pad to 8K */
};

/*
 * A CPU node.
 */
struct bi_cpu {
	struct	biiregs biic;	/* interface chip */
	u_long	bi_gpr[4];	/* gprs (unused) */
	u_long	bi_sosr;	/* slave only status register */
	u_long	bi_xxx[63];	/* pad */
	u_long	bi_rxcd;	/* receive console data register */
};
#endif

#define	BIREG_DTYPE		0x00
#define	BIREG_VAXBICSR		0x04
#define	BIREG_BER		0x08
#define	BIREG_EINTRCSR		0x0c
#define	BIREG_INTRDES		0x10
#define	BIREG_IPINTRMSK		0x14
#define	BIREG_FIPSDES		0x18
#define	BIREG_IPINTRSRC		0x1c
#define	BIREG_SADR		0x20
#define	BIREG_EADR		0x24
#define	BIREG_BCICSR		0x28
#define	BIREG_WSTAT		0x2c
#define	BIREG_FIPSCMD		0x30
#define	BIREG_UINTRCSR		0x40

/* device types */
#define	BIDT_MS820	0x0001	/* MS820 memory board */
#define	BIDT_DRB32	0x0101	/* DRB32 (MFA) Supercomputer gateway */
#define	BIDT_DWBUA	0x0102	/* DWBUA Unibus adapter */
#define	BIDT_KLESI	0x0103	/* KLESI-B (DWBLA) adapter */
#define	BIDT_HSB70	0x4104	/* HSB70 */
#define	BIDT_KA820	0x0105	/* KA820 CPU */
#define	BIDT_DB88	0x0106	/* DB88 (NBI) adapter */
#define	BIDT_DWMBA	0x2107	/* XMI-BI (XBI) adapter */
#define	BIDT_DWMBB	0x0107	/* XMI-BI (XBI) adapter */
#define	BIDT_CIBCA	0x0108	/* Computer Interconnect adapter */
#define	BIDT_DMB32	0x0109	/* DMB32 (COMB) adapter */
#define	BIDT_BAA	0x010a	/* BAA */
#define	BIDT_CIBCI	0x010b	/* Computer Interconnect adapter (old) */
#define	BIDT_DEBNT	0x410b	/* (AIE_TK70) Ethernet+TK50/TBK70  */
#define	BIDT_KA800	0x010c	/* KA800 (ACP) slave processor */
#define	BIDT_KFBTA	0x410d	/* RD/RX disk controller */
#define	BIDT_KDB50	0x010e	/* KDB50 (BDA) disk controller */
#define	BIDT_DEBNK	0x410e	/* (AIE_TK) BI Ethernet (Lance) + TK50 */
#define	BIDT_DEBNA	0x410f	/* (AIE) BI Ethernet (Lance) adapter */
#define	BIDT_DEBNI	0x0118	/* (XNA) BI Ethernet adapter */


/* bits in bi_csr */
#define	BICSR_IREV(x)	((u_char)((x) >> 24))	/* VAXBI interface rev */
#define	BICSR_TYPE(x)	((u_char)((x) >> 16))	/* BIIC type */
#define	BICSR_HES	0x8000		/* hard error summary */
#define	BICSR_SES	0x4000		/* soft error summary */
#define	BICSR_INIT	0x2000		/* initialise node */
#define	BICSR_BROKE	0x1000		/* broke */
#define	BICSR_STS	0x0800		/* self test status */
#define	BICSR_NRST	0x0400		/* node reset */
#define	BICSR_UWP	0x0100		/* unlock write pending */
#define	BICSR_HEIE	0x0080		/* hard error interrupt enable */
#define	BICSR_SEIE	0x0040		/* soft error interrupt enable */
#define	BICSR_ARB_MASK	0x0030		/* mask to get arbitration codes */
#define	BICSR_ARB_NONE	0x0030		/* no arbitration */
#define	BICSR_ARB_LOG	0x0020		/* low priority */
#define	BICSR_ARB_HIGH	0x0010		/* high priority */
#define	BICSR_ARB_RR	0x0000		/* round robin */
#define	BICSR_NODEMASK	0x000f		/* node ID */

#define	BICSR_BITS \
"\20\20HES\17SES\16INIT\15BROKE\14STS\13NRST\11UWP\10HEIE\7SEIE"

/* bits in bi_ber */
#define	BIBER_MBZ	0x8000fff0
#define	BIBER_NMR	0x40000000	/* no ack to multi-responder command */
#define	BIBER_MTCE	0x20000000	/* master transmit check error */
#define	BIBER_CTE	0x10000000	/* control transmit error */
#define	BIBER_MPE	0x08000000	/* master parity error */
#define	BIBER_ISE	0x04000000	/* interlock sequence error */
#define	BIBER_TDF	0x02000000	/* transmitter during fault */
#define	BIBER_IVE	0x01000000	/* ident vector error */
#define	BIBER_CPE	0x00800000	/* command parity error */
#define	BIBER_SPE	0x00400000	/* slave parity error */
#define	BIBER_RDS	0x00200000	/* read data substitute */
#define	BIBER_RTO	0x00100000	/* retry timeout */
#define	BIBER_STO	0x00080000	/* stall timeout */
#define	BIBER_BTO	0x00040000	/* bus timeout */
#define	BIBER_NEX	0x00020000	/* nonexistent address */
#define	BIBER_ICE	0x00010000	/* illegal confirmation error */
#define	BIBER_UPEN	0x00000008	/* user parity enable */
#define	BIBER_IPE	0x00000004	/* ID parity error */
#define	BIBER_CRD	0x00000002	/* corrected read data */
#define	BIBER_NPE	0x00000001	/* null bus parity error */
#define	BIBER_HARD	0x4fff0000

#define	BIBER_BITS \
"\20\37NMR\36MTCE\35CTE\34MPE\33ISE\32TDF\31IVE\30CPE\
\27SPE\26RDS\25RTO\24STO\23BTO\22NEX\21ICE\4UPEN\3IPE\2CRD\1NPE"

/* bits in bi_eintrcsr */
#define	BIEIC_INTRAB	0x01000000	/* interrupt abort */
#define	BIEIC_INTRC	0x00800000	/* interrupt complete */
#define	BIEIC_INTRSENT	0x00200000	/* interrupt command sent */
#define	BIEIC_INTRFORCE	0x00100000	/* interrupt force */
#define	BIEIC_LEVELMASK	0x000f0000	/* mask for interrupt levels */
#define	BIEIC_IPL17	0x00080000	/* ipl 0x17 */
#define	BIEIC_IPL16	0x00040000	/* ipl 0x16 */
#define	BIEIC_IPL15	0x00020000	/* ipl 0x15 */
#define	BIEIC_IPL14	0x00010000	/* ipl 0x14 */
#define	BIEIC_VECMASK	0x00003ffc	/* vector mask for error intr */

/* bits in bi_intrdes */
#define	BIDEST_MASK	0x0000ffff	/* one bit per node to be intr'ed */

/* bits in bi_ipintrmsk */
#define	BIIPINTR_MASK	0xffff0000	/* one per node to allow to ipintr */

/* bits in bi_fipsdes */
#define	BIFIPSD_MASK	0x0000ffff

/* bits in bi_ipintrsrc */
#define	BIIPSRC_MASK	0xffff0000

/* sadr and eadr are simple addresses */

/* bits in bi_bcicsr */
#define	BCI_BURSTEN	0x00020000	/* burst mode enable */
#define	BCI_IPSTOP_FRC	0x00010000	/* ipintr/stop force */
#define	BCI_MCASTEN	0x00008000	/* multicast space enable */
#define	BCI_BCASTEN	0x00004000	/* broadcast enable */
#define	BCI_STOPEN	0x00002000	/* stop enable */
#define	BCI_RSRVDEN	0x00001000	/* reserved enable */
#define	BCI_IDENTEN	0x00000800	/* ident enable */
#define	BCI_INVALEN	0x00000400	/* inval enable */
#define	BCI_WINVEN	0x00000200	/* write invalidate enable */
#define	BCI_UINTEN	0x00000100	/* user interface csr space enable */
#define	BCI_BIICEN	0x00000080	/* BIIC csr space enable */
#define	BCI_INTEN	0x00000040	/* interrupt enable */
#define	BCI_IPINTEN	0x00000020	/* ipintr enable */
#define	BCI_PIPEEN	0x00000010	/* pipeline NXT enable */
#define	BCI_RTOEVEN	0x00000008	/* read timeout EV enable */

#define	BCI_BITS \
"\20\22BURSTEN\21IPSTOP_FRC\20MCASTEN\
\17BCASTEN\16STOPEN\15RSRVDEN\14IDENTEN\13INVALEN\12WINVEN\11UINTEN\
\10BIICEN\7INTEN\6IPINTEN\5PIPEEN\4RTOEVEN"

/* bits in bi_wstat */
#define	BIW_GPR3	0x80000000	/* gpr 3 was written */
#define	BIW_GPR2	0x40000000	/* gpr 2 was written */
#define	BIW_GPR1	0x20000000	/* gpr 1 was written */
#define	BIW_GPR0	0x10000000	/* gpr 0 was written */

/* bits in force-bit ipintr/stop command register */
#define	BIFIPSC_CMDMASK	0x0000f000	/* command */
#define	BIFIPSC_MIDEN	0x00000800	/* master ID enable */

/* bits in bi_uintcsr */
#define	BIUI_INTAB	0xf0000000	/* interrupt abort level */
#define	BIUI_INTC	0x0f000000	/* interrupt complete bits */
#define	BIUI_SENT	0x00f00000	/* interrupt sent bits */
#define	BIUI_FORCE	0x000f0000	/* force interrupt level */
#define	BIUI_EVECEN	0x00008000	/* external vector enable */
#define	BIUI_VEC	0x00003ffc	/* interrupt vector */

/* tell if a bi device is a slave (hence has SOSR) */
#define	BIDT_ISSLAVE(x)	(((x) & 0x7f00) == 0)

/* bits in bi_sosr */
#define	BISOSR_MEMSIZE	0x1ffc0000	/* memory size */
#define	BISOSR_BROKE	0x00001000	/* broke */

/* bits in bi_rxcd */
#define	BIRXCD_BUSY2	0x80000000	/* busy 2 */
#define	BIRXCD_NODE2	0x0f000000	/* node id 2 */
#define	BIRXCD_CHAR2	0x00ff0000	/* character 2 */
#define	BIRXCD_BUSY1	0x00008000	/* busy 1 */
#define	BIRXCD_NODE1	0x00000f00	/* node id 1 */
#define	BIRXCD_CHAR1	0x000000ff	/* character 1 */
