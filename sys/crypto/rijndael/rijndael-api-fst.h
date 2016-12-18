/*	$NetBSD: rijndael-api-fst.h,v 1.8 2007/01/21 23:00:08 cbiere Exp $	*/

/**
 * rijndael-api-fst.h
 *
 * @version 2.9 (December 2000)
 *
 * Optimised ANSI C code for the Rijndael cipher (now AES)
 *
 * @author Vincent Rijmen <vincent.rijmen@esat.kuleuven.ac.be>
 * @author Antoon Bosselaers <antoon.bosselaers@esat.kuleuven.ac.be>
 * @author Paulo Barreto <paulo.barreto@terra.com.br>
 *
 * This code is hereby placed in the public domain.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Acknowledgements:
 *
 * We are deeply indebted to the following people for their bug reports,
 * fixes, and improvement suggestions to this implementation. Though we
 * tried to list all contributions, we apologise in advance for any
 * missing reference.
 *
 * Andrew Bales <Andrew.Bales@Honeywell.com>
 * Markus Friedl <markus.friedl@informatik.uni-erlangen.de>
 * John Skodon <skodonj@webquill.com>
 */

#ifndef __RIJNDAEL_API_FST_H
#define __RIJNDAEL_API_FST_H

#include "rijndael-alg-fst.h"

/*  Generic Defines  */
#define     DIR_ENCRYPT           0 /*  Are we encrpyting?  */
#define     DIR_DECRYPT           1 /*  Are we decrpyting?  */
#define     MODE_ECB              1 /*  Are we ciphering in ECB mode?   */
#define     MODE_CBC              2 /*  Are we ciphering in CBC mode?   */
#define     MODE_CFB1             3 /*  Are we ciphering in 1-bit CFB mode? */
#define     TRUE                  1
#define     FALSE                 0
#define     BITSPERBLOCK        128 /* Default number of bits in a cipher block */

/*  Error Codes  */
#define     BAD_KEY_DIR          -1 /*  Key direction is invalid, e.g., unknown value */
#define     BAD_KEY_MAT          -2 /*  Key material not of correct length */
#define     BAD_KEY_INSTANCE     -3 /*  Key passed is not valid */
#define     BAD_CIPHER_MODE      -4 /*  Params struct passed to cipherInit invalid */
#define     BAD_CIPHER_STATE     -5 /*  Cipher in wrong state (e.g., not initialized) */
#define     BAD_BLOCK_LENGTH     -6
#define     BAD_CIPHER_INSTANCE  -7
#define     BAD_DATA             -8 /*  Data contents are invalid, e.g., invalid padding */
#define     BAD_OTHER            -9 /*  Unknown error */

/*  Algorithm-specific Defines  */
#define     RIJNDAEL_MAX_KEY_SIZE         64 /* # of ASCII char's needed to represent a key */
#define     RIJNDAEL_MAX_IV_SIZE          16 /* # bytes needed to represent an IV  */

/*  Typedefs  */

typedef unsigned char   BYTE;

/*  The structure for key information */
typedef struct {
    BYTE  direction;                /* Key used for encrypting or decrypting? */
    int   keyLen;                   /* Length of the key  */
    char  keyMaterial[RIJNDAEL_MAX_KEY_SIZE+1];  /* Raw key data in ASCII, e.g., user input or KAT values */
	int   Nr;                       /* key-length-dependent number of rounds */
	u_int32_t   rk[4*(RIJNDAEL_MAXNR + 1)];        /* key schedule */
	u_int32_t   ek[4*(RIJNDAEL_MAXNR + 1)];        /* CFB1 key schedule (encryption only) */
} keyInstance;

/*  The structure for cipher information */
typedef struct {                    /* changed order of the components */
    u_int32_t  IV[RIJNDAEL_MAX_IV_SIZE / sizeof(u_int32_t)];
			/* A possible Initialization Vector for ciphering */
    BYTE  mode;                     /* MODE_ECB, MODE_CBC, or MODE_CFB1 */
} cipherInstance;

/*  Function prototypes  */

int rijndael_makeKey(keyInstance *, BYTE, int, const char *);

int rijndael_cipherInit(cipherInstance *, BYTE, const char *);

int rijndael_blockEncrypt(cipherInstance *, keyInstance *, const BYTE *, int, BYTE *);

int rijndael_padEncrypt(cipherInstance *, keyInstance *, const BYTE *, int, BYTE *);

int rijndael_blockDecrypt(cipherInstance *, keyInstance *, const BYTE *, int, BYTE *);

int rijndael_padDecrypt(cipherInstance *, keyInstance *, const BYTE *, int, BYTE *);

#endif /* __RIJNDAEL_API_FST_H */
