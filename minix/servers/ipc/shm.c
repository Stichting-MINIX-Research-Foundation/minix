#include "inc.h"

/* Private shm_perm.mode flags, synchronized with NetBSD kernel values */
#define SHM_ALLOC	0x0800	/* slot is in use (SHMSEG_ALLOCATED) */

struct shm_struct {
	struct shmid_ds shmid_ds;
	vir_bytes page;
	phys_bytes vm_id;
};
static struct shm_struct shm_list[SHMMNI];
static unsigned int shm_list_nr = 0; /* highest in-use slot number plus one */

static struct shm_struct *
shm_find_key(key_t key)
{
	unsigned int i;

	if (key == IPC_PRIVATE)
		return NULL;

	for (i = 0; i < shm_list_nr; i++) {
		if (!(shm_list[i].shmid_ds.shm_perm.mode & SHM_ALLOC))
			continue;
		if (shm_list[i].shmid_ds.shm_perm._key == key)
			return &shm_list[i];
	}

	return NULL;
}

static struct shm_struct *
shm_find_id(int id)
{
	struct shm_struct *shm;
	unsigned int i;

	i = IPCID_TO_IX(id);
	if (i >= shm_list_nr)
		return NULL;

	shm = &shm_list[i];
	if (!(shm->shmid_ds.shm_perm.mode & SHM_ALLOC))
		return NULL;
	if (shm->shmid_ds.shm_perm._seq != IPCID_TO_SEQ(id))
		return NULL;
	return shm;
}

int
do_shmget(message * m)
{
	struct shm_struct *shm;
	unsigned int i, seq;
	key_t key;
	size_t size, old_size;
	int flag;
	void *page;

	key = m->m_lc_ipc_shmget.key;
	old_size = size = m->m_lc_ipc_shmget.size;
	flag = m->m_lc_ipc_shmget.flag;

	if ((shm = shm_find_key(key)) != NULL) {
		if (!check_perm(&shm->shmid_ds.shm_perm, m->m_source, flag))
			return EACCES;
		if ((flag & IPC_CREAT) && (flag & IPC_EXCL))
			return EEXIST;
		if (size && shm->shmid_ds.shm_segsz < size)
			return EINVAL;
		i = shm - shm_list;
	} else { /* no key found */
		if (!(flag & IPC_CREAT))
			return ENOENT;
		if (size <= 0)
			return EINVAL;
		size = roundup(size, PAGE_SIZE);
		if (size <= 0)
			return EINVAL;

		/* Find a free entry. */
		for (i = 0; i < __arraycount(shm_list); i++)
			if (!(shm_list[i].shmid_ds.shm_perm.mode & SHM_ALLOC))
				break;
		if (i == __arraycount(shm_list))
			return ENOSPC;

		/*
		 * Allocate memory to share.  For now, we store the page
		 * reference as a numerical value so as to avoid issues with
		 * live update.  TODO: a proper solution.
		 */
		page = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
		if (page == MAP_FAILED)
			return ENOMEM;
		memset(page, 0, size);

		/* Initialize the entry. */
		shm = &shm_list[i];
		seq = shm->shmid_ds.shm_perm._seq;
		memset(shm, 0, sizeof(*shm));

		shm->shmid_ds.shm_perm._key = key;
		shm->shmid_ds.shm_perm.cuid =
			shm->shmid_ds.shm_perm.uid = getnuid(m->m_source);
		shm->shmid_ds.shm_perm.cgid =
			shm->shmid_ds.shm_perm.gid = getngid(m->m_source);
		shm->shmid_ds.shm_perm.mode = SHM_ALLOC | (flag & ACCESSPERMS);
		shm->shmid_ds.shm_perm._seq = (seq + 1) & 0x7fff;
		shm->shmid_ds.shm_segsz = old_size;
		shm->shmid_ds.shm_atime = 0;
		shm->shmid_ds.shm_dtime = 0;
		shm->shmid_ds.shm_ctime = clock_time(NULL);
		shm->shmid_ds.shm_cpid = getnpid(m->m_source);
		shm->shmid_ds.shm_lpid = 0;
		shm->shmid_ds.shm_nattch = 0;
		shm->page = (vir_bytes)page;
		shm->vm_id = vm_getphys(sef_self(), page);

		assert(i <= shm_list_nr);
		if (i == shm_list_nr)
			shm_list_nr++;
	}

	m->m_lc_ipc_shmget.retid = IXSEQ_TO_IPCID(i, shm->shmid_ds.shm_perm);
	return OK;
}

int
do_shmat(message * m)
{
	int id, flag, mask;
	vir_bytes addr;
	void *ret;
	struct shm_struct *shm;

	id = m->m_lc_ipc_shmat.id;
	addr = (vir_bytes)m->m_lc_ipc_shmat.addr;
	flag = m->m_lc_ipc_shmat.flag;

	if (addr % PAGE_SIZE) {
		if (flag & SHM_RND)
			addr -= addr % PAGE_SIZE;
		else
			return EINVAL;
	}

	if ((shm = shm_find_id(id)) == NULL)
		return EINVAL;

	mask = 0;
	if (flag & SHM_RDONLY)
		mask = IPC_R;
	else
		mask = IPC_R | IPC_W;
	if (!check_perm(&shm->shmid_ds.shm_perm, m->m_source, mask))
		return EACCES;

	ret = vm_remap(m->m_source, sef_self(), (void *)addr,
	    (void *)shm->page, shm->shmid_ds.shm_segsz);
	if (ret == MAP_FAILED)
		return ENOMEM;

	shm->shmid_ds.shm_atime = clock_time(NULL);
	shm->shmid_ds.shm_lpid = getnpid(m->m_source);
	/* nattch is updated lazily */

	m->m_lc_ipc_shmat.retaddr = ret;
	return OK;
}

void
update_refcount_and_destroy(void)
{
	u8_t rc;
	unsigned int i;

	for (i = 0; i < shm_list_nr; i++) {
		if (!(shm_list[i].shmid_ds.shm_perm.mode & SHM_ALLOC))
			continue;

		rc = vm_getrefcount(sef_self(), (void *)shm_list[i].page);
		if (rc == (u8_t)-1) {
			printf("IPC: can't find physical region.\n");
			continue;
		}
		shm_list[i].shmid_ds.shm_nattch = rc - 1;

		if (shm_list[i].shmid_ds.shm_nattch == 0 &&
		    (shm_list[i].shmid_ds.shm_perm.mode & SHM_DEST)) {
			munmap((void *)shm_list[i].page,
			    roundup(shm_list[i].shmid_ds.shm_segsz,
			    PAGE_SIZE));
			/* Mark the entry as free. */
			shm_list[i].shmid_ds.shm_perm.mode &= ~SHM_ALLOC;
		}
	}

	/*
	 * Now that we may have removed an arbitrary set of slots, ensure that
	 * shm_list_nr again equals the highest in-use slot number plus one.
	 */
	while (shm_list_nr > 0 &&
	    !(shm_list[shm_list_nr - 1].shmid_ds.shm_perm.mode & SHM_ALLOC))
		shm_list_nr--;
}

int
do_shmdt(message * m)
{
	struct shm_struct *shm;
	vir_bytes addr;
	phys_bytes vm_id;
	unsigned int i;

	addr = (vir_bytes)m->m_lc_ipc_shmdt.addr;

	if ((vm_id = vm_getphys(m->m_source, (void *)addr)) == 0)
		return EINVAL;

	for (i = 0; i < shm_list_nr; i++) {
		shm = &shm_list[i];

		if (!(shm->shmid_ds.shm_perm.mode & SHM_ALLOC))
			continue;

		if (shm->vm_id == vm_id) {
			shm->shmid_ds.shm_atime = clock_time(NULL);
			shm->shmid_ds.shm_lpid = getnpid(m->m_source);
			/* nattch is updated lazily */

			vm_unmap(m->m_source, (void *)addr);
			break;
		}
	}
	if (i == shm_list_nr)
		printf("IPC: do_shmdt: ID %lu not found\n", vm_id);

	update_refcount_and_destroy();

	return OK;
}

/*
 * Fill a shminfo structure with actual information.
 */
static void
fill_shminfo(struct shminfo * sinfo)
{

	memset(sinfo, 0, sizeof(*sinfo));

	sinfo->shmmax = (unsigned long)-1;
	sinfo->shmmin = 1;
	sinfo->shmmni = __arraycount(shm_list);
	sinfo->shmseg = (unsigned long)-1;
	sinfo->shmall = (unsigned long)-1;
}

int
do_shmctl(message * m)
{
	struct shmid_ds tmp_ds;
	struct shm_struct *shm;
	struct shminfo sinfo;
	struct shm_info s_info;
	vir_bytes buf;
	unsigned int i;
	uid_t uid;
	int r, id, cmd;

	id = m->m_lc_ipc_shmctl.id;
	cmd = m->m_lc_ipc_shmctl.cmd;
	buf = (vir_bytes)m->m_lc_ipc_shmctl.buf;

	/*
	 * For stat calls, sure that all information is up-to-date.  Since this
	 * may free the slot, do this before mapping from ID to slot below.
	 */
	if (cmd == IPC_STAT || cmd == SHM_STAT)
		update_refcount_and_destroy();

	switch (cmd) {
	case IPC_INFO:
	case SHM_INFO:
		shm = NULL;
		break;
	case SHM_STAT:
		if (id < 0 || (unsigned int)id >= shm_list_nr)
			return EINVAL;
		shm = &shm_list[id];
		if (!(shm->shmid_ds.shm_perm.mode & SHM_ALLOC))
			return EINVAL;
		break;
	default:
		if ((shm = shm_find_id(id)) == NULL)
			return EINVAL;
		break;
	}

	switch (cmd) {
	case IPC_STAT:
	case SHM_STAT:
		/* Check whether the caller has read permission. */
		if (!check_perm(&shm->shmid_ds.shm_perm, m->m_source, IPC_R))
			return EACCES;
		if ((r = sys_datacopy(SELF, (vir_bytes)&shm->shmid_ds,
		    m->m_source, buf, sizeof(shm->shmid_ds))) != OK)
			return r;
		if (cmd == SHM_STAT)
			m->m_lc_ipc_shmctl.ret =
			    IXSEQ_TO_IPCID(id, shm->shmid_ds.shm_perm);
		break;
	case IPC_SET:
		uid = getnuid(m->m_source);
		if (uid != shm->shmid_ds.shm_perm.cuid &&
		    uid != shm->shmid_ds.shm_perm.uid && uid != 0)
			return EPERM;
		if ((r = sys_datacopy(m->m_source, buf, SELF,
		    (vir_bytes)&tmp_ds, sizeof(tmp_ds))) != OK)
			return r;
		shm->shmid_ds.shm_perm.uid = tmp_ds.shm_perm.uid;
		shm->shmid_ds.shm_perm.gid = tmp_ds.shm_perm.gid;
		shm->shmid_ds.shm_perm.mode &= ~ACCESSPERMS;
		shm->shmid_ds.shm_perm.mode |=
		    tmp_ds.shm_perm.mode & ACCESSPERMS;
		shm->shmid_ds.shm_ctime = clock_time(NULL);
		break;
	case IPC_RMID:
		uid = getnuid(m->m_source);
		if (uid != shm->shmid_ds.shm_perm.cuid &&
		    uid != shm->shmid_ds.shm_perm.uid && uid != 0)
			return EPERM;
		shm->shmid_ds.shm_perm.mode |= SHM_DEST;
		/* Destroy if possible. */
		update_refcount_and_destroy();
		break;
	case IPC_INFO:
		fill_shminfo(&sinfo);
		if ((r = sys_datacopy(SELF, (vir_bytes)&sinfo, m->m_source,
		    buf, sizeof(sinfo))) != OK)
			return r;
		if (shm_list_nr > 0)
			m->m_lc_ipc_shmctl.ret = shm_list_nr - 1;
		else
			m->m_lc_ipc_shmctl.ret = 0;
		break;
	case SHM_INFO:
		memset(&s_info, 0, sizeof(s_info));
		s_info.used_ids = shm_list_nr;
		s_info.shm_tot = 0;
		for (i = 0; i < shm_list_nr; i++)
			s_info.shm_tot +=
			    shm_list[i].shmid_ds.shm_segsz / PAGE_SIZE;
		s_info.shm_rss = s_info.shm_tot;
		s_info.shm_swp = 0;
		s_info.swap_attempts = 0;
		s_info.swap_successes = 0;
		if ((r = sys_datacopy(SELF, (vir_bytes)&s_info, m->m_source,
		    buf, sizeof(s_info))) != OK)
			return r;
		if (shm_list_nr > 0)
			m->m_lc_ipc_shmctl.ret = shm_list_nr - 1;
		else
			m->m_lc_ipc_shmctl.ret = 0;
		break;
	default:
		return EINVAL;
	}
	return OK;
}

/*
 * Return shared memory information for a remote MIB call on the sysvipc_info
 * node in the kern.ipc subtree.  The particular semantics of this call are
 * tightly coupled to the implementation of the ipcs(1) userland utility.
 */
ssize_t
get_shm_mib_info(struct rmib_oldp * oldp)
{
	struct shm_sysctl_info shmsi;
	struct shmid_ds *shmds;
	unsigned int i;
	ssize_t r, off;

	off = 0;

	fill_shminfo(&shmsi.shminfo);

	/*
	 * As a hackish exception, the requested size may imply that just
	 * general information is to be returned, without throwing an ENOMEM
	 * error because there is no space for full output.
	 */
	if (rmib_getoldlen(oldp) == sizeof(shmsi.shminfo))
		return rmib_copyout(oldp, 0, &shmsi.shminfo,
		    sizeof(shmsi.shminfo));

	/*
	 * ipcs(1) blindly expects the returned array to be of size
	 * shminfo.shmmni, using the SHMSEG_ALLOCATED (aka SHM_ALLOC) mode flag
	 * to see whether each entry is valid.  If we return a smaller size,
	 * ipcs(1) will access arbitrary memory.
	 */
	assert(shmsi.shminfo.shmmni > 0);

	if (oldp == NULL)
		return sizeof(shmsi) + sizeof(shmsi.shmids[0]) *
		    (shmsi.shminfo.shmmni - 1);

	/*
	 * Copy out entries one by one.  For the first entry, copy out the
	 * entire "shmsi" structure.  For subsequent entries, reuse the single
	 * embedded 'shmids' element of "shmsi" and copy out only that element.
	 */
	for (i = 0; i < shmsi.shminfo.shmmni; i++) {
		shmds = &shm_list[i].shmid_ds;

		memset(&shmsi.shmids[0], 0, sizeof(shmsi.shmids[0]));
		if (i < shm_list_nr && (shmds->shm_perm.mode & SHM_ALLOC)) {
			prepare_mib_perm(&shmsi.shmids[0].shm_perm,
			    &shmds->shm_perm);
			shmsi.shmids[0].shm_segsz = shmds->shm_segsz;
			shmsi.shmids[0].shm_lpid = shmds->shm_lpid;
			shmsi.shmids[0].shm_cpid = shmds->shm_cpid;
			shmsi.shmids[0].shm_atime = shmds->shm_atime;
			shmsi.shmids[0].shm_dtime = shmds->shm_dtime;
			shmsi.shmids[0].shm_ctime = shmds->shm_ctime;
			shmsi.shmids[0].shm_nattch = shmds->shm_nattch;
		}

		if (off == 0)
			r = rmib_copyout(oldp, off, &shmsi, sizeof(shmsi));
		else
			r = rmib_copyout(oldp, off, &shmsi.shmids[0],
			    sizeof(shmsi.shmids[0]));

		if (r < 0)
			return r;
		off += r;
	}

	return off;
}

#if 0
static void
list_shm_ds(void)
{
	unsigned int i;

	printf("key\tid\tpage\n");
	for (i = 0; i < shm_list_nr; i++) {
		if (!(shm_list[i].shmid_ds.shm_perm.mode & SHM_ALLOC))
			continue;
		printf("%ld\t%d\t%lx\n",
		    shm_list[i].shmid_ds.shm_perm._key,
		    IXSEQ_TO_IPCID(i, shm_list[i].shmid_ds.shm_perm),
		    shm_list[i].page);
	}
}
#endif

int
is_shm_nil(void)
{

	return (shm_list_nr == 0);
}
