#ifndef _MINIX_PROCFS_H
#define _MINIX_PROCFS_H

/*
 * The compatibility model is as follows. The current format should be retained
 * for as long as possible; new fields can be added at the end of the line,
 * because ps/top only read as much as they know of from the start of the line.
 * Once fields (really) have to be removed, or the whole line becomes too big
 * of a mess, a completely new format string can be put in, but with an
 * increased PSINFO_VERSION at the beginning. That way, older ps/top copies
 * will not misinterpret the new fields, but rather fail cleanly.
 */
#define PSINFO_VERSION	1

/* Process types. */
#define TYPE_TASK	'T'
#define TYPE_SYSTEM	'S'
#define TYPE_USER	'U'

/* General process states. */
#define STATE_SLEEP	'S'
#define STATE_ZOMBIE	'Z'
#define STATE_RUN	'R'
#define STATE_STOP	'T'

#endif /* _MINIX_PROCFS_H */
