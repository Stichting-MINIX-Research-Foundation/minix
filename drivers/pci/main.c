/*
main.c
*/

#include "../drivers.h"

#include <ibm/pci.h>

#include "pci.h"

#define NR_DRIVERS	16

PRIVATE struct name
{
	char name[M3_STRING];
	int tasknr;
} names[NR_DRIVERS];

FORWARD _PROTOTYPE( void do_init, (message *mp)				);
FORWARD _PROTOTYPE( void do_sig_handler, (void)				);
FORWARD _PROTOTYPE( void do_first_dev, (message *mp)			);
FORWARD _PROTOTYPE( void do_next_dev, (message *mp)			);
FORWARD _PROTOTYPE( void do_find_dev, (message *mp)			);
FORWARD _PROTOTYPE( void do_ids, (message *mp)				);
FORWARD _PROTOTYPE( void do_dev_name, (message *mp)			);
FORWARD _PROTOTYPE( void do_slot_name, (message *mp)			);
FORWARD _PROTOTYPE( void do_reserve, (message *mp)			);
FORWARD _PROTOTYPE( void do_attr_r8, (message *mp)			);
FORWARD _PROTOTYPE( void do_attr_r16, (message *mp)			);
FORWARD _PROTOTYPE( void do_attr_r32, (message *mp)			);
FORWARD _PROTOTYPE( void do_attr_w8, (message *mp)			);
FORWARD _PROTOTYPE( void do_attr_w16, (message *mp)			);
FORWARD _PROTOTYPE( void do_attr_w32, (message *mp)			);
FORWARD _PROTOTYPE( void do_rescan_bus, (message *mp)			);

int main(void)
{
	int i, r;
	message m;

	pci_init();

	for (i= 0; i<NR_DRIVERS; i++)
		names[i].tasknr= ANY;

	for(;;)
	{
		r= receive(ANY, &m);
		if (r < 0)
		{
			printf("PCI: receive from ANY failed: %d\n", r);
			break;
		}
		switch(m.m_type)
		{
		case BUSC_PCI_INIT: do_init(&m); break;
		case BUSC_PCI_FIRST_DEV: do_first_dev(&m); break;
		case BUSC_PCI_NEXT_DEV: do_next_dev(&m); break;
		case BUSC_PCI_FIND_DEV: do_find_dev(&m); break;
		case BUSC_PCI_IDS: do_ids(&m); break;
		case BUSC_PCI_DEV_NAME: do_dev_name(&m); break;
		case BUSC_PCI_SLOT_NAME: do_slot_name(&m); break;
		case BUSC_PCI_RESERVE: do_reserve(&m); break;
		case BUSC_PCI_ATTR_R8: do_attr_r8(&m); break;
		case BUSC_PCI_ATTR_R16: do_attr_r16(&m); break;
		case BUSC_PCI_ATTR_R32: do_attr_r32(&m); break;
		case BUSC_PCI_ATTR_W8: do_attr_w8(&m); break;
		case BUSC_PCI_ATTR_W16: do_attr_w16(&m); break;
		case BUSC_PCI_ATTR_W32: do_attr_w32(&m); break;
		case BUSC_PCI_RESCAN: do_rescan_bus(&m); break;
		case PROC_EVENT: do_sig_handler(); break;
		default:
			printf("PCI: got message from %d, type %d\n",
				m.m_source, m.m_type);
			break;
		}
	}

	return 0;
}

/*===========================================================================*
 *				do_sig_handler                               *
 *===========================================================================*/
PRIVATE void do_sig_handler()
{
  sigset_t sigset;
  int sig;

  /* Try to obtain signal set from PM. */
  if (getsigset(&sigset) != 0) return;

  /* Check for known signals. */
  if (sigismember(&sigset, SIGTERM)) {
      exit(0);
  }
}

PRIVATE void do_init(mp)
message *mp;
{
	int i, r, empty;

#if DEBUG
	printf("pci_init: called by '%s'\n", mp->m3_ca1);
#endif
	empty= -1;
	for (i= 0; i<NR_DRIVERS; i++)
	{
		if (empty == -1 && names[i].tasknr == ANY)
			empty= i;
		if (strcmp(names[i].name, mp->m3_ca1) == 0)
			break;
	}
	if (i < NR_DRIVERS)
		pci_release(names[i].name);
	else
	{
		i= empty;
		strcpy(names[i].name, mp->m3_ca1);
	}
	names[i].tasknr= mp->m_source;

	mp->m_type= 0;
	r= send(mp->m_source, mp);
	if (r != 0)
		printf("do_init: unable to send to %d: %d\n", mp->m_source, r);
}

PRIVATE void do_first_dev(mp)
message *mp;
{
	int r, devind;
	u16_t vid, did;

	r= pci_first_dev(&devind, &vid, &did);
	if (r == 1)
	{
		mp->m1_i1= devind;
		mp->m1_i2= vid;
		mp->m1_i3= did;
	}
	mp->m_type= r;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_first_dev: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_next_dev(mp)
message *mp;
{
	int r, devind;
	u16_t vid, did;

	devind= mp->m1_i1;

	r= pci_next_dev(&devind, &vid, &did);
	if (r == 1)
	{
		mp->m1_i1= devind;
		mp->m1_i2= vid;
		mp->m1_i3= did;
	}
	mp->m_type= r;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_next_dev: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_find_dev(mp)
message *mp;
{
	int r, devind;
	u8_t bus, dev, func;

	bus= mp->m1_i1;
	dev= mp->m1_i2;
	func= mp->m1_i3;

	r= pci_find_dev(bus, dev, func, &devind);
	if (r == 1)
		mp->m1_i1= devind;
	mp->m_type= r;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_find_dev: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_ids(mp)
message *mp;
{
	int r, devind;
	u16_t vid, did;

	devind= mp->m1_i1;

	pci_ids(devind, &vid, &did);
	mp->m1_i1= vid;
	mp->m1_i2= did;
	mp->m_type= OK;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_ids: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_dev_name(mp)
message *mp;
{
	int r, name_len, len;
	u16_t vid, did;
	char *name_ptr, *name;

	vid= mp->m1_i1;
	did= mp->m1_i2;
	name_len= mp->m1_i3;
	name_ptr= mp->m1_p1;

	name= pci_dev_name(vid, did);
	if (name == NULL)
	{
		/* No name */
		r= ENOENT;
	}
	else
	{
		len= strlen(name)+1;
		if (len > name_len)
			len= name_len;
		r= sys_vircopy(SELF, D, (vir_bytes)name, mp->m_source, D,
			(vir_bytes)name_ptr, len);
	}

	mp->m_type= r;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_dev_name: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_slot_name(mp)
message *mp;
{
	int r, devind, name_len, len;
	char *name_ptr, *name;

	devind= mp->m1_i1;
	name_len= mp->m1_i2;
	name_ptr= mp->m1_p1;

	name= pci_slot_name(devind);

	len= strlen(name)+1;
	if (len > name_len)
		len= name_len;
	r= sys_vircopy(SELF, D, (vir_bytes)name, mp->m_source, D,
		(vir_bytes)name_ptr, len);

	mp->m_type= r;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_slot_name: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_reserve(mp)
message *mp;
{
	int i, r, devind;

	/* Find the name of the caller */
	for (i= 0; i<NR_DRIVERS; i++)
	{
		if (names[i].tasknr == mp->m_source)
			break;
	}
	if (i >= NR_DRIVERS)
	{
		printf("pci`do_reserve: task %d did not call pci_init\n",
			mp->m_source);
		return;
	}

	devind= mp->m1_i1;

	pci_reserve3(devind, mp->m_source, names[i].name);
	mp->m_type= OK;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_reserve: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_attr_r8(mp)
message *mp;
{
	int r, devind, port;
	u8_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;

	v= pci_attr_r8(devind, port);
	mp->m2_l1= v;
	mp->m_type= OK;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_r8: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_attr_r16(mp)
message *mp;
{
	int r, devind, port;
	u32_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;

	v= pci_attr_r16(devind, port);
	mp->m2_l1= v;
	mp->m_type= OK;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_r16: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_attr_r32(mp)
message *mp;
{
	int r, devind, port;
	u32_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;

	v= pci_attr_r32(devind, port);
	mp->m2_l1= v;
	mp->m_type= OK;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_r32: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_attr_w8(mp)
message *mp;
{
	int r, devind, port;
	u8_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;
	v= mp->m2_l1;

	pci_attr_w8(devind, port, v);
	mp->m_type= OK;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_w8: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_attr_w16(mp)
message *mp;
{
	int r, devind, port;
	u16_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;
	v= mp->m2_l1;

	pci_attr_w16(devind, port, v);
	mp->m_type= OK;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_w16: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_attr_w32(mp)
message *mp;
{
	int r, devind, port;
	u32_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;
	v= mp->m2_l1;

	pci_attr_w32(devind, port, v);
	mp->m_type= OK;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_w32: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

PRIVATE void do_rescan_bus(mp)
message *mp;
{
	int r, busnr;

	busnr= mp->m2_i1;

	pci_rescan_bus(busnr);
	mp->m_type= OK;
	r= send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_rescan_bus: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

