/* Implementation of CPU local variables generics */
#ifndef __CPULOCALS_H__
#define __CPULOCALS_H__

#ifndef __ASSEMBLY__

#ifdef CONFIG_SMP

/* SMP */

#define CPULOCAL_ARRAY	[CONFIG_MAX_CPUS]

#define get_cpu_var(cpu, name)		CPULOCAL_STRUCT[cpu].name
#define get_cpu_var_ptr(cpu, name)	(&(get_cpu_var(cpu, name)))
#define get_cpulocal_var(name)		get_cpu_var(cpuid, name)
#define get_cpulocal_var_ptr(name)	get_cpu_var_ptr(cpuid, name)

/* FIXME - padd the structure so that items in the array do not share cacheline
 * with other cpus */

#else

/* single CPU */

#define CPULOCAL_ARRAY

#define get_cpulocal_var(name)		CPULOCAL_STRUCT.name
#define get_cpulocal_var_ptr(name)	&(get_cpulocal_var(name))
#define get_cpu_var(cpu, name)		get_cpulocal_var(name)
#define get_cpu_var_ptr(cpu, name)	get_cpulocal_var_ptr(name)

#endif



#define DECLARE_CPULOCAL(type, name)	type name

#define CPULOCAL_STRUCT			__cpu_local_vars
#define ___CPULOCAL_START		struct CPULOCAL_STRUCT {
#define ___CPULOCAL_END		} CPULOCAL_STRUCT CPULOCAL_ARRAY;

#define DECLARE_CPULOCAL_START		extern ___CPULOCAL_START
#define DECLARE_CPULOCAL_END		___CPULOCAL_END

#define DEFINE_CPULOCAL_VARS	struct CPULOCAL_STRUCT CPULOCAL_STRUCT CPULOCAL_ARRAY


/*
 * The global cpu local variables in use
 */
DECLARE_CPULOCAL_START

/* Process scheduling information and the kernel reentry count. */
DECLARE_CPULOCAL(struct proc *,proc_ptr);/* pointer to currently running process */
DECLARE_CPULOCAL(struct proc *,bill_ptr);/* process to bill for clock ticks */
DECLARE_CPULOCAL(struct proc ,idle_proc);/* stub for an idle process */

/* 
 * signal whether pagefault is already being handled to detect recursive
 * pagefaults
 */
DECLARE_CPULOCAL(int, pagefault_handled);

/*
 * which processpage tables are loaded right now. We need to know this because
 * some processes are loaded in each process pagetables and don't have their own
 * pagetables. Therefore we cannot use the proc_ptr pointer
 */
DECLARE_CPULOCAL(struct proc *, ptproc);

/* CPU private run queues */
DECLARE_CPULOCAL(struct proc *, run_q_head[NR_SCHED_QUEUES]); /* ptrs to ready list headers */
DECLARE_CPULOCAL(struct proc *, run_q_tail[NR_SCHED_QUEUES]); /* ptrs to ready list tails */
DECLARE_CPULOCAL(volatile int, cpu_is_idle); /* let the others know that you are idle */

DECLARE_CPULOCAL(volatile int, idle_interrupted); /* to interrupt busy-idle
						     while profiling */

DECLARE_CPULOCAL(u64_t ,tsc_ctr_switch); /* when did we switched time accounting */

/* last values read from cpu when sending ooq msg to scheduler */
DECLARE_CPULOCAL(u64_t, cpu_last_tsc);
DECLARE_CPULOCAL(u64_t, cpu_last_idle);


DECLARE_CPULOCAL(char ,fpu_presence); /* whether the cpu has FPU or not */
DECLARE_CPULOCAL(struct proc * ,fpu_owner); /* who owns the FPU of the local cpu */

DECLARE_CPULOCAL_END

#endif /* __ASSEMBLY__ */

#endif /* __CPULOCALS_H__ */
