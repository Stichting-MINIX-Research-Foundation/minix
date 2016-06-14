/* MIB service - kern.c - implementation of the CTL_KERN subtree */

#include "mib.h"

#include <sys/svrctl.h>
#include <minix/sysinfo.h>
#include <machine/partition.h>

#include "servers/vfs/const.h"
#include "servers/vfs/dmap.h"

static char hostname[MAXHOSTNAMELEN], domainname[MAXHOSTNAMELEN];

/*
 * Verification for CTL_KERN KERN_SECURELVL.
 */
static int
mib_kern_securelvl(struct mib_call * call __unused, struct mib_node * node,
	void * ptr, size_t size __unused)
{
	int v;

	memcpy(&v, ptr, sizeof(v));

	/*
	 * Only ever allow the security level to be increased.  This is a mock
	 * implementation.  TODO: implement actual support for security levels.
	 */
	return (v >= node->node_int);
}

/*
 * Implementation of CTL_KERN KERN_CLOCKRATE.
 */
static ssize_t
mib_kern_clockrate(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	struct clockinfo clockinfo;

	memset(&clockinfo, 0, sizeof(clockinfo));

	clockinfo.hz = sys_hz();
	clockinfo.tick = 1000000 / clockinfo.hz;
	clockinfo.profhz = clockinfo.hz;
	clockinfo.stathz = clockinfo.hz;

	/*
	 * Number of microseconds that can be corrected per clock tick through
	 * adjtime(2).  The kernel allows correction of one clock tick per
	 * clock tick, which means it should be the same as .tick.. I think.
	 * TODO: get this from the kernel itself.
	 */
	clockinfo.tickadj = clockinfo.tick;

	return mib_copyout(oldp, 0, &clockinfo, sizeof(clockinfo));
}

/*
 * Implementation of CTL_KERN KERN_PROFILING.
 */
static ssize_t
mib_kern_profiling(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp __unused,
	struct mib_newp * newp __unused)
{

	/* As per sysctl(7).  We have a different profiling API. */
	return EOPNOTSUPP;
}

/*
 * Implementation of CTL_KERN KERN_HARDCLOCK_TICKS.
 */
static ssize_t
mib_kern_hardclock_ticks(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	int uptime;

	/*
	 * The number of hardclock (hardware clock driver) ticks is what we
	 * call the number of monotonic clock ticks AKA the uptime clock ticks.
	 */
	uptime = (int)getticks();

	return mib_copyout(oldp, 0, &uptime, sizeof(uptime));
}

/*
 * Implementation of CTL_KERN KERN_ROOT_DEVICE.
 */
static ssize_t
mib_kern_root_device(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	char name[PATH_MAX];
	struct sysgetenv sysgetenv;

	sysgetenv.key = __UNCONST("rootdevname");
	sysgetenv.keylen = strlen(sysgetenv.key) + 1;
	sysgetenv.val = name;
	sysgetenv.vallen = sizeof(name);

	if (svrctl(PMGETPARAM, &sysgetenv) != 0)
		return EINVAL;

	name[MIN(sysgetenv.vallen, sizeof(name) - 1)] = '\0';

	return mib_copyout(oldp, 0, name, strlen(name) + 1);
}

/*
 * Implementation of CTL_KERN KERN_CCPU.
 */
static ssize_t
mib_kern_ccpu(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	int ccpu;

	ccpu = (int)cpuavg_getccpu();

	return mib_copyout(oldp, 0, &ccpu, sizeof(ccpu));
}

/*
 * Implementation of CTL_KERN KERN_CP_TIME.
 */
static ssize_t
mib_kern_cp_time(struct mib_call * call, struct mib_node * node __unused,
	struct mib_oldp * oldp, struct mib_newp * newp __unused)
{
	uint64_t ticks[MINIX_CPUSTATES], sum[MINIX_CPUSTATES];
	unsigned int cpu;
	int i, r, do_sum;

	/*
	 * If a subnode is provided, it identifies the CPU number for which to
	 * return information.  If no subnode is provided, but a size is given
	 * that allows returning information for all CPUs, return information
	 * for all of them in an array.  If no such size is given either,
	 * return a summation of all CPU statistics.  Both we and the kernel
	 * are considering the number of configured CPUs (hw.ncpu).
	 */
	if (call->call_namelen > 1)
		return EINVAL;

	if (call->call_namelen == 1) {
		/* Do not bother saving on this call if oldp is NULL. */
		if ((r = sys_getcputicks(ticks, call->call_name[0])) != OK)
			return r;

		return mib_copyout(oldp, 0, ticks, sizeof(ticks));
	}

	if (oldp == NULL)
		return sizeof(ticks); /* implying a summation request */

	do_sum = (mib_getoldlen(oldp) == sizeof(ticks));

	if (do_sum)
		memset(&sum, 0, sizeof(sum));

	for (cpu = 0; cpu < CONFIG_MAX_CPUS; cpu++) {
		if ((r = sys_getcputicks(ticks, cpu)) != OK)
			return r;

		if (do_sum) {
			for (i = 0; i < MINIX_CPUSTATES; i++)
				sum[i] += ticks[i];
		} else {
			if ((r = mib_copyout(oldp, cpu * sizeof(ticks), ticks,
			    sizeof(ticks))) < 0)
				return r;
		}
	}

	if (do_sum)
		return mib_copyout(oldp, 0, sum, sizeof(sum));
	else
		return cpu * sizeof(ticks);
}

/*
 * Implementation of CTL_KERN KERN_CONSDEV.
 */
static ssize_t
mib_kern_consdev(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	dev_t dev;

	dev = makedev(TTY_MAJOR, CONS_MINOR);

	/* No support for legacy 32-bit requests. */
	return mib_copyout(oldp, 0, &dev, sizeof(dev));
}

/*
 * Verification for CTL_KERN KERN_FORKFSLEEP.
 */
static int
mib_kern_forkfsleep(struct mib_call * call __unused,
	struct mib_node * node __unused, void * ptr, size_t size __unused)
{
	int v;

	memcpy(&v, ptr, sizeof(v));

	return (v >= 0 && v <= MAXSLP * 1000); /* rules from NetBSD */
}

/*
 * Implementation of CTL_KERN KERN_DRIVERS.
 */
static ssize_t
mib_kern_drivers(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	struct dmap dmap_tab[NR_DEVICES];
	struct kinfo_drivers drivers[NR_DEVICES + 1];
	unsigned int count;
	devmajor_t maj;

	/*
	 * On MINIX3, we list only drivers that are actually running.
	 */

	if (getsysinfo(VFS_PROC_NR, SI_DMAP_TAB, dmap_tab,
	    sizeof(dmap_tab)) != OK)
		return EINVAL;

	count = 0;

	/*
	 * Compatibility hack.  NetBSD userland expects that the name of the
	 * PTY driver is "pts".  Add an extra entry for this purpose if needed.
	 */
	if (dmap_tab[PTY_MAJOR].dmap_driver != NONE &&
	    strcmp(dmap_tab[PTY_MAJOR].dmap_label, "pts")) {
		if (mib_inrange(oldp, 0)) {
			memset(&drivers[0], 0, sizeof(drivers[0]));
			strlcpy(drivers[count].d_name, "pts",
			    sizeof(drivers[0].d_name));
			drivers[count].d_bmajor = -1;
			drivers[count].d_cmajor = PTY_MAJOR;
		}
		count++;
	}

	for (maj = 0; maj < NR_DEVICES; maj++) {
		if (dmap_tab[maj].dmap_driver == NONE)
			continue;

		if (mib_inrange(oldp, sizeof(drivers[0]) * count)) {
			memset(&drivers[count], 0, sizeof(drivers[0]));

			strlcpy(drivers[count].d_name,
			    dmap_tab[maj].dmap_label,
			    sizeof(drivers[0].d_name));

			/*
			 * We do not know whether the device is a block device,
			 * character device, or both.  In any case, a driver
			 * has only one major number.
			 */
			drivers[count].d_bmajor = maj;
			drivers[count].d_cmajor = maj;
		}
		count++;
	}

	return mib_copyout(oldp, 0, drivers, count * sizeof(drivers[0]));
}

/*
 * Implementation of CTL_KERN KERN_BOOTTIME.
 */
static ssize_t
mib_kern_boottime(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	struct timeval tv;

	memset(&tv, 0, sizeof(tv));

	if (getuptime(NULL, NULL, &tv.tv_sec) != OK)
		return EINVAL;

	return mib_copyout(oldp, 0, &tv, sizeof(tv));
}

/*
 * Mock implementation of CTL_KERN KERN_SYSVIPC KERN_SYSVIPC_INFO.  Normally,
 * the IPC service overrides the entire "kern.ipc" subtree.  Therefore, this
 * function will only ever be called when the IPC service is *not* running.
 */
static ssize_t
mib_kern_ipc_info(struct mib_call * call, struct mib_node * node __unused,
	struct mib_oldp * oldp __unused, struct mib_newp * newp __unused)
{

	/* The caller must always specify the resouce type (sem/shm/msg). */
	if (call->call_namelen != 1)
		return EINVAL;

	return EOPNOTSUPP;
}

/* The CTL_KERN KERN_SYSVIPC nodes, when not overridden by the IPC service. */
static struct mib_node mib_kern_ipc_table[] = {
/* 1*/	[KERN_SYSVIPC_INFO]	= MIB_FUNC(_P | _RO | CTLTYPE_NODE, 0,
				    mib_kern_ipc_info, "sysvipc_info",
				    "System V style IPC information"),
/* 2*/	[KERN_SYSVIPC_MSG]	= MIB_INT(_P | _RO, 0, "sysvmsg", "System V "
				    "style message support available"),
/* 3*/	[KERN_SYSVIPC_SEM]	= MIB_INT(_P | _RO, 0, "sysvsem", "System V "
				    "style semaphore support available"),
/* 4*/	[KERN_SYSVIPC_SHM]	= MIB_INT(_P | _RO, 0, "sysvshm", "System V "
				    "style shared memory support available"),
};

/* The CTL_KERN nodes. */
static struct mib_node mib_kern_table[] = {
/* 1*/	[KERN_OSTYPE]		= MIB_STRING(_P | _RO, OS_NAME, "ostype",
				    "Operating system type"),
/* 2*/	[KERN_OSRELEASE]	= MIB_STRING(_P | _RO, OS_RELEASE, "osrelease",
				    "Operating system release"),
/* 3*/	[KERN_OSREV]		= MIB_INT(_P | _RO , OS_REV, "osrevision",
				    "Operating system revision"),
/* 4*/	[KERN_VERSION]		= MIB_STRING(_P | _RO, OS_VERSION, "version",
				    "Kernel version"),
/* 5*/	[KERN_MAXVNODES]	= MIB_INT(_P | _RO, NR_VNODES, "maxvnodes",
				    "Maximum number of vnodes"),
/* 6*/	[KERN_MAXPROC]		= MIB_INT(_P | _RO, NR_PROCS, "maxproc",
				    "Maximum number of simultaneous "
				    "processes"),
/* 7*/	[KERN_MAXFILES]		= MIB_INT(_P | _RO, NR_VNODES, "maxfiles",
				    "Maximum number of open files"),
/* 8*/	[KERN_ARGMAX]		= MIB_INT(_P | _RO, ARG_MAX, "argmax",
				    "Maximum number of bytes of arguments to "
				    "execve(2)"),
/* 9*/	[KERN_SECURELVL]	= MIB_INTV(_P | _RW, -1, mib_kern_securelvl,
				    "securelevel", "System security level"),
/*10*/	[KERN_HOSTNAME]		= MIB_STRING(_P | _RW, hostname, "hostname",
				    "System hostname"),
/*11*/	[KERN_HOSTID]		= MIB_INT(_P | _RW | CTLFLAG_HEX, 0, "hostid",
				    "System host ID number"),
/*12*/	[KERN_CLOCKRATE]	= MIB_FUNC(_P | _RO | CTLTYPE_STRUCT,
				    sizeof(struct clockinfo),
				    mib_kern_clockrate, "clockrate",
				    "Kernel clock rates"),
/*13*/	/* KERN_VNODE: not yet implemented */
/*14*/	/* KERN_PROC: not yet implemented */
/*15*/	/* KERN_FILE: not yet implemented */
/*16*/	[KERN_PROF]		= MIB_FUNC(_P | _RO | CTLTYPE_NODE, 0,
				    mib_kern_profiling, "profiling",
				    "Profiling information (not available)"),
/*17*/	[KERN_POSIX1]		= MIB_INT(_P | _RO, _POSIX_VERSION,
				    "posix1version", "Version of ISO/IEC 9945 "
				    "(POSIX 1003.1) with which the operating "
				    "system attempts to comply"),
/*18*/	[KERN_NGROUPS]		= MIB_INT(_P | _RO, NGROUPS_MAX, "ngroups",
				    "Maximum number of supplemental groups"),
/*19*/	[KERN_JOB_CONTROL]	= MIB_INT(_P | _RO, 0, "job_control",
				    "Whether job control is available"),
/*20*/	[KERN_SAVED_IDS]	= MIB_INT(_P | _RO, 0, "saved_ids",
				    "Whether POSIX saved set-group/user ID is "
				    "available"),
/*21*/	/* KERN_OBOOTTIME: obsolete */
/*22*/	[KERN_DOMAINNAME]	= MIB_STRING(_P | _RW, domainname,
				    "domainname", "YP domain name"),
/*23*/	[KERN_MAXPARTITIONS]	= MIB_INT(_P | _RO, NR_PARTITIONS,
				    "maxpartitions", "Maximum number of "
				    "partitions allowed per disk"),
/*24*/	/* KERN_RAWPARTITION: incompatible with our device node scheme */
/*25*/	/* KERN_NTPTIME: not yet supported */
/*26*/	/* KERN_TIMEX: not yet supported */
/*27*/	/* KERN_AUTONICETIME: not yet supported */
/*28*/	/* KERN_AUTONICEVAL: not yet supported */
/*29*/	[KERN_RTC_OFFSET]	= MIB_INT(_P | _RW, 0, "rtc_offset", "Offset "
				    "of real time clock from UTC in minutes"),
/*30*/	[KERN_ROOT_DEVICE]	= MIB_FUNC(_P | _RO | CTLTYPE_STRING, 0,
				    mib_kern_root_device, "root_device",
				    "Name of the root device"),
/*31*/	[KERN_MSGBUFSIZE]	= MIB_INT(_P | _RO, DIAG_BUFSIZE, "msgbufsize",
				    "Size of the kernel message buffer"),
/*32*/	[KERN_FSYNC]		= MIB_INT(_P | _RO, 1, "fsync", "Whether the "
				    "POSIX 1003.1b File Synchronization Option"
				    " is available on this system"),
/*33*/	/* KERN_OLDSYSVMSG: obsolete */
/*34*/	/* KERN_OLDSYSVSEM: obsolete */
/*35*/	/* KERN_OLDSYSVSHM: obsolete */
/*36*/	/* KERN_OLDSHORTCORENAME: obsolete */
/*37*/	[KERN_SYNCHRONIZED_IO]	= MIB_INT(_P | _RO, 0, "synchronized_io",
				    "Whether the POSIX 1003.1b Synchronized "
				    "I/O Option is available on this system"),
/*38*/	[KERN_IOV_MAX]		= MIB_INT(_P | _RO, IOV_MAX, "iov_max",
				    "Maximum number of iovec structures per "
				    "process"),
/*39*/	/* KERN_MBUF: not yet supported */
/*40*/	[KERN_MAPPED_FILES]	= MIB_INT(_P | _RO, 1, "mapped_files",
				    "Whether the POSIX 1003.1b Memory Mapped "
				    "Files Option is available on this "
				    "system"),
/*41*/	[KERN_MEMLOCK]		= MIB_INT(_P | _RO, 0, "memlock", "Whether "
				    "the POSIX 1003.1b Process Memory Locking "
				    "Option is available on this system"),
/*42*/	[KERN_MEMLOCK_RANGE]	= MIB_INT(_P | _RO, 0, "memlock_range",
				    "Whether the POSIX 1003.1b Range Memory "
				    "Locking Option is available on this "
				    "system"),
/*43*/	[KERN_MEMORY_PROTECTION]= MIB_INT(_P | _RO, 0, "memory_protection",
				    "Whether the POSIX 1003.1b Memory "
				    "Protection Option is available on this "
				    "system"),
/*44*/	/* KERN_LOGIN_NAME_MAX: not yet supported */
/*45*/	/* KERN_DEFCORENAME: obsolete */
/*46*/	/* KERN_LOGSIGEXIT: not yet supported */
/*47*/	[KERN_PROC2]		= MIB_FUNC(_P | _RO | CTLTYPE_NODE, 0,
				    mib_kern_proc2, "proc2",
				    "Machine-independent process information"),
/*48*/	[KERN_PROC_ARGS]	= MIB_FUNC(_P | _RO | CTLTYPE_NODE, 0,
				    mib_kern_proc_args, "proc_args",
				    "Process argument information"),
/*49*/	[KERN_FSCALE]		= MIB_INT(_P | _RO, FSCALE, "fscale",
				    "Kernel fixed-point scale factor"),
/*50*/	[KERN_CCPU]		= MIB_FUNC(_P | _RO | CTLTYPE_INT, sizeof(int),
				    mib_kern_ccpu, "ccpu",
				    "Scheduler exponential decay value"),
/*51*/	[KERN_CP_TIME]		= MIB_FUNC(_P | _RO | CTLTYPE_NODE, 0,
				    mib_kern_cp_time, "cp_time", "Clock ticks "
				    "spent in different CPU states"),
/*52*/	/* KERN_OLDSYSVIPC_INFO: obsolete */
/*53*/	/* KERN_MSGBUF: not yet supported */
/*54*/	[KERN_CONSDEV]		= MIB_FUNC(_P | _RO | CTLTYPE_STRUCT,
				    sizeof(dev_t), mib_kern_consdev, "consdev",
				    "Console device"),
/*55*/	[KERN_MAXPTYS]		= MIB_INT(_P | _RO, NR_PTYS, "maxptys",
				    "Maximum number of pseudo-ttys"),
/*56*/	/* KERN_PIPE: not yet supported */
/*57*/	[KERN_MAXPHYS]		= MIB_INT(_P | _RO, 4*1024*1024, "maxphys",
				    "Maximum raw I/O transfer size"),
				    /* 4MB is the upper limit for AHCI */
/*58*/	/* KERN_SBMAX: not yet supported */
/*59*/	/* KERN_TKSTAT: not yet supported */
/*60*/	[KERN_MONOTONIC_CLOCK]	= MIB_INT(_P | _RO, _POSIX_MONOTONIC_CLOCK,
				    "monotonic_clock",
				    "Implementation version of the POSIX "
				    "1003.1b Monotonic Clock Option"),
/*61*/	/* KERN_URND: not yet supported */
/*62*/	/* KERN_LABELSECTOR: not yet supported */
/*63*/	/* KERN_LABELOFFSET: not yet supported */
/*64*/	[KERN_LWP]		= MIB_FUNC(_P | _RO | CTLTYPE_NODE, 0,
				    mib_kern_lwp, "lwp",
				    "System-wide LWP information"),
/*65*/	[KERN_FORKFSLEEP]	= MIB_INTV(_P | _RW, 0, mib_kern_forkfsleep,
				    "forkfsleep", "Milliseconds to sleep on "
				    "fork failure due to process limits"),
/*66*/	/* KERN_POSIX_THREADS: not yet supported */
/*67*/	/* KERN_POSIX_SEMAPHORES: not yet supported */
/*68*/	/* KERN_POSIX_BARRIERS: not yet supported */
/*69*/	/* KERN_POSIX_TIMERS: not yet supported */
/*70*/	/* KERN_POSIX_SPIN_LOCKS: not yet supported */
/*71*/	/* KERN_POSIX_READER_WRITER_LOCKS: not yet supported */
/*72*/	[KERN_DUMP_ON_PANIC]	= MIB_INT(_P | _RO, 0, "dump_on_panic",
				    "Perform a crash dump on system panic"),
/*73*/	/* KERN_SOMAXKVA: not yet supported */
/*74*/	/* KERN_ROOT_PARTITION: incompatible with our device node scheme */
/*75*/	[KERN_DRIVERS]		= MIB_FUNC(_P | _RO | CTLTYPE_STRUCT, 0,
				    mib_kern_drivers, "drivers",
				    "List of all drivers with block and "
				    "character device numbers"),
/*76*/	/* KERN_BUF: not yet supported */
/*77*/	/* KERN_FILE2: not yet supported */
/*78*/	/* KERN_VERIEXEC: not yet supported */
/*79*/	/* KERN_CP_ID: not yet supported */
/*80*/	[KERN_HARDCLOCK_TICKS]	= MIB_FUNC(_P | _RO | CTLFLAG_UNSIGNED |
				    CTLTYPE_INT, sizeof(int),
				    mib_kern_hardclock_ticks,
				    "hardclock_ticks",
				    "Number of hardclock ticks"),
/*81*/	/* KERN_ARND: not yet supported */
/*82*/	[KERN_SYSVIPC]		= MIB_NODE(_P | _RO, mib_kern_ipc_table, "ipc",
				    "SysV IPC options"),
/*83*/	[KERN_BOOTTIME]		= MIB_FUNC(_P | _RO | CTLTYPE_STRUCT,
				    sizeof(struct timeval), mib_kern_boottime,
				    "boottime", "System boot time"),
/*84*/	/* KERN_EVCNT: not yet supported */
};

/*
 * Initialize the CTL_KERN subtree.
 */
void
mib_kern_init(struct mib_node * node)
{

	MIB_INIT_ENODE(node, mib_kern_table);
}
