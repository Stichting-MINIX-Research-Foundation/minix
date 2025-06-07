#include "kstring.h" // Assuming klib/include will be an include path for klib itself
#include <minix/kernel_types.h> // For k_size_t and other types if used implicitly

/*
 * Implementation of kernel-space string manipulation functions.
 */

/*
 * Calculates the length of a null-terminated string.
 */
k_size_t kstrlen(const char *s) {
    const char *p = s;
    if (!s) return 0; // Handle NULL pointer case
    while (*p) {
        p++;
    }
    return (k_size_t)(p - s);
}

/*
 * Copies a string with size limitation, ensuring null termination.
 */
k_size_t kstrlcpy(char *dst, const char *src, k_size_t size) {
    if (!dst || !src) return 0; // Handle NULL pointers

    // Calculate source length first, ensuring not to read out of bounds if src is huge
    // and not null-terminated within reasonable kernel limits.
    const char *s_counter = src;
    k_size_t src_len = 0;
    // Check against K_SIZE_T_MAX or similar if available, for now (k_size_t)-1
    // to prevent src_len wrap-around if k_size_t is unsigned.
    while (*s_counter && src_len < ((k_size_t)-1)) {
        s_counter++;
        src_len++;
    }

    if (size > 0) {
        k_size_t copy_len = (src_len < size - 1) ? src_len : size - 1;

        // Use compiler builtin for potentially optimal code generation.
        // No include needed for __builtin_memcpy, it's a compiler intrinsic.
        if (copy_len > 0) {
             __builtin_memcpy(dst, src, copy_len);
        }
        dst[copy_len] = '\0';
    }
    return src_len; // Return full length of src, as per strlcpy behavior
}

/*
 * Compares two null-terminated strings lexicographically.
 */
int kstrcmp(const char *s1, const char *s2) {
    if (s1 == s2) return 0; // If both point to the same address or both are NULL
    if (!s1) return -1; // Define behavior for NULL pointers (s1 < s2 if s1 is NULL)
    if (!s2) return 1;  // (s1 > s2 if s2 is NULL)

    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}
