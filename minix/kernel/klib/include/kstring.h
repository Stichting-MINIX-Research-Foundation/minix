#ifndef _KERNEL_KLIB_KSTRING_H
#define _KERNEL_KLIB_KSTRING_H

#include <minix/kernel_types.h> // For k_size_t

/*
 * Kernel-space string manipulation functions.
 * These functions are designed to be simple, safe, and have no
 * external dependencies beyond what's provided by the kernel itself.
 */

/*
 * Calculates the length of a null-terminated string.
 * @param s The string to measure.
 * @return The number of characters in s, excluding the null terminator.
 */
k_size_t kstrlen(const char *s);

/*
 * Copies a string with size limitation, ensuring null termination.
 * Similar to strlcpy, it's designed to be safer than strncpy.
 * @param dst Destination buffer.
 * @param src Source string.
 * @param size Full size of the destination buffer.
 * @return The length of the source string (src). This allows truncation detection:
 *         if retval >= size, truncation occurred.
 */
k_size_t kstrlcpy(char *dst, const char *src, k_size_t size);

/*
 * Compares two null-terminated strings lexicographically.
 * @param s1 The first string.
 * @param s2 The second string.
 * @return An integer less than, equal to, or greater than zero if s1 is found,
 *         respectively, to be less than, to match, or be greater than s2.
 */
int kstrcmp(const char *s1, const char *s2);

#endif /* _KERNEL_KLIB_KSTRING_H */
