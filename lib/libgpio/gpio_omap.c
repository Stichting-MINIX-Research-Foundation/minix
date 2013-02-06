/* kernel headers */
#include <minix/syslib.h>
#include <minix/drvlib.h>
#include <minix/log.h>
#include <minix/mmio.h>
#include <minix/gpio.h>

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

#include "clkconf.h"

/* local headers */

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
	int irq_id;		/* orignhal hook id??? */
	int irq_hook_id;	/* hook id */
	uint32_t inter_values;	/* values when the interrupt was called */
};

#define GPIO1_BASE (0x48310000)
#define GPIO2_BASE (0x49050000)
#define GPIO3_BASE (0x49052000)
#define GPIO4_BASE (0x49054000)
#define GPIO5_BASE (0x49056000)
#define GPIO6_BASE (0x49058000)
#define GPIO1_IRQ  29		/* GPIO module 1 */
#define GPIO2_IRQ  30		/* GPIO module 2 */
#define GPIO3_IRQ  31		/* GPIO module 3 */
#define GPIO4_IRQ  32		/* GPIO module 4 */
#define GPIO5_IRQ  33		/* GPIO module 5 */
#define GPIO6_IRQ  34		/* GPIO module 6 */
#define GPIO1_IRQ_HOOK_ID 0
#define GPIO2_IRQ_HOOK_ID 1
#define GPIO3_IRQ_HOOK_ID 2
#define GPIO4_IRQ_HOOK_ID 3
#define GPIO5_IRQ_HOOK_ID 4
#define GPIO6_IRQ_HOOK_ID 5

#define GPIO_IRQSTATUS1 (0x18)
#define GPIO_IRQENABLE1 (0x01C)
#define GPIO_DATAOUT (0x3c)
#define GPIO_DATAIN (0x38)
#define GPIO_OE    (0x34)	/* Output Data Enable */
#define GPIO_RISINGDETECT1 (0x048)
#define GPIO_FALLINGDETECT1 (0x04c)
#define GPIO_CLEARDATAOUT (0x90)
#define GPIO_SETDATAOUT (0x94)

static struct omap_gpio_bank omap_gpio_banks[] = {
	{
		    .name = "GPIO1",
		    .register_address = GPIO1_BASE,
		    .irq_nr = GPIO1_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = GPIO1_IRQ_HOOK_ID,
		    .irq_hook_id = GPIO1_IRQ_HOOK_ID,
	    },
	{
		    .name = "GPIO2",
		    .register_address = GPIO2_BASE,
		    .irq_nr = GPIO2_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = GPIO2_IRQ_HOOK_ID,
		    .irq_hook_id = GPIO2_IRQ_HOOK_ID,
	    },
	{
		    .name = "GPIO3",
		    .register_address = GPIO3_BASE,
		    .irq_nr = GPIO3_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = GPIO3_IRQ_HOOK_ID,
		    .irq_hook_id = GPIO3_IRQ_HOOK_ID,
	    },
	{
		    .name = "GPIO4",
		    .register_address = GPIO4_BASE,
		    .irq_nr = GPIO4_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = GPIO4_IRQ_HOOK_ID,
		    .irq_hook_id = GPIO4_IRQ_HOOK_ID,
	    },
	{
		    .name = "GPIO5",
		    .register_address = GPIO5_BASE,
		    .irq_nr = GPIO5_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = GPIO5_IRQ_HOOK_ID,
		    .irq_hook_id = GPIO5_IRQ_HOOK_ID,
	    },
	{
		    .name = "GPIO6",
		    .register_address = GPIO6_BASE,
		    .irq_nr = GPIO6_IRQ,
		    .base_address = 0,
		    .disabled = 0,
		    .irq_id = GPIO6_IRQ_HOOK_ID,
		    .irq_hook_id = GPIO6_IRQ_HOOK_ID,
	    },
	{NULL, 0, 0}
};

#define GPIO_REVISION 0x00
#define GPIO_REVISION_MAJOR(X) ((X & 0xF0) >> 4)
#define GPIO_REVISION_MINOR(X) (X & 0XF)

struct omap_gpio_bank *
omap_gpio_bank_get(int gpio_nr)
{
	struct omap_gpio_bank *bank;
	assert(gpio_nr >= 0 && gpio_nr <= 32 * 6);
	bank = &omap_gpio_banks[gpio_nr / 32];
	return bank;
}

int
omap_gpio_claim(char *owner, int nr, struct gpio **gpio)
{
	log_trace(&log, "%s s claiming %d\n", owner, nr);

	if (nr < 0 && nr >= 32 * 6) {
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

int
omap_gpio_pin_mode(struct gpio *gpio, int mode)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	gpio->mode = mode;

	bank = omap_gpio_bank_get(gpio->nr);
	log_debug(&log,
	    "pin mode bank %s, base address 0x%x -> register address (0x%x,0x%x,0x%x)\n",
	    bank->name, bank->base_address, bank->register_address, GPIO_OE,
	    bank->register_address + GPIO_OE);

	if (mode == GPIO_MODE_OUTPUT) {
		set32(bank->base_address + GPIO_OE, BIT(gpio->nr % 32), 0);
	} else {
		set32(bank->base_address + GPIO_FALLINGDETECT1,
		    BIT(gpio->nr % 32), 0xffffffff);
		set32(bank->base_address + GPIO_IRQENABLE1, BIT(gpio->nr % 32),
		    0xffffffff);
		set32(bank->base_address + GPIO_OE, BIT(gpio->nr % 32),
		    0xffffffff);
	}
	return 0;
}

int
omap_gpio_set(struct gpio *gpio, int value)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	assert(gpio->nr >= 0 && gpio->nr <= 32 * 6);

	bank = omap_gpio_bank_get(gpio->nr);
	if (value == 1) {
		write32(bank->base_address + GPIO_SETDATAOUT,
		    BIT(gpio->nr % 32));
	} else {
		write32(bank->base_address + GPIO_CLEARDATAOUT,
		    BIT(gpio->nr % 32));
	}
	return OK;
}

int
omap_gpio_read(struct gpio *gpio, int *value)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	assert(gpio->nr >= 0 && gpio->nr <= 32 * 6);

	bank = omap_gpio_bank_get(gpio->nr);
	log_trace(&log, "mode=%d OU/IN 0x%08x 0x%08x\n", gpio->mode,
	    read32(bank->base_address + GPIO_DATAIN),
	    read32(bank->base_address + GPIO_DATAOUT));

	if (gpio->mode == GPIO_MODE_INPUT) {
		*value =
		    (read32(bank->base_address +
			GPIO_DATAIN) >> (gpio->nr % 32)) & 0x1;
	} else {
		*value =
		    (read32(bank->base_address +
			GPIO_DATAOUT) >> (gpio->nr % 32)) & 0x1;
	}

	return OK;
}

int
omap_gpio_intr_read(struct gpio *gpio, int *value)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	assert(gpio->nr >= 0 && gpio->nr <= 32 * 6);

	bank = omap_gpio_bank_get(gpio->nr);
	/* TODO: check if interrupt where enabled?? */

	*value = (bank->inter_values >> (gpio->nr % 32)) & 0x1;
	/* clear the data */
	bank->inter_values &= ~(1 << (gpio->nr % 32));

	return OK;
}

int
omap_message_hook(message * m)
{
	unsigned long irq_set, i;
	struct omap_gpio_bank *bank;

	switch (_ENDPOINT_P(m->m_source)) {
	case HARDWARE:
		/* Hardware interrupt return a "set" if pending interrupts */
		irq_set = m->NOTIFY_ARG;
		log_debug(&log, "HW message 0X%08x\n", m->NOTIFY_ARG);
		bank = &omap_gpio_banks[0];
		for (i = 0; omap_gpio_banks[i].name != NULL; i++) {
			bank = &omap_gpio_banks[i];

			if (irq_set & (1 << (bank->irq_id))) {
				log_trace(&log, "Interrupt for bank %s\n",
				    bank->name);
				bank->inter_values |=
				    read32(bank->base_address +
				    GPIO_IRQSTATUS1);
				/* clear the interrupts */
				write32(bank->base_address + GPIO_IRQSTATUS1,
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
		log_warn(&log, "Unknown message\n");
		break;
	}
	return OK;
}

int
omap_gpio_init(struct gpio_driver *drv)
{
	u32_t revision;
	int i;
	struct minix_mem_range mr;
	struct omap_gpio_bank *bank;

	bank = &omap_gpio_banks[0];
	for (i = 0; omap_gpio_banks[i].name != NULL; i++) {
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

		revision = 0;
		revision = read32(bank->base_address + GPIO_REVISION);
		/* test if we can access it */
		if (GPIO_REVISION_MAJOR(revision) != 2
		    || GPIO_REVISION_MINOR(revision) != 5) {
			log_warn(&log,
			    "Failed to read the revision of GPIO bank %s.. disabling\n",
			    bank->name);
			bank->disabled = 1;
		}
		bank->disabled = 0;

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
	/* enable the interface and functional clock on GPIO bank 1 */
	clkconf_set(CM_FCLKEN_WKUP, BIT(3), 0xffffffff);
	clkconf_set(CM_ICLKEN_WKUP, BIT(3), 0xffffffff);
	clkconf_release();


	drv->claim = omap_gpio_claim;
	drv->pin_mode = omap_gpio_pin_mode;
	drv->set = omap_gpio_set;
	drv->read = omap_gpio_read;
	drv->intr_read = omap_gpio_intr_read;
	drv->message_hook = omap_message_hook;
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
gpio_release()
{
	return OK;
}
