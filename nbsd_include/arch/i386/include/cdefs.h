/*	$NetBSD: cdefs.h,v 1.7 2008/10/26 06:57:30 mrg Exp $	*/

#ifndef	_I386_CDEFS_H_
#define	_I386_CDEFS_H_

#ifdef __minix
#ifndef __ELF__
#define __LEADING_UNDERSCORE
#endif
#else /* !__minix */
/* No arch-specific cdefs. */
#endif



#endif /* !_I386_CDEFS_H_ */
