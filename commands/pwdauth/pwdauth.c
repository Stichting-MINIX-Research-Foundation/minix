/*	pwdauth 2.0 - check a shadow password		Author: Kees J. Bot
 *								7 Feb 1994
 *
 * This program gets as input the key and salt arguments of the crypt(3)
 * function as two null terminated strings.  The crypt result is output as
 * one null terminated string.  Input and output must be <= 1024 characters.
 * The exit code will be 1 on any error.
 *
 * If the key has the form '##name' then the key will be encrypted and the
 * result checked to be equal to the encrypted password in the shadow password
 * file.  If equal than '##name' will be returned, otherwise exit code 2.
 *
 * Otherwise the key will be encrypted normally and the result returned.
 *
 * As a special case, anything matches a null encrypted password to allow
 * a no-password login.
 */
#define nil 0
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef __NBSD_LIBC
#define setkey pwdauth_setkey
#define encrypt pwdauth_encrypt
#endif

#define LEN	1024

int main(int argc, char **argv)
{
	char key[LEN];
	char *salt;
	struct passwd *pw;
	int n;

	/* Read input data.  Check if there are exactly two null terminated
	 * strings.
	 */
	n= read(0, key, LEN);
	if (n < 0) return 1;
	salt = key + n;
	n = 0;
	while (salt > key) if (*--salt == 0) n++;
	if (n != 2) return 1;
	salt = key + strlen(key) + 1;

	if (salt[0] == '#' && salt[1] == '#') {
		if ((pw= getpwnam(salt + 2)) == nil) return 2;

		/* A null encrypted password matches a null key, otherwise
		 * do the normal crypt(3) authentication check.
		 */
		if (*pw->pw_passwd == 0 && *key == 0) {
			/* fine */
		} else
		if (strcmp(crypt(key, pw->pw_passwd), pw->pw_passwd) != 0) {
			return 2;
		}
	} else {
		/* Normal encryption. */
		if (*salt == 0 && *key == 0) {
			/* fine */
		} else {
			salt= crypt(key, salt);
		}
	}

	/* Return the (possibly new) salt to the caller. */
	if (write(1, salt, strlen(salt) + 1) < 0) return 1;
	return 0;
}
