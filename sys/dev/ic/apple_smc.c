/*	$NetBSD: apple_smc.c,v 1.6 2014/04/25 23:54:59 riastradh Exp $	*/

/*
 * Apple System Management Controller
 */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: apple_smc.c,v 1.6 2014/04/25 23:54:59 riastradh Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#if 0                           /* XXX sysctl */
#include <sys/sysctl.h>
#endif
#include <sys/systm.h>

#include <dev/ic/apple_smc.h>
#include <dev/ic/apple_smcreg.h>
#include <dev/ic/apple_smcvar.h>

/* Must match the config(5) name.  */
#define	APPLE_SMC_BUS	"applesmcbus"

static int	apple_smc_search(device_t, cfdata_t, const int *, void *);
static uint8_t	apple_smc_bus_read_1(struct apple_smc_tag *, bus_size_t);
static void	apple_smc_bus_write_1(struct apple_smc_tag *, bus_size_t,
		    uint8_t);
static int	apple_smc_read_data(struct apple_smc_tag *, uint8_t *);
static int	apple_smc_write(struct apple_smc_tag *, bus_size_t, uint8_t);
static int	apple_smc_write_cmd(struct apple_smc_tag *, uint8_t);
static int	apple_smc_write_data(struct apple_smc_tag *, uint8_t);
static int	apple_smc_begin(struct apple_smc_tag *, uint8_t,
		    const char *, uint8_t);
static int	apple_smc_input(struct apple_smc_tag *, uint8_t,
		    const char *, void *, uint8_t);
static int	apple_smc_output(struct apple_smc_tag *, uint8_t,
		    const char *, const void *, uint8_t);

void
apple_smc_attach(struct apple_smc_tag *smc)
{

	mutex_init(&smc->smc_io_lock, MUTEX_DEFAULT, IPL_NONE);
#if 0				/* XXX sysctl */
	apple_smc_sysctl_setup(smc);
#endif

	/* Attach any children.  */
        (void)apple_smc_rescan(smc, APPLE_SMC_BUS, NULL);
}

int
apple_smc_detach(struct apple_smc_tag *smc, int flags)
{
	int error;

	/* Fail if we can't detach all our children.  */
	error = config_detach_children(smc->smc_dev, flags);
	if (error)
		return error;

#if 0				/* XXX sysctl */
	sysctl_teardown(&smc->smc_log);
#endif
	mutex_destroy(&smc->smc_io_lock);

	return 0;
}

int
apple_smc_rescan(struct apple_smc_tag *smc, const char *ifattr,
    const int *locators)
{

	/* Let autoconf(9) do the work of finding new children.  */
	(void)config_search_loc(&apple_smc_search, smc->smc_dev, APPLE_SMC_BUS,
	    locators, smc);
	return 0;
}

static int
apple_smc_search(device_t parent, cfdata_t cf, const int *locators, void *aux)
{
	struct apple_smc_tag *const smc = aux;
	static const struct apple_smc_attach_args zero_asa;
	struct apple_smc_attach_args asa = zero_asa;
	device_t dev;
	deviter_t di;
	bool attached = false;

	/*
	 * If this device has already attached, don't attach it again.
	 *
	 * XXX This is a pretty silly way to query the children, but
	 * struct device doesn't seem to list its children.
	 */
	for (dev = deviter_first(&di, DEVITER_F_LEAVES_FIRST);
	     dev != NULL;
	     dev = deviter_next(&di)) {
		if (device_parent(dev) != parent)
			continue;
		if (!device_is_a(dev, cf->cf_name))
			continue;
		attached = true;
		break;
	}
	deviter_release(&di);
	if (attached)
		return 0;

	/* If this device doesn't match, don't attach it.  */
	if (!config_match(parent, cf, aux))
		return 0;

	/* Looks hunky-dory.  Attach.  */
	asa.asa_smc = smc;
	(void)config_attach_loc(parent, cf, locators, &asa, NULL);
	return 0;
}

void
apple_smc_child_detached(struct apple_smc_tag *smc __unused,
    device_t child __unused)
{
	/* We keep no books about our children.  */
}

static uint8_t
apple_smc_bus_read_1(struct apple_smc_tag *smc, bus_size_t reg)
{

	return bus_space_read_1(smc->smc_bst, smc->smc_bsh, reg);
}

static void
apple_smc_bus_write_1(struct apple_smc_tag *smc, bus_size_t reg, uint8_t v)
{

	bus_space_write_1(smc->smc_bst, smc->smc_bsh, reg, v);
}

/*
 * XXX These delays are pretty randomly chosen.  Wait in 100 us
 * increments, up to a total of 1 ms.
 */

static int
apple_smc_read_data(struct apple_smc_tag *smc, uint8_t *byte)
{
	uint8_t status;
	unsigned int i;

	KASSERT(mutex_owned(&smc->smc_io_lock));

	/*
	 * Wait until the status register says there's data to read and
	 * read it.
	 */
	for (i = 0; i < 100; i++) {
		status = apple_smc_bus_read_1(smc, APPLE_SMC_CSR);
		if (status & APPLE_SMC_STATUS_READ_READY) {
			*byte = apple_smc_bus_read_1(smc, APPLE_SMC_DATA);
			return 0;
		}
		DELAY(100);
	}

	return ETIMEDOUT;
}

static int
apple_smc_write(struct apple_smc_tag *smc, bus_size_t reg, uint8_t byte)
{
	uint8_t status;
	unsigned int i;

	KASSERT(mutex_owned(&smc->smc_io_lock));

	/*
	 * Write the byte and then wait until the status register says
	 * it has been accepted.
	 */
	apple_smc_bus_write_1(smc, reg, byte);
	for (i = 0; i < 100; i++) {
		status = apple_smc_bus_read_1(smc, APPLE_SMC_CSR);
		if (status & APPLE_SMC_STATUS_WRITE_ACCEPTED)
			return 0;
		DELAY(100);

		/* Write again if it hasn't been acknowledged at all.  */
		if (!(status & APPLE_SMC_STATUS_WRITE_PENDING))
			apple_smc_bus_write_1(smc, reg, byte);
	}

	return ETIMEDOUT;
}

static int
apple_smc_write_cmd(struct apple_smc_tag *smc, uint8_t cmd)
{

	return apple_smc_write(smc, APPLE_SMC_CSR, cmd);
}

static int
apple_smc_write_data(struct apple_smc_tag *smc, uint8_t data)
{

	return apple_smc_write(smc, APPLE_SMC_DATA, data);
}

static int
apple_smc_begin(struct apple_smc_tag *smc, uint8_t cmd, const char *key,
    uint8_t size)
{
	unsigned int i;
	int error;

	KASSERT(mutex_owned(&smc->smc_io_lock));

	/* Write the command first.  */
	error = apple_smc_write_cmd(smc, cmd);
	if (error)
		return error;

	/* Write the key next.  */
	for (i = 0; i < 4; i++) {
		error = apple_smc_write_data(smc, key[i]);
		if (error)
			return error;
	}

	/* Finally, report how many bytes of data we want to send/receive.  */
	error = apple_smc_write_data(smc, size);
	if (error)
		return error;

	return 0;
}

static int
apple_smc_input(struct apple_smc_tag *smc, uint8_t cmd, const char *key,
    void *buffer, uint8_t size)
{
	uint8_t *bytes = buffer;
	uint8_t i;
	int error;

	/* Grab the SMC I/O lock.  */
	mutex_enter(&smc->smc_io_lock);

	/* Initiate the command with this key.  */
	error = apple_smc_begin(smc, cmd, key, size);
	if (error)
		goto out;

	/* Read each byte of data in sequence.  */
	for (i = 0; i < size; i++) {
		error = apple_smc_read_data(smc, &bytes[i]);
		if (error)
			goto out;
	}

	/* Success!  */
	error = 0;

out:	mutex_exit(&smc->smc_io_lock);
	return error;
}

static int
apple_smc_output(struct apple_smc_tag *smc, uint8_t cmd, const char *key,
    const void *buffer, uint8_t size)
{
	const uint8_t *bytes = buffer;
	uint8_t i;
	int error;

	/* Grab the SMC I/O lock.  */
	mutex_enter(&smc->smc_io_lock);

	/* Initiate the command with this key.  */
	error = apple_smc_begin(smc, cmd, key, size);
	if (error)
		goto out;

	/* Write each byte of data in sequence.  */
	for (i = 0; i < size; i++) {
		error = apple_smc_write_data(smc, bytes[i]);
		if (error)
			goto out;
	}

	/* Success!  */
	error = 0;

out:	mutex_exit(&smc->smc_io_lock);
	return error;
}

struct apple_smc_key {
	char ask_name[4 + 1];
	struct apple_smc_desc ask_desc;
#ifdef DIAGNOSTIC
	struct apple_smc_tag *ask_smc;
#endif
};

const char *
apple_smc_key_name(const struct apple_smc_key *key)
{

	return key->ask_name;
}

const struct apple_smc_desc *
apple_smc_key_desc(const struct apple_smc_key *key)
{

	return &key->ask_desc;
}

uint32_t
apple_smc_nkeys(struct apple_smc_tag *smc)
{

	return smc->smc_nkeys;
}

int
apple_smc_nth_key(struct apple_smc_tag *smc, uint32_t index,
    const char type[4 + 1], struct apple_smc_key **keyp)
{
	union { uint32_t u32; char name[4]; } index_be;
	struct apple_smc_key *key;
	int error;

	/* Paranoia: type must be NULL or 4 non-null characters long.  */
	if ((type != NULL) && (strlen(type) != 4))
		return EINVAL;

	/* Create a new key.  XXX Consider caching these.  */
	key = kmem_alloc(sizeof(*key), KM_SLEEP);
#ifdef DIAGNOSTIC
	key->ask_smc = smc;
#endif

	/* Ask the SMC what the name of the key by this number is.  */
	index_be.u32 = htobe32(index);
	error = apple_smc_input(smc, APPLE_SMC_CMD_NTH_KEY, index_be.name,
	    key->ask_name, 4);
	if (error)
		goto fail;

	/* Null-terminate the name. */
	key->ask_name[4] = '\0';

	/* Ask the SMC for a description of this key by name.  */
	CTASSERT(sizeof(key->ask_desc) == 6);
	error = apple_smc_input(smc, APPLE_SMC_CMD_KEY_DESC, key->ask_name,
	    &key->ask_desc, 6);
	if (error)
		goto fail;

	/* Fail with EINVAL if the types don't match.  */
	if ((type != NULL) && (0 != memcmp(key->ask_desc.asd_type, type, 4))) {
		error = EINVAL;
		goto fail;
	}

	/* Success!  */
	*keyp = key;
	return 0;

fail:	kmem_free(key, sizeof(*key));
	return error;
}

int
apple_smc_named_key(struct apple_smc_tag *smc, const char name[4 + 1],
    const char type[4 + 1], struct apple_smc_key **keyp)
{
	struct apple_smc_key *key;
	int error;

	/* Paranoia: name must be 4 non-null characters long.  */
	KASSERT(name != NULL);
	if (strlen(name) != 4)
		return EINVAL;

	/* Paranoia: type must be NULL or 4 non-null characters long.  */
	if ((type != NULL) && (strlen(type) != 4))
		return EINVAL;

	/* Create a new key.  XXX Consider caching these.  */
	key = kmem_alloc(sizeof(*key), KM_SLEEP);
#ifdef DIAGNOSTIC
	key->ask_smc = smc;
#endif

	/* Use the specified name, and make sure it's null-terminated.  */
	(void)memcpy(key->ask_name, name, 4);
	key->ask_name[4] = '\0';

	/* Ask the SMC for a description of this key by name.  */
	CTASSERT(sizeof(key->ask_desc) == 6);
	error = apple_smc_input(smc, APPLE_SMC_CMD_KEY_DESC, key->ask_name,
	    &key->ask_desc, 6);
	if (error)
		goto fail;

	/* Fail with EINVAL if the types don't match.  */
	if ((type != NULL) && (0 != memcmp(key->ask_desc.asd_type, type, 4))) {
		error = EINVAL;
		goto fail;
	}

	/* Success!  */
	*keyp = key;
	return 0;

fail:	kmem_free(key, sizeof(*key));
	return error;
}

void
apple_smc_release_key(struct apple_smc_tag *smc, struct apple_smc_key *key)
{

#ifdef DIAGNOSTIC
	/* Make sure the caller didn't mix up SMC tags.  */
	if (key->ask_smc != smc)
		aprint_error_dev(smc->smc_dev,
		    "releasing key with wrong tag: %p != %p",
		    smc, key->ask_smc);
#endif

	/* Nothing to do but free the key's memory.  */
	kmem_free(key, sizeof(*key));
}

int
apple_smc_key_search(struct apple_smc_tag *smc, const char *name,
    uint32_t *result)
{
	struct apple_smc_key *key;
	uint32_t start = 0, end = apple_smc_nkeys(smc), median;
	int cmp;
	int error;

	/* Do a binary search on the SMC's key space.  */
	while (start < end) {
		median = (start + ((end - start) / 2));
		error = apple_smc_nth_key(smc, median, NULL, &key);
		if (error)
			return error;

		cmp = memcmp(name, apple_smc_key_name(key), 4);
		if (cmp < 0)
			end = median;
		else if (cmp > 0)
			start = (median + 1);
		else
			start = end = median; /* stop here */

		apple_smc_release_key(smc, key);
	}

	/* Success!  */
	*result = start;
	return 0;
}

int
apple_smc_read_key(struct apple_smc_tag *smc, const struct apple_smc_key *key,
    void *buffer, uint8_t size)
{

	/* Refuse if software and hardware disagree on the key's size.  */
	if (key->ask_desc.asd_size != size)
		return EINVAL;

	/* Refuse if the hardware doesn't want us to read it.  */
	if (!(key->ask_desc.asd_flags & APPLE_SMC_FLAG_READ))
		return EACCES;

	/* Looks good.  Try reading it from the hardware.  */
	return apple_smc_input(smc, APPLE_SMC_CMD_READ_KEY, key->ask_name,
	    buffer, size);
}

int
apple_smc_read_key_1(struct apple_smc_tag *smc,
    const struct apple_smc_key *key, uint8_t *p)
{

	return apple_smc_read_key(smc, key, p, 1);
}

int
apple_smc_read_key_2(struct apple_smc_tag *smc,
    const struct apple_smc_key *key, uint16_t *p)
{
	uint16_t be;
	int error;

	/* Read a big-endian quantity from the hardware.  */
	error = apple_smc_read_key(smc, key, &be, 2);
	if (error)
		return error;

	/* Convert it to host order.  */
	*p = be16toh(be);

	/* Success!  */
	return 0;
}

int
apple_smc_read_key_4(struct apple_smc_tag *smc,
    const struct apple_smc_key *key, uint32_t *p)
{
	uint32_t be;
	int error;

	/* Read a big-endian quantity from the hardware.  */
	error = apple_smc_read_key(smc, key, &be, 4);
	if (error)
		return error;

	/* Convert it to host order.  */
	*p = be32toh(be);

	/* Success!  */
	return 0;
}

int
apple_smc_write_key(struct apple_smc_tag *smc, const struct apple_smc_key *key,
    const void *buffer, uint8_t size)
{

	/* Refuse if software and hardware disagree on the key's size.  */
	if (key->ask_desc.asd_size != size)
		return EINVAL;

	/* Refuse if the hardware doesn't want us to write it.  */
	if (!(key->ask_desc.asd_flags & APPLE_SMC_FLAG_WRITE))
		return EACCES;

	/* Looks good.  Try writing it to the hardware.  */
	return apple_smc_output(smc, APPLE_SMC_CMD_WRITE_KEY, key->ask_name,
	    buffer, size);
}

int
apple_smc_write_key_1(struct apple_smc_tag *smc,
    const struct apple_smc_key *key, uint8_t v)
{

	return apple_smc_write_key(smc, key, &v, 1);
}

int
apple_smc_write_key_2(struct apple_smc_tag *smc,
    const struct apple_smc_key *key, uint16_t v)
{
	/* Convert the quantity from host to big-endian byte order.  */
	const uint16_t v_be = htobe16(v);

	/* Write the big-endian quantity to the hardware.  */
	return apple_smc_write_key(smc, key, &v_be, 2);
}

int
apple_smc_write_key_4(struct apple_smc_tag *smc,
    const struct apple_smc_key *key, uint32_t v)
{
	/* Convert the quantity from host to big-endian byte order.  */
	const uint32_t v_be = htobe32(v);

	/* Write the big-endian quantity to the hardware.  */
	return apple_smc_write_key(smc, key, &v_be, 4);
}

MODULE(MODULE_CLASS_MISC, apple_smc, NULL)

static int
apple_smc_modcmd(modcmd_t cmd, void *data __unused)
{

	/* Nothing to do for now to set up or tear down the module.  */
	switch (cmd) {
	case MODULE_CMD_INIT:
		return 0;

	case MODULE_CMD_FINI:
		return 0;

	default:
		return ENOTTY;
	}
}
