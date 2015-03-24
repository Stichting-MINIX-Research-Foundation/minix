/* $NetBSD: udf_osta.h,v 1.4 2008/05/14 16:49:48 reinoud Exp $ */

/*
 * Prototypes for the OSTA functions
 */


#ifndef _FS_UDF_OSTA_H_
#define _FS_UDF_OSTA_H_

#include <sys/types.h>

#ifndef _KERNEL
#include <stdbool.h>
#endif

#ifndef UNIX
#define	UNIX
#endif

#ifndef MAXLEN
#define	MAXLEN	255
#endif


/***********************************************************************
 * The following two typedef's are to remove compiler dependancies.
 * byte needs to be unsigned 8-bit, and unicode_t needs to be
 * unsigned 16-bit.
 */
typedef uint16_t unicode_t;
typedef uint8_t    byte;


int udf_UncompressUnicode(int, byte *, unicode_t *);
int udf_CompressUnicode(int, int, unicode_t *, byte *);
unsigned short udf_cksum(unsigned char *, int);
unsigned short udf_unicode_cksum(unsigned short *, int);
uint16_t udf_ea_cksum(uint8_t *data);
int UDFTransName(unicode_t *, unicode_t *, int);
int UnicodeLength(unicode_t *string);


#endif /* _FS_UDF_OSTA_H_ */
