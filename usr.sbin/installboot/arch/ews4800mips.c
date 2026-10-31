
#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD: ews4800mips.c,v 1.3 2019/05/07 04:35:31 thorpej Exp $");
#endif	/* !__lint */

#include <sys/param.h>
#include <unistd.h>
#include <err.h>
#include <stdio.h>
#include "installboot.h"

static int ews4800mips_setboot(ib_params *);

struct ib_mach ib_mach_ews4800mips = {
	.name		=	"ews4800mips",
	.setboot	=	ews4800mips_setboot,
	.clearboot	=	no_clearboot,
	.editboot	=	no_editboot,
};

struct bbinfo_params ews4800mips_bbparams = {
	EWS4800MIPS_BBINFO_MAGIC,
	EWS4800MIPS_BOOT_BLOCK_OFFSET,
	EWS4800MIPS_BOOT_BLOCK_BLOCKSIZE,
	EWS4800MIPS_BOOT_BLOCK_MAX_SIZE,
	0,
	BBINFO_BIG_ENDIAN,
};

static int
ews4800mips_setboot(ib_params *params)
{
	u_int8_t buf[EWS4800MIPS_BOOT_BLOCK_MAX_SIZE];
	int rv;

	rv = pread(params->s1fd, buf, sizeof buf, 0);
	if (rv == -1) {
		warn("Reading `%s'", params->stage1);
		return 0;
	} else if (rv != sizeof buf) {
		warnx("Reading `%s' : short read", params->stage1);
		return 0;
	}

	if (params->flags & IB_NOWRITE)
		return 1;

	if (params->flags & IB_VERBOSE)
		printf("Writing boot block\n");

	rv = pwrite(params->fsfd, buf, sizeof buf, 0);
	if (rv == -1) {
		warn("Writing `%s'", params->filesystem);
		return 0;
	} else if (rv != sizeof buf) {
		warnx("Writing `%s': short write", params->filesystem);
		return 0;
	}

	return 1;
}
