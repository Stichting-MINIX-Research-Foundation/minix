/*	$NetBSD: i2c.c,v 1.49 2015/04/13 22:26:20 pgoyette Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef _KERNEL_OPT
#include "opt_i2c.h"
#endif

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i2c.c,v 1.49 2015/04/13 22:26:20 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/event.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/module.h>

#include <dev/i2c/i2cvar.h>

#include "locators.h"

#ifndef I2C_MAX_ADDR
#define I2C_MAX_ADDR	0x3ff	/* 10-bit address, max */
#endif

struct iic_softc {
	i2c_tag_t sc_tag;
	int sc_type;
	device_t sc_devices[I2C_MAX_ADDR + 1];
};

static dev_type_open(iic_open);
static dev_type_close(iic_close);
static dev_type_ioctl(iic_ioctl);

const struct cdevsw iic_cdevsw = {
	.d_open = iic_open,
	.d_close = iic_close,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = iic_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

extern struct cfdriver iic_cd;

static void	iic_smbus_intr_thread(void *);
static void	iic_fill_compat(struct i2c_attach_args*, const char*,
			size_t, char **);

static int
iic_print_direct(void *aux, const char *pnp)
{
	struct i2c_attach_args *ia = aux;

	if (pnp != NULL)
		aprint_normal("%s at %s addr 0x%02x", ia->ia_name, pnp,
			ia->ia_addr);
	else
		aprint_normal(" addr 0x%02x", ia->ia_addr);

	return UNCONF;
}

static int
iic_print(void *aux, const char *pnp)
{
	struct i2c_attach_args *ia = aux;

	if (ia->ia_addr != (i2c_addr_t)-1)
		aprint_normal(" addr 0x%x", ia->ia_addr);

	return UNCONF;
}

static int
iic_search(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct iic_softc *sc = device_private(parent);
	struct i2c_attach_args ia;

	ia.ia_tag = sc->sc_tag;
	ia.ia_size = cf->cf_loc[IICCF_SIZE];
	ia.ia_type = sc->sc_type;

	ia.ia_name = NULL;
	ia.ia_ncompat = 0;
	ia.ia_compat = NULL;

	for (ia.ia_addr = 0; ia.ia_addr <= I2C_MAX_ADDR; ia.ia_addr++) {
		if (sc->sc_devices[ia.ia_addr] != NULL)
			continue;

		if (cf->cf_loc[IICCF_ADDR] != -1 &&
		    cf->cf_loc[IICCF_ADDR] != ia.ia_addr)
			continue;

		if (config_match(parent, cf, &ia) > 0)
			sc->sc_devices[ia.ia_addr] =
			    config_attach(parent, cf, &ia, iic_print);
	}

	return 0;
}

static void
iic_child_detach(device_t parent, device_t child)
{
	struct iic_softc *sc = device_private(parent);
	int i;

	for (i = 0; i <= I2C_MAX_ADDR; i++)
		if (sc->sc_devices[i] == child) {
			sc->sc_devices[i] = NULL;
			break;
		}
}

static int
iic_rescan(device_t self, const char *ifattr, const int *locators)
{
	config_search_ia(iic_search, self, ifattr, NULL);
	return 0;
}

static int
iic_match(device_t parent, cfdata_t cf, void *aux)
{

	return 1;
}

static void
iic_attach(device_t parent, device_t self, void *aux)
{
	struct iic_softc *sc = device_private(self);
	struct i2cbus_attach_args *iba = aux;
	prop_array_t child_devices;
	prop_dictionary_t props;
	char *buf;
	i2c_tag_t ic;
	int rv;
	bool indirect_config;

	aprint_naive("\n");
	aprint_normal(": I2C bus\n");

	sc->sc_tag = iba->iba_tag;
	sc->sc_type = iba->iba_type;
	ic = sc->sc_tag;
	ic->ic_devname = device_xname(self);

	LIST_INIT(&(sc->sc_tag->ic_list));
	LIST_INIT(&(sc->sc_tag->ic_proc_list));

	rv = kthread_create(PRI_NONE, KTHREAD_MUSTJOIN, NULL,
	    iic_smbus_intr_thread, ic, &ic->ic_intr_thread,
	    "%s", ic->ic_devname);
	if (rv)
		aprint_error_dev(self, "unable to create intr thread\n");

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	props = device_properties(parent);
	if (!prop_dictionary_get_bool(props, "i2c-indirect-config",
	    &indirect_config))
		indirect_config = true;
	child_devices = prop_dictionary_get(props, "i2c-child-devices");
	if (child_devices) {
		unsigned int i, count;
		prop_dictionary_t dev;
		prop_data_t cdata;
		uint32_t addr, size;
		uint64_t cookie;
		const char *name;
		struct i2c_attach_args ia;
		int loc[2];

		memset(loc, 0, sizeof loc);
		count = prop_array_count(child_devices);
		for (i = 0; i < count; i++) {
			dev = prop_array_get(child_devices, i);
			if (!dev) continue;
 			if (!prop_dictionary_get_cstring_nocopy(
			    dev, "name", &name))
				continue;
			if (!prop_dictionary_get_uint32(dev, "addr", &addr))
				continue;
			if (!prop_dictionary_get_uint64(dev, "cookie", &cookie))
				cookie = 0;
			loc[0] = addr;
			if (prop_dictionary_get_uint32(dev, "size", &size))
				loc[1] = size;
			else
				loc[1] = -1;

			memset(&ia, 0, sizeof ia);
			ia.ia_addr = addr;
			ia.ia_type = sc->sc_type;
			ia.ia_tag = ic;
			ia.ia_name = name;
			ia.ia_cookie = cookie;
			ia.ia_size = size;

			buf = NULL;
			cdata = prop_dictionary_get(dev, "compatible");
			if (cdata)
				iic_fill_compat(&ia,
				    prop_data_data_nocopy(cdata),
				    prop_data_size(cdata), &buf);

			if (addr > I2C_MAX_ADDR) {
				aprint_error_dev(self,
				    "WARNING: ignoring bad device address "
				    "@ 0x%02x\n", addr);
			} else if (sc->sc_devices[addr] == NULL) {
				sc->sc_devices[addr] =
				    config_found_sm_loc(self, "iic", loc, &ia,
					iic_print_direct, NULL);
			}

			if (ia.ia_compat)
				free(ia.ia_compat, M_TEMP);
			if (buf)
				free(buf, M_TEMP);
		}
	} else if (indirect_config) {
		/*
		 * Attach all i2c devices described in the kernel
		 * configuration file.
		 */
		iic_rescan(self, "iic", NULL);
	}
}

static int
iic_detach(device_t self, int flags)
{
	struct iic_softc *sc = device_private(self);
	i2c_tag_t ic = sc->sc_tag;
	int i, error;
	void *hdl;

	for (i = 0; i <= I2C_MAX_ADDR; i++) {
		if (sc->sc_devices[i]) {
			error = config_detach(sc->sc_devices[i], flags);
			if (error)
				return error;
		}
	}

	if (ic->ic_running) {
		ic->ic_running = 0;
		wakeup(ic);
		kthread_join(ic->ic_intr_thread);
	}

	if (!LIST_EMPTY(&ic->ic_list)) {
		device_printf(self, "WARNING: intr handler list not empty\n");
		while (!LIST_EMPTY(&ic->ic_list)) {
			hdl = LIST_FIRST(&ic->ic_list);
			iic_smbus_intr_disestablish(ic, hdl);
		}
	}
	if (!LIST_EMPTY(&ic->ic_proc_list)) {
		device_printf(self, "WARNING: proc handler list not empty\n");
		while (!LIST_EMPTY(&ic->ic_proc_list)) {
			hdl = LIST_FIRST(&ic->ic_proc_list);
			iic_smbus_intr_disestablish_proc(ic, hdl);
		}
	}

	pmf_device_deregister(self);

	return 0;
}

static void
iic_smbus_intr_thread(void *aux)
{
	i2c_tag_t ic;
	struct ic_intr_list *il;

	ic = (i2c_tag_t)aux;
	ic->ic_running = 1;
	ic->ic_pending = 0;

	while (ic->ic_running) {
		if (ic->ic_pending == 0)
			tsleep(ic, PZERO, "iicintr", hz);
		if (ic->ic_pending > 0) {
			LIST_FOREACH(il, &(ic->ic_proc_list), il_next) {
				(*il->il_intr)(il->il_intrarg);
			}
			ic->ic_pending--;
		}
	}

	kthread_exit(0);
}

void *
iic_smbus_intr_establish(i2c_tag_t ic, int (*intr)(void *), void *intrarg)
{
	struct ic_intr_list *il;

	il = malloc(sizeof(struct ic_intr_list), M_DEVBUF, M_WAITOK);
	if (il == NULL)
		return NULL;

	il->il_intr = intr;
	il->il_intrarg = intrarg;

	LIST_INSERT_HEAD(&(ic->ic_list), il, il_next);

	return il;
}

void
iic_smbus_intr_disestablish(i2c_tag_t ic, void *hdl)
{
	struct ic_intr_list *il;

	il = (struct ic_intr_list *)hdl;

	LIST_REMOVE(il, il_next);
	free(il, M_DEVBUF);

	return;
}

void *
iic_smbus_intr_establish_proc(i2c_tag_t ic, int (*intr)(void *), void *intrarg)
{
	struct ic_intr_list *il;

	il = malloc(sizeof(struct ic_intr_list), M_DEVBUF, M_WAITOK);
	if (il == NULL)
		return NULL;

	il->il_intr = intr;
	il->il_intrarg = intrarg;

	LIST_INSERT_HEAD(&(ic->ic_proc_list), il, il_next);

	return il;
}

void
iic_smbus_intr_disestablish_proc(i2c_tag_t ic, void *hdl)
{
	struct ic_intr_list *il;

	il = (struct ic_intr_list *)hdl;

	LIST_REMOVE(il, il_next);
	free(il, M_DEVBUF);

	return;
}

int
iic_smbus_intr(i2c_tag_t ic)
{
	struct ic_intr_list *il;

	LIST_FOREACH(il, &(ic->ic_list), il_next) {
		(*il->il_intr)(il->il_intrarg);
	}

	ic->ic_pending++;
	wakeup(ic);

	return 1;
}

static void
iic_fill_compat(struct i2c_attach_args *ia, const char *compat, size_t len,
	char **buffer)
{
	int count, i;
	const char *c, *start, **ptr;

	*buffer = NULL;
	for (i = count = 0, c = compat; i < len; i++, c++)
		if (*c == 0)
			count++;
	count += 2;
	ptr = malloc(sizeof(char*)*count, M_TEMP, M_WAITOK);
	if (!ptr) return;

	for (i = count = 0, start = c = compat; i < len; i++, c++) {
		if (*c == 0) {
			ptr[count++] = start;
			start = c+1;
		}
	}
	if (start < compat+len) {
		/* last string not 0 terminated */
		size_t l = c-start;
		*buffer = malloc(l+1, M_TEMP, M_WAITOK);
		memcpy(*buffer, start, l);
		(*buffer)[l] = 0;
		ptr[count++] = *buffer;
	}
	ptr[count] = NULL;

	ia->ia_compat = ptr;
	ia->ia_ncompat = count;
}

int
iic_compat_match(struct i2c_attach_args *ia, const char ** compats)
{
	int i;

	for (; compats && *compats; compats++) {
		for (i = 0; i < ia->ia_ncompat; i++) {
			if (strcmp(*compats, ia->ia_compat[i]) == 0)
				return 1;
		}
	}
	return 0;
}

static int
iic_open(dev_t dev, int flag, int fmt, lwp_t *l)
{
	struct iic_softc *sc = device_lookup_private(&iic_cd, minor(dev));

	if (sc == NULL)
		return ENXIO;

	return 0;
}

static int
iic_close(dev_t dev, int flag, int fmt, lwp_t *l)
{
	return 0;
}

static int
iic_ioctl_exec(struct iic_softc *sc, i2c_ioctl_exec_t *iie, int flag)
{
	i2c_tag_t ic = sc->sc_tag;
	uint8_t buf[I2C_EXEC_MAX_BUFLEN];
	void *cmd = NULL;
	int error;

	/* Validate parameters */
	if (iie->iie_addr > I2C_MAX_ADDR)
		return EINVAL;
	if (iie->iie_cmdlen > I2C_EXEC_MAX_CMDLEN ||
	    iie->iie_buflen > I2C_EXEC_MAX_BUFLEN)
		return EINVAL;
	if (iie->iie_cmd != NULL && iie->iie_cmdlen == 0)
		return EINVAL;
	if (iie->iie_buf != NULL && iie->iie_buflen == 0)
		return EINVAL;
	if (I2C_OP_WRITE_P(iie->iie_op) && (flag & FWRITE) == 0)
		return EBADF;

#if 0
	/* Disallow userspace access to devices that have drivers attached. */
	if (sc->sc_devices[iie->iie_addr] != NULL)
		return EBUSY;
#endif

	if (iie->iie_cmd != NULL) {
		cmd = kmem_alloc(iie->iie_cmdlen, KM_SLEEP);
		if (cmd == NULL)
			return ENOMEM;
		error = copyin(iie->iie_cmd, cmd, iie->iie_cmdlen);
		if (error)
			goto out;
	}

	if (iie->iie_buf != NULL && I2C_OP_WRITE_P(iie->iie_op)) {
		error = copyin(iie->iie_buf, buf, iie->iie_buflen);
		if (error)
			goto out;
	}

	iic_acquire_bus(ic, 0);
	error = iic_exec(ic, iie->iie_op, iie->iie_addr, cmd, iie->iie_cmdlen,
	    buf, iie->iie_buflen, 0);
	iic_release_bus(ic, 0);

	/*
	 * Some drivers return error codes on failure, and others return -1.
	 */
	if (error < 0)
		error = EIO;

out:
	if (cmd)
		kmem_free(cmd, iie->iie_cmdlen);

	if (error)
		return error;

	if (iie->iie_buf != NULL && I2C_OP_READ_P(iie->iie_op))
		error = copyout(buf, iie->iie_buf, iie->iie_buflen);

	return error;
}

static int
iic_ioctl(dev_t dev, u_long cmd, void *data, int flag, lwp_t *l)
{
	struct iic_softc *sc = device_lookup_private(&iic_cd, minor(dev));

	if (sc == NULL)
		return ENXIO;

	switch (cmd) {
	case I2C_IOCTL_EXEC:
		return iic_ioctl_exec(sc, (i2c_ioctl_exec_t *)data, flag);
	default:
		return ENODEV;
	}
}


CFATTACH_DECL2_NEW(iic, sizeof(struct iic_softc),
    iic_match, iic_attach, iic_detach, NULL, iic_rescan, iic_child_detach);

MODULE(MODULE_CLASS_DRIVER, iic, "i2cexec");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
iic_modcmd(modcmd_t cmd, void *opaque)
{
	int error;

	error = 0;
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_iic,
		    cfattach_ioconf_iic, cfdata_ioconf_iic);
		if (error)
			aprint_error("%s: unable to init component\n",
			    iic_cd.cd_name);
#endif
		break;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		config_fini_component(cfdriver_ioconf_iic,
		    cfattach_ioconf_iic, cfdata_ioconf_iic);
#endif
		break;
	default:
		error = ENOTTY;
	}
	return error;
}
