/*	$NetBSD: write_pid.c,v 1.2 2017/01/28 21:31:50 christos Exp $	*/

/*
 * Copyright (c) 1999 - 2001 Kungliga Tekniska Högskolan
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

#ifdef HAVE_UTIL_H
#include <util.h>
#endif

ROKEN_LIB_FUNCTION char * ROKEN_LIB_CALL
pid_file_write(const char *progname)
{
    const char *pidfile_dir = NULL;
    char *ret = NULL;
    FILE *fp;

    /*
     * Maybe we could have a version of this function (and pidfile())
     * where we get a directory from the caller.  That would allow us to
     * have command-line options for the daemons for this.
     *
     * For now we use an environment variable.
     */
    if (!issuid())
        pidfile_dir = getenv("HEIM_PIDFILE_DIR");
    if (pidfile_dir == NULL)
        pidfile_dir = _PATH_VARRUN;

    if (asprintf(&ret, "%s%s.pid", pidfile_dir, progname) < 0 || ret == NULL)
	return NULL;
    fp = fopen(ret, "w");
    if (fp == NULL) {
	free(ret);
	return NULL;
    }
    fprintf(fp, "%lu\n", (unsigned long)getpid());
    fclose(fp);
    return ret;
}

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
pid_file_delete(char **filename)
{
    if (*filename != NULL) {
	unlink(*filename);
	free(*filename);
	*filename = NULL;
    }
}

static char *pidfile_path;
static pid_t pidfile_pid;

static void
pidfile_cleanup(void)
{
    if (pidfile_path != NULL && pidfile_pid == getpid())
	pid_file_delete(&pidfile_path);
}

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
rk_pidfile(const char *bname)
{
    /*
     * If the OS has a pidfile(), call that, but still call
     * pid_file_write().  Even if both want to write the same file,
     * writing it twice will still work.
     */
#ifdef HAVE_PIDFILE
    pidfile(bname);
#endif

    if (pidfile_path != NULL)
	return;
    if (bname == NULL)
	bname = getprogname();
    pidfile_path = pid_file_write(bname);
    pidfile_pid = getpid();
#if defined(HAVE_ATEXIT)
    if (pidfile_path != NULL)
        atexit(pidfile_cleanup);
#elif defined(HAVE_ON_EXIT)
    if (pidfile_path != NULL)
        on_exit(pidfile_cleanup);
#endif
}
