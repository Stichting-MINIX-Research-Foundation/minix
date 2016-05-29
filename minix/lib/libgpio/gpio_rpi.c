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
#include <minix/padconf.h>

/* used for logging */
static struct log log = {
	.name = "gpio_rpi",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

static uint32_t rpi_gpio_base_address;

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

static int
rpi_gpio_claim(char *owner, int nr, struct gpio **gpio)
{
	log_trace(&log, "%s s claiming %d\n", owner, nr);

	if (nr < 0 && nr > 53) {
		log_warn(&log, "%s is claiming unknown GPIO number %d\n",
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
rpi_gpio_pin_mode(struct gpio *gpio, int mode)
{
	assert(gpio != NULL);
	gpio->mode = mode;

	log_debug(&log, "pin %d mode %d\n", gpio->nr, mode);

	int gpio_reg = gpio->nr / 10;
	int gpio_offset = (gpio->nr % 10) * 3;

	set32(rpi_gpio_base_address + 4*gpio_reg, 0x7 << gpio_offset, mode << gpio_offset);

	return 0;
}

static int
rpi_gpio_set(struct gpio *gpio, int value)
{
	assert(gpio != NULL);
	assert(gpio->nr >= 0 && gpio->nr <= 53);

	int gpio_reg = gpio->nr / 32;

	if (value == 1) {
		write32(rpi_gpio_base_address + 0x1c + 4*gpio_reg,
		    BIT(gpio->nr % 32));
	} else {
		write32(rpi_gpio_base_address + 0x28 + 4*gpio_reg,
		    BIT(gpio->nr % 32));
	}
	return OK;
}

static int
rpi_gpio_read(struct gpio *gpio, int *value)
{
	assert(gpio != NULL);
	assert(gpio->nr >= 0 && gpio->nr <= 53);

	int gpio_reg = gpio->nr / 32;

	*value = (read32(rpi_gpio_base_address + 0x34 + 4*gpio_reg) >>
	          (gpio->nr % 32)) & 0x1;

	return OK;
}

static int
rpi_gpio_intr_read(struct gpio *gpio, int *value)
{
	/* No interrupts yet */
	return OK;
}

static int
rpi_message_hook(message * m)
{
	/* No interrupts yet */
	return OK;
}

static int
rpi_gpio_init(struct gpio_driver *gpdrv)
{
	struct minix_mem_range mr;

	mr.mr_base = PADCONF_RPI2_REGISTERS_BASE;
	mr.mr_limit = mr.mr_base + PADCONF_RPI2_REGISTERS_SIZE;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		log_warn(&log,
			"Unable to request permission to map memory\n");
		return EPERM;	/* fixme */
	}

	/* Set the base address to use */
	rpi_gpio_base_address =
		(uint32_t) vm_map_phys(SELF,
		(void *) PADCONF_RPI2_REGISTERS_BASE, PADCONF_RPI2_REGISTERS_SIZE);

	if (rpi_gpio_base_address == (uint32_t) MAP_FAILED) {
		log_warn(&log, "Unable to map GPIO memory\n");
		return EPERM;	/* fixme */
	}

	gpdrv->claim = rpi_gpio_claim;
	gpdrv->pin_mode = rpi_gpio_pin_mode;
	gpdrv->set = rpi_gpio_set;
	gpdrv->read = rpi_gpio_read;
	gpdrv->intr_read = rpi_gpio_intr_read;
	gpdrv->message_hook = rpi_message_hook;
	return 0;
}

int
gpio_init()
{
	return rpi_gpio_init(&drv);
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
