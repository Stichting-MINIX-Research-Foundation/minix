/* kernel headers */
#include <minix/syslib.h>
#include <minix/drvlib.h>
#include <minix/log.h>
#include <minix/mmio.h>
#include <minix/gpio.h>
#include <minix/clkconf.h>
#include <minix/type.h>
#include <minix/board.h>

/* system headers */
#include <sys/mman.h>
#include <sys/types.h>

/* usr headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

/* local headers */
#include "gpio_omap.h"

/* used for logging */
static struct log log = {
	.name = "gpio_omap",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

struct gpio_driver
{
	/* request access to a gpio */
	int (*claim) (char *owner, int nr, struct gpio ** gpio);

	/* Configure the GPIO for a certain purpose */
	int (*pin_mode) (struct gpio * gpio, int mode);

	/* Set the value for a GPIO */
	int (*set) (struct gpio * gpio, int value);

	/* Read the current value of the GPIO */
	int (*read) (struct gpio * gpio, int *value);

	/* Read and clear the value interrupt value of the GPIO */
	int (*intr_read) (struct gpio * gpio, int *value);

	/* Interrupt hook */
	int (*message_hook) (message * m);
};

static struct gpio_driver drv;

struct omap_gpio_bank
{
	const char *name;
	uint32_t register_address;
	uint32_t irq_nr;	/* irq number */
	uint32_t base_address;
	int32_t disabled;
	int irq_id;		/* original hook id??? */
	int irq_hook_id;	/* hook id */
	uint32_t inter_values;	/* values when the interrupt was called */
};

static struct omap_gpio_bank *omap_gpio_banks;

static struct omap_gpio_bank am335x_gpio_banks[] = {
	{
		    .name = "GPIO0",
		    .register_address = AM335X_GPIO0_BASE,
		    .irq_nr = AM335X_GPIO0A_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = AM335X_GPIO0A_IRQ_HOOK_ID,
		    .irq_hook_id = AM335X_GPIO0A_IRQ_HOOK_ID,

	    },
	{
		    .name = "GPIO1",
		    .register_address = AM335X_GPIO1_BASE,
		    .irq_nr = AM335X_GPIO1A_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = AM335X_GPIO1A_IRQ_HOOK_ID,
		    .irq_hook_id = AM335X_GPIO1A_IRQ_HOOK_ID,

	    },
	{
		    .name = "GPIO2",
		    .register_address = AM335X_GPIO2_BASE,
		    .irq_nr = AM335X_GPIO2A_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = AM335X_GPIO2A_IRQ_HOOK_ID,
		    .irq_hook_id = AM335X_GPIO2A_IRQ_HOOK_ID,

	    },
	{
		    .name = "GPIO3",
		    .register_address = AM335X_GPIO3_BASE,
		    .irq_nr = AM335X_GPIO3A_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = AM335X_GPIO3A_IRQ_HOOK_ID,
		    .irq_hook_id = AM335X_GPIO3A_IRQ_HOOK_ID,

	    },
	{NULL, 0, 0, 0, 0, 0, 0, 0 }
};

static struct omap_gpio_bank dm37xx_gpio_banks[] = {
	{
		    .name = "GPIO1",
		    .register_address = DM37XX_GPIO1_BASE,
		    .irq_nr = DM37XX_GPIO1_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = DM37XX_GPIO1_IRQ_HOOK_ID,
		    .irq_hook_id = DM37XX_GPIO1_IRQ_HOOK_ID,
	    },
	{
		    .name = "GPIO2",
		    .register_address = DM37XX_GPIO2_BASE,
		    .irq_nr = DM37XX_GPIO2_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = DM37XX_GPIO2_IRQ_HOOK_ID,
		    .irq_hook_id = DM37XX_GPIO2_IRQ_HOOK_ID,
	    },
	{
		    .name = "GPIO3",
		    .register_address = DM37XX_GPIO3_BASE,
		    .irq_nr = DM37XX_GPIO3_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = DM37XX_GPIO3_IRQ_HOOK_ID,
		    .irq_hook_id = DM37XX_GPIO3_IRQ_HOOK_ID,
	    },
	{
		    .name = "GPIO4",
		    .register_address = DM37XX_GPIO4_BASE,
		    .irq_nr = DM37XX_GPIO4_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = DM37XX_GPIO4_IRQ_HOOK_ID,
		    .irq_hook_id = DM37XX_GPIO4_IRQ_HOOK_ID,
	    },
	{
		    .name = "GPIO5",
		    .register_address = DM37XX_GPIO5_BASE,
		    .irq_nr = DM37XX_GPIO5_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = DM37XX_GPIO5_IRQ_HOOK_ID,
		    .irq_hook_id = DM37XX_GPIO5_IRQ_HOOK_ID,
	    },
	{
		    .name = "GPIO6",
		    .register_address = DM37XX_GPIO6_BASE,
		    .irq_nr = DM37XX_GPIO6_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = DM37XX_GPIO6_IRQ_HOOK_ID,
		    .irq_hook_id = DM37XX_GPIO6_IRQ_HOOK_ID,
	    },
	{NULL, 0, 0, 0, 0, 0, 0, 0 }
};

static int nbanks; /* number of banks */

/*
 * Defines the set of registers. There is a lot of commonality between the
 * AM335X and DM37XX gpio registers. To avoid ifdefs everywhere, we define
 * a central register set and only use ifdefs where they differ.
 */
typedef struct gpio_omap_registers {
	vir_bytes REVISION;
	vir_bytes IRQENABLE;
	vir_bytes IRQSTATUS;
	vir_bytes DATAOUT;
	vir_bytes DATAIN;
	vir_bytes OE;
	vir_bytes RISINGDETECT;
	vir_bytes FALLINGDETECT;
	vir_bytes CLEARDATAOUT;
	vir_bytes SETDATAOUT;
} gpio_omap_regs_t;

/* Define the registers for each chip */

gpio_omap_regs_t gpio_omap_dm37xx = {
	.REVISION = DM37XX_GPIO_REVISION,
	.IRQENABLE = DM37XX_GPIO_IRQENABLE1,
	.IRQSTATUS = DM37XX_GPIO_IRQSTATUS1,
	.DATAOUT = DM37XX_GPIO_DATAOUT,
	.DATAIN = DM37XX_GPIO_DATAIN,
	.OE = DM37XX_GPIO_OE,
	.RISINGDETECT = DM37XX_GPIO_RISINGDETECT1,
	.FALLINGDETECT = DM37XX_GPIO_FALLINGDETECT1,
	.CLEARDATAOUT = DM37XX_GPIO_CLEARDATAOUT,
	.SETDATAOUT = DM37XX_GPIO_SETDATAOUT
};

gpio_omap_regs_t gpio_omap_am335x = {
	.REVISION = AM335X_GPIO_REVISION,
	.IRQENABLE = AM335X_GPIO_IRQSTATUS_SET_0,
	.IRQSTATUS = AM335X_GPIO_IRQSTATUS_0,
	.DATAOUT = AM335X_GPIO_DATAOUT,
	.DATAIN = AM335X_GPIO_DATAIN,
	.OE = AM335X_GPIO_OE,
	.RISINGDETECT = AM335X_GPIO_RISINGDETECT,
	.FALLINGDETECT = AM335X_GPIO_FALLINGDETECT,
	.CLEARDATAOUT = AM335X_GPIO_CLEARDATAOUT,
	.SETDATAOUT = AM335X_GPIO_SETDATAOUT
};

static gpio_omap_regs_t *regs;


static struct omap_gpio_bank *
omap_gpio_bank_get(int gpio_nr)
{
	struct omap_gpio_bank *bank;
	assert(gpio_nr >= 0 && gpio_nr <= 32 * nbanks);
	bank = &omap_gpio_banks[gpio_nr / 32];
	return bank;
}

static int
omap_gpio_claim(char *owner, int nr, struct gpio **gpio)
{
	log_trace(&log, "%s s claiming %d\n", owner, nr);

	if (nr < 0 && nr >= 32 * nbanks) {
		log_warn(&log, "%s is claiming unknown GPIO number %d\n",
		    owner, nr);
		return EINVAL;
	}

	if (omap_gpio_bank_get(nr)->disabled == 1) {
		log_warn(&log, "%s is claiming GPIO %d from disabled bank\n",
		    owner, nr);
		return EINVAL;
	}

	struct gpio *tmp = malloc(sizeof(struct gpio));
	memset(tmp, 0, sizeof(*tmp));

	tmp->nr = nr;
	*gpio = tmp;
	return OK;
}

static int
omap_gpio_pin_mode(struct gpio *gpio, int mode)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	gpio->mode = mode;

	bank = omap_gpio_bank_get(gpio->nr);
	log_debug(&log,
	    "pin mode bank %s, base address 0x%x -> register address (0x%x,0x%x,0x%x)\n",
	    bank->name, bank->base_address, bank->register_address, regs->OE,
	    bank->register_address + regs->OE);

	if (mode == GPIO_MODE_OUTPUT) {
		set32(bank->base_address + regs->OE, BIT(gpio->nr % 32), 0);
	} else {
		set32(bank->base_address + regs->FALLINGDETECT,
		    BIT(gpio->nr % 32), 0xffffffff);
		set32(bank->base_address + regs->IRQENABLE, BIT(gpio->nr % 32),
		    0xffffffff);
		set32(bank->base_address + regs->OE, BIT(gpio->nr % 32),
		    0xffffffff);
	}
	return 0;
}

static int
omap_gpio_set(struct gpio *gpio, int value)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	assert(gpio->nr >= 0 && gpio->nr <= 32 * nbanks);

	bank = omap_gpio_bank_get(gpio->nr);
	if (value == 1) {
		write32(bank->base_address + regs->SETDATAOUT,
		    BIT(gpio->nr % 32));
	} else {
		write32(bank->base_address + regs->CLEARDATAOUT,
		    BIT(gpio->nr % 32));
	}
	return OK;
}

static int
omap_gpio_read(struct gpio *gpio, int *value)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	assert(gpio->nr >= 0 && gpio->nr <= 32 * nbanks);

	bank = omap_gpio_bank_get(gpio->nr);
	log_trace(&log, "mode=%d OU/IN 0x%08x 0x%08x\n", gpio->mode,
	    read32(bank->base_address + regs->DATAIN),
	    read32(bank->base_address + regs->DATAOUT));

	if (gpio->mode == GPIO_MODE_INPUT) {
		*value =
		    (read32(bank->base_address +
			regs->DATAIN) >> (gpio->nr % 32)) & 0x1;
	} else {
		*value =
		    (read32(bank->base_address +
			regs->DATAOUT) >> (gpio->nr % 32)) & 0x1;
	}

	return OK;
}

static int
omap_gpio_intr_read(struct gpio *gpio, int *value)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	assert(gpio->nr >= 0 && gpio->nr <= 32 * nbanks);

	bank = omap_gpio_bank_get(gpio->nr);
	/* TODO: check if interrupt where enabled?? */

	*value = (bank->inter_values >> (gpio->nr % 32)) & 0x1;
	/* clear the data */
	bank->inter_values &= ~(1 << (gpio->nr % 32));

	return OK;
}

static int
omap_message_hook(message * m)
{
	unsigned long irq_set, i;
	struct omap_gpio_bank *bank;

	switch (_ENDPOINT_P(m->m_source)) {
	case HARDWARE:
		/* Hardware interrupt return a "set" if pending interrupts */
		irq_set = m->m_notify.interrupts;
		log_debug(&log, "HW message 0X%08llx\n", m->m_notify.interrupts);
		bank = &omap_gpio_banks[0];
		for (i = 0; omap_gpio_banks[i].name != NULL; i++) {
			bank = &omap_gpio_banks[i];

			if (irq_set & (1 << (bank->irq_id))) {
				log_trace(&log, "Interrupt for bank %s\n",
				    bank->name);
				bank->inter_values |=
				    read32(bank->base_address +
				    regs->IRQSTATUS);
				/* clear the interrupts */
				write32(bank->base_address + regs->IRQSTATUS,
				    0xffffffff);
				if (sys_irqenable(&bank->irq_hook_id) != OK) {
					log_warn(&log,
					    "Failed to enable irq for bank %s\n",
					    bank->name);
				}
			}
		}
		return OK;
	default:
		log_debug(&log, "Unknown message\n");
		break;
	}
	return OK;
}

static int revision_matches(u32_t board_id,u32_t rev) {
	/* figures out if the collected resition matches the one expected
	 * from the board */
	if (BOARD_IS_BBXM(board_id)){
		if(
		   DM37XX_GPIO_REVISION_MAJOR(rev) != 2
		   || DM37XX_GPIO_REVISION_MINOR(rev) !=  5
		   ) {
			return 0;
		}
	} else if (BOARD_IS_BB(board_id)){
		if (
		    AM335X_GPIO_REVISION_MAJOR(rev) != 0
		    || AM335X_GPIO_REVISION_MINOR(rev) != 1
		    ) {
			return 0;
		}
	}
	return 1;
}

static int
omap_gpio_init(struct gpio_driver *gpdrv)
{
	u32_t revision;
	int i;
	struct minix_mem_range mr;
	struct omap_gpio_bank *bank;
	struct machine machine;
	sys_getmachine(&machine);

	nbanks =0;
	omap_gpio_banks = NULL;
	if (BOARD_IS_BBXM(machine.board_id)){
		omap_gpio_banks = dm37xx_gpio_banks;
		regs = &gpio_omap_dm37xx;
	} else if (BOARD_IS_BB(machine.board_id)){
		omap_gpio_banks = am335x_gpio_banks;
		regs = &gpio_omap_am335x;
	}

	bank = &omap_gpio_banks[0];
	for (i = 0; omap_gpio_banks[i].name != NULL; i++) {
		nbanks++;
		bank = &omap_gpio_banks[i];
		mr.mr_base = bank->register_address;
		mr.mr_limit = bank->register_address + 0x400;

		if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
			log_warn(&log,
			    "Unable to request permission to map memory\n");
			return EPERM;	/* fixme */
		}

		/* Set the base address to use */
		bank->base_address =
		    (uint32_t) vm_map_phys(SELF,
		    (void *) bank->register_address, 0x400);

		if (bank->base_address == (uint32_t) MAP_FAILED) {
			log_warn(&log, "Unable to map GPIO memory\n");
			return EPERM;	/* fixme */
		}

		revision = read32(bank->base_address + regs->REVISION);
		/* test if we can access it */
		if (! revision_matches(machine.board_id,revision)) {
			log_warn(&log,
			    "Failed to read the revision of GPIO bank %s.. disabling\n",
			    bank->name);
			log_warn(&log, "Got 0x%x\n", revision);
			bank->disabled = 1;
		} else {
			bank->disabled = 0;
		}

		if (sys_irqsetpolicy(bank->irq_nr, 0,
			&bank->irq_hook_id) != OK) {
			log_warn(&log,
			    "GPIO: couldn't set IRQ policy for bank %s\n",
			    bank->name);
			continue;
		};
		if (bank->irq_id != bank->irq_hook_id) {
			log_debug(&log, "requested id %d but got id %d\n",
			    bank->irq_id, bank->irq_hook_id);
		}
		if (sys_irqenable(&bank->irq_hook_id) != OK) {
			log_warn(&log,
			    "GPIO: couldn't enable interrupt for %s\n",
			    bank->name);
		};
		log_trace(&log, "bank %s mapped on 0x%x with irq hook id %d\n",
		    bank->name, bank->base_address, bank->irq_hook_id);

	};

	clkconf_init();

	if (BOARD_IS_BBXM(machine.board_id)){
		/* enable the interface and functional clock on GPIO bank 1 , this only
		   applies to the Beagelboard XM */
		clkconf_set(CM_FCLKEN_WKUP, BIT(3), 0xffffffff);
		clkconf_set(CM_ICLKEN_WKUP, BIT(3), 0xffffffff);
	}
	clkconf_release();


	gpdrv->claim = omap_gpio_claim;
	gpdrv->pin_mode = omap_gpio_pin_mode;
	gpdrv->set = omap_gpio_set;
	gpdrv->read = omap_gpio_read;
	gpdrv->intr_read = omap_gpio_intr_read;
	gpdrv->message_hook = omap_message_hook;
	return 0;
}

int
gpio_init()
{
	return omap_gpio_init(&drv);
}

/* request access to a gpio */
int
gpio_claim(char *owner, int nr, struct gpio **gpio)
{
	return drv.claim(owner, nr, gpio);
}

/* Configure the GPIO for a certain purpose */
int
gpio_pin_mode(struct gpio *gpio, int mode)
{
	return drv.pin_mode(gpio, mode);
}

/* Set the value for a GPIO */
int
gpio_set(struct gpio *gpio, int value)
{
	return drv.set(gpio, value);
}

/* Read the current value of the GPIO */
int
gpio_read(struct gpio *gpio, int *value)
{
	return drv.read(gpio, value);
}

/* Read and clear the value interrupt value of the GPIO */
int
gpio_intr_read(struct gpio *gpio, int *value)
{
	return drv.intr_read(gpio, value);
}

/* Interrupt hook */
int
gpio_intr_message(message * m)
{
	return drv.message_hook(m);
}

int
gpio_release(void)
{
	return OK;
}
