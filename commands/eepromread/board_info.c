#include <minix/i2c.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "eepromread.h"

/*
 * Attempt to read the board info from an eeprom on this board.
 * Currently only supports the BeagleBone and BeagleBone Black.
 * In the future, this could be expanded to support cape EEPROMs.
 */

static int board_info_beaglebone(int fd, i2c_addr_t address, int flags);

/* Memory Layout of the BeagleBone and BeagleBone Black EEPROM */
typedef struct beaglebone_info
{
	uint8_t magic_number[4];	/* Should be 0xaa 0x55 0x33 0xee */
	char board_name[8];	/* Warning: strings not NULL terminated */
	char version[4];
	char serial_number[12];
	char config[32];	/* All 0x00 on BeagleBone White */
	char mac_addrs[3][6];	/* Not set on BeagleBone White */
} beaglebone_info_t;

static int
board_info_beaglebone(int fd, i2c_addr_t address, int flags)
{
	int r;
	int i, j;
	char s[33];
	beaglebone_info_t boneinfo;

	r = eeprom_read(fd, address, 0x0000, &boneinfo,
	    sizeof(beaglebone_info_t), flags);
	if (r == -1) {
		fprintf(stderr, "Failed to read BeagleBone info r=%d\n", r);
		return -1;
	}

	fprintf(stdout, "%-16s: 0x%x%x%x%x\n", "MAGIC_NUMBER",
	    boneinfo.magic_number[0], boneinfo.magic_number[1],
	    boneinfo.magic_number[2], boneinfo.magic_number[3]);

	memset(s, '\0', 33);
	memcpy(s, boneinfo.board_name, 8);
	fprintf(stdout, "%-16s: %s\n", "BOARD_NAME", s);

	memset(s, '\0', 33);
	memcpy(s, boneinfo.version, 4);
	fprintf(stdout, "%-16s: %s\n", "VERSION", s);

	memcpy(s, boneinfo.serial_number, 12);
	fprintf(stdout, "%-16s: %s\n", "SERIAL_NUMBER", s);

	return 0;
}

int
board_info(int fd, i2c_addr_t address, int flags)
{
	int r;
	uint8_t magic_number[4];

	r = eeprom_read(fd, address, 0x0000, &magic_number, 4, flags);
	if (r == -1) {
		printf("%-16s: %s\n", "BOARD_NAME", "UNKNOWN");
		return 0;
	}

	if (magic_number[0] == 0xaa && magic_number[1] == 0x55 &&
	    magic_number[2] == 0x33 && magic_number[3] == 0xee) {
		board_info_beaglebone(fd, address, flags);
	} else {
		printf("%-16s: %s\n", "BOARD_NAME", "UNKNOWN");
	}

	return 0;
}
