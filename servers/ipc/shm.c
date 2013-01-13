#include "inc.h"

#define MAX_SHM_NR 1024

struct shm_struct {
	key_t key;
	int id;
	struct shmid_ds shmid_ds;
	vir_bytes page;
	int vm_id;
};
static struct shm_struct shm_list[MAX_SHM_NR];
static int shm_list_nr = 0;

static struct shm_struct *shm_find_key(key_t key)
{
	int i;
	if (key == IPC_PRIVATE)
		return NULL;
	for (i = 0; i < shm_list_nr; i++)
		if (shm_list[i].key == key)
			return shm_list+i;
	return NULL;
}

static struct shm_struct *shm_find_id(int id)
{
	int i;
	for (i = 0; i < shm_list_nr; i++)
		if (shm_list[i].id == id)
			return shm_list+i;
	return NULL;
}

/*===========================================================================*
 *				do_shmget		     		     *
 *===========================================================================*/
int do_shmget(message *m)
{
	struct shm_struct *shm;
	long key, size, old_size;
	int flag;
	int id;

	key = m->SHMGET_KEY;
	old_size = size = m->SHMGET_SIZE;
	flag = m->SHMGET_FLAG;

	if ((shm = shm_find_key(key))) {
		if (!check_perm(&shm->shmid_ds.shm_perm, who_e, flag))
			return EACCES;
		if ((flag & IPC_CREAT) && (flag & IPC_EXCL))
			return EEXIST;
		if (size && shm->shmid_ds.shm_segsz < size)
			return EINVAL;
		id = shm->id;
	} else { /* no key found */
		if (!(flag & IPC_CREAT))
			return ENOENT;
		if (size <= 0)
			return EINVAL;
		/* round up to a multiple of PAGE_SIZE */
		if (size % PAGE_SIZE)
			size += PAGE_SIZE - size % PAGE_SIZE;
		if (size <= 0)
			return EINVAL;

		if (shm_list_nr == MAX_SHM_NR)
			return ENOMEM;
		/* TODO: shmmni should be changed... */
		if (identifier == SHMMNI)
			return ENOSPC;
		shm = &shm_list[shm_list_nr];
		memset(shm, 0, sizeof(struct shm_struct));
		shm->page = (vir_bytes) minix_mmap(0, size,
					PROT_READ|PROT_WRITE, MAP_ANON, -1, 0);
		if (shm->page == (vir_bytes) MAP_FAILED)
			return ENOMEM;
		shm->vm_id = vm_getphys(SELF_E, (void *) shm->page);
		memset((void *)shm->page, 0, size);

		shm->shmid_ds.shm_perm.cuid =
			shm->shmid_ds.shm_perm.uid = getnuid(who_e);
		shm->shmid_ds.shm_perm.cgid =
			shm->shmid_ds.shm_perm.gid = getngid(who_e);
		shm->shmid_ds.shm_perm.mode = flag & 0777;
		shm->shmid_ds.shm_segsz = old_size;
		shm->shmid_ds.shm_atime = 0;
		shm->shmid_ds.shm_dtime = 0;
		shm->shmid_ds.shm_ctime = time(NULL);
		shm->shmid_ds.shm_cpid = getnpid(who_e);
		shm->shmid_ds.shm_lpid = 0;
		shm->shmid_ds.shm_nattch = 0;
		shm->id = id = identifier++;
		shm->key = key;

		shm_list_nr++;
	}

	m->SHMGET_RETID = id;
	return OK;
}

/*===========================================================================*
 *				do_shmat		     		     *
 *===========================================================================*/
int do_shmat(message *m)
{
	int id, flag;
	vir_bytes addr;
	void *ret;
	struct shm_struct *shm;

	id = m->SHMAT_ID;
	addr = (vir_bytes) m->SHMAT_ADDR;
	flag = m->SHMAT_FLAG;

	if (addr && (addr % PAGE_SIZE)) {
		if (flag & SHM_RND)
			addr -= (addr % PAGE_SIZE);
		else
			return EINVAL;
	}

	if (!(shm = shm_find_id(id)))
		return EINVAL;

	if (flag & SHM_RDONLY)
		flag = 0444;
	else
		flag = 0666;
	if (!check_perm(&shm->shmid_ds.shm_perm, who_e, flag))
		return EACCES;

	ret = vm_remap(who_e, SELF_E, (void *)addr, (void *)shm->page,
			shm->shmid_ds.shm_segsz);
	if (ret == MAP_FAILED)
		return ENOMEM;

	shm->shmid_ds.shm_atime = time(NULL);
	shm->shmid_ds.shm_lpid = getnpid(who_e);
	/* nattach is updated lazily */

	m->SHMAT_RETADDR = (long) ret;
	return OK;
}

/*===========================================================================*
 *				update_refcount_and_destroy		     *
 *===========================================================================*/
void update_refcount_and_destroy(void)
{
	int i, j;

	for (i = 0, j = 0; i < shm_list_nr; i++) {
		u8_t rc;

		rc = vm_getrefcount(SELF_E, (void *) shm_list[i].page);
		if (rc == (u8_t) -1) {
			printf("IPC: can't find physical region.\n");
			continue;
		}
		shm_list[i].shmid_ds.shm_nattch = rc - 1;

		if (shm_list[i].shmid_ds.shm_nattch ||
			!(shm_list[i].shmid_ds.shm_perm.mode & SHM_DEST)) {
			if (i != j)
				shm_list[j] = shm_list[i];
			j++;
		} else {
			int size = shm_list[i].shmid_ds.shm_segsz;
			if (size % PAGE_SIZE)
				size += PAGE_SIZE - size % PAGE_SIZE;
			minix_munmap((void *)shm_list[i].page, size);
		}
	}
	shm_list_nr = j;
}

/*===========================================================================*
 *				do_shmdt		     		     *
 *===========================================================================*/
int do_shmdt(message *m)
{
	vir_bytes addr;
	phys_bytes vm_id;
	int i;

	addr = m->SHMDT_ADDR;

	if ((vm_id = vm_getphys(who_e, (void *) addr)) == 0)
		return EINVAL;

	for (i = 0; i < shm_list_nr; i++) {
		if (shm_list[i].vm_id == vm_id) {
			struct shm_struct *shm = &shm_list[i];

			shm->shmid_ds.shm_atime = time(NULL);
			shm->shmid_ds.shm_lpid = getnpid(who_e);
			/* nattch is updated lazily */

			vm_unmap(who_e, (void *) addr);
			break;
		}
	}
	if (i == shm_list_nr)
		printf("IPC: do_shmdt impossible error! could not find id %lu to unmap\n",
			vm_id);

	update_refcount_and_destroy();

	return OK;
}

/*===========================================================================*
 *				do_shmctl		     		     *
 *===========================================================================*/
int do_shmctl(message *m)
{
	int id = m->SHMCTL_ID;
	int cmd = m->SHMCTL_CMD;
	struct shmid_ds *ds = (struct shmid_ds *)m->SHMCTL_BUF;
	struct shmid_ds tmp_ds;
	struct shm_struct *shm = NULL;
	struct shminfo sinfo;
	struct shm_info s_info;
	uid_t uid;
	int r, i;

	if (cmd == IPC_STAT)
		update_refcount_and_destroy();

	if ((cmd == IPC_STAT ||
		cmd == IPC_SET ||
		cmd == IPC_RMID) &&
		!(shm = shm_find_id(id)))
		return EINVAL;

	switch (cmd) {
	case IPC_STAT:
		if (!ds)
			return EFAULT;
		/* check whether it has read permission */
		if (!check_perm(&shm->shmid_ds.shm_perm, who_e, 0444))
			return EACCES;
		r = sys_datacopy(SELF_E, (vir_bytes)&shm->shmid_ds,
			who_e, (vir_bytes)ds, sizeof(struct shmid_ds));
		if (r != OK)
			return EFAULT;
		break;
	case IPC_SET:
		uid = getnuid(who_e);
		if (uid != shm->shmid_ds.shm_perm.cuid &&
			uid != shm->shmid_ds.shm_perm.uid &&
			uid != 0)
			return EPERM;
		r = sys_datacopy(who_e, (vir_bytes)ds,
			SELF_E, (vir_bytes)&tmp_ds, sizeof(struct shmid_ds));
		if (r != OK)
			return EFAULT;
		shm->shmid_ds.shm_perm.uid = tmp_ds.shm_perm.uid;
		shm->shmid_ds.shm_perm.gid = tmp_ds.shm_perm.gid;
		shm->shmid_ds.shm_perm.mode &= ~0777;
		shm->shmid_ds.shm_perm.mode |= tmp_ds.shm_perm.mode & 0666;
		shm->shmid_ds.shm_ctime = time(NULL);
		break;
	case IPC_RMID:
		uid = getnuid(who_e);
		if (uid != shm->shmid_ds.shm_perm.cuid &&
			uid != shm->shmid_ds.shm_perm.uid &&
			uid != 0)
			return EPERM;
		shm->shmid_ds.shm_perm.mode |= SHM_DEST;
		/* destroy if possible */
		update_refcount_and_destroy();
		break;
	case IPC_INFO:
		if (!ds)
			return EFAULT;
		sinfo.shmmax = (unsigned long) -1;
		sinfo.shmmin = 1;
		sinfo.shmmni = MAX_SHM_NR;
		sinfo.shmseg = (unsigned long) -1;
		sinfo.shmall = (unsigned long) -1;
		r = sys_datacopy(SELF_E, (vir_bytes)&sinfo,
			who_e, (vir_bytes)ds, sizeof(struct shminfo));
		if (r != OK)
			return EFAULT;
		m->SHMCTL_RET = shm_list_nr - 1;
		if (m->SHMCTL_RET < 0)
			m->SHMCTL_RET = 0;
		break;
	case SHM_INFO:
		if (!ds)
			return EFAULT;
		s_info.used_ids = shm_list_nr;
		s_info.shm_tot = 0;
		for (i = 0; i < shm_list_nr; i++)
			s_info.shm_tot +=
				shm_list[i].shmid_ds.shm_segsz/PAGE_SIZE;
		s_info.shm_rss = s_info.shm_tot;
		s_info.shm_swp = 0;
		s_info.swap_attempts = 0;
		s_info.swap_successes = 0;
		r = sys_datacopy(SELF_E, (vir_bytes)&s_info,
			who_e, (vir_bytes)ds, sizeof(struct shm_info));
		if (r != OK)
			return EFAULT;
		m->SHMCTL_RET = shm_list_nr - 1;
		if (m->SHMCTL_RET < 0)
			m->SHMCTL_RET = 0;
		break;
	case SHM_STAT:
		if (id < 0 || id >= shm_list_nr)
			return EINVAL;
		shm = &shm_list[id];
		r = sys_datacopy(SELF_E, (vir_bytes)&shm->shmid_ds,
			who_e, (vir_bytes)ds, sizeof(struct shmid_ds));
		if (r != OK)
			return EFAULT;
		m->SHMCTL_RET = shm->id;
		break;
	default:
		return EINVAL;
	}
	return OK;
}

#if 0
static void list_shm_ds(void)
{
	int i;
	printf("key\tid\tpage\n");
	for (i = 0; i < shm_list_nr; i++)
		printf("%ld\t%d\t%lx\n",
			shm_list[i].key,
			shm_list[i].id,
			shm_list[i].page);
}
#endif

/*===========================================================================*
 *				is_shm_nil		     		     *
 *===========================================================================*/
int is_shm_nil(void)
{
	return (shm_list_nr == 0);
}
