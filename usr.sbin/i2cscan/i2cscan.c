/* $NetBSD: i2cscan.c,v 1.4 2013/07/10 15:18:54 tcort Exp $ */

/*-
 * Copyright (c) 2011, 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Goyette and Jared McNeill
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: i2cscan.c,v 1.4 2013/07/10 15:18:54 tcort Exp $");

#include <sys/types.h>
#include <sys/ioctl.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dev/i2c/i2c_io.h>

#define MODE_DEFAULT 0
#define MODE_READ 1

__dead static void
usage(void)
{
	fprintf(stderr, "usage: %s [-r] <i2cdev>\n", getprogname());
	exit(EXIT_FAILURE);
}

static int
iic_smbus_quick_write(int fd, i2c_addr_t addr, int flags)
{
	i2c_ioctl_exec_t iie;

	iie.iie_op = I2C_OP_WRITE_WITH_STOP;
	iie.iie_addr = addr;
	iie.iie_cmd = NULL;
	iie.iie_cmdlen = 0;
	iie.iie_buf = NULL;
	iie.iie_buflen = 0;

	if (ioctl(fd, I2C_IOCTL_EXEC, &iie) == -1)
		return errno;
	return 0;
}

static int
iic_smbus_receive_byte(int fd, i2c_addr_t addr, uint8_t *valp, int flags)
{
	i2c_ioctl_exec_t iie;

	iie.iie_op = I2C_OP_READ_WITH_STOP;
	iie.iie_addr = addr;
	iie.iie_cmd = NULL;
	iie.iie_cmdlen = 0;
	iie.iie_buf = valp;
	iie.iie_buflen = 1;

	if (ioctl(fd, I2C_IOCTL_EXEC, &iie) == -1)
		return errno;
	return 0;
	
}

static void
do_i2c_scan(const char *dname, int fd, int mode)
{
	int error;
	int found = 0;
	i2c_addr_t addr;
	uint8_t val;

	for (addr = 0x09; addr < 0x78; addr++) {
		/*
		 * Skip certain i2c addresses:
		 *	0x00		General Call / START
		 *	0x01		CBUS Address
		 *	0x02		Different Bus format
		 *	0x03 - 0x07	Reserved
		 *	0x08		Host Address
		 *	0x0c		Alert Response Address
		 *	0x28		ACCESS.Bus host
		 *	0x37		ACCESS.Bus default address
		 *	0x48 - 0x4b	Prototypes
		 *	0x61		Device Default Address
		 *	0x78 - 0x7b	10-bit addresses
		 *	0x7c - 0x7f	Reserved
		 *
		 * Some of these are skipped by judicious selection
		 * of the range of the above for (;;) statement.
		 *
		 * if (addr <= 0x08 || addr >= 0x78)
		 *	continue;
		 */
		if (addr == 0x0c || addr == 0x28 || addr == 0x37 ||
		    addr == 0x61 || (addr & 0x7c) == 0x48)
			continue;

		/*
		 * Use SMBus quick_write command to detect most
		 * addresses;  should avoid hanging the bus on
		 * some write-only devices (like clocks that show
		 * up at address 0x69)
		 *
		 * XXX The quick_write() is allegedly known to
		 * XXX corrupt the Atmel AT24RF08 EEPROM found
		 * XXX on some IBM Thinkpads!
		 */
		printf("\r%s: scanning 0x%02x", dname, addr);
		fflush(stdout);
		if ((addr & 0xf8) == 0x30 ||
		    (addr & 0xf0) == 0x50 ||
		    mode == MODE_READ)
			error = iic_smbus_receive_byte(fd, addr, &val, 0);
		else
			error = iic_smbus_quick_write(fd, addr, 0);
		if (error == 0) {
			printf("\r%s: found device at 0x%02x\n",
			    dname, addr);
			++found;
		}
	}
	if (found == 0)
		printf("\r%s: no devices found\n", dname);
	else
		printf("\r%s: %d devices found\n", dname, found);
}

int
main(int argc, char *argv[])
{
	int fd;
	int ch, rflag;
	int mode;

	setprogname(*argv);

	rflag = 0;

	while ((ch = getopt(argc, argv, "r")) != -1)
		switch (ch) {
		case 'r':
			rflag = 1;
			break;
		default:
			break;
		}
	argv += optind;
	argc -= optind;

	if (rflag)
		mode = MODE_READ;
	else
		mode = MODE_DEFAULT;

	if (*argv == NULL)
		usage();

	fd = open(*argv, O_RDWR);
	if (fd == -1)
		err(EXIT_FAILURE, "couldn't open %s", *argv);

	do_i2c_scan(*argv, fd, mode);

	close(fd);

	return EXIT_SUCCESS;
}
