#include <minix/i2c.h>
#include <minix/com.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "eepromread.h"

static int __eeprom_read128(int fd, i2c_addr_t addr, uint16_t memaddr,
    void *buf, size_t buflen, int flags);
static int eeprom_dump(int fd, i2c_addr_t addr, int flags);

#define DEFAULT_I2C_DEVICE "/dev/i2c-1"
#define DEFAULT_I2C_ADDRESS 0x50

/*
 * The /dev interface only supports 128 byte reads/writes and the EEPROM is
 * larger, so to read the whole EEPROM, the task is broken down into 128 byte
 * chunks in eeprom_read(). __eeprom_read128() does the actual ioctl() to do
 * the read.
 *
 * A future enhancement might be to add support for the /dev/eeprom interface
 * and if one way fails, fall back to the other. /dev/eeprom can fail if the
 * eeprom driver isn't running and /dev/i2c can fail if the eeprom driver
 * claimed the eeprom device.
 */

static int
__eeprom_read128(int fd, i2c_addr_t addr, uint16_t memaddr, void *buf,
    size_t buflen, int flags)
{
	int r;
	minix_i2c_ioctl_exec_t ioctl_exec;

	if (buflen > I2C_EXEC_MAX_BUFLEN || buf == NULL
	    || ((memaddr + buflen) < memaddr)) {
		errno = EINVAL;
		return -1;
	}

	memset(&ioctl_exec, '\0', sizeof(minix_i2c_ioctl_exec_t));

	ioctl_exec.iie_op = I2C_OP_READ_WITH_STOP;
	ioctl_exec.iie_addr = addr;

	/* set the address to read from */
	if ((BDEV_NOPAGE & flags) == BDEV_NOPAGE) {
		/* reading within the current page */
		ioctl_exec.iie_cmd[0] = (memaddr & 0xff);
		ioctl_exec.iie_cmdlen = 1;
	} else {
		/* reading from device with multiple pages */
		ioctl_exec.iie_cmd[0] = ((memaddr >> 8) & 0xff);
		ioctl_exec.iie_cmd[1] = (memaddr & 0xff);
		ioctl_exec.iie_cmdlen = 2;
	}
	ioctl_exec.iie_buflen = buflen;

	r = ioctl(fd, MINIX_I2C_IOCTL_EXEC, &ioctl_exec);
	if (r == -1) {
		return -1;
	}

	/* call was good, copy results to caller's buffer */
	memcpy(buf, ioctl_exec.iie_buf, buflen);

	return 0;
}

int
eeprom_read(int fd, i2c_addr_t addr, uint16_t memaddr, void *buf,
    size_t buflen, int flags)
{
	int r;
	uint16_t i;

	if (buf == NULL || ((memaddr + buflen) < memaddr)) {
		errno = EINVAL;
		return -1;
	}


	for (i = 0; i < buflen; i += 128) {

		r = __eeprom_read128(fd, addr, memaddr + i, buf + i,
		    ((buflen - i) < 128) ? (buflen - i) : 128, flags);
		if (r == -1) {
			return -1;
		}
	}

	return 0;
}

/*
 * Read 256 bytes and print it to the screen in HEX and ASCII.
 */
static int
eeprom_dump(int fd, i2c_addr_t addr, int flags)
{
	int i, j, r;
	uint8_t buf[256];

	memset(buf, '\0', 256);

	r = eeprom_read(fd, addr, 0x0000, buf, 256, flags);
	if (r == -1) {
		return r;
	}

	/* print table header */
	for (i = 0; i < 2; i++) {
		printf("   ");
		for (j = 0x0; j <= 0xf; j++) {
			if (i == 0) {
				printf("  ");
			}
			printf("%x", j);
		}
	}
	printf("\n");

	/* print table data */
	for (i = 0x00; i < 0xff; i += 0x10) {

		/* row label */
		printf("%02x:", i);

		/* row data (in hex) */
		for (j = 0x0; j <= 0xf; j++) {
			printf(" %02x", buf[i + j]);
		}

		printf("   ");

		/* row data (in ASCII) */
		for (j = 0x0; j <= 0xf; j++) {
			if (isprint(buf[i + j])) {
				printf("%c", buf[i + j]);
			} else {
				printf(".");
			}
		}

		printf("\n");
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	int r, fd;
	int ch, iflag = 0, read_flags = 0;
	char *device = DEFAULT_I2C_DEVICE;
	i2c_addr_t address = DEFAULT_I2C_ADDRESS;

	setprogname(*argv);

	while ((ch = getopt(argc, argv, "a:f:in")) != -1) {
		switch (ch) {
		case 'a':
			address = strtol(optarg, NULL, 0x10);
			break;
		case 'f':
			device = optarg;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'n':
			read_flags |= BDEV_NOPAGE;
			break;
		default:
			break;
		}
	}

	fd = open(device, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "open(): %s\n", strerror(errno));
		return 1;
	}

	if (iflag == 1) {
		r = board_info(fd, address, read_flags);
		if (r == -1) {
			fprintf(stderr, "board_info(): %s\n", strerror(errno));
			return 1;
		}
	} else {
		r = eeprom_dump(fd, address, read_flags);
		if (r == -1) {
			fprintf(stderr, "eeprom_dump(): %s\n", strerror(errno));
			return 1;
		}
	}

	r = close(fd);
	if (r == -1) {
		fprintf(stderr, "close(): %s\n", strerror(errno));
		return 1;
	}

	return 0;
}

