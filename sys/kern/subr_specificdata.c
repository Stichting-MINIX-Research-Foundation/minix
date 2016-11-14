/*	$NetBSD: subr_specificdata.c,v 1.13 2008/04/28 20:24:04 martin Exp $	*/

/*-
 * Copyright (c) 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*-
 * Copyright (c) 2006 YAMAMOTO Takashi.
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
__KERNEL_RCSID(0, "$NetBSD: subr_specificdata.c,v 1.13 2008/04/28 20:24:04 martin Exp $");

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/specificdata.h>
#include <sys/queue.h>
#include <sys/mutex.h>

/*
 * Locking notes:
 *
 * The specdataref_container pointer in the specificdata_reference
 * is volatile.  To read it, you must hold EITHER the domain lock
 * or the ref lock.  To write it, you must hold BOTH the domain lock
 * and the ref lock.  The locks must be acquired in the following
 * order:
 *	domain -> ref
 */

typedef struct {
	specificdata_dtor_t	ski_dtor;
} specificdata_key_impl;

struct specificdata_container {
	size_t		sc_nkey;
	LIST_ENTRY(specificdata_container) sc_list;
	void *		sc_data[];	/* variable length */
};

#define	SPECIFICDATA_CONTAINER_BYTESIZE(n)		\
	(sizeof(struct specificdata_container) + ((n) * sizeof(void *)))

struct specificdata_domain {
	kmutex_t	sd_lock;
	unsigned int	sd_nkey;
	LIST_HEAD(, specificdata_container) sd_list;
	specificdata_key_impl *sd_keys;
};

static void
specificdata_container_link(specificdata_domain_t sd,
			    specificdata_container_t sc)
{

	LIST_INSERT_HEAD(&sd->sd_list, sc, sc_list);
}

static void
specificdata_container_unlink(specificdata_domain_t sd,
			      specificdata_container_t sc)
{

	LIST_REMOVE(sc, sc_list);
}

static void
specificdata_destroy_datum(specificdata_domain_t sd,
			   specificdata_container_t sc, specificdata_key_t key)
{
	specificdata_dtor_t dtor;
	void *data;

	if (key >= sc->sc_nkey)
		return;

	KASSERT(key < sd->sd_nkey);
	
	data = sc->sc_data[key];
	dtor = sd->sd_keys[key].ski_dtor;

	if (dtor != NULL) {
		if (data != NULL) {
			sc->sc_data[key] = NULL;
			(*dtor)(data);
		}
	} else {
		KASSERT(data == NULL);
	}
}

static void
specificdata_noop_dtor(void *data)
{

	/* nothing */
}

/*
 * specificdata_domain_create --
 *	Create a specificdata domain.
 */
specificdata_domain_t
specificdata_domain_create(void)
{
	specificdata_domain_t sd;

	sd = kmem_zalloc(sizeof(*sd), KM_SLEEP);
	KASSERT(sd != NULL);
	mutex_init(&sd->sd_lock, MUTEX_DEFAULT, IPL_NONE);
	LIST_INIT(&sd->sd_list);

	return (sd);
}

/*
 * specificdata_domain_delete --
 *	Destroy a specificdata domain.
 */
void
specificdata_domain_delete(specificdata_domain_t sd)
{

	panic("specificdata_domain_delete: not implemented");
}

/*
 * specificdata_key_create --
 *	Create a specificdata key for a domain.
 *
 *	Note: This is a rare operation.
 */
int
specificdata_key_create(specificdata_domain_t sd, specificdata_key_t *keyp,
			specificdata_dtor_t dtor)
{
	specificdata_key_impl *newkeys;
	specificdata_key_t key = 0;
	size_t nsz;

	ASSERT_SLEEPABLE();

	if (dtor == NULL)
		dtor = specificdata_noop_dtor;
	
	mutex_enter(&sd->sd_lock);

	if (sd->sd_keys == NULL)
		goto needalloc;

	for (; key < sd->sd_nkey; key++) {
		if (sd->sd_keys[key].ski_dtor == NULL)
			goto gotit;
	}

 needalloc:
	nsz = (sd->sd_nkey + 1) * sizeof(*newkeys);
	/* XXXSMP allocating memory while holding a lock. */
	newkeys = kmem_zalloc(nsz, KM_SLEEP);
	KASSERT(newkeys != NULL);
	if (sd->sd_keys != NULL) {
		size_t osz = sd->sd_nkey * sizeof(*newkeys);
		memcpy(newkeys, sd->sd_keys, osz);
		kmem_free(sd->sd_keys, osz);
	}
	sd->sd_keys = newkeys;
	sd->sd_nkey++;
 gotit:
	sd->sd_keys[key].ski_dtor = dtor;

	mutex_exit(&sd->sd_lock);

	*keyp = key;
	return (0);
}

/*
 * specificdata_key_delete --
 *	Destroy a specificdata key for a domain.
 *
 *	Note: This is a rare operation.
 */
void
specificdata_key_delete(specificdata_domain_t sd, specificdata_key_t key)
{
	specificdata_container_t sc;

	mutex_enter(&sd->sd_lock);

	if (key >= sd->sd_nkey)
		goto out;

	/*
	 * Traverse all of the specificdata containers in the domain
	 * and the destroy the datum for the dying key.
	 */
	LIST_FOREACH(sc, &sd->sd_list, sc_list) {
		specificdata_destroy_datum(sd, sc, key);
	}

	sd->sd_keys[key].ski_dtor = NULL;

 out:
	mutex_exit(&sd->sd_lock);
}

/*
 * specificdata_init --
 *	Initialize a specificdata container for operation in the
 *	specified domain.
 */
int
specificdata_init(specificdata_domain_t sd, specificdata_reference *ref)
{

	/*
	 * Just NULL-out the container pointer; we'll allocate the
	 * container the first time specificdata is put into it.
	 */
	ref->specdataref_container = NULL;
	mutex_init(&ref->specdataref_lock, MUTEX_DEFAULT, IPL_NONE);

	return (0);
}

/*
 * specificdata_fini --
 *	Destroy a specificdata container.  We destroy all of the datums
 *	stuffed into the container just as if the key were destroyed.
 */
void
specificdata_fini(specificdata_domain_t sd, specificdata_reference *ref)
{
	specificdata_container_t sc;
	specificdata_key_t key;

	ASSERT_SLEEPABLE();

	mutex_destroy(&ref->specdataref_lock);

	sc = ref->specdataref_container;
	if (sc == NULL)
		return;
	ref->specdataref_container = NULL;
	
	mutex_enter(&sd->sd_lock);

	specificdata_container_unlink(sd, sc);
	for (key = 0; key < sc->sc_nkey; key++) {
		specificdata_destroy_datum(sd, sc, key);
	}

	mutex_exit(&sd->sd_lock);

	kmem_free(sc, SPECIFICDATA_CONTAINER_BYTESIZE(sc->sc_nkey));
}

/*
 * specificdata_getspecific --
 *	Get a datum from a container.
 */
void *
specificdata_getspecific(specificdata_domain_t sd, specificdata_reference *ref,
			 specificdata_key_t key)
{
	specificdata_container_t sc;
	void *data = NULL;

	mutex_enter(&ref->specdataref_lock);

	sc = ref->specdataref_container;
	if (sc != NULL && key < sc->sc_nkey)
		data = sc->sc_data[key];

	mutex_exit(&ref->specdataref_lock);

	return (data);
}

/*
 * specificdata_getspecific_unlocked --
 *	Get a datum from a container in a lockless fashion.
 *
 *	Note: When using this routine, care must be taken to ensure
 *	that no other thread could cause the specificdata_reference
 *	to become invalid (i.e. point at the wrong container) by
 *	issuing a setspecific call or destroying the container.
 */
void *
specificdata_getspecific_unlocked(specificdata_domain_t sd,
				  specificdata_reference *ref,
				  specificdata_key_t key)
{
	specificdata_container_t sc;
	
	sc = ref->specdataref_container;
	if (sc != NULL && key < sc->sc_nkey)
		return (sc->sc_data[key]);

	return (NULL);
}

/*
 * specificdata_setspecific --
 *      Put a datum into a container.
 */
void
specificdata_setspecific(specificdata_domain_t sd,
			 specificdata_reference *ref,
			 specificdata_key_t key, void *data)
{
	specificdata_container_t sc, newsc;
	size_t newnkey, sz;

	ASSERT_SLEEPABLE();

	mutex_enter(&ref->specdataref_lock);

	sc = ref->specdataref_container;
	if (__predict_true(sc != NULL && key < sc->sc_nkey)) {
		sc->sc_data[key] = data;
		mutex_exit(&ref->specdataref_lock);
		return;
	}

	mutex_exit(&ref->specdataref_lock);

	/*
	 * Slow path: need to resize.
	 */
	
	mutex_enter(&sd->sd_lock);
	newnkey = sd->sd_nkey;
	if (key >= newnkey) {
		mutex_exit(&sd->sd_lock);
		panic("specificdata_setspecific");
	}
	sz = SPECIFICDATA_CONTAINER_BYTESIZE(newnkey);
	newsc = kmem_zalloc(sz, KM_SLEEP);
	KASSERT(newsc != NULL);
	newsc->sc_nkey = newnkey;

	mutex_enter(&ref->specdataref_lock);

	sc = ref->specdataref_container;
	if (sc != NULL) {
		if (key < sc->sc_nkey) {
			/*
			 * Someone beat us to the punch.  Unwind and put
			 * the object into the now large enough container.
			 */
			sc->sc_data[key] = data;
			mutex_exit(&ref->specdataref_lock);
			mutex_exit(&sd->sd_lock);
			kmem_free(newsc, sz);
			return;
		}
		specificdata_container_unlink(sd, sc);
		memcpy(newsc->sc_data, sc->sc_data,
		       sc->sc_nkey * sizeof(void *));
	}
	newsc->sc_data[key] = data;
	specificdata_container_link(sd, newsc);
	ref->specdataref_container = newsc;

	mutex_exit(&ref->specdataref_lock);
	mutex_exit(&sd->sd_lock);

	if (sc != NULL)
		kmem_free(sc, SPECIFICDATA_CONTAINER_BYTESIZE(sc->sc_nkey));
}
