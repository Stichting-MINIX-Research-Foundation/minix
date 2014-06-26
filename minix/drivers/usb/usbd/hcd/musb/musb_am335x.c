/*
 * Implementation of whatever is hardware specific in AM335x MCU
 */

#include <string.h>					/* memset */

#include <usbd/hcd_common.h>
#include <usbd/hcd_platforms.h>
#include <usbd/hcd_interface.h>
#include <usbd/usbd_common.h>

#include "musb_core.h"

/* TODO: BeagleBone white uses USB0 for PC connection as peripheral,
 * so this is disabled by default */
/* Should USB0 ever be used on BBB/BBW, one can change it to '#define': */
#undef AM335X_USE_USB0


/*===========================================================================*
 *    AM335x base register defines                                           *
 *===========================================================================*/
/* Memory placement defines */
#define AM335X_USBSS_BASE_ADDR			0x47400000u
#define AM335X_USB0_BASE_OFFSET			0x1000u
#define AM335X_MUSB_CORE0_BASE_OFFSET		0x1400u
#define AM335X_USB1_BASE_OFFSET			0x1800u
#define AM335X_MUSB_CORE1_BASE_OFFSET		0x1C00u
#define AM335X_USBSS_TOTAL_REG_LEN		0x5000u


/*===========================================================================*
 *    AM335x USB specific register defines                                   *
 *===========================================================================*/
/* SS registers base address */

#define AM335X_REG_REVREG			0x000u
#define AM335X_REG_SYSCONFIG			0x010u
#define AM335X_REG_IRQSTATRAW			0x024u
#define AM335X_REG_IRQSTAT			0x028u
#define AM335X_REG_IRQENABLER			0x02Cu
#define AM335X_REG_IRQCLEARR			0x030u
#define AM335X_REG_IRQDMATHOLDTX00		0x100u
#define AM335X_REG_IRQDMATHOLDTX01		0x104u
#define AM335X_REG_IRQDMATHOLDTX02		0x108u
#define AM335X_REG_IRQDMATHOLDTX03		0x10Cu
#define AM335X_REG_IRQDMATHOLDRX00		0x110u
#define AM335X_REG_IRQDMATHOLDRX01		0x114u
#define AM335X_REG_IRQDMATHOLDRX02		0x118u
#define AM335X_REG_IRQDMATHOLDRX03		0x11Cu
#define AM335X_REG_IRQDMATHOLDTX10		0x120u
#define AM335X_REG_IRQDMATHOLDTX11		0x124u
#define AM335X_REG_IRQDMATHOLDTX12		0x128u
#define AM335X_REG_IRQDMATHOLDTX13		0x12Cu
#define AM335X_REG_IRQDMATHOLDRX10		0x130u
#define AM335X_REG_IRQDMATHOLDRX11		0x134u
#define AM335X_REG_IRQDMATHOLDRX12		0x138u
#define AM335X_REG_IRQDMATHOLDRX13		0x13Cu
#define AM335X_REG_IRQDMAENABLE0		0x140u
#define AM335X_REG_IRQDMAENABLE1		0x144u
#define AM335X_REG_IRQFRAMETHOLDTX00		0x200u
#define AM335X_REG_IRQFRAMETHOLDTX01		0x204u
#define AM335X_REG_IRQFRAMETHOLDTX02		0x208u
#define AM335X_REG_IRQFRAMETHOLDTX03		0x20Cu
#define AM335X_REG_IRQFRAMETHOLDRX00		0x210u
#define AM335X_REG_IRQFRAMETHOLDRX01		0x214u
#define AM335X_REG_IRQFRAMETHOLDRX02		0x218u
#define AM335X_REG_IRQFRAMETHOLDRX03		0x21Cu
#define AM335X_REG_IRQFRAMETHOLDTX10		0x220u
#define AM335X_REG_IRQFRAMETHOLDTX11		0x224u
#define AM335X_REG_IRQFRAMETHOLDTX12		0x228u
#define AM335X_REG_IRQFRAMETHOLDTX13		0x22Cu
#define AM335X_REG_IRQFRAMETHOLDRX10		0x230u
#define AM335X_REG_IRQFRAMETHOLDRX11		0x234u
#define AM335X_REG_IRQFRAMETHOLDRX12		0x238u
#define AM335X_REG_IRQFRAMETHOLDRX13		0x23Cu
#define AM335X_REG_IRQFRAMEENABLE0		0x240u
#define AM335X_REG_IRQFRAMEENABLE1		0x244u

#define AM335X_REG_USBXREV			0x00u
#define AM335X_REG_USBXCTRL			0x14u
#define AM335X_REG_USBXSTAT			0x18u
#define AM335X_REG_USBXIRQMSTAT			0x20u
#define AM335X_REG_USBXIRQSTATRAW0		0x28u
#define AM335X_REG_USBXIRQSTATRAW1		0x2Cu
#define AM335X_REG_USBXIRQSTAT0			0x30u
#define AM335X_REG_USBXIRQSTAT1			0x34u
#define AM335X_REG_USBXIRQENABLESET0		0x38u
#define AM335X_REG_USBXIRQENABLESET1		0x3Cu
#define AM335X_REG_USBXIRQENABLECLR0		0x40u
#define AM335X_REG_USBXIRQENABLECLR1		0x44u
#define AM335X_REG_USBXTXMODE			0x70u
#define AM335X_REG_USBXRXMODE			0x74u
#define AM335X_REG_USBXGENRNDISEP1		0x80u
#define AM335X_REG_USBXGENRNDISEP2		0x84u
#define AM335X_REG_USBXGENRNDISEP3		0x88u
#define AM335X_REG_USBXGENRNDISEP4		0x8Cu
#define AM335X_REG_USBXGENRNDISEP5		0x90u
#define AM335X_REG_USBXGENRNDISEP6		0x94u
#define AM335X_REG_USBXGENRNDISEP7		0x98u
#define AM335X_REG_USBXGENRNDISEP8		0x9Cu
#define AM335X_REG_USBXGENRNDISEP9		0xA0u
#define AM335X_REG_USBXGENRNDISEP10		0xA4u
#define AM335X_REG_USBXGENRNDISEP11		0xA8u
#define AM335X_REG_USBXGENRNDISEP12		0xACu
#define AM335X_REG_USBXGENRNDISEP13		0xB0u
#define AM335X_REG_USBXGENRNDISEP14		0xB4u
#define AM335X_REG_USBXGENRNDISEP15		0xB8u
#define AM335X_REG_USBXAUTOREQ			0xD0u
#define AM335X_REG_USBXSRPFIXTIME		0xD4u
#define AM335X_REG_USBX_TDOWN			0xD8u
#define AM335X_REG_USBXUTMI			0xE0u
#define AM335X_REG_USBXMGCUTMILB		0xE4u
#define AM335X_REG_USBXMODE			0xE8u

/* Values to be set */
#define AM335X_VAL_USBXCTRL_SOFT_RESET		HCD_BIT(0)
#define AM335X_VAL_USBXCTRL_UINT		HCD_BIT(3)

#define AM335X_VAL_USBXMODE_IDDIG_MUX		HCD_BIT(7)
#define AM335X_VAL_USBXMODE_IDDIG		HCD_BIT(8)

#define AM335X_VAL_USBXIRQENABLEXXX0_EP0	HCD_BIT(0)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP1	HCD_BIT(1)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP2	HCD_BIT(2)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP3	HCD_BIT(3)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP4	HCD_BIT(4)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP5	HCD_BIT(5)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP6	HCD_BIT(6)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP7	HCD_BIT(7)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP8	HCD_BIT(8)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP9	HCD_BIT(9)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP10	HCD_BIT(10)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP11	HCD_BIT(11)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP12	HCD_BIT(12)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP13	HCD_BIT(13)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP14	HCD_BIT(14)
#define AM335X_VAL_USBXIRQENABLEXXX0_TX_EP15	HCD_BIT(15)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP1	HCD_BIT(17)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP2	HCD_BIT(18)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP3	HCD_BIT(19)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP4	HCD_BIT(20)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP5	HCD_BIT(21)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP6	HCD_BIT(22)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP7	HCD_BIT(23)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP8	HCD_BIT(24)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP9	HCD_BIT(25)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP10	HCD_BIT(26)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP11	HCD_BIT(27)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP12	HCD_BIT(28)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP13	HCD_BIT(29)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP14	HCD_BIT(30)
#define AM335X_VAL_USBXIRQENABLEXXX0_RX_EP15	HCD_BIT(31)

#define AM335X_VAL_USBXIRQENABLEXXX1_SUSPEND		HCD_BIT(0)
#define AM335X_VAL_USBXIRQENABLEXXX1_RESUME		HCD_BIT(1)
#define AM335X_VAL_USBXIRQENABLEXXX1_RESET_BABBLE	HCD_BIT(2)
#define AM335X_VAL_USBXIRQENABLEXXX1_SOF		HCD_BIT(3)
#define AM335X_VAL_USBXIRQENABLEXXX1_CONNECTED		HCD_BIT(4)
#define AM335X_VAL_USBXIRQENABLEXXX1_DISCONNECTED	HCD_BIT(5)
#define AM335X_VAL_USBXIRQENABLEXXX1_SRP		HCD_BIT(6)
#define AM335X_VAL_USBXIRQENABLEXXX1_VBUS		HCD_BIT(7)
#define AM335X_VAL_USBXIRQENABLEXXX1_DRVVBUS		HCD_BIT(8)
#define AM335X_VAL_USBXIRQENABLEXXX1_GENERIC		HCD_BIT(9)

#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_1		HCD_BIT(17)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_2		HCD_BIT(18)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_3		HCD_BIT(19)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_4		HCD_BIT(20)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_5		HCD_BIT(21)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_6		HCD_BIT(22)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_7		HCD_BIT(23)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_8		HCD_BIT(24)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_9		HCD_BIT(25)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_10		HCD_BIT(26)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_11		HCD_BIT(27)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_12		HCD_BIT(28)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_13		HCD_BIT(29)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_14		HCD_BIT(30)
#define AM335X_VAL_USBXIRQENABLEXXX1_TX_FIFO_15		HCD_BIT(31)

#define AM335X_VAL_USBXIRQSTAT1_SUSPEND		HCD_BIT(0)
#define AM335X_VAL_USBXIRQSTAT1_RESUME		HCD_BIT(1)
#define AM335X_VAL_USBXIRQSTAT1_RESET_BABBLE	HCD_BIT(2)
#define AM335X_VAL_USBXIRQSTAT1_SOF		HCD_BIT(3)
#define AM335X_VAL_USBXIRQSTAT1_CONNECTED	HCD_BIT(4)
#define AM335X_VAL_USBXIRQSTAT1_DISCONNECTED	HCD_BIT(5)
#define AM335X_VAL_USBXIRQSTAT1_SRP		HCD_BIT(6)
#define AM335X_VAL_USBXIRQSTAT1_VBUS		HCD_BIT(7)
#define AM335X_VAL_USBXIRQSTAT1_DRVVBUS		HCD_BIT(8)
#define AM335X_VAL_USBXIRQSTAT1_GENERIC		HCD_BIT(9)

/* Helpers for interrupt clearing */
#define CLEAR_IRQ0(irq0_bit)	HCD_WR4(r, AM335X_REG_USBXIRQSTAT0, (irq0_bit))
#define CLEAR_IRQ1(irq1_bit)	HCD_WR4(r, AM335X_REG_USBXIRQSTAT1, (irq1_bit))


/*===========================================================================*
 *    AM335x clocking register defines                                       *
 *===========================================================================*/
/* Clock module registers offsets */
#define AM335X_REG_CM_PER_USB0_CLKCTRL_OFFSET			0x1Cu

/* Possible values to be set */
#define AM335X_VAL_CM_PER_USB0_CLKCTRL_MODULEMODE_ENABLE	0x2u
#define AM335X_CLKCONF_FULL_VAL					0xFFFFFFFFu


/*===========================================================================*
 *    AM335x USB configuration structures                                    *
 *===========================================================================*/
#define AM335X_USBSS_IRQ			17
#define AM335X_USB0_IRQ				18
#define AM335X_USB1_IRQ				19

/* Hardware configuration values specific to AM335X USBSS (USB Subsystem) */
typedef struct am335x_ss_config {

	void * regs; /* Points to beginning of memory mapped register space */
}
am335x_ss_config;

/* Hardware configuration values specific to AM335X USB(0,1) OTG */
typedef struct am335x_usbX_config {

	void * regs; /* Points to beginning of memory mapped register space */
}
am335x_usbX_config;

/* Private data for AM335X's IRQ thread */
typedef struct am335x_irq_private {

	int usb_num; /* Number of currently handled controller (0, 1) */
}
am335x_irq_private;

/* Single MUSB peripheral information */
typedef struct am335x_controller {

	am335x_irq_private	priv;
	am335x_usbX_config	usb;
	musb_core_config	core;
	hcd_driver_state	driver;
}
am335x_controller;

#define AM335X_NUM_USB_CONTROLLERS	2
#define AM335X_USB0			0
#define AM335X_USB1			1

/* Configuration values specific to AM335X... */
typedef struct am335x_config {

	am335x_ss_config	ss;
	am335x_controller	ctrl[AM335X_NUM_USB_CONTROLLERS];
}
am335x_config;

/* ...and their current holder */
static am335x_config am335x;


/*===========================================================================*
 *    Local declarations                                                     *
 *===========================================================================*/
/* Basic functionality */
static int musb_am335x_internal_init(void);
static void musb_am335x_internal_deinit(void);

/* Interrupt related */
static void musb_am335x_irq_init(void *);
static void musb_am335x_usbss_isr(void *);
static void musb_am335x_usbx_isr(void *);
static hcd_reg1 musb_am335x_irqstat0_to_ep(int);

/* Configuration helpers */
static void musb_am335x_usb_reset(int);
static void musb_am335x_otg_enable(int);


/*===========================================================================*
 *    musb_am335x_init                                                       *
 *===========================================================================*/
int
musb_am335x_init(void)
{
	am335x_controller * ctrl;

	DEBUG_DUMP;

	/* Initial cleanup */
	memset(&am335x, 0, sizeof(am335x));

	/* These registers are specific to AM335X so they are mapped here */
	/* -------------------------------------------------------------- */
	/* USBSS -------------------------------------------------------- */
	/* -------------------------------------------------------------- */

	/* Map memory for USBSS */
	am335x.ss.regs = hcd_os_regs_init(AM335X_USBSS_BASE_ADDR,
					AM335X_USBSS_TOTAL_REG_LEN);

	if (NULL == am335x.ss.regs)
		return EXIT_FAILURE;

	/* Attach IRQ to number */
	if (EXIT_SUCCESS != hcd_os_interrupt_attach(AM335X_USBSS_IRQ,
						musb_am335x_irq_init,
						musb_am335x_usbss_isr,
						NULL))
		return EXIT_FAILURE;

#ifdef AM335X_USE_USB0
	/* -------------------------------------------------------------- */
	/* USB0 --------------------------------------------------------- */
	/* -------------------------------------------------------------- */
	{
		ctrl = &(am335x.ctrl[AM335X_USB0]);

		/* IRQ thread private data */
		ctrl->priv.usb_num = AM335X_USB0;

		/* MUSB core addresses for later registering */
		ctrl->core.regs = (void*)((hcd_addr)am335x.ss.regs +
					AM335X_MUSB_CORE0_BASE_OFFSET);

		/* Map AM335X USB0 specific addresses */
		ctrl->usb.regs = (void*)((hcd_addr)am335x.ss.regs +
					AM335X_USB0_BASE_OFFSET);

		/* Attach IRQ to number */
		if (EXIT_SUCCESS != hcd_os_interrupt_attach(AM335X_USB0_IRQ,
							musb_am335x_irq_init,
							musb_am335x_usbx_isr,
							&(ctrl->priv)))
			return EXIT_FAILURE;

		/* Initialize HCD driver */
		ctrl->driver.private_data = &(ctrl->core);
		ctrl->driver.setup_device = musb_setup_device;
		ctrl->driver.reset_device = musb_reset_device;
		ctrl->driver.setup_stage = musb_setup_stage;
		ctrl->driver.rx_stage = musb_rx_stage;
		ctrl->driver.tx_stage = musb_tx_stage;
		ctrl->driver.in_data_stage = musb_in_data_stage;
		ctrl->driver.out_data_stage = musb_out_data_stage;
		ctrl->driver.in_status_stage = musb_in_status_stage;
		ctrl->driver.out_status_stage = musb_out_status_stage;
		ctrl->driver.read_data = musb_read_data;
		ctrl->driver.check_error = musb_check_error;
		ctrl->driver.port_device = NULL;
	}
#endif

	/* -------------------------------------------------------------- */
	/* USB1 --------------------------------------------------------- */
	/* -------------------------------------------------------------- */
	{
		ctrl = &(am335x.ctrl[AM335X_USB1]);

		/* IRQ thread private data */
		ctrl->priv.usb_num = AM335X_USB1;

		/* MUSB core addresses for later registering */
		ctrl->core.regs = (void*)((hcd_addr)am335x.ss.regs +
					AM335X_MUSB_CORE1_BASE_OFFSET);

		/* Map AM335X USB0 specific addresses */
		ctrl->usb.regs = (void*)((hcd_addr)am335x.ss.regs +
					AM335X_USB1_BASE_OFFSET);

		/* Attach IRQ to number */
		if (EXIT_SUCCESS != hcd_os_interrupt_attach(AM335X_USB1_IRQ,
							musb_am335x_irq_init,
							musb_am335x_usbx_isr,
							&(ctrl->priv)))
			return EXIT_FAILURE;

		/* Initialize HCD driver */
		ctrl->driver.private_data = &(ctrl->core);
		ctrl->driver.setup_device = musb_setup_device;
		ctrl->driver.reset_device = musb_reset_device;
		ctrl->driver.setup_stage = musb_setup_stage;
		ctrl->driver.rx_stage = musb_rx_stage;
		ctrl->driver.tx_stage = musb_tx_stage;
		ctrl->driver.in_data_stage = musb_in_data_stage;
		ctrl->driver.out_data_stage = musb_out_data_stage;
		ctrl->driver.in_status_stage = musb_in_status_stage;
		ctrl->driver.out_status_stage = musb_out_status_stage;
		ctrl->driver.read_data = musb_read_data;
		ctrl->driver.check_error = musb_check_error;
		ctrl->driver.port_device = NULL;
	}

	return musb_am335x_internal_init();
}


/*===========================================================================*
 *    musb_am335x_deinit                                                     *
 *===========================================================================*/
void
musb_am335x_deinit(void)
{
	DEBUG_DUMP;

	musb_am335x_internal_deinit();

	/* Release IRQ resources */
	/* TODO: DDEKit has no checks on NULL IRQ and may crash on detach
	 * if interrupts were not attached properly */
	hcd_os_interrupt_detach(AM335X_USB1_IRQ);
#ifdef AM335X_USE_USB0
	hcd_os_interrupt_detach(AM335X_USB0_IRQ);
#endif
	hcd_os_interrupt_detach(AM335X_USBSS_IRQ);

	/* Release maps if anything was assigned */
	if (NULL != am335x.ss.regs)
		if (EXIT_SUCCESS != hcd_os_regs_deinit((hcd_addr)am335x.ss.regs,
						AM335X_USBSS_TOTAL_REG_LEN))
			USB_MSG("Failed to release USBSS mapping");
}


/*===========================================================================*
 *    musb_am335x_internal_init                                              *
 *===========================================================================*/
static int
musb_am335x_internal_init(void)
{
	DEBUG_DUMP;

	/* Configure clocking */
	if (hcd_os_clkconf(AM335X_REG_CM_PER_USB0_CLKCTRL_OFFSET,
			AM335X_VAL_CM_PER_USB0_CLKCTRL_MODULEMODE_ENABLE,
			AM335X_CLKCONF_FULL_VAL))
		return EXIT_FAILURE;

	/* Read and dump revision register */
	USB_MSG("MUSB revision (REVREG): 0x%08X",
		(unsigned int)HCD_RD4(am335x.ss.regs, AM335X_REG_REVREG));

	/* Allow OS to handle previously configured USBSS interrupts */
	hcd_os_interrupt_enable(AM335X_USBSS_IRQ);

#ifdef AM335X_USE_USB0
	/* Reset controllers so we get default register values */
	musb_am335x_usb_reset(AM335X_USB0);
	/* Allow OS to handle previously configured USB0 interrupts */
	hcd_os_interrupt_enable(AM335X_USB0_IRQ);
	/* Enable whatever necessary for OTG part of controller */
	musb_am335x_otg_enable(AM335X_USB0);
	/* Start actual MUSB core */
	musb_core_start(&(am335x.ctrl[AM335X_USB0].core));
#endif

	/* Reset controllers so we get default register values */
	musb_am335x_usb_reset(AM335X_USB1);
	/* Allow OS to handle previously configured USB1 interrupts */
	hcd_os_interrupt_enable(AM335X_USB1_IRQ);
	/* Enable whatever necessary for OTG part of controller */
	musb_am335x_otg_enable(AM335X_USB1);
	/* Start actual MUSB core */
	musb_core_start(&(am335x.ctrl[AM335X_USB1].core));

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    musb_am335x_internal_deinit                                            *
 *===========================================================================*/
static void
musb_am335x_internal_deinit(void)
{
	DEBUG_DUMP;

	/* Disable all interrupts */
	hcd_os_interrupt_disable(AM335X_USBSS_IRQ);
#ifdef AM335X_USE_USB0
	hcd_os_interrupt_disable(AM335X_USB0_IRQ);
#endif
	hcd_os_interrupt_disable(AM335X_USB1_IRQ);

	/* Stop core */
#ifdef AM335X_USE_USB0
	musb_core_stop(&(am335x.ctrl[AM335X_USB0].core));
#endif
	musb_core_stop(&(am335x.ctrl[AM335X_USB1].core));

	/* Every clkconf call should have corresponding release */
	hcd_os_clkconf_release();
}


/*===========================================================================*
 *    musb_am335x_irq_init                                                   *
 *===========================================================================*/
static void
musb_am335x_irq_init(void * UNUSED(unused))
{
	DEBUG_DUMP;

	/* TODO: This function does nothing and is not needed by MUSB but
	 * is still required by DDEKit for initialization, as NULL pointer
	 * cannot be passed to ddekit_interrupt_attach */
	return;
}


/*===========================================================================*
 *    musb_am335x_usbss_isr                                                  *
 *===========================================================================*/
static void
musb_am335x_usbss_isr(void * UNUSED(data))
{
	void * r;
	hcd_reg4 irqstat;

	DEBUG_DUMP;

	r = am335x.ss.regs;

	irqstat = HCD_RD4(r, AM335X_REG_IRQSTAT);

	USB_DBG("AM335X_REG_IRQSTAT = %X", (unsigned int)irqstat);

	/* Write to clear interrupt */
	HCD_WR4(r, AM335X_REG_IRQSTAT, irqstat);
}


/*===========================================================================*
 *    musb_am335x_usbx_isr                                                   *
 *===========================================================================*/
static void
musb_am335x_usbx_isr(void * data)
{
	void * r;
	hcd_driver_state * driver;
	hcd_reg4 irqstat0;
	hcd_reg4 irqstat1;
	int usb_num;

	DEBUG_DUMP;

	/* Prepare locals based on USB controller number for this interrupt */
	usb_num = ((am335x_irq_private*)data)->usb_num;
	r = am335x.ctrl[usb_num].usb.regs;
	driver = &(am335x.ctrl[usb_num].driver);

	/* Read, handle and clean interrupts */
	irqstat0 = HCD_RD4(r, AM335X_REG_USBXIRQSTAT0);
	irqstat1 = HCD_RD4(r, AM335X_REG_USBXIRQSTAT1);

	/* Interrupts, seem to appear one at a time
	 * Check which bit is set to resolve event */
	if (irqstat1 & AM335X_VAL_USBXIRQSTAT1_DRVVBUS) {
		USB_DBG("DRVVBUS level changed");
		CLEAR_IRQ1(AM335X_VAL_USBXIRQSTAT1_DRVVBUS);
		return;
	}

	if (irqstat1 & AM335X_VAL_USBXIRQENABLEXXX1_CONNECTED) {
		USB_DBG("Device connected");
		CLEAR_IRQ1(AM335X_VAL_USBXIRQENABLEXXX1_CONNECTED);
		hcd_update_port(driver, HCD_EVENT_CONNECTED);
		hcd_handle_event(driver->port_device, HCD_EVENT_CONNECTED,
				HCD_UNUSED_VAL);
		return;
	}

	if (irqstat1 & AM335X_VAL_USBXIRQENABLEXXX1_DISCONNECTED) {
		USB_DBG("Device disconnected");
		CLEAR_IRQ1(AM335X_VAL_USBXIRQENABLEXXX1_DISCONNECTED);
		hcd_handle_event(driver->port_device, HCD_EVENT_DISCONNECTED,
				HCD_UNUSED_VAL);
		hcd_update_port(driver, HCD_EVENT_DISCONNECTED);
		return;
	}

	if (0 != irqstat0) {
		USB_DBG("EP interrupt");
		CLEAR_IRQ0(irqstat0);
		hcd_handle_event(driver->port_device, HCD_EVENT_ENDPOINT,
				musb_am335x_irqstat0_to_ep(irqstat0));
		return;
	}

	if (irqstat1 & AM335X_VAL_USBXIRQENABLEXXX1_SUSPEND) {
		USB_DBG("Unhandled SUSPEND IRQ");
		CLEAR_IRQ1(AM335X_VAL_USBXIRQENABLEXXX1_SUSPEND);
		return;
	}

	if (irqstat1 & AM335X_VAL_USBXIRQENABLEXXX1_RESUME) {
		USB_DBG("Unhandled RESUME IRQ");
		CLEAR_IRQ1(AM335X_VAL_USBXIRQENABLEXXX1_RESUME);
		return;
	}

	if (irqstat1 & AM335X_VAL_USBXIRQENABLEXXX1_RESET_BABBLE) {
		USB_DBG("Unhandled RESET/BABBLE IRQ");
		CLEAR_IRQ1(AM335X_VAL_USBXIRQENABLEXXX1_RESET_BABBLE);
		return;
	}

	if (irqstat1 & AM335X_VAL_USBXIRQENABLEXXX1_SOF) {
		USB_DBG("Unhandled SOF IRQ");
		CLEAR_IRQ1(AM335X_VAL_USBXIRQENABLEXXX1_SOF);
		return;
	}

	if (irqstat1 & AM335X_VAL_USBXIRQENABLEXXX1_SRP) {
		USB_DBG("Unhandled SRP IRQ");
		CLEAR_IRQ1(AM335X_VAL_USBXIRQENABLEXXX1_SRP);
		return;
	}

	if (irqstat1 & AM335X_VAL_USBXIRQENABLEXXX1_VBUS) {
		USB_DBG("Unhandled VBUS IRQ");
		CLEAR_IRQ1(AM335X_VAL_USBXIRQENABLEXXX1_VBUS);
		return;
	}

	if (irqstat1 & AM335X_VAL_USBXIRQENABLEXXX1_GENERIC) {
		USB_DBG("Unhandled GENERIC IRQ");
		CLEAR_IRQ1(AM335X_VAL_USBXIRQENABLEXXX1_GENERIC);
		return;
	}

	/* When controller is correctly configured this should never happen: */
	USB_MSG("Illegal value of IRQxSTAT: 0=%X 1=%X",
		(unsigned int)irqstat0, (unsigned int)irqstat1);
	USB_ASSERT(0, "IRQxSTAT error");
}


/*===========================================================================*
 *    musb_am335x_irqstat0_to_ep                                             *
 *===========================================================================*/
static hcd_reg1
musb_am335x_irqstat0_to_ep(int irqstat0)
{
	hcd_reg1 ep;

	DEBUG_DUMP;

	ep = 0;

	while (0 == (irqstat0 & 0x01)) {
		irqstat0 >>= 1;
		ep++;
		/* Must be within two consecutive EP sets */
		USB_ASSERT(ep < (2 * HCD_TOTAL_EP),
			"Invalid IRQSTAT0 supplied (1)");
	}

	/* Convert RX interrupt to EP number */
	if (ep >= HCD_TOTAL_EP) {
		ep -= HCD_TOTAL_EP;
		/* Must not be control EP */
		USB_ASSERT(ep != HCD_DEFAULT_EP,
			"Invalid IRQSTAT0 supplied (2)");
	}

	return ep;
}


/*===========================================================================*
 *    musb_am335x_usb_reset                                                  *
 *===========================================================================*/
static void
musb_am335x_usb_reset(int usb_num)
{
	void * r;
	hcd_reg4 ctrl;

	DEBUG_DUMP;

	r = am335x.ctrl[usb_num].usb.regs;

	/* Set SOFT_RESET bit and wait until it is off */
	ctrl = HCD_RD4(r, AM335X_REG_USBXCTRL);
	HCD_SET(ctrl, AM335X_VAL_USBXCTRL_SOFT_RESET);
	HCD_WR4(r, AM335X_REG_USBXCTRL, ctrl);
	while (HCD_RD4(r, AM335X_REG_USBXCTRL) &
		AM335X_VAL_USBXCTRL_SOFT_RESET);
}


/*===========================================================================*
 *    musb_am335x_otg_enable                                                 *
 *===========================================================================*/
static void
musb_am335x_otg_enable(int usb_num)
{
	void * r;
	hcd_reg4 intreg;
	hcd_reg4 mode;

	DEBUG_DUMP;

	r = am335x.ctrl[usb_num].usb.regs;

	/* Force host operation */
	mode = HCD_RD4(r, AM335X_REG_USBXMODE);
	HCD_SET(mode, AM335X_VAL_USBXMODE_IDDIG_MUX);
	HCD_CLR(mode, AM335X_VAL_USBXMODE_IDDIG);
	HCD_WR4(r, AM335X_REG_USBXMODE, mode);

	/* Set all important interrupts to be handled */
	intreg = HCD_RD4(r, AM335X_REG_USBXIRQENABLESET1);
	HCD_SET(intreg,	AM335X_VAL_USBXIRQENABLEXXX1_SUSPEND		|
			AM335X_VAL_USBXIRQENABLEXXX1_RESUME		|
			AM335X_VAL_USBXIRQENABLEXXX1_RESET_BABBLE	|
			/* AM335X_VAL_USBXIRQENABLEXXX1_SOF		| */
			AM335X_VAL_USBXIRQENABLEXXX1_CONNECTED		|
			AM335X_VAL_USBXIRQENABLEXXX1_DISCONNECTED	|
			AM335X_VAL_USBXIRQENABLEXXX1_SRP		|
			AM335X_VAL_USBXIRQENABLEXXX1_VBUS		|
			AM335X_VAL_USBXIRQENABLEXXX1_DRVVBUS		|
			AM335X_VAL_USBXIRQENABLEXXX1_GENERIC);

	HCD_WR4(r, AM335X_REG_USBXIRQENABLESET1, intreg);

	/* Set all EP interrupts as enabled */
	intreg = AM335X_VAL_USBXIRQENABLEXXX0_EP0	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP1	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP2	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP3	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP4	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP5	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP6	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP7	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP8	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP9	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP10	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP11	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP12	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP13	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP14	|
		AM335X_VAL_USBXIRQENABLEXXX0_TX_EP15	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP1	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP2	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP3	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP4	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP5	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP6	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP7	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP8	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP9	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP10	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP11	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP12	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP13	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP14	|
		AM335X_VAL_USBXIRQENABLEXXX0_RX_EP15	;

	HCD_WR4(r, AM335X_REG_USBXIRQENABLESET0, intreg);
}
