#define __USE_MISC

#include <minix/vm.h>

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
	key_t key;
	int id;
	struct semid_ds semid_ds;
	struct semaphore sems[SEMMSL];
};

static struct sem_struct sem_list[SEMMNI];
static int sem_list_nr = 0;

static struct sem_struct *sem_find_key(key_t key)
{
	int i;
	if (key == IPC_PRIVATE)
		return NULL;
	for (i = 0; i < sem_list_nr; i++)
		if (sem_list[i].key == key)
			return sem_list+i;
	return NULL;
}

static struct sem_struct *sem_find_id(int id)
{
	int i;
	for (i = 0; i < sem_list_nr; i++)
		if (sem_list[i].id == id)
			return sem_list+i;
	return NULL;
}

/*===========================================================================*
 *				do_semget		     		     *
 *===========================================================================*/
int do_semget(message *m)
{
	key_t key;
	int nsems, flag, id;
	struct sem_struct *sem;

	key = m->m_lc_ipc_semget.key;
	nsems = m->m_lc_ipc_semget.nr;
	flag = m->m_lc_ipc_semget.flag;

	if ((sem = sem_find_key(key))) {
		if ((flag & IPC_CREAT) && (flag & IPC_EXCL))
			return EEXIST;
		if (!check_perm(&sem->semid_ds.sem_perm, who_e, flag))
			return EACCES;
		if (nsems > sem->semid_ds.sem_nsems)
			return EINVAL;
		id = sem->id;
	} else {
		if (!(flag & IPC_CREAT))
			return ENOENT;
		if (nsems < 0 || nsems >= SEMMSL)
			return EINVAL;
		if (sem_list_nr == SEMMNI)
			return ENOSPC;

		/* create a new semaphore set */
		sem = &sem_list[sem_list_nr];
		memset(sem, 0, sizeof(struct sem_struct));
		sem->semid_ds.sem_perm.cuid =
			sem->semid_ds.sem_perm.uid = getnuid(who_e);
		sem->semid_ds.sem_perm.cgid =
			sem->semid_ds.sem_perm.gid = getngid(who_e);
		sem->semid_ds.sem_perm.mode = flag & 0777;
		sem->semid_ds.sem_nsems = nsems;
		sem->semid_ds.sem_otime = 0;
		sem->semid_ds.sem_ctime = time(NULL);
		sem->id = id = identifier++;
		sem->key = key;

		sem_list_nr++;
	}

	m->m_lc_ipc_semget.retid = id;
	return OK;
}

static void send_message_to_process(endpoint_t who, int ret, int ignore)
{
	message m;

	m.m_type = ret;
	ipc_sendnb(who, &m);
}

static void remove_semaphore(struct sem_struct *sem)
{
	int i, nr;

	nr = sem->semid_ds.sem_nsems;

	for (i = 0; i < nr; i++) {
		if (sem->sems[i].zlist)
			free(sem->sems[i].zlist);
		if (sem->sems[i].nlist)
			free(sem->sems[i].nlist);
	}

	for (i = 0; i < sem_list_nr; i++) {
		if (&sem_list[i] == sem)
			break;
	}

	if (i < sem_list_nr && --sem_list_nr != i)
		sem_list[i] = sem_list[sem_list_nr];
}

#if 0
static void show_semaphore(void)
{
	int i, j, k;

	for (i = 0; i < sem_list_nr; i++) {
		int nr = sem_list[i].semid_ds.sem_nsems;

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
						semaphore->nlist[k].who, semaphore->nlist[k].val);
				printf(")");
			}
			printf("\n");
		}
	}
	printf("\n");
}
#endif

static void remove_process(endpoint_t pt)
{
	int i;

	for (i = 0; i < sem_list_nr; i++) {
		struct sem_struct *sem = &sem_list[i];
		int nr = sem->semid_ds.sem_nsems;
		int j;

		for (j = 0; j < nr; j++) {
			struct semaphore *semaphore = &sem->sems[j];
			int k;

			for (k = 0; k < semaphore->semzcnt; k++) {
				endpoint_t who_waiting = semaphore->zlist[k].who;

				if (who_waiting == pt) {
					/* remove this slot first */
					memmove(semaphore->zlist+k, semaphore->zlist+k+1,
						sizeof(struct waiting) * (semaphore->semzcnt-k-1));
					--semaphore->semzcnt;
					/* then send message to the process */
					send_message_to_process(who_waiting, EINTR, 1);

					break;
				}
			}

			for (k = 0; k < semaphore->semncnt; k++) {
				endpoint_t who_waiting = semaphore->nlist[k].who;

				if (who_waiting == pt) {
					/* remove it first */
					memmove(semaphore->nlist+k, semaphore->nlist+k+1,
						sizeof(struct waiting) * (semaphore->semncnt-k-1));
					--semaphore->semncnt;
					/* send the message to the process */
					send_message_to_process(who_waiting, EINTR, 1);

					break;
				}
			}
		}
	}
}

static void update_one_semaphore(struct sem_struct *sem, int is_remove)
{
	int i, j, nr;
	struct semaphore *semaphore;
	endpoint_t who;

	nr = sem->semid_ds.sem_nsems;

	if (is_remove) {
		for (i = 0; i < nr; i++) {
			semaphore = &sem->sems[i];

			for (j = 0; j < semaphore->semzcnt; j++)
				send_message_to_process(semaphore->zlist[j].who, EIDRM, 0);
			for (j = 0; j < semaphore->semncnt; j++)
				send_message_to_process(semaphore->nlist[j].who, EIDRM, 0);
		}

		remove_semaphore(sem);
		return;
	}

	for (i = 0; i < nr; i++) {
		semaphore = &sem->sems[i];

		if (semaphore->zlist && !semaphore->semval) {
			/* choose one process, policy: FIFO. */
			who = semaphore->zlist[0].who;

			memmove(semaphore->zlist, semaphore->zlist+1,
				sizeof(struct waiting) * (semaphore->semzcnt-1));
			--semaphore->semzcnt;

			send_message_to_process(who, OK, 0);
		}

		if (semaphore->nlist) {
			for (j = 0; j < semaphore->semncnt; j++) {
				if (semaphore->nlist[j].val <= semaphore->semval) {
					semaphore->semval -= semaphore->nlist[j].val;
					who = semaphore->nlist[j].who;

					memmove(semaphore->nlist+j, semaphore->nlist+j+1,
						sizeof(struct waiting) * (semaphore->semncnt-j-1));
					--semaphore->semncnt;

					send_message_to_process(who, OK, 0);

					/* choose only one process */
					break;
				}
			}
		}
	}
}

static void update_semaphores(void)
{
	int i;

	for (i = 0; i < sem_list_nr; i++)
		update_one_semaphore(sem_list+i, 0 /* not remove */);
}

/*===========================================================================*
 *				do_semctl		     		     *
 *===========================================================================*/
int do_semctl(message *m)
{
	int r, i;
	long opt = 0;
	uid_t uid;
	int id, num, cmd, val;
	unsigned short *buf;
	struct semid_ds *ds, tmp_ds;
	struct sem_struct *sem;

	id = m->m_lc_ipc_semctl.id;
	num = m->m_lc_ipc_semctl.num;
	cmd = m->m_lc_ipc_semctl.cmd;

	if (cmd == IPC_STAT || cmd == IPC_SET || cmd == IPC_INFO ||
		cmd == SEM_INFO || cmd == SEM_STAT || cmd == GETALL ||
		cmd == SETALL || cmd == SETVAL)
		opt = m->m_lc_ipc_semctl.opt;

	if (!(sem = sem_find_id(id))) {
		return EINVAL;
	}

	/* IPC_SET and IPC_RMID as its own permission check */
	if (cmd != IPC_SET && cmd != IPC_RMID) {
		/* check read permission */
		if (!check_perm(&sem->semid_ds.sem_perm, who_e, 0444))
			return EACCES;
	}

	switch (cmd) {
	case IPC_STAT:
		ds = (struct semid_ds *) opt;
		if (!ds)
			return EFAULT;
		r = sys_datacopy(SELF, (vir_bytes) &sem->semid_ds,
			who_e, (vir_bytes) ds, sizeof(struct semid_ds));
		if (r != OK)
			return EINVAL;
		break;
	case IPC_SET:
		uid = getnuid(who_e);
		if (uid != sem->semid_ds.sem_perm.cuid &&
			uid != sem->semid_ds.sem_perm.uid &&
			uid != 0)
			return EPERM;
		ds = (struct semid_ds *) opt;
		r = sys_datacopy(who_e, (vir_bytes) ds,
			SELF, (vir_bytes) &tmp_ds, sizeof(struct semid_ds));
		if (r != OK)
			return EINVAL;
		sem->semid_ds.sem_perm.uid = tmp_ds.sem_perm.uid;
		sem->semid_ds.sem_perm.gid = tmp_ds.sem_perm.gid;
		sem->semid_ds.sem_perm.mode &= ~0777;
		sem->semid_ds.sem_perm.mode |= tmp_ds.sem_perm.mode & 0666;
		sem->semid_ds.sem_ctime = time(NULL);
		break;
	case IPC_RMID:
		uid = getnuid(who_e);
		if (uid != sem->semid_ds.sem_perm.cuid &&
			uid != sem->semid_ds.sem_perm.uid &&
			uid != 0)
			return EPERM;
		/* awaken all processes block in semop
		 * and remove the semaphore set.
		 */
		update_one_semaphore(sem, 1);
		break;
	case IPC_INFO:
		break;
	case SEM_INFO:
		break;
	case SEM_STAT:
		break;
	case GETALL:
		buf = malloc(sizeof(unsigned short) * sem->semid_ds.sem_nsems);
		if (!buf)
			return ENOMEM;
		for (i = 0; i < sem->semid_ds.sem_nsems; i++)
			buf[i] = sem->sems[i].semval;
		r = sys_datacopy(SELF, (vir_bytes) buf,
			who_e, (vir_bytes) opt,
			sizeof(unsigned short) * sem->semid_ds.sem_nsems);
		if (r != OK)
			return EINVAL;
		free(buf);
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
		if (!buf)
			return ENOMEM;
		r = sys_datacopy(who_e, (vir_bytes) opt,
			SELF, (vir_bytes) buf,
			sizeof(unsigned short) * sem->semid_ds.sem_nsems);
		if (r != OK)
			return EINVAL;
#ifdef DEBUG_SEM
		printf("SEMCTL: SETALL: opt: %lu\n", (vir_bytes) opt);
		for (i = 0; i < sem->semid_ds.sem_nsems; i++)
			printf("SEMCTL: SETALL val: [%d] %d\n", i, buf[i]);
#endif
		for (i = 0; i < sem->semid_ds.sem_nsems; i++) {
			if (buf[i] > SEMVMX) {
				free(buf);
				update_semaphores();
				return ERANGE;
			}
			sem->sems[i].semval = buf[i];
		}
		free(buf);
		/* awaken if possible */
		update_semaphores();
		break;
	case SETVAL:
		val = (int) opt;
		/* check write permission */
		if (!check_perm(&sem->semid_ds.sem_perm, who_e, 0222))
			return EACCES;
		if (num < 0 || num >= sem->semid_ds.sem_nsems)
			return EINVAL;
		if (val < 0 || val > SEMVMX)
			return ERANGE;
		sem->sems[num].semval = val;
#ifdef DEBUG_SEM
		printf("SEMCTL: SETVAL: %d %d\n", num, val);
#endif
		sem->semid_ds.sem_ctime = time(NULL);
		/* awaken if possible */
		update_semaphores();
		break;
	default:
		return EINVAL;
	}

	return OK;
}

/*===========================================================================*
 *				do_semop		     		     *
 *===========================================================================*/
int do_semop(message *m)
{
	int id, i, j, r;
	struct sembuf *sops;
	unsigned int nsops;
	struct sem_struct *sem;
	int no_reply = 0;

	id = m->m_lc_ipc_semop.id;
	nsops = m->m_lc_ipc_semop.size;

	r = EINVAL;
	if (!(sem = sem_find_id(id)))
		goto out;

	if (nsops <= 0)
		goto out;

	r = E2BIG;
	if (nsops > SEMOPM)
		goto out;

	/* check for read permission */
	r = EACCES;
	if (!check_perm(&sem->semid_ds.sem_perm, who_e, 0444))
		goto out;

	/* get the array from user application */
	r = ENOMEM;
	sops = malloc(sizeof(struct sembuf) * nsops);
	if (!sops)
		goto out_free;
	r = sys_datacopy(who_e, (vir_bytes) m->m_lc_ipc_semop.ops,
			SELF, (vir_bytes) sops,
			sizeof(struct sembuf) * nsops);
	if (r != OK) {
		r = EINVAL;
		goto out_free;
	}

#ifdef DEBUG_SEM
	for (i = 0; i < nsops; i++)
		printf("SEMOP: num:%d  op:%d  flg:%d\n",
			sops[i].sem_num, sops[i].sem_op, sops[i].sem_flg);
#endif
	/* check for value range */
	r = EFBIG;
	for (i = 0; i < nsops; i++)
		if (sops[i].sem_num >= sem->semid_ds.sem_nsems)
			goto out_free;

	/* check for duplicate number */
	r = EINVAL;
	for (i = 0; i < nsops; i++)
		for (j = i + 1; j < nsops; j++)
			if (sops[i].sem_num == sops[j].sem_num)
				goto out_free;

	/* check for EAGAIN error */
	r = EAGAIN;
	for (i = 0; i < nsops; i++) {
		int op_n, val;

		op_n = sops[i].sem_op;
		val = sem->sems[sops[i].sem_num].semval;

		if ((sops[i].sem_flg & IPC_NOWAIT) &&
				((!op_n && val) ||
				 (op_n < 0 &&
				  -op_n > val)))
			goto out_free;

	}
	/* there will be no errors left, so we can go ahead */
	for (i = 0; i < nsops; i++) {
		struct semaphore *s;
		int op_n;

		s = &sem->sems[sops[i].sem_num];
		op_n = sops[i].sem_op;

		s->sempid = getnpid(who_e);

		if (op_n > 0) {
			/* check for alter permission */
			r = EACCES;
			if (!check_perm(&sem->semid_ds.sem_perm, who_e, 0222))
				goto out_free;
			s->semval += sops[i].sem_op;
		} else if (!op_n) {
			if (s->semval) {
				/* put the process asleep */
				s->semzcnt++;
				s->zlist = realloc(s->zlist, sizeof(struct waiting) * s->semzcnt);
				if (!s->zlist) {
					printf("IPC: zero waiting list lost...\n");
					break;
				}
				s->zlist[s->semzcnt-1].who = who_e;
				s->zlist[s->semzcnt-1].val = op_n;

#ifdef DEBUG_SEM
				printf("SEMOP: Put into sleep... %d\n", who_e);
#endif
				no_reply++;
			}
		} else {
			/* check for alter permission */
			r = EACCES;
			if (!check_perm(&sem->semid_ds.sem_perm, who_e, 0222))
				goto out_free;
			if (s->semval >= -op_n)
				s->semval += op_n;
			else {
				/* put the process asleep */
				s->semncnt++;
				s->nlist = realloc(s->nlist, sizeof(struct waiting) * s->semncnt);
				if (!s->nlist) {
					printf("IPC: increase waiting list lost...\n");
					break;
				}
				s->nlist[s->semncnt-1].who = who_e;
				s->nlist[s->semncnt-1].val = -op_n;

				no_reply++;
			}
		}
	}

	r = OK;
out_free:
	free(sops);
out:
	/* if we reach here by errors
	 * or with no errors but we should reply back.
	 */
	if (r != OK || !no_reply) {
		m->m_type = r;

		ipc_sendnb(who_e, m);
	}

	/* awaken process if possible */
	update_semaphores();

	return 0;
}

/*===========================================================================*
 *				is_sem_nil		     		     *
 *===========================================================================*/
int is_sem_nil(void)
{
	return (sem_list_nr == 0);
}

/*===========================================================================*
 *				sem_process_vm_notify	     		     *
 *===========================================================================*/
void sem_process_vm_notify(void)
{
	endpoint_t pt;
	int r;

	while ((r = vm_query_exit(&pt)) >= 0) {
		/* for each enpoint 'pt', check whether it's waiting... */
		remove_process(pt);

		if (r == 0)
			break;
	}
	if (r < 0)
		printf("IPC: query exit error!\n");
}

