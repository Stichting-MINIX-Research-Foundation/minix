/*
sys/param.h
*/

#ifndef __SYS_PARAM_H__
#define __SYS_PARAM_H__

#include <limits.h>
#include <minix/limits.h>

#define MAXHOSTNAMELEN  256	/* max hostname size */
#define NGROUPS		8	/* max number of supplementary groups */
#define MAXPATHLEN	__MINIX_PATH_MAX

/* Macros for min/max. */
#define MIN(a,b)        ((/*CONSTCOND*/(a)<(b))?(a):(b))
#define MAX(a,b)        ((/*CONSTCOND*/(a)>(b))?(a):(b))

#endif /* __SYS_PARAM_H__ */
