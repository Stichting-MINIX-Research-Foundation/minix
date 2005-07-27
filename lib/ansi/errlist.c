/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#include	<errno.h>

static const char unknown[] = "Unknown error";

const char *_sys_errlist[] = {
        "Error 0",			/* EGENERIC */
        "Not owner",			/* EPERM */
        "No such file or directory",	/* ENOENT */
        "No such process",		/* ESRCH */
        "Interrupted system call",	/* EINTR */
        "I/O error",			/* EIO */
        "No such device or address",	/* ENXIO */
        "Arg list too long",		/* E2BIG */
        "Exec format error",		/* ENOEXEC */
        "Bad file number",		/* EBADF */
        "No children",			/* ECHILD */
        "Resource temporarily unavailable",/* EAGAIN */
        "Not enough core",		/* ENOMEM */
        "Permission denied",		/* EACCES */
        "Bad address",			/* EFAULT */
        "Block device required",	/* ENOTBLK */
        "Resource busy",		/* EBUSY */
        "File exists",			/* EEXIST */
        "Cross-device link",		/* EXDEV */
        "No such device",		/* ENODEV */
        "Not a directory",		/* ENOTDIR */
        "Is a directory",		/* EISDIR */
        "Invalid argument",		/* EINVAL */
        "File table overflow",		/* ENFILE */
        "Too many open files",		/* EMFILE */
        "Not a typewriter",		/* ENOTTY */
        "Text file busy",		/* ETXTBSY */
        "File too large",		/* EFBIG */
        "No space left on device",	/* ENOSPC */
        "Illegal seek",			/* ESPIPE */
        "Read-only file system",	/* EROFS */
        "Too many links",		/* EMLINK */
        "Broken pipe",			/* EPIPE */
        "Math argument",		/* EDOM */
        "Result too large",		/* ERANGE */
	"Resource deadlock avoided",	/* EDEADLK */
	"File name too long",		/* ENAMETOOLONG */
	"No locks available",		/* ENOLCK */
	"Function not implemented",	/* ENOSYS */
	"Directory not empty",		/* ENOTEMPTY */
	unknown,			/* 40 */
	unknown,			/* 41 */
	unknown,			/* 42 */
	unknown,			/* 43 */
	unknown,			/* 44 */
	unknown,			/* 45 */
	unknown,			/* 46 */
	unknown,			/* 47 */
	unknown,			/* 48 */
	unknown,			/* 49 */
	"Invalid packet size",		/* EPACKSIZE */
	"Not enough buffers left",	/* EOUTOFBUFS */
	"Illegal ioctl for device",	/* EBADIOCTL */
	"Bad mode for ioctl",		/* EBADMODE */
	"Would block",			/* EWOULDBLOCK */
	"Bad destination address",	/* EBADDEST */
	"Destination not reachable",	/* EDSTNOTRCH */
	"Already connected",		/* EISCONN */
	"Address in use",		/* EADDRINUSE */
	"Connection refused",		/* ECONNREFUSED */
	"Connection reset",		/* ECONNRESET */
	"Connection timed out",		/* ETIMEDOUT */
	"Urgent data present",		/* EURG */
	"No urgent data present",	/* ENOURG */
	"No connection",		/* ENOTCONN */
	"Already shutdown",		/* ESHUTDOWN */
	"No such connection",		/* ENOCONN */
	"Address family not supported",	/* EAFNOSUPPORT */
	"Protocol not supported by AF",	/* EPROTONOSUPPORT */
	"Protocol wrong type for socket", /* EPROTOTYPE */
	"Operation in progress",	/* EINPROGRESS */
	"Address not available",	/* EADDRNOTAVAIL */
	"Connection already in progress", /* EALREADY */
};

const int _sys_nerr = sizeof(_sys_errlist) / sizeof(_sys_errlist[0]);
