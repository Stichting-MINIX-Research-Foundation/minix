#include "kmemory.h" // Assuming klib/include will be an include path for klib itself
#include <minix/kernel_types.h> // For k_size_t

/*
 * Implementation of kernel-space memory manipulation functions.
 */

void *kmemcpy(void *dest, const void *src, k_size_t n) {
    if (dest == NULL || src == NULL) {
        return dest; // Or handle error appropriately for kernel
    }
    // Use compiler builtin for potentially optimal code generation.
    return __builtin_memcpy(dest, src, n);
}

void *kmemset(void *s, int c, k_size_t n) {
    if (s == NULL) {
        return s; // Or handle error appropriately
    }
    // Use compiler builtin for potentially optimal code generation.
    return __builtin_memset(s, c, n);
}

void *kmemmove(void *dest, const void *src, k_size_t n) {
    // Ensure kernel_types.h is included for k_size_t if not already.
    // It should be via "kmemory.h" -> <minix/kernel_types.h>
    unsigned char *pd = (unsigned char *)dest;
    const unsigned char *ps = (const unsigned char *)src;

    if (pd == ps || n == 0) {
        return dest;
    }

    // Correct overlap detection: backward copy needed when
    // dest > src AND dest < src + n (i.e., dest is within src and src+n)
    if (pd > ps && pd < (ps + n)) {
        // Copy backwards
        pd = pd + n;
        ps = ps + n;
        while (n--) {
            *--pd = *--ps;
        }
    } else {
        // Forward copy is safe (src and dest do not overlap in a way that requires backward copy)
        // This includes cases where dest < src, or dest >= ps + n.
        while (n--) {
            *pd++ = *ps++;
        }
    }
    return dest;
}
