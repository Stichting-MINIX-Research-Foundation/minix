/*
**  File:	3c509.h		Jun. 01, 2000
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  Interface description for 3Com Etherlink III board.
**
**  $Log$
**  Revision 1.1  2005/06/29 10:16:46  beng
**  Import of dpeth 3c501/3c509b/.. ethernet driver by
**  Giovanni Falzoni <fgalzoni@inwind.it>.
**
**  Revision 2.0  2005/06/26 16:16:46  lsodgf0
**  Initial revision for Minix 3.0.6
*/

/* Command codes */
#define	CMD_GlobalReset		0x0000	/* resets adapter (power up status) */
#define	CMD_SelectWindow       (1<<11)	/* select register window */
#define	CMD_StartIntXcvr       (2<<11)	/* start internal transciver */
#define	CMD_RxDisable	       (3<<11)	/* rx disable */
#define	CMD_RxEnable	       (4<<11)	/* rx enable */
#define	CMD_RxReset	       (5<<11)	/* rx reset */
#define	CMD_RxDiscard	       (8<<11)	/* rx discard top packet */
#define	CMD_TxEnable	       (9<<11)	/* tx enable */
#define	CMD_TxDisable	      (10<<11)	/* tx disable */
#define	CMD_TxReset	      (11<<11)	/* tx reset */
#define	CMD_Acknowledge	      (13<<11)	/* acknowledge interrupt */
#define	CMD_SetIntMask	      (14<<11)	/* set interrupt mask */
#define	CMD_SetStatusEnab     (15<<11)	/* set read zero mask */
#define	CMD_SetRxFilter	      (16<<11)	/* set rx filter */
#define	CMD_SetTxAvailable    (18<<11)	/* set tx available threshold */
#define	CMD_StatsEnable	      (21<<11)	/* statistics enable */
#define	CMD_StatsDisable      (22<<11)	/* statistics disable */
#define	CMD_StopIntXcvr	      (23<<11)	/* start internal transciver */

/* Status register bits (INT for interrupt sources, ST for the rest) */
#define	INT_Latch		0x0001	/* interrupt latch */
#define	INT_AdapterFail		0x0002	/* adapter failure */
#define	INT_TxComplete		0x0004	/* tx complete */
#define	INT_TxAvailable		0x0008	/* tx available */
#define	INT_RxComplete	 	0x0010	/* rx complete */
#define	INT_RxEarly		0x0020	/* rx early */
#define	INT_Requested		0x0040	/* interrupt requested */
#define	INT_UpdateStats		0x0080	/* update statistics */

/* Rx Status register bits */
#define	RXS_Error		0x4000	/* error in packet */
#define	RXS_Length		0x07FF	/* bytes in RxFIFO */
#define	RXS_ErrType		0x3800	/* Rx error type, bit 13-11 */
#define	RXS_Overrun		0x0000	/* overrun error */
#define	RXS_Oversize		0x0800	/* oversize packet error */
#define	RXS_Dribble		0x1000	/* dribble bit (not an error) */
#define	RXS_Runt		0x1800	/* runt packet error */
#define	RXS_Framing		0x2000	/* framing error */
#define	RXS_CRC			0x2800	/* CRC error */

/* Tx Status register bits */

/* Window Numbers */
#define	WNO_Setup		0x0000	/* setup/configuration */
#define	WNO_Operating		0x0001	/* operating set */
#define	WNO_StationAddress	0x0002	/* station address setup/read */
#define	WNO_Diagnostics		0x0004	/* diagnostics */
#define	WNO_Statistics		0x0006	/* statistics */

/* Register offsets - Window 1 (WNO_Operating) */
#define	REG_CmdStatus		0x000E	/* command/status */
#define	REG_TxFree		0x000C	/* free transmit bytes */
#define	REG_TxStatus		0x000B	/* transmit status (byte) */
#define	REG_RxStatus		0x0008	/* receive status */
#define	REG_RxFIFO		0x0000	/* RxFIFO read */
#define	REG_TxFIFO		0x0000	/* TxFIFO write */

/* Register offsets - Window 0 (WNO_Setup) */
#define	REG_CfgControl		0x0004	/* configuration control */

/* Register offsets - Window 2 (WNO_StationAddress) */
#define	REG_SA0_1		0x0000	/* station address bytes 0,1 */

/* Register offsets - Window 3 (WNO_FIFO) */

/* Register offsets - Window 4 (WNO_Diagnostics) */
#define	REG_MediaStatus		0x000A	/* media type/status */

/* Register offsets - Window 5 (WNO_Readable) */

/* Register offsets - Window 6 (WNO_Statistics) */
#define	REG_TxBytes		0x000C	/* tx bytes ok */
#define	REG_RxBytes		0x000A	/* rx bytes ok */
#define	REG_TxDefer		0x0008	/* tx frames deferred (byte) */
#define	REG_RxFrames		0x0007	/* rx frames ok (byte) */
#define	REG_TxFrames		0x0006	/* tx frames ok (byte) */
#define	REG_RxDiscarded		0x0005	/* rx frames discarded (byte) */
#define	REG_TxLate		0x0004	/* tx frames late coll. (byte) */
#define	REG_TxSingleColl	0x0003	/* tx frames one coll. (byte) */
#define	REG_TxMultColl		0x0002	/* tx frames mult. coll. (byte) */
#define	REG_TxNoCD		0x0001	/* tx frames no CDheartbt (byte) */
#define	REG_TxCarrierLost	0x0000	/* tx frames carrier lost (byte) */

/* Various command arguments */

#define	FilterIndividual	0x0001	/* individual address */
#define	FilterMulticast		0x0002	/* multicast/group addresses */
#define	FilterBroadcast		0x0004	/* broadcast address */
#define	FilterPromiscuous	0x0008	/* promiscuous mode */

/* Resource Configuration Register bits */
#define	EL3_CONFIG_IRQ_MASK	0xF000

/* Address Configuration Register bits */
#define	EL3_CONFIG_XCVR_MASK	0xC000
#define	EL3_CONFIG_IOBASE_MASK	0x001F

#define	TP_XCVR			0x0000
#define	BNC_XCVR		0xC000
#define	AUI_XCVR		0x4000

#define	EL3_IO_BASE_ADDR	0x200

/* Transmit Preamble */

/* Bits in various diagnostics registers */
#define	MediaLBeatEnable	0x0080	/* link beat enable (TP) */
#define	MediaJabberEnable	0x0040	/* jabber enable (TP) */

/* Board identification codes, byte swapped in Rev 0 */
#define	EL3_3COM_CODE		0x6D50	/* EISA manufacturer code */
#define	EL3_PRODUCT_ID		0x9050	/* Product ID for ISA board */

/* EEProm access */
#define	EE_3COM_NODE_ADDR	0x00
#define	EE_PROD_ID		0x03
#define	EE_MANUFACTURING_DATA	0x04
#define	EE_3COM_CODE		0x07
#define	EE_ADDR_CFG		0x08
#define	EE_RESOURCE_CFG		0x09
#define	EE_SW_CONFIG_INFO	0x0D
#define	EE_PROD_ID_MASK		0xF0FF	/* Mask off revision nibble */

/* Contention logic */
#define	EL3_READ_EEPROM		0x80
#define	EL3_ID_GLOBAL_RESET	0xC0
#define	EL3_SET_TAG_REGISTER	0xD0
#define	EL3_ACTIVATE_AND_SET_IO	0xE0
#define	EL3_ACTIVATE		0xFF

/* Software Configuration Register bits */

/* Configuration Control Register bits */
#define	EL3_EnableAdapter	0x01

/* EL3 access macros */
#define inb_el3(dep,reg) (inb((dep)->de_base_port+(reg)))
#define inw_el3(dep,reg) (inw((dep)->de_base_port+(reg)))
#define outb_el3(dep,reg,data) (outb((dep)->de_base_port+(reg),(data)))
#define outw_el3(dep,reg,data) (outw((dep)->de_base_port+(reg),(data)))

#define SetWindow(win)  \
  outw(dep->de_base_port+REG_CmdStatus,CMD_SelectWindow|(win))

/** 3c509.h **/
