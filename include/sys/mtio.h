/* <sys/mtio.h> magnetic tape commands			Author: Kees J. Bot
 */

#ifndef _SYS__MTIO_H
#define _SYS__MTIO_H

/* Tape operations: ioctl(fd, MTIOCTOP, &struct mtop) */

struct mtop {
	short	mt_op;		/* Operation (MTWEOF, etc.) */
	int	mt_count;	/* Repeat count. */
};

#define MTWEOF	 0	/* Write End-Of-File Marker */
#define MTFSF	 1	/* Forward Space File mark */
#define MTBSF	 2	/* Backward Space File mark */
#define MTFSR	 3	/* Forward Space Record */
#define MTBSR	 4	/* Backward Space Record */
#define MTREW	 5	/* Rewind tape */
#define MTOFFL	 6	/* Rewind and take Offline */
#define MTNOP	 7	/* No-Operation, set status only */
#define MTRETEN	 8	/* Retension (completely wind and rewind) */
#define MTERASE	 9	/* Erase the tape and rewind */
#define MTEOM	10	/* Position at End-Of-Media */
#define MTMODE	11	/* Select tape density */
#define MTBLKZ	12	/* Select tape block size */

/* Tape status: ioctl(fd, MTIOCGET, &struct mtget) */

struct mtget {
	short	mt_type;	/* Type of tape device. */

	/* Device dependent "registers". */
	short	mt_dsreg;	/* Drive status register. */
	short	mt_erreg;	/* Error register. */
	short	dummy;		/* (alignment) */

	/* Misc info. */
	off_t	mt_resid;	/* Residual count. */
	off_t	mt_fileno;	/* Current File Number. */
	off_t	mt_blkno;	/* Current Block Number within file. */
	off_t	mt_blksize;	/* Current block size. */
};

#endif /* _SYS__MTIO_H */
