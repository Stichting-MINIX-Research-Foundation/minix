/*
 * rijndael-alg.h   v2.4   April '2000
 *
 * Optimised ANSI C code
 */

#ifndef __RIJNDAEL_ALG_H
#define __RIJNDAEL_ALG_H

#define MAXKC			(256/32)
#define MAXROUNDS		14

/* Fix me: something generic based on inttypes.h */
#include "word_i386.h"

int rijndael_KeySched(word8 k[MAXKC][4], word8 rk[MAXROUNDS+1][4][4], int ROUNDS);

int rijndael_KeyEncToDec(word8 W[MAXROUNDS+1][4][4], int ROUNDS);

int rijndael_Encrypt(const void *a, void *b, word8 rk[MAXROUNDS+1][4][4], int ROUNDS);

#ifdef INTERMEDIATE_VALUE_KAT
int rijndaelEncryptRound(word8 a[4][4], word8 rk[MAXROUNDS+1][4][4], int ROUNDS, int rounds);
#endif /* INTERMEDIATE_VALUE_KAT */

int rijndael_Decrypt(const void *a, void *b, word8 rk[MAXROUNDS+1][4][4], int ROUNDS);

#ifdef INTERMEDIATE_VALUE_KAT
int rijndaelDecryptRound(word8 a[4][4], word8 rk[MAXROUNDS+1][4][4], int ROUNDS, int rounds);
#endif /* INTERMEDIATE_VALUE_KAT */

#endif /* __RIJNDAEL_ALG_H */

/*
 * $PchId: rijndael-alg.h,v 1.3 2003/09/29 09:19:17 philip Exp $
 */
