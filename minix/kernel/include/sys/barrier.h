#ifndef _SYS_BARRIER_H
#define _SYS_BARRIER_H

/* Compiler barrier - prevents compiler reordering instructions across this point */
#define kcompiler_barrier() __asm__ __volatile__("" ::: "memory")

#if defined(__i386__) || defined(__x86_64__)
/* Full memory barrier - ensures all prior memory ops complete before subsequent ones */
#define kmb()   __asm__ __volatile__("mfence" ::: "memory")
/* Read memory barrier - ensures all prior loads complete before subsequent loads */
#define krmb()  __asm__ __volatile__("lfence" ::: "memory")
/* Write memory barrier - ensures all prior stores complete before subsequent stores */
#define kwmb()  __asm__ __volatile__("sfence" ::: "memory")
#else
/* Fallback to compiler barriers for other architectures.
   This is often insufficient for true SMP safety on those architectures,
   requiring platform-specific implementations. */
#define kmb()   kcompiler_barrier()
#define krmb()  kcompiler_barrier()
#define kwmb()  kcompiler_barrier()
#endif

/*
 * Acquire/Release semantics for lock-free programming.
 * These ensure that operations before an atomic operation (acquire) are visible
 * before the atomic, and operations after an atomic operation (release) are
 * visible only after the atomic.
 * Using full memory barriers (kmb) provides strong ordering.
 * On some architectures, lighter-weight barriers might be used for pure
 * acquire or release semantics if available and appropriate.
 */
#define k_smp_mb__before_atomic() kmb()
#define k_smp_mb__after_atomic()  kmb()

#endif /* _SYS_BARRIER_H */
