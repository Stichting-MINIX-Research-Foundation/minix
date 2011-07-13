/*
 * $NetBSD: crypt.h,v 1.4 2006/10/27 18:22:56 drochner Exp $
 */
char	*__md5crypt(const char *pw, const char *salt);	/* XXX */
char *__bcrypt(const char *, const char *);	/* XXX */
char *__crypt_sha1(const char *pw, const char *salt);
unsigned int __crypt_sha1_iterations (unsigned int hint);
void __hmac_sha1(const unsigned char *, size_t, const unsigned char *, size_t,
		 unsigned char *);
void __crypt_to64(char *s, u_int32_t v, int n);

int __gensalt_blowfish(char *salt, size_t saltlen, const char *option);
int __gensalt_old(char *salt, size_t saltsiz, const char *option);
int __gensalt_new(char *salt, size_t saltsiz, const char *option);
int __gensalt_md5(char *salt, size_t saltsiz, const char *option);
int __gensalt_sha1(char *salt, size_t saltsiz, const char *option);

#define SHA1_MAGIC "$sha1$"
#define SHA1_SIZE 20
