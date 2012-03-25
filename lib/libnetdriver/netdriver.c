/* This file contains device independent network device driver interface.
 *
 * Changes:
 *   Apr 01, 2010   Created  (Cristiano Giuffrida)
 *
 * The file contains the following entry points:
 *
 *   netdriver_announce: called by a network driver to announce it is up
 *   netdriver_receive:	 receive() interface for network drivers
 */

#include <minix/drivers.h>
#include <minix/endpoint.h>
#include <minix/netdriver.h>
#include <minix/ds.h>

static int conf_expected = TRUE;

/*===========================================================================*
 *			    netdriver_announce				     *
 *===========================================================================*/
void netdriver_announce()
{
/* Announce we are up after a fresh start or restart. */
  int r;
  char key[DS_MAX_KEYLEN];
  char label[DS_MAX_KEYLEN];
  char *driver_prefix = "drv.net.";

  /* Publish a driver up event. */
  r = ds_retrieve_label_name(label, getprocnr());
  if (r != OK) {
	panic("driver_announce: unable to get own label: %d\n", r);
  }
  snprintf(key, DS_MAX_KEYLEN, "%s%s", driver_prefix, label);
  r = ds_publish_u32(key, DS_DRIVER_UP, DSF_OVERWRITE);
  if (r != OK) {
	panic("driver_announce: unable to publish driver up event: %d\n", r);
  }

  conf_expected = TRUE;
}

/*===========================================================================*
 *			     netdriver_receive				     *
 *===========================================================================*/
int netdriver_receive(src, m_ptr, status_ptr)
endpoint_t src;
message *m_ptr;
int *status_ptr;
{
/* receive() interface for drivers. */
  int r;

  while (TRUE) {
	/* Wait for a request. */
	r = sef_receive_status(src, m_ptr, status_ptr);
	if (r != OK) {
		return r;
	}

	/* See if only DL_CONF is to be expected. */
	if(conf_expected) {
		if(m_ptr->m_type == DL_CONF) {
			conf_expected = FALSE;
		}
		else {
			continue;
		}
	}

	break;
  }

  return OK;
}

