/*	$NetBSD: md2.c,v 1.1 2001/03/20 18:46:26 atatat Exp $	*/

#include <md2.h>	/* this hash type */
#include <md5.h>	/* the hash we're replacing */

#define HASHTYPE	"MD2"
#define HASHLEN		32

#define MD5Filter	MD2Filter
#define MD5String	MD2String
#define MD5TestSuite	MD2TestSuite
#define MD5TimeTrial	MD2TimeTrial

#define MD5Data		MD2Data
#define MD5Init		MD2Init
#define MD5Update	MD2Update
#define MD5End		MD2End

#define MD5_CTX		MD2_CTX

#include "md5.c"
