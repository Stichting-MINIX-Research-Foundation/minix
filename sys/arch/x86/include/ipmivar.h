/* $NetBSD: ipmivar.h,v 1.11 2010/08/01 08:16:14 mlelstv Exp $ */

/*
 * Copyright (c) 2005 Jordan Hargrave
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/mutex.h>
#include <sys/condvar.h>

#include <dev/sysmon/sysmonvar.h>

#ifndef _IPMIVAR_H_
#define _IPMIVAR_H_

#define IPMI_IF_KCS		1
#define IPMI_IF_SMIC		2
#define IPMI_IF_BT		3

#define IPMI_IF_KCS_NREGS	2
#define IPMI_IF_SMIC_NREGS	3
#define IPMI_IF_BT_NREGS	3

struct ipmi_thread;
struct ipmi_softc;

struct ipmi_attach_args {
	bus_space_tag_t	iaa_iot;
	bus_space_tag_t	iaa_memt;

	int		iaa_if_type;
	int		iaa_if_rev;
	int		iaa_if_iotype;
	int		iaa_if_iobase;
	int		iaa_if_iospacing;
	int		iaa_if_irq;
	int		iaa_if_irqlvl;
};

struct ipmi_if {
	const char	*name;
	int		nregs;
	void		*(*buildmsg)(struct ipmi_softc *, int, int, int,
			    const void *, int *);
	int		(*sendmsg)(struct ipmi_softc *, int, const uint8_t *);
	int		(*recvmsg)(struct ipmi_softc *, int, int *, uint8_t *);
	int		(*reset)(struct ipmi_softc *);
	int		(*probe)(struct ipmi_softc *);
};

struct ipmi_softc {
	device_t		sc_dev;

	struct ipmi_if		*sc_if;		/* Interface layer */
	int			sc_if_iospacing; /* Spacing of I/O ports */
	int			sc_if_rev;	/* IPMI Revision */
	struct ipmi_attach_args	sc_ia;

	void			*sc_ih;		/* Interrupt/IO handles */
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	int			sc_btseq;

	struct lwp		*sc_kthread;

	int			sc_max_retries;

	kmutex_t		sc_poll_mtx;
	kcondvar_t		sc_poll_cv;

	kmutex_t		sc_cmd_mtx;
	kmutex_t		sc_sleep_mtx;
	kcondvar_t		sc_cmd_sleep;

	struct ipmi_bmc_args	*sc_iowait_args;

	struct ipmi_sensor	*current_sensor;
	volatile bool		sc_thread_running;
	volatile bool		sc_tickle_due;
	struct sysmon_wdog	sc_wdog;
	struct sysmon_envsys	*sc_envsys;
	envsys_data_t		*sc_sensor;
	int 		sc_nsensors; /* total number of sensors */

	char		sc_buf[64];
	bool		sc_buf_rsvd;
};

struct ipmi_thread {
	struct ipmi_softc   *sc;
	volatile int	    running;
};

#define IPMI_WDOG_USE_NOLOG		__BIT(7)
#define IPMI_WDOG_USE_NOSTOP		__BIT(6)
#define IPMI_WDOG_USE_RSVD1		__BITS(5, 3)
#define IPMI_WDOG_USE_USE_MASK		__BITS(2, 0)
#define IPMI_WDOG_USE_USE_RSVD		__SHIFTIN(0, IPMI_WDOG_USE_USE_MASK);
#define IPMI_WDOG_USE_USE_FRB2		__SHIFTIN(1, IPMI_WDOG_USE_USE_MASK);
#define IPMI_WDOG_USE_USE_POST		__SHIFTIN(2, IPMI_WDOG_USE_USE_MASK);
#define IPMI_WDOG_USE_USE_OSLOAD	__SHIFTIN(3, IPMI_WDOG_USE_USE_MASK);
#define IPMI_WDOG_USE_USE_OS		__SHIFTIN(4, IPMI_WDOG_USE_USE_MASK);
#define IPMI_WDOG_USE_USE_OEM		__SHIFTIN(5, IPMI_WDOG_USE_USE_MASK);

#define IPMI_WDOG_ACT_PRE_RSVD1		__BIT(7)
#define IPMI_WDOG_ACT_PRE_MASK		__BITS(6, 4)
#define IPMI_WDOG_ACT_PRE_DISABLED	__SHIFTIN(0, IPMI_WDOG_ACT_MASK)
#define IPMI_WDOG_ACT_PRE_SMI		__SHIFTIN(1, IPMI_WDOG_ACT_MASK)
#define IPMI_WDOG_ACT_PRE_NMI		__SHIFTIN(2, IPMI_WDOG_ACT_MASK)
#define IPMI_WDOG_ACT_PRE_INTERRUPT	__SHIFTIN(3, IPMI_WDOG_ACT_MASK)
#define IPMI_WDOG_ACT_PRE_RSVD0		__BIT(3)
#define IPMI_WDOG_ACT_MASK		__BITS(2, 0)
#define IPMI_WDOG_ACT_DISABLED		__SHIFTIN(0, IPMI_WDOG_ACT_MASK)
#define IPMI_WDOG_ACT_RESET		__SHIFTIN(1, IPMI_WDOG_ACT_MASK)
#define IPMI_WDOG_ACT_PWROFF		__SHIFTIN(2, IPMI_WDOG_ACT_MASK)
#define IPMI_WDOG_ACT_PWRCYCLE		__SHIFTIN(3, IPMI_WDOG_ACT_MASK)

#define IPMI_WDOG_FLAGS_RSVD1		__BITS(7, 6)
#define IPMI_WDOG_FLAGS_OEM		__BIT(5)
#define IPMI_WDOG_FLAGS_OS		__BIT(4)
#define IPMI_WDOG_FLAGS_OSLOAD		__BIT(3)
#define IPMI_WDOG_FLAGS_POST		__BIT(2)
#define IPMI_WDOG_FLAGS_FRB2		__BIT(1)
#define IPMI_WDOG_FLAGS_RSVD0		__BIT(0)

struct ipmi_set_watchdog {
	uint8_t		wdog_use;
	uint8_t		wdog_action;
	uint8_t		wdog_pretimeout;
	uint8_t		wdog_flags;
	uint16_t		wdog_timeout;
} __packed;

struct ipmi_get_watchdog {
	uint8_t		wdog_use;
	uint8_t		wdog_action;
	uint8_t		wdog_pretimeout;
	uint8_t		wdog_flags;
	uint16_t		wdog_timeout;
	uint16_t		wdog_countdown;
} __packed;

void	ipmi_poll_thread(void *);

int	kcs_probe(struct ipmi_softc *);
int	kcs_reset(struct ipmi_softc *);
int	kcs_sendmsg(struct ipmi_softc *, int, const uint8_t *);
int	kcs_recvmsg(struct ipmi_softc *, int, int *len, uint8_t *);

int	bt_probe(struct ipmi_softc *);
int	bt_reset(struct ipmi_softc *);
int	bt_sendmsg(struct ipmi_softc *, int, const uint8_t *);
int	bt_recvmsg(struct ipmi_softc *, int, int *, uint8_t *);

int	smic_probe(struct ipmi_softc *);
int	smic_reset(struct ipmi_softc *);
int	smic_sendmsg(struct ipmi_softc *, int, const uint8_t *);
int	smic_recvmsg(struct ipmi_softc *, int, int *, uint8_t *);

struct dmd_ipmi {
	uint8_t	dmd_sig[4];		/* Signature 'IPMI' */
	uint8_t	dmd_i2c_address;	/* Address of BMC */
	uint8_t	dmd_nvram_address;	/* Address of NVRAM */
	uint8_t	dmd_if_type;		/* IPMI Interface Type */
	uint8_t	dmd_if_rev;		/* IPMI Interface Revision */
} __packed;


#define APP_NETFN			0x06
#define APP_GET_DEVICE_ID		0x01
#define APP_RESET_WATCHDOG		0x22
#define APP_SET_WATCHDOG_TIMER		0x24
#define APP_GET_WATCHDOG_TIMER		0x25

#define TRANSPORT_NETFN			0xC
#define BRIDGE_NETFN			0x2

#define STORAGE_NETFN			0x0A
#define STORAGE_GET_FRU_INV_AREA	0x10
#define STORAGE_READ_FRU_DATA		0x11
#define STORAGE_RESERVE_SDR		0x22
#define STORAGE_GET_SDR			0x23
#define STORAGE_ADD_SDR			0x24
#define STORAGE_ADD_PARTIAL_SDR		0x25
#define STORAGE_DELETE_SDR		0x26
#define STORAGE_RESERVE_SEL		0x42
#define STORAGE_GET_SEL			0x43
#define STORAGE_ADD_SEL			0x44
#define STORAGE_ADD_PARTIAL_SEL		0x45
#define STORAGE_DELETE_SEL		0x46

#define SE_NETFN			0x04
#define SE_GET_SDR_INFO			0x20
#define SE_GET_SDR			0x21
#define SE_RESERVE_SDR			0x22
#define SE_GET_SENSOR_FACTOR		0x23
#define SE_SET_SENSOR_HYSTERESIS	0x24
#define SE_GET_SENSOR_HYSTERESIS	0x25
#define SE_SET_SENSOR_THRESHOLD		0x26
#define SE_GET_SENSOR_THRESHOLD		0x27
#define SE_SET_SENSOR_EVENT_ENABLE	0x28
#define SE_GET_SENSOR_EVENT_ENABLE	0x29
#define SE_REARM_SENSOR_EVENTS		0x2A
#define SE_GET_SENSOR_EVENT_STATUS	0x2B
#define SE_GET_SENSOR_READING		0x2D
#define SE_SET_SENSOR_TYPE		0x2E
#define SE_GET_SENSOR_TYPE		0x2F

struct sdrhdr {
	uint16_t	record_id;		/* SDR Record ID */
	uint8_t	sdr_version;		/* SDR Version */
	uint8_t	record_type;		/* SDR Record Type */
	uint8_t	record_length;		/* SDR Record Length */
} __packed;

/* SDR: Record Type 1 */
struct sdrtype1 {
	struct sdrhdr	sdrhdr;

	uint8_t	owner_id;
	uint8_t	owner_lun;
	uint8_t	sensor_num;

	uint8_t	entity_id;
	uint8_t	entity_instance;
	uint8_t	sensor_init;
	uint8_t	sensor_caps;
	uint8_t	sensor_type;
	uint8_t	event_code;
	uint16_t	trigger_mask;
	uint16_t	reading_mask;
	uint16_t	settable_mask;
	uint8_t	units1;
	uint8_t	units2;
	uint8_t	units3;
	uint8_t	linear;
	uint8_t	m;
	uint8_t	m_tolerance;
	uint8_t	b;
	uint8_t	b_accuracy;
	uint8_t	accuracyexp;
	uint8_t	rbexp;
	uint8_t	analogchars;
	uint8_t	nominalreading;
	uint8_t	normalmax;
	uint8_t	normalmin;
	uint8_t	sensormax;
	uint8_t	sensormin;
	uint8_t	uppernr;
	uint8_t	upperc;
	uint8_t	uppernc;
	uint8_t	lowernr;
	uint8_t	lowerc;
	uint8_t	lowernc;
	uint8_t	physt;
	uint8_t	nhyst;
	uint8_t	resvd[2];
	uint8_t	oem;
	uint8_t	typelen;
	uint8_t	name[1];
} __packed;

/* SDR: Record Type 2 */
struct sdrtype2 {
	struct sdrhdr	sdrhdr;

	uint8_t	owner_id;
	uint8_t	owner_lun;
	uint8_t	sensor_num;

	uint8_t	entity_id;
	uint8_t	entity_instance;
	uint8_t	sensor_init;
	uint8_t	sensor_caps;
	uint8_t	sensor_type;
	uint8_t	event_code;
	uint16_t	trigger_mask;
	uint16_t	reading_mask;
	uint16_t	set_mask;
	uint8_t	units1;
	uint8_t	units2;
	uint8_t	units3;
	uint8_t	share1;
	uint8_t	share2;
	uint8_t	physt;
	uint8_t	nhyst;
	uint8_t	resvd[3];
	uint8_t	oem;
	uint8_t	typelen;
	uint8_t	name[1];
} __packed;

int ipmi_probe(struct ipmi_attach_args *);

#endif				/* _IPMIVAR_H_ */
