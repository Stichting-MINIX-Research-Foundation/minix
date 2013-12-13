/*
 * This file implements support for i2c on the BeagleBone and BeagleBoard-xM
 */

/* kernel headers */
#include <minix/chardriver.h>
#include <minix/clkconf.h>
#include <minix/drivers.h>
#include <minix/ds.h>
#include <minix/log.h>
#include <minix/mmio.h>
#include <minix/padconf.h>
#include <minix/sysutil.h>
#include <minix/type.h>
#include <minix/board.h>
#include <minix/spin.h>

/* device headers */
#include <minix/i2c.h>

/* system headers */
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

/* usr headers */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* local headers */
#include "omap_i2c.h"

/*
 * defines the set of register
 *
 * Warning: always use the 16-bit variants of read/write/set from mmio.h
 * to access these registers. The DM37XX TRM Section 17.6 warns that 32-bit
 * accesses can corrupt the register contents.
 */
typedef struct omap_i2c_registers
{
	vir_bytes I2C_REVNB_LO;	/* AM335X Only */
	vir_bytes I2C_REVNB_HI;	/* AM335X Only */
	vir_bytes I2C_REV;	/* DM37XX Only */
	vir_bytes I2C_IE;	/* DM37XX Only */
	vir_bytes I2C_STAT;	/* DM37XX Only */
	vir_bytes I2C_SYSC;
	vir_bytes I2C_IRQSTATUS_RAW;	/* AM335X Only */
	vir_bytes I2C_IRQSTATUS;	/* AM335X Only */
	vir_bytes I2C_IRQENABLE_SET;	/* AM335X Only */
	vir_bytes I2C_IRQENABLE_CLR;	/* AM335X Only */
	vir_bytes I2C_WE;
	vir_bytes I2C_DMARXENABLE_SET;	/* AM335X Only */
	vir_bytes I2C_DMATXENABLE_SET;	/* AM335X Only */
	vir_bytes I2C_DMARXENABLE_CLR;	/* AM335X Only */
	vir_bytes I2C_DMATXENABLE_CLR;	/* AM335X Only */
	vir_bytes I2C_DMARXWAKE_EN;	/* AM335X Only */
	vir_bytes I2C_DMATXWAKE_EN;	/* AM335X Only */
	vir_bytes I2C_SYSS;
	vir_bytes I2C_BUF;
	vir_bytes I2C_CNT;
	vir_bytes I2C_DATA;
	vir_bytes I2C_CON;
	vir_bytes I2C_OA;	/* AM335X Only */
	vir_bytes I2C_OA0;	/* DM37XX Only */
	vir_bytes I2C_SA;
	vir_bytes I2C_PSC;
	vir_bytes I2C_SCLL;
	vir_bytes I2C_SCLH;
	vir_bytes I2C_SYSTEST;
	vir_bytes I2C_BUFSTAT;
	vir_bytes I2C_OA1;
	vir_bytes I2C_OA2;
	vir_bytes I2C_OA3;
	vir_bytes I2C_ACTOA;
	vir_bytes I2C_SBLOCK;
} omap_i2c_regs_t;

/* generic definition an i2c bus */

typedef struct omap_i2c_bus
{
	enum bus_types
	{ AM335X_I2C_BUS, DM37XX_I2C_BUS} bus_type;
	phys_bytes mr_base;
	phys_bytes mr_size;
	vir_bytes mapped_addr;
	omap_i2c_regs_t *regs;
	uint32_t functional_clock;
	uint32_t module_clock;
	uint32_t bus_speed;
	uint16_t major;
	uint16_t minor;
	int irq;
	int irq_hook_id;
	int irq_hook_kernel_id;
} omap_i2c_bus_t;

/* Define the registers for each chip */

static omap_i2c_regs_t am335x_i2c_regs = {
	.I2C_REVNB_LO = AM335X_I2C_REVNB_LO,
	.I2C_REVNB_HI = AM335X_I2C_REVNB_HI,
	.I2C_SYSC = AM335X_I2C_SYSC,
	.I2C_IRQSTATUS_RAW = AM335X_I2C_IRQSTATUS_RAW,
	.I2C_IRQSTATUS = AM335X_I2C_IRQSTATUS,
	.I2C_IRQENABLE_SET = AM335X_I2C_IRQENABLE_SET,
	.I2C_IRQENABLE_CLR = AM335X_I2C_IRQENABLE_CLR,
	.I2C_WE = AM335X_I2C_WE,
	.I2C_DMARXENABLE_SET = AM335X_I2C_DMARXENABLE_SET,
	.I2C_DMATXENABLE_SET = AM335X_I2C_DMATXENABLE_SET,
	.I2C_DMARXENABLE_CLR = AM335X_I2C_DMARXENABLE_CLR,
	.I2C_DMATXENABLE_CLR = AM335X_I2C_DMATXENABLE_CLR,
	.I2C_DMARXWAKE_EN = AM335X_I2C_DMARXWAKE_EN,
	.I2C_DMATXWAKE_EN = AM335X_I2C_DMATXWAKE_EN,
	.I2C_SYSS = AM335X_I2C_SYSS,
	.I2C_BUF = AM335X_I2C_BUF,
	.I2C_CNT = AM335X_I2C_CNT,
	.I2C_DATA = AM335X_I2C_DATA,
	.I2C_CON = AM335X_I2C_CON,
	.I2C_OA = AM335X_I2C_OA,
	.I2C_SA = AM335X_I2C_SA,
	.I2C_PSC = AM335X_I2C_PSC,
	.I2C_SCLL = AM335X_I2C_SCLL,
	.I2C_SCLH = AM335X_I2C_SCLH,
	.I2C_SYSTEST = AM335X_I2C_SYSTEST,
	.I2C_BUFSTAT = AM335X_I2C_BUFSTAT,
	.I2C_OA1 = AM335X_I2C_OA1,
	.I2C_OA2 = AM335X_I2C_OA2,
	.I2C_OA3 = AM335X_I2C_OA3,
	.I2C_ACTOA = AM335X_I2C_ACTOA,
	.I2C_SBLOCK = AM335X_I2C_SBLOCK
};

static omap_i2c_regs_t dm37xx_i2c_regs = {
	.I2C_REV = DM37XX_I2C_REV,
	.I2C_IE = DM37XX_I2C_IE,
	.I2C_STAT = DM37XX_I2C_STAT,
	.I2C_WE = DM37XX_I2C_WE,
	.I2C_SYSS = DM37XX_I2C_SYSS,
	.I2C_BUF = DM37XX_I2C_BUF,
	.I2C_CNT = DM37XX_I2C_CNT,
	.I2C_DATA = DM37XX_I2C_DATA,
	.I2C_SYSC = DM37XX_I2C_SYSC,
	.I2C_CON = DM37XX_I2C_CON,
	.I2C_OA0 = DM37XX_I2C_OA0,
	.I2C_SA = DM37XX_I2C_SA,
	.I2C_PSC = DM37XX_I2C_PSC,
	.I2C_SCLL = DM37XX_I2C_SCLL,
	.I2C_SCLH = DM37XX_I2C_SCLH,
	.I2C_SYSTEST = DM37XX_I2C_SYSTEST,
	.I2C_BUFSTAT = DM37XX_I2C_BUFSTAT,
	.I2C_OA1 = DM37XX_I2C_OA1,
	.I2C_OA2 = DM37XX_I2C_OA2,
	.I2C_OA3 = DM37XX_I2C_OA3,
	.I2C_ACTOA = DM37XX_I2C_ACTOA,
	.I2C_SBLOCK = DM37XX_I2C_SBLOCK
};

/* Define the buses available on each chip */

static omap_i2c_bus_t am335x_i2c_buses[] = {
	{AM335X_I2C_BUS, AM335X_I2C0_BASE, AM335X_I2C0_SIZE, 0, &am335x_i2c_regs,
		    AM335X_FUNCTIONAL_CLOCK, AM335X_MODULE_CLOCK,
		    BUS_SPEED_400KHz, AM335X_REV_MAJOR, AM335X_REV_MINOR,
	    AM335X_I2C0_IRQ, 1, 1},
	{AM335X_I2C_BUS, AM335X_I2C1_BASE, AM335X_I2C1_SIZE, 0, &am335x_i2c_regs,
		    AM335X_FUNCTIONAL_CLOCK, AM335X_MODULE_CLOCK,
		    BUS_SPEED_100KHz, AM335X_REV_MAJOR, AM335X_REV_MINOR,
	    AM335X_I2C1_IRQ, 2, 3},
	{AM335X_I2C_BUS, AM335X_I2C2_BASE, AM335X_I2C2_SIZE, 0, &am335x_i2c_regs,
		    AM335X_FUNCTIONAL_CLOCK, AM335X_MODULE_CLOCK,
		    BUS_SPEED_100KHz, AM335X_REV_MAJOR, AM335X_REV_MINOR,
	    AM335X_I2C2_IRQ, 3, 3}
};

#define AM335X_OMAP_NBUSES (sizeof(am335x_i2c_buses) / sizeof(omap_i2c_bus_t))

static omap_i2c_bus_t dm37xx_i2c_buses[] = {
	{DM37XX_I2C_BUS, DM37XX_I2C0_BASE, DM37XX_I2C0_SIZE, 0, &dm37xx_i2c_regs,
		    DM37XX_FUNCTIONAL_CLOCK, DM37XX_MODULE_CLOCK,
		    BUS_SPEED_100KHz, DM37XX_REV_MAJOR, DM37XX_REV_MINOR,
	    DM37XX_I2C0_IRQ, 1, 1},
	{DM37XX_I2C_BUS, DM37XX_I2C1_BASE, DM37XX_I2C1_SIZE, 0, &dm37xx_i2c_regs,
		    DM37XX_FUNCTIONAL_CLOCK, DM37XX_MODULE_CLOCK,
		    BUS_SPEED_100KHz, DM37XX_REV_MAJOR, DM37XX_REV_MINOR,
	    DM37XX_I2C1_IRQ, 2, 2},
	{DM37XX_I2C_BUS, DM37XX_I2C2_BASE, DM37XX_I2C2_SIZE, 0, &dm37xx_i2c_regs,
		    DM37XX_FUNCTIONAL_CLOCK, DM37XX_MODULE_CLOCK,
		    BUS_SPEED_100KHz, DM37XX_REV_MAJOR, DM37XX_REV_MINOR,
	    DM37XX_I2C2_IRQ, 3, 3}
};

#define DM37XX_OMAP_NBUSES (sizeof(dm37xx_i2c_buses) / sizeof(omap_i2c_bus_t))

/* Globals */

static omap_i2c_bus_t *omap_i2c_buses;	/* all available buses for this SoC */
static omap_i2c_bus_t *omap_i2c_bus;	/* the bus selected at start-up */
static int omap_i2c_nbuses;	/* number of buses supported by SoC */

/* logging - use with log_warn(), log_info(), log_debug(), log_trace() */
static struct log log = {
	.name = "i2c",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* Local Function Prototypes */

/* Implementation of Generic I2C Interface using Bus Specific Code */
static int omap_i2c_process(minix_i2c_ioctl_exec_t * m);

/* Bus Specific Code */
static void omap_i2c_flush(void);
static uint16_t omap_i2c_poll(uint16_t mask);
static int omap_i2c_bus_is_free(void);
static int omap_i2c_soft_reset(void);
static void omap_i2c_bus_init(void);
static void omap_i2c_padconf(int i2c_bus_id);
static void omap_i2c_clkconf(int i2c_bus_id);
static void omap_i2c_intr_enable(void);
static uint16_t omap_i2c_read_status(void);
static void omap_i2c_write_status(uint16_t mask);
static int omap_i2c_read(i2c_addr_t addr, uint8_t * buf, size_t buflen,
    int dostop);
static int omap_i2c_write(i2c_addr_t addr, const uint8_t * buf, size_t buflen,
    int dostop);

/*
 * Performs the action in minix_i2c_ioctl_exec_t.
 */
static int
omap_i2c_process(minix_i2c_ioctl_exec_t * ioctl_exec)
{
	int r;

	/*
	 * Zero data bytes transfers are not allowed. The controller treats
	 * I2C_CNT register value of 0x0 as 65536. This is true for both the
	 * am335x and dm37xx. Full details in the TRM on the I2C_CNT page.
	 */
	if (ioctl_exec->iie_buflen == 0) {
		return EINVAL;
	}

	omap_i2c_flush();	/* clear any garbage in the fifo */

	/* Check bus busy flag before using the bus */
	r = omap_i2c_bus_is_free();
	if (r == 0) {
		log_warn(&log, "Bus is busy\n");
		return EBUSY;
	}

	if (ioctl_exec->iie_cmdlen > 0) {
		r = omap_i2c_write(ioctl_exec->iie_addr, ioctl_exec->iie_cmd,
		    ioctl_exec->iie_cmdlen,
		    !(I2C_OP_READ_P(ioctl_exec->iie_op)));
		if (r != OK) {
			omap_i2c_soft_reset();
			omap_i2c_bus_init();
			return r;
		}
	}

	if (I2C_OP_READ_P(ioctl_exec->iie_op)) {
		r = omap_i2c_read(ioctl_exec->iie_addr, ioctl_exec->iie_buf,
		    ioctl_exec->iie_buflen, I2C_OP_STOP_P(ioctl_exec->iie_op));
	} else {
		r = omap_i2c_write(ioctl_exec->iie_addr, ioctl_exec->iie_buf,
		    ioctl_exec->iie_buflen, I2C_OP_STOP_P(ioctl_exec->iie_op));
	}

	if (r != OK) {
		omap_i2c_soft_reset();
		omap_i2c_bus_init();
		return r;
	}

	return OK;
}

/*
 * Drain the incoming FIFO.
 *
 * Usually called to clear any garbage that may be in the buffer before
 * doing a read.
 */
static void
omap_i2c_flush(void)
{
	int tries;
	int status;

	for (tries = 0; tries < 1000; tries++) {
		status = omap_i2c_poll(1 << RRDY);
		if ((status & (1 << RRDY)) != 0) {	/* bytes available for reading */

			/* consume the byte and throw it away */
			(void) read16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_DATA);

			/* clear the read ready flag */
			omap_i2c_write_status(1 << RRDY);

		} else {
			break;	/* buffer drained */
		}
	}
}

/*
 * Poll the status register checking the bits set in 'mask'.
 * Returns the status if any bits set or 0x0000 when the timeout is reached.
 */
static uint16_t
omap_i2c_poll(uint16_t mask)
{
	spin_t spin;
	uint16_t status;

	/* poll for up to 1 s */
	spin_init(&spin, 1000000);
	do {
		status = omap_i2c_read_status();
		if ((status & mask) != 0) {	/* any bits in mask set */
			return status;
		}

	} while (spin_check(&spin));

	return status;		/* timeout reached, abort */
}

/*
 * Poll Bus Busy Flag until the bus becomes free (return 1) or the timeout
 * expires (return 0).
 */
static int
omap_i2c_bus_is_free(void)
{
	spin_t spin;
	uint16_t status;

	/* wait for up to 1 second for the bus to become free */
	spin_init(&spin, 1000000);
	do {

		status = omap_i2c_read_status();
		if ((status & (1 << BB)) == 0) {
			return 1;	/* bus is free */
		}

	} while (spin_check(&spin));

	return 0;		/* timeout expired */
}

static void
omap_i2c_clkconf(int i2c_bus_id)
{
	clkconf_init();

	if (omap_i2c_bus->bus_type == DM37XX_I2C_BUS) {

		clkconf_set(CM_ICLKEN1_CORE, BIT((15 + i2c_bus_id)),
		    0xffffffff);
		clkconf_set(CM_FCLKEN1_CORE, BIT((15 + i2c_bus_id)),
		    0xffffffff);

	} else if (omap_i2c_bus->bus_type == AM335X_I2C_BUS) {

		switch (i2c_bus_id) {
		case 0:
			clkconf_set(CM_WKUP_I2C0_CLKCTRL, BIT(1), 0xffffffff);
			break;
		case 1:
			clkconf_set(CM_PER_I2C1_CLKCTRL, BIT(1), 0xffffffff);
			break;
		case 2:
			clkconf_set(CM_PER_I2C2_CLKCTRL, BIT(1), 0xffffffff);
			break;
		default:
			log_warn(&log, "Invalid i2c_bus_id\n");
			break;
		}
	}

	clkconf_release();
}

static void
omap_i2c_padconf(int i2c_bus_id)
{
	int r;
	u32_t pinopts;

	if (omap_i2c_bus->bus_type == AM335X_I2C_BUS) {

		/* use the options suggested in starterware driver */
		pinopts =
		    CONTROL_CONF_SLEWCTRL | CONTROL_CONF_RXACTIVE |
		    CONTROL_CONF_PUTYPESEL;

		switch (i2c_bus_id) {
		case 0:
			pinopts |= CONTROL_CONF_MUXMODE(0);

			r = sys_padconf(CONTROL_CONF_I2C0_SDA, 0xffffffff,
			    pinopts);
			if (r != OK) {
				log_warn(&log, "padconf failed (r=%d)\n", r);
			}

			r = sys_padconf(CONTROL_CONF_I2C0_SCL, 0xffffffff,
			    pinopts);
			if (r != OK) {
				log_warn(&log, "padconf failed (r=%d)\n", r);
			}

			log_debug(&log, "pinopts=0x%x\n", pinopts);
			break;

		case 1:
			pinopts |= CONTROL_CONF_MUXMODE(2);

			r = sys_padconf(CONTROL_CONF_SPI0_CS0, 0xffffffff,
			    pinopts);
			if (r != OK) {
				log_warn(&log, "padconf failed (r=%d)\n", r);
			}

			r = sys_padconf(CONTROL_CONF_SPI0_D1, 0xffffffff,
			    pinopts);
			if (r != OK) {
				log_warn(&log, "padconf failed (r=%d)\n", r);
			}
			log_debug(&log, "pinopts=0x%x\n", pinopts);
			break;

		case 2:
			pinopts |= CONTROL_CONF_MUXMODE(3);

			r = sys_padconf(CONTROL_CONF_UART1_CTSN, 0xffffffff,
			    pinopts);
			if (r != OK) {
				log_warn(&log, "padconf failed (r=%d)\n", r);
			}

			r = sys_padconf(CONTROL_CONF_UART1_RTSN,
			    0xffffffff, pinopts);
			if (r != OK) {
				log_warn(&log, "padconf failed (r=%d)\n", r);
			}

			log_debug(&log, "pinopts=0x%x\n", pinopts);
			break;

		default:
			log_warn(&log, "Invalid i2c_bus_id\n");
			break;
		}
	}

	/* nothing to do for the DM37XX */
}

static int
omap_i2c_soft_reset(void)
{
	spin_t spin;

	/* Disable to do soft reset */
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_CON, 0);
	micro_delay(50000);

	/* Do a soft reset */
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_SYSC, (1 << SRST));

	/* Have to temporarily enable I2C to read RDONE */
	set16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_CON, (1<<I2C_EN), (1<<I2C_EN));
	micro_delay(50000);

	/* wait up to 3 seconds for reset to complete */
	spin_init(&spin, 3000000);
	do {
		if (read16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_SYSS) & (1 << RDONE)) {
			return OK;
		}

	} while (spin_check(&spin));

	log_warn(&log, "Tried soft reset, but bus never came back.\n");
	return EIO;
}

static void
omap_i2c_intr_enable(void)
{
	int r;
	uint16_t intmask;
	static int policy_set = 0;
	static int enabled = 0;

	if (!policy_set) {
		r = sys_irqsetpolicy(omap_i2c_bus->irq, 0,
		    &omap_i2c_bus->irq_hook_kernel_id);
		if (r == OK) {
			policy_set = 1;
		} else {
			log_warn(&log, "Couldn't set irq policy\n");
		}
	}

	if (policy_set && !enabled) {
		r = sys_irqenable(&omap_i2c_bus->irq_hook_kernel_id);
		if (r == OK) {
			enabled = 1;
		} else {
			log_warn(&log, "Couldn't enable irq %d (hooked)\n",
			    omap_i2c_bus->irq);
		}
	}

	/* According to NetBSD driver and u-boot, these are needed even
	 * if just using polling (i.e. non-interrupt driver programming).
	 */
	intmask = 0;
	intmask |= (1 << ROVR);
	intmask |= (1 << AERR);
	intmask |= (1 << XRDY);
	intmask |= (1 << RRDY);
	intmask |= (1 << ARDY);
	intmask |= (1 << NACK);
	intmask |= (1 << AL);

	if (omap_i2c_bus->bus_type == AM335X_I2C_BUS) {
		write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_IRQENABLE_SET, intmask);
	} else if (omap_i2c_bus->bus_type == DM37XX_I2C_BUS) {
		write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_IE, intmask);
	} else {
		log_warn(&log, "Don't know how to enable interrupts.\n");
	}
}

static void
omap_i2c_bus_init(void)
{

	/* Ensure i2c module is disabled before setting prescalar & bus speed */
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_CON, 0);
	micro_delay(50000);

	/* Disable autoidle */
	set16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_SYSC, (1<<AUTOIDLE), (0<<AUTOIDLE));

	/* Set prescalar to obtain 12 MHz i2c module clock */
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_PSC,
	    ((omap_i2c_bus->functional_clock / omap_i2c_bus->module_clock) -
		1));

	/* Set the bus speed */
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_SCLL,
	    ((omap_i2c_bus->module_clock / (2 * omap_i2c_bus->bus_speed)) -
		7));
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_SCLH,
	    ((omap_i2c_bus->module_clock / (2 * omap_i2c_bus->bus_speed)) -
		5));

	/* Set own I2C address */
	if (omap_i2c_bus->bus_type == AM335X_I2C_BUS) {
		write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_OA, I2C_OWN_ADDRESS);
	} else if (omap_i2c_bus->bus_type == DM37XX_I2C_BUS) {
		write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_OA0, I2C_OWN_ADDRESS);
	} else {
		log_warn(&log, "Don't know how to set own address.\n");
	}

	/* Set TX/RX Threshold to 1 and disable I2C DMA */
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_BUF, 0x0000);

	/* Bring the i2c module out of reset */
	set16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_CON, (1<<I2C_EN), (1<<I2C_EN));
	micro_delay(50000);

	/*
	 * Enable interrupts
	 */
	omap_i2c_intr_enable();
}

static uint16_t
omap_i2c_read_status(void)
{
	uint16_t status = 0x0000;

	if (omap_i2c_bus->bus_type == AM335X_I2C_BUS) {
		/* TRM says to use RAW for polling for events */
		status = read16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_IRQSTATUS_RAW);
	} else if (omap_i2c_bus->bus_type == DM37XX_I2C_BUS) {
		status = read16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_STAT);
	} else {
		log_warn(&log, "Don't know how to read i2c bus status.\n");
	}

	return status;
}

static void
omap_i2c_write_status(uint16_t mask)
{
	if (omap_i2c_bus->bus_type == AM335X_I2C_BUS) {
		/* write 1's to IRQSTATUS (not RAW) to clear the bits */
		write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_IRQSTATUS, mask);
	} else if (omap_i2c_bus->bus_type == DM37XX_I2C_BUS) {
		write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_STAT, mask);
	} else {
		log_warn(&log, "Don't know how to clear i2c bus status.\n");
	}
}

static int
omap_i2c_read(i2c_addr_t addr, uint8_t * buf, size_t buflen, int dostop)
{
	int r, i;
	uint16_t conopts;
	uint16_t pollmask;
	uint16_t errmask;

	/* Set address of slave device */
	conopts = 0;
	addr &= MAX_I2C_SA_MASK;	/* sanitize address (10-bit max) */
	if (addr > 0x7f) {
		/* 10-bit extended address in use, need to set XSA */
		conopts |= (1 << XSA);
	}

	errmask = 0;
	errmask |= (1 << ROVR);
	errmask |= (1 << AERR);
	errmask |= (1 << NACK);
	errmask |= (1 << AL);

	pollmask = 0;
	pollmask |= (1 << RRDY);

	/* Set bytes to read and slave address */
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_CNT, buflen);
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_SA, addr);

	/* Set control register */
	conopts |= (1 << I2C_EN);	/* enabled */
	conopts |= (1 << MST);	/* master mode */
	conopts |= (1 << STT);	/* start condition */

	if (dostop != 0) {
		conopts |= (1 << STP);	/* stop condition */
	}

	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_CON, conopts);

	for (i = 0; i < buflen; i++) {
		/* Data to read? */
		r = omap_i2c_poll(pollmask | errmask);
		if ((r & errmask) != 0) {
			/* only debug log level because i2cscan trigers this */
			log_debug(&log, "Read Error! Status=%x\n", r);
			return EIO;
		} else if ((r & pollmask) == 0) {
			log_warn(&log, "No RRDY Interrupt. Status=%x\n", r);
			log_warn(&log,
			    "Likely cause: bad pinmux or no devices on bus\n");
			return EBUSY;
		}

		/* read a byte */
		buf[i] = read16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_DATA) & 0xff;

		/* clear the read ready flag */
		omap_i2c_write_status(pollmask);
	}

	r = omap_i2c_read_status();
	if ((r & (1 << NACK)) != 0) {
		log_warn(&log, "NACK\n");
		return EIO;
	}

	/* Wait for operation to complete */
	pollmask = (1<<ARDY); /* poll access ready bit */
	r = omap_i2c_poll(pollmask);
	if ((r & pollmask) == 0) {
		log_warn(&log, "Read operation never finished.\n");
		return EBUSY;
	}
	omap_i2c_write_status(0x7fff);

	return 0;
}

static int
omap_i2c_write(i2c_addr_t addr, const uint8_t * buf, size_t buflen, int dostop)
{
	int r, i;
	uint16_t conopts;
	uint16_t pollmask;
	uint16_t errmask;

	/* Set address of slave device */
	conopts = 0;
	addr &= MAX_I2C_SA_MASK;	/* sanitize address (10-bit max) */
	if (addr > 0x7f) {
		/* 10-bit extended address in use, need to set XSA */
		conopts |= (1 << XSA);
	}

	pollmask = 0;
	pollmask |= (1 << XRDY);

	errmask = 0;
	errmask |= (1 << ROVR);
	errmask |= (1 << AERR);
	errmask |= (1 << NACK);
	errmask |= (1 << AL);

	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_CNT, buflen);
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_SA, addr);

	/* Set control register */
	conopts |= (1 << I2C_EN);	/* enabled */
	conopts |= (1 << MST);	/* master mode */
	conopts |= (1 << TRX);	/* TRX mode */
	conopts |= (1 << STT);	/* start condition */

	if (dostop != 0) {
		conopts |= (1 << STP);	/* stop condition */
	}

	omap_i2c_write_status(0x7fff);
	write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_CON, conopts);

	for (i = 0; i < buflen; i++) {

		/* Ready to write? */
		r = omap_i2c_poll(pollmask | errmask);
		if ((r & errmask) != 0) {
			log_warn(&log, "Write Error! Status=%x\n", r);
			return EIO;
		} else if ((r & pollmask) == 0) {
			log_warn(&log, "Not ready for write? Status=%x\n", r);
			return EBUSY;
		}

		write16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_DATA, buf[i]);

		/* clear the write ready flag */
		omap_i2c_write_status(pollmask);
	}

	r = omap_i2c_read_status();
	if ((r & (1 << NACK)) != 0) {
		log_warn(&log, "NACK\n");
		return EIO;
	}

	/* Wait for operation to complete */
	pollmask = (1<<ARDY); /* poll access ready bit */
	r = omap_i2c_poll(pollmask);
	if ((r & pollmask) == 0) {
		log_warn(&log, "Write operation never finished.\n");
		return EBUSY;
	}
	omap_i2c_write_status(0x7fff);

	return 0;
}

int
omap_interface_setup(int (**process) (minix_i2c_ioctl_exec_t * ioctl_exec),
    int i2c_bus_id)
{
	int r;
	int i2c_rev, major, minor;
	struct minix_mem_range mr;
	struct machine machine;
	sys_getmachine(&machine);

	/* Fill in the function pointer */

	*process = omap_i2c_process;

	/* Select the correct i2c definition for this SoC */

	if (BOARD_IS_BBXM(machine.board_id)){
		omap_i2c_buses = dm37xx_i2c_buses;
		omap_i2c_nbuses = DM37XX_OMAP_NBUSES;
	} else if (BOARD_IS_BB(machine.board_id)){
		omap_i2c_buses = am335x_i2c_buses;
		omap_i2c_nbuses = AM335X_OMAP_NBUSES;
	} else {
		return EINVAL;
	}

	if (i2c_bus_id < 0 || i2c_bus_id >= omap_i2c_nbuses) {
		return EINVAL;
	}

	/* select the bus to operate on */
	omap_i2c_bus = &omap_i2c_buses[i2c_bus_id];

	/* Configure Pins */
	omap_i2c_padconf(i2c_bus_id);

	/*
	 * Map I2C Registers
	 */

	/* Configure memory access */
	mr.mr_base = omap_i2c_bus->mr_base;	/* start addr */
	mr.mr_limit = mr.mr_base + omap_i2c_bus->mr_size;	/* end addr */

	/* ask for privileges to access the I2C memory range */
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
		panic("Unable to obtain i2c memory range privileges");
	}

	/* map the memory into this process */
	omap_i2c_bus->mapped_addr = (vir_bytes) vm_map_phys(SELF,
	    (void *) omap_i2c_bus->mr_base, omap_i2c_bus->mr_size);

	if (omap_i2c_bus->mapped_addr == (vir_bytes) MAP_FAILED) {
		panic("Unable to map i2c registers");
	}

	/* Enable Clocks */
	omap_i2c_clkconf(i2c_bus_id);

	/* Perform a soft reset of the I2C module to ensure a fresh start */
	r = omap_i2c_soft_reset();
	if (r != OK) {
		/* module didn't come back up :( */
		return r;
	}

	/* Bring up I2C module */
	omap_i2c_bus_init();

	/* Get I2C Revision */
	if (omap_i2c_bus->bus_type == AM335X_I2C_BUS) {
		/* I2C_REVLO revision: major (bits 10-8), minor (bits 5-0) */
		i2c_rev = read16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_REVNB_LO);
		major = (i2c_rev >> 8) & 0x07;
		minor = i2c_rev & 0x3f;

	} else if (omap_i2c_bus->bus_type == DM37XX_I2C_BUS) {
		/* I2C_REV revision: major (bits 7-4), minor (bits 3-0) */
		i2c_rev = read16(omap_i2c_bus->mapped_addr + omap_i2c_bus->regs->I2C_REV);
		major = (i2c_rev >> 4) & 0x0f;
		minor = i2c_rev & 0x0f;
	} else {
		panic("Don't know how to read i2c revision.");
	}

	if (major != omap_i2c_bus->major || minor != omap_i2c_bus->minor) {
		log_warn(&log, "Unrecognized value in I2C_REV register.\n");
		log_warn(&log, "Read: 0x%x.0x%x | Expected: 0x%x.0x%x\n",
		    major, minor, omap_i2c_bus->major, omap_i2c_bus->minor);
	}

	/* display i2c revision information for debugging purposes */
	log_debug(&log, "i2c_%d: I2C rev 0x%x.0x%x\n", (i2c_bus_id + 1),
	    major, minor);

	return OK;
}
