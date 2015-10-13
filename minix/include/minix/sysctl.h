#ifndef _MINIX_SYSCTL_H
#define _MINIX_SYSCTL_H

/* MINIX3-specific sysctl(2) extensions. */

#include <sys/sysctl.h>

/* Special values. */
#define SYSCTL_NODE_FN	((sysctlfn)0x1)		/* node is function-driven */

/*
 * The top-level MINIX3 identifier is quite a bit beyond the last top-level
 * identifier in use by NetBSD, because NetBSD may add more later, and we do
 * not want conflicts: this definition is part of the MINIX3 ABI.
 */
#define CTL_MINIX	32

#if CTL_MAXID > CTL_MINIX
#error "CTL_MAXID has grown too large!"
#endif

/*
 * The identifiers below follow the standard sysctl naming scheme, which means
 * care should be taken not to introduce clashes with other definitions
 * elsewhere.  On the upside, not many places need to include this header file.
 */
#define MINIX_TEST	0
#define MINIX_MIB	1

/*
 * These identifiers, under MINIX_TEST, are used by test87 to test the MIB
 * service.
 */
#define TEST_INT	0
#define TEST_BOOL	1
#define TEST_QUAD	2
#define TEST_STRING	3
#define TEST_STRUCT	4
#define TEST_PRIVATE	5
#define TEST_ANYWRITE	6
#define TEST_DYNAMIC	7
#define TEST_SECRET	8
#define TEST_PERM	9
#define TEST_DESTROY1	10
#define TEST_DESTROY2	11

#define SECRET_VALUE	0

/* Identifiers for subnodes of MINIX_MIB. */
#define MIB_NODES	1
#define MIB_OBJECTS	2

#endif /* !_MINIX_SYSCTL_H */
