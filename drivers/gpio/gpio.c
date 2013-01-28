/*
 * GPIO driver. This driver acts as a file system to allow
 * reading and toggling of GPIO's.
 */
/* kernel headers */
#include <minix/driver.h>
#include <minix/drvlib.h>
#include <minix/vtreefs.h>

/* system headers */
#include <sys/stat.h>
#include <sys/queue.h>

/* usr headers */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

/* local headers */
#include "log.h"
#include "mmio.h"
#include "gpio.h"

/* used for logging */
static struct log log = {
	.name = "gpio",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

#define GPIO_CB_READ 0
#define GPIO_CB_ON 1
#define GPIO_CB_OFF 2

/* The vtreefs library provides callback data when calling
 * the read function of inode. gpio_cbdata is used here to
 * map between inodes and gpio's. VTreeFS is read-only. to work
 * around that issue for a single GPIO we create multiple virtual
 * files that can be *read* to read the gpio value and power on
 * and off the gpio.
 */
struct gpio_cbdata
{
	struct gpio *gpio;	/* obtained from the driver */
	int type;		/* read=0/on=1/off=2 */
	    TAILQ_ENTRY(gpio_cbdata) next;
};

/* list of inodes used in this driver */
TAILQ_HEAD(gpio_cbdata_head, gpio_cbdata)
    gpio_cbdata_list = TAILQ_HEAD_INITIALIZER(gpio_cbdata_list);

static struct gpio_driver drv;

/* Sane file stats for a directory */
static struct inode_stat default_file_stat = {
	.mode = S_IFREG | 04,
	.uid = 0,
	.gid = 0,
	.size = 0,
	.dev = NO_DEV,
};

int
add_gpio_inode(char *name, int nr, int mode)
{
	/* Create 2 files nodes for "name" "nameon" and "nameoff" to read and
	 * set values as we don't support writing yet */
	char tmpname[200];
	struct gpio_cbdata *cb;
	struct gpio *gpio;

	/* claim and configure the gpio */
	if (drv.claim("gpiofs", nr, &gpio)) {
		log_warn(&log, "Failed to claim GPIO %d\n", nr);
		return EIO;
	}
	assert(gpio != NULL);

	if (drv.pin_mode(gpio, mode)) {
		log_warn(&log, "Failed to switch GPIO %d to mode %d\n", nr,
		    mode);
		return EIO;
	}

	/* read value */
	cb = malloc(sizeof(struct gpio_cbdata));
	if (cb == NULL) {
		return ENOMEM;
	}
	memset(cb, 0, sizeof(*cb));

	cb->type = GPIO_CB_READ;
	cb->gpio = gpio;

	snprintf(tmpname, 200, "%s", name);
	add_inode(get_root_inode(), tmpname, NO_INDEX, &default_file_stat, 0,
	    (cbdata_t) cb);
	TAILQ_INSERT_HEAD(&gpio_cbdata_list, cb, next);

	if (mode == GPIO_MODE_OUTPUT) {
		/* if we configured the GPIO pin as output mode also create
		 * two additional files to turn on and off the GPIO. */
		/* turn on */
		cb = malloc(sizeof(struct gpio_cbdata));
		if (cb == NULL) {
			return ENOMEM;
		}
		memset(cb, 0, sizeof(*cb));

		cb->type = GPIO_CB_ON;
		cb->gpio = gpio;

		snprintf(tmpname, 200, "%son", name);
		add_inode(get_root_inode(), tmpname, NO_INDEX,
		    &default_file_stat, 0, (cbdata_t) cb);
		TAILQ_INSERT_HEAD(&gpio_cbdata_list, cb, next);

		/* turn off */
		cb = malloc(sizeof(struct gpio_cbdata));
		if (cb == NULL) {
			return ENOMEM;
		}
		memset(cb, 0, sizeof(*cb));

		cb->type = GPIO_CB_OFF;
		cb->gpio = gpio;

		snprintf(tmpname, 200, "%soff", name);
		add_inode(get_root_inode(), tmpname, NO_INDEX,
		    &default_file_stat, 0, (cbdata_t) cb);
		TAILQ_INSERT_HEAD(&gpio_cbdata_list, cb, next);
	}
	return OK;
}

static void
init_hook(void)
{
	/* This hook will be called once, after VTreeFS has initialized. */
	if (omap_gpio_init(&drv)) {
		log_warn(&log, "Failed to init gpio driver\n");
	}
	add_gpio_inode("USR0", 149, GPIO_MODE_OUTPUT);
	add_gpio_inode("USR1", 150, GPIO_MODE_OUTPUT);
	add_gpio_inode("Button", 4, GPIO_MODE_INPUT);

#if 0
	add_gpio_inode("input1", 139, GPIO_MODE_INPUT);
	add_gpio_inode("input2", 144, GPIO_MODE_INPUT);
#endif
}

static int
    read_hook
    (struct inode *inode, off_t offset, char **ptr, size_t * len,
    cbdata_t cbdata)
{
	/* This hook will be called every time a regular file is read. We use
	 * it to dyanmically generate the contents of our file. */
	static char data[26];
	int value;
	struct gpio_cbdata *gpio_cbdata = (struct gpio_cbdata *) cbdata;
	assert(gpio_cbdata->gpio != NULL);

	if (gpio_cbdata->type == GPIO_CB_ON) {
		/* turn on */
		if (drv.set(gpio_cbdata->gpio, 1)) {
			*len = 0;
			return EIO;
		}
		*len = 0;
		return OK;
	} else if (gpio_cbdata->type == GPIO_CB_OFF) {
		/* turn off */
		if (drv.set(gpio_cbdata->gpio, 0)) {
			*len = 0;
			return EIO;
		}
		*len = 0;
		return OK;
	}

	/* reading */
	if (drv.read(gpio_cbdata->gpio, &value)) {
		*len = 0;
		return EIO;
	}
	snprintf(data, 26, "%d\n", value);

	/* If the offset is beyond the end of the string, return EOF. */
	if (offset > strlen(data)) {
		*len = 0;

		return OK;
	}

	/* Otherwise, return a pointer into 'data'. If necessary, bound the
	 * returned length to the length of the rest of the string. Note that
	 * 'data' has to be static, because it will be used after this
	 * function returns. */
	*ptr = data + offset;

	if (*len > strlen(data) - offset)
		*len = strlen(data) - offset;

	return OK;
}

int
main(int argc, char **argv)
{

	struct fs_hooks hooks;
	struct inode_stat root_stat;

	/* Set and apply the environment */
	env_setargs(argc, argv);

	/* fill in the hooks */
	memset(&hooks, 0, sizeof(hooks));
	hooks.init_hook = init_hook;
	hooks.read_hook = read_hook;

	root_stat.mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH;
	root_stat.uid = 0;
	root_stat.gid = 0;
	root_stat.size = 0;
	root_stat.dev = NO_DEV;

	/* limit the number of indexed entries */
	start_vtreefs(&hooks, 10, &root_stat, 0);

	return EXIT_SUCCESS;
}
