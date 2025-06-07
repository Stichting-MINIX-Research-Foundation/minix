#include <sys/kassert.h>          // For kpanic's own declaration
#include <klib/include/kprintf.h> // For kprintf_stub
#include <stdarg.h>               // For ... in kpanic signature, even if va_list not fully used by stub

// Forward declarations for HAL functions.
// These are expected to be provided by architecture-specific code.
// TODO: Replace these with includes of a proper HAL header when available.
extern void arch_disable_interrupts(void);
extern void arch_halt(void) __attribute__((noreturn));

// Definition of kpanic
void kpanic(const char *fmt, ...) {
    // Disable interrupts as early as possible
    arch_disable_interrupts();

    // Basic message indicating panic
    kprintf_stub("KERNEL PANIC: "); // kprintf_stub is from <klib/include/kprintf.h>

    // Handle variadic arguments for the format string
    // This is a simplified handling due to kprintf_stub's limitations.
    // A proper kvprintf is needed to format this correctly.
    if (fmt) {
        kprintf_stub(fmt); // This will print the raw format string
        // TODO: When a real kvprintf is available, use it here:
        // va_list ap;
        // va_start(ap, fmt);
        // kvprintf_real(fmt, ap); // Call the real kvprintf
        // va_end(ap);
    }
    kprintf_stub("\n");

    // Halt the system
    arch_halt();
}
