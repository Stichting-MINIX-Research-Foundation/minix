/*	$NetBSD: simple_exec_w32.c,v 1.1.1.2 2014/04/24 12:45:52 pettai Exp $	*/

/***********************************************************************
 * Copyright (c) 2009, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **********************************************************************/

#include <config.h>

#include <krb5/roken.h>
#include <strsafe.h>

#ifndef _WIN32
#error This implementation is Windows specific.
#endif

/**
 * wait_for_process_timed waits for a process to terminate or until a
 * specified timeout occurs.
 *
 * @param[in] pid Process id for the monitored process

 * @param[in] func Timeout callback function.  When the wait times out,
 *     the callback function is called.  The possible return values
 *     from the callback function are:
 *
 * - ((time_t) -2) Exit loop without killing child and return SE_E_EXECTIMEOUT.
 * - ((time_t) -1) Kill child with SIGTERM and wait for child to exit.
 * - 0             Don't timeout again
 * - n             Seconds to next timeout
 *
 * @param[in] ptr Optional parameter for func()
 *
 * @param[in] timeout Seconds to first timeout.
 *
 * @retval SE_E_UNSPECIFIED   Unspecified system error
 * @retval SE_E_FORKFAILED    Fork failure (not applicable for _WIN32 targets)
 * @retval SE_E_WAITPIDFAILED waitpid errors
 * @retval SE_E_EXECTIMEOUT   exec timeout
 * @retval 0 <= Return value  from subprocess
 * @retval SE_E_NOTFOUND      The program coudln't be found
 * @retval 128- The signal that killed the subprocess +128.
 */
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
wait_for_process_timed(pid_t pid, time_t (*func)(void *),
		       void *ptr, time_t timeout)
{
    HANDLE hProcess;
    DWORD wrv = 0;
    DWORD dtimeout;
    int rv = 0;

    hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);

    if (hProcess == NULL) {
        return SE_E_WAITPIDFAILED;
    }

    dtimeout = (DWORD) ((timeout == 0)? INFINITE: timeout * 1000);

    do {
	wrv = WaitForSingleObject(hProcess, dtimeout);

	if (wrv == WAIT_OBJECT_0) {

	    DWORD prv = 0;

	    GetExitCodeProcess(hProcess, &prv);
	    rv = (int) prv;
	    break;

	} else if (wrv == WAIT_TIMEOUT) {

	    if (func == NULL)
		continue;

	    timeout = (*func)(ptr);

	    if (timeout == (time_t)-1) {

		if (TerminateProcess(hProcess, 128 + 9)) {
		    dtimeout = INFINITE;
		    continue;
		}
		rv = SE_E_UNSPECIFIED;
		break;

	    } else if (timeout == (time_t) -2) {

		rv = SE_E_EXECTIMEOUT;
		break;

	    } else {

		dtimeout = (DWORD) ((timeout == 0)? INFINITE: timeout * 1000);
		continue;

	    }

	} else {

	    rv = SE_E_UNSPECIFIED;
	    break;

	}

    } while(TRUE);

    CloseHandle(hProcess);

    return rv;
}

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
wait_for_process(pid_t pid)
{
    return wait_for_process_timed(pid, NULL, NULL, 0);
}

static char *
collect_commandline(const char * fn, va_list * ap)
{
    size_t len = 0;
    size_t alloc_len = 0;
    const char * s;
    char * cmd = NULL;

    for (s = fn; s; s = (char *) va_arg(*ap, char *)) {
	size_t cmp_len;
	int need_quote = FALSE;

	if (FAILED(StringCchLength(s, MAX_PATH, &cmp_len))) {
	    if (cmd)
		free(cmd);
	    return NULL;
	}

	if (cmp_len == 0)
	    continue;

	if (strchr(s, ' ') &&	/* need to quote any component that
				   has embedded spaces, but not if
				   they are already quoted. */
	    s[0] != '"' &&
	    s[cmp_len - 1] != '"') {
	    need_quote = TRUE;
	    cmp_len += 2 * sizeof(char);
	}

	if (s != fn)
	    cmp_len += 1 * sizeof(char);

	if (alloc_len < len + cmp_len + 1) {
	    char * nc;

	    alloc_len += ((len + cmp_len - alloc_len) / MAX_PATH + 1) * MAX_PATH;
	    nc = (char *) realloc(cmd, alloc_len * sizeof(char));
	    if (nc == NULL) {
		if (cmd)
		    free(cmd);
		return NULL;
	    }
	}

	if (cmd == NULL)
	    return NULL;

	if (s != fn)
	    cmd[len++] = ' ';

	if (need_quote) {
	    StringCchPrintf(cmd + len, alloc_len - len, "\"%s\"", s);
	} else {
	    StringCchCopy(cmd + len, alloc_len - len, s);
	}

	len += cmp_len;
    }

    return cmd;
}

ROKEN_LIB_FUNCTION pid_t ROKEN_LIB_CALL
pipe_execv(FILE **stdin_fd, FILE **stdout_fd, FILE **stderr_fd,
	   const char *file, ...)
{
    HANDLE  hOut_r = NULL;
    HANDLE  hOut_w = NULL;
    HANDLE  hIn_r  = NULL;
    HANDLE  hIn_w  = NULL;
    HANDLE  hErr_r = NULL;
    HANDLE  hErr_w = NULL;

    SECURITY_ATTRIBUTES sa;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    char * commandline = NULL;

    pid_t rv = (pid_t) -1;

    {
	va_list ap;

	va_start(ap, file);
	commandline = collect_commandline(file, &ap);

	if (commandline == NULL)
	    return rv;
    }

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&sa, sizeof(sa));

    pi.hProcess = NULL;
    pi.hThread = NULL;

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if ((stdout_fd && !CreatePipe(&hOut_r, &hOut_w, &sa, 0 /* Use default */)) ||

	(stdin_fd && !CreatePipe(&hIn_r, &hIn_w, &sa, 0)) ||

	(stderr_fd && !CreatePipe(&hErr_r, &hErr_w, &sa, 0)) ||

	(!stdout_fd && (hOut_w = CreateFile("CON", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
					    &sa, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE) ||

	(!stdin_fd && (hIn_r = CreateFile("CON",GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
					  &sa, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE) ||

	(!stderr_fd && (hErr_w = CreateFile("CON", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
					    &sa, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE))

	goto _exit;

    /* We don't want the child processes inheriting these */
    if (hOut_r)
	SetHandleInformation(hOut_r, HANDLE_FLAG_INHERIT, FALSE);

    if (hIn_w)
	SetHandleInformation(hIn_w, HANDLE_FLAG_INHERIT, FALSE);

    if (hErr_r)
	SetHandleInformation(hErr_r, HANDLE_FLAG_INHERIT, FALSE);

    si.cb = sizeof(si);
    si.lpReserved = NULL;
    si.lpDesktop = NULL;
    si.lpTitle = NULL;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hIn_r;
    si.hStdOutput = hOut_w;
    si.hStdError = hErr_w;

    if (!CreateProcess(file, commandline, NULL, NULL,
		       TRUE,	/* bInheritHandles */
		       CREATE_NO_WINDOW, /* dwCreationFlags */
		       NULL,		 /* lpEnvironment */
		       NULL,		 /* lpCurrentDirectory */
		       &si,
		       &pi)) {

	rv = (pid_t) (GetLastError() == ERROR_FILE_NOT_FOUND)? 127 : -1;
	goto _exit;
    }

    if (stdin_fd) {
	*stdin_fd = _fdopen(_open_osfhandle((intptr_t) hIn_w, 0), "wb");
	if (*stdin_fd)
	    hIn_w = NULL;
    }

    if (stdout_fd) {
	*stdout_fd = _fdopen(_open_osfhandle((intptr_t) hOut_r, _O_RDONLY), "rb");
	if (*stdout_fd)
	    hOut_r = NULL;
    }

    if (stderr_fd) {
	*stderr_fd = _fdopen(_open_osfhandle((intptr_t) hErr_r, _O_RDONLY), "rb");
	if (*stderr_fd)
	    hErr_r = NULL;
    }

    rv = (pid_t) pi.dwProcessId;

 _exit:

    if (pi.hProcess) CloseHandle(pi.hProcess);

    if (pi.hThread) CloseHandle(pi.hThread);

    if (hIn_r) CloseHandle(hIn_r);

    if (hIn_w) CloseHandle(hIn_w);

    if (hOut_r) CloseHandle(hOut_r);

    if (hOut_w) CloseHandle(hOut_w);

    if (hErr_r) CloseHandle(hErr_r);

    if (hErr_w) CloseHandle(hErr_w);

    return rv;
}


ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
simple_execvp_timed(const char *file, char *const args[],
		    time_t (*func)(void *), void *ptr, time_t timeout)
{
    intptr_t hp;
    int rv;

    hp = spawnvp(_P_NOWAIT, file, args);

    if (hp == -1)
	return (errno == ENOENT)? 127: 126;
    else if (hp == 0)
	return 0;

    rv = wait_for_process_timed(GetProcessId((HANDLE) hp), func, ptr, timeout);

    CloseHandle((HANDLE) hp);

    return rv;
}


ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
simple_execvp(const char *file, char *const args[])
{
    return simple_execvp_timed(file, args, NULL, NULL, 0);
}


ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
simple_execve_timed(const char *file, char *const args[], char *const envp[],
		    time_t (*func)(void *), void *ptr, time_t timeout)
{
    intptr_t hp;
    int rv;

    hp = spawnve(_P_NOWAIT, file, args, envp);

    if (hp == -1)
	return (errno == ENOENT)? 127: 126;
    else if (hp == 0)
	return 0;

    rv = wait_for_process_timed(GetProcessId((HANDLE) hp), func, ptr, timeout);

    CloseHandle((HANDLE) hp);

    return rv;
}


ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
simple_execve(const char *file, char *const args[], char *const envp[])
{
    return simple_execve_timed(file, args, envp, NULL, NULL, 0);
}


ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
simple_execlp(const char *file, ...)
{
    va_list ap;
    char **argv;
    int ret;

    va_start(ap, file);
    argv = vstrcollect(&ap);
    va_end(ap);
    if(argv == NULL)
	return SE_E_UNSPECIFIED;
    ret = simple_execvp(file, argv);
    free(argv);
    return ret;
}


ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
simple_execle(const char *file, ... /* ,char *const envp[] */)
{
    va_list ap;
    char **argv;
    char *const* envp;
    int ret;

    va_start(ap, file);
    argv = vstrcollect(&ap);
    envp = va_arg(ap, char **);
    va_end(ap);
    if(argv == NULL)
	return SE_E_UNSPECIFIED;
    ret = simple_execve(file, argv, envp);
    free(argv);
    return ret;
}
