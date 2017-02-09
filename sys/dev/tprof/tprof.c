/*	$NetBSD: tprof.c,v 1.13 2015/08/20 14:40:18 christos Exp $	*/

/*-
 * Copyright (c)2008,2009,2010 YAMAMOTO Takashi,
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tprof.c,v 1.13 2015/08/20 14:40:18 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <sys/cpu.h>
#include <sys/conf.h>
#include <sys/callout.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/workqueue.h>
#include <sys/queue.h>

#include <dev/tprof/tprof.h>
#include <dev/tprof/tprof_ioctl.h>

#include "ioconf.h"

/*
 * locking order:
 *	tprof_reader_lock -> tprof_lock
 *	tprof_startstop_lock -> tprof_lock
 */

/*
 * protected by:
 *	L: tprof_lock
 *	R: tprof_reader_lock
 *	S: tprof_startstop_lock
 *	s: writer should hold tprof_startstop_lock and tprof_lock
 *	   reader should hold tprof_startstop_lock or tprof_lock
 */

typedef struct tprof_buf {
	u_int b_used;
	u_int b_size;
	u_int b_overflow;
	u_int b_unused;
	STAILQ_ENTRY(tprof_buf) b_list;
	tprof_sample_t b_data[];
} tprof_buf_t;
#define	TPROF_BUF_BYTESIZE(sz) \
	(sizeof(tprof_buf_t) + (sz) * sizeof(tprof_sample_t))
#define	TPROF_MAX_SAMPLES_PER_BUF	10000

#define	TPROF_MAX_BUF			100

typedef struct {
	tprof_buf_t *c_buf;
	uint32_t c_cpuid;
	struct work c_work;
	callout_t c_callout;
} __aligned(CACHE_LINE_SIZE) tprof_cpu_t;

typedef struct tprof_backend {
	const char *tb_name;
	const tprof_backend_ops_t *tb_ops;
	LIST_ENTRY(tprof_backend) tb_list;
	int tb_usecount;	/* S: */
} tprof_backend_t;

static kmutex_t tprof_lock;
static bool tprof_running;		/* s: */
static u_int tprof_nworker;		/* L: # of running worker LWPs */
static lwp_t *tprof_owner;
static STAILQ_HEAD(, tprof_buf) tprof_list; /* L: global buffer list */
static u_int tprof_nbuf_on_list;	/* L: # of buffers on tprof_list */
static struct workqueue *tprof_wq;
static tprof_cpu_t tprof_cpus[MAXCPUS] __aligned(CACHE_LINE_SIZE);
static u_int tprof_samples_per_buf;

static tprof_backend_t *tprof_backend;	/* S: */
static LIST_HEAD(, tprof_backend) tprof_backends =
    LIST_HEAD_INITIALIZER(tprof_backend); /* S: */

static kmutex_t tprof_reader_lock;
static kcondvar_t tprof_reader_cv;	/* L: */
static off_t tprof_reader_offset;	/* R: */

static kmutex_t tprof_startstop_lock;
static kcondvar_t tprof_cv;		/* L: */

static struct tprof_stat tprof_stat;	/* L: */

static tprof_cpu_t *
tprof_cpu(struct cpu_info *ci)
{

	return &tprof_cpus[cpu_index(ci)];
}

static tprof_cpu_t *
tprof_curcpu(void)
{

	return tprof_cpu(curcpu());
}

static tprof_buf_t *
tprof_buf_alloc(void)
{
	tprof_buf_t *new;
	u_int size = tprof_samples_per_buf;
	
	new = kmem_alloc(TPROF_BUF_BYTESIZE(size), KM_SLEEP);
	new->b_used = 0;
	new->b_size = size;
	new->b_overflow = 0;
	return new;
}

static void
tprof_buf_free(tprof_buf_t *buf)
{

	kmem_free(buf, TPROF_BUF_BYTESIZE(buf->b_size));
}

static tprof_buf_t *
tprof_buf_switch(tprof_cpu_t *c, tprof_buf_t *new)
{
	tprof_buf_t *old;

	old = c->c_buf;
	c->c_buf = new;
	return old;
}

static tprof_buf_t *
tprof_buf_refresh(void)
{
	tprof_cpu_t * const c = tprof_curcpu();
	tprof_buf_t *new;

	new = tprof_buf_alloc();
	return tprof_buf_switch(c, new);
}

static void
tprof_worker(struct work *wk, void *dummy)
{
	tprof_cpu_t * const c = tprof_curcpu();
	tprof_buf_t *buf;
	bool shouldstop;

	KASSERT(wk == &c->c_work);
	KASSERT(dummy == NULL);

	/*
	 * get a per cpu buffer.
	 */
	buf = tprof_buf_refresh();

	/*
	 * and put it on the global list for read(2).
	 */
	mutex_enter(&tprof_lock);
	shouldstop = !tprof_running;
	if (shouldstop) {
		KASSERT(tprof_nworker > 0);
		tprof_nworker--;
		cv_broadcast(&tprof_cv);
		cv_broadcast(&tprof_reader_cv);
	}
	if (buf->b_used == 0) {
		tprof_stat.ts_emptybuf++;
	} else if (tprof_nbuf_on_list < TPROF_MAX_BUF) {
		tprof_stat.ts_sample += buf->b_used;
		tprof_stat.ts_overflow += buf->b_overflow;
		tprof_stat.ts_buf++;
		STAILQ_INSERT_TAIL(&tprof_list, buf, b_list);
		tprof_nbuf_on_list++;
		buf = NULL;
		cv_broadcast(&tprof_reader_cv);
	} else {
		tprof_stat.ts_dropbuf_sample += buf->b_used;
		tprof_stat.ts_dropbuf++;
	}
	mutex_exit(&tprof_lock);
	if (buf) {
		tprof_buf_free(buf);
	}
	if (!shouldstop) {
		callout_schedule(&c->c_callout, hz);
	}
}

static void
tprof_kick(void *vp)
{
	struct cpu_info * const ci = vp;
	tprof_cpu_t * const c = tprof_cpu(ci);

	workqueue_enqueue(tprof_wq, &c->c_work, ci);
}

static void
tprof_stop1(void)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	KASSERT(mutex_owned(&tprof_startstop_lock));
	KASSERT(tprof_nworker == 0);

	for (CPU_INFO_FOREACH(cii, ci)) {
		tprof_cpu_t * const c = tprof_cpu(ci);
		tprof_buf_t *old;

		old = tprof_buf_switch(c, NULL);
		if (old != NULL) {
			tprof_buf_free(old);
		}
		callout_destroy(&c->c_callout);
	}
	workqueue_destroy(tprof_wq);
}

static int
tprof_start(const struct tprof_param *param)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	int error;
	uint64_t freq;
	tprof_backend_t *tb;

	KASSERT(mutex_owned(&tprof_startstop_lock));
	if (tprof_running) {
		error = EBUSY;
		goto done;
	}

	tb = tprof_backend;
	if (tb == NULL) {
		error = ENOENT;
		goto done;
	}
	if (tb->tb_usecount > 0) {
		error = EBUSY;
		goto done;
	}

	tb->tb_usecount++;
	freq = tb->tb_ops->tbo_estimate_freq();
	tprof_samples_per_buf = MIN(freq * 2, TPROF_MAX_SAMPLES_PER_BUF);

	error = workqueue_create(&tprof_wq, "tprofmv", tprof_worker, NULL,
	    PRI_NONE, IPL_SOFTCLOCK, WQ_MPSAFE | WQ_PERCPU);
	if (error != 0) {
		goto done;
	}

	for (CPU_INFO_FOREACH(cii, ci)) {
		tprof_cpu_t * const c = tprof_cpu(ci);
		tprof_buf_t *new;
		tprof_buf_t *old;

		new = tprof_buf_alloc();
		old = tprof_buf_switch(c, new);
		if (old != NULL) {
			tprof_buf_free(old);
		}
		callout_init(&c->c_callout, CALLOUT_MPSAFE);
		callout_setfunc(&c->c_callout, tprof_kick, ci);
	}

	error = tb->tb_ops->tbo_start(NULL);
	if (error != 0) {
		KASSERT(tb->tb_usecount > 0);
		tb->tb_usecount--;
		tprof_stop1();
		goto done;
	}

	mutex_enter(&tprof_lock);
	tprof_running = true;
	mutex_exit(&tprof_lock);
	for (CPU_INFO_FOREACH(cii, ci)) {
		tprof_cpu_t * const c = tprof_cpu(ci);

		mutex_enter(&tprof_lock);
		tprof_nworker++;
		mutex_exit(&tprof_lock);
		workqueue_enqueue(tprof_wq, &c->c_work, ci);
	}
done:
	return error;
}

static void
tprof_stop(void)
{
	tprof_backend_t *tb;

	KASSERT(mutex_owned(&tprof_startstop_lock));
	if (!tprof_running) {
		goto done;
	}

	tb = tprof_backend;
	KASSERT(tb->tb_usecount > 0);
	tb->tb_ops->tbo_stop(NULL);
	tb->tb_usecount--;

	mutex_enter(&tprof_lock);
	tprof_running = false;
	cv_broadcast(&tprof_reader_cv);
	while (tprof_nworker > 0) {
		cv_wait(&tprof_cv, &tprof_lock);
	}
	mutex_exit(&tprof_lock);

	tprof_stop1();
done:
	;
}

/*
 * tprof_clear: drain unread samples.
 */

static void
tprof_clear(void)
{
	tprof_buf_t *buf;

	mutex_enter(&tprof_reader_lock);
	mutex_enter(&tprof_lock);
	while ((buf = STAILQ_FIRST(&tprof_list)) != NULL) {
		if (buf != NULL) {
			STAILQ_REMOVE_HEAD(&tprof_list, b_list);
			KASSERT(tprof_nbuf_on_list > 0);
			tprof_nbuf_on_list--;
			mutex_exit(&tprof_lock);
			tprof_buf_free(buf);
			mutex_enter(&tprof_lock);
		}
	}
	KASSERT(tprof_nbuf_on_list == 0);
	mutex_exit(&tprof_lock);
	tprof_reader_offset = 0;
	mutex_exit(&tprof_reader_lock);

	memset(&tprof_stat, 0, sizeof(tprof_stat));
}

static tprof_backend_t *
tprof_backend_lookup(const char *name)
{
	tprof_backend_t *tb;

	KASSERT(mutex_owned(&tprof_startstop_lock));

	LIST_FOREACH(tb, &tprof_backends, tb_list) {
		if (!strcmp(tb->tb_name, name)) {
			return tb;
		}
	}
	return NULL;
}

/* -------------------- backend interfaces */

/*
 * tprof_sample: record a sample on the per-cpu buffer.
 *
 * be careful; can be called in NMI context.
 * we are bluntly assuming the followings are safe.
 *	curcpu()
 *	curlwp->l_lid
 *	curlwp->l_proc->p_pid
 */

void
tprof_sample(tprof_backend_cookie_t *cookie, const tprof_frame_info_t *tfi)
{
	tprof_cpu_t * const c = tprof_curcpu();
	tprof_buf_t * const buf = c->c_buf;
	tprof_sample_t *sp;
	const uintptr_t pc = tfi->tfi_pc;
	const lwp_t * const l = curlwp;
	u_int idx;

	idx = buf->b_used;
	if (__predict_false(idx >= buf->b_size)) {
		buf->b_overflow++;
		return;
	}
	sp = &buf->b_data[idx];
	sp->s_pid = l->l_proc->p_pid;
	sp->s_lwpid = l->l_lid;
	sp->s_cpuid = c->c_cpuid;
	sp->s_flags = (tfi->tfi_inkernel) ? TPROF_SAMPLE_INKERNEL : 0;
	sp->s_pc = pc;
	buf->b_used = idx + 1;
}

/*
 * tprof_backend_register: 
 */

int
tprof_backend_register(const char *name, const tprof_backend_ops_t *ops,
    int vers)
{
	tprof_backend_t *tb;

	if (vers != TPROF_BACKEND_VERSION) {
		return EINVAL;
	}

	mutex_enter(&tprof_startstop_lock);
	tb = tprof_backend_lookup(name);
	if (tb != NULL) {
		mutex_exit(&tprof_startstop_lock);
		return EEXIST;
	}
#if 1 /* XXX for now */
	if (!LIST_EMPTY(&tprof_backends)) {
		mutex_exit(&tprof_startstop_lock);
		return ENOTSUP;
	}
#endif
	tb = kmem_alloc(sizeof(*tb), KM_SLEEP);
	tb->tb_name = name;
	tb->tb_ops = ops;
	tb->tb_usecount = 0;
	LIST_INSERT_HEAD(&tprof_backends, tb, tb_list);
#if 1 /* XXX for now */
	if (tprof_backend == NULL) {
		tprof_backend = tb;
	}
#endif
	mutex_exit(&tprof_startstop_lock);

	return 0;
}

/*
 * tprof_backend_unregister: 
 */

int
tprof_backend_unregister(const char *name)
{
	tprof_backend_t *tb;

	mutex_enter(&tprof_startstop_lock);
	tb = tprof_backend_lookup(name);
#if defined(DIAGNOSTIC)
	if (tb == NULL) {
		mutex_exit(&tprof_startstop_lock);
		panic("%s: not found '%s'", __func__, name);
	}
#endif /* defined(DIAGNOSTIC) */
	if (tb->tb_usecount > 0) {
		mutex_exit(&tprof_startstop_lock);
		return EBUSY;
	}
#if 1 /* XXX for now */
	if (tprof_backend == tb) {
		tprof_backend = NULL;
	}
#endif
	LIST_REMOVE(tb, tb_list);
	mutex_exit(&tprof_startstop_lock);

	kmem_free(tb, sizeof(*tb));

	return 0;
}

/* -------------------- cdevsw interfaces */

static int
tprof_open(dev_t dev, int flags, int type, struct lwp *l)
{

	if (minor(dev) != 0) {
		return EXDEV;
	}
	mutex_enter(&tprof_lock);
	if (tprof_owner != NULL) {
		mutex_exit(&tprof_lock);
		return  EBUSY;
	}
	tprof_owner = curlwp;
	mutex_exit(&tprof_lock);

	return 0;
}

static int
tprof_close(dev_t dev, int flags, int type, struct lwp *l)
{

	KASSERT(minor(dev) == 0);

	mutex_enter(&tprof_startstop_lock);
	mutex_enter(&tprof_lock);
	tprof_owner = NULL;
	mutex_exit(&tprof_lock);
	tprof_stop();
	tprof_clear();
	mutex_exit(&tprof_startstop_lock);

	return 0;
}

static int
tprof_read(dev_t dev, struct uio *uio, int flags)
{
	tprof_buf_t *buf;
	size_t bytes;
	size_t resid;
	size_t done;
	int error = 0;

	KASSERT(minor(dev) == 0);
	mutex_enter(&tprof_reader_lock);
	while (uio->uio_resid > 0 && error == 0) {
		/*
		 * take the first buffer from the list.
		 */
		mutex_enter(&tprof_lock);
		buf = STAILQ_FIRST(&tprof_list);
		if (buf == NULL) {
			if (tprof_nworker == 0) {
				mutex_exit(&tprof_lock);
				error = 0;
				break;
			}
			mutex_exit(&tprof_reader_lock);
			error = cv_wait_sig(&tprof_reader_cv, &tprof_lock);
			mutex_exit(&tprof_lock);
			mutex_enter(&tprof_reader_lock);
			continue;
		}
		STAILQ_REMOVE_HEAD(&tprof_list, b_list);
		KASSERT(tprof_nbuf_on_list > 0);
		tprof_nbuf_on_list--;
		mutex_exit(&tprof_lock);

		/*
		 * copy it out.
		 */
		bytes = MIN(buf->b_used * sizeof(tprof_sample_t) -
		    tprof_reader_offset, uio->uio_resid);
		resid = uio->uio_resid;
		error = uiomove((char *)buf->b_data + tprof_reader_offset,
		    bytes, uio);
		done = resid - uio->uio_resid;
		tprof_reader_offset += done;

		/*
		 * if we didn't consume the whole buffer,
		 * put it back to the list.
		 */
		if (tprof_reader_offset <
		    buf->b_used * sizeof(tprof_sample_t)) {
			mutex_enter(&tprof_lock);
			STAILQ_INSERT_HEAD(&tprof_list, buf, b_list);
			tprof_nbuf_on_list++;
			cv_broadcast(&tprof_reader_cv);
			mutex_exit(&tprof_lock);
		} else {
			tprof_buf_free(buf);
			tprof_reader_offset = 0;
		}
	}
	mutex_exit(&tprof_reader_lock);

	return error;
}

static int
tprof_ioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	const struct tprof_param *param;
	int error = 0;

	KASSERT(minor(dev) == 0);

	switch (cmd) {
	case TPROF_IOC_GETVERSION:
		*(int *)data = TPROF_VERSION;
		break;
	case TPROF_IOC_START:
		param = data;
		mutex_enter(&tprof_startstop_lock);
		error = tprof_start(param);
		mutex_exit(&tprof_startstop_lock);
		break;
	case TPROF_IOC_STOP:
		mutex_enter(&tprof_startstop_lock);
		tprof_stop();
		mutex_exit(&tprof_startstop_lock);
		break;
	case TPROF_IOC_GETSTAT:
		mutex_enter(&tprof_lock);
		memcpy(data, &tprof_stat, sizeof(tprof_stat));
		mutex_exit(&tprof_lock);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

const struct cdevsw tprof_cdevsw = {
	.d_open = tprof_open,
	.d_close = tprof_close,
	.d_read = tprof_read,
	.d_write = nowrite,
	.d_ioctl = tprof_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};

void
tprofattach(int nunits)
{

	/* nothing */
}

MODULE(MODULE_CLASS_DRIVER, tprof, NULL);

static void
tprof_driver_init(void)
{
	unsigned int i;

	mutex_init(&tprof_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&tprof_reader_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&tprof_startstop_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&tprof_cv, "tprof");
	cv_init(&tprof_reader_cv, "tprof_rd");
	STAILQ_INIT(&tprof_list);
	for (i = 0; i < __arraycount(tprof_cpus); i++) {
		tprof_cpu_t * const c = &tprof_cpus[i];

		c->c_buf = NULL;
		c->c_cpuid = i;
	}
}

static void
tprof_driver_fini(void)
{

	mutex_destroy(&tprof_lock);
	mutex_destroy(&tprof_reader_lock);
	mutex_destroy(&tprof_startstop_lock);
	cv_destroy(&tprof_cv);
	cv_destroy(&tprof_reader_cv);
}

static int
tprof_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		tprof_driver_init();
#if defined(_MODULE)
		{
			devmajor_t bmajor = NODEVMAJOR;
			devmajor_t cmajor = NODEVMAJOR;
			int error;

			error = devsw_attach("tprof", NULL, &bmajor,
			    &tprof_cdevsw, &cmajor);
			if (error) {
				tprof_driver_fini();
				return error;
			}
		}
#endif /* defined(_MODULE) */
		return 0;

	case MODULE_CMD_FINI:
#if defined(_MODULE)
		{
			int error;
			error = devsw_detach(NULL, &tprof_cdevsw);
			if (error) {
				return error;
			}
		}
#endif /* defined(_MODULE) */
		tprof_driver_fini();
		return 0;

	default:
		return ENOTTY;
	}
}
