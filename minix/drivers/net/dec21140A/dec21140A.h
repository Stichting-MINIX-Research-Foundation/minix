#ifndef INCL_DEC21041_H_GUARD
#define INCL_DEC21041_H_GUARD 
/*
de.h

Header for the driver of the DEC 21140A ethernet card as emulated 
by VirtualPC 2007

Created: 09/01/2009   Nicolas Tittley (first.last @ gmail DOT com)
*/

#include <sys/null.h>

#if debug == 1
#	define DEBUG(statm) statm
#else
#	define DEBUG(statm)
#endif

#define DE_NB_SEND_DESCR    32
#define DE_SEND_BUF_SIZE    (NDEV_ETH_PACKET_MAX+2)
#define DE_NB_RECV_DESCR    32
#define DE_RECV_BUF_SIZE    (NDEV_ETH_PACKET_MAX+2)

#define DE_MIN_BASE_ADDR    0x0400
#define DE_SROM_EA_OFFSET   20
#define DE_SETUP_FRAME_SIZE 192

typedef struct de_descr {
  u32_t des[4];
} de_descr_t;

typedef struct de_local_descr {
  de_descr_t *descr;
  u8_t *buf1;
  u8_t *buf2;
} de_loc_descr_t;

typedef struct dpeth {
  port_t de_base_port;          /* Base port, for multiple card instance */
  int de_irq;                   /* IRQ line number */
  int de_hook;			/* interrupt hook at kernel */

  int de_type;			/* What kind of hardware */

  /* Space reservation. We will allocate all structures later in the code.
     here we just make sure we have the space we need at compile time */
  u8_t sendrecv_descr_buf[(DE_NB_SEND_DESCR+DE_NB_RECV_DESCR)*
			  sizeof(de_descr_t)];
  u8_t sendrecv_buf[DE_NB_SEND_DESCR*DE_SEND_BUF_SIZE +
		    DE_NB_RECV_DESCR*DE_RECV_BUF_SIZE];
  phys_bytes sendrecv_descr_phys_addr[2];
  de_loc_descr_t descr[2][MAX(DE_NB_RECV_DESCR, DE_NB_SEND_DESCR)];
  int cur_descr[2];

#define DESCR_RECV 0
#define DESCR_TRAN 1
  
  /* Serial ROM */
#define SROM_BITWIDTH 6

  u8_t srom[((1<<SROM_BITWIDTH)-1)*2];    /* Space to read in 
					     all the configuration ROM */
} dpeth_t;


/************/
/* Revisons */
/************/

#define DEC_21140A 0x20
#define DE_TYPE_UNKNOWN 0x0
/* #define CSR_ADDR(x, i) csraddr2(x->de_base_port + i) */
#define CSR_ADDR(x, i) (x->de_base_port + i)

/* CSRs */
#define CSR0 0x00
#define     CSR0_SWR   0x00000001 /* sw reset */
#define     CSR0_BAR   0x00000002 /* bus arbitration */
#define     CSR0_CAL_8 0x00004000 /* cache align 8 long word */
#define     CSR0_TAP   0x00080000 /* trans auto polling */
#define CSR1 0x08 /* transmit poll demand */
#define CSR2 0x10 /* receive poll demand */
#define CSR3 0x18 /* receive list address */
#define CSR4 0x20 /* transmit list address */
#define CSR5 0x28              /* status register */
#define     CSR5_EB  0x03800000 /* error bits */
#define     CSR5_TS  0x00700000 /* Transmit proc state */
#define     CSR5_RS  0x000E0000 /* Receive proc state */
#define     CSR5_NIS 0x00010000 /* Norm Int summ */
#define     CSR5_AIS 0x00008000 /* Abnorm Int sum */
#define     CSR5_FBE 0x00002000 /* Fatal bit error */
#define     CSR5_GTE 0x00000800 /* Gen-purp timer exp */
#define     CSR5_ETI 0x00000400 /* Early Trans int */
#define     CSR5_RWT 0x00000200 /* Recv watchdog timeout */
#define     CSR5_RPS 0x00000100 /* Recv proc stop */
#define     CSR5_RU  0x00000080 /* Recv buf unavail */
#define     CSR5_RI  0x00000040 /* Recv interrupt */
#define     CSR5_UNF 0x00000020 /* Trans underflow */
#define     CSR5_TJT 0x00000008 /* Trans Jabber Timeout */
#define     CSR5_TU  0x00000004 /* Trans buf unavail */
#define     CSR5_TPS 0x00000002 /* Trans proc stopped */
#define     CSR5_TI  0x00000001 /* Trans interrupt */
#define CSR6 0x30 /* Operation mode */
#define     CSR6_SC  0x80000000 /* Special capt effect ena 31 */
#define     CSR6_RA  0x40000000 /* receive all 30 */
#define     CSR6_MBO 0x02000000 /* must be one 25 */
#define     CSR6_SCR 0x01000000 /* Scrambler mode 24 */
#define     CSR6_PCS 0x00800000 /* PCS function 23 */
#define     CSR6_TTM 0x00400000 /* Trans threshold mode 22 */
#define     CSR6_SF  0x00200000 /* store and forward 21 */
#define     CSR6_HBD 0x00080000 /* Heartbeat disable 19 */
#define     CSR6_PS  0x00040000 /* port select 18 */
#define     CSR6_CA  0x00020000 /* Capt effect ena 17 */
#define     CSR6_TR_00 0x00000000 /* Trans thresh 15:14 */
#define     CSR6_TR_01 0x00004000 /* Trans thresh 15:14 */
#define     CSR6_TR_10 0x00008000 /* Trans thresh 15:14 */
#define     CSR6_TR_11 0x0000C000 /* Trans thresh 15:14 */
#define     CSR6_ST  0x00002000 /* start/stop trans 13 */
#define     CSR6_FD  0x00000200 /* Full Duplex 9 */
#define     CSR6_PM  0x00000080 /* Pass all multicast 7 */
#define     CSR6_PR  0x00000040 /* Promisc mode 6 */
#define     CSR6_IF  0x00000010 /* Inv filtering 4 */
#define     CSR6_HO  0x00000004 /* Hash-only filtering 2 */
#define     CSR6_SR  0x00000002 /* start/stop recv 1 */
#define     CSR6_HP  0x00000001 /* Hash/perfect recv filt mode 0 */ 
#define CSR7 0x38 /* Interrupt enable */
#define     CSR7_NI  0x00010000 /* Normal interrupt ena */  
#define     CSR7_AI  0x00008000 /* Abnormal int ena */
#define     CSR7_TI  0x00000001 /* trans int ena */  
#define     CSR7_TU  0x00000004 /* trans buf unavail ena */  
#define     CSR7_RI  0x00000040 /* recv interp ena */  
#define     CSR7_GPT 0x00000800 /* gen purpose timer ena */  
#define CSR9 0x48 /* Boot Rom, serial ROM, MII */
#define     CSR9_SR  0x0800 /* serial ROM select */
#define     CSR9_RD  0x4000 /* read */
#define     CSR9_DO  0x0008 /* data out */
#define     CSR9_DI  0x0004 /* data in */
#define     CSR9_SRC 0x0002 /* serial clock */
#define     CSR9_CS  0x0001 /* serial rom chip select */
/* Send/Recv Descriptors */

#define DES0 0
#define  DES0_OWN 0x80000000 /* descr ownership. 1=211140A */
#define  DES0_FL  0x3FFF0000 /* frame length */
#define   DES0_FL_SHIFT 16   /* shift to fix frame length */
#define   DES0_ES 0x00008000 /* Error sum */
#define   DES0_TO 0x00004000 /* Trans jabber timeout */
#define   DES0_LO 0x00000800 /* Loss of carrier */
#define   DES0_NC 0x00000400 /* no carrier */
#define   DES0_LC 0x00000200 /* Late coll */ 
#define   DES0_EC 0x00000100 /* Excessive coll */
#define   DES0_UF 0x00000002 /* Underflow error */ 
#define   DES0_RE 0x00000008 /* MII error */
#define   DES0_FS 0x00000200 /* first descr */
#define   DES0_LS 0x00000100 /* last descr */
#define DES1 1
#define  DES1_ER  0x02000000 /* end of ring */
#define  DES1_SAC 0x01000000 /* 2nd address chained */
#define  DES1_BS2 0x003FF800 /* 2nd buffer size */
#define  DES1_BS2_SHFT 11    /* shift to obtain 2nd buffer size */
#define  DES1_BS1 0x000007FF /* 1nd buffer size */
#define  DES1_IC  0x80000000 /* Interrupt on completion 31 */
#define  DES1_LS  0x40000000 /* Last Segment 30 */
#define  DES1_FS  0x20000000 /* First Segment 29 */
#define  DES1_FT1 0x10000000 /* Filtering type 28 */
#define  DES1_SET 0x08000000 /* Setup frame 27 */
#define  DES1_AC  0x04000000 /* Add CRC disable 26 */
#define  DES1_DPD 0x00800000 /* Disabled padding 23 */
#define  DES1_FT0 0x00400000 /* Filtering type 22 */
#define DES2 2 /* 1st buffer addr */
#define DES3 3 /* 2nd buffer addr */

#define DES_BUF1 DES2
#define DES_BUF2 DES3

#endif /* Include Guard */
