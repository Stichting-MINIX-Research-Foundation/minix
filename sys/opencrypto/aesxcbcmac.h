/* $NetBSD: aesxcbcmac.h,v 1.1 2011/05/24 19:10:09 drochner Exp $ */

#include <sys/types.h>

#define AES_BLOCKSIZE   16

typedef struct {
	u_int8_t	e[AES_BLOCKSIZE];
	u_int8_t	buf[AES_BLOCKSIZE];
	size_t		buflen;
	u_int32_t	r_k1s[(RIJNDAEL_MAXNR+1)*4];
	u_int32_t	r_k2s[(RIJNDAEL_MAXNR+1)*4];
	u_int32_t	r_k3s[(RIJNDAEL_MAXNR+1)*4];
	int		r_nr; /* key-length-dependent number of rounds */
	u_int8_t	k2[AES_BLOCKSIZE];
	u_int8_t	k3[AES_BLOCKSIZE];
} aesxcbc_ctx;

int aes_xcbc_mac_init(void *, const u_int8_t *, u_int16_t);
int aes_xcbc_mac_loop(void *, const u_int8_t *, u_int16_t);
void aes_xcbc_mac_result(u_int8_t *, void *);
