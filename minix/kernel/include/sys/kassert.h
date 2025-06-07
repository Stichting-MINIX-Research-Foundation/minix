#ifndef _SYS_KASSERT_H
#define _SYS_KASSERT_H

// Include kernel_types.h if kpanic might eventually need types from it,
// or if __FILE__, __LINE__, __func__ relate to types defined there (unlikely).
// For now, not strictly needed by the macros themselves.
// #include <minix/kernel_types.h>

/* Kernel panic function - defined in kpanic.c */
/* The format attribute helps the compiler check printf-style arguments. */
void kpanic(const char *fmt, ...) __attribute__((noreturn, format (printf, 1, 2) ));

// DEBUG_KERNEL flag will control if KASSERT is active.
// This should be defined in the build system for debug builds.
#ifdef DEBUG_KERNEL
#define KASSERT(cond) do { \
    if (!(cond)) { \
        kpanic("KASSERT failed: %s\n  at %s:%d in %s()", \
               #cond, __FILE__, __LINE__, __func__); \
    } \
} while (0)
#else
#define KASSERT(cond) ((void)0)
#endif

/* Compile-time assertion (requires C11 or later) */
#define KSTATIC_ASSERT(cond) _Static_assert(cond, #cond)

#endif /* _SYS_KASSERT_H */
