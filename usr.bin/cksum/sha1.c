/*	$NetBSD: sha1.c,v 1.1 2001/03/20 18:46:27 atatat Exp $	*/

#include <sha1.h>	/* this hash type */
#include <md5.h>	/* the hash we're replacing */

#define HASHTYPE	"SHA1"
#define HASHLEN		40

#define MD5Filter	SHA1Filter
#define MD5String	SHA1String
#define MD5TestSuite	SHA1TestSuite
#define MD5TimeTrial	SHA1TimeTrial

#define MD5Data		SHA1Data
#define MD5Init		SHA1Init
#define MD5Update	SHA1Update
#define MD5End		SHA1End

#define MD5_CTX		SHA1_CTX

#include "md5.c"
