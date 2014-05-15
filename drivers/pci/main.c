/*
main.c
*/

#include "pci.h"

struct pci_acl pci_acl[NR_DRIVERS];

static void do_init(message *mp);
static void do_first_dev(message *mp);
static void do_next_dev(message *mp);
static void do_find_dev(message *mp);
static void do_ids(message *mp);
static void do_dev_name_s(message *mp);
static void do_slot_name_s(message *mp);
static void do_set_acl(message *mp);
static void do_del_acl(message *mp);
static void do_reserve(message *mp);
static void do_attr_r8(message *mp);
static void do_attr_r16(message *mp);
static void do_attr_r32(message *mp);
static void do_attr_w8(message *mp);
static void do_attr_w16(message *mp);
static void do_attr_w32(message *mp);
static void do_get_bar(message *mp);
static void do_rescan_bus(message *mp);
static void reply(message *mp, int result);
static struct rs_pci *find_acl(int endpoint);

extern int debug;

/* SEF functions and variables. */
static void sef_local_startup(void);

int main(void)
{
	int r;
	message m;
	int ipc_status;

	/* SEF local startup. */
	sef_local_startup();

	for(;;)
	{
		r= driver_receive(ANY, &m, &ipc_status);
		if (r < 0)
		{
			printf("PCI: driver_receive failed: %d\n", r);
			break;
		}

		if (is_ipc_notify(ipc_status)) {
			printf("PCI: got notify from %d\n", m.m_source);

			/* done, get a new message */
			continue;
		}

		switch(m.m_type)
		{
		case BUSC_PCI_INIT: do_init(&m); break;
		case BUSC_PCI_FIRST_DEV: do_first_dev(&m); break;
		case BUSC_PCI_NEXT_DEV: do_next_dev(&m); break;
		case BUSC_PCI_FIND_DEV: do_find_dev(&m); break;
		case BUSC_PCI_IDS: do_ids(&m); break;
		case BUSC_PCI_RESERVE: do_reserve(&m); break;
		case BUSC_PCI_ATTR_R8: do_attr_r8(&m); break;
		case BUSC_PCI_ATTR_R16: do_attr_r16(&m); break;
		case BUSC_PCI_ATTR_R32: do_attr_r32(&m); break;
		case BUSC_PCI_ATTR_W8: do_attr_w8(&m); break;
		case BUSC_PCI_ATTR_W16: do_attr_w16(&m); break;
		case BUSC_PCI_ATTR_W32: do_attr_w32(&m); break;
		case BUSC_PCI_RESCAN: do_rescan_bus(&m); break;
		case BUSC_PCI_DEV_NAME_S: do_dev_name_s(&m); break;
		case BUSC_PCI_SLOT_NAME_S: do_slot_name_s(&m); break;
		case BUSC_PCI_SET_ACL: do_set_acl(&m); break;
		case BUSC_PCI_DEL_ACL: do_del_acl(&m); break;
		case BUSC_PCI_GET_BAR: do_get_bar(&m); break;
		default:
			printf("PCI: got message from %d, type %d\n",
				m.m_source, m.m_type);
			break;
		}
	}

	return 0;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* Register live update callbacks. */
  sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);

  /* Let SEF perform startup. */
  sef_startup();
}

static void do_init(mp)
message *mp;
{
	int r;

#if DEBUG
	printf("PCI: do_init: called by '%d'\n", mp->m_source);
#endif

	mp->m_type= 0;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
		printf("PCI: do_init: unable to send to %d: %d\n",
			mp->m_source, r);
}

static void do_first_dev(message *mp)
{
	int r, devind;
	u16_t vid, did;
	struct rs_pci *aclp;

	aclp= find_acl(mp->m_source);

	if (!aclp && debug)
		printf("PCI: do_first_dev: no acl for caller %d\n",
			mp->m_source);

	r= pci_first_dev_a(aclp, &devind, &vid, &did);
	if (r == 1)
	{
		mp->m1_i1= devind;
		mp->m1_i2= vid;
		mp->m1_i3= did;
	}
	mp->m_type= r;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("PCI: do_first_dev: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_next_dev(message *mp)
{
	int r, devind;
	u16_t vid, did;
	struct rs_pci *aclp;

	devind= mp->m1_i1;
	aclp= find_acl(mp->m_source);

	r= pci_next_dev_a(aclp, &devind, &vid, &did);
	if (r == 1)
	{
		mp->m1_i1= devind;
		mp->m1_i2= vid;
		mp->m1_i3= did;
	}
	mp->m_type= r;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("PCI: do_next_dev: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_find_dev(mp)
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
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("PCI: do_find_dev: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_ids(mp)
message *mp;
{
	int r, devind;
	u16_t vid, did;

	devind= mp->m1_i1;

	r= pci_ids_s(devind, &vid, &did);
	if (r != OK)
	{
		printf("pci:do_ids: failed for devind %d: %d\n",
			devind, r);
	}

	mp->m1_i1= vid;
	mp->m1_i2= did;
	mp->m_type= r;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("PCI: do_ids: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_dev_name_s(mp)
message *mp;
{
	int r, name_len, len;
	u16_t vid, did;
	cp_grant_id_t name_gid;
	char *name;

	vid= mp->m7_i1;
	did= mp->m7_i2;
	name_len= mp->m7_i3;
	name_gid= mp->m7_i4;

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
		r= sys_safecopyto(mp->m_source, name_gid, 0, (vir_bytes)name,
			len);
	}

	mp->m_type= r;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("PCI: do_dev_name: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_slot_name_s(mp)
message *mp;
{
	int r, devind, name_len, len;
	cp_grant_id_t gid;
	char *name;

	devind= mp->m1_i1;
	name_len= mp->m1_i2;
	gid= mp->m1_i3;

	r= pci_slot_name_s(devind, &name);
	if (r != OK)
	{
		printf("pci:do_slot_name_s: failed for devind %d: %d\n",
			devind, r);
	}

	if (r == OK)
	{
		len= strlen(name)+1;
		if (len > name_len)
			len= name_len;
		r= sys_safecopyto(mp->m_source, gid, 0,
			(vir_bytes)name, len);
	}

	mp->m_type= r;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("PCI: do_slot_name: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_set_acl(mp)
message *mp;
{
	int i, r, gid;

	if (mp->m_source != RS_PROC_NR)
	{
		printf("PCI: do_set_acl: not from RS\n");
		reply(mp, EPERM);
		return;
	}

	for (i= 0; i<NR_DRIVERS; i++)
	{
		if (!pci_acl[i].inuse)
			break;
	}
	if (i >= NR_DRIVERS)
	{
		printf("PCI: do_set_acl: table is full\n");
		reply(mp, ENOMEM);
		return;
	}

	gid= mp->m1_i1;

	r= sys_safecopyfrom(mp->m_source, gid, 0, (vir_bytes)&pci_acl[i].acl,
		sizeof(pci_acl[i].acl));
	if (r != OK)
	{
		printf("PCI: do_set_acl: safecopyfrom failed\n");
		reply(mp, r);
		return;
	}
	pci_acl[i].inuse= 1;
	if(debug)
	  printf("PCI: do_acl: setting ACL for %d ('%s') at entry %d\n",
		pci_acl[i].acl.rsp_endpoint, pci_acl[i].acl.rsp_label,
		i);

	reply(mp, OK);
}

static void do_del_acl(message *mp)
{
	int i, proc_nr;

	if (mp->m_source != RS_PROC_NR)
	{
		printf("do_del_acl: not from RS\n");
		reply(mp, EPERM);
		return;
	}

	proc_nr= mp->m1_i1;

	for (i= 0; i<NR_DRIVERS; i++)
	{
		if (!pci_acl[i].inuse)
			continue;
		if (pci_acl[i].acl.rsp_endpoint == proc_nr)
			break;
	}

	if (i >= NR_DRIVERS)
	{
		printf("do_del_acl: nothing found for %d\n", proc_nr);
		reply(mp, EINVAL);
		return;
	}

	pci_acl[i].inuse= 0;
#if 0
	printf("do_acl: deleting ACL for %d ('%s') at entry %d\n",
		pci_acl[i].acl.rsp_endpoint, pci_acl[i].acl.rsp_label, i);
#endif

	/* Also release all devices held by this process */
	pci_release(proc_nr);

	reply(mp, OK);
}

static void do_reserve(message *mp)
{
	struct rs_pci *aclp;
	int r, devind;

	devind= mp->m1_i1;

	aclp= find_acl(mp->m_source);

	mp->m_type= pci_reserve_a(devind, mp->m_source, aclp);
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_reserve: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_attr_r8(mp)
message *mp;
{
	int r, devind, port;
	u8_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;

	r= pci_attr_r8_s(devind, port, &v);
	if (r != OK)
	{
		printf(
		"pci:do_attr_r8: pci_attr_r8_s(%d, %d, ...) failed: %d\n",
			devind, port, r);
	}
	mp->m2_l1= v;
	mp->m_type= r;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_r8: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_attr_r16(mp)
message *mp;
{
	int r, devind, port;
	u32_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;

	v= pci_attr_r16(devind, port);
	mp->m2_l1= v;
	mp->m_type= OK;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_r16: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_attr_r32(mp)
message *mp;
{
	int r, devind, port;
	u32_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;

	r= pci_attr_r32_s(devind, port, &v);
	if (r != OK)
	{
		printf(
		"pci:do_attr_r32: pci_attr_r32_s(%d, %d, ...) failed: %d\n",
			devind, port, r);
	}
	mp->m2_l1= v;
	mp->m_type= OK;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_r32: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_attr_w8(mp)
message *mp;
{
	int r, devind, port;
	u8_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;
	v= mp->m2_l1;

	pci_attr_w8(devind, port, v);
	mp->m_type= OK;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_w8: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_attr_w16(mp)
message *mp;
{
	int r, devind, port;
	u16_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;
	v= mp->m2_l1;

	pci_attr_w16(devind, port, v);
	mp->m_type= OK;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_w16: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_attr_w32(mp)
message *mp;
{
	int r, devind, port;
	u32_t v;

	devind= mp->m2_i1;
	port= mp->m2_i2;
	v= mp->m2_l1;

	pci_attr_w32(devind, port, v);
	mp->m_type= OK;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_attr_w32: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_get_bar(mp)
message *mp;
{
	int r, devind, port, ioflag;
	u32_t base, size;

	devind= mp->m_lsys_pci_busc_get_bar.devind;
	port= mp->m_lsys_pci_busc_get_bar.port;

	mp->m_type= pci_get_bar_s(devind, port, &base, &size, &ioflag);

	if (mp->m_type == OK)
	{
		mp->m_pci_lsys_busc_get_bar.base= base;
		mp->m_pci_lsys_busc_get_bar.size= size;
		mp->m_pci_lsys_busc_get_bar.flags= ioflag;
	}

	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_get_bar: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}

static void do_rescan_bus(mp)
message *mp;
{
	int r, busnr;

	busnr= mp->m2_i1;

	pci_rescan_bus(busnr);
	mp->m_type= OK;
	r= ipc_send(mp->m_source, mp);
	if (r != 0)
	{
		printf("do_rescan_bus: unable to send to %d: %d\n",
			mp->m_source, r);
	}
}


static void reply(mp, result)
message *mp;
int result;
{
	int r;
	message m;

	m.m_type= result;
	r= ipc_send(mp->m_source, &m);
	if (r != 0)
		printf("reply: unable to send to %d: %d\n", mp->m_source, r);
}


static struct rs_pci *find_acl(endpoint)
int endpoint;
{
	int i;

	/* Find ACL entry for caller */
	for (i= 0; i<NR_DRIVERS; i++)
	{
		if (!pci_acl[i].inuse)
			continue;
		if (pci_acl[i].acl.rsp_endpoint == endpoint)
			return &pci_acl[i].acl;
	}
	return NULL;
}
