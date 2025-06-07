#include "include/kprintf.h" // Should resolve to klib/include/kprintf.h

// stdarg.h is included by kprintf.h for now.
// #include <stdarg.h>

/*
 * Kernel-space printf STUB implementations.
 * These are temporary.
 */

int kprintf_stub(const char *format, ...) {
    // va_list args;
    // va_start(args, format);
    // kvprintf_stub(format, args);
    // va_end(args);
    // For now, do nothing. A real kprintf is complex.
    (void)format; // Suppress unused parameter warning
    return 0;
}

int kvprintf_stub(const char *format, va_list ap) {
    // For now, do nothing.
    (void)format; // Suppress unused parameter warning
    (void)ap;     // Suppress unused parameter warning
    return 0;
}

// New stub implementations
int ksprintf_stub(char *buf, const char *format, ...) {
    (void)buf;
    (void)format;
    // A common stub behavior for sprintf is to just null-terminate the buffer.
    if (buf) {
        buf[0] = '\0';
    }
    return 0; // Number of characters written (excluding null)
}

int kvsprintf_stub(char *buf, const char *format, va_list ap) {
    (void)buf;
    (void)format;
    (void)ap;
    if (buf) {
        buf[0] = '\0';
    }
    return 0;
}

// Note: A real kprintf would interact with console drivers (e.g., serial, video).
// The `kputc` in utility.c is a starting point for character output.
// This stub avoids any such complexities for now.
