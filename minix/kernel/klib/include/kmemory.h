#ifndef _KERNEL_KLIB_KMEMORY_H
#define _KERNEL_KLIB_KMEMORY_H

#include <minix/kernel_types.h> // For k_size_t

/*
 * Kernel-space memory manipulation functions.
 * These are designed for internal kernel use.
 */

/*
 * Copies 'n' bytes from memory area 'src' to memory area 'dest'.
 * The memory areas must not overlap; use kmemmove for overlapping areas.
 * @param dest Pointer to the destination array where the content is to be copied.
 * @param src Pointer to the source of data to be copied.
 * @param n Number of bytes to copy.
 * @return A pointer to the destination buffer ('dest').
 */
void *kmemcpy(void *dest, const void *src, k_size_t n);

/*
 * Fills the first 'n' bytes of the memory area pointed to by 's'
 * with the constant byte 'c'.
 * @param s Pointer to the block of memory to fill.
 * @param c Value to be set. The value is passed as an int, but the function fills
 *          the block of memory using the unsigned char conversion of this value.
 * @param n Number of bytes to be set to the value.
 * @return A pointer to the memory area 's'.
 */
void *kmemset(void *s, int c, k_size_t n);

/*
 * Copies 'n' bytes from memory area 'src' to memory area 'dest'.
 * The memory areas may overlap.
 * @param dest Pointer to the destination array where the content is to be copied.
 * @param src Pointer to the source of data to be copied.
 * @param n Number of bytes to copy.
 * @return A pointer to the destination buffer ('dest').
 */
void *kmemmove(void *dest, const void *src, k_size_t n);

#endif /* _KERNEL_KLIB_KMEMORY_H */
