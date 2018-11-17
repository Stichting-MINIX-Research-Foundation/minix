/*	$NetBSD: getxxyyy.c,v 1.2.8.1 2017/09/11 04:58:44 snj Exp $	*/

/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <krb5/roken.h>

#ifdef TEST_GETXXYYY
#undef rk_getpwnam_r
#undef rk_getpwuid_r

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
rk_getpwnam_r(const char *, struct passwd *, char *, size_t, struct passwd **);
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
rk_getpwuid_r(uid_t, struct passwd *, char *, size_t, struct passwd **);
#endif

#if !defined(POSIX_GETPWUID_R) || !defined(POSIX_GETPWNAM_R) || defined(TEST_GETXXYYY)
static void
copypw(struct passwd *pwd, char *buffer, size_t bufsize, const struct passwd *p)
{
     memset(pwd, 0, sizeof(*pwd));

#define APPEND(el)					\
do {							\
     slen = strlen(p->el) + 1;				\
     if (slen > bufsize) return (errno = ENOMEM);	\
     memcpy(buffer, p->el, slen);			\
     pwd->el = buffer;					\
     buffer += slen;					\
     bufsize -= slen;					\
} while(0)
     
     APPEND(pw_name);
     if (p->pw_passwd)
	 APPEND(pw_name);
     pwd->pw_uid = p->pw_uid;
     pwd->pw_gid = p->pw_gid;
     APPEND(pw_gecos);
     APPEND(pw_dir);
     APPEND(pw_shell);
}

#if !defined(POSIX_GETPWUID_R) || defined(TEST_GETXXYYY)
/*
 * At least limit the race between threads
 */

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
rk_getpwnam_r(const char *name, struct passwd *pwd, char *buffer,
	      size_t bufsize, struct passwd **result)
{
     struct passwd *p;
     size_t slen, n = 0;
     
     *result = NULL;

     p = getpwnam(name);
     if(p == NULL)
	 return (errno = ENOENT);
	 
     copypw(pwd, buffer, bufsize, p);

     *result = pwd;

     return 0;
}

#if !defined(POSIX_GETPWNAM_R) || defined(TEST_GETXXYYY)

/*
 * At least limit the race between threads
 */

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
rk_getpwnam_r(const char *name, struct passwd *pwd, char *buffer,
	      size_t bufsize, struct passwd **result)
{
     struct passwd *p;
     size_t slen, n = 0;
     
     *result = NULL;

     p = getpwnam(name);
     if(p == NULL)
	 return (errno = ENOENT);
	 
     copypw(pwd, buffer, bufsize, p);

     *result = pwd;

     return 0;
}

#endif /* POSIX_GETPWNAM_R */

#ifdef TEST_GETXXYYY

#include <err.h>

int verbose_flag = 0;

static void
print_result(struct passwd *p)
{
    if (!verbose_flag)
	return;
    printf("%s\n", p->pw_name);
    printf("%d\n", (int)p->pw_uid);
    printf("%s\n", p->pw_shell);
    printf("%s\n", p->pw_dir);
}

int
main(int argc, char **argv)
{
    struct passwd pwd, *result;
    char buf[1024];
    int ret;
    const char *user;

    user = getenv("USER");
    if (!user)
	user = "root";

    ret = rk_getpwnam_r(user, &pwd, buf, sizeof(buf), &result);
    if (ret)
	errx(1, "rk_getpwnam_r");
    print_result(result);

    ret = rk_getpwnam_r(user, &pwd, buf, 1, &result);
    if (ret == 0)
	errx(1, "rk_getpwnam_r too small buf");

    ret = rk_getpwnam_r("no-user-here-promise", &pwd, buf, sizeof(buf), &result);
    if (ret == 0)
	errx(1, "rk_getpwnam_r no user");

    ret = rk_getpwuid_r(0, &pwd, buf, sizeof(buf), &result);
    if (ret)
	errx(1, "rk_getpwuid_r");
    print_result(result);

    ret = rk_getpwuid_r(0, &pwd, buf, 1, &result);
    if (ret == 0)
	errx(1, "rk_getpwuid_r too small buf");

    ret = rk_getpwuid_r(-1234, &pwd, buf, sizeof(buf), &result);
    if (ret == 0)
	errx(1, "rk_getpwuid_r no user");
    return 0;
}

#endif
