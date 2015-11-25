#include "inc.h"

struct waiting {
	endpoint_t who;			/* who is waiting */
	int val;			/* value he/she is waiting for */
};

struct semaphore {
	unsigned short semval;		/* semaphore value */
	unsigned short semzcnt;		/* # waiting for zero */
	unsigned short semncnt;		/* # waiting for increase */
	struct waiting *zlist;		/* process waiting for zero */
	struct waiting *nlist;		/* process waiting for increase */
	pid_t sempid;			/* process that did last op */
};

struct sem_struct {
	struct semid_ds semid_ds;
	struct semaphore sems[SEMMSL];
};

static struct sem_struct sem_list[SEMMNI];
static unsigned int sem_list_nr = 0; /* highest in-use slot number plus one */

static struct sem_struct *
sem_find_key(key_t key)
{
	unsigned int i;

	if (key == IPC_PRIVATE)
		return NULL;

	for (i = 0; i < sem_list_nr; i++) {
		if (!(sem_list[i].semid_ds.sem_perm.mode & SEM_ALLOC))
			continue;
		if (sem_list[i].semid_ds.sem_perm._key == key)
			return &sem_list[i];
	}

	return NULL;
}

static struct sem_struct *
sem_find_id(int id)
{
	struct sem_struct *sem;
	unsigned int i;

	i = IPCID_TO_IX(id);
	if (i >= sem_list_nr)
		return NULL;

	sem = &sem_list[i];
	if (!(sem->semid_ds.sem_perm.mode & SEM_ALLOC))
		return NULL;
	if (sem->semid_ds.sem_perm._seq != IPCID_TO_SEQ(id))
		return NULL;
	return sem;
}

int
do_semget(message * m)
{
	struct sem_struct *sem;
	unsigned int i, seq;
	key_t key;
	int nsems, flag;

	key = m->m_lc_ipc_semget.key;
	nsems = m->m_lc_ipc_semget.nr;
	flag = m->m_lc_ipc_semget.flag;

	if ((sem = sem_find_key(key)) != NULL) {
		if ((flag & IPC_CREAT) && (flag & IPC_EXCL))
			return EEXIST;
		if (!check_perm(&sem->semid_ds.sem_perm, m->m_source, flag))
			return EACCES;
		if (nsems > sem->semid_ds.sem_nsems)
			return EINVAL;
		i = sem - sem_list;
	} else {
		if (!(flag & IPC_CREAT))
			return ENOENT;
		if (nsems < 0 || nsems >= SEMMSL)
			return EINVAL;

		/* Find a free entry. */
		for (i = 0; i < __arraycount(sem_list); i++)
			if (!(sem_list[i].semid_ds.sem_perm.mode & SEM_ALLOC))
				break;
		if (i == __arraycount(sem_list))
			return ENOSPC;

		/* Initialize the entry. */
		sem = &sem_list[i];
		seq = sem->semid_ds.sem_perm._seq;
		memset(sem, 0, sizeof(*sem));
		sem->semid_ds.sem_perm._key = key;
		sem->semid_ds.sem_perm.cuid =
		    sem->semid_ds.sem_perm.uid = getnuid(m->m_source);
		sem->semid_ds.sem_perm.cgid =
		    sem->semid_ds.sem_perm.gid = getngid(m->m_source);
		sem->semid_ds.sem_perm.mode = SEM_ALLOC | (flag & ACCESSPERMS);
		sem->semid_ds.sem_perm._seq = (seq + 1) & 0x7fff;
		sem->semid_ds.sem_nsems = nsems;
		sem->semid_ds.sem_otime = 0;
		sem->semid_ds.sem_ctime = clock_time(NULL);

		assert(i <= sem_list_nr);
		if (i == sem_list_nr)
			sem_list_nr++;
	}

	m->m_lc_ipc_semget.retid = IXSEQ_TO_IPCID(i, sem->semid_ds.sem_perm);
	return OK;
}

static void
send_reply(endpoint_t who, int ret)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_type = ret;

	ipc_sendnb(who, &m);
}

static void
remove_semaphore(struct sem_struct * sem)
{
	int i, j, nr;
	struct semaphore *semaphore;

	nr = sem->semid_ds.sem_nsems;

	/* Deal with processes waiting for this semaphore set. */
	for (i = 0; i < nr; i++) {
		semaphore = &sem->sems[i];

		for (j = 0; j < semaphore->semzcnt; j++)
			send_reply(semaphore->zlist[j].who, EIDRM);
		for (j = 0; j < semaphore->semncnt; j++)
			send_reply(semaphore->nlist[j].who, EIDRM);

		if (semaphore->zlist != NULL) {
			free(semaphore->zlist);
			semaphore->zlist = NULL;
		}
		if (semaphore->nlist != NULL) {
			free(semaphore->nlist);
			semaphore->nlist = NULL;
		}
	}

	/* Mark the entry as free. */
	sem->semid_ds.sem_perm.mode &= ~SEM_ALLOC;

	/*
	 * This may have been the last in-use slot in the list.  Ensure that
	 * sem_list_nr again equals the highest in-use slot number plus one.
	 */
	while (sem_list_nr > 0 &&
	    !(sem_list[sem_list_nr - 1].semid_ds.sem_perm.mode & SEM_ALLOC))
		sem_list_nr--;
}

#if 0
static void
show_semaphore(void)
{
	unsigned int i;
	int j, k, nr;

	for (i = 0; i < sem_list_nr; i++) {
		if (!(sem_list[i].semid_ds.sem_perm.mode & SEM_ALLOC))
			continue;

		nr = sem_list[i].semid_ds.sem_nsems;

		printf("===== [%d] =====\n", i);
		for (j = 0; j < nr; j++) {
			struct semaphore *semaphore = &sem_list[i].sems[j];

			if (!semaphore->semzcnt && !semaphore->semncnt)
				continue;

			printf("  (%d): ", semaphore->semval);
			if (semaphore->semzcnt) {
				printf("zero(");
				for (k = 0; k < semaphore->semzcnt; k++)
					printf("%d,", semaphore->zlist[k].who);
				printf(")    ");
			}
			if (semaphore->semncnt) {
				printf("incr(");
				for (k = 0; k < semaphore->semncnt; k++)
					printf("%d-%d,",
					    semaphore->nlist[k].who,
					    semaphore->nlist[k].val);
				printf(")");
			}
			printf("\n");
		}
	}
	printf("\n");
}
#endif

static void
remove_process(endpoint_t endpt)
{
	struct sem_struct *sem;
	struct semaphore *semaphore;
	endpoint_t who_waiting;
	unsigned int i;
	int j, k, nr;

	for (i = 0; i < sem_list_nr; i++) {
		sem = &sem_list[i];
		if (!(sem->semid_ds.sem_perm.mode & SEM_ALLOC))
			continue;

		nr = sem->semid_ds.sem_nsems;
		for (j = 0; j < nr; j++) {
			semaphore = &sem->sems[j];

			for (k = 0; k < semaphore->semzcnt; k++) {
				who_waiting = semaphore->zlist[k].who;

				if (who_waiting == endpt) {
					/* Remove this slot first. */
					memmove(semaphore->zlist + k,
					    semaphore->zlist + k + 1,
					    sizeof(struct waiting) *
					    (semaphore->semzcnt - k - 1));
					semaphore->semzcnt--;

					/* Then send message to the process. */
					send_reply(who_waiting, EINTR);

					break;
				}
			}

			for (k = 0; k < semaphore->semncnt; k++) {
				who_waiting = semaphore->nlist[k].who;

				if (who_waiting == endpt) {
					/* Remove it first. */
					memmove(semaphore->nlist + k,
					    semaphore->nlist + k + 1,
					    sizeof(struct waiting) *
					    (semaphore->semncnt-k-1));
					semaphore->semncnt--;

					/* Send the message to the process. */
					send_reply(who_waiting, EINTR);

					break;
				}
			}
		}
	}
}

static void
check_semaphore(struct sem_struct * sem)
{
	int i, j, nr;
	struct semaphore *semaphore;
	endpoint_t who;

	nr = sem->semid_ds.sem_nsems;

	for (i = 0; i < nr; i++) {
		semaphore = &sem->sems[i];

		if (semaphore->zlist && !semaphore->semval) {
			/* Choose one process, policy: FIFO. */
			who = semaphore->zlist[0].who;

			memmove(semaphore->zlist, semaphore->zlist + 1,
			    sizeof(struct waiting) * (semaphore->semzcnt - 1));
			semaphore->semzcnt--;

			send_reply(who, OK);
		}

		if (semaphore->nlist) {
			for (j = 0; j < semaphore->semncnt; j++) {
				if (semaphore->nlist[j].val <=
				    semaphore->semval) {
					semaphore->semval -=
					    semaphore->nlist[j].val;
					who = semaphore->nlist[j].who;

					memmove(semaphore->nlist + j,
					    semaphore->nlist + j + 1,
					    sizeof(struct waiting) *
					    (semaphore->semncnt-j-1));
					semaphore->semncnt--;

					send_reply(who, OK);
					break;
				}
			}
		}
	}
}

int
do_semctl(message * m)
{
	unsigned int i;
	vir_bytes opt;
	uid_t uid;
	int r, id, num, cmd, val;
	unsigned short *buf;
	struct semid_ds tmp_ds;
	struct sem_struct *sem;
	struct seminfo sinfo;

	id = m->m_lc_ipc_semctl.id;
	num = m->m_lc_ipc_semctl.num;
	cmd = m->m_lc_ipc_semctl.cmd;
	opt = m->m_lc_ipc_semctl.opt;

	switch (cmd) {
	case IPC_INFO:
	case SEM_INFO:
		sem = NULL;
		break;
	case SEM_STAT:
		if (id < 0 || (unsigned int)id >= sem_list_nr)
			return EINVAL;
		sem = &sem_list[id];
		if (!(sem->semid_ds.sem_perm.mode & SEM_ALLOC))
			return EINVAL;
		break;
	default:
		if ((sem = sem_find_id(id)) == NULL)
			return EINVAL;
		break;
	}

	/*
	 * IPC_SET and IPC_RMID have their own permission checks.  IPC_INFO and
	 * SEM_INFO are free for general use.
	 */
	if (sem != NULL && cmd != IPC_SET && cmd != IPC_RMID) {
		/* Check read permission. */
		if (!check_perm(&sem->semid_ds.sem_perm, m->m_source, 0444))
			return EACCES;
	}

	switch (cmd) {
	case IPC_STAT:
	case SEM_STAT:
		if ((r = sys_datacopy(SELF, (vir_bytes)&sem->semid_ds,
		    m->m_source, opt, sizeof(sem->semid_ds))) != OK)
			return r;
		if (cmd == SEM_STAT)
			m->m_lc_ipc_semctl.ret =
			    IXSEQ_TO_IPCID(id, sem->semid_ds.sem_perm);
		break;
	case IPC_SET:
		uid = getnuid(m->m_source);
		if (uid != sem->semid_ds.sem_perm.cuid &&
		    uid != sem->semid_ds.sem_perm.uid && uid != 0)
			return EPERM;
		if ((r = sys_datacopy(m->m_source, opt, SELF,
		    (vir_bytes)&tmp_ds, sizeof(tmp_ds))) != OK)
			return r;
		sem->semid_ds.sem_perm.uid = tmp_ds.sem_perm.uid;
		sem->semid_ds.sem_perm.gid = tmp_ds.sem_perm.gid;
		sem->semid_ds.sem_perm.mode &= ~ACCESSPERMS;
		sem->semid_ds.sem_perm.mode |=
		    tmp_ds.sem_perm.mode & ACCESSPERMS;
		sem->semid_ds.sem_ctime = clock_time(NULL);
		break;
	case IPC_RMID:
		uid = getnuid(m->m_source);
		if (uid != sem->semid_ds.sem_perm.cuid &&
		    uid != sem->semid_ds.sem_perm.uid && uid != 0)
			return EPERM;
		/*
		 * Awaken all processes blocked in semop(2) on any semaphore in
		 * this set, and remove the semaphore set itself.
		 */
		remove_semaphore(sem);
		break;
	case IPC_INFO:
	case SEM_INFO:
		memset(&sinfo, 0, sizeof(sinfo));
		sinfo.semmap = SEMMNI;
		sinfo.semmni = SEMMNI;
		sinfo.semmns = SEMMNI * SEMMSL;
		sinfo.semmnu = 0; /* TODO: support for SEM_UNDO */
		sinfo.semmsl = SEMMSL;
		sinfo.semopm = SEMOPM;
		sinfo.semume = 0; /* TODO: support for SEM_UNDO */
		if (cmd == SEM_INFO) {
			/*
			 * For SEM_INFO the semusz field is expected to contain
			 * the number of semaphore sets currently in use.
			 */
			sinfo.semusz = sem_list_nr;
		} else
			sinfo.semusz = 0; /* TODO: support for SEM_UNDO */
		sinfo.semvmx = SEMVMX;
		if (cmd == SEM_INFO) {
			/*
			 * For SEM_INFO the semaem field is expected to contain
			 * the total number of allocated semaphores.
			 */
			for (i = 0; i < sem_list_nr; i++)
				sinfo.semaem += sem_list[i].semid_ds.sem_nsems;
		} else
			sinfo.semaem = 0; /* TODO: support for SEM_UNDO */

		if ((r = sys_datacopy(SELF, (vir_bytes)&sinfo, m->m_source,
		    opt, sizeof(sinfo))) != OK)
			return r;
		/* Return the highest in-use slot number if any, or zero. */
		if (sem_list_nr > 0)
			m->m_lc_ipc_semctl.ret = sem_list_nr - 1;
		else
			m->m_lc_ipc_semctl.ret = 0;
		break;
	case GETALL:
		buf = malloc(sizeof(unsigned short) * sem->semid_ds.sem_nsems);
		if (buf == NULL)
			return ENOMEM;
		for (i = 0; i < sem->semid_ds.sem_nsems; i++)
			buf[i] = sem->sems[i].semval;
		r = sys_datacopy(SELF, (vir_bytes)buf, m->m_source,
		    opt, sizeof(unsigned short) * sem->semid_ds.sem_nsems);
		free(buf);
		if (r != OK)
			return EINVAL;
		break;
	case GETNCNT:
		if (num < 0 || num >= sem->semid_ds.sem_nsems)
			return EINVAL;
		m->m_lc_ipc_semctl.ret = sem->sems[num].semncnt;
		break;
	case GETPID:
		if (num < 0 || num >= sem->semid_ds.sem_nsems)
			return EINVAL;
		m->m_lc_ipc_semctl.ret = sem->sems[num].sempid;
		break;
	case GETVAL:
		if (num < 0 || num >= sem->semid_ds.sem_nsems)
			return EINVAL;
		m->m_lc_ipc_semctl.ret = sem->sems[num].semval;
		break;
	case GETZCNT:
		if (num < 0 || num >= sem->semid_ds.sem_nsems)
			return EINVAL;
		m->m_lc_ipc_semctl.ret = sem->sems[num].semzcnt;
		break;
	case SETALL:
		buf = malloc(sizeof(unsigned short) * sem->semid_ds.sem_nsems);
		if (buf == NULL)
			return ENOMEM;
		r = sys_datacopy(m->m_source, opt, SELF, (vir_bytes)buf,
		    sizeof(unsigned short) * sem->semid_ds.sem_nsems);
		if (r != OK) {
			free(buf);
			return EINVAL;
		}
		for (i = 0; i < sem->semid_ds.sem_nsems; i++) {
			if (buf[i] > SEMVMX) {
				free(buf);
				return ERANGE;
			}
		}
#ifdef DEBUG_SEM
		for (i = 0; i < sem->semid_ds.sem_nsems; i++)
			printf("SEMCTL: SETALL val: [%d] %d\n", i, buf[i]);
#endif
		for (i = 0; i < sem->semid_ds.sem_nsems; i++)
			sem->sems[i].semval = buf[i];
		free(buf);
		/* Awaken any waiting parties if now possible. */
		check_semaphore(sem);
		break;
	case SETVAL:
		val = (int)opt;
		/* Check write permission. */
		if (!check_perm(&sem->semid_ds.sem_perm, m->m_source, 0222))
			return EACCES;
		if (num < 0 || num >= sem->semid_ds.sem_nsems)
			return EINVAL;
		if (val < 0 || val > SEMVMX)
			return ERANGE;
		sem->sems[num].semval = val;
#ifdef DEBUG_SEM
		printf("SEMCTL: SETVAL: %d %d\n", num, val);
#endif
		sem->semid_ds.sem_ctime = clock_time(NULL);
		/* Awaken any waiting parties if now possible. */
		check_semaphore(sem);
		break;
	default:
		return EINVAL;
	}

	return OK;
}

int
do_semop(message * m)
{
	unsigned int i, mask;
	int id, r;
	struct sembuf *sops;
	unsigned int nsops;
	struct sem_struct *sem;
	struct semaphore *s;
	int op_n, val, no_reply;

	id = m->m_lc_ipc_semop.id;
	nsops = m->m_lc_ipc_semop.size;

	if ((sem = sem_find_id(id)) == NULL)
		return EINVAL;

	if (nsops <= 0)
		return EINVAL;
	if (nsops > SEMOPM)
		return E2BIG;

	/* Get the array from the user process. */
	sops = malloc(sizeof(sops[0]) * nsops);
	if (!sops)
		return ENOMEM;
	r = sys_datacopy(m->m_source, (vir_bytes)m->m_lc_ipc_semop.ops, SELF,
	    (vir_bytes)sops, sizeof(sops[0]) * nsops);
	if (r != OK)
		goto out_free;

#ifdef DEBUG_SEM
	for (i = 0; i < nsops; i++)
		printf("SEMOP: num:%d  op:%d  flg:%d\n",
			sops[i].sem_num, sops[i].sem_op, sops[i].sem_flg);
#endif
	/* Check that all given semaphore numbers are within range. */
	r = EFBIG;
	for (i = 0; i < nsops; i++)
		if (sops[i].sem_num >= sem->semid_ds.sem_nsems)
			goto out_free;

	/* Check for permissions. */
	r = EACCES;
	mask = 0;
	for (i = 0; i < nsops; i++) {
		if (sops[i].sem_op != 0)
			mask |= 0222; /* check for write permission */
		else
			mask |= 0444; /* check for read permission */
	}
	if (mask && !check_perm(&sem->semid_ds.sem_perm, m->m_source, mask))
		goto out_free;

	/* Check for nonblocking operations. */
	r = EAGAIN;
	for (i = 0; i < nsops; i++) {
		op_n = sops[i].sem_op;
		val = sem->sems[sops[i].sem_num].semval;

		if ((sops[i].sem_flg & IPC_NOWAIT) &&
		    ((op_n == 0 && val != 0) || (op_n < 0 && -op_n > val)))
			goto out_free;
	}

	/* There will be no errors left, so we can go ahead. */
	no_reply = 0;
	for (i = 0; i < nsops; i++) {
		s = &sem->sems[sops[i].sem_num];
		op_n = sops[i].sem_op;

		s->sempid = getnpid(m->m_source);

		if (op_n > 0) {
			/* XXX missing ERANGE check */
			s->semval += sops[i].sem_op;
		} else if (op_n == 0) {
			if (s->semval) {
				/* Put the process to sleep. */
				s->semzcnt++;
				s->zlist = realloc(s->zlist,
				    sizeof(struct waiting) * s->semzcnt);
				/* continuing if NULL would lead to disaster */
				if (s->zlist == NULL)
					panic("out of memory");
				s->zlist[s->semzcnt - 1].who = m->m_source;
				s->zlist[s->semzcnt - 1].val = op_n;

				no_reply++;
			}
		} else /* (op_n < 0) */ {
			if (s->semval >= -op_n)
				s->semval += op_n;
			else {
				/* Put the process to sleep. */
				s->semncnt++;
				s->nlist = realloc(s->nlist,
				    sizeof(struct waiting) * s->semncnt);
				/* continuing if NULL would lead to disaster */
				if (s->nlist == NULL)
					panic("out of memory");
				s->nlist[s->semncnt - 1].who = m->m_source;
				s->nlist[s->semncnt - 1].val = -op_n;

				no_reply++;
			}
		}
	}

	r = no_reply ? SUSPEND : OK;

	/* Awaken any other waiting parties if now possible. */
	check_semaphore(sem);

out_free:
	free(sops);

	return r;
}

int
is_sem_nil(void)
{

	return (sem_list_nr == 0);
}

void
sem_process_vm_notify(void)
{
	endpoint_t endpt;
	int r;

	/* For each endpoint, check whether it is waiting. */
	while ((r = vm_query_exit(&endpt)) >= 0) {
		remove_process(endpt);

		if (r == 0)
			break;
	}
	if (r < 0)
		printf("IPC: query exit error (%d)\n", r);
}
