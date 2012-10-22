/*	$NetBSD: md5.c,v 1.10 2008/12/29 00:51:29 christos Exp $	*/

/*
 * MDDRIVER.C - test driver for MD2, MD4 and MD5
 */

/*
 *  Copyright (C) 1990-2, RSA Data Security, Inc. Created 1990. All
 *  rights reserved.
 *
 *  RSA Data Security, Inc. makes no representations concerning either
 *  the merchantability of this software or the suitability of this
 *  software for any particular purpose. It is provided "as is"
 *  without express or implied warranty of any kind.
 *
 *  These notices must be retained in any copies of any part of this
 *  documentation and/or software.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: md5.c,v 1.10 2008/12/29 00:51:29 christos Exp $");
#endif /* not lint */

#include <sys/types.h>

#include <err.h>
#include <md5.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void	MD5Filter(int);
void	MD5String(const char *);
void	MD5TestSuite(void);
void	MD5TimeTrial(void);

#ifndef HASHTYPE
#define HASHTYPE "MD5"
#endif

#ifndef HASHLEN
#define HASHLEN 32
#endif

/*
 * Length of test block, number of test blocks.
 */
#define TEST_BLOCK_LEN 1000
#define TEST_BLOCK_COUNT 1000

/*
 * Digests a string and prints the result.
 */
void
MD5String(const char *string)
{
	unsigned int len = strlen(string);
	char buf[HASHLEN + 1];

	printf("%s (\"%s\") = %s\n", HASHTYPE, string,
	       MD5Data((const unsigned char *)string, len, buf));
}

/*
 * Measures the time to digest TEST_BLOCK_COUNT TEST_BLOCK_LEN-byte blocks.
 */
void
MD5TimeTrial(void)
{
	MD5_CTX context;
	time_t endTime, startTime;
	unsigned char block[TEST_BLOCK_LEN];
	unsigned int i;
	char *p, buf[HASHLEN + 1];

	printf("%s time trial.  Digesting %d %d-byte blocks ...", HASHTYPE,
	    TEST_BLOCK_LEN, TEST_BLOCK_COUNT);
	fflush(stdout);

	/* Initialize block */
	for (i = 0; i < TEST_BLOCK_LEN; i++)
		block[i] = (unsigned char) (i & 0xff);

	/* Start timer */
	time(&startTime);

	/* Digest blocks */
	MD5Init(&context);
	for (i = 0; i < TEST_BLOCK_COUNT; i++)
		MD5Update(&context, block, TEST_BLOCK_LEN);
	p = MD5End(&context,buf);

	/* Stop timer */
	time(&endTime);

	printf(" done\n");
	printf("Digest = %s\n", p);
	printf("Time = %ld seconds\n", (long) (endTime - startTime));

	/*
	 * Be careful that endTime-startTime is not zero.
	 * (Bug fix from Ric * Anderson, ric@Artisoft.COM.)
	 */
	printf("Speed = %lld bytes/second\n",
	    (long long) TEST_BLOCK_LEN * TEST_BLOCK_COUNT /
	    ((endTime - startTime) != 0 ? (endTime - startTime) : 1));
}

/*
 * Digests a reference suite of strings and prints the results.
 */
void
MD5TestSuite(void)
{
	printf("%s test suite:\n", HASHTYPE);

	MD5String("");
	MD5String("a");
	MD5String("abc");
	MD5String("message digest");
	MD5String("abcdefghijklmnopqrstuvwxyz");
	MD5String("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
	MD5String
	    ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
	MD5String
	    ("1234567890123456789012345678901234567890\
1234567890123456789012345678901234567890");
}

/*
 * Digests the standard input and prints the result.
 */
void
MD5Filter(int pipe)
{
	MD5_CTX context;
	size_t len;
	unsigned char buffer[BUFSIZ];
	char buf[HASHLEN + 1];

	MD5Init(&context);
	while ((len = fread(buffer, (size_t)1, (size_t)BUFSIZ, stdin)) > 0) {
		if (pipe && (len != fwrite(buffer, (size_t)1, len, stdout)))
			err(1, "stdout");
		MD5Update(&context, buffer, (unsigned int)len);
	}
	printf("%s\n", MD5End(&context,buf));
}
