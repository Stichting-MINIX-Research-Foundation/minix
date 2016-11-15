/*	$NetBSD: pud.c,v 1.11 2011/07/08 09:32:45 mrg Exp $	*/

/*
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Research Foundation of Helsinki University of Technology
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pud.c,v 1.11 2011/07/08 09:32:45 mrg Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/poll.h>
#include <sys/queue.h>

#include <dev/pud/pud_sys.h>
#include <dev/putter/putter_sys.h>

void	pudattach(void);

static int	pud_putter_getout(void *, size_t, int, uint8_t **,
				  size_t *, void **);
static void	pud_putter_releaseout(void *, void *, int);
static int	pud_putter_dispatch(void *, struct putter_hdr *);
static size_t	pud_putter_waitcount(void *);
static int	pud_putter_close(void *);

struct putter_ops pud_putter = {
	.pop_getout	= pud_putter_getout,
	.pop_releaseout	= pud_putter_releaseout,
	.pop_waitcount	= pud_putter_waitcount,
	.pop_dispatch	= pud_putter_dispatch,
	.pop_close	= pud_putter_close,
};

extern struct bdevsw pud_bdevsw;
extern struct cdevsw pud_cdevsw;

kmutex_t pud_mtx;
static LIST_HEAD(, pud_dev) pudlist = LIST_HEAD_INITIALIZER(pudlist);

static uint64_t
nextreq(struct pud_dev *pd)
{
	uint64_t rv;

	mutex_enter(&pd->pd_mtx);
	rv = pd->pd_nextreq++;
	mutex_exit(&pd->pd_mtx);

	return rv;
}

static int
pud_putter_getout(void *this, size_t maxsize, int nonblock,
	uint8_t **data, size_t *dlen, void **cookie)
{
	struct pud_dev *pd = this;
	struct pud_touser *putp = NULL;
	int error = 0;

	mutex_enter(&pd->pd_mtx);
	for (;;) {
		if (TAILQ_EMPTY(&pd->pd_waitq_req)) {
			if (nonblock) {
				error = EWOULDBLOCK;
				break;
			}

			error = cv_wait_sig(&pd->pd_waitq_req_cv, &pd->pd_mtx);
			if (error)
				break;
			else
				continue;
		}

		putp = TAILQ_FIRST(&pd->pd_waitq_req);
		TAILQ_REMOVE(&pd->pd_waitq_req, putp, pt_entries);
		KASSERT(error == 0);
		break;
	}
	mutex_exit(&pd->pd_mtx);

	if (error == 0) {
		*data = (uint8_t *)putp->pt_pdr;
		*dlen = putp->pt_pdr->pdr_pth.pth_framelen;
		*cookie = putp;
	}

	return error;
}

static void
pud_putter_releaseout(void *this, void *cookie, int status)
{
	struct pud_dev *pd = this;
	struct pud_touser *putp = cookie;

	mutex_enter(&pd->pd_mtx);
	TAILQ_INSERT_TAIL(&pd->pd_waitq_resp, putp, pt_entries);
	mutex_exit(&pd->pd_mtx);

}

static size_t
pud_putter_waitcount(void *this)
{
	struct pud_dev *pd = this;
	size_t rv;

	mutex_enter(&pd->pd_mtx);
	rv = pd->pd_waitcount;
	mutex_exit(&pd->pd_mtx);

	return rv;
}

static int
pudop_dev(struct pud_dev *pd, struct pud_req *pdr)
{
	struct putter_hdr *pth = (void *)pdr;
	struct pud_touser *putp;

	mutex_enter(&pd->pd_mtx);
	TAILQ_FOREACH(putp, &pd->pd_waitq_resp, pt_entries)
		if (putp->pt_pdr->pdr_reqid == pdr->pdr_reqid)
			break;
	if (putp == NULL) {
		mutex_exit(&pd->pd_mtx);
		return EINVAL;
	}
	TAILQ_REMOVE(&pd->pd_waitq_resp, putp, pt_entries);
	mutex_exit(&pd->pd_mtx);

	if (pth->pth_framelen > putp->pt_pdr->pdr_len) {
		return EINVAL;
	}
	memcpy(putp->pt_pdr, pth, pth->pth_framelen);
	cv_signal(&putp->pt_cv);

	return 0;
}

/*
 * Register our major number.  Always register char device functions,
 * register block devices optionally.
 *
 * XXX: no way to configure "any major you like" currently.
 */
static int
pudconf_reg(struct pud_dev *pd, struct pud_conf_reg *pcr)
{
	struct bdevsw *bsw;
	devmajor_t cmajor, bmajor;
	int error;

	if (pcr->pm_version != (PUD_DEVELVERSION | PUD_VERSION)) {
		printf("pud version mismatch %d vs %d\n",
		    pcr->pm_version & ~PUD_DEVELVERSION, PUD_VERSION);
		return EINVAL; /* XXX */
	}

	cmajor = major(pcr->pm_regdev);
	if (pcr->pm_flags & PUD_CONFFLAG_BDEV) {
		bsw = &pud_bdevsw;
		bmajor = cmajor;
	} else {
		bsw = NULL;
		bmajor = NODEVMAJOR;
	}

	pcr->pm_devname[PUD_DEVNAME_MAX] = '\0';
        error = devsw_attach(pcr->pm_devname, bsw, &bmajor,
	    &pud_cdevsw, &cmajor);
	if (error == 0)
		pd->pd_dev = pcr->pm_regdev;

	return error;
}

static int
pudop_conf(struct pud_dev *pd, struct pud_req *pdr)
{
	int rv;

	switch (pdr->pdr_reqtype) {
	case PUD_CONF_REG:
		rv = pudconf_reg(pd, (struct pud_conf_reg *)pdr);
		break;
	case PUD_CONF_DEREG:
		/* unimplemented */
		rv = 0;
		break;
	default:
		rv = EINVAL;
		break;
	}

	return rv;
}

static int
pud_putter_dispatch(void *this, struct putter_hdr *pth)
{
	struct pud_dev *pd = this;
	struct pud_req *pdr = (void *)pth;
	int rv;

	if (pdr->pdr_pth.pth_framelen < sizeof(struct pud_req))
		return EINVAL;

	switch (pdr->pdr_reqclass) {
	case PUD_REQ_CDEV:
	case PUD_REQ_BDEV:
		rv = pudop_dev(pd, pdr);
		break;
	case PUD_REQ_CONF:
		rv = pudop_conf(pd, pdr);
		break;
	default:
		rv = EINVAL;
		break;
	}

	return rv;
}

/* Device server severed the umbilical cord */
static int
pud_putter_close(void *this)
{
	struct pud_dev *pd = this;
	struct pud_touser *putp;

	mutex_enter(&pud_mtx);
	LIST_REMOVE(pd, pd_entries);
	mutex_exit(&pud_mtx);

	mutex_enter(&pd->pd_mtx);
	while ((putp = TAILQ_FIRST(&pd->pd_waitq_req)) != NULL) {
		putp->pt_pdr->pdr_rv = ENXIO;
		cv_signal(&putp->pt_cv);
		TAILQ_REMOVE(&pd->pd_waitq_req, putp, pt_entries);
	}

	while ((putp = TAILQ_FIRST(&pd->pd_waitq_resp)) != NULL) {
		putp->pt_pdr->pdr_rv = ENXIO;
		cv_signal(&putp->pt_cv);
		TAILQ_REMOVE(&pd->pd_waitq_resp, putp, pt_entries);
	}
	if (pd->pd_waitcount)
		cv_wait(&pd->pd_draincv, &pd->pd_mtx);
	KASSERT(pd->pd_waitcount == 0);

	mutex_exit(&pd->pd_mtx);

	if (pd->pd_dev)
		devsw_detach(&pud_bdevsw /* XXX */, &pud_cdevsw);
		
	putter_detach(pd->pd_pi);

	mutex_destroy(&pd->pd_mtx);
	cv_destroy(&pd->pd_draincv);
	cv_destroy(&pd->pd_waitq_req_cv);
	kmem_free(pd, sizeof(struct pud_dev));

	return 0;
}

struct pud_dev *
pud_dev2pud(dev_t dev)
{
	struct pud_dev *pd;

	mutex_enter(&pud_mtx);
	LIST_FOREACH(pd, &pudlist, pd_entries)
		if (major(pd->pd_dev) == major(dev))
			break;
	mutex_exit(&pud_mtx);

	return pd;
}

/* Toss request to the device server and wait for result */
int
pud_request(dev_t dev, void *data, size_t dlen, int class, int type)
{
	struct pud_touser put;
	struct pud_req *pdr = data;
	struct pud_dev *pd;

	pd = pud_dev2pud(dev);
	if (pd == NULL)
		return ENXIO;

	pdr->pdr_dev = dev;
	pdr->pdr_len = pdr->pdr_pth.pth_framelen = dlen;
	pdr->pdr_reqid = nextreq(pd);

	pdr->pdr_reqclass = class;
	pdr->pdr_reqtype = type;

	put.pt_pdr = pdr;
	cv_init(&put.pt_cv, "pudresp");

	mutex_enter(&pd->pd_mtx);
	pd->pd_waitcount++;

	TAILQ_INSERT_TAIL(&pd->pd_waitq_req, &put, pt_entries);
	putter_notify(pd->pd_pi);
	cv_broadcast(&pd->pd_waitq_req_cv);
	cv_wait(&put.pt_cv, &pd->pd_mtx);

	if (--pd->pd_waitcount == 0)
		cv_signal(&pd->pd_draincv);
	mutex_exit(&pd->pd_mtx);
	cv_destroy(&put.pt_cv);

	return pdr->pdr_rv;
}

/* Called from putter based on minor dev number */
int
pud_config(int fd, int flags, int fmt)
{
	struct pud_dev *pd;

	pd = kmem_zalloc(sizeof(struct pud_dev), KM_SLEEP);
	pd->pd_pi = putter_attach(curlwp->l_proc->p_pid, fd, pd, &pud_putter);
	if (pd->pd_pi == NULL) {
		kmem_free(pd, sizeof(struct pud_dev));
		return ENOENT; /* XXX */
	}
	pd->pd_dev = NODEV;

	mutex_init(&pd->pd_mtx, MUTEX_DEFAULT, IPL_NONE);
	TAILQ_INIT(&pd->pd_waitq_req);
	TAILQ_INIT(&pd->pd_waitq_resp);
	cv_init(&pd->pd_waitq_req_cv, "pudreq");
	cv_init(&pd->pd_draincv, "pudrain");

	mutex_enter(&pud_mtx);
	LIST_INSERT_HEAD(&pudlist, pd, pd_entries);
	mutex_exit(&pud_mtx);

	return 0;
}

void
pudattach(void)
{
	int error;

	if ((error = putter_register(pud_config, PUTTER_MINOR_PUD)) != 0) {
		printf("pudattach: can't register to putter: %d\n", error);
		return;
	}
	mutex_init(&pud_mtx, MUTEX_DEFAULT, IPL_NONE);
}

MODULE(MODULE_CLASS_DRIVER, pud, "putter");

static int
pud_modcmd(modcmd_t cmd, void *arg)
{
	#ifdef _MODULE
	devmajor_t bmajor = NODEVMAJOR, cmajor = NODEVMAJOR;

	switch (cmd) {
	case MODULE_CMD_INIT:
		pudattach();
		return devsw_attach("pud", NULL, &bmajor,
		    &pud_cdevsw, &cmajor);
	case MODULE_CMD_FINI:
		return ENOTTY; /* XXX: puddetach */
	default:
		return ENOTTY;
	}
	#else
	if (cmd == MODULE_CMD_INIT)
		return 0;
	return ENOTTY;
	#endif
}
