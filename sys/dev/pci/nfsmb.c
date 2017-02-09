/*	$NetBSD: nfsmb.c,v 1.23 2012/02/14 15:08:07 pgoyette Exp $	*/
/*
 * Copyright (c) 2007 KIYOHARA Takashi
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfsmb.c,v 1.23 2012/02/14 15:08:07 pgoyette Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <sys/bus.h>

#include <dev/i2c/i2cvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/nfsmbreg.h>


struct nfsmbc_attach_args {
	int nfsmb_num;
	bus_space_tag_t nfsmb_iot;
	int nfsmb_addr;
};

struct nfsmb_softc;
struct nfsmbc_softc {
	device_t sc_dev;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_tag;
	struct pci_attach_args *sc_pa;

	bus_space_tag_t sc_iot;
	device_t sc_nfsmb[2];
};

struct nfsmb_softc {
	device_t sc_dev;
	int sc_num;
	device_t sc_nfsmbc;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct i2c_controller sc_i2c;	/* i2c controller info */
	kmutex_t sc_mutex;
};


static int nfsmbc_match(device_t, cfdata_t, void *);
static void nfsmbc_attach(device_t, device_t, void *);
static int nfsmbc_print(void *, const char *);

static int nfsmb_match(device_t, cfdata_t, void *);
static void nfsmb_attach(device_t, device_t, void *);
static int nfsmb_acquire_bus(void *, int);
static void nfsmb_release_bus(void *, int);
static int nfsmb_exec(
    void *, i2c_op_t, i2c_addr_t, const void *, size_t, void *, size_t, int);
static int nfsmb_check_done(struct nfsmb_softc *);
static int
    nfsmb_send_1(struct nfsmb_softc *, uint8_t, i2c_addr_t, i2c_op_t, int);
static int nfsmb_write_1(
    struct nfsmb_softc *, uint8_t, uint8_t, i2c_addr_t, i2c_op_t, int);
static int nfsmb_write_2(
    struct nfsmb_softc *, uint8_t, uint16_t, i2c_addr_t, i2c_op_t, int);
static int nfsmb_receive_1(struct nfsmb_softc *, i2c_addr_t, i2c_op_t, int);
static int
    nfsmb_read_1(struct nfsmb_softc *, uint8_t, i2c_addr_t, i2c_op_t, int);
static int
    nfsmb_read_2(struct nfsmb_softc *, uint8_t, i2c_addr_t, i2c_op_t, int);
static int 
    nfsmb_quick(struct nfsmb_softc *, i2c_addr_t, i2c_op_t, int);

CFATTACH_DECL_NEW(nfsmbc, sizeof(struct nfsmbc_softc),
    nfsmbc_match, nfsmbc_attach, NULL, NULL);

static int
nfsmbc_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NVIDIA) {
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_NVIDIA_NFORCE2_SMBUS:
		case PCI_PRODUCT_NVIDIA_NFORCE2_400_SMBUS:
		case PCI_PRODUCT_NVIDIA_NFORCE3_SMBUS:
		case PCI_PRODUCT_NVIDIA_NFORCE3_250_SMBUS:
		case PCI_PRODUCT_NVIDIA_NFORCE4_SMBUS:
		case PCI_PRODUCT_NVIDIA_NFORCE430_SMBUS:
		case PCI_PRODUCT_NVIDIA_MCP04_SMBUS:
		case PCI_PRODUCT_NVIDIA_MCP55_SMB:
		case PCI_PRODUCT_NVIDIA_MCP61_SMB:
		case PCI_PRODUCT_NVIDIA_MCP65_SMB:
		case PCI_PRODUCT_NVIDIA_MCP67_SMB:
		case PCI_PRODUCT_NVIDIA_MCP73_SMB:
		case PCI_PRODUCT_NVIDIA_MCP78S_SMB:
		case PCI_PRODUCT_NVIDIA_MCP79_SMB:
			return 1;
		}
	}

	return 0;
}

static void
nfsmbc_attach(device_t parent, device_t self, void *aux)
{
	struct nfsmbc_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct nfsmbc_attach_args nfsmbca;
	pcireg_t reg;
	int baseregs[2];

	pci_aprint_devinfo(pa, NULL);

	sc->sc_dev = self;
	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_pa = pa;
	sc->sc_iot = pa->pa_iot;

	nfsmbca.nfsmb_iot = sc->sc_iot;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_NVIDIA_NFORCE2_SMBUS:
	case PCI_PRODUCT_NVIDIA_NFORCE2_400_SMBUS:
	case PCI_PRODUCT_NVIDIA_NFORCE3_SMBUS:
	case PCI_PRODUCT_NVIDIA_NFORCE3_250_SMBUS:
	case PCI_PRODUCT_NVIDIA_NFORCE4_SMBUS:
		baseregs[0] = NFORCE_OLD_SMB1;
		baseregs[1] = NFORCE_OLD_SMB2;
		break;
	default:
		baseregs[0] = NFORCE_SMB1;
		baseregs[1] = NFORCE_SMB2;
		break;
	}

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, baseregs[0]);
	nfsmbca.nfsmb_num = 1;
	nfsmbca.nfsmb_addr = NFORCE_SMBBASE(reg);
	sc->sc_nfsmb[0] = config_found(sc->sc_dev, &nfsmbca, nfsmbc_print);

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, baseregs[1]);
	nfsmbca.nfsmb_num = 2;
	nfsmbca.nfsmb_addr = NFORCE_SMBBASE(reg);
	sc->sc_nfsmb[1] = config_found(sc->sc_dev, &nfsmbca, nfsmbc_print);

	/* This driver is similar to an ISA bridge that doesn't
	 * need any special handling. So registering NULL handlers
	 * are sufficent. */
	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
nfsmbc_print(void *aux, const char *pnp)
{
	struct nfsmbc_attach_args *nfsmbcap = aux;

	if (pnp)
		aprint_normal("nfsmb SMBus %d at %s",
		    nfsmbcap->nfsmb_num, pnp);
	else
		aprint_normal(" SMBus %d", nfsmbcap->nfsmb_num);
	return UNCONF;
}


CFATTACH_DECL_NEW(nfsmb, sizeof(struct nfsmb_softc),
    nfsmb_match, nfsmb_attach, NULL, NULL);

static int
nfsmb_match(device_t parent, cfdata_t match, void *aux)
{
	struct nfsmbc_attach_args *nfsmbcap = aux;

	if (nfsmbcap->nfsmb_num == 1 || nfsmbcap->nfsmb_num == 2)
		return 1;
	return 0;
}

static void
nfsmb_attach(device_t parent, device_t self, void *aux)
{
	struct nfsmb_softc *sc = device_private(self);
	struct nfsmbc_attach_args *nfsmbcap = aux;
	struct i2cbus_attach_args iba;

	aprint_naive("\n");
	aprint_normal("\n");

	sc->sc_dev = self;
	sc->sc_nfsmbc = parent;
	sc->sc_num = nfsmbcap->nfsmb_num;
	sc->sc_iot = nfsmbcap->nfsmb_iot;

	/* register with iic */
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = nfsmb_acquire_bus;
	sc->sc_i2c.ic_release_bus = nfsmb_release_bus;
	sc->sc_i2c.ic_send_start = NULL;
	sc->sc_i2c.ic_send_stop = NULL;
	sc->sc_i2c.ic_initiate_xfer = NULL;
	sc->sc_i2c.ic_read_byte = NULL;
	sc->sc_i2c.ic_write_byte = NULL;
	sc->sc_i2c.ic_exec = nfsmb_exec;

	mutex_init(&sc->sc_mutex, MUTEX_DEFAULT, IPL_NONE);

	if (bus_space_map(sc->sc_iot, nfsmbcap->nfsmb_addr, NFORCE_SMBSIZE, 0,
	    &sc->sc_ioh) != 0) {
		aprint_error_dev(self, "failed to map SMBus space\n");
		return;
	}

	iba.iba_type = I2C_TYPE_SMBUS;
	iba.iba_tag = &sc->sc_i2c;
	(void) config_found_ia(sc->sc_dev, "i2cbus", &iba, iicbus_print);

	/* This driver is similar to an ISA bridge that doesn't
	 * need any special handling. So registering NULL handlers
	 * are sufficent. */
	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
nfsmb_acquire_bus(void *cookie, int flags)
{
	struct nfsmb_softc *sc = cookie;

	mutex_enter(&sc->sc_mutex);
	return 0;
}

static void
nfsmb_release_bus(void *cookie, int flags)
{
	struct nfsmb_softc *sc = cookie;

	mutex_exit(&sc->sc_mutex);
}

static int
nfsmb_exec(void *cookie, i2c_op_t op, i2c_addr_t addr, const void *cmd,
	   size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
	struct nfsmb_softc *sc  = (struct nfsmb_softc *)cookie;
	uint8_t *p = vbuf;
	int rv;

	if ((cmdlen == 0) && (buflen == 0)) {
		return nfsmb_quick(sc, addr, op, flags);
	}

	if (I2C_OP_READ_P(op) && (cmdlen == 0) && (buflen == 1)) {
		rv = nfsmb_receive_1(sc, addr, op, flags);
		if (rv == -1)
			return -1;
		*p = (uint8_t)rv;
		return 0;
	}

	if ((I2C_OP_READ_P(op)) && (cmdlen == 1) && (buflen == 1)) {
		rv = nfsmb_read_1(sc, *(const uint8_t*)cmd, addr, op, flags);
		if (rv == -1)
			return -1;
		*p = (uint8_t)rv;
		return 0;
	}

	if ((I2C_OP_READ_P(op)) && (cmdlen == 1) && (buflen == 2)) {
		rv = nfsmb_read_2(sc, *(const uint8_t*)cmd, addr, op, flags);
		if (rv == -1)
			return -1;
		*(uint16_t *)p = (uint16_t)rv;
		return 0;
	}

	if ((I2C_OP_WRITE_P(op)) && (cmdlen == 0) && (buflen == 1))
		return nfsmb_send_1(sc, *(uint8_t*)vbuf, addr, op, flags);

	if ((I2C_OP_WRITE_P(op)) && (cmdlen == 1) && (buflen == 1))
		return nfsmb_write_1(sc, *(const uint8_t*)cmd, *(uint8_t*)vbuf,
		    addr, op, flags);

	if ((I2C_OP_WRITE_P(op)) && (cmdlen == 1) && (buflen == 2))
		return nfsmb_write_2(sc,
		    *(const uint8_t*)cmd, *((uint16_t *)vbuf), addr, op, flags);

	return -1;
}

static int
nfsmb_check_done(struct nfsmb_softc *sc)
{
	int us;
	uint8_t stat;

	us = 10 * 1000;	/* XXXX: wait maximum 10 msec */
	do {
		delay(10);
		us -= 10;
		if (us <= 0)
			return -1;
	} while (bus_space_read_1(sc->sc_iot, sc->sc_ioh,
	    NFORCE_SMB_PROTOCOL) != 0);

	stat = bus_space_read_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_STATUS);
	if ((stat & NFORCE_SMB_STATUS_DONE) &&
	    !(stat & NFORCE_SMB_STATUS_STATUS))
		return 0;
	return -1;
}

/* ARGSUSED */
static int
nfsmb_quick(struct nfsmb_softc *sc, i2c_addr_t addr, i2c_op_t op, int flags)
{
	uint8_t data;

	/* write smbus slave address to register */
	data = addr << 1;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_ADDRESS, data);

	/* write smbus protocol to register */
	data = I2C_OP_READ_P(op) | NFORCE_SMB_PROTOCOL_QUICK;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_PROTOCOL, data);

	return nfsmb_check_done(sc);
}

/* ARGSUSED */
static int
nfsmb_send_1(struct nfsmb_softc *sc, uint8_t val, i2c_addr_t addr, i2c_op_t op,
	     int flags)
{
	uint8_t data;

	/* store cmd */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_COMMAND, val);

	/* write smbus slave address to register */
	data = addr << 1;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_ADDRESS, data);

	/* write smbus protocol to register */
	data = I2C_OP_READ_P(op) | NFORCE_SMB_PROTOCOL_BYTE;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_PROTOCOL, data);

	return nfsmb_check_done(sc);
}

/* ARGSUSED */
static int
nfsmb_write_1(struct nfsmb_softc *sc, uint8_t cmd, uint8_t val, i2c_addr_t addr,
	      i2c_op_t op, int flags)
{
	uint8_t data;

	/* store cmd */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_COMMAND, cmd);

	/* store data */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_DATA, val);

	/* write smbus slave address to register */
	data = addr << 1;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_ADDRESS, data);

	/* write smbus protocol to register */
	data = I2C_OP_READ_P(op) | NFORCE_SMB_PROTOCOL_BYTE_DATA;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_PROTOCOL, data);

	return nfsmb_check_done(sc);
}

static int
nfsmb_write_2(struct nfsmb_softc *sc, uint8_t cmd, uint16_t val,
	      i2c_addr_t addr, i2c_op_t op, int flags)
{
	uint8_t data, low, high;

	/* store cmd */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_COMMAND, cmd);

	/* store data */
	low = val;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_DATA, low);
	high = val >> 8;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_DATA + 1, high);

	/* write smbus slave address to register */
	data = addr << 1;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_ADDRESS, data);

	/* write smbus protocol to register */
	data = I2C_OP_READ_P(op) | NFORCE_SMB_PROTOCOL_WORD_DATA;
	if (flags & I2C_F_PEC)
		data |= NFORCE_SMB_PROTOCOL_PEC;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_PROTOCOL, data);

	return nfsmb_check_done(sc);
}

/* ARGSUSED */
static int
nfsmb_receive_1(struct nfsmb_softc *sc, i2c_addr_t addr, i2c_op_t op, int flags)
{
	uint8_t data;

	/* write smbus slave address to register */
	data = addr << 1;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_ADDRESS, data);

	/* write smbus protocol to register */
	data = I2C_OP_READ_P(op) | NFORCE_SMB_PROTOCOL_BYTE;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_PROTOCOL, data);

	/* check for errors */
	if (nfsmb_check_done(sc) < 0)
		return -1;

	/* read data */
	return bus_space_read_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_DATA);
}

/* ARGSUSED */
static int
nfsmb_read_1(struct nfsmb_softc *sc, uint8_t cmd, i2c_addr_t addr, i2c_op_t op,
	     int flags)
{
	uint8_t data;

	/* store cmd */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_COMMAND, cmd);

	/* write smbus slave address to register */
	data = addr << 1;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_ADDRESS, data);

	/* write smbus protocol to register */
	data = I2C_OP_READ_P(op) | NFORCE_SMB_PROTOCOL_BYTE_DATA;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_PROTOCOL, data);

	/* check for errors */
	if (nfsmb_check_done(sc) < 0)
		return -1;

	/* read data */
	return bus_space_read_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_DATA);
}

static int
nfsmb_read_2(struct nfsmb_softc *sc, uint8_t cmd, i2c_addr_t addr, i2c_op_t op,
	     int flags)
{
	uint8_t data, low, high;

	/* store cmd */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_COMMAND, cmd);

	/* write smbus slave address to register */
	data = addr << 1;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_ADDRESS, data);

	/* write smbus protocol to register */
	data = I2C_OP_READ_P(op) | NFORCE_SMB_PROTOCOL_WORD_DATA;
	if (flags & I2C_F_PEC)
		data |= NFORCE_SMB_PROTOCOL_PEC;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_PROTOCOL, data);

	/* check for errors */
	if (nfsmb_check_done(sc) < 0)
		return -1;

	/* read data */
	low = bus_space_read_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_DATA);
	high = bus_space_read_1(sc->sc_iot, sc->sc_ioh, NFORCE_SMB_DATA + 1);
	return low | high << 8;
}
