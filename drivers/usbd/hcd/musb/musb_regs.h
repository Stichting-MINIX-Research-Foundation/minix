/*
 * Common MUSB core registers definitions
 */

#ifndef _MUSB_REGS_H_
#define _MUSB_REGS_H_

#include <usb/hcd_common.h>


/*===========================================================================*
 *    MUSB core register offsets                                             *
 *===========================================================================*/
#define MUSB_REG_FADDR				0x00u
#define MUSB_REG_POWER				0x01u
#define MUSB_REG_INTRTX				0x02u
#define MUSB_REG_INTRRX				0x04u
#define MUSB_REG_INTRTXE			0x06u
#define MUSB_REG_INTRRXE			0x08u
#define MUSB_REG_INTRUSB			0x0Au
#define MUSB_REG_INTRUSBE			0x0Bu
#define MUSB_REG_FRAME				0x0Cu
#define MUSB_REG_INDEX				0x0Eu
#define MUSB_REG_TESTMODE			0x0Fu

/* Proxy registers for endpoint configuration,
 * that correspond to specific endpoint's register space
 * selected with MUSB_REG_INDEX */
#define MUSB_REG_TXMAXP				0x10u
#define MUSB_REG_PERI_CSR0			0x12u
#define MUSB_REG_HOST_CSR0				MUSB_REG_PERI_CSR0
#define MUSB_REG_PERI_TXCSR				MUSB_REG_PERI_CSR0
#define MUSB_REG_HOST_TXCSR				MUSB_REG_PERI_CSR0
#define MUSB_REG_RXMAXP				0x14u
#define MUSB_REG_PERI_RXCSR			0x16u
#define MUSB_REG_HOST_RXCSR				MUSB_PERI_RXCSR
#define MUSB_REG_COUNT0				0x18u
#define MUSB_REG_RXCOUNT				MUSB_COUNT0
#define MUSB_REG_HOST_TYPE0			0x1Au
#define MUSB_REG_HOST_TXTYPE				MUSB_HOST_TYPE0
#define MUSB_REG_HOST_NAKLIMIT0			0x1Bu
#define MUSB_REG_HOST_TXINTERVAL			MUSB_HOST_NAKLIMIT0
#define MUSB_REG_HOST_RXTYPE			0x1Cu
#define MUSB_REG_HOST_RXINTERVAL		0x1Du
#define MUSB_REG_CONFIGDATA			0x1Fu

#define MUSB_REG_FIFO0				0x20u
#define MUSB_REG_FIFO1				0x24u
#define MUSB_REG_FIFO2				0x28u
#define MUSB_REG_FIFO3				0x2Cu
#define MUSB_REG_FIFO4				0x30u
#define MUSB_REG_FIFO_LEN			0x04u

#define MUSB_REG_DEVCTL				0x60u
#define MUSB_REG_TXFIFOSZ			0x62u
#define MUSB_REG_RXFIFOSZ			0x63u
#define MUSB_REG_TXFIFOADDR			0x64u
#define MUSB_REG_RXFIFOADDR			0x66u

#define MUSB_REG_TXFUNCADDR			0x80u
#define MUSB_REG_TXHUBADDR			0x82u
#define MUSB_REG_TXHUBPORT			0x83u
#define MUSB_REG_RXFUNCADDR			0x84u
#define MUSB_REG_RXHUBADDR			0x86u
#define MUSB_REG_RXHUBPORT			0x87u


/*===========================================================================*
 *    MUSB core register values                                              *
 *===========================================================================*/
/* POWER */
#define MUSB_VAL_POWER_ENSUSPM			HCD_BIT(0)
#define MUSB_VAL_POWER_SUSPENDM			HCD_BIT(1)
#define MUSB_VAL_POWER_RESUME			HCD_BIT(2)
#define MUSB_VAL_POWER_RESET			HCD_BIT(3)
#define MUSB_VAL_POWER_HSMODE			HCD_BIT(4)
#define MUSB_VAL_POWER_HSEN			HCD_BIT(5)
#define MUSB_VAL_POWER_SOFTCONN			HCD_BIT(6)
#define MUSB_VAL_POWER_ISOUPDATE		HCD_BIT(7)

/* DEVCTL */
#define MUSB_VAL_DEVCTL_SESSION			HCD_BIT(0)
#define MUSB_VAL_DEVCTL_HOSTREQ			HCD_BIT(1)
#define MUSB_VAL_DEVCTL_HOSTMODE		HCD_BIT(2)
#define MUSB_VAL_DEVCTL_VBUS_1			HCD_BIT(3)
#define MUSB_VAL_DEVCTL_VBUS_2			HCD_BIT(4)
#define MUSB_VAL_DEVCTL_VBUS_3			(HCD_BIT(3) | HCD_BIT(4))
#define MUSB_VAL_DEVCTL_LSDEV			HCD_BIT(5)
#define MUSB_VAL_DEVCTL_FSDEV			HCD_BIT(6)
#define MUSB_VAL_DEVCTL_BDEVICE			HCD_BIT(7)

/* INTRUSBE */
#define MUSB_VAL_INTRUSBE_SUSPEND		HCD_BIT(0)
#define MUSB_VAL_INTRUSBE_RESUME		HCD_BIT(1)
#define MUSB_VAL_INTRUSBE_RESET_BABBLE		HCD_BIT(2)
#define MUSB_VAL_INTRUSBE_SOF			HCD_BIT(3)
#define MUSB_VAL_INTRUSBE_CONN			HCD_BIT(4)
#define MUSB_VAL_INTRUSBE_DISCON		HCD_BIT(5)
#define MUSB_VAL_INTRUSBE_SESSREQ		HCD_BIT(6)
#define MUSB_VAL_INTRUSBE_VBUSERR		HCD_BIT(7)

/* HOST_TYPE0 */
#define MUSB_VAL_HOST_TYPE0_MASK		(HCD_BIT(6) | HCD_BIT(7))
#define MUSB_VAL_HOST_TYPE0_HIGH_SPEED		HCD_BIT(6)
#define MUSB_VAL_HOST_TYPE0_FULL_SPEED		HCD_BIT(7)
#define MUSB_VAL_HOST_TYPE0_LOW_SPEED		(HCD_BIT(6) | HCD_BIT(7))

/* INTRTXE */
#define MUSB_VAL_INTRTXE_EP0			HCD_BIT(0)
#define MUSB_VAL_INTRTXE_EP1TX			HCD_BIT(1)
#define MUSB_VAL_INTRTXE_EP2TX			HCD_BIT(2)
#define MUSB_VAL_INTRTXE_EP3TX			HCD_BIT(3)
#define MUSB_VAL_INTRTXE_EP4TX			HCD_BIT(4)

/* HOST_CSR0 */
#define MUSB_VAL_HOST_CSR0_RXPKTRDY		HCD_BIT(0)
#define MUSB_VAL_HOST_CSR0_TXPKTRDY		HCD_BIT(1)
#define MUSB_VAL_HOST_CSR0_RXSTALL		HCD_BIT(2)
#define MUSB_VAL_HOST_CSR0_SETUPPKT		HCD_BIT(3)
#define MUSB_VAL_HOST_CSR0_ERROR		HCD_BIT(4)
#define MUSB_VAL_HOST_CSR0_REQPKT		HCD_BIT(5)
#define MUSB_VAL_HOST_CSR0_STATUSPKT		HCD_BIT(6)
#define MUSB_VAL_HOST_CSR0_NAK_TIMEOUT		HCD_BIT(7)
#define MUSB_VAL_HOST_CSR0_FLUSHFIFO		HCD_BIT(8)

#endif /* !_MUSB_REGS_H_ */
