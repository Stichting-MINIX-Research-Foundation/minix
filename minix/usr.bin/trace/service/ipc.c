/* This file is concerned with the IPC server, not with kernel-level IPC. */

#include "inc.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

static void
put_key(struct trace_proc * proc, const char * name, key_t key)
{

	if (!valuesonly && key == IPC_PRIVATE)
		put_field(proc, name, "IPC_PRIVATE");
	else
		put_value(proc, name, "%ld", key);
}

static const struct flags ipcget_flags[] = {
	FLAG(IPC_CREAT),
	FLAG(IPC_EXCL),
};

static int
ipc_shmget_out(struct trace_proc * proc, const message * m_out)
{

	put_key(proc, "key", m_out->m_lc_ipc_shmget.key);
	put_value(proc, "size", "%zu", m_out->m_lc_ipc_shmget.size);
	put_flags(proc, "shmflg", ipcget_flags, COUNT(ipcget_flags), "0%o",
	    m_out->m_lc_ipc_shmget.flag);

	return CT_DONE;
}

static void
ipc_shmget_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_value(proc, NULL, "%d", m_in->m_lc_ipc_shmget.retid);
	else
		put_result(proc);
}

static const struct flags shmat_flags[] = {
	FLAG(SHM_RDONLY),
	FLAG(SHM_RND),
};

static int
ipc_shmat_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "shmid", "%d", m_out->m_lc_ipc_shmat.id);
	put_ptr(proc, "shmaddr", (vir_bytes)m_out->m_lc_ipc_shmat.addr);
	put_flags(proc, "shmflg", shmat_flags, COUNT(shmat_flags), "0x%x",
	    m_out->m_lc_ipc_shmat.flag);

	return CT_DONE;
}

static void
ipc_shmat_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_ptr(proc, NULL, (vir_bytes)m_in->m_lc_ipc_shmat.retaddr);
	else
		put_result(proc);
}

static int
ipc_shmdt_out(struct trace_proc * proc, const message * m_out)
{

	put_ptr(proc, "shmaddr", (vir_bytes)m_out->m_lc_ipc_shmdt.addr);

	return CT_DONE;
}

static void
put_shmctl_cmd(struct trace_proc * proc, const char * name, int cmd)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (cmd) {
		TEXT(IPC_RMID);
		TEXT(IPC_SET);
		TEXT(IPC_STAT);
		TEXT(SHM_STAT);
		TEXT(SHM_INFO);
		TEXT(IPC_INFO);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", cmd);
}

static const struct flags shm_mode_flags[] = {
	FLAG(SHM_DEST),
	FLAG(SHM_LOCKED),
};

static void
put_struct_shmid_ds(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct shmid_ds buf;
	int set;

	if (!put_open_struct(proc, name, flags, addr, &buf, sizeof(buf)))
		return;

	/* Is this an IPC_SET call?  Then print a small subset of fields.. */
	set = (flags & PF_ALT);

	put_open(proc, "shm_perm", 0, "{", ", ");

	put_value(proc, "uid", "%u", buf.shm_perm.uid);
	put_value(proc, "gid", "%u", buf.shm_perm.gid);
	if (!set && verbose > 0) {
		put_value(proc, "cuid", "%u", buf.shm_perm.cuid);
		put_value(proc, "cgid", "%u", buf.shm_perm.cgid);
	}
	put_flags(proc, "mode", shm_mode_flags, COUNT(shm_mode_flags),
	    "0%03o", buf.shm_perm.mode);

	put_close(proc, "}");

	if (!set) {
		put_value(proc, "shm_segsz", "%zu", buf.shm_segsz);
		if (verbose > 0) {
			put_value(proc, "shm_lpid", "%d", buf.shm_lpid);
			put_value(proc, "shm_cpid", "%d", buf.shm_cpid);
			put_time(proc, "shm_atime", buf.shm_atime);
			put_time(proc, "shm_dtime", buf.shm_dtime);
			put_time(proc, "shm_ctime", buf.shm_ctime);
		}
		put_value(proc, "shm_nattch", "%u", buf.shm_nattch);
	}

	put_close_struct(proc, set || verbose > 0);
}

static int
ipc_shmctl_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "shmid", "%d", m_out->m_lc_ipc_shmctl.id);
	put_shmctl_cmd(proc, "cmd", m_out->m_lc_ipc_shmctl.cmd);

	/* TODO: add support for the IPC_INFO and SHM_INFO structures.. */
	switch (m_out->m_lc_ipc_shmctl.cmd) {
	case IPC_STAT:
	case SHM_STAT:
		return CT_NOTDONE;

	case IPC_SET:
		put_struct_shmid_ds(proc, "buf", PF_ALT,
		    (vir_bytes)m_out->m_lc_ipc_shmctl.buf);

		return CT_DONE;

	default:
		put_ptr(proc, "buf", (vir_bytes)m_out->m_lc_ipc_shmctl.buf);

		return CT_DONE;
	}
}

static void
ipc_shmctl_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	switch (m_out->m_lc_ipc_shmctl.cmd) {
	case IPC_STAT:
	case SHM_STAT:
		put_struct_shmid_ds(proc, "buf", failed,
		    (vir_bytes)m_out->m_lc_ipc_shmctl.buf);
		put_equals(proc);

		break;
	}

	if (!failed) {
		switch (m_out->m_lc_ipc_shmctl.cmd) {
		case SHM_INFO:
		case SHM_STAT:
		case IPC_INFO:
			put_value(proc, NULL, "%d", m_in->m_lc_ipc_shmctl.ret);

			return;
		}
	}

	put_result(proc);
}

static int
ipc_semget_out(struct trace_proc * proc, const message * m_out)
{

	put_key(proc, "key", m_out->m_lc_ipc_semget.key);
	put_value(proc, "nsems", "%d", m_out->m_lc_ipc_semget.nr);
	put_flags(proc, "semflg", ipcget_flags, COUNT(ipcget_flags), "0%o",
	    m_out->m_lc_ipc_semget.flag);

	return CT_DONE;
}

static void
ipc_semget_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_value(proc, NULL, "%d", m_in->m_lc_ipc_semget.retid);
	else
		put_result(proc);
}

static void
put_semctl_cmd(struct trace_proc * proc, const char * name, int cmd)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (cmd) {
		TEXT(IPC_RMID);
		TEXT(IPC_SET);
		TEXT(IPC_STAT);
		TEXT(GETNCNT);
		TEXT(GETPID);
		TEXT(GETVAL);
		TEXT(GETALL);
		TEXT(GETZCNT);
		TEXT(SETVAL);
		TEXT(SETALL);
		TEXT(SEM_STAT);
		TEXT(SEM_INFO);
		TEXT(IPC_INFO);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", cmd);
}

static void
put_struct_semid_ds(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct semid_ds buf;
	int set;

	if (!put_open_struct(proc, name, flags, addr, &buf, sizeof(buf)))
		return;

	/* Is this an IPC_SET call?  Then print a small subset of fields.. */
	set = (flags & PF_ALT);

	put_open(proc, "sem_perm", 0, "{", ", ");

	put_value(proc, "uid", "%u", buf.sem_perm.uid);
	put_value(proc, "gid", "%u", buf.sem_perm.gid);
	if (!set && verbose > 0) {
		put_value(proc, "cuid", "%u", buf.sem_perm.cuid);
		put_value(proc, "cgid", "%u", buf.sem_perm.cgid);
	}
	put_value(proc, "mode", "0%03o", buf.sem_perm.mode);

	put_close(proc, "}");

	if (!set) {
		if (verbose > 0) {
			put_time(proc, "sem_otime", buf.sem_otime);
			put_time(proc, "sem_ctime", buf.sem_ctime);
		}
		put_value(proc, "sem_nsems", "%u", buf.sem_nsems);
	}

	put_close_struct(proc, set || verbose > 0);
}


static int
ipc_semctl_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "semid", "%d", m_out->m_lc_ipc_semctl.id);
	put_value(proc, "semnum", "%d", m_out->m_lc_ipc_semctl.num);
	put_semctl_cmd(proc, "cmd", m_out->m_lc_ipc_semctl.cmd);

	/* TODO: add support for the IPC_INFO and SEM_INFO structures.. */
	switch (m_out->m_lc_ipc_semctl.cmd) {
	case IPC_STAT:
	case SEM_STAT:
		return CT_NOTDONE;

	case IPC_SET:
		put_struct_semid_ds(proc, "buf", PF_ALT,
		    (vir_bytes)m_out->m_lc_ipc_semctl.opt);

		return CT_DONE;

	case IPC_INFO:
	case SEM_INFO:
		put_ptr(proc, "buf", (vir_bytes)m_out->m_lc_ipc_semctl.opt);

		return CT_DONE;

	case GETALL:
	case SETALL:
		put_ptr(proc, "array", (vir_bytes)m_out->m_lc_ipc_semctl.opt);

		return CT_DONE;

	case SETVAL:
		put_value(proc, "val", "%lu", m_out->m_lc_ipc_semctl.opt);

		return CT_DONE;

	default:
		return CT_DONE;
	}
}

static void
ipc_semctl_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	switch (m_out->m_lc_ipc_semctl.cmd) {
	case IPC_STAT:
	case SEM_STAT:
		put_struct_semid_ds(proc, "buf", failed,
		    (vir_bytes)m_out->m_lc_ipc_semctl.opt);
		put_equals(proc);

		break;
	}

	if (!failed) {
		switch (m_out->m_lc_ipc_semctl.cmd) {
		case GETNCNT:
		case GETPID:
		case GETVAL:
		case GETZCNT:
		case SEM_INFO:
		case SEM_STAT:
		case IPC_INFO:
			put_value(proc, NULL, "%d", m_in->m_lc_ipc_semctl.ret);
			return;
		}
	}
	put_result(proc);
}

static const struct flags sem_flags[] = {
	FLAG(IPC_NOWAIT),
	FLAG(SEM_UNDO),
};

static void
put_struct_sembuf(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct sembuf buf;
	int all;

	if (!put_open_struct(proc, name, flags, addr, &buf, sizeof(buf)))
		return;

	all = FALSE;
	put_value(proc, "sem_num", "%u", buf.sem_num);
	put_value(proc, "sem_op", "%d", buf.sem_op);
	if (verbose > 0 || (buf.sem_flg & ~SEM_UNDO) != 0) {
		put_flags(proc, "sem_flg", sem_flags, COUNT(sem_flags), "0x%x",
		   buf.sem_flg);
		all = TRUE;
	}

	put_close_struct(proc, all);
}

static void
put_sembuf_array(struct trace_proc * proc, const char * name, vir_bytes addr,
	size_t count)
{
	struct sembuf buf[SEMOPM]; /* about 600 bytes, so OK for the stack */
	size_t i;

	if (valuesonly > 1 || count > SEMOPM ||
	    mem_get_data(proc->pid, addr, &buf, count * sizeof(buf[0])) != 0) {
		put_ptr(proc, name, addr);

		return;
	}

	put_open(proc, name, PF_NONAME, "[", ", ");
	for (i = 0; i < count; i++)
		put_struct_sembuf(proc, NULL, PF_LOCADDR, (vir_bytes)&buf[i]);
	put_close(proc, "]");
}

static int
ipc_semop_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "semid", "%d", m_out->m_lc_ipc_semop.id);
	put_sembuf_array(proc, "sops", (vir_bytes)m_out->m_lc_ipc_semop.ops,
	    m_out->m_lc_ipc_semop.size);
	put_value(proc, "nsops", "%u", m_out->m_lc_ipc_semop.size);

	return CT_DONE;
}

#define IPC_CALL(c) [((IPC_ ## c) - IPC_BASE)]

static const struct call_handler ipc_map[] = {
	IPC_CALL(SHMGET) = HANDLER("shmget", ipc_shmget_out, ipc_shmget_in),
	IPC_CALL(SHMAT) = HANDLER("shmat", ipc_shmat_out, ipc_shmat_in),
	IPC_CALL(SHMDT) = HANDLER("shmdt", ipc_shmdt_out, default_in),
	IPC_CALL(SHMCTL) = HANDLER("shmctl", ipc_shmctl_out, ipc_shmctl_in),
	IPC_CALL(SEMGET) = HANDLER("semget", ipc_semget_out, ipc_semget_in),
	IPC_CALL(SEMCTL) = HANDLER("semctl", ipc_semctl_out, ipc_semctl_in),
	IPC_CALL(SEMOP) = HANDLER("semop", ipc_semop_out, default_in),
};

const struct calls ipc_calls = {
	.endpt = ANY,
	.base = IPC_BASE,
	.map = ipc_map,
	.count = COUNT(ipc_map)
};
