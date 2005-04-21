/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
#define ushort	unsigned short
#define CHARBITS 0xffff
#define NSUBEXP  10
typedef struct regexp {
	ushort *startp[NSUBEXP];
	ushort *endp[NSUBEXP];
	ushort regstart;		/* Internal use only. */
	ushort reganch;		/* Internal use only. */
	ushort *regmust;		/* Internal use only. */
	int regmlen;		/* Internal use only. */
	ushort program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

extern regexp *regcomp();
extern int regexec();
extern int regsub();
extern int regerror();
