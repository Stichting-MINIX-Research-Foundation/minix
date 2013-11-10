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
 */

static int board_info_beaglebone(int fd, i2c_addr_t address, int flags,
    enum device_types device_type);
static int board_info_cape_a0(int fd, i2c_addr_t address, int flags,
    enum device_types device_type);
static int board_info_cape_a1(int fd, i2c_addr_t address, int flags,
    enum device_types device_type);

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

/* Memory Layout of the Cape EEPROM (A0 Format) - Defined in BBW SRM Rev A5.3 */
typedef struct cape_info_a0
{
	uint8_t magic_number[4];	/* Should be 0xaa 0x55 0x33 0xee */
	char revision[2];	/* Should be 'A' '0' */
	char board_name[32];	/* Warning: strings not NULL terminated */
	char version[4];
	char manufacturer[16];
	char partno[16];
	char num_pins[2];
	char serial[12];
	char pin_usage[140];
	char vdd_3v3exp_i[2];
	char vdd_5v_i[2];
	char sys_5v_i[2];
	char dc_supp[2];
} cape_info_a0_t;

/* Memory Layout of the Cape EEPROM (A1 Format) - Defined in BBB SRM Rev A5.3 */
typedef struct cape_info_a1
{
	uint8_t magic_number[4];	/* Should be 0xaa 0x55 0x33 0xee */
	char revision[2];	/* Should be 'A' '1' */
	char board_name[32];	/* Warning: strings not NULL terminated */
	char version[4];
	char manufacturer[16];
	char partno[16];
	char num_pins[2];
	char serial[12];
	char pin_usage[148];
	char vdd_3v3exp_i[2];
	char vdd_5v_i[2];
	char sys_5v_i[2];
	char dc_supp[2];
} cape_info_a1_t;

static int
board_info_beaglebone(int fd, i2c_addr_t address, int flags,
    enum device_types device_type)
{
	int r;
	int i, j;
	char s[33];
	beaglebone_info_t boneinfo;

	r = eeprom_read(fd, address, 0x0000, &boneinfo,
	    sizeof(beaglebone_info_t), flags, device_type);
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

	memset(s, '\0', 33);
	memcpy(s, boneinfo.serial_number, 12);
	fprintf(stdout, "%-16s: %s\n", "SERIAL_NUMBER", s);

	return 0;
}

static int
board_info_cape_a0(int fd, i2c_addr_t address, int flags,
    enum device_types device_type)
{
	int r;
	int i, j;
	char s[33];
	cape_info_a0_t capeinfo;

	r = eeprom_read(fd, address, 0x0000, &capeinfo,
	    sizeof(cape_info_a0_t), flags, device_type);
	if (r == -1) {
		fprintf(stderr, "failed to read cape A0 info r=%d\n", r);
		return -1;
	}

	fprintf(stdout, "%-16s: 0x%x%x%x%x\n", "MAGIC_NUMBER",
	    capeinfo.magic_number[0], capeinfo.magic_number[1],
	    capeinfo.magic_number[2], capeinfo.magic_number[3]);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.revision, 2);
	fprintf(stdout, "%-16s: %s\n", "REVISION", s);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.board_name, 32);
	fprintf(stdout, "%-16s: %s\n", "BOARD_NAME", s);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.version, 4);
	fprintf(stdout, "%-16s: %s\n", "VERSION", s);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.manufacturer, 16);
	fprintf(stdout, "%-16s: %s\n", "MANUFACTURER", s);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.partno, 16);
	fprintf(stdout, "%-16s: %s\n", "PART_NUMBER", s);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.serial, 12);
	fprintf(stdout, "%-16s: %s\n", "SERIAL", s);

	return 0;
}

static int
board_info_cape_a1(int fd, i2c_addr_t address, int flags,
    enum device_types device_type)
{
	int r;
	int i, j;
	char s[33];
	cape_info_a1_t capeinfo;

	r = eeprom_read(fd, address, 0x0000, &capeinfo,
	    sizeof(cape_info_a1_t), flags, device_type);
	if (r == -1) {
		fprintf(stderr, "failed to read cape A0 info r=%d\n", r);
		return -1;
	}

	fprintf(stdout, "%-16s: 0x%x%x%x%x\n", "MAGIC_NUMBER",
	    capeinfo.magic_number[0], capeinfo.magic_number[1],
	    capeinfo.magic_number[2], capeinfo.magic_number[3]);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.revision, 2);
	fprintf(stdout, "%-16s: %s\n", "REVISION", s);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.board_name, 32);
	fprintf(stdout, "%-16s: %s\n", "BOARD_NAME", s);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.version, 4);
	fprintf(stdout, "%-16s: %s\n", "VERSION", s);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.manufacturer, 16);
	fprintf(stdout, "%-16s: %s\n", "MANUFACTURER", s);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.partno, 16);
	fprintf(stdout, "%-16s: %s\n", "PART_NUMBER", s);

	memset(s, '\0', 33);
	memcpy(s, capeinfo.serial, 12);
	fprintf(stdout, "%-16s: %s\n", "SERIAL", s);

	return 0;
}

int
board_info(int fd, i2c_addr_t address, int flags,
    enum device_types device_type)
{
	int r;
	uint8_t magic_number[6];

	r = eeprom_read(fd, address, 0x0000, &magic_number, 6, flags,
	    device_type);
	if (r == -1) {
		printf("%-16s: %s\n", "BOARD_NAME", "UNKNOWN");
		return 0;
	}

	/* Check for BeagleBone/BeagleBone Black/Cape Magic Number */
	if (magic_number[0] == 0xaa && magic_number[1] == 0x55 &&
	    magic_number[2] == 0x33 && magic_number[3] == 0xee) {

		/* Check if Cape Rev A0, Cape Rev A1, or on-board EEPROM */
		if (magic_number[4] == 'A' && magic_number[5] == '0') {
			board_info_cape_a0(fd, address, flags, device_type);
		} else if (magic_number[4] == 'A' && magic_number[5] == '1') {
			board_info_cape_a1(fd, address, flags, device_type);
		} else {
			board_info_beaglebone(fd, address, flags, device_type);
		}
	} else {
		printf("%-16s: %s\n", "BOARD_NAME", "UNKNOWN");
	}

	return 0;
}
