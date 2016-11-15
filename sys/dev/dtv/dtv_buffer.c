/* $NetBSD: dtv_buffer.c,v 1.7 2011/08/09 01:42:24 jmcneill Exp $ */

/*-
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by Jared D. McNeill.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dtv_buffer.c,v 1.7 2011/08/09 01:42:24 jmcneill Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/select.h>

#include <dev/dtv/dtvvar.h>

#define	BLOCK_SIZE	DTV_DEFAULT_BLOCKSIZE
#define	BLOCK_ALIGN(a)	(((a) + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1))

static void
dtv_buffer_write(struct dtv_softc *sc, const uint8_t *buf, size_t buflen)
{
	struct dtv_stream *ds = &sc->sc_stream;
	struct dtv_buffer *db;
	struct dtv_scatter_io sio;
	size_t resid = buflen, avail;
       	off_t offset = 0;

	KASSERT(buflen == TS_PKTLEN);

	while (resid > 0) {
		mutex_enter(&ds->ds_ingress_lock);

		if (SIMPLEQ_EMPTY(&ds->ds_ingress)) {
			aprint_debug_dev(sc->sc_dev,
			    "dropping sample (%zu)\n", resid);
			mutex_exit(&ds->ds_ingress_lock);
			return;
		}

		db = SIMPLEQ_FIRST(&ds->ds_ingress);
		mutex_exit(&ds->ds_ingress_lock);

		avail = min(db->db_length - db->db_bytesused, resid);
		if (dtv_scatter_io_init(&ds->ds_data,
		    db->db_offset + db->db_bytesused, avail, &sio)) {
			dtv_scatter_io_copyin(&sio, buf + offset);
			db->db_bytesused += (avail - sio.sio_resid);
			offset += (avail - sio.sio_resid);
			resid -= (avail - sio.sio_resid);
		}

		if (db->db_bytesused == db->db_length) {
			mutex_enter(&ds->ds_ingress_lock);
			SIMPLEQ_REMOVE_HEAD(&ds->ds_ingress, db_entries);
			mutex_exit(&ds->ds_ingress_lock);
			mutex_enter(&ds->ds_egress_lock);
			SIMPLEQ_INSERT_TAIL(&ds->ds_egress, db, db_entries);
			selnotify(&ds->ds_sel, 0, 0);
			cv_broadcast(&ds->ds_sample_cv);
			mutex_exit(&ds->ds_egress_lock);
		}
	}
}

void
dtv_buffer_submit(void *priv, const struct dtv_payload *payload)
{
	struct dtv_softc *sc = priv;
	struct dtv_ts *ts = &sc->sc_ts;
	const uint8_t *tspkt;
	unsigned int npkts, i;

	tspkt = payload->data;
	npkts = payload->size / TS_PKTLEN;
	for (i = 0; i < npkts; i++) {
		if (TS_HAS_SYNC(tspkt)) {
			if (ts->ts_pidfilter[TS_PID(tspkt)]) {
				dtv_buffer_write(sc, tspkt, TS_PKTLEN);
			}
			dtv_demux_write(sc, tspkt, TS_PKTLEN);
		}
		tspkt += TS_PKTLEN;
	}
}

static struct dtv_buffer *
dtv_buffer_alloc(void)
{
	return kmem_alloc(sizeof(struct dtv_buffer), KM_SLEEP);
}

static void
dtv_buffer_free(struct dtv_buffer *db)
{
	kmem_free(db, sizeof(*db));
}

int
dtv_buffer_realloc(struct dtv_softc *sc, size_t bufsize)
{
	struct dtv_stream *ds = &sc->sc_stream;
	unsigned int i, nbufs, oldnbufs, minnbufs;
	struct dtv_buffer **oldbuf;
	off_t offset;
	int error;

	nbufs = BLOCK_ALIGN(bufsize) / BLOCK_SIZE;

	error = dtv_scatter_buf_set_size(&ds->ds_data, bufsize);
	if (error)
		return error;

	oldnbufs = ds->ds_nbufs;
	oldbuf = ds->ds_buf;

	ds->ds_nbufs = nbufs;
	if (nbufs > 0) {
		ds->ds_buf = kmem_alloc(sizeof(struct dtv_buffer *) * nbufs,
		    KM_SLEEP);
		if (ds->ds_buf == NULL) {
			ds->ds_nbufs = oldnbufs;
			ds->ds_buf = oldbuf;
			return ENOMEM;
		}
	} else {
		ds->ds_buf = NULL;
	}

	minnbufs = min(nbufs, oldnbufs);
	for (i = 0; i < minnbufs; i++)
		ds->ds_buf[i] = oldbuf[i];
	for (; i < nbufs; i++)
		ds->ds_buf[i] = dtv_buffer_alloc();
	for (; i < oldnbufs; i++) {
		dtv_buffer_free(oldbuf[i]);
		oldbuf[i] = NULL;
	}
	if (oldbuf != NULL)
		kmem_free(oldbuf, sizeof(struct dtv_buffer *) * oldnbufs);

	offset = 0;
	for (i = 0; i < nbufs; i++) {
		ds->ds_buf[i]->db_offset = offset;
		ds->ds_buf[i]->db_bytesused = 0;
		ds->ds_buf[i]->db_length = BLOCK_SIZE;
		offset += BLOCK_SIZE;
	}

	return 0;
}

static struct dtv_buffer *
dtv_stream_dequeue(struct dtv_stream *ds)
{
	struct dtv_buffer *db;

	if (!SIMPLEQ_EMPTY(&ds->ds_egress)) {
		db = SIMPLEQ_FIRST(&ds->ds_egress);
		SIMPLEQ_REMOVE_HEAD(&ds->ds_egress, db_entries);
		return db;
	}

	return NULL;
}

static void
dtv_stream_enqueue(struct dtv_stream *ds, struct dtv_buffer *db)
{
	db->db_bytesused = 0;
	SIMPLEQ_INSERT_TAIL(&ds->ds_ingress, db, db_entries);
}

int
dtv_buffer_setup(struct dtv_softc *sc)
{
	struct dtv_stream *ds = &sc->sc_stream;
	unsigned int i;

	mutex_enter(&ds->ds_ingress_lock);
	for (i = 0; i < ds->ds_nbufs; i++)
		dtv_stream_enqueue(ds, ds->ds_buf[i]);
	mutex_exit(&ds->ds_ingress_lock);

	return 0;
}

int
dtv_buffer_destroy(struct dtv_softc *sc)
{
	struct dtv_stream *ds = &sc->sc_stream;

	mutex_enter(&ds->ds_ingress_lock);
	while (SIMPLEQ_FIRST(&ds->ds_ingress))
		SIMPLEQ_REMOVE_HEAD(&ds->ds_ingress, db_entries);
	mutex_exit(&ds->ds_ingress_lock);
	mutex_enter(&ds->ds_egress_lock);
	while (SIMPLEQ_FIRST(&ds->ds_egress))
		SIMPLEQ_REMOVE_HEAD(&ds->ds_egress, db_entries);
	mutex_exit(&ds->ds_egress_lock);

	return 0;
}

int
dtv_buffer_read(struct dtv_softc *sc, struct uio *uio, int flags)
{
	struct dtv_stream *ds = &sc->sc_stream;
	struct dtv_buffer *db;
	struct dtv_scatter_io sio;
	off_t offset;
	size_t len, bread = 0;
	int error;

	while (uio->uio_resid > 0) {
retry:
		mutex_enter(&ds->ds_egress_lock);
		while (SIMPLEQ_EMPTY(&ds->ds_egress)) {
			if (flags & IO_NDELAY) {
				mutex_exit(&ds->ds_egress_lock);
				return EWOULDBLOCK;
			}

			error = cv_wait_sig(&ds->ds_sample_cv,
			    &ds->ds_egress_lock);
			if (error) {
				mutex_exit(&ds->ds_egress_lock);
				return EINTR;
			}
		}
		db = SIMPLEQ_FIRST(&ds->ds_egress);
		mutex_exit(&ds->ds_egress_lock);

		if (db->db_bytesused == 0) {
			mutex_enter(&ds->ds_egress_lock);
			db = dtv_stream_dequeue(ds);
			mutex_exit(&ds->ds_egress_lock);
			mutex_enter(&ds->ds_ingress_lock);
			dtv_stream_enqueue(ds, db);
			mutex_exit(&ds->ds_ingress_lock);
			ds->ds_bytesread = 0;
			goto retry;
		}

		len = min(uio->uio_resid, db->db_bytesused - ds->ds_bytesread);
		offset = db->db_offset + ds->ds_bytesread;

		if (dtv_scatter_io_init(&ds->ds_data, offset, len, &sio)) {
			error = dtv_scatter_io_uiomove(&sio, uio);
			if (error == EFAULT)
				return EFAULT;
			ds->ds_bytesread += (len - sio.sio_resid);
			bread += (len - sio.sio_resid);
		}

		if (ds->ds_bytesread >= db->db_bytesused) {
			mutex_enter(&ds->ds_egress_lock);
			db = dtv_stream_dequeue(ds);
			mutex_exit(&ds->ds_egress_lock);
			mutex_enter(&ds->ds_ingress_lock);
			dtv_stream_enqueue(ds, db);
			mutex_exit(&ds->ds_ingress_lock);

			ds->ds_bytesread = 0;
		}
	}

	return 0;
}

int
dtv_buffer_poll(struct dtv_softc *sc, int events, lwp_t *l)
{
	struct dtv_stream *ds = &sc->sc_stream;
	int revents = 0;
#ifdef DTV_BUFFER_DEBUG
	struct dtv_buffer *db;
	size_t bufsize = 0;
#endif

	mutex_enter(&ds->ds_egress_lock);
	if (!SIMPLEQ_EMPTY(&ds->ds_egress)) {
#ifdef DTV_BUFFER_DEBUG
		SIMPLEQ_FOREACH(db, &ds->ds_egress, db_entries)
			bufsize += db->db_bytesused;
#endif
		revents |= (POLLIN | POLLOUT | POLLPRI);
	} else {
		selrecord(l, &ds->ds_sel);
	}
	mutex_exit(&ds->ds_egress_lock);

#ifdef DTV_BUFFER_DEBUG
	device_printf(sc->sc_dev, "%s: bufsize=%zu\n", __func__, bufsize);
#endif

	return revents;
}
