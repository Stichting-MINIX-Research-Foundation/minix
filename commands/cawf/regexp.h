/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
#define NSUBEXP  10
typedef struct regexp {
	unsigned char *startp[NSUBEXP];
	unsigned char *endp[NSUBEXP];
	unsigned char regstart;		/* Internal use only. */
	unsigned char reganch;		/* Internal use only. */
	unsigned char *regmust;		/* Internal use only. */
	unsigned char regmlen;		/* Internal use only. */
	unsigned char program[1];	/* Unwarranted chumminess with
					 * compiler. */
} regexp;
