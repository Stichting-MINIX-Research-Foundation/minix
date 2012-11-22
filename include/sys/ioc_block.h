/*	sys/ioc_block.h - Block ioctl() command codes.
 *
 */

#ifndef _S_I_BLOCK_H
#define _S_I_BLOCK_H

#include <minix/ioctl.h>
#include <minix/btrace.h>

#define BIOCTRACEBUF	_IOW('b', 1, size_t)
#define BIOCTRACECTL	_IOW('b', 2, int)
#define BIOCTRACEGET	_IOR_BIG(3, btrace_entry[BTBUF_SIZE])

#endif /* _S_I_BLOCK_H */
