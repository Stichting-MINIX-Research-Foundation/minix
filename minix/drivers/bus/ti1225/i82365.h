/*
i82365.h

Created:	May 1995 by Philip Homburg <philip@cs.vu.nl>
*/

#ifndef I82365_H
#define I82365_H

/* The default I/O ports used by a i82365 are the following: */
#define I365_INDEX	0x3E0
#define I365_DATA	0x3E1

/* The index register is used to select one of the following registers: */
#define I365_REVISION		0x00	/* IDREG */
#define		I365R_ID_MASK		0xC0
#define			I365R_ID_IO		0x00
#define			I365R_ID_MEM		0x40
#define			I365R_ID_MEM_IO		0x80
#define		I365R_RES_MASK		0x30
#define		I365R_REV_MASK		0x0F

#define I365_IF_STAT		0x01	/* ISTAT */
#define			I365IS_GPI		0x80
#define			CL6722IS_VPPVALID	0x80
#define			I365IS_POWER		0x40
#define			I365IS_READY		0x20
#define			I365IS_WRTPROT		0x10
#define		I365IS_CARD_MASK	0x0C
#define			I365IS_CARD_ABSENT	0x00
#define			I365IS_CARD_PART_0	0x04
#define			I365IS_CARD_PART_1	0x08
#define			I365IS_CARD_PRESENT	0x0C
#define		I365IS_BAT_MASK		0x03
#define			I365IS_BAT_LOST_0	0x00
#define			I365IS_BAT_LOW		0x01
#define			I365IS_BAT_LOST_1	0x02
#define			I365IS_BAT_OKAY		0x03

#define I365_PWR_CTL		0x02	/* PCTRL */
#define 		I365PC_CARD_EN		0x80
#define			I365PC_NORESET		0x40
#define			CL6722PC_COMPAT_0	0x40
#define 		I365PC_AUTO_PWR		0x20
#define			I365PC_Vcc_MASK		0x18
#define				I365PC_Vcc_NC		0x00
#define				I365PC_Vcc_Reserved	0x08
#define				I365PC_Vcc_5		0x10
#define				I365PC_Vcc_33		0x18
#define 		CL6722PC_Vcc_PWR	0x10
#define 		CL6722PC_COMPAT_1	0x08
#define 	I365PC_Vpp_MASK		0x03
#define 		I365PC_Vpp_NC		0x00
#define 		CL6722PC_Vpp_ZERO_0	0x00
#define 		I365PC_Vpp_5		0x01
#define 		CL6722PC_Vpp_Vcc	0x01
#define 		I365PC_Vpp_12		0x02
#define 		I365PC_Vpp_Reserved	0x03
#define 		CL6722PC_Vpp_ZERO_1	0x03

#define I365_INT_GEN_CTL	0x03
#define 		I365IGC_RING_IND	0x80
#define 		I365IGC_RESET		0x40
#define 		I365IGC_CARD_IS_IO	0x20
#define 		I365IGC_EN_MNG_INT	0x10
#define 	I365IGC_IRQ_MASK	0x0F

#define I365_CRD_STAT_CHG	0x04	/* CSTCH */
#define 		I365CSC_GPI		0x10
#define 		I365CSC_CARD_DETECT	0x08
#define 		I365CSC_READY		0x04
#define 		I365CSC_BAT_WARN	0x02
#define 		I365CSC_BAT_DEAD	0x01

#define I365_MNG_INT_CONF	0x05
#define		I365MIC_IRQ_MASK	0xF0
#define 		I365MIC_CARD_DETECT	0x08
#define 		I365MIC_READY		0x04
#define 		I365MIC_BAT_WARN	0x02
#define 		I365MIC_BAT_DEAD	0x01

#define I365_MAP_ENABLE		0x06	/* ADWEN */
#define 		I365ME_IO_MAP_0		0x40
#define 		I365ME_MEM_MAP_0	0x01

#define I365_IO_WND_CTL		0x07
#define 		I365IWC_AUTO_1		0x80
#define 		CL6722IWC_TIMING_1	0x80
#define 		I365IWC_0WS_1		0x40
#define 		I365IWC_AUTO_SIZE_1	0x20
#define 		I365IWC_IO_SIZE_1	0x10
#define 		I365IWC_WAIT_0		0x08
#define 		I365IWC_0WS_0		0x04
#define 		CL6722IWC_TIMING_0	0x08
#define 		I365IWC_AUTO_SIZE_0	0x02
#define 		I365IWC_IO_SIZE_0	0x01

#define I365_IO_0_START_LOW	0x08
#define I365_IO_0_START_HIGH	0x09
#define I365_IO_0_END_LOW	0x0A
#define I365_IO_0_END_HIGH	0x0B
#define I365_IO_1_START_LOW	0x0C
#define I365_IO_1_START_HIGH	0x0D
#define I365_IO_1_END_LOW	0x0E
#define I365_IO_1_END_HIGH	0x0F

#define I365_MEM_0_START_LOW	0x10
#define I365_MEM_0_START_HIGH	0x11
#define I365_MEM_0_END_LOW	0x12
#define I365_MEM_0_END_HIGH	0x13
#define I365_MEM_0_OFF_LOW	0x14
#define I365_MEM_0_OFF_HIGH	0x15

#define CL6722_MISC_CTL_1	0x16
#define CL6722_FIFO_CTL		0x17

#define I365_MEM_1_START_LOW	0x18
#define I365_MEM_1_START_HIGH	0x19
#define I365_MEM_1_END_LOW	0x1A
#define I365_MEM_1_END_HIGH	0x1B
#define I365_MEM_1_OFF_LOW	0x1C
#define I365_MEM_1_OFF_HIGH	0x1D

#define CL6722_MISC_CTL_2	0x1E
#define CL6722_CHIP_INFO	0x1F
#define		CL6722CI_ID_MASK	0xC0

#define I365_MEM_2_START_LOW	0x20
#define I365_MEM_2_START_HIGH	0x21
#define I365_MEM_2_END_LOW	0x22
#define I365_MEM_2_END_HIGH	0x23
#define I365_MEM_2_OFF_LOW	0x24
#define I365_MEM_2_OFF_HIGH	0x25

#define CL6722_ATA_CONTROL	0x26	/* CPAGE */
#define I365_RESERVED		0x27

#define I365_MEM_3_START_LOW	0x28
#define I365_MEM_3_START_HIGH	0x29
#define I365_MEM_3_END_LOW	0x2A
#define I365_MEM_3_END_HIGH	0x2B
#define I365_MEM_3_OFF_LOW	0x2C
#define I365_MEM_3_OFF_HIGH	0x2D

#define CL6722_EXT_INDEX	0x2E	/* CSCTRL */
#define CL6722_EXT_DATA		0x2F

#define I365_MEM_4_START_LOW	0x30
#define I365_MEM_4_START_HIGH	0x31
#define I365_MEM_4_END_LOW	0x32
#define I365_MEM_4_END_HIGH	0x33
#define I365_MEM_4_OFF_LOW	0x34
#define I365_MEM_4_OFF_HIGH	0x35

#define CL6722_IO_0_OFF_LOW	0x36
#define CL6722_IO_0_OFF_HIGH	0x37
#define CL6722_IO_1_OFF_LOW	0x38
#define CL6722_IO_1_OFF_HIGH	0x39

#define I365_SETUP_TIM_0	0x3A
#define I365_CMD_TIM_0		0x3B
#define I365_RECOV_TIM_0	0x3C
#define I365_SETUP_TIM_1	0x3D
#define I365_CMD_TIM_1		0x3E
#define I365_RECOV_TIM_1	0x3F

#endif /* I82365_H */
