/*	mt 1.3 - magnetic tape control			Author: Kees J. Bot
 *								4 Apr 1993
 */
#define nil	NULL
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE	1
#endif
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>

/* Device status. */
#define DS_OK		0
#define DS_ERR		1
#define DS_EOF		2

/* SCSI Sense key bits. */
#define SENSE_KEY	0x0F	/* The key part. */
#define SENSE_ILI	0x20	/* Illegal block size. */
#define SENSE_EOM	0x40	/* End-of-media. */
#define SENSE_EOF	0x80	/* Filemark reached. */

/* Supported operations: */

typedef struct tape_operation {
	int	op;		/* Opcode for MTIOCTOP ioctl (if any). */
	char	*cmd;		/* Command name. */
	int	lim;		/* Limits on count. */
} tape_operation_t;

#define SELF	-1	/* Not a simple command, have to interpret. */
#define IGN	-1	/* Ignore count field (or accept anything.) */
#define NNG	 0	/* Nonnegative count field. */
#define POS	 1	/* Positive count field. */

tape_operation_t tapeops[] = {
	{ MTWEOF,  "eof",      POS },	/* Write EOF mark */
	{ MTWEOF,  "weof",     POS },	/* Same */
	{ MTFSF,   "fsf",      POS },	/* Forward Space File */
	{ MTFSR,   "fsr",      POS },	/* Forward Space Record */
	{ MTBSF,   "bsf",      NNG },	/* Backward Space File */
	{ MTBSR,   "bsr",      POS },	/* Backward Space Record */
	{ MTEOM,   "eom",      IGN },	/* To End-Of-Media */
	{ MTREW,   "rewind",   IGN },	/* Rewind */
	{ MTOFFL,  "offline",  IGN },	/* Rewind and take offline */
	{ MTOFFL,  "rewoffl",  IGN },	/* Same */
	{ SELF,	   "status",   IGN },	/* Tape Status */
	{ MTRETEN, "retension",IGN },	/* Retension the tape */
	{ MTERASE, "erase",    IGN },	/* Erase the tape */
	{ MTSETDNSTY,  "density",  NNG },	/* Select density */
	{ MTSETBSIZ,  "blksize",  NNG },	/* Select block size */
	{ MTSETBSIZ,  "blocksize",NNG },	/* Same */
};

#define arraysize(a)	(sizeof(a)/sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

/* From aha_scsi.c: */
char *dev_state[] = {
	"OK", "ERR", "EOF"
};

char *scsi_sense[] = {
	"NO SENSE INFO", "RECOVERED ERROR", "NOT READY", "MEDIUM ERROR",
	"HARDWARE ERROR", "ILLEGAL REQUEST", "UNIT ATTENTION", "DATA PROTECT",
	"BLANK CHECK", "VENDOR UNIQUE ERROR", "COPY ABORTED", "ABORTED COMMAND",
	"EQUAL", "VOLUME OVERFLOW", "MISCOMPARE", "SENSE RESERVED"
};

void usage(void)
{
	fprintf(stderr, "Usage: mt [-f device] command [count]\n");
	exit(1);
}

int main(int argc, char **argv)
{
	char *tape;
	char *cmd;
	int count= 1;
	int fd, r;
	tape_operation_t *op, *found;
	struct mtop mtop;
	struct mtget mtget;

	tape= getenv("TAPE");

	/* -f tape? */
	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'f') {
		tape= argv[1] + 2;

		if (*tape == 0) {
			if (--argc < 2) usage();
			argv++;
			tape= argv[1];
		}
		argc--;
		argv++;
	}

	if (argc != 2 && argc != 3) usage();

	if (argc == 3) {
		/* Check and convert the 'count' argument. */
		char *end;

		errno= 0;
		count= strtol(argv[2], &end, 0);
		if (*end != 0) usage();
		if (errno == ERANGE || (mtop.mt_count= count) != count) {
			fprintf(stderr, "mt: %s: count too large, overflow\n",
				argv[2]);
			exit(1);
		}
	}

	if (tape == nil) {
		fprintf(stderr,
			"mt: tape device not specified by -f or $TAPE\n");
		exit(1);
	}

	cmd= argv[1];
	if (strcmp(cmd, "rew") == 0) cmd= "rewind";	/* aha! */
	found= nil;

	/* Search for an operation that is unambiguously named. */
	for (op= tapeops; op < arraylimit(tapeops); op++) {
		if (strncmp(op->cmd, cmd, strlen(cmd)) == 0) {
			if (found != nil) {
				fprintf(stderr, "mt: %s: ambiguous\n", cmd);
				exit(1);
			}
			found= op;
		}
	}

	if ((op= found) == nil) {
		fprintf(stderr, "mt: unknown command '%s'\n", cmd);
		exit(1);
	}

	/* Check count. */
	switch (op->lim) {
	case NNG:
		if (count < 0) {
			fprintf(stderr, "mt %s: count may not be negative\n",
				op->cmd);
			exit(1);
		}
		break;
	case POS:
		if (count <= 0) {
			fprintf(stderr,
				"mt %s: count must be greater than zero\n",
				op->cmd);
			exit(1);
		}
		break;
	}

	if (strcmp(tape, "-") == 0) {
		fd= 0;
	} else
	if ((fd= open(tape, O_RDONLY)) < 0) {
		fprintf(stderr, "mt: %s: %s\n", tape, strerror(errno));
		exit(1);
	}

	if (op->op != SELF) {
		/* A simple tape operation. */

		mtop.mt_op= op->op;
		mtop.mt_count= count;
		r= ioctl(fd, MTIOCTOP, &mtop);
	} else
	if (strcmp(op->cmd, "status") == 0) {
		/* Get status information. */

		if ((r= ioctl(fd, MTIOCGET, &mtget)) == 0) {
			printf("\
SCSI tape drive %s:\n\
   drive status = 0x%02x (%s), sense key = 0x%02x (%s%s%s%s)\n\
   file no = %ld, block no = %ld, residual = %ld, block size = ",
   				tape, mtget.mt_dsreg,
   				mtget.mt_dsreg > 2 ? "?" :
   						dev_state[mtget.mt_dsreg],
				mtget.mt_erreg,
				mtget.mt_erreg & SENSE_EOF ? "EOF + " : "",
				mtget.mt_erreg & SENSE_EOM ? "EOM + " : "",
				mtget.mt_erreg & SENSE_ILI ? "ILI + " : "",
				scsi_sense[mtget.mt_erreg & SENSE_KEY],
				(long) mtget.mt_fileno,
				(long) mtget.mt_blkno,
				(long) mtget.mt_resid);
			if (mtget.mt_blksiz == 0) printf("variable\n");
			else printf("%d\n", mtget.mt_blksiz);
		}
	}
	if (r < 0) {
		if (errno == ENOTTY) {
			fprintf(stderr, "mt: %s: command '%s' not supported\n",
				tape, op->cmd);
			exit(2);
		}
		fprintf(stderr, "mt: %s: %s\n", tape, strerror(errno));
		exit(1);
	}
	exit(0);
}
