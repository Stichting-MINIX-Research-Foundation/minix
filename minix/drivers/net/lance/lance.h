
/* macros for 'mode' */
#define EC_DISABLED    0x0
#define EC_SINK        0x1
#define EC_ENABLED     0x2

/* macros for 'flags' */
#define ECF_EMPTY       0x000
#define ECF_PACK_SEND   0x001
#define ECF_PACK_RECV   0x002
#define ECF_SEND_AVAIL  0x004
#define ECF_READING     0x010
#define ECF_PROMISC     0x040
#define ECF_MULTI       0x080
#define ECF_BROAD       0x100
#define ECF_ENABLED     0x200
#define ECF_STOPPED     0x400

/* === macros for ether cards (our generalized version) === */
#define EC_ISR_RINT     0x0001
#define EC_ISR_WINT     0x0002
#define EC_ISR_RERR     0x0010
#define EC_ISR_WERR     0x0020
#define EC_ISR_ERR      0x0040
#define EC_ISR_RST      0x0100

/* IOVEC */
#define IOVEC_NR        16
typedef struct iovec_dat
{
  iovec_s_t iod_iovec[IOVEC_NR];
  int iod_iovec_s;
  endpoint_t iod_proc_nr;
  cp_grant_id_t iod_grant;
  vir_bytes iod_iovec_offset;
} iovec_dat_t;

/* ====== ethernet card info. ====== */
typedef struct ether_card
{
  /* ####### MINIX style ####### */
  char port_name[sizeof("lance#n")];
  int flags;
  int mode;
  int transfer_mode;
  eth_stat_t eth_stat;
  iovec_dat_t read_iovec;
  iovec_dat_t write_iovec;
  iovec_dat_t tmp_iovec;
  vir_bytes write_s;
  vir_bytes read_s;
  int client;
  message sendmsg;

  /* ######## device info. ####### */
  port_t ec_port;
  phys_bytes ec_linmem;
  int ec_irq;
  int ec_int_pending;
  int ec_hook;

  int ec_ramsize;
 
  /* Addrassing */
  u16_t ec_memseg;
  vir_bytes ec_memoff;
  
  ether_addr_t mac_address;
} ether_card_t;

#define DEI_DEFAULT    0x8000

/*
 * NOTE: Not all the CSRs are defined. Just the ones that were deemed
 * necessary or potentially useful.
 */

/* Control and Status Register Addresses */
#define LANCE_CSR0   0   /* Controller Status Register */
#define LANCE_CSR1   1   /* Initialization Block Address (Lower) */
#define LANCE_CSR2   2   /* Initialization Block Address (Upper) */
#define LANCE_CSR3   3   /* Interrupt Masks and Deferral Control */
#define LANCE_CSR4   4   /* Test and Features Control */
#define LANCE_CSR5   5   /* Extended Control and Interrupt */
#define LANCE_CSR8   8   /* Logical Address Filter 0 */
#define LANCE_CSR9   9   /* Logical Address Filter 1 */
#define LANCE_CSR10 10   /* Logical Address Filter 2 */
#define LANCE_CSR11 11   /* Logical Address Filter 3 */
#define LANCE_CSR15 15   /* Mode */
#define LANCE_CSR88 88   /* Chip ID Register (Lower) */
#define LANCE_CSR89 89   /* Chip ID Register (Upper) */

/* Control and Status Register 0 (CSR0) */
#define LANCE_CSR0_ERR       0x8000 /* Error Occurred */
#define LANCE_CSR0_BABL      0x4000 /* Transmitter Timeout Error */
#define LANCE_CSR0_CERR      0x2000 /* Collision Error */
#define LANCE_CSR0_MISS      0x1000 /* Missed Frame */
#define LANCE_CSR0_MERR      0x0800 /* Memory Error */
#define LANCE_CSR0_RINT      0x0400 /* Receive Interrupt */
#define LANCE_CSR0_TINT      0x0200 /* Transmit Interrupt */
#define LANCE_CSR0_IDON      0x0100 /* Initialization Done */
#define LANCE_CSR0_INTR      0x0080 /* Interrupt Flag */
#define LANCE_CSR0_IENA      0x0040 /* Interrupt Enable */
#define LANCE_CSR0_RXON      0x0020 /* Receive On */
#define LANCE_CSR0_TXON      0x0010 /* Transmit On */
#define LANCE_CSR0_TDMD      0x0008 /* Transmit Demand */
#define LANCE_CSR0_STOP      0x0004 /* Stop */
#define LANCE_CSR0_STRT      0x0002 /* Start */
#define LANCE_CSR0_INIT      0x0001 /* Init */

/* Control and Status Register 3 (CSR3) */
/*                           0x8000    Reserved */
#define LANCE_CSR3_BABLM     0x4000 /* Babble Mask */
/*                           0x2000    Reserved */
#define LANCE_CSR3_MISSM     0x1000 /* Missed Frame Mask */
#define LANCE_CSR3_MERRM     0x0800 /* Memory Error Mask */
#define LANCE_CSR3_RINTM     0x0400 /* Receive Interrupt Mask */
#define LANCE_CSR3_TINTM     0x0200 /* Transmit Interrupt Mask */
#define LANCE_CSR3_IDONM     0x0100 /* Initialization Done Mask */
/*                           0x0080    Reserved */
#define LANCE_CSR3_DXSUFLO   0x0040 /* Disable Transmit Stop on Underflow */
#define LANCE_CSR3_LAPPEN    0x0020 /* Look Ahead Packet Processing Enable */
#define LANCE_CSR3_DXMT2PD   0x0010 /* Disable Transmit Two Part Deferral */
#define LANCE_CSR3_EMBA      0x0008 /* Enable Modified Back-off Algorithm */
#define LANCE_CSR3_BSWP      0x0004 /* Byte Swap */
/*                           0x0002    Reserved
 *                           0x0001    Reserved */

/* Control and Status Register 4 (CSR4) */
#define LANCE_CSR4_EN124     0x8000 /* Enable CSR124 Access */
#define LANCE_CSR4_DMAPLUS   0x4000 /* Disable DMA Burst Transfer Counter */
#define LANCE_CSR4_TIMER     0x2000 /* Enable Bus Activity Timer */
#define LANCE_CSR4_DPOLL     0x1000 /* Disable Transmit Polling */
#define LANCE_CSR4_APAD_XMT  0x0800 /* Auto Pad Transmit */
#define LANCE_CSR4_ASTRP_RCV 0x0400 /* Auto Strip Receive */
#define LANCE_CSR4_MFCO      0x0200 /* Missed Frame Counter Overflow */
#define LANCE_CSR4_MFCOM     0x0100 /* Missed Frame Counter Overflow Mask */
#define LANCE_CSR4_UINTCMD   0x0080 /* User Interrupt Command */
#define LANCE_CSR4_UINT      0x0040 /* User Interrupt */
#define LANCE_CSR4_RCVCCO    0x0020 /* Receive Collision Counter Overflow */
#define LANCE_CSR4_RCVCCOM   0x0010 /* Receive Collision Counter Overflow
                                     * Mask */
#define LANCE_CSR4_TXSTRT    0x0008 /* Transmit Start */
#define LANCE_CSR4_TXSTRTM   0x0004 /* Transmit Start Mask */
#define LANCE_CSR4_JAB       0x0002 /* Jabber Error */
#define LANCE_CSR4_JABM      0x0001 /* Jabber Error Mask */

/* Control and Status Register 5 (CSR5) */
#define LANCE_CSR5_TOKINTD   0x8000 /* Transmit OK Interrupt Disable */
#define LANCE_CSR5_LINTEN    0x4000 /* Last Transmit Interrupt Enable */
/*                           0x2000    Reserved
 *                           0x1000    Reserved */
#define LANCE_CSR5_SINT      0x0800 /* System Interrupt */
#define LANCE_CSR5_SINTE     0x0400 /* System Interrupt Enable */
#define LANCE_CSR5_SLPINT    0x0200 /* Sleep Interrupt */
#define LANCE_CSR5_SLPINTE   0x0100 /* Sleep Interrupt Enable */
#define LANCE_CSR5_EXDINT    0x0080 /* Excessive Deferral Interrupt */
#define LANCE_CSR5_EXDINTE   0x0040 /* Excessive Deferral Interrupt Enable */
#define LANCE_CSR5_MPPLBA    0x0020 /* Magic Packet Physical Logical Broadcast
                                     * Accept */
#define LANCE_CSR5_MPINT     0x0010 /* Magic Packet Interrupt */
#define LANCE_CSR5_MPINTE    0x0008 /* Magic Packet Interrupt Enable */
#define LANCE_CSR5_MPEN      0x0004 /* Magic Packet Enable */
#define LANCE_CSR5_MPMODE    0x0002 /* Magic Packet Mode */
#define LANCE_CSR5_SPND      0x0001 /* Suspend */

/* Control and Status Register 15 (CSR15) */
#define LANCE_CSR15_PROM     0x8000 /* Promiscuous Mode */
#define LANCE_CSR15_DRCVBC   0x4000 /* Disable Receive Broadcast */
#define LANCE_CSR15_DRCVPA   0x2000 /* Disable Receive Physical Address */
#define LANCE_CSR15_DLNKTST  0x1000 /* Disable Link Status */
#define LANCE_CSR15_DAPC     0x0800 /* Disable Automatic Polarity Correction */
#define LANCE_CSR15_MENDECL  0x0400 /* MENDEC Loopback Mode */
#define LANCE_CSR15_LRT      0x0200 /* Low Receive Threshold (T-MAU Mode) */
#define LANCE_CSR15_TSEL     0x0200 /* Transmit Mode Select  (AUI Mode) */
/*                           0x0100    Portsel[1]
 *                           0x0080    Portsel[0] */
#define LANCE_CSR15_INTL     0x0040 /* Internal Loopback */
#define LANCE_CSR15_DRTY     0x0020 /* Disable Retry */
#define LANCE_CSR15_FCOLL    0x0010 /* Force Collision */
#define LANCE_CSR15_DXMTFCS  0x0008 /* Disable Transmit CRC (FCS) */
#define LANCE_CSR15_LOOP     0x0004 /* Loopback Enable */
#define LANCE_CSR15_DTX      0x0002 /* Disable Transmit */
#define LANCE_CSR15_DRX      0x0001 /* Disable Receiver */
