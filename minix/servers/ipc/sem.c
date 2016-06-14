#include "inc.h"

struct sem_struct;

/* IPC-server process table, currently used for semaphores only. */
struct iproc {
	struct sem_struct *ip_sem;	/* affected semaphore set, or NULL */
	struct sembuf *ip_sops;		/* pending operations (malloc'ed) */
	unsigned int ip_nsops;		/* number of pending operations */
	struct sembuf *ip_blkop;	/* pointer to operation that blocked */
	endpoint_t ip_endpt;		/* process endpoint */
	pid_t ip_pid;			/* process PID */
	TAILQ_ENTRY(iproc) ip_next;	/* next waiting process */
} iproc[NR_PROCS];

struct semaphore {
	unsigned short semval;		/* semaphore value */
	unsigned short semzcnt;		/* # waiting for zero */
	unsigned short semncnt;		/* # waiting for increase */
	pid_t sempid;			/* process that did last op */
};

/*
 * For the list of waiting processes, we use a doubly linked tail queue.  In
 * order to maintain a basic degree of fairness, we keep the pending processes
 * in FCFS (well, at least first tested) order, which means we need to be able
 * to add new processes at the end of the list.  In order to remove waiting
 * processes O(1) instead of O(n) we need a doubly linked list; in the common
 * case we do have the element's predecessor, but STAILQ_REMOVE is O(n) anyway
 * and NetBSD has no STAILQ_REMOVE_AFTER yet.
 *
 * We use one list per semaphore set: semop(2) affects only one semaphore set,
 * but it may involve operations on multiple semaphores within the set.  While
 * it is possible to recheck only semaphores that were affected by a particular
 * operation, and associate waiting lists to individual semaphores, the number
 * of expected waiting processes is currently not high enough to justify the
 * extra complexity of such an implementation.
 */
struct sem_struct {
	struct semid_ds semid_ds;
	struct semaphore sems[SEMMSL];
	TAILQ_HEAD(waiters, iproc) waiters;
};

static struct sem_struct sem_list[SEMMNI];
static unsigned int sem_list_nr = 0; /* highest in-use slot number plus one */

/*
 * Find a semaphore set by key.  The given key must not be IPC_PRIVATE.  Return
 * a pointer to the semaphore set if found, or NULL otherwise.
 */
static struct sem_struct *
sem_find_key(key_t key)
{
	unsigned int i;

	for (i = 0; i < sem_list_nr; i++) {
		if (!(sem_list[i].semid_ds.sem_perm.mode & SEM_ALLOC))
			continue;
		if (sem_list[i].semid_ds.sem_perm._key == key)
			return &sem_list[i];
	}

	return NULL;
}

/*
 * Find a semaphore set by identifier.  Return a pointer to the semaphore set
 * if found, or NULL otherwise.
 */
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

/*
 * Implementation of the semget(2) system call.
 */
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

	if (key != IPC_PRIVATE && (sem = sem_find_key(key)) != NULL) {
		if ((flag & IPC_CREAT) && (flag & IPC_EXCL))
			return EEXIST;
		if (!check_perm(&sem->semid_ds.sem_perm, m->m_source, flag))
			return EACCES;
		if (nsems > sem->semid_ds.sem_nsems)
			return EINVAL;
		i = sem - sem_list;
	} else {
		if (key != IPC_PRIVATE && !(flag & IPC_CREAT))
			return ENOENT;
		if (nsems <= 0 || nsems > SEMMSL)
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
		TAILQ_INIT(&sem->waiters);

		assert(i <= sem_list_nr);
		if (i == sem_list_nr) {
			/*
			 * If no semaphore sets were allocated before,
			 * subscribe to process events now.
			 */
			if (sem_list_nr == 0)
				update_sem_sub(TRUE /*want_events*/);

			sem_list_nr++;
		}
	}

	m->m_lc_ipc_semget.retid = IXSEQ_TO_IPCID(i, sem->semid_ds.sem_perm);
	return OK;
}

/*
 * Increase the proper suspension count (semncnt or semzcnt) of the semaphore
 * on which the given process is blocked.
 */
static void
inc_susp_count(struct iproc * ip)
{
	struct sembuf *blkop;
	struct semaphore *sp;

	blkop = ip->ip_blkop;
	sp = &ip->ip_sem->sems[blkop->sem_num];

	if (blkop->sem_op != 0) {
		assert(sp->semncnt < USHRT_MAX);
		sp->semncnt++;
	} else {
		assert(sp->semncnt < USHRT_MAX);
		sp->semzcnt++;
	}
}

/*
 * Decrease the proper suspension count (semncnt or semzcnt) of the semaphore
 * on which the given process is blocked.
 */
static void
dec_susp_count(struct iproc * ip)
{
	struct sembuf *blkop;
	struct semaphore *sp;

	blkop = ip->ip_blkop;
	sp = &ip->ip_sem->sems[blkop->sem_num];

	if (blkop->sem_op != 0) {
		assert(sp->semncnt > 0);
		sp->semncnt--;
	} else {
		assert(sp->semzcnt > 0);
		sp->semzcnt--;
	}
}

/*
 * Send a reply for a semop(2) call suspended earlier, thus waking up the
 * process.
 */
static void
send_reply(endpoint_t who, int ret)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_type = ret;

	ipc_sendnb(who, &m);
}

/*
 * Satisfy or cancel the semop(2) call on which the given process is blocked,
 * and send the given reply code (OK or a negative error code) to wake it up,
 * unless the given code is EDONTREPLY.
 */
static void
complete_semop(struct iproc * ip, int code)
{
	struct sem_struct *sem;

	sem = ip->ip_sem;

	assert(sem != NULL);

	TAILQ_REMOVE(&sem->waiters, ip, ip_next);

	dec_susp_count(ip);

	assert(ip->ip_sops != NULL);
	free(ip->ip_sops);

	ip->ip_sops = NULL;
	ip->ip_blkop = NULL;
	ip->ip_sem = NULL;

	if (code != EDONTREPLY)
		send_reply(ip->ip_endpt, code);
}

/*
 * Free up the given semaphore set.  This includes cancelling any blocking
 * semop(2) calls on any of its semaphores.
 */
static void
remove_set(struct sem_struct * sem)
{
	struct iproc *ip;

	/*
	 * Cancel all semop(2) operations on this semaphore set, with an EIDRM
	 * reply code.
	 */
	while (!TAILQ_EMPTY(&sem->waiters)) {
		ip = TAILQ_FIRST(&sem->waiters);

		complete_semop(ip, EIDRM);
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

	/*
	 * If this was our last semaphore set, unsubscribe from process events.
	 */
	if (sem_list_nr == 0)
		update_sem_sub(FALSE /*want_events*/);
}

/*
 * Try to perform a set of semaphore operations, as given by semop(2), on a
 * semaphore set.  The entire action must be atomic, i.e., either succeed in
 * its entirety or fail without making any changes.  Return OK on success, in
 * which case the PIDs of all affected semaphores will be updated to the given
 * 'pid' value, and the semaphore set's sem_otime will be updated as well.
 * Return SUSPEND if the call should be suspended, in which case 'blkop' will
 * be set to a pointer to the operation causing the call to block.  Return an
 * error code if the call failed altogether.
 */
static int
try_semop(struct sem_struct *sem, struct sembuf *sops, unsigned int nsops,
	pid_t pid, struct sembuf ** blkop)
{
	struct semaphore *sp;
	struct sembuf *op;
	unsigned int i;
	int r;

	/*
	 * The operation must be processed atomically.  However, it must also
	 * be processed "in array order," which we assume to mean that while
	 * processing one operation, the changes of the previous operations
	 * must be taken into account.  This is relevant for cases where the
	 * same semaphore is referenced by more than one operation, for example
	 * to perform an atomic increase-if-zero action on a single semaphore.
	 * As a result, we must optimistically modify semaphore values and roll
	 * back on suspension or failure afterwards.
	 */
	r = OK;
	op = NULL;
	for (i = 0; i < nsops; i++) {
		sp = &sem->sems[sops[i].sem_num];
		op = &sops[i];

		if (op->sem_op > 0) {
			if (SEMVMX - sp->semval < op->sem_op) {
				r = ERANGE;
				break;
			}
			sp->semval += op->sem_op;
		} else if (op->sem_op < 0) {
			/*
			 * No SEMVMX check; if the process wants to deadlock
			 * itself by supplying -SEMVMX it is free to do so..
			 */
			if ((int)sp->semval < -(int)op->sem_op) {
				r = (op->sem_flg & IPC_NOWAIT) ? EAGAIN :
				    SUSPEND;
				break;
			}
			sp->semval += op->sem_op;
		} else /* (op->sem_op == 0) */ {
			if (sp->semval != 0) {
				r = (op->sem_flg & IPC_NOWAIT) ? EAGAIN :
				    SUSPEND;
				break;
			}
		}
	}

	/*
	 * If we did not go through all the operations, then either an error
	 * occurred or the user process is to be suspended.  In that case we
	 * must roll back any progress we have made so far, and return the
	 * operation that caused the call to block.
	 */
	if (i < nsops) {
		assert(op != NULL);
		*blkop = op;

		/* Roll back all changes made so far. */
		while (i-- > 0)
			sem->sems[sops[i].sem_num].semval -= sops[i].sem_op;

		assert(r != OK);
		return r;
	}

	/*
	 * The operation has completed successfully.  Also update all affected
	 * semaphores' PID values, and the semaphore set's last-semop time.
	 * The caller must do everything else.
	 */
	for (i = 0; i < nsops; i++)
		sem->sems[sops[i].sem_num].sempid = pid;

	sem->semid_ds.sem_otime = clock_time(NULL);

	return OK;
}

/*
 * Check whether any blocked operations can now be satisfied on any of the
 * semaphores in the given semaphore set.  Do this repeatedly as necessary, as
 * any unblocked operation may in turn allow other operations to be resumed.
 */
static void
check_set(struct sem_struct * sem)
{
	struct iproc *ip, *nextip;
	struct sembuf *blkop;
	int r, woken_up;

	/*
	 * Go through all the waiting processes in FIFO order, which is our
	 * best attempt at providing at least some fairness.  Keep trying as
	 * long as we woke up at least one process, which means we made actual
	 * progress.
	 */
	do {
		woken_up = FALSE;

		TAILQ_FOREACH_SAFE(ip, &sem->waiters, ip_next, nextip) {
			/* Retry the entire semop(2) operation, atomically. */
			r = try_semop(ip->ip_sem, ip->ip_sops, ip->ip_nsops,
			    ip->ip_pid, &blkop);

			if (r != SUSPEND) {
				/* Success or failure. */
				complete_semop(ip, r);

				/* No changes are made on failure. */
				if (r == OK)
					woken_up = TRUE;
			} else if (blkop != ip->ip_blkop) {
				/*
				 * The process stays suspended, but it is now
				 * blocked on a different semaphore.  As a
				 * result, we need to adjust the semaphores'
				 * suspension counts.
				 */
				dec_susp_count(ip);

				ip->ip_blkop = blkop;

				inc_susp_count(ip);
			}
		}
	} while (woken_up);
}

/*
 * Fill a seminfo structure with actual information.  The information returned
 * depends on the given command, which may be either IPC_INFO or SEM_INFO.
 */
static void
fill_seminfo(struct seminfo * sinfo, int cmd)
{
	unsigned int i;

	assert(cmd == IPC_INFO || cmd == SEM_INFO);

	memset(sinfo, 0, sizeof(*sinfo));

	sinfo->semmap = SEMMNI;
	sinfo->semmni = SEMMNI;
	sinfo->semmns = SEMMNI * SEMMSL;
	sinfo->semmnu = 0; /* TODO: support for SEM_UNDO */
	sinfo->semmsl = SEMMSL;
	sinfo->semopm = SEMOPM;
	sinfo->semume = 0; /* TODO: support for SEM_UNDO */
	if (cmd == SEM_INFO) {
		/*
		 * For SEM_INFO the semusz field is expected to contain the
		 * number of semaphore sets currently in use.
		 */
		sinfo->semusz = sem_list_nr;
	} else
		sinfo->semusz = 0; /* TODO: support for SEM_UNDO */
	sinfo->semvmx = SEMVMX;
	if (cmd == SEM_INFO) {
		/*
		 * For SEM_INFO the semaem field is expected to contain
		 * the total number of allocated semaphores.
		 */
		for (i = 0; i < sem_list_nr; i++)
			sinfo->semaem += sem_list[i].semid_ds.sem_nsems;
	} else
		sinfo->semaem = 0; /* TODO: support for SEM_UNDO */
}

/*
 * Implementation of the semctl(2) system call.
 */
int
do_semctl(message * m)
{
	static unsigned short valbuf[SEMMSL];
	unsigned int i;
	vir_bytes opt;
	uid_t uid;
	int r, id, num, cmd, val;
	struct semid_ds tmp_ds;
	struct sem_struct *sem;
	struct seminfo sinfo;

	id = m->m_lc_ipc_semctl.id;
	num = m->m_lc_ipc_semctl.num;
	cmd = m->m_lc_ipc_semctl.cmd;
	opt = m->m_lc_ipc_semctl.opt;

	/*
	 * Look up the target semaphore set.  The IPC_INFO and SEM_INFO
	 * commands have no associated semaphore set.  The SEM_STAT command
	 * takes an array index into the semaphore set table.  For all other
	 * commands, look up the semaphore set by its given identifier.
	 * */
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
	 * Check if the caller has the appropriate permissions on the target
	 * semaphore set.  SETVAL and SETALL require write permission.  IPC_SET
	 * and IPC_RMID require ownership permission, and return EPERM instead
	 * of EACCES on failure.  IPC_INFO and SEM_INFO are free for general
	 * use.  All other calls require read permission.
	 */
	switch (cmd) {
	case SETVAL:
	case SETALL:
		assert(sem != NULL);
		if (!check_perm(&sem->semid_ds.sem_perm, m->m_source, IPC_W))
			return EACCES;
		break;
	case IPC_SET:
	case IPC_RMID:
		assert(sem != NULL);
		uid = getnuid(m->m_source);
		if (uid != sem->semid_ds.sem_perm.cuid &&
		    uid != sem->semid_ds.sem_perm.uid && uid != 0)
			return EPERM;
		break;
	case IPC_INFO:
	case SEM_INFO:
		break;
	default:
		assert(sem != NULL);
		if (!check_perm(&sem->semid_ds.sem_perm, m->m_source, IPC_R))
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
		/*
		 * Awaken all processes blocked in semop(2) on any semaphore in
		 * this set, and remove the semaphore set itself.
		 */
		remove_set(sem);
		break;
	case IPC_INFO:
	case SEM_INFO:
		fill_seminfo(&sinfo, cmd);

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
		assert(sem->semid_ds.sem_nsems <= __arraycount(valbuf));
		for (i = 0; i < sem->semid_ds.sem_nsems; i++)
			valbuf[i] = sem->sems[i].semval;
		r = sys_datacopy(SELF, (vir_bytes)valbuf, m->m_source,
		    opt, sizeof(unsigned short) * sem->semid_ds.sem_nsems);
		if (r != OK)
			return r;
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
		assert(sem->semid_ds.sem_nsems <= __arraycount(valbuf));
		r = sys_datacopy(m->m_source, opt, SELF, (vir_bytes)valbuf,
		    sizeof(unsigned short) * sem->semid_ds.sem_nsems);
		if (r != OK)
			return r;
		for (i = 0; i < sem->semid_ds.sem_nsems; i++)
			if (valbuf[i] > SEMVMX)
				return ERANGE;
#ifdef DEBUG_SEM
		for (i = 0; i < sem->semid_ds.sem_nsems; i++)
			printf("SEMCTL: SETALL val: [%d] %d\n", i, valbuf[i]);
#endif
		for (i = 0; i < sem->semid_ds.sem_nsems; i++)
			sem->sems[i].semval = valbuf[i];
		sem->semid_ds.sem_ctime = clock_time(NULL);
		/* Awaken any waiting parties if now possible. */
		check_set(sem);
		break;
	case SETVAL:
		val = (int)opt;
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
		check_set(sem);
		break;
	default:
		return EINVAL;
	}

	return OK;
}

/*
 * Implementation of the semop(2) system call.
 */
int
do_semop(message * m)
{
	unsigned int i, mask, slot;
	int id, r;
	struct sembuf *sops, *blkop;
	unsigned int nsops;
	struct sem_struct *sem;
	struct iproc *ip;
	pid_t pid;

	id = m->m_lc_ipc_semop.id;
	nsops = m->m_lc_ipc_semop.size;

	if ((sem = sem_find_id(id)) == NULL)
		return EINVAL;

	if (nsops == 0)
		return OK; /* nothing to do */
	if (nsops > SEMOPM)
		return E2BIG;

	/* Get the array from the user process. */
	sops = malloc(sizeof(sops[0]) * nsops);
	if (sops == NULL)
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
	/*
	 * Check for permissions.  We do this only once, even though the call
	 * might suspend and the semaphore set's permissions might be changed
	 * before the call resumes.  The specification is not clear on this.
	 * Either way, perform the permission check before checking on the
	 * validity of semaphore numbers, since obtaining the semaphore set
	 * size itself requires read permission (except through sysctl(2)..).
	 */
	mask = 0;
	for (i = 0; i < nsops; i++) {
		if (sops[i].sem_op != 0)
			mask |= IPC_W; /* check for write permission */
		else
			mask |= IPC_R; /* check for read permission */
	}
	r = EACCES;
	if (!check_perm(&sem->semid_ds.sem_perm, m->m_source, mask))
		goto out_free;

	/* Check that all given semaphore numbers are within range. */
	r = EFBIG;
	for (i = 0; i < nsops; i++)
		if (sops[i].sem_num >= sem->semid_ds.sem_nsems)
			goto out_free;

	/*
	 * Do not check if the same semaphore is referenced more than once
	 * (there was such a check here originally), because that is actually
	 * a valid case.  The result is however that it is possible to
	 * construct a semop(2) request that will never complete, and thus,
	 * care must be taken that such requests do not create potential
	 * deadlock situations etc.
	 */

	pid = getnpid(m->m_source);

	/*
	 * We do not yet support SEM_UNDO at all, so we better not give the
	 * caller the impression that we do.  For now, print a warning so that
	 * we know when an application actually fails for that reason.
	 */
	for (i = 0; i < nsops; i++) {
		if (sops[i].sem_flg & SEM_UNDO) {
			/* Print a warning only if this isn't the test set.. */
			if (sops[i].sem_flg != SHRT_MAX)
				printf("IPC: pid %d tried to use SEM_UNDO\n",
				    pid);
			r = EINVAL;
			goto out_free;
		}
	}

	/* Try to perform the operation now. */
	r = try_semop(sem, sops, nsops, pid, &blkop);

	if (r == SUSPEND) {
		/*
		 * The operation ended up blocking on a particular semaphore
		 * operation.  Save all details in the slot for the user
		 * process, and add it to the list of processes waiting for
		 * this semaphore set.
		 */
		slot = _ENDPOINT_P(m->m_source);
		assert(slot < __arraycount(iproc));

		ip = &iproc[slot];
		assert(ip->ip_sem == NULL); /* can't already be in use */

		ip->ip_endpt = m->m_source;
		ip->ip_pid = pid;
		ip->ip_sem = sem;
		ip->ip_sops = sops;
		ip->ip_nsops = nsops;
		ip->ip_blkop = blkop;

		TAILQ_INSERT_TAIL(&sem->waiters, ip, ip_next);

		inc_susp_count(ip);

		return r;
	}

out_free:
	free(sops);

	/* Awaken any other waiting parties if now possible. */
	if (r == OK)
		check_set(sem);

	return r;
}

/*
 * Return semaphore information for a remote MIB call on the sysvipc_info node
 * in the kern.ipc subtree.  The particular semantics of this call are tightly
 * coupled to the implementation of the ipcs(1) userland utility.
 */
ssize_t
get_sem_mib_info(struct rmib_oldp * oldp)
{
	struct sem_sysctl_info semsi;
	struct semid_ds *semds;
	unsigned int i;
	ssize_t r, off;

	off = 0;

	fill_seminfo(&semsi.seminfo, IPC_INFO);

	/*
	 * As a hackish exception, the requested size may imply that just
	 * general information is to be returned, without throwing an ENOMEM
	 * error because there is no space for full output.
	 */
	if (rmib_getoldlen(oldp) == sizeof(semsi.seminfo))
		return rmib_copyout(oldp, 0, &semsi.seminfo,
		    sizeof(semsi.seminfo));

	/*
	 * ipcs(1) blindly expects the returned array to be of size
	 * seminfo.semmni, using the SEM_ALLOC mode flag to see whether each
	 * entry is valid.  If we return a smaller size, ipcs(1) will access
	 * arbitrary memory.
	 */
	assert(semsi.seminfo.semmni > 0);

	if (oldp == NULL)
		return sizeof(semsi) + sizeof(semsi.semids[0]) *
		    (semsi.seminfo.semmni - 1);

	/*
	 * Copy out entries one by one.  For the first entry, copy out the
	 * entire "semsi" structure.  For subsequent entries, reuse the single
	 * embedded 'semids' element of "semsi" and copy out only that element.
	 */
	for (i = 0; i < (unsigned int)semsi.seminfo.semmni; i++) {
		semds = &sem_list[i].semid_ds;

		memset(&semsi.semids[0], 0, sizeof(semsi.semids[0]));
		if (i < sem_list_nr && (semds->sem_perm.mode & SEM_ALLOC)) {
			prepare_mib_perm(&semsi.semids[0].sem_perm,
			    &semds->sem_perm);
			semsi.semids[0].sem_nsems = semds->sem_nsems;
			semsi.semids[0].sem_otime = semds->sem_otime;
			semsi.semids[0].sem_ctime = semds->sem_ctime;
		}

		if (off == 0)
			r = rmib_copyout(oldp, off, &semsi, sizeof(semsi));
		else
			r = rmib_copyout(oldp, off, &semsi.semids[0],
			    sizeof(semsi.semids[0]));

		if (r < 0)
			return r;
		off += r;
	}

	return off;
}

/*
 * Return TRUE iff no semaphore sets are allocated.
 */
int
is_sem_nil(void)
{

	return (sem_list_nr == 0);
}

/*
 * Check if the given endpoint is blocked on a semop(2) call.  If so, cancel
 * the call, because either it is interrupted by a signal or the process was
 * killed.  In the former case, unblock the process by replying with EINTR.
 */
void
sem_process_event(endpoint_t endpt, int has_exited)
{
	unsigned int slot;
	struct iproc *ip;

	slot = _ENDPOINT_P(endpt);
	assert(slot < __arraycount(iproc));

	ip = &iproc[slot];

	/* Was the process blocked on a semop(2) call at all? */
	if (ip->ip_sem == NULL)
		return;

	assert(ip->ip_endpt == endpt);

	/*
	 * It was; cancel the semop(2) call.  If the process is being removed
	 * because its call was interrupted by a signal, then we must wake it
	 * up with EINTR.
	 */
	complete_semop(ip, has_exited ? EDONTREPLY : EINTR);
}
