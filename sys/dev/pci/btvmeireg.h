/* $NetBSD: btvmeireg.h,v 1.5 2005/12/11 12:22:48 christos Exp $ */

#ifndef _bit3reg_h_
#define _bit3reg_h_

#ifdef _KERNEL

#define BIT3_LSR_BITS "\20\1CABLE\2LRCERR\3ITO\6INTPR\7RBERR\10PARERR"

/* following is from:
 **      Filename:   btpciio.h
 **
 **      Purpose:    Bit 3 400-809 PCI Applications Toolkit
 **                  Adaptor Node Register Include File.
 */

/******************************************************************************
**
**      Mapping Register Defines
**
******************************************************************************/

#define MR_PCI_VME           0x0        /* PCI to VME Map RAM base offset       */
#define MR_PCI_VME_SIZE      0x8000     /* PCI to VME Map RAM size (bytes)      */

#define MR_VME_PCI           0x8000     /* VME to PCI Map RAM base offset       */
#define MR_VME_PCI_SIZE      0x4000     /* VME to PCI Map RAM size (bytes)      */

#define MR_DMA_PCI           0xC000     /* DMA to PCI Map RAM base offset       */
#define MR_DMA_PCI_SIZE      0x4000     /* DMA to PCI Map RAM size (bytes)      */

#define MR_BYTE_SWAP         (1<<3)     /* r/w, Byte swap bytes                 */
#define MR_WORD_SWAP         (1<<2)     /* r/w, Swap words                      */
#define MR_WBYTE_SWAP        (1<<1)     /* r/w, Byte Swap non-bytes             */
#define MR_RAM_INVALID       (1<<0)     /* r/w, Map register is invalid         */

#define MR_ADDR_MASK       0xFFFFF000UL /* Mask for map address bits A31-A12    */
#define MR_REM_BUS_MASK    0x00000FFFUL /* Mask for remote address bits A11-A0  */

#define MR_AMOD_MASK       0x00000FC0UL /* Mask for address modifier bits       */
                                        /* AM5-AM0 for register bits A11-A6     */
#define MR_AMOD_SHIFT        6          /* Shift value for AMOD bits AM5-AM0    */

#define MR_FC_MASK           0x30       /* Mask for Mapping RAM Function Codes  */
#define MR_FC_RBIO           0x10       /* Remote Bus I/O Mapping function code */
#define MR_FC_RRAM           0x20       /* Remote RAM Mapping function code     */
#define MR_FC_DPRAM          0x30       /* Dual Port RAM Mapping function code  */



/******************************************************************************
**
**      Local Adaptor Node Registers
**
*******************************************************************************
**
**      Local Command Register (Read/Write, 8 Bits)
**
*******************************************************************************
**    7    |   6    |    5   |   4    |    3   |    2   |    1    |    0
** +C_STAT |+CLR_PR |+SND_PT |        |        |        |         |
******************************************************************************/

#define LOC_CMD1             (0x00)                /* Local Command Register */

#define LC1_CLR_ERROR        (1<<7)     /* w,   Clear status error bits (1 Shot) */
#define LC1_CLR_PR_INT       (1<<6)     /* w,   Clear PR (PS) Interrupt (1 Shot)  */
#define LC1_SND_PT_INT       (1<<5)     /* r/w, Set PT Interrupt                  */

/******************************************************************************
**
**       Local Interrupt Control Register (Read/Write, 8 Bits)
**
*******************************************************************************
**     7    |    6    |    5    |   4   |   3   |    2   |    1   |    0
** +INT_ACT | +INT_EN |+ERR_INT |   0   |   0   | +CINT2 | +CINT1 | +CINT0
******************************************************************************/

#define LOC_INT_CTRL         (0x01)      /* Local Interrupt Control Register */

#define LIC_INT_PENDING      (1<<7)     /* r  , Adaptor asserting INT on PCI bus */
#define LIC_INT_ENABLE       (1<<6)     /* r/w, Normal (PR & CINT) INT Enable */
#define LIC_ERR_INT_ENABLE   (1<<5)     /* r/w, Error Interrupt Enable         */

#define LIC_PT_CINT_SEL2     (1<<2)     /* r/w, PT CINT Line Selection Bit 2  */
#define LIC_PT_CINT_SEL1     (1<<1)     /* r/w, PT CINT Line Selection Bit 1  */
#define LIC_PT_CINT_SEL0     (1<<0)     /* r/w, PT CINT Line Selection Bit 0  */


/******************************************************************************
**
**       Local Status Register (Read Only, 8 Bits)
**
*******************************************************************************
**    7    |   6    |    5   |    4    |   3   |    2   |    1   |    0
** +PARITY |+REMBUS |+PR_INT |+CARD_RDY|   0   |+TIMEOUT|+LRC_ERR|+NOCONNECT
******************************************************************************/

#define LOC_STATUS           (0x02)                 /* Local Status Register  */

#define LSR_PARITY_ERR       (1<<7)     /* Interface Parity Error PCI->REM.   */
#define LSR_REMBUS_ERR       (1<<6)     /* BERR from VME on PCI->REM. xfer    */
#define LSR_PR_STATUS        (1<<5)     /* PR interrupt received from REMOTE  */
#define LSR_TIMEOUT_ERR      (1<<2)     /* Interface Timeout error PCI->REM   */
#define LSR_LRC_ERR          (1<<1)     /* LRC error (DMA master only)        */
#define LSR_NO_CONNECT       (1<<0)     /* REM. bus power or I/O cable is off */

#define LSR_ERROR_MASK  (LSR_PARITY_ERR|LSR_REMBUS_ERR|LSR_TIMEOUT_ERR|LSR_LRC_ERR)
#define LSR_CERROR_MASK (LSR_NO_CONNECT|LSR_ERROR_MASK)


/******************************************************************************
**
**       Local Interrupt Status Register (Read Only)
**
*******************************************************************************
**    7   |   6    |    5   |    4   |    3   |    2   |    1   |    0
** +CINT7 | +CINT6 | +CINT5 | +CINT4 | +CINT3 | +CINT2 | +CINT1 | +CINT0
******************************************************************************/

#define LOC_INT_STATUS       (0x03)       /* Local Interrupt Status Register */

#define LIS_CINT7            (1<<7)     /* Cable Interrupt 7 - CINT7         */
#define LIS_CINT6            (1<<6)     /* Cable Interrupt 6 - CINT6         */
#define LIS_CINT5            (1<<5)     /* Cable Interrupt 5 - CINT5         */
#define LIS_CINT4            (1<<4)     /* Cable Interrupt 4 - CINT4         */
#define LIS_CINT3            (1<<3)     /* Cable Interrupt 3 - CINT3         */
#define LIS_CINT2            (1<<2)     /* Cable Interrupt 2 - CINT2         */
#define LIS_CINT1            (1<<1)     /* Cable Interrupt 1 - CINT1         */

#define LIS_CINT_MASK (LIS_CINT1 | LIS_CINT2 | LIS_CINT3 | LIS_CINT4 | LIS_CINT5 | LIS_CINT6 | LIS_CINT7 )



/******************************************************************************
**
**      Remote Adaptor Registers
**
*******************************************************************************
**
**      Remote Command Register 1 (Write Only, 8 Bits)
**
*******************************************************************************
**    7    |   6    |    5    |   4    |    3   |    2   |    1   |    0  |
** +RESET  |+CLR_PT | +SND_PR |+LOCKBUS| +PGMODE| +IACK2 | +IACK1 | +IACK0|VME
**    "    |   "    |    "    |   "    |    "   |+IOPGSEL|+PRMOD1 |+PRMOD0|Q-bus
**    "    |   "    |    "    |   "    |    "   |+IOPGSEL|    0   |    0  |MBus1
******************************************************************************/

#define REM_CMD1             (0x08)             /* Remote Command Register 1 */

#define RC1_RESET_REM        (1<<7)     /* Reset remote bus - ONE SHOT       */
#define RC1_CLR_PT_INT       (1<<6)     /* PT (PM) interrupt - FROM REMOTE   */
#define RC1_SND_PR_INT       (1<<5)     /* PR (PS) interrupt - TO REMOTE     */
#define RC1_LOCK_REM_BUS     (1<<4)     /* Lock remote bus - FOR RMW ONLY    */
#define RC1_PG_SEL           (1<<3)     /* Enable page mode access           */

#define RC1_INT_ACK_A3       (1<<2)     /* IACK Read Mode Address Bit 2      */
#define RC1_INT_ACK_A2       (1<<1)     /* IACK Read Mode Address Bit 1      */
#define RC1_INT_ACK_A1       (1<<0)     /* IACK Read Mode Address Bit 0      */

#define RC1_IACK_MASK        (0x07)     /* IACK read level select mask       */


/*****************************************************************************
**
**      Remote Status Register  (Read Only, 8 Bits)
**
******************************************************************************
**    7    |    6   |    5   |    4   |    3   |   2  |   1  |   0  |
** +RRESET | IACK_1 | +PRSET | +LKNSET| +PGREG |IACK_2|+PTSET|IACK_0|VME
**    0    |    0   | +PRSET | +LKNSET| +PGMOD |   0  |+PTSET|   0  |A24,Q,MB1
*****************************************************************************/

#define REM_STATUS           (0x08)                /* Remote Status Register */

#define RSR_PR_STATUS        (1<<5)     /* PR Interrupt is set               */
#define RSR_NOT_LOCK_STATUS  (1<<4)     /* Remote bus is *NOT* locked        */
#define RSR_PG_STATUS        (1<<3)     /* Page mode access is Enabled       */
#define RSR_PT_STATUS        (1<<1)     /* PT interrupt is set               */

/* The following bits apply to A32 VMEbus products ONLY */
#define RSR_WAS_RESET        (1<<7)     /* Remote bus was reset              */
#define RSR_IACK2            (1<<2)     /* IACK Read Mode Address Bit 2      */
#define RSR_IACK1            (1<<6)     /* IACK Read Mode Address Bit 1      */
#define RSR_IACK0            (1<<0)     /* IACK Read Mode Address Bit 0      */


/******************************************************************************
**
**      Remote Command Register 2 (Read/Write, 8 Bits)
**            THIS REGISTER DOES NOT APPLY TO A24 VMEbus
**
*******************************************************************************
**      7    |     6     |     5    |     4    |   3   |   2   |   1   |   0
** +DMA_PAUS | +AMOD_SEL | +DMA_BLK | +INT_DIS | PGSZ3 | PGSZ2 | PGSZ1 | PGSZ0
******************************************************************************/

#define REM_CMD2             (0x09)             /* Remote Command Register 2 */

/* The following bits apply to A32 DMA VMEbus products only */
#define RC2_DMA_PAUSE        (1<<7)     /* DMA remote pause after 16 xfers   */
#define RC2_REM_AMOD_SEL     (1<<6)     /* Use remote address modifier  reg. */
#define RC2_DMA_BLK_SEL      (1<<5)     /* Use remote block-mode DMA operatn */
#define RC2_CINT_DISABLE     (1<<4)     /* Disable passing of rem cable intr */

/* The following bits apply to all products */
#define RC2_PG_SIZE_64K      (0x00)     /* 64K Page Size                     */
#define RC2_PG_SIZE_128K     (0x01)     /* 128K Page Size                    */
#define RC2_PG_SIZE_256K     (0x03)     /* 256K Page Size                    */
#define RC2_PG_SIZE_512K     (0x07)     /* 512K Page Size                    */
#define RC2_PG_SIZE_1MB      (0x0F)     /* 1M Page Size                      */

#define RC2_PG_SIZE_MASK     (0x0F)     /* Page Size select mask             */


/******************************************************************************
**
**      Remote Node Address Page Register (Read/Write, 16 Bits)
**
******************************************************************************/

#define REM_PAGE             (0x0A)          /* Remote Address Page Register */

#define REM_PAGE_LO          (0x0A)     /* Address page byte - A16-A23       */

/* The following define applies to A32 VMEbus products only */
#define REM_PAGE_HI          (0x0B)     /* Address page byte - A24-A31       */

#define MIN_PAGE_SHIFT       16         /* 64k is minimum size (shift value)   */
#define MAX_PAGE_SHIFT       20         /* 1MB maximum page size (shift value) */

#define QBUS_PG_MAX          (0x3F)     /* Needed for "bt_remid" to identify QBUS */


/******************************************************************************
**
**      Remote Card ID Register (Read/Write, 8 Bits)
**                              A32 Products Only
**
******************************************************************************/

#define REM_CARD_ID          (0x0C)               /* Remote Card ID Register */


/******************************************************************************
**
**      Remote Address Modifier Register (Read/Write, 8 Bits)
**                              VMEbus Products Only
**
******************************************************************************/

#define REM_AMOD             (0x0D)      /* Remote Address Modifier Register */


/******************************************************************************
**
**      IACK Read Register (Read Only)
**                             VMEbus & Q-bus Only
**
******************************************************************************/

#define REM_IACK             (0x0E)    /* IACK Read Register */

#define REM_IACK_WORD        (0x0E)    /* IACK vector-D0-D7(word)             */
#define REM_IACK_BYTE        (0x0F)    /* IACK vector-D0-D7(byte)D8-D15(word) */



/******************************************************************************
**
**      DMA Registers
**
*******************************************************************************
**
**      Local DMA Command Register (Read/Write, 8 Bits)
**
*******************************************************************************
**      7     |    6    |     5    |     4    |  3  |    2    |    1    |  0
** +LDC_START | +DP_SEL | +WRT_SEL | +D32_SEL |  0  | +INT_EN | +DMA_DN |  0
******************************************************************************/

#define LDMA_CMD             (0x10)            /* Local DMA Command Register */

#define LDC_START            (1<<7)    /* Start DMA                          */
#define LDC_DP_SEL           (1<<6)    /* DMA to Dual-Port select            */
#define LDC_WRITE_SEL        (1<<5)    /* DMA transfer direction             */
#define LDC_DMA_D32_SEL      (1<<4)    /* DMA transfer size 16 / 32 bit data */
#define LDC_DMA_INT_ENABLE   (1<<2)    /* DMA done interrupt enable          */
#define LDC_DMA_DONE         (1<<1)    /* DMA done indicator flag            */
#define LDC_DMA_ACTIVE       (1<<0)    /* DMA in progress indicator flag     */


/******************************************************************************
**
**      Local DMA Remainder Count Register (Read/Write, 8 Bits)
**
******************************************************************************/

#define LDMA_RMD_CNT         (0x11)    /* Local DMA Remainder Count Register */


/*******************************************************************************
**
**      Local DMA Packet Count Register (Read/Write, 16 Bits)
**
*******************************************************************************/

#define LDMA_PKT_CNT         (0x12)       /* Local DMA Packet Count Register  */

#define LDMA_PKT_CNT_LO      (0x12)       /* Packet Count Byte - D0-D7          */
#define LDMA_PKT_CNT_HI      (0x13)       /* Packet Count Byte - D8-D15         */

#define LDMA_MIN_DMA_PKT_SIZE 4UL         /* Minimum DMA Packet Size In Bytes   */
#define LDMA_DMA_PKT_SIZE     8UL         /* Standard DMA Packet Size Shift Val */
#define LDMA_MAX_XFER_LEN     0xFFFFFFL   /* Maximum DMA transfer Size In Bytes */


/******************************************************************************
**
**      Local DMA Address Registers (Read/Write)
**
*******************************************************************************/
#define LDMA_ADDR            (0x14)     /* Local DMA Address [indexes DMA map space] */

#define LDMA_ADDR_LO         (0x14)     /* Local DMA Address Byte - D0-D7     */
#define LDMA_ADDR_MID        (0x15)     /* Local DMA Address Byte - D8-D15    */
#define LDMA_ADDR_HI         (0x16)     /* Local DMA Address Byte - D8-D15    */



/******************************************************************************
**
**      Remote DMA Packet Length Count Register (Read/Write)
**
******************************************************************************/
#define RDMA_LEN_CNT         (0x18)            /* Remote DMA 1st packet size */


/******************************************************************************
**
**      Remote DMA Address Register (Read/Write)
**
******************************************************************************/
#define RDMA_ADDR             (0x1A)                   /* Remote DMA Address */

#define RDMA_ADDR_HI          (0x1A)   /* Remote DMA Address (A31-A16)       */
#define RDMA_ADDR_LO          (0x1C)   /* Remote DMA Address (A15-A0)        */


/******************************************************************************
**
**      Remote Slave Status Register (Read Only, 8 Bits)
**
*******************************************************************************
**    7    |   6    |    5   |    4    |   3   |    2   |    1   |    0
** +PARITY |+REMBUS |+PR_INT |+CARD_RDY|   0   |+TIMEOUT|+LRC_ERR|+NOCONNECT
******************************************************************************/

#define REM_SLAVE_STATUS     (0x1F)     /* Local Status of remote card */

#define RSS_PARITY_ERR       (1<<7)     /* Interface Parity Error Remote->PCI */
#define RSS_REMBUS_ERR       (1<<6)     /* Invalid mapping RAM access or a    */
                                        /*     data parity error occurred      */
#define RSS_PR_STATUS        (1<<5)     /* PR interrupt set on the VME card   */
#define RSS_TIMEOUT_ERR      (1<<2)     /* Interface Timeout error on DMA xfer */
#define RSS_PT_STATUS        (1<<1)     /* PT interrupt set on the VME card   */
#define RSS_NO_CONNECT       (1<<0)     /* Rem. bus power / I/O cable is off  */

#define RSS_ERROR_MASK       (RSS_PARITY_ERR|RSS_REMBUS_ERR|RSS_TIMEOUT_ERR)
#define RSS_CERROR_MASK      (RSS_NO_CONNECT|RSS_ERROR_MASK)

#endif /* KERNEL */

#endif
