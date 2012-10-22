/*	$NetBSD: md4.c,v 1.1 2001/03/20 18:46:26 atatat Exp $	*/

#include <md4.h>	/* this hash type */
#include <md5.h>	/* the hash we're replacing */

#define HASHTYPE	"MD4"
#define HASHLEN		32

#define MD5Filter	MD4Filter
#define MD5String	MD4String
#define MD5TestSuite	MD4TestSuite
#define MD5TimeTrial	MD4TimeTrial

#define MD5Data		MD4Data
#define MD5Init		MD4Init
#define MD5Update	MD4Update
#define MD5End		MD4End

#define MD5_CTX		MD4_CTX

#include "md5.c"
