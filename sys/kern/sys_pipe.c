/*	$NetBSD: sys_pipe.c,v 1.140 2014/09/05 09:20:59 matt Exp $	*/

/*-
 * Copyright (c) 2003, 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg, and by Andrew Doran.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996 John S. Dyson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    John S. Dyson.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 */

/*
 * This file contains a high-performance replacement for the socket-based
 * pipes scheme originally used.  It does not support all features of
 * sockets, but does do everything that pipes normally do.
 *
 * This code has two modes of operation, a small write mode and a large
 * write mode.  The small write mode acts like conventional pipes with
 * a kernel buffer.  If the buffer is less than PIPE_MINDIRECT, then the
 * "normal" pipe buffering is done.  If the buffer is between PIPE_MINDIRECT
 * and PIPE_SIZE in size it is mapped read-only into the kernel address space
 * using the UVM page loan facility from where the receiving process can copy
 * the data directly from the pages in the sending process.
 *
 * The constant PIPE_MINDIRECT is chosen to make sure that buffering will
 * happen for small transfers so that the system will not spend all of
 * its time context switching.  PIPE_SIZE is constrained by the
 * amount of kernel virtual memory.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sys_pipe.c,v 1.140 2014/09/05 09:20:59 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/ttycom.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#include <sys/atomic.h>
#include <sys/pipe.h>

#include <uvm/uvm_extern.h>

/*
 * Use this to disable direct I/O and decrease the code size:
 * #define PIPE_NODIRECT
 */

/* XXX Disabled for now; rare hangs switching between direct/buffered */        
#define PIPE_NODIRECT

static int	pipe_read(file_t *, off_t *, struct uio *, kauth_cred_t, int);
static int	pipe_write(file_t *, off_t *, struct uio *, kauth_cred_t, int);
static int	pipe_close(file_t *);
static int	pipe_poll(file_t *, int);
static int	pipe_kqfilter(file_t *, struct knote *);
static int	pipe_stat(file_t *, struct stat *);
static int	pipe_ioctl(file_t *, u_long, void *);
static void	pipe_restart(file_t *);

static const struct fileops pipeops = {
	.fo_read = pipe_read,
	.fo_write = pipe_write,
	.fo_ioctl = pipe_ioctl,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = pipe_poll,
	.fo_stat = pipe_stat,
	.fo_close = pipe_close,
	.fo_kqfilter = pipe_kqfilter,
	.fo_restart = pipe_restart,
};

/*
 * Default pipe buffer size(s), this can be kind-of large now because pipe
 * space is pageable.  The pipe code will try to maintain locality of
 * reference for performance reasons, so small amounts of outstanding I/O
 * will not wipe the cache.
 */
#define	MINPIPESIZE	(PIPE_SIZE / 3)
#define	MAXPIPESIZE	(2 * PIPE_SIZE / 3)

/*
 * Maximum amount of kva for pipes -- this is kind-of a soft limit, but
 * is there so that on large systems, we don't exhaust it.
 */
#define	MAXPIPEKVA	(8 * 1024 * 1024)
static u_int	maxpipekva = MAXPIPEKVA;

/*
 * Limit for direct transfers, we cannot, of course limit
 * the amount of kva for pipes in general though.
 */
#define	LIMITPIPEKVA	(16 * 1024 * 1024)
static u_int	limitpipekva = LIMITPIPEKVA;

/*
 * Limit the number of "big" pipes
 */
#define	LIMITBIGPIPES	32
static u_int	maxbigpipes = LIMITBIGPIPES;
static u_int	nbigpipe = 0;

/*
 * Amount of KVA consumed by pipe buffers.
 */
static u_int	amountpipekva = 0;

static void	pipeclose(struct pipe *);
static void	pipe_free_kmem(struct pipe *);
static int	pipe_create(struct pipe **, pool_cache_t);
static int	pipelock(struct pipe *, bool);
static inline void pipeunlock(struct pipe *);
static void	pipeselwakeup(struct pipe *, struct pipe *, int);
#ifndef PIPE_NODIRECT
static int	pipe_direct_write(file_t *, struct pipe *, struct uio *);
#endif
static int	pipespace(struct pipe *, int);
static int	pipe_ctor(void *, void *, int);
static void	pipe_dtor(void *, void *);

#ifndef PIPE_NODIRECT
static int	pipe_loan_alloc(struct pipe *, int);
static void	pipe_loan_free(struct pipe *);
#endif /* PIPE_NODIRECT */

static pool_cache_t	pipe_wr_cache;
static pool_cache_t	pipe_rd_cache;

void
pipe_init(void)
{

	/* Writer side is not automatically allocated KVA. */
	pipe_wr_cache = pool_cache_init(sizeof(struct pipe), 0, 0, 0, "pipewr",
	    NULL, IPL_NONE, pipe_ctor, pipe_dtor, NULL);
	KASSERT(pipe_wr_cache != NULL);

	/* Reader side gets preallocated KVA. */
	pipe_rd_cache = pool_cache_init(sizeof(struct pipe), 0, 0, 0, "piperd",
	    NULL, IPL_NONE, pipe_ctor, pipe_dtor, (void *)1);
	KASSERT(pipe_rd_cache != NULL);
}

static int
pipe_ctor(void *arg, void *obj, int flags)
{
	struct pipe *pipe;
	vaddr_t va;

	pipe = obj;

	memset(pipe, 0, sizeof(struct pipe));
	if (arg != NULL) {
		/* Preallocate space. */
		va = uvm_km_alloc(kernel_map, PIPE_SIZE, 0,
		    UVM_KMF_PAGEABLE | UVM_KMF_WAITVA);
		KASSERT(va != 0);
		pipe->pipe_kmem = va;
		atomic_add_int(&amountpipekva, PIPE_SIZE);
	}
	cv_init(&pipe->pipe_rcv, "pipe_rd");
	cv_init(&pipe->pipe_wcv, "pipe_wr");
	cv_init(&pipe->pipe_draincv, "pipe_drn");
	cv_init(&pipe->pipe_lkcv, "pipe_lk");
	selinit(&pipe->pipe_sel);
	pipe->pipe_state = PIPE_SIGNALR;

	return 0;
}

static void
pipe_dtor(void *arg, void *obj)
{
	struct pipe *pipe;

	pipe = obj;

	cv_destroy(&pipe->pipe_rcv);
	cv_destroy(&pipe->pipe_wcv);
	cv_destroy(&pipe->pipe_draincv);
	cv_destroy(&pipe->pipe_lkcv);
	seldestroy(&pipe->pipe_sel);
	if (pipe->pipe_kmem != 0) {
		uvm_km_free(kernel_map, pipe->pipe_kmem, PIPE_SIZE,
		    UVM_KMF_PAGEABLE);
		atomic_add_int(&amountpipekva, -PIPE_SIZE);
	}
}

/*
 * The pipe system call for the DTYPE_PIPE type of pipes
 */
int
pipe1(struct lwp *l, register_t *retval, int flags)
{
	struct pipe *rpipe, *wpipe;
	file_t *rf, *wf;
	int fd, error;
	proc_t *p;

	if (flags & ~(O_CLOEXEC|O_NONBLOCK|O_NOSIGPIPE))
		return EINVAL;
	p = curproc;
	rpipe = wpipe = NULL;
	if ((error = pipe_create(&rpipe, pipe_rd_cache)) ||
	    (error = pipe_create(&wpipe, pipe_wr_cache))) {
		goto free2;
	}
	rpipe->pipe_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);
	wpipe->pipe_lock = rpipe->pipe_lock;
	mutex_obj_hold(wpipe->pipe_lock);

	error = fd_allocfile(&rf, &fd);
	if (error)
		goto free2;
	retval[0] = fd;

	error = fd_allocfile(&wf, &fd);
	if (error)
		goto free3;
	retval[1] = fd;

	rf->f_flag = FREAD | flags;
	rf->f_type = DTYPE_PIPE;
	rf->f_pipe = rpipe;
	rf->f_ops = &pipeops;
	fd_set_exclose(l, (int)retval[0], (flags & O_CLOEXEC) != 0);

	wf->f_flag = FWRITE | flags;
	wf->f_type = DTYPE_PIPE;
	wf->f_pipe = wpipe;
	wf->f_ops = &pipeops;
	fd_set_exclose(l, (int)retval[1], (flags & O_CLOEXEC) != 0);

	rpipe->pipe_peer = wpipe;
	wpipe->pipe_peer = rpipe;

	fd_affix(p, rf, (int)retval[0]);
	fd_affix(p, wf, (int)retval[1]);
	return (0);
free3:
	fd_abort(p, rf, (int)retval[0]);
free2:
	pipeclose(wpipe);
	pipeclose(rpipe);

	return (error);
}

/*
 * Allocate kva for pipe circular buffer, the space is pageable
 * This routine will 'realloc' the size of a pipe safely, if it fails
 * it will retain the old buffer.
 * If it fails it will return ENOMEM.
 */
static int
pipespace(struct pipe *pipe, int size)
{
	void *buffer;

	/*
	 * Allocate pageable virtual address space.  Physical memory is
	 * allocated on demand.
	 */
	if (size == PIPE_SIZE && pipe->pipe_kmem != 0) {
		buffer = (void *)pipe->pipe_kmem;
	} else {
		buffer = (void *)uvm_km_alloc(kernel_map, round_page(size),
		    0, UVM_KMF_PAGEABLE);
		if (buffer == NULL)
			return (ENOMEM);
		atomic_add_int(&amountpipekva, size);
	}

	/* free old resources if we're resizing */
	pipe_free_kmem(pipe);
	pipe->pipe_buffer.buffer = buffer;
	pipe->pipe_buffer.size = size;
	pipe->pipe_buffer.in = 0;
	pipe->pipe_buffer.out = 0;
	pipe->pipe_buffer.cnt = 0;
	return (0);
}

/*
 * Initialize and allocate VM and memory for pipe.
 */
static int
pipe_create(struct pipe **pipep, pool_cache_t cache)
{
	struct pipe *pipe;
	int error;

	pipe = pool_cache_get(cache, PR_WAITOK);
	KASSERT(pipe != NULL);
	*pipep = pipe;
	error = 0;
	getnanotime(&pipe->pipe_btime);
	pipe->pipe_atime = pipe->pipe_mtime = pipe->pipe_btime;
	pipe->pipe_lock = NULL;
	if (cache == pipe_rd_cache) {
		error = pipespace(pipe, PIPE_SIZE);
	} else {
		pipe->pipe_buffer.buffer = NULL;
		pipe->pipe_buffer.size = 0;
		pipe->pipe_buffer.in = 0;
		pipe->pipe_buffer.out = 0;
		pipe->pipe_buffer.cnt = 0;
	}
	return error;
}

/*
 * Lock a pipe for I/O, blocking other access
 * Called with pipe spin lock held.
 */
static int
pipelock(struct pipe *pipe, bool catch_p)
{
	int error;

	KASSERT(mutex_owned(pipe->pipe_lock));

	while (pipe->pipe_state & PIPE_LOCKFL) {
		pipe->pipe_state |= PIPE_LWANT;
		if (catch_p) {
			error = cv_wait_sig(&pipe->pipe_lkcv, pipe->pipe_lock);
			if (error != 0)
				return error;
		} else
			cv_wait(&pipe->pipe_lkcv, pipe->pipe_lock);
	}

	pipe->pipe_state |= PIPE_LOCKFL;

	return 0;
}

/*
 * unlock a pipe I/O lock
 */
static inline void
pipeunlock(struct pipe *pipe)
{

	KASSERT(pipe->pipe_state & PIPE_LOCKFL);

	pipe->pipe_state &= ~PIPE_LOCKFL;
	if (pipe->pipe_state & PIPE_LWANT) {
		pipe->pipe_state &= ~PIPE_LWANT;
		cv_broadcast(&pipe->pipe_lkcv);
	}
}

/*
 * Select/poll wakup. This also sends SIGIO to peer connected to
 * 'sigpipe' side of pipe.
 */
static void
pipeselwakeup(struct pipe *selp, struct pipe *sigp, int code)
{
	int band;

	switch (code) {
	case POLL_IN:
		band = POLLIN|POLLRDNORM;
		break;
	case POLL_OUT:
		band = POLLOUT|POLLWRNORM;
		break;
	case POLL_HUP:
		band = POLLHUP;
		break;
	case POLL_ERR:
		band = POLLERR;
		break;
	default:
		band = 0;
#ifdef DIAGNOSTIC
		printf("bad siginfo code %d in pipe notification.\n", code);
#endif
		break;
	}

	selnotify(&selp->pipe_sel, band, NOTE_SUBMIT);

	if (sigp == NULL || (sigp->pipe_state & PIPE_ASYNC) == 0)
		return;

	fownsignal(sigp->pipe_pgid, SIGIO, code, band, selp);
}

static int
pipe_read(file_t *fp, off_t *offset, struct uio *uio, kauth_cred_t cred,
    int flags)
{
	struct pipe *rpipe = fp->f_pipe;
	struct pipebuf *bp = &rpipe->pipe_buffer;
	kmutex_t *lock = rpipe->pipe_lock;
	int error;
	size_t nread = 0;
	size_t size;
	size_t ocnt;
	unsigned int wakeup_state = 0;

	mutex_enter(lock);
	++rpipe->pipe_busy;
	ocnt = bp->cnt;

again:
	error = pipelock(rpipe, true);
	if (error)
		goto unlocked_error;

	while (uio->uio_resid) {
		/*
		 * Normal pipe buffer receive.
		 */
		if (bp->cnt > 0) {
			size = bp->size - bp->out;
			if (size > bp->cnt)
				size = bp->cnt;
			if (size > uio->uio_resid)
				size = uio->uio_resid;

			mutex_exit(lock);
			error = uiomove((char *)bp->buffer + bp->out, size, uio);
			mutex_enter(lock);
			if (error)
				break;

			bp->out += size;
			if (bp->out >= bp->size)
				bp->out = 0;

			bp->cnt -= size;

			/*
			 * If there is no more to read in the pipe, reset
			 * its pointers to the beginning.  This improves
			 * cache hit stats.
			 */
			if (bp->cnt == 0) {
				bp->in = 0;
				bp->out = 0;
			}
			nread += size;
			continue;
		}

#ifndef PIPE_NODIRECT
		if ((rpipe->pipe_state & PIPE_DIRECTR) != 0) {
			struct pipemapping * const rmap = &rpipe->pipe_map;
			/*
			 * Direct copy, bypassing a kernel buffer.
			 */
			void *va;
			u_int gen;

			KASSERT(rpipe->pipe_state & PIPE_DIRECTW);

			size = rmap->cnt;
			if (size > uio->uio_resid)
				size = uio->uio_resid;

			va = (char *)rmap->kva + rmap->pos;
			gen = rmap->egen;
			mutex_exit(lock);

			/*
			 * Consume emap and read the data from loaned pages.
			 */
			uvm_emap_consume(gen);
			error = uiomove(va, size, uio);

			mutex_enter(lock);
			if (error)
				break;
			nread += size;
			rmap->pos += size;
			rmap->cnt -= size;
			if (rmap->cnt == 0) {
				rpipe->pipe_state &= ~PIPE_DIRECTR;
				cv_broadcast(&rpipe->pipe_wcv);
			}
			continue;
		}
#endif
		/*
		 * Break if some data was read.
		 */
		if (nread > 0)
			break;

		/*
		 * Detect EOF condition.
		 * Read returns 0 on EOF, no need to set error.
		 */
		if (rpipe->pipe_state & PIPE_EOF)
			break;

		/*
		 * Don't block on non-blocking I/O.
		 */
		if (fp->f_flag & FNONBLOCK) {
			error = EAGAIN;
			break;
		}

		/*
		 * Unlock the pipe buffer for our remaining processing.
		 * We will either break out with an error or we will
		 * sleep and relock to loop.
		 */
		pipeunlock(rpipe);

		/*
		 * Re-check to see if more direct writes are pending.
		 */
		if ((rpipe->pipe_state & PIPE_DIRECTR) != 0)
			goto again;

#if 1   /* XXX (dsl) I'm sure these aren't needed here ... */
		/*
		 * We want to read more, wake up select/poll.
		 */
		pipeselwakeup(rpipe, rpipe->pipe_peer, POLL_OUT);

		/*
		 * If the "write-side" is blocked, wake it up now.
		 */
		cv_broadcast(&rpipe->pipe_wcv);
#endif

		if (wakeup_state & PIPE_RESTART) {
			error = ERESTART;
			goto unlocked_error;
		}

		/* Now wait until the pipe is filled */
		error = cv_wait_sig(&rpipe->pipe_rcv, lock);
		if (error != 0)
			goto unlocked_error;
		wakeup_state = rpipe->pipe_state;
		goto again;
	}

	if (error == 0)
		getnanotime(&rpipe->pipe_atime);
	pipeunlock(rpipe);

unlocked_error:
	--rpipe->pipe_busy;
	if (rpipe->pipe_busy == 0) {
		rpipe->pipe_state &= ~PIPE_RESTART;
		cv_broadcast(&rpipe->pipe_draincv);
	}
	if (bp->cnt < MINPIPESIZE) {
		cv_broadcast(&rpipe->pipe_wcv);
	}

	/*
	 * If anything was read off the buffer, signal to the writer it's
	 * possible to write more data. Also send signal if we are here for the
	 * first time after last write.
	 */
	if ((bp->size - bp->cnt) >= PIPE_BUF
	    && (ocnt != bp->cnt || (rpipe->pipe_state & PIPE_SIGNALR))) {
		pipeselwakeup(rpipe, rpipe->pipe_peer, POLL_OUT);
		rpipe->pipe_state &= ~PIPE_SIGNALR;
	}

	mutex_exit(lock);
	return (error);
}

#ifndef PIPE_NODIRECT
/*
 * Allocate structure for loan transfer.
 */
static int
pipe_loan_alloc(struct pipe *wpipe, int npages)
{
	struct pipemapping * const wmap = &wpipe->pipe_map;
	const vsize_t len = ptoa(npages);

	atomic_add_int(&amountpipekva, len);
	wmap->kva = uvm_km_alloc(kernel_map, len, 0,
	    UVM_KMF_COLORMATCH | UVM_KMF_VAONLY | UVM_KMF_WAITVA);
	if (wmap->kva == 0) {
		atomic_add_int(&amountpipekva, -len);
		return (ENOMEM);
	}

	wmap->npages = npages;
	wmap->pgs = kmem_alloc(npages * sizeof(struct vm_page *), KM_SLEEP);
	return (0);
}

/*
 * Free resources allocated for loan transfer.
 */
static void
pipe_loan_free(struct pipe *wpipe)
{
	struct pipemapping * const wmap = &wpipe->pipe_map;
	const vsize_t len = ptoa(wmap->npages);

	uvm_emap_remove(wmap->kva, len);	/* XXX */
	uvm_km_free(kernel_map, wmap->kva, len, UVM_KMF_VAONLY);
	wmap->kva = 0;
	atomic_add_int(&amountpipekva, -len);
	kmem_free(wmap->pgs, wmap->npages * sizeof(struct vm_page *));
	wmap->pgs = NULL;
#if 0
	wmap->npages = 0;
	wmap->pos = 0;
	wmap->cnt = 0;
#endif
}

/*
 * NetBSD direct write, using uvm_loan() mechanism.
 * This implements the pipe buffer write mechanism.  Note that only
 * a direct write OR a normal pipe write can be pending at any given time.
 * If there are any characters in the pipe buffer, the direct write will
 * be deferred until the receiving process grabs all of the bytes from
 * the pipe buffer.  Then the direct mapping write is set-up.
 *
 * Called with the long-term pipe lock held.
 */
static int
pipe_direct_write(file_t *fp, struct pipe *wpipe, struct uio *uio)
{
	struct pipemapping * const wmap = &wpipe->pipe_map;
	kmutex_t * const lock = wpipe->pipe_lock;
	struct vm_page **pgs;
	vaddr_t bbase, base, bend;
	vsize_t blen, bcnt;
	int error, npages;
	voff_t bpos;
	u_int starting_color;

	KASSERT(mutex_owned(wpipe->pipe_lock));
	KASSERT(wmap->cnt == 0);

	mutex_exit(lock);

	/*
	 * Handle first PIPE_CHUNK_SIZE bytes of buffer. Deal with buffers
	 * not aligned to PAGE_SIZE.
	 */
	bbase = (vaddr_t)uio->uio_iov->iov_base;
	base = trunc_page(bbase);
	bend = round_page(bbase + uio->uio_iov->iov_len);
	blen = bend - base;
	bpos = bbase - base;

	if (blen > PIPE_DIRECT_CHUNK) {
		blen = PIPE_DIRECT_CHUNK;
		bend = base + blen;
		bcnt = PIPE_DIRECT_CHUNK - bpos;
	} else {
		bcnt = uio->uio_iov->iov_len;
	}
	npages = atop(blen);
	starting_color = atop(base) & uvmexp.colormask;

	/*
	 * Free the old kva if we need more pages than we have
	 * allocated.
	 */
	if (wmap->kva != 0 && starting_color + npages > wmap->npages)
		pipe_loan_free(wpipe);

	/* Allocate new kva. */
	if (wmap->kva == 0) {
		error = pipe_loan_alloc(wpipe, starting_color + npages);
		if (error) {
			mutex_enter(lock);
			return (error);
		}
	}

	/* Loan the write buffer memory from writer process */
	pgs = wmap->pgs + starting_color;
	error = uvm_loan(&uio->uio_vmspace->vm_map, base, blen,
			 pgs, UVM_LOAN_TOPAGE);
	if (error) {
		pipe_loan_free(wpipe);
		mutex_enter(lock);
		return (ENOMEM); /* so that caller fallback to ordinary write */
	}

	/* Enter the loaned pages to KVA, produce new emap generation number. */
	uvm_emap_enter(wmap->kva + ptoa(starting_color), pgs, npages);
	wmap->egen = uvm_emap_produce();

	/* Now we can put the pipe in direct write mode */
	wmap->pos = bpos + ptoa(starting_color);
	wmap->cnt = bcnt;

	/*
	 * But before we can let someone do a direct read, we
	 * have to wait until the pipe is drained.  Release the
	 * pipe lock while we wait.
	 */
	mutex_enter(lock);
	wpipe->pipe_state |= PIPE_DIRECTW;
	pipeunlock(wpipe);

	while (error == 0 && wpipe->pipe_buffer.cnt > 0) {
		cv_broadcast(&wpipe->pipe_rcv);
		error = cv_wait_sig(&wpipe->pipe_wcv, lock);
		if (error == 0 && wpipe->pipe_state & PIPE_EOF)
			error = EPIPE;
	}

	/* Pipe is drained; next read will off the direct buffer */
	wpipe->pipe_state |= PIPE_DIRECTR;

	/* Wait until the reader is done */
	while (error == 0 && (wpipe->pipe_state & PIPE_DIRECTR)) {
		cv_broadcast(&wpipe->pipe_rcv);
		pipeselwakeup(wpipe, wpipe, POLL_IN);
		error = cv_wait_sig(&wpipe->pipe_wcv, lock);
		if (error == 0 && wpipe->pipe_state & PIPE_EOF)
			error = EPIPE;
	}

	/* Take pipe out of direct write mode */
	wpipe->pipe_state &= ~(PIPE_DIRECTW | PIPE_DIRECTR);

	/* Acquire the pipe lock and cleanup */
	(void)pipelock(wpipe, false);
	mutex_exit(lock);

	if (pgs != NULL) {
		/* XXX: uvm_emap_remove */
		uvm_unloan(pgs, npages, UVM_LOAN_TOPAGE);
	}
	if (error || amountpipekva > maxpipekva)
		pipe_loan_free(wpipe);

	mutex_enter(lock);
	if (error) {
		pipeselwakeup(wpipe, wpipe, POLL_ERR);

		/*
		 * If nothing was read from what we offered, return error
		 * straight on. Otherwise update uio resid first. Caller
		 * will deal with the error condition, returning short
		 * write, error, or restarting the write(2) as appropriate.
		 */
		if (wmap->cnt == bcnt) {
			wmap->cnt = 0;
			cv_broadcast(&wpipe->pipe_wcv);
			return (error);
		}

		bcnt -= wpipe->cnt;
	}

	uio->uio_resid -= bcnt;
	/* uio_offset not updated, not set/used for write(2) */
	uio->uio_iov->iov_base = (char *)uio->uio_iov->iov_base + bcnt;
	uio->uio_iov->iov_len -= bcnt;
	if (uio->uio_iov->iov_len == 0) {
		uio->uio_iov++;
		uio->uio_iovcnt--;
	}

	wmap->cnt = 0;
	return (error);
}
#endif /* !PIPE_NODIRECT */

static int
pipe_write(file_t *fp, off_t *offset, struct uio *uio, kauth_cred_t cred,
    int flags)
{
	struct pipe *wpipe, *rpipe;
	struct pipebuf *bp;
	kmutex_t *lock;
	int error;
	unsigned int wakeup_state = 0;

	/* We want to write to our peer */
	rpipe = fp->f_pipe;
	lock = rpipe->pipe_lock;
	error = 0;

	mutex_enter(lock);
	wpipe = rpipe->pipe_peer;

	/*
	 * Detect loss of pipe read side, issue SIGPIPE if lost.
	 */
	if (wpipe == NULL || (wpipe->pipe_state & PIPE_EOF) != 0) {
		mutex_exit(lock);
		return EPIPE;
	}
	++wpipe->pipe_busy;

	/* Aquire the long-term pipe lock */
	if ((error = pipelock(wpipe, true)) != 0) {
		--wpipe->pipe_busy;
		if (wpipe->pipe_busy == 0) {
			wpipe->pipe_state &= ~PIPE_RESTART;
			cv_broadcast(&wpipe->pipe_draincv);
		}
		mutex_exit(lock);
		return (error);
	}

	bp = &wpipe->pipe_buffer;

	/*
	 * If it is advantageous to resize the pipe buffer, do so.
	 */
	if ((uio->uio_resid > PIPE_SIZE) &&
	    (nbigpipe < maxbigpipes) &&
#ifndef PIPE_NODIRECT
	    (wpipe->pipe_state & PIPE_DIRECTW) == 0 &&
#endif
	    (bp->size <= PIPE_SIZE) && (bp->cnt == 0)) {

		if (pipespace(wpipe, BIG_PIPE_SIZE) == 0)
			atomic_inc_uint(&nbigpipe);
	}

	while (uio->uio_resid) {
		size_t space;

#ifndef PIPE_NODIRECT
		/*
		 * Pipe buffered writes cannot be coincidental with
		 * direct writes.  Also, only one direct write can be
		 * in progress at any one time.  We wait until the currently
		 * executing direct write is completed before continuing.
		 *
		 * We break out if a signal occurs or the reader goes away.
		 */
		while (error == 0 && wpipe->pipe_state & PIPE_DIRECTW) {
			cv_broadcast(&wpipe->pipe_rcv);
			pipeunlock(wpipe);
			error = cv_wait_sig(&wpipe->pipe_wcv, lock);
			(void)pipelock(wpipe, false);
			if (wpipe->pipe_state & PIPE_EOF)
				error = EPIPE;
		}
		if (error)
			break;

		/*
		 * If the transfer is large, we can gain performance if
		 * we do process-to-process copies directly.
		 * If the write is non-blocking, we don't use the
		 * direct write mechanism.
		 *
		 * The direct write mechanism will detect the reader going
		 * away on us.
		 */
		if ((uio->uio_iov->iov_len >= PIPE_MINDIRECT) &&
		    (fp->f_flag & FNONBLOCK) == 0 &&
		    (wmap->kva || (amountpipekva < limitpipekva))) {
			error = pipe_direct_write(fp, wpipe, uio);

			/*
			 * Break out if error occurred, unless it's ENOMEM.
			 * ENOMEM means we failed to allocate some resources
			 * for direct write, so we just fallback to ordinary
			 * write. If the direct write was successful,
			 * process rest of data via ordinary write.
			 */
			if (error == 0)
				continue;

			if (error != ENOMEM)
				break;
		}
#endif /* PIPE_NODIRECT */

		space = bp->size - bp->cnt;

		/* Writes of size <= PIPE_BUF must be atomic. */
		if ((space < uio->uio_resid) && (uio->uio_resid <= PIPE_BUF))
			space = 0;

		if (space > 0) {
			int size;	/* Transfer size */
			int segsize;	/* first segment to transfer */

			/*
			 * Transfer size is minimum of uio transfer
			 * and free space in pipe buffer.
			 */
			if (space > uio->uio_resid)
				size = uio->uio_resid;
			else
				size = space;
			/*
			 * First segment to transfer is minimum of
			 * transfer size and contiguous space in
			 * pipe buffer.  If first segment to transfer
			 * is less than the transfer size, we've got
			 * a wraparound in the buffer.
			 */
			segsize = bp->size - bp->in;
			if (segsize > size)
				segsize = size;

			/* Transfer first segment */
			mutex_exit(lock);
			error = uiomove((char *)bp->buffer + bp->in, segsize,
			    uio);

			if (error == 0 && segsize < size) {
				/*
				 * Transfer remaining part now, to
				 * support atomic writes.  Wraparound
				 * happened.
				 */
				KASSERT(bp->in + segsize == bp->size);
				error = uiomove(bp->buffer,
				    size - segsize, uio);
			}
			mutex_enter(lock);
			if (error)
				break;

			bp->in += size;
			if (bp->in >= bp->size) {
				KASSERT(bp->in == size - segsize + bp->size);
				bp->in = size - segsize;
			}

			bp->cnt += size;
			KASSERT(bp->cnt <= bp->size);
			wakeup_state = 0;
		} else {
			/*
			 * If the "read-side" has been blocked, wake it up now.
			 */
			cv_broadcast(&wpipe->pipe_rcv);

			/*
			 * Don't block on non-blocking I/O.
			 */
			if (fp->f_flag & FNONBLOCK) {
				error = EAGAIN;
				break;
			}

			/*
			 * We have no more space and have something to offer,
			 * wake up select/poll.
			 */
			if (bp->cnt)
				pipeselwakeup(wpipe, wpipe, POLL_IN);

			if (wakeup_state & PIPE_RESTART) {
				error = ERESTART;
				break;
			}

			pipeunlock(wpipe);
			error = cv_wait_sig(&wpipe->pipe_wcv, lock);
			(void)pipelock(wpipe, false);
			if (error != 0)
				break;
			/*
			 * If read side wants to go away, we just issue a signal
			 * to ourselves.
			 */
			if (wpipe->pipe_state & PIPE_EOF) {
				error = EPIPE;
				break;
			}
			wakeup_state = wpipe->pipe_state;
		}
	}

	--wpipe->pipe_busy;
	if (wpipe->pipe_busy == 0) {
		wpipe->pipe_state &= ~PIPE_RESTART;
		cv_broadcast(&wpipe->pipe_draincv);
	}
	if (bp->cnt > 0) {
		cv_broadcast(&wpipe->pipe_rcv);
	}

	/*
	 * Don't return EPIPE if I/O was successful
	 */
	if (error == EPIPE && bp->cnt == 0 && uio->uio_resid == 0)
		error = 0;

	if (error == 0)
		getnanotime(&wpipe->pipe_mtime);

	/*
	 * We have something to offer, wake up select/poll.
	 * wmap->cnt is always 0 in this point (direct write
	 * is only done synchronously), so check only wpipe->pipe_buffer.cnt
	 */
	if (bp->cnt)
		pipeselwakeup(wpipe, wpipe, POLL_IN);

	/*
	 * Arrange for next read(2) to do a signal.
	 */
	wpipe->pipe_state |= PIPE_SIGNALR;

	pipeunlock(wpipe);
	mutex_exit(lock);
	return (error);
}

/*
 * We implement a very minimal set of ioctls for compatibility with sockets.
 */
int
pipe_ioctl(file_t *fp, u_long cmd, void *data)
{
	struct pipe *pipe = fp->f_pipe;
	kmutex_t *lock = pipe->pipe_lock;

	switch (cmd) {

	case FIONBIO:
		return (0);

	case FIOASYNC:
		mutex_enter(lock);
		if (*(int *)data) {
			pipe->pipe_state |= PIPE_ASYNC;
		} else {
			pipe->pipe_state &= ~PIPE_ASYNC;
		}
		mutex_exit(lock);
		return (0);

	case FIONREAD:
		mutex_enter(lock);
#ifndef PIPE_NODIRECT
		if (pipe->pipe_state & PIPE_DIRECTW)
			*(int *)data = pipe->pipe_map.cnt;
		else
#endif
			*(int *)data = pipe->pipe_buffer.cnt;
		mutex_exit(lock);
		return (0);

	case FIONWRITE:
		/* Look at other side */
		pipe = pipe->pipe_peer;
		mutex_enter(lock);
#ifndef PIPE_NODIRECT
		if (pipe->pipe_state & PIPE_DIRECTW)
			*(int *)data = pipe->pipe_map.cnt;
		else
#endif
			*(int *)data = pipe->pipe_buffer.cnt;
		mutex_exit(lock);
		return (0);

	case FIONSPACE:
		/* Look at other side */
		pipe = pipe->pipe_peer;
		mutex_enter(lock);
#ifndef PIPE_NODIRECT
		/*
		 * If we're in direct-mode, we don't really have a
		 * send queue, and any other write will block. Thus
		 * zero seems like the best answer.
		 */
		if (pipe->pipe_state & PIPE_DIRECTW)
			*(int *)data = 0;
		else
#endif
			*(int *)data = pipe->pipe_buffer.size -
			    pipe->pipe_buffer.cnt;
		mutex_exit(lock);
		return (0);

	case TIOCSPGRP:
	case FIOSETOWN:
		return fsetown(&pipe->pipe_pgid, cmd, data);

	case TIOCGPGRP:
	case FIOGETOWN:
		return fgetown(pipe->pipe_pgid, cmd, data);

	}
	return (EPASSTHROUGH);
}

int
pipe_poll(file_t *fp, int events)
{
	struct pipe *rpipe = fp->f_pipe;
	struct pipe *wpipe;
	int eof = 0;
	int revents = 0;

	mutex_enter(rpipe->pipe_lock);
	wpipe = rpipe->pipe_peer;

	if (events & (POLLIN | POLLRDNORM))
		if ((rpipe->pipe_buffer.cnt > 0) ||
#ifndef PIPE_NODIRECT
		    (rpipe->pipe_state & PIPE_DIRECTR) ||
#endif
		    (rpipe->pipe_state & PIPE_EOF))
			revents |= events & (POLLIN | POLLRDNORM);

	eof |= (rpipe->pipe_state & PIPE_EOF);

	if (wpipe == NULL)
		revents |= events & (POLLOUT | POLLWRNORM);
	else {
		if (events & (POLLOUT | POLLWRNORM))
			if ((wpipe->pipe_state & PIPE_EOF) || (
#ifndef PIPE_NODIRECT
			     (wpipe->pipe_state & PIPE_DIRECTW) == 0 &&
#endif
			     (wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt) >= PIPE_BUF))
				revents |= events & (POLLOUT | POLLWRNORM);

		eof |= (wpipe->pipe_state & PIPE_EOF);
	}

	if (wpipe == NULL || eof)
		revents |= POLLHUP;

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(curlwp, &rpipe->pipe_sel);

		if (events & (POLLOUT | POLLWRNORM))
			selrecord(curlwp, &wpipe->pipe_sel);
	}
	mutex_exit(rpipe->pipe_lock);

	return (revents);
}

static int
pipe_stat(file_t *fp, struct stat *ub)
{
	struct pipe *pipe = fp->f_pipe;

	mutex_enter(pipe->pipe_lock);
	memset(ub, 0, sizeof(*ub));
	ub->st_mode = S_IFIFO | S_IRUSR | S_IWUSR;
	ub->st_blksize = pipe->pipe_buffer.size;
	if (ub->st_blksize == 0 && pipe->pipe_peer)
		ub->st_blksize = pipe->pipe_peer->pipe_buffer.size;
	ub->st_size = pipe->pipe_buffer.cnt;
	ub->st_blocks = (ub->st_size) ? 1 : 0;
	ub->st_atimespec = pipe->pipe_atime;
	ub->st_mtimespec = pipe->pipe_mtime;
	ub->st_ctimespec = ub->st_birthtimespec = pipe->pipe_btime;
	ub->st_uid = kauth_cred_geteuid(fp->f_cred);
	ub->st_gid = kauth_cred_getegid(fp->f_cred);

	/*
	 * Left as 0: st_dev, st_ino, st_nlink, st_rdev, st_flags, st_gen.
	 * XXX (st_dev, st_ino) should be unique.
	 */
	mutex_exit(pipe->pipe_lock);
	return 0;
}

static int
pipe_close(file_t *fp)
{
	struct pipe *pipe = fp->f_pipe;

	fp->f_pipe = NULL;
	pipeclose(pipe);
	return (0);
}

static void
pipe_restart(file_t *fp)
{
	struct pipe *pipe = fp->f_pipe;

	/*
	 * Unblock blocked reads/writes in order to allow close() to complete.
	 * System calls return ERESTART so that the fd is revalidated.
	 * (Partial writes return the transfer length.)
	 */
	mutex_enter(pipe->pipe_lock);
	pipe->pipe_state |= PIPE_RESTART;
	/* Wakeup both cvs, maybe we only need one, but maybe there are some
	 * other paths where wakeup is needed, and it saves deciding which! */
	cv_broadcast(&pipe->pipe_rcv);
	cv_broadcast(&pipe->pipe_wcv);
	mutex_exit(pipe->pipe_lock);
}

static void
pipe_free_kmem(struct pipe *pipe)
{

	if (pipe->pipe_buffer.buffer != NULL) {
		if (pipe->pipe_buffer.size > PIPE_SIZE) {
			atomic_dec_uint(&nbigpipe);
		}
		if (pipe->pipe_buffer.buffer != (void *)pipe->pipe_kmem) {
			uvm_km_free(kernel_map,
			    (vaddr_t)pipe->pipe_buffer.buffer,
			    pipe->pipe_buffer.size, UVM_KMF_PAGEABLE);
			atomic_add_int(&amountpipekva,
			    -pipe->pipe_buffer.size);
		}
		pipe->pipe_buffer.buffer = NULL;
	}
#ifndef PIPE_NODIRECT
	if (pipe->pipe_map.kva != 0) {
		pipe_loan_free(pipe);
		pipe->pipe_map.cnt = 0;
		pipe->pipe_map.pos = 0;
		pipe->pipe_map.npages = 0;
	}
#endif /* !PIPE_NODIRECT */
}

/*
 * Shutdown the pipe.
 */
static void
pipeclose(struct pipe *pipe)
{
	kmutex_t *lock;
	struct pipe *ppipe;

	if (pipe == NULL)
		return;

	KASSERT(cv_is_valid(&pipe->pipe_rcv));
	KASSERT(cv_is_valid(&pipe->pipe_wcv));
	KASSERT(cv_is_valid(&pipe->pipe_draincv));
	KASSERT(cv_is_valid(&pipe->pipe_lkcv));

	lock = pipe->pipe_lock;
	if (lock == NULL)
		/* Must have failed during create */
		goto free_resources;

	mutex_enter(lock);
	pipeselwakeup(pipe, pipe, POLL_HUP);

	/*
	 * If the other side is blocked, wake it up saying that
	 * we want to close it down.
	 */
	pipe->pipe_state |= PIPE_EOF;
	if (pipe->pipe_busy) {
		while (pipe->pipe_busy) {
			cv_broadcast(&pipe->pipe_wcv);
			cv_wait_sig(&pipe->pipe_draincv, lock);
		}
	}

	/*
	 * Disconnect from peer.
	 */
	if ((ppipe = pipe->pipe_peer) != NULL) {
		pipeselwakeup(ppipe, ppipe, POLL_HUP);
		ppipe->pipe_state |= PIPE_EOF;
		cv_broadcast(&ppipe->pipe_rcv);
		ppipe->pipe_peer = NULL;
	}

	/*
	 * Any knote objects still left in the list are
	 * the one attached by peer.  Since no one will
	 * traverse this list, we just clear it.
	 */
	SLIST_INIT(&pipe->pipe_sel.sel_klist);

	KASSERT((pipe->pipe_state & PIPE_LOCKFL) == 0);
	mutex_exit(lock);
	mutex_obj_free(lock);

	/*
	 * Free resources.
	 */
    free_resources:
	pipe->pipe_pgid = 0;
	pipe->pipe_state = PIPE_SIGNALR;
	pipe_free_kmem(pipe);
	if (pipe->pipe_kmem != 0) {
		pool_cache_put(pipe_rd_cache, pipe);
	} else {
		pool_cache_put(pipe_wr_cache, pipe);
	}
}

static void
filt_pipedetach(struct knote *kn)
{
	struct pipe *pipe;
	kmutex_t *lock;

	pipe = ((file_t *)kn->kn_obj)->f_pipe;
	lock = pipe->pipe_lock;

	mutex_enter(lock);

	switch(kn->kn_filter) {
	case EVFILT_WRITE:
		/* Need the peer structure, not our own. */
		pipe = pipe->pipe_peer;

		/* If reader end already closed, just return. */
		if (pipe == NULL) {
			mutex_exit(lock);
			return;
		}

		break;
	default:
		/* Nothing to do. */
		break;
	}

	KASSERT(kn->kn_hook == pipe);
	SLIST_REMOVE(&pipe->pipe_sel.sel_klist, kn, knote, kn_selnext);
	mutex_exit(lock);
}

static int
filt_piperead(struct knote *kn, long hint)
{
	struct pipe *rpipe = ((file_t *)kn->kn_obj)->f_pipe;
	struct pipe *wpipe;

	if ((hint & NOTE_SUBMIT) == 0) {
		mutex_enter(rpipe->pipe_lock);
	}
	wpipe = rpipe->pipe_peer;
	kn->kn_data = rpipe->pipe_buffer.cnt;

	if ((kn->kn_data == 0) && (rpipe->pipe_state & PIPE_DIRECTW))
		kn->kn_data = rpipe->pipe_map.cnt;

	if ((rpipe->pipe_state & PIPE_EOF) ||
	    (wpipe == NULL) || (wpipe->pipe_state & PIPE_EOF)) {
		kn->kn_flags |= EV_EOF;
		if ((hint & NOTE_SUBMIT) == 0) {
			mutex_exit(rpipe->pipe_lock);
		}
		return (1);
	}

	if ((hint & NOTE_SUBMIT) == 0) {
		mutex_exit(rpipe->pipe_lock);
	}
	return (kn->kn_data > 0);
}

static int
filt_pipewrite(struct knote *kn, long hint)
{
	struct pipe *rpipe = ((file_t *)kn->kn_obj)->f_pipe;
	struct pipe *wpipe;

	if ((hint & NOTE_SUBMIT) == 0) {
		mutex_enter(rpipe->pipe_lock);
	}
	wpipe = rpipe->pipe_peer;

	if ((wpipe == NULL) || (wpipe->pipe_state & PIPE_EOF)) {
		kn->kn_data = 0;
		kn->kn_flags |= EV_EOF;
		if ((hint & NOTE_SUBMIT) == 0) {
			mutex_exit(rpipe->pipe_lock);
		}
		return (1);
	}
	kn->kn_data = wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt;
	if (wpipe->pipe_state & PIPE_DIRECTW)
		kn->kn_data = 0;

	if ((hint & NOTE_SUBMIT) == 0) {
		mutex_exit(rpipe->pipe_lock);
	}
	return (kn->kn_data >= PIPE_BUF);
}

static const struct filterops pipe_rfiltops =
	{ 1, NULL, filt_pipedetach, filt_piperead };
static const struct filterops pipe_wfiltops =
	{ 1, NULL, filt_pipedetach, filt_pipewrite };

static int
pipe_kqfilter(file_t *fp, struct knote *kn)
{
	struct pipe *pipe;
	kmutex_t *lock;

	pipe = ((file_t *)kn->kn_obj)->f_pipe;
	lock = pipe->pipe_lock;

	mutex_enter(lock);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &pipe_rfiltops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &pipe_wfiltops;
		pipe = pipe->pipe_peer;
		if (pipe == NULL) {
			/* Other end of pipe has been closed. */
			mutex_exit(lock);
			return (EBADF);
		}
		break;
	default:
		mutex_exit(lock);
		return (EINVAL);
	}

	kn->kn_hook = pipe;
	SLIST_INSERT_HEAD(&pipe->pipe_sel.sel_klist, kn, kn_selnext);
	mutex_exit(lock);

	return (0);
}

/*
 * Handle pipe sysctls.
 */
SYSCTL_SETUP(sysctl_kern_pipe_setup, "sysctl kern.pipe subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "pipe",
		       SYSCTL_DESCR("Pipe settings"),
		       NULL, 0, NULL, 0,
		       CTL_KERN, KERN_PIPE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxkvasz",
		       SYSCTL_DESCR("Maximum amount of kernel memory to be "
				    "used for pipes"),
		       NULL, 0, &maxpipekva, 0,
		       CTL_KERN, KERN_PIPE, KERN_PIPE_MAXKVASZ, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxloankvasz",
		       SYSCTL_DESCR("Limit for direct transfers via page loan"),
		       NULL, 0, &limitpipekva, 0,
		       CTL_KERN, KERN_PIPE, KERN_PIPE_LIMITKVA, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxbigpipes",
		       SYSCTL_DESCR("Maximum number of \"big\" pipes"),
		       NULL, 0, &maxbigpipes, 0,
		       CTL_KERN, KERN_PIPE, KERN_PIPE_MAXBIGPIPES, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "nbigpipes",
		       SYSCTL_DESCR("Number of \"big\" pipes"),
		       NULL, 0, &nbigpipe, 0,
		       CTL_KERN, KERN_PIPE, KERN_PIPE_NBIGPIPES, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "kvasize",
		       SYSCTL_DESCR("Amount of kernel memory consumed by pipe "
				    "buffers"),
		       NULL, 0, &amountpipekva, 0,
		       CTL_KERN, KERN_PIPE, KERN_PIPE_KVASIZE, CTL_EOL);
}
