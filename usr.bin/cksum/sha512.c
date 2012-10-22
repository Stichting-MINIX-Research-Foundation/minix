/* $NetBSD: sha512.c,v 1.3 2006/10/30 20:22:54 christos Exp $ */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sha2.h>	/* this hash type */
#include <md5.h>	/* the hash we're replacing */

#define HASHTYPE	"SHA512"
#define HASHLEN		128

#define MD5Filter	SHA512_Filter
#define MD5String	SHA512_String
#define MD5TestSuite	SHA512_TestSuite
#define MD5TimeTrial	SHA512_TimeTrial

#define MD5Data		SHA512_Data
#define MD5Init		SHA512_Init
#define MD5Update	SHA512_Update
#define MD5End		SHA512_End

#define MD5_CTX		SHA512_CTX

#include "md5.c"
