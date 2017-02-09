/*	$NetBSD: sysmon_envsys.c,v 1.137 2015/04/25 23:40:09 pgoyette Exp $	*/

/*-
 * Copyright (c) 2007, 2008 Juan Romero Pardines.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2000 Zembu Labs, Inc.
 * All rights reserved.
 *
 * Author: Jason R. Thorpe <thorpej@zembu.com>
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
 *	This product includes software developed by Zembu Labs, Inc.
 * 4. Neither the name of Zembu Labs nor the names of its employees may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ZEMBU LABS, INC. ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WAR-
 * RANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DIS-
 * CLAIMED.  IN NO EVENT SHALL ZEMBU LABS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Environmental sensor framework for sysmon, exported to userland
 * with proplib(3).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysmon_envsys.c,v 1.137 2015/04/25 23:40:09 pgoyette Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/kmem.h>
#include <sys/rndsource.h>
#include <sys/module.h>
#include <sys/once.h>

#include <dev/sysmon/sysmonvar.h>
#include <dev/sysmon/sysmon_envsysvar.h>
#include <dev/sysmon/sysmon_taskq.h>

kmutex_t sme_global_mtx;

prop_dictionary_t sme_propd;

struct sysmon_envsys_lh sysmon_envsys_list;

static uint32_t sysmon_envsys_next_sensor_index;
static struct sysmon_envsys *sysmon_envsys_find_40(u_int);

static void sysmon_envsys_destroy_plist(prop_array_t);
static void sme_remove_userprops(void);
static int sme_add_property_dictionary(struct sysmon_envsys *, prop_array_t,
				       prop_dictionary_t);
static sme_event_drv_t * sme_add_sensor_dictionary(struct sysmon_envsys *,
	prop_array_t, prop_dictionary_t, envsys_data_t *);
static void sme_initial_refresh(void *);
static uint32_t sme_get_max_value(struct sysmon_envsys *,
     bool (*)(const envsys_data_t*), bool);

MODULE(MODULE_CLASS_MISC, sysmon_envsys, "sysmon,sysmon_taskq,sysmon_power");

static struct sysmon_opvec sysmon_envsys_opvec = {    
        sysmonopen_envsys, sysmonclose_envsys, sysmonioctl_envsys,
        NULL, NULL, NULL
};

ONCE_DECL(once_envsys);

static int
sme_preinit(void)
{

	LIST_INIT(&sysmon_envsys_list);
	mutex_init(&sme_global_mtx, MUTEX_DEFAULT, IPL_NONE);
	sme_propd = prop_dictionary_create();

	return 0;
}

/*
 * sysmon_envsys_init:
 *
 * 	+ Initialize global mutex, dictionary and the linked list.
 */
int
sysmon_envsys_init(void)
{
	int error;

	(void)RUN_ONCE(&once_envsys, sme_preinit);

	error = sysmon_attach_minor(SYSMON_MINOR_ENVSYS, &sysmon_envsys_opvec);

	return error;
}

int
sysmon_envsys_fini(void)
{
	int error;

	if ( ! LIST_EMPTY(&sysmon_envsys_list))
		error = EBUSY;
	else
		error = sysmon_attach_minor(SYSMON_MINOR_ENVSYS, NULL);

	if (error == 0)
		mutex_destroy(&sme_global_mtx);

	// XXX: prop_dictionary ???

	return error;
}

/*
 * sysmonopen_envsys:
 *
 *	+ Open the system monitor device.
 */
int
sysmonopen_envsys(dev_t dev, int flag, int mode, struct lwp *l)
{
	return 0;
}

/*
 * sysmonclose_envsys:
 *
 *	+ Close the system monitor device.
 */
int
sysmonclose_envsys(dev_t dev, int flag, int mode, struct lwp *l)
{
	return 0;
}

/*
 * sysmonioctl_envsys:
 *
 *	+ Perform a sysmon envsys control request.
 */
int
sysmonioctl_envsys(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct sysmon_envsys *sme = NULL;
	int error = 0;
	u_int oidx;

	switch (cmd) {
	/*
	 * To update the global dictionary with latest data from devices.
	 */
	case ENVSYS_GETDICTIONARY:
	    {
		struct plistref *plist = (struct plistref *)data;

		/*
		 * Update dictionaries on all sysmon envsys devices
		 * registered.
		 */		
		mutex_enter(&sme_global_mtx);
		LIST_FOREACH(sme, &sysmon_envsys_list, sme_list) {
			sysmon_envsys_acquire(sme, false);
			error = sme_update_dictionary(sme);
			if (error) {
				DPRINTF(("%s: sme_update_dictionary, "
				    "error=%d\n", __func__, error));
				sysmon_envsys_release(sme, false);
				mutex_exit(&sme_global_mtx);
				return error;
			}
			sysmon_envsys_release(sme, false);
		}
		mutex_exit(&sme_global_mtx);
		/*
		 * Copy global dictionary to userland.
		 */
		error = prop_dictionary_copyout_ioctl(plist, cmd, sme_propd);
		break;
	    }
	/*
	 * To set properties on multiple devices.
	 */
	case ENVSYS_SETDICTIONARY:
	    {
		const struct plistref *plist = (const struct plistref *)data;
		prop_dictionary_t udict;
		prop_object_iterator_t iter, iter2;
		prop_object_t obj, obj2;
		prop_array_t array_u, array_k;
		const char *devname = NULL;

		if ((flag & FWRITE) == 0)
			return EPERM;

		/*
		 * Get dictionary from userland.
		 */
		error = prop_dictionary_copyin_ioctl(plist, cmd, &udict);
		if (error) {
			DPRINTF(("%s: copyin_ioctl error=%d\n",
			    __func__, error));
			break;
		}

		iter = prop_dictionary_iterator(udict);
		if (!iter) {
			prop_object_release(udict);
			return ENOMEM;
		}

		/*
		 * Iterate over the userland dictionary and process
		 * the list of devices.
		 */
		while ((obj = prop_object_iterator_next(iter))) {
			array_u = prop_dictionary_get_keysym(udict, obj);
			if (prop_object_type(array_u) != PROP_TYPE_ARRAY) {
				prop_object_iterator_release(iter);
				prop_object_release(udict);
				return EINVAL;
			}
			
			devname = prop_dictionary_keysym_cstring_nocopy(obj);
			DPRINTF(("%s: processing the '%s' array requests\n",
			    __func__, devname));

			/*
			 * find the correct sme device.
			 */
			sme = sysmon_envsys_find(devname);
			if (!sme) {
				DPRINTF(("%s: NULL sme\n", __func__));
				prop_object_iterator_release(iter);
				prop_object_release(udict);
				return EINVAL;
			}

			/*
			 * Find the correct array object with the string
			 * supplied by the userland dictionary.
			 */
			array_k = prop_dictionary_get(sme_propd, devname);
			if (prop_object_type(array_k) != PROP_TYPE_ARRAY) {
				DPRINTF(("%s: array device failed\n",
				    __func__));
				sysmon_envsys_release(sme, false);
				prop_object_iterator_release(iter);
				prop_object_release(udict);
				return EINVAL;
			}

			iter2 = prop_array_iterator(array_u);
			if (!iter2) {
				sysmon_envsys_release(sme, false);
				prop_object_iterator_release(iter);
				prop_object_release(udict);
				return ENOMEM;
			}

			/*
			 * Iterate over the array of dictionaries to
			 * process the list of sensors and properties.
			 */
			while ((obj2 = prop_object_iterator_next(iter2))) {
				/* 
				 * do the real work now.
				 */
				error = sme_userset_dictionary(sme,
							       obj2,
							       array_k);
				if (error) {
					sysmon_envsys_release(sme, false);
					prop_object_iterator_release(iter2);
					prop_object_iterator_release(iter);
					prop_object_release(udict);
					return error;
				}
			}

			sysmon_envsys_release(sme, false);
			prop_object_iterator_release(iter2);
		}

		prop_object_iterator_release(iter);
		prop_object_release(udict);
		break;
	    }
	/*
	 * To remove all properties from all devices registered.
	 */
	case ENVSYS_REMOVEPROPS:
	    {
		const struct plistref *plist = (const struct plistref *)data;
		prop_dictionary_t udict;
		prop_object_t obj;

		if ((flag & FWRITE) == 0)
			return EPERM;

		error = prop_dictionary_copyin_ioctl(plist, cmd, &udict);
		if (error) {
			DPRINTF(("%s: copyin_ioctl error=%d\n",
			    __func__, error));
			break;
		}

		obj = prop_dictionary_get(udict, "envsys-remove-props");
		if (!obj || !prop_bool_true(obj)) {
			DPRINTF(("%s: invalid 'envsys-remove-props'\n",
			     __func__));
			return EINVAL;
		}

		prop_object_release(udict);
		sme_remove_userprops();

		break;
	    }
	/*
	 * Compatibility ioctls with the old interface, only implemented
	 * ENVSYS_GTREDATA and ENVSYS_GTREINFO; enough to make old
	 * applications work.
	 */
	case ENVSYS_GTREDATA:
	    {
		struct envsys_tre_data *tred = (void *)data;
		envsys_data_t *edata = NULL;
		bool found = false;

		tred->validflags = 0;

		sme = sysmon_envsys_find_40(tred->sensor);
		if (!sme)
			break;

		oidx = tred->sensor;
		tred->sensor = SME_SENSOR_IDX(sme, tred->sensor);

		DPRINTFOBJ(("%s: sensor=%d oidx=%d dev=%s nsensors=%d\n",
		    __func__, tred->sensor, oidx, sme->sme_name,
		    sme->sme_nsensors));

		TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
			if (edata->sensor == tred->sensor) {
				found = true;
				break;
			}
		}

		if (!found) {
			sysmon_envsys_release(sme, false);
			error = ENODEV;
			break;
		}

		if (tred->sensor < sme->sme_nsensors) {
			if ((sme->sme_flags & SME_POLL_ONLY) == 0) {
				mutex_enter(&sme->sme_mtx);
				sysmon_envsys_refresh_sensor(sme, edata);
				mutex_exit(&sme->sme_mtx);
			}

			/* 
			 * copy required values to the old interface.
			 */
			tred->sensor = edata->sensor;
			tred->cur.data_us = edata->value_cur;
			tred->cur.data_s = edata->value_cur;
			tred->max.data_us = edata->value_max;
			tred->max.data_s = edata->value_max;
			tred->min.data_us = edata->value_min;
			tred->min.data_s = edata->value_min;
			tred->avg.data_us = 0;
			tred->avg.data_s = 0;
			if (edata->units == ENVSYS_BATTERY_CHARGE)
				tred->units = ENVSYS_INDICATOR;
			else
				tred->units = edata->units;

			tred->validflags |= ENVSYS_FVALID;
			tred->validflags |= ENVSYS_FCURVALID;

			if (edata->flags & ENVSYS_FPERCENT) {
				tred->validflags |= ENVSYS_FMAXVALID;
				tred->validflags |= ENVSYS_FFRACVALID;
			}

			if (edata->state == ENVSYS_SINVALID) {
				tred->validflags &= ~ENVSYS_FCURVALID;
				tred->cur.data_us = tred->cur.data_s = 0;
			}

			DPRINTFOBJ(("%s: sensor=%s tred->cur.data_s=%d\n",
			    __func__, edata->desc, tred->cur.data_s));
			DPRINTFOBJ(("%s: tred->validflags=%d tred->units=%d"
			    " tred->sensor=%d\n", __func__, tred->validflags,
			    tred->units, tred->sensor));
		}
		tred->sensor = oidx;
		sysmon_envsys_release(sme, false);

		break;
	    }
	case ENVSYS_GTREINFO:
	    {
		struct envsys_basic_info *binfo = (void *)data;
		envsys_data_t *edata = NULL;
		bool found = false;

		binfo->validflags = 0;

		sme = sysmon_envsys_find_40(binfo->sensor);
		if (!sme)
			break;

		oidx = binfo->sensor;
		binfo->sensor = SME_SENSOR_IDX(sme, binfo->sensor);

		TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
			if (edata->sensor == binfo->sensor) {
				found = true;
				break;
			}
		}

		if (!found) {
			sysmon_envsys_release(sme, false);
			error = ENODEV;
			break;
		}

		binfo->validflags |= ENVSYS_FVALID;

		if (binfo->sensor < sme->sme_nsensors) {
			if (edata->units == ENVSYS_BATTERY_CHARGE)
				binfo->units = ENVSYS_INDICATOR;
			else
				binfo->units = edata->units;

			/*
			 * previously, the ACPI sensor names included the
			 * device name. Include that in compatibility code.
			 */
			if (strncmp(sme->sme_name, "acpi", 4) == 0)
				(void)snprintf(binfo->desc, sizeof(binfo->desc),
				    "%s %s", sme->sme_name, edata->desc);
			else
				(void)strlcpy(binfo->desc, edata->desc,
				    sizeof(binfo->desc));
		}

		DPRINTFOBJ(("%s: binfo->units=%d binfo->validflags=%d\n",
		    __func__, binfo->units, binfo->validflags));
		DPRINTFOBJ(("%s: binfo->desc=%s binfo->sensor=%d\n",
		    __func__, binfo->desc, binfo->sensor));

		binfo->sensor = oidx;
		sysmon_envsys_release(sme, false);

		break;
	    }
	default:
		error = ENOTTY;
		break;
	}

	return error;
}

/*
 * sysmon_envsys_create:
 * 
 * 	+ Allocates a new sysmon_envsys object and initializes the
 * 	  stuff for sensors and events.
 */
struct sysmon_envsys *
sysmon_envsys_create(void)
{
	struct sysmon_envsys *sme;

	sme = kmem_zalloc(sizeof(*sme), KM_SLEEP);
	TAILQ_INIT(&sme->sme_sensors_list);
	LIST_INIT(&sme->sme_events_list);
	mutex_init(&sme->sme_mtx, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sme->sme_work_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sme->sme_condvar, "sme_wait");

	return sme;
}

/*
 * sysmon_envsys_destroy:
 *
 * 	+ Removes all sensors from the tail queue, destroys the callout
 * 	  and frees the sysmon_envsys object.
 */
void
sysmon_envsys_destroy(struct sysmon_envsys *sme)
{
	envsys_data_t *edata;

	KASSERT(sme != NULL);

	while (!TAILQ_EMPTY(&sme->sme_sensors_list)) {
		edata = TAILQ_FIRST(&sme->sme_sensors_list);
		TAILQ_REMOVE(&sme->sme_sensors_list, edata, sensors_head);
	}
	mutex_destroy(&sme->sme_mtx);
	mutex_destroy(&sme->sme_work_mtx);
	cv_destroy(&sme->sme_condvar);
	kmem_free(sme, sizeof(*sme));
}

/*
 * sysmon_envsys_sensor_attach:
 *
 * 	+ Attaches a sensor into a sysmon_envsys device checking that units
 * 	  is set to a valid type and description is unique and not empty.
 */
int
sysmon_envsys_sensor_attach(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	const struct sme_descr_entry *sdt_units;
	envsys_data_t *oedata;

	KASSERT(sme != NULL || edata != NULL);

	/* 
	 * Find the correct units for this sensor.
	 */
	sdt_units = sme_find_table_entry(SME_DESC_UNITS, edata->units);
	if (sdt_units->type == -1)
		return EINVAL;

	/*
	 * Check that description is not empty or duplicate.
	 */
	if (strlen(edata->desc) == 0)
		return EINVAL;

	mutex_enter(&sme->sme_mtx);
	sysmon_envsys_acquire(sme, true);
	TAILQ_FOREACH(oedata, &sme->sme_sensors_list, sensors_head) {
		if (strcmp(oedata->desc, edata->desc) == 0) {
			sysmon_envsys_release(sme, true);
			mutex_exit(&sme->sme_mtx);
			return EEXIST;
		}
	}
	/*
	 * Ok, the sensor has been added into the device queue.
	 */
	TAILQ_INSERT_TAIL(&sme->sme_sensors_list, edata, sensors_head);

	/*
	 * Give the sensor an index position.
	 */
	edata->sensor = sme->sme_nsensors;
	sme->sme_nsensors++;
	sysmon_envsys_release(sme, true);
	mutex_exit(&sme->sme_mtx);

	DPRINTF(("%s: attached #%d (%s), units=%d (%s)\n",
	    __func__, edata->sensor, edata->desc,
	    sdt_units->type, sdt_units->desc));

	return 0;
}

/*
 * sysmon_envsys_sensor_detach:
 *
 * 	+ Detachs a sensor from a sysmon_envsys device and decrements the
 * 	  sensors count on success.
 */
int
sysmon_envsys_sensor_detach(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	envsys_data_t *oedata;
	bool found = false;
	bool destroy = false;

	KASSERT(sme != NULL || edata != NULL);

	/*
	 * Check the sensor is already on the list.
	 */
	mutex_enter(&sme->sme_mtx);
	sysmon_envsys_acquire(sme, true);
	TAILQ_FOREACH(oedata, &sme->sme_sensors_list, sensors_head) {
		if (oedata->sensor == edata->sensor) {
			found = true;
			break;
		}
	}

	if (!found) {
		sysmon_envsys_release(sme, true);
		mutex_exit(&sme->sme_mtx);
		return EINVAL;
	}

	/*
	 * remove it, unhook from rnd(4), and decrement the sensors count.
	 */
	sme_event_unregister_sensor(sme, edata);
	if (LIST_EMPTY(&sme->sme_events_list)) {
		sme_events_halt_callout(sme);
		destroy = true;
	}
	TAILQ_REMOVE(&sme->sme_sensors_list, edata, sensors_head);
	sme->sme_nsensors--;
	sysmon_envsys_release(sme, true);
	mutex_exit(&sme->sme_mtx);

	if (destroy)
		sme_events_destroy(sme);

	return 0;
}


/*
 * sysmon_envsys_register:
 *
 *	+ Register a sysmon envsys device.
 *	+ Create array of dictionaries for a device.
 */
int
sysmon_envsys_register(struct sysmon_envsys *sme)
{
	struct sme_evdrv {
		SLIST_ENTRY(sme_evdrv) evdrv_head;
		sme_event_drv_t *evdrv;
	};
	SLIST_HEAD(, sme_evdrv) sme_evdrv_list;
	struct sme_evdrv *evdv = NULL;
	struct sysmon_envsys *lsme;
	prop_array_t array = NULL;
	prop_dictionary_t dict, dict2;
	envsys_data_t *edata = NULL;
	sme_event_drv_t *this_evdrv;
	int nevent;
	int error = 0;
	char rnd_name[sizeof(edata->rnd_src.name)];

	KASSERT(sme != NULL);
	KASSERT(sme->sme_name != NULL);

	(void)RUN_ONCE(&once_envsys, sme_preinit);

	/*
	 * Check if requested sysmon_envsys device is valid
	 * and does not exist already in the list.
	 */
	mutex_enter(&sme_global_mtx);
	LIST_FOREACH(lsme, &sysmon_envsys_list, sme_list) {
	       if (strcmp(lsme->sme_name, sme->sme_name) == 0) {
			mutex_exit(&sme_global_mtx);
			return EEXIST;
	       }
	}
	mutex_exit(&sme_global_mtx);

	/*
	 * sanity check: if SME_DISABLE_REFRESH is not set,
	 * the sme_refresh function callback must be non NULL.
	 */
	if ((sme->sme_flags & SME_DISABLE_REFRESH) == 0)
		if (!sme->sme_refresh)
			return EINVAL;

	/*
	 * If the list of sensors is empty, there's no point to continue...
	 */
	if (TAILQ_EMPTY(&sme->sme_sensors_list)) {
		DPRINTF(("%s: sensors list empty for %s\n", __func__,
		    sme->sme_name));
		return ENOTSUP;
	}

	/* 
	 * Initialize the singly linked list for driver events.
	 */
	SLIST_INIT(&sme_evdrv_list);

	array = prop_array_create();
	if (!array)
		return ENOMEM;

	/*
	 * Iterate over all sensors and create a dictionary per sensor.
	 * We must respect the order in which the sensors were added.
	 */
	TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
		dict = prop_dictionary_create();
		if (!dict) {
			error = ENOMEM;
			goto out2;
		}

		/*
		 * Create all objects in sensor's dictionary.
		 */
		this_evdrv = sme_add_sensor_dictionary(sme, array,
						       dict, edata);
		if (this_evdrv) {
			evdv = kmem_zalloc(sizeof(*evdv), KM_SLEEP);
			evdv->evdrv = this_evdrv;
			SLIST_INSERT_HEAD(&sme_evdrv_list, evdv, evdrv_head);
		}
	}

	/* 
	 * If the array does not contain any object (sensor), there's
	 * no need to attach the driver.
	 */
	if (prop_array_count(array) == 0) {
		error = EINVAL;
		DPRINTF(("%s: empty array for '%s'\n", __func__,
		    sme->sme_name));
		goto out;
	}

	/*
	 * Add the dictionary for the global properties of this device.
	 */
	dict2 = prop_dictionary_create();
	if (!dict2) {
		error = ENOMEM;
		goto out;
	}

	error = sme_add_property_dictionary(sme, array, dict2);
	if (error) {
		prop_object_release(dict2);
		goto out;
	}

	/*
	 * Add the array into the global dictionary for the driver.
	 *
	 * <dict>
	 * 	<key>foo0</key>
	 * 	<array>
	 * 		...
	 */
	mutex_enter(&sme_global_mtx);
	if (!prop_dictionary_set(sme_propd, sme->sme_name, array)) {
		error = EINVAL;
		mutex_exit(&sme_global_mtx);
		DPRINTF(("%s: prop_dictionary_set for '%s'\n", __func__,
		    sme->sme_name));
		goto out;
	}

	/*
	 * Add the device into the list.
	 */
	LIST_INSERT_HEAD(&sysmon_envsys_list, sme, sme_list);
	sme->sme_fsensor = sysmon_envsys_next_sensor_index;
	sysmon_envsys_next_sensor_index += sme->sme_nsensors;
	mutex_exit(&sme_global_mtx);

out:
	/*
	 * No errors?  Make an initial data refresh if was requested,
	 * then register the events that were set in the driver.  Do
	 * the refresh first in case it is needed to establish the
	 * limits or max_value needed by some events.
	 */
	if (error == 0) {
		nevent = 0;

		if (sme->sme_flags & SME_INIT_REFRESH) {
			sysmon_task_queue_sched(0, sme_initial_refresh, sme);
			DPRINTF(("%s: scheduled initial refresh for '%s'\n",
				__func__, sme->sme_name));
		}
		SLIST_FOREACH(evdv, &sme_evdrv_list, evdrv_head) {
			sysmon_task_queue_sched(0,
			    sme_event_drvadd, evdv->evdrv);
			nevent++;
		}
		/*
		 * Hook the sensor into rnd(4) entropy pool if requested
		 */
		TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
			if (edata->flags & ENVSYS_FHAS_ENTROPY) {
				uint32_t rnd_type, rnd_flag = 0;
				size_t n;
				int tail = 1;

				snprintf(rnd_name, sizeof(rnd_name), "%s-%s",
				    sme->sme_name, edata->desc);
				n = strlen(rnd_name);
				/*
				 * 1) Remove trailing white space(s).
				 * 2) If space exist, replace it with '-'
				 */
				while (--n) {
					if (rnd_name[n] == ' ') {
						if (tail != 0)
							rnd_name[n] = '\0';
						else
							rnd_name[n] = '-';
					} else
						tail = 0;
				}
				rnd_flag |= RND_FLAG_COLLECT_TIME;
				rnd_flag |= RND_FLAG_ESTIMATE_TIME;

				switch (edata->units) {
				    case ENVSYS_STEMP:
				    case ENVSYS_SFANRPM:
				    case ENVSYS_INTEGER:
					rnd_type = RND_TYPE_ENV;
					rnd_flag |= RND_FLAG_COLLECT_VALUE;
					rnd_flag |= RND_FLAG_ESTIMATE_VALUE;
					break;
				    case ENVSYS_SVOLTS_AC:
				    case ENVSYS_SVOLTS_DC:
				    case ENVSYS_SOHMS:
				    case ENVSYS_SWATTS:
				    case ENVSYS_SAMPS:
				    case ENVSYS_SWATTHOUR:
				    case ENVSYS_SAMPHOUR:
					rnd_type = RND_TYPE_POWER;
					rnd_flag |= RND_FLAG_COLLECT_VALUE;
					rnd_flag |= RND_FLAG_ESTIMATE_VALUE;
					break;
				    default:
					rnd_type = RND_TYPE_UNKNOWN;
					break;
				}
				rnd_attach_source(&edata->rnd_src, rnd_name,
				    rnd_type, rnd_flag);
			}
		}
		DPRINTF(("%s: driver '%s' registered (nsens=%d nevent=%d)\n",
		    __func__, sme->sme_name, sme->sme_nsensors, nevent));
	}

out2:
	while (!SLIST_EMPTY(&sme_evdrv_list)) {
		evdv = SLIST_FIRST(&sme_evdrv_list);
		SLIST_REMOVE_HEAD(&sme_evdrv_list, evdrv_head);
		kmem_free(evdv, sizeof(*evdv));
	}
	if (!error)
		return 0;

	/*
	 * Ugh... something wasn't right; unregister all events and sensors
	 * previously assigned and destroy the array with all its objects.
	 */
	DPRINTF(("%s: failed to register '%s' (%d)\n", __func__,
	    sme->sme_name, error));

	sme_event_unregister_all(sme);
	while (!TAILQ_EMPTY(&sme->sme_sensors_list)) {
		edata = TAILQ_FIRST(&sme->sme_sensors_list);
		TAILQ_REMOVE(&sme->sme_sensors_list, edata, sensors_head);
	}
	sysmon_envsys_destroy_plist(array);
	return error;
}

/*
 * sysmon_envsys_destroy_plist:
 *
 * 	+ Remove all objects from the array of dictionaries that is
 * 	  created in a sysmon envsys device.
 */
static void
sysmon_envsys_destroy_plist(prop_array_t array)
{
	prop_object_iterator_t iter, iter2;
	prop_dictionary_t dict;
	prop_object_t obj;

	KASSERT(array != NULL);
	KASSERT(prop_object_type(array) == PROP_TYPE_ARRAY);

	DPRINTFOBJ(("%s: objects in array=%d\n", __func__,
	    prop_array_count(array)));

	iter = prop_array_iterator(array);
	if (!iter)
		return;

	while ((dict = prop_object_iterator_next(iter))) {
		KASSERT(prop_object_type(dict) == PROP_TYPE_DICTIONARY);
		iter2 = prop_dictionary_iterator(dict);
		if (!iter2)
			goto out;
		DPRINTFOBJ(("%s: iterating over dictionary\n", __func__));
		while ((obj = prop_object_iterator_next(iter2)) != NULL) {
			DPRINTFOBJ(("%s: obj=%s\n", __func__,
			    prop_dictionary_keysym_cstring_nocopy(obj)));
			prop_dictionary_remove(dict,
			    prop_dictionary_keysym_cstring_nocopy(obj));
			prop_object_iterator_reset(iter2);
		}
		prop_object_iterator_release(iter2);
		DPRINTFOBJ(("%s: objects in dictionary:%d\n",
		    __func__, prop_dictionary_count(dict)));
		prop_object_release(dict);
	}

out:
	prop_object_iterator_release(iter);
	prop_object_release(array);
}

/*
 * sysmon_envsys_unregister:
 *
 *	+ Unregister a sysmon envsys device.
 */
void
sysmon_envsys_unregister(struct sysmon_envsys *sme)
{
	prop_array_t array;
	struct sysmon_envsys *osme;

	KASSERT(sme != NULL);

	/*
	 * Decrement global sensors counter and the first_sensor index
	 * for remaining devices in the list (only used for compatibility
	 * with previous API), and remove the device from the list.
	 */
	mutex_enter(&sme_global_mtx);
	sysmon_envsys_next_sensor_index -= sme->sme_nsensors;
	LIST_FOREACH(osme, &sysmon_envsys_list, sme_list) {
		if (osme->sme_fsensor >= sme->sme_fsensor)
			osme->sme_fsensor -= sme->sme_nsensors;
	}
	LIST_REMOVE(sme, sme_list);
	mutex_exit(&sme_global_mtx);

	/*
	 * Unregister all events associated with device.
	 */
	sme_event_unregister_all(sme);

	/*
	 * Remove the device (and all its objects) from the global dictionary.
	 */
	array = prop_dictionary_get(sme_propd, sme->sme_name);
	if (array && prop_object_type(array) == PROP_TYPE_ARRAY) {
		mutex_enter(&sme_global_mtx);
		prop_dictionary_remove(sme_propd, sme->sme_name);
		mutex_exit(&sme_global_mtx);
		sysmon_envsys_destroy_plist(array);
	}
	/*
	 * And finally destroy the sysmon_envsys object.
	 */
	sysmon_envsys_destroy(sme);
}

/*
 * sysmon_envsys_find:
 *
 *	+ Find a sysmon envsys device and mark it as busy
 *	  once it's available.
 */
struct sysmon_envsys *
sysmon_envsys_find(const char *name)
{
	struct sysmon_envsys *sme;

	mutex_enter(&sme_global_mtx);
	LIST_FOREACH(sme, &sysmon_envsys_list, sme_list) {
		if (strcmp(sme->sme_name, name) == 0) {
			sysmon_envsys_acquire(sme, false);
			break;
		}
	}
	mutex_exit(&sme_global_mtx);

	return sme;
}

/*
 * Compatibility function with the old API.
 */
struct sysmon_envsys *
sysmon_envsys_find_40(u_int idx)
{
	struct sysmon_envsys *sme;

	mutex_enter(&sme_global_mtx);
	LIST_FOREACH(sme, &sysmon_envsys_list, sme_list) {
		if (idx >= sme->sme_fsensor &&
	    	    idx < (sme->sme_fsensor + sme->sme_nsensors)) {
			sysmon_envsys_acquire(sme, false);
			break;
		}
	}
	mutex_exit(&sme_global_mtx);

	return sme;
}

/*
 * sysmon_envsys_acquire:
 *
 * 	+ Wait until a sysmon envsys device is available and mark
 * 	  it as busy.
 */
void
sysmon_envsys_acquire(struct sysmon_envsys *sme, bool locked)
{
	KASSERT(sme != NULL);

	if (locked) {
		while (sme->sme_flags & SME_FLAG_BUSY)
			cv_wait(&sme->sme_condvar, &sme->sme_mtx);
		sme->sme_flags |= SME_FLAG_BUSY;
	} else {
		mutex_enter(&sme->sme_mtx);
		while (sme->sme_flags & SME_FLAG_BUSY)
			cv_wait(&sme->sme_condvar, &sme->sme_mtx);
		sme->sme_flags |= SME_FLAG_BUSY;
		mutex_exit(&sme->sme_mtx);
	}
}

/*
 * sysmon_envsys_release:
 *
 * 	+ Unmark a sysmon envsys device as busy, and notify
 * 	  waiters.
 */
void
sysmon_envsys_release(struct sysmon_envsys *sme, bool locked)
{
	KASSERT(sme != NULL);

	if (locked) {
		sme->sme_flags &= ~SME_FLAG_BUSY;
		cv_broadcast(&sme->sme_condvar);
	} else {
		mutex_enter(&sme->sme_mtx);
		sme->sme_flags &= ~SME_FLAG_BUSY;
		cv_broadcast(&sme->sme_condvar);
		mutex_exit(&sme->sme_mtx);
	}
}

/*
 * sme_initial_refresh:
 * 	
 * 	+ Do an initial refresh of the sensors in a device just after
 * 	  interrupts are enabled in the autoconf(9) process.
 *
 */
static void
sme_initial_refresh(void *arg)
{
	struct sysmon_envsys *sme = arg;
	envsys_data_t *edata;

	mutex_enter(&sme->sme_mtx);
	sysmon_envsys_acquire(sme, true);
	TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head)
		sysmon_envsys_refresh_sensor(sme, edata);
	sysmon_envsys_release(sme, true);
	mutex_exit(&sme->sme_mtx);
}

/*
 * sme_sensor_dictionary_get:
 *
 * 	+ Returns a dictionary of a device specified by its index
 * 	  position.
 */
prop_dictionary_t
sme_sensor_dictionary_get(prop_array_t array, const char *index)
{
	prop_object_iterator_t iter;
	prop_dictionary_t dict;
	prop_object_t obj;

	KASSERT(array != NULL || index != NULL);

	iter = prop_array_iterator(array);
	if (!iter)
		return NULL;

	while ((dict = prop_object_iterator_next(iter))) {
		obj = prop_dictionary_get(dict, "index");
		if (prop_string_equals_cstring(obj, index))
			break;
	}

	prop_object_iterator_release(iter);
	return dict;
}

/*
 * sme_remove_userprops:
 *
 * 	+ Remove all properties from all devices that were set by
 * 	  the ENVSYS_SETDICTIONARY ioctl.
 */
static void
sme_remove_userprops(void)
{
	struct sysmon_envsys *sme;
	prop_array_t array;
	prop_dictionary_t sdict;
	envsys_data_t *edata = NULL;
	char tmp[ENVSYS_DESCLEN];
	char rnd_name[sizeof(edata->rnd_src.name)];
	sysmon_envsys_lim_t lims;
	const struct sme_descr_entry *sdt_units;
	uint32_t props;
	int ptype;

	mutex_enter(&sme_global_mtx);
	LIST_FOREACH(sme, &sysmon_envsys_list, sme_list) {
		sysmon_envsys_acquire(sme, false);
		array = prop_dictionary_get(sme_propd, sme->sme_name);

		TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
			(void)snprintf(tmp, sizeof(tmp), "sensor%d",
				       edata->sensor);
			sdict = sme_sensor_dictionary_get(array, tmp);
			KASSERT(sdict != NULL);

			ptype = 0;
			if (edata->upropset & PROP_BATTCAP) {
				prop_dictionary_remove(sdict,
				    "critical-capacity");
				ptype = PENVSYS_EVENT_CAPACITY;
			}

			if (edata->upropset & PROP_BATTWARN) {
				prop_dictionary_remove(sdict,
				    "warning-capacity");
				ptype = PENVSYS_EVENT_CAPACITY;
			}

			if (edata->upropset & PROP_BATTHIGH) {
				prop_dictionary_remove(sdict,
				    "high-capacity");
				ptype = PENVSYS_EVENT_CAPACITY;
			}

			if (edata->upropset & PROP_BATTMAX) {
				prop_dictionary_remove(sdict,
				    "maximum-capacity");
				ptype = PENVSYS_EVENT_CAPACITY;
			}
			if (edata->upropset & PROP_WARNMAX) {
				prop_dictionary_remove(sdict, "warning-max");
				ptype = PENVSYS_EVENT_LIMITS;
			}

			if (edata->upropset & PROP_WARNMIN) {
				prop_dictionary_remove(sdict, "warning-min");
				ptype = PENVSYS_EVENT_LIMITS;
			}

			if (edata->upropset & PROP_CRITMAX) {
				prop_dictionary_remove(sdict, "critical-max");
				ptype = PENVSYS_EVENT_LIMITS;
			}

			if (edata->upropset & PROP_CRITMIN) {
				prop_dictionary_remove(sdict, "critical-min");
				ptype = PENVSYS_EVENT_LIMITS;
			}
			if (edata->upropset & PROP_RFACT) {
				(void)sme_sensor_upint32(sdict, "rfact", 0);
				edata->rfact = 0;
			}

			if (edata->upropset & PROP_DESC)
				(void)sme_sensor_upstring(sdict,
			  	    "description", edata->desc);

			if (ptype == 0)
				continue;

			/*
			 * If there were any limit values removed, we
			 * need to revert to initial limits.
			 *
			 * First, tell the driver that we need it to 
			 * restore any h/w limits which may have been 
			 * changed to stored, boot-time values.
			 */
			if (sme->sme_set_limits) {
				DPRINTF(("%s: reset limits for %s %s\n",
					__func__, sme->sme_name, edata->desc));
				(*sme->sme_set_limits)(sme, edata, NULL, NULL);
			}

			/*
			 * Next, we need to retrieve those initial limits.
			 */
			props = 0;
			edata->upropset &= ~PROP_LIMITS;
			if (sme->sme_get_limits) {
				DPRINTF(("%s: retrieve limits for %s %s\n",
					__func__, sme->sme_name, edata->desc));
				lims = edata->limits;
				(*sme->sme_get_limits)(sme, edata, &lims,
						       &props);
			}

			/*
			 * Finally, remove any old limits event, then
			 * install a new event (which will update the
			 * dictionary)
			 */
			sme_event_unregister(sme, edata->desc,
			    PENVSYS_EVENT_LIMITS);

			/*
			 * Find the correct units for this sensor.
			 */
			sdt_units = sme_find_table_entry(SME_DESC_UNITS,
			    edata->units);

			if (props & PROP_LIMITS) {
				DPRINTF(("%s: install limits for %s %s\n",
					__func__, sme->sme_name, edata->desc));

				sme_event_register(sdict, edata, sme,
				    &lims, props, PENVSYS_EVENT_LIMITS,
				    sdt_units->crittype);
			}
			if (edata->flags & ENVSYS_FHAS_ENTROPY) {
				sme_event_register(sdict, edata, sme,
				    &lims, props, PENVSYS_EVENT_NULL,
				    sdt_units->crittype);
				snprintf(rnd_name, sizeof(rnd_name), "%s-%s",
				    sme->sme_name, edata->desc);
				rnd_attach_source(&edata->rnd_src, rnd_name,
				    RND_TYPE_ENV, RND_FLAG_COLLECT_VALUE|
						  RND_FLAG_COLLECT_TIME|
						  RND_FLAG_ESTIMATE_VALUE|
						  RND_FLAG_ESTIMATE_TIME);
			}
		}

		/*
		 * Restore default timeout value.
		 */
		sme->sme_events_timeout = SME_EVENTS_DEFTIMEOUT;
		sme_schedule_callout(sme);
		sysmon_envsys_release(sme, false);
	}
	mutex_exit(&sme_global_mtx);
}

/*
 * sme_add_property_dictionary:
 * 
 * 	+ Add global properties into a device.
 */
static int
sme_add_property_dictionary(struct sysmon_envsys *sme, prop_array_t array,
			    prop_dictionary_t dict)
{
	prop_dictionary_t pdict;
	const char *class;
	int error = 0;

	pdict = prop_dictionary_create();
	if (!pdict)
		return EINVAL;

	/*
	 * Add the 'refresh-timeout' and 'dev-class' objects into the
	 * 'device-properties' dictionary.
	 *
	 * 	...
	 * 	<dict>
	 * 		<key>device-properties</key>
	 * 		<dict>
	 * 			<key>refresh-timeout</key>
	 * 			<integer>120</integer<
	 *			<key>device-class</key>
	 *			<string>class_name</string>
	 * 		</dict>
	 * 	</dict>
	 * 	...
	 *
	 */
	if (sme->sme_events_timeout == 0) {
		sme->sme_events_timeout = SME_EVENTS_DEFTIMEOUT;
		sme_schedule_callout(sme);
	}

	if (!prop_dictionary_set_uint64(pdict, "refresh-timeout",
					sme->sme_events_timeout)) {
		error = EINVAL;
		goto out;
	}
	if (sme->sme_class == SME_CLASS_BATTERY)
		class = "battery";
	else if (sme->sme_class == SME_CLASS_ACADAPTER)
		class = "ac-adapter";
	else
		class = "other";
	if (!prop_dictionary_set_cstring_nocopy(pdict, "device-class", class)) {
		error = EINVAL;
		goto out;
	}

	if (!prop_dictionary_set(dict, "device-properties", pdict)) {
		error = EINVAL;
		goto out;
	}

	/*
	 * Add the device dictionary into the sysmon envsys array.
	 */
	if (!prop_array_add(array, dict))
		error = EINVAL;

out:
	prop_object_release(pdict);
	return error;
}

/*
 * sme_add_sensor_dictionary:
 *
 * 	+ Adds the sensor objects into the dictionary and returns a pointer
 * 	  to a sme_event_drv_t object if a monitoring flag was set
 * 	  (or NULL otherwise).
 */
static sme_event_drv_t *
sme_add_sensor_dictionary(struct sysmon_envsys *sme, prop_array_t array,
		    	  prop_dictionary_t dict, envsys_data_t *edata)
{
	const struct sme_descr_entry *sdt;
	int error;
	sme_event_drv_t *sme_evdrv_t = NULL;
	char indexstr[ENVSYS_DESCLEN];
	bool mon_supported, allow_rfact;

	/*
	 * Add the index sensor string.
	 *
	 * 		...
	 * 		<key>index</eyr
	 * 		<string>sensor0</string>
	 * 		...
	 */
	(void)snprintf(indexstr, sizeof(indexstr), "sensor%d", edata->sensor);
	if (sme_sensor_upstring(dict, "index", indexstr))
		goto bad;

	/*
	 * 		...
	 * 		<key>description</key>
	 * 		<string>blah blah</string>
	 * 		...
	 */
	if (sme_sensor_upstring(dict, "description", edata->desc))
		goto bad;

	/*
	 * Add the monitoring boolean object:
	 *
	 * 		...
	 * 		<key>monitoring-supported</key>
	 * 		<true/>
	 *		...
	 * 
	 * always false on Battery {capacity,charge}, Drive and Indicator types.
	 * They cannot be monitored.
	 *
	 */
	if ((edata->flags & ENVSYS_FMONNOTSUPP) ||
	    (edata->units == ENVSYS_INDICATOR) ||
	    (edata->units == ENVSYS_DRIVE) ||
	    (edata->units == ENVSYS_BATTERY_CAPACITY) ||
	    (edata->units == ENVSYS_BATTERY_CHARGE))
		mon_supported = false;
	else
		mon_supported = true;
	if (sme_sensor_upbool(dict, "monitoring-supported", mon_supported))
		goto out;

	/*
	 * Add the allow-rfact boolean object, true if
	 * ENVSYS_FCHANGERFACT is set, false otherwise.
	 *
	 * 		...
	 * 		<key>allow-rfact</key>
	 * 		<true/>
	 * 		...
	 */
	if (edata->units == ENVSYS_SVOLTS_DC ||
	    edata->units == ENVSYS_SVOLTS_AC) {
		if (edata->flags & ENVSYS_FCHANGERFACT)
			allow_rfact = true;
		else
			allow_rfact = false;
		if (sme_sensor_upbool(dict, "allow-rfact", allow_rfact))
			goto out;
	}

	error = sme_update_sensor_dictionary(dict, edata,
			(edata->state == ENVSYS_SVALID));
	if (error < 0)
		goto bad;
	else if (error)
		goto out;

	/*
	 * 	...
	 * </dict>
	 *
	 * Add the dictionary into the array.
	 *
	 */
	if (!prop_array_add(array, dict)) {
		DPRINTF(("%s: prop_array_add\n", __func__));
		goto bad;
	}

	/*
	 * Register new event(s) if any monitoring flag was set or if
	 * the sensor provides entropy for rnd(4).
	 */
	if (edata->flags & (ENVSYS_FMONANY | ENVSYS_FHAS_ENTROPY)) {
		sme_evdrv_t = kmem_zalloc(sizeof(*sme_evdrv_t), KM_SLEEP);
		sme_evdrv_t->sed_sdict = dict;
		sme_evdrv_t->sed_edata = edata;
		sme_evdrv_t->sed_sme = sme;
		sdt = sme_find_table_entry(SME_DESC_UNITS, edata->units);
		sme_evdrv_t->sed_powertype = sdt->crittype;
	}

out:
	return sme_evdrv_t;

bad:
	prop_object_release(dict);
	return NULL;
}

/*
 * Find the maximum of all currently reported values.
 * The provided callback decides whether a sensor is part of the
 * maximum calculation (by returning true) or ignored (callback
 * returns false). Example usage: callback selects temperature
 * sensors in a given thermal zone, the function calculates the
 * maximum currently reported temperature in this zone.
 * If the parameter "refresh" is true, new values will be aquired
 * from the hardware, if not, the last reported value will be used.
 */
uint32_t
sysmon_envsys_get_max_value(bool (*predicate)(const envsys_data_t*),
	bool refresh)
{
	struct sysmon_envsys *sme;
	uint32_t maxv, v;

	maxv = 0;
	mutex_enter(&sme_global_mtx);
	LIST_FOREACH(sme, &sysmon_envsys_list, sme_list) {
		sysmon_envsys_acquire(sme, false);
		v = sme_get_max_value(sme, predicate, refresh);
		sysmon_envsys_release(sme, false);
		if (v > maxv)
			maxv = v;
	}
	mutex_exit(&sme_global_mtx);
	return maxv;
}

static uint32_t
sme_get_max_value(struct sysmon_envsys *sme,
    bool (*predicate)(const envsys_data_t*),
    bool refresh)
{
	envsys_data_t *edata;
	uint32_t maxv, v;

	/* 
	 * Iterate over all sensors that match the predicate
	 */
	maxv = 0;
	TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
		if (!(*predicate)(edata))
			continue;

		/* 
		 * refresh sensor data
		 */
		mutex_enter(&sme->sme_mtx);
		sysmon_envsys_refresh_sensor(sme, edata);
		mutex_exit(&sme->sme_mtx);

		v = edata->value_cur;
		if (v > maxv)
			maxv = v;

	}

	return maxv;
}

/*
 * sme_update_dictionary:
 *
 * 	+ Update per-sensor dictionaries with new values if there were
 * 	  changes, otherwise the object in dictionary is untouched.
 */
int
sme_update_dictionary(struct sysmon_envsys *sme)
{
	envsys_data_t *edata;
	prop_object_t array, dict, obj, obj2;
	int error = 0;

	/* 
	 * Retrieve the array of dictionaries in device.
	 */
	array = prop_dictionary_get(sme_propd, sme->sme_name);
	if (prop_object_type(array) != PROP_TYPE_ARRAY) {
		DPRINTF(("%s: not an array (%s)\n", __func__, sme->sme_name));
		return EINVAL;
	}

	/*
	 * Get the last dictionary on the array, this contains the
	 * 'device-properties' sub-dictionary.
	 */
	obj = prop_array_get(array, prop_array_count(array) - 1);
	if (!obj || prop_object_type(obj) != PROP_TYPE_DICTIONARY) {
		DPRINTF(("%s: not a device-properties dictionary\n", __func__));
		return EINVAL;
	}

	obj2 = prop_dictionary_get(obj, "device-properties");
	if (!obj2)
		return EINVAL;

	/*
	 * Update the 'refresh-timeout' property.
	 */
	if (!prop_dictionary_set_uint64(obj2, "refresh-timeout",
					sme->sme_events_timeout))
		return EINVAL;

	/* 
	 * - iterate over all sensors.
	 * - fetch new data.
	 * - check if data in dictionary is different than new data.
	 * - update dictionary if there were changes.
	 */
	DPRINTF(("%s: updating '%s' with nsensors=%d\n", __func__,
	    sme->sme_name, sme->sme_nsensors));

	/*
	 * Don't bother with locking when traversing the queue,
	 * the device is already marked as busy; if a sensor
	 * is going to be removed or added it will have to wait.
	 */
	TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
		/* 
		 * refresh sensor data via sme_envsys_refresh_sensor
		 */
		mutex_enter(&sme->sme_mtx);
		sysmon_envsys_refresh_sensor(sme, edata);
		mutex_exit(&sme->sme_mtx);

		/* 
		 * retrieve sensor's dictionary.
		 */
		dict = prop_array_get(array, edata->sensor);
		if (prop_object_type(dict) != PROP_TYPE_DICTIONARY) {
			DPRINTF(("%s: not a dictionary (%d:%s)\n",
			    __func__, edata->sensor, sme->sme_name));
			return EINVAL;
		}

		/* 
		 * update sensor's state.
		 */
		error = sme_update_sensor_dictionary(dict, edata, true);

		if (error)
			break;
	}

	return error;
}

int
sme_update_sensor_dictionary(prop_object_t dict, envsys_data_t *edata,
	bool value_update)
{
	const struct sme_descr_entry *sdt;
	int error = 0;

	sdt = sme_find_table_entry(SME_DESC_STATES, edata->state);
	if (sdt == NULL) {
		printf("sme_update_sensor_dictionary: can not update sensor "
		    "state %d unknown\n", edata->state);
		return EINVAL;
	}

	DPRINTFOBJ(("%s: sensor #%d type=%d (%s) flags=%d\n", __func__,
	    edata->sensor, sdt->type, sdt->desc, edata->flags));

	error = sme_sensor_upstring(dict, "state", sdt->desc);
	if (error)
		return (-error);

	/* 
	 * update sensor's type.
	 */
	sdt = sme_find_table_entry(SME_DESC_UNITS, edata->units);

	DPRINTFOBJ(("%s: sensor #%d units=%d (%s)\n", __func__, edata->sensor,
	    sdt->type, sdt->desc));

	error = sme_sensor_upstring(dict, "type", sdt->desc);
	if (error)
		return (-error);

	if (value_update) {
		/* 
		 * update sensor's current value.
		 */
		error = sme_sensor_upint32(dict, "cur-value", edata->value_cur);
		if (error)
			return error;
	}

	/*
	 * Battery charge and Indicator types do not
	 * need the remaining objects, so skip them.
	 */
	if (edata->units == ENVSYS_INDICATOR ||
	    edata->units == ENVSYS_BATTERY_CHARGE)
		return error;

	/* 
	 * update sensor flags.
	 */
	if (edata->flags & ENVSYS_FPERCENT) {
		error = sme_sensor_upbool(dict, "want-percentage", true);
		if (error)
			return error;
	}

	if (value_update) {
		/*
		 * update sensor's {max,min}-value.
		 */
		if (edata->flags & ENVSYS_FVALID_MAX) {
			error = sme_sensor_upint32(dict, "max-value",
						   edata->value_max);
			if (error)
				return error;
		}

		if (edata->flags & ENVSYS_FVALID_MIN) {
			error = sme_sensor_upint32(dict, "min-value",
						   edata->value_min);
			if (error)
				return error;
		}

		/* 
		 * update 'rpms' only for ENVSYS_SFANRPM sensors.
		 */
		if (edata->units == ENVSYS_SFANRPM) {
			error = sme_sensor_upuint32(dict, "rpms", edata->rpms);
			if (error)
				return error;
		}

		/* 
		 * update 'rfact' only for ENVSYS_SVOLTS_[AD]C sensors.
		 */
		if (edata->units == ENVSYS_SVOLTS_AC ||
		    edata->units == ENVSYS_SVOLTS_DC) {
			error = sme_sensor_upint32(dict, "rfact", edata->rfact);
			if (error)
				return error;
		}
	}

	/* 
	 * update 'drive-state' only for ENVSYS_DRIVE sensors.
	 */
	if (edata->units == ENVSYS_DRIVE) {
		sdt = sme_find_table_entry(SME_DESC_DRIVE_STATES,
					   edata->value_cur);
		error = sme_sensor_upstring(dict, "drive-state", sdt->desc);
		if (error)
			return error;
	}

	/* 
	 * update 'battery-capacity' only for ENVSYS_BATTERY_CAPACITY
	 * sensors.
	 */
	if (edata->units == ENVSYS_BATTERY_CAPACITY) {
		sdt = sme_find_table_entry(SME_DESC_BATTERY_CAPACITY,
		    edata->value_cur);
		error = sme_sensor_upstring(dict, "battery-capacity",
					    sdt->desc);
		if (error)
			return error;
	}

	return error;
}

/*
 * sme_userset_dictionary:
 *
 * 	+ Parse the userland dictionary and run the appropiate tasks
 * 	  that were specified.
 */
int
sme_userset_dictionary(struct sysmon_envsys *sme, prop_dictionary_t udict,
		       prop_array_t array)
{
	const struct sme_descr_entry *sdt;
	envsys_data_t *edata;
	prop_dictionary_t dict, tdict = NULL;
	prop_object_t obj, obj1, obj2, tobj = NULL;
	uint32_t props;
	uint64_t refresh_timo = 0;
	sysmon_envsys_lim_t lims;
	int i, error = 0;
	const char *blah;
	bool targetfound = false;

	/*
	 * The user wanted to change the refresh timeout value for this
	 * device.
	 *
	 * Get the 'device-properties' object from the userland dictionary.
	 */
	obj = prop_dictionary_get(udict, "device-properties");
	if (obj && prop_object_type(obj) == PROP_TYPE_DICTIONARY) {
		/*
		 * Get the 'refresh-timeout' property for this device.
		 */
		obj1 = prop_dictionary_get(obj, "refresh-timeout");
		if (obj1 && prop_object_type(obj1) == PROP_TYPE_NUMBER) {
			targetfound = true;
			refresh_timo =
			    prop_number_unsigned_integer_value(obj1);
			if (refresh_timo < 1)
				error = EINVAL;
			else {
				mutex_enter(&sme->sme_mtx);
				if (sme->sme_events_timeout != refresh_timo) {
					sme->sme_events_timeout = refresh_timo;
					sme_schedule_callout(sme);
				}
				mutex_exit(&sme->sme_mtx);
		}
		}
		return error;

	} else if (!obj) {
		/* 
		 * Get sensor's index from userland dictionary.
		 */
		obj = prop_dictionary_get(udict, "index");
		if (!obj)
			return EINVAL;
		if (prop_object_type(obj) != PROP_TYPE_STRING) {
			DPRINTF(("%s: 'index' not a string\n", __func__));
			return EINVAL;
		}
	} else
		return EINVAL;

	/*
	 * Don't bother with locking when traversing the queue,
	 * the device is already marked as busy; if a sensor
	 * is going to be removed or added it will have to wait.
	 */
	TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
		/*
		 * Get a dictionary and check if it's our sensor by checking
		 * at its index position.
		 */
		dict = prop_array_get(array, edata->sensor);
		obj1 = prop_dictionary_get(dict, "index");

		/* 
		 * is it our sensor?
		 */
		if (!prop_string_equals(obj1, obj))
			continue;

		props = 0;

		/*
		 * Check if a new description operation was
		 * requested by the user and set new description.
		 */
		obj2 = prop_dictionary_get(udict, "description");
		if (obj2 && prop_object_type(obj2) == PROP_TYPE_STRING) {
			targetfound = true;
			blah = prop_string_cstring_nocopy(obj2);

			/*
			 * Check for duplicate description.
			 */
			for (i = 0; i < sme->sme_nsensors; i++) {
				if (i == edata->sensor)
					continue;
				tdict = prop_array_get(array, i);
				tobj =
				    prop_dictionary_get(tdict, "description");
				if (prop_string_equals(obj2, tobj)) {
					error = EEXIST;
					goto out;
				}
			}

			/*
			 * Update the object in dictionary.
			 */
			mutex_enter(&sme->sme_mtx);
			error = sme_sensor_upstring(dict,
						    "description",
						    blah);
			if (error) {
				mutex_exit(&sme->sme_mtx);
				goto out;
			}

			DPRINTF(("%s: sensor%d changed desc to: %s\n",
			    __func__, edata->sensor, blah));
			edata->upropset |= PROP_DESC;
			mutex_exit(&sme->sme_mtx);
		}

		/* 
		 * did the user want to change the rfact?
		 */
		obj2 = prop_dictionary_get(udict, "rfact");
		if (obj2 && prop_object_type(obj2) == PROP_TYPE_NUMBER) {
			targetfound = true;
			if (edata->flags & ENVSYS_FCHANGERFACT) {
				mutex_enter(&sme->sme_mtx);
				edata->rfact = prop_number_integer_value(obj2);
				edata->upropset |= PROP_RFACT;
				mutex_exit(&sme->sme_mtx);
				DPRINTF(("%s: sensor%d changed rfact to %d\n",
				    __func__, edata->sensor, edata->rfact));
			} else {
				error = ENOTSUP;
				goto out;
			}
		}

		sdt = sme_find_table_entry(SME_DESC_UNITS, edata->units);

		/* 
		 * did the user want to set a critical capacity event?
		 */
		obj2 = prop_dictionary_get(udict, "critical-capacity");
		if (obj2 && prop_object_type(obj2) == PROP_TYPE_NUMBER) {
			targetfound = true;
			lims.sel_critmin = prop_number_integer_value(obj2);
			props |= PROP_BATTCAP;
		}

		/* 
		 * did the user want to set a warning capacity event?
		 */
		obj2 = prop_dictionary_get(udict, "warning-capacity");
		if (obj2 && prop_object_type(obj2) == PROP_TYPE_NUMBER) {
			targetfound = true;
			lims.sel_warnmin = prop_number_integer_value(obj2);
			props |= PROP_BATTWARN;
		}

		/* 
		 * did the user want to set a high capacity event?
		 */
		obj2 = prop_dictionary_get(udict, "high-capacity");
		if (obj2 && prop_object_type(obj2) == PROP_TYPE_NUMBER) {
			targetfound = true;
			lims.sel_warnmin = prop_number_integer_value(obj2);
			props |= PROP_BATTHIGH;
		}

		/* 
		 * did the user want to set a maximum capacity event?
		 */
		obj2 = prop_dictionary_get(udict, "maximum-capacity");
		if (obj2 && prop_object_type(obj2) == PROP_TYPE_NUMBER) {
			targetfound = true;
			lims.sel_warnmin = prop_number_integer_value(obj2);
			props |= PROP_BATTMAX;
		}

		/* 
		 * did the user want to set a critical max event?
		 */
		obj2 = prop_dictionary_get(udict, "critical-max");
		if (obj2 && prop_object_type(obj2) == PROP_TYPE_NUMBER) {
			targetfound = true;
			lims.sel_critmax = prop_number_integer_value(obj2);
			props |= PROP_CRITMAX;
		}

		/* 
		 * did the user want to set a warning max event?
		 */
		obj2 = prop_dictionary_get(udict, "warning-max");
		if (obj2 && prop_object_type(obj2) == PROP_TYPE_NUMBER) {
			targetfound = true;
			lims.sel_warnmax = prop_number_integer_value(obj2);
			props |= PROP_WARNMAX;
		}

		/* 
		 * did the user want to set a critical min event?
		 */
		obj2 = prop_dictionary_get(udict, "critical-min");
		if (obj2 && prop_object_type(obj2) == PROP_TYPE_NUMBER) {
			targetfound = true;
			lims.sel_critmin = prop_number_integer_value(obj2);
			props |= PROP_CRITMIN;
		}

		/* 
		 * did the user want to set a warning min event?
		 */
		obj2 = prop_dictionary_get(udict, "warning-min");
		if (obj2 && prop_object_type(obj2) == PROP_TYPE_NUMBER) {
			targetfound = true;
			lims.sel_warnmin = prop_number_integer_value(obj2);
			props |= PROP_WARNMIN;
		}

		if (props && (edata->flags & ENVSYS_FMONNOTSUPP) != 0) {
			error = ENOTSUP;
			goto out;
		}
		if (props || (edata->flags & ENVSYS_FHAS_ENTROPY) != 0) {
			error = sme_event_register(dict, edata, sme, &lims,
					props,
					(edata->flags & ENVSYS_FPERCENT)?
						PENVSYS_EVENT_CAPACITY:
						PENVSYS_EVENT_LIMITS,
					sdt->crittype);
			if (error == EEXIST)
				error = 0;
			if (error) 
				goto out;
		}

		/*
		 * All objects in dictionary were processed.
		 */
		break;
	}

out:
	/* 
	 * invalid target? return the error.
	 */
	if (!targetfound)
		error = EINVAL;

	return error;
}

/*
 * + sysmon_envsys_foreach_sensor
 *
 *	Walk through the devices' sensor lists and execute the callback.
 *	If the callback returns false, the remainder of the current
 *	device's sensors are skipped.
 */
void   
sysmon_envsys_foreach_sensor(sysmon_envsys_callback_t func, void *arg,
			     bool refresh)
{
	struct sysmon_envsys *sme;
	envsys_data_t *sensor;

	mutex_enter(&sme_global_mtx);
	LIST_FOREACH(sme, &sysmon_envsys_list, sme_list) {

		sysmon_envsys_acquire(sme, false);
		TAILQ_FOREACH(sensor, &sme->sme_sensors_list, sensors_head) {
			if (refresh) {
				mutex_enter(&sme->sme_mtx);
				sysmon_envsys_refresh_sensor(sme, sensor);
				mutex_exit(&sme->sme_mtx);
			}
			if (!(*func)(sme, sensor, arg))
				break;
		}
		sysmon_envsys_release(sme, false);
	}
	mutex_exit(&sme_global_mtx);
}

/*
 * Call the sensor's refresh function, and collect/stir entropy
 */
void
sysmon_envsys_refresh_sensor(struct sysmon_envsys *sme, envsys_data_t *edata)
{

	if ((sme->sme_flags & SME_DISABLE_REFRESH) == 0)
		(*sme->sme_refresh)(sme, edata);

	if (edata->flags & ENVSYS_FHAS_ENTROPY &&
	    edata->state != ENVSYS_SINVALID &&
	    edata->value_prev != edata->value_cur)
		rnd_add_uint32(&edata->rnd_src, edata->value_cur);
	edata->value_prev = edata->value_cur;
}

static
int
sysmon_envsys_modcmd(modcmd_t cmd, void *arg)
{
        int ret;
 
        switch (cmd) { 
        case MODULE_CMD_INIT:
                ret = sysmon_envsys_init();
                break;
 
        case MODULE_CMD_FINI:
                ret = sysmon_envsys_fini();
                break; 
   
        case MODULE_CMD_STAT:
        default:
                ret = ENOTTY;
        }
  
        return ret; 
} 
