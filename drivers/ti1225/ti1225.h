/*
ti1225.h

Created:	Dec 2005 by Philip Homburg
*/

/* PCI attribute space registers */
#define TI_CB_BASEADDR	0x10
#define TI_PCI_BUS_NR	0x18
#define TI_CB_BUS_NR	0x19
#define TI_SO_BUS_NR	0x1A
#define TI_LEGACY_BA	0x44
#define TI_SYSCTRL	0x80
#define TI_MF_ROUTE	0x8C
#define TI_CARD_CTRL	0x91
#define		TI_CCR_IFG	0x01
#define TI_DEV_CTRL	0x92

/* CardBus Socket Registers */
struct csr
{
/*00*/	u32_t csr_event;
/*04*/	u32_t csr_mask;
/*08*/	u32_t csr_present;
/*0C*/	u32_t csr_force_event;
/*10*/	u32_t csr_control;
/*14*/	u32_t csr_res0;
/*18*/	u32_t csr_res1;
/*1C*/	u32_t csr_res2;
/*20*/	u32_t csr_power;
};

/* csr_mask */
#define CM_PWRMASK	0x00000008
#define CM_CDMASK	0x00000006
#define CM_CSTSMASK	0x00000001

/* csr_present */
#define CP_YVSOCKET	0x80000000
#define CP_XVSOCKET	0x40000000
#define CP_3VSOCKET	0x20000000
#define CP_5VSOCKET	0x10000000
#define CP_YVCARD	0x00002000
#define CP_XVCARD	0x00001000
#define CP_3VCARD	0x00000800
#define CP_5VCARD	0x00000400
#define CP_BADVCCREQ	0x00000200
#define CP_DATALOST	0x00000100
#define CP_NOTACARD	0x00000080
#define CP_IREQCINT	0x00000040
#define CP_CBCARD	0x00000020
#define CP_16BITCARD	0x00000010
#define CP_PWRCYCLE	0x00000008
#define CP_CDETECT2	0x00000004
#define CP_CDETECT1	0x00000002
#define CP_CARDSTS	0x00000001

/* csr_control */
#define CC_VCCCTRL	0x00000070
#define 	CC_VCC_OFF	0x00000000
#define 	CC_VCC_5V	0x00000020
#define 	CC_VCC_3V	0x00000030
#define 	CC_VCC_XV	0x00000040
#define 	CC_VCC_YV	0x00000050
#define CC_VPPCTRL	0x00000007
#define 	CC_VPP_OFF	0x00000000
#define 	CC_VPP_12V	0x00000001
#define 	CC_VPP_5V	0x00000002
#define 	CC_VPP_3V	0x00000003
#define 	CC_VPP_XV	0x00000004
#define 	CC_VPP_YV	0x00000005
