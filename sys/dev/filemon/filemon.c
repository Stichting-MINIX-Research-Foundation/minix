/*
 * Copyright (c) 2010, Juniper Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: filemon.c,v 1.11 2015/08/20 14:40:17 christos Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/rwlock.h>
#include <sys/condvar.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/syslog.h>
#include <sys/kauth.h>

#include "filemon.h"
#include "ioconf.h"

MODULE(MODULE_CLASS_DRIVER, filemon, NULL);

static dev_type_open(filemon_open);

struct cdevsw filemon_cdevsw = {
	.d_open = filemon_open,
	.d_close = noclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_MPSAFE
};

static int filemon_ioctl(struct file *, u_long, void *);
static int filemon_close(struct file *);

static const struct fileops filemon_fileops = {
	.fo_ioctl = filemon_ioctl,
	.fo_close = filemon_close,
	.fo_read = fbadop_read,
	.fo_write = fbadop_write,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = fnullop_poll,
	.fo_stat = fbadop_stat,
	.fo_kqfilter = fnullop_kqfilter,
};

static krwlock_t filemon_mtx;

static TAILQ_HEAD(, filemon) filemons_inuse =
	TAILQ_HEAD_INITIALIZER(filemons_inuse);

#ifdef DEBUG
static int logLevel = LOG_DEBUG;
#endif

void
filemon_output(struct filemon * filemon, char *msg, size_t len)
{
	struct uio auio;
	struct iovec aiov;

	if (filemon->fm_fp == NULL)
		return;

	aiov.iov_base = msg;
	aiov.iov_len = len;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = len;
	auio.uio_rw = UIO_WRITE;
	auio.uio_offset = (off_t) - 1;
	uio_setup_sysspace(&auio);

#ifdef DEBUG
	{
		char *cp;
		int x = 16;

		cp = strchr(msg, '\n');
		if (cp && cp - msg <= 16)
			x = (cp - msg) - 2;
		log(logLevel, "filemont_output:('%.*s%s'", x,
		    (x < 16) ? "..." : "", msg);
	}
#endif
	(*filemon->fm_fp->f_ops->fo_write) (filemon->fm_fp,
	    &(filemon->fm_fp->f_offset),
	    &auio, curlwp->l_cred, FOF_UPDATE_OFFSET);
}

void
filemon_printf(struct filemon *filemon, const char *fmt, ...)
{
	size_t len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(filemon->fm_msgbufr, sizeof(filemon->fm_msgbufr),
	    fmt, ap);
	va_end(ap);
	if (len > sizeof(filemon->fm_msgbufr))
		len = sizeof(filemon->fm_msgbufr);
	filemon_output(filemon, filemon->fm_msgbufr, len);
}

static void
filemon_comment(struct filemon * filemon)
{
	filemon_printf(filemon, "# filemon version %d\n# Target pid %d\nV %d\n",
	   FILEMON_VERSION, curproc->p_pid, FILEMON_VERSION);
}


static struct filemon *
filemon_pid_check(struct proc * p)
{
	struct filemon *filemon;
	struct proc * lp;

	if (!TAILQ_EMPTY(&filemons_inuse)) {
		while (p) {
			/*
			 * make sure p cannot exit
			 * until we have moved on to p_pptr
			 */
			rw_enter(&p->p_reflock, RW_READER);
			TAILQ_FOREACH(filemon, &filemons_inuse, fm_link) {
				if (p->p_pid == filemon->fm_pid) {
					rw_exit(&p->p_reflock);
					return (filemon);
				}
			}
			lp = p;
			p = p->p_pptr;
			rw_exit(&lp->p_reflock);
		}
	}
	return (NULL);
}

/*
 * return exclusive access to a filemon struct
 */
struct filemon *
filemon_lookup(struct proc * p)
{
	struct filemon *filemon;

	rw_enter(&filemon_mtx, RW_READER);
	filemon = filemon_pid_check(p);
	if (filemon) {
		rw_enter(&filemon->fm_mtx, RW_WRITER);
	}
	rw_exit(&filemon_mtx);
	return filemon;
}

static struct filemon *
filemon_fp_data(struct file * fp, int lck)
{
	struct filemon *filemon;
	
	rw_enter(&filemon_mtx, RW_READER);
	filemon = fp->f_data;
	if (filemon && lck) {
		rw_enter(&filemon->fm_mtx, lck);
	}
	rw_exit(&filemon_mtx);
	return filemon;
}

static int n_open = 0;

static int
filemon_open(dev_t dev, int oflags __unused, int mode __unused,
    struct lwp * l __unused)
{
	struct filemon *filemon;
	struct file *fp;
	int error, fd;

	/* falloc() will fill in the descriptor for us. */
	if ((error = fd_allocfile(&fp, &fd)) != 0)
		return error;

	filemon = kmem_alloc(sizeof(struct filemon), KM_SLEEP);
	rw_init(&filemon->fm_mtx);
	filemon->fm_fd = -1;
	filemon->fm_fp = NULL;
	filemon->fm_pid = curproc->p_pid;

	rw_enter(&filemon_mtx, RW_WRITER);
	TAILQ_INSERT_TAIL(&filemons_inuse, filemon, fm_link);
	n_open++;
	rw_exit(&filemon_mtx);

	return fd_clone(fp, fd, oflags, &filemon_fileops, filemon);
}


static int
filemon_close(struct file * fp)
{
	struct filemon *filemon;

#ifdef DEBUG
	log(logLevel, "filemon_close()");
#endif
	/*
	 * Follow the same lock order as filemon_lookup()
	 * and filemon_fp_data() but hold exclusive access to
	 * filemon_mtx until we are done.
	 */
	rw_enter(&filemon_mtx, RW_WRITER);
	filemon = fp->f_data;
	if (!filemon) {
		rw_exit(&filemon_mtx);
		return EBADF;
	}
	/* ensure that filemon_lookup() will now fail */
	TAILQ_REMOVE(&filemons_inuse, filemon, fm_link);
	n_open--;
	/* ensure that filemon_fp_data() will now fail */
	fp->f_data = NULL;

	/*
	 * once we have exclusive access, it should never be used again
	 */
	rw_enter(&filemon->fm_mtx, RW_WRITER);
	if (filemon->fm_fp) {
		fd_putfile(filemon->fm_fd);	/* release our reference */
		filemon->fm_fp = NULL;
	}
	rw_exit(&filemon->fm_mtx);
	rw_destroy(&filemon->fm_mtx);
	kmem_free(filemon, sizeof(struct filemon));
	rw_exit(&filemon_mtx);
	return (0);
}

static int
filemon_ioctl(struct file * fp, u_long cmd, void *data)
{
	int error = 0;
	struct filemon *filemon;
	struct proc *tp;

#ifdef DEBUG
	log(logLevel, "filemon_ioctl(%lu)", cmd);;
#endif

	/*
	 * this ensures we cannot get filemon if it is closing.
	 */
	filemon = filemon_fp_data(fp, RW_WRITER);
	if (!filemon)
		return EBADF;

	switch (cmd) {
	case FILEMON_SET_FD:
		/* Set the output file descriptor. */
		filemon->fm_fd = *((int *) data);
		if ((filemon->fm_fp = fd_getfile(filemon->fm_fd)) == NULL) {
			rw_exit(&filemon->fm_mtx);
			return EBADF;
		}
		/* Write the file header. */
		filemon_comment(filemon);
		break;

	case FILEMON_SET_PID:
		/* Set the monitored process ID - if allowed. */
		mutex_enter(proc_lock);
		tp = proc_find(*((pid_t *) data));
		mutex_exit(proc_lock);
		if (tp == NULL) {
			error = ESRCH;
			break;
		}
		error = kauth_authorize_process(curproc->p_cred,
		    KAUTH_PROCESS_CANSEE, tp,
		    KAUTH_ARG(KAUTH_REQ_PROCESS_CANSEE_ENTRY), NULL, NULL);
		if (!error) {
			filemon->fm_pid = tp->p_pid;
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	rw_exit(&filemon->fm_mtx);
	return (error);
}

static void
filemon_load(void *dummy __unused)
{
	rw_init(&filemon_mtx);

	/* Install the syscall wrappers. */
	filemon_wrapper_install();
}

/*
 * If this gets called we are linked into the kernel
 */
void
filemonattach(int num)
{
    filemon_load(NULL);
}


static int
filemon_unload(void)
{
	int error = 0;

	rw_enter(&filemon_mtx, RW_WRITER);

	if (TAILQ_FIRST(&filemons_inuse) != NULL)
		error = EBUSY;
	else {
		/* Deinstall the syscall wrappers. */
		error = filemon_wrapper_deinstall();
	}
	rw_exit(&filemon_mtx);

	if (error == 0) {
		rw_destroy(&filemon_mtx);
	}
	return (error);
}

static int
filemon_modcmd(modcmd_t cmd, void *data)
{
	int error = 0;
	int bmajor = -1;
	int cmajor = -1;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef DEBUG
		logLevel = LOG_INFO;
#endif

		filemon_load(data);
		error = devsw_attach("filemon", NULL, &bmajor,
		    &filemon_cdevsw, &cmajor);
		break;

	case MODULE_CMD_FINI:
		error = filemon_unload();
		if (!error)
			error = devsw_detach(NULL, &filemon_cdevsw);
		break;

	case MODULE_CMD_STAT:
		log(LOG_INFO, "filemon: open=%d", n_open);
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}

	return (error);
}
