/*	sys/ioc_fbd.h - Faulty Block Device ioctl() command codes.
 *
 */

#ifndef _S_I_FBD_H
#define _S_I_FBD_H

/* FBD rule structure. */

typedef int fbd_rulenum_t;

struct fbd_rule {
	fbd_rulenum_t num;	/* rule number (1-based) */
	u64_t start;		/* start position of area to match */
	u64_t end;		/* end position (exclusive) (0 = up to EOF) */
	int flags;		/* match read and/or write requests */
	unsigned int skip;	/* # matches to skip before activating */
	unsigned int count;	/* # times left to trigger (0 = no limit) */
	int action;		/* action type to perform when triggered */

	union {			/* action parameters */
		struct {
			int type;	/* corruption type */
		} corrupt;

		struct {
			int code;	/* error code to return (OK, EIO..) */
		} error;

		struct {
			u64_t start;	/* start position of target area */
			u64_t end;	/* end position of area (excl) */
			u32_t align;	/* alignment to use in target area */
		} misdir;

		struct {
			u32_t lead;	/* # bytes to process normally */
		} losttorn;
	} params;
};

/* Match flags. */
#define FBD_FLAG_READ	0x1		/* match read requests */
#define FBD_FLAG_WRITE	0x2		/* match write requests */

/* Action types. */
enum {
	FBD_ACTION_CORRUPT,		/* write or return corrupt data */
	FBD_ACTION_ERROR,		/* return an I/O error */
	FBD_ACTION_MISDIR,		/* perform a misdirected write */
	FBD_ACTION_LOSTTORN		/* perform a lost or torn write */
};

/* Corruption types. */
enum {
	FBD_CORRUPT_ZERO,		/* write or return ..zeroed data */
	FBD_CORRUPT_PERSIST,		/* ..consequently the same bad data */
	FBD_CORRUPT_RANDOM		/* ..new random data every time */
};

/* The I/O control requests. */
#define FBDCADDRULE	_IOW('F', 1, struct fbd_rule)	/* add a rule */
#define FBDCDELRULE	_IOW('F', 2, fbd_rulenum_t)	/* delete a rule */
#define FBDCGETRULE	_IORW('F', 3, struct fbd_rule)	/* retrieve a rule */

#endif /* _S_I_FBD_H */
