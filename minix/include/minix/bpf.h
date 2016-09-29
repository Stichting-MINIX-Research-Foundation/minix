#ifndef _MINIX_BPF_H
#define _MINIX_BPF_H

#include <net/bpf.h>

/*
 * MINIX3-specific extensions to the NetBSD Berkeley Packet Filter header.
 * These extensions are necessary because NetBSD BPF uses a few ioctl(2)
 * structure formats that contain pointers--something that MINIX3 has to avoid,
 * due to its memory granting mechanisms.  Thus, those ioctl(2) calls have to
 * be converted from NetBSD to MINIX3 format.  We currently do that in libc.
 * This header specifies the numbers and formats for the MINIX3 versions.
 *
 * See <minix/if.h> for details on how things work here.
 */

/* BIOCSETF: set BPF filter program. */
/*
 * This ioctl is an exception, as it is write-only, so we do not need the
 * original structure.  Also, the size of this structure is currently slightly
 * over 4KB, which makes it too big for a regular ioctl call.  Thus, we have to
 * use a big ioctl call.  Note that future changes of BPF_MAXINSNS will
 * unfortunately (necessarily) change the ioctl call number.
 */
struct minix_bpf_program {
	u_int mbf_len;
	struct bpf_insn mbf_insns[BPF_MAXINSNS];
};

#define MINIX_BIOCSETF		_IOW_BIG(2, struct minix_bpf_program)

/* BIOCGDLTLIST: retrieve list of possible data link types. */
#define MINIX_BPF_MAXDLT	256

struct minix_bpf_dltlist {
	struct bpf_dltlist mbfl_dltlist;		/* MUST be first */
	u_int mbfl_list[MINIX_BPF_MAXDLT];
};

#define MINIX_BIOCGDLTLIST	_IOWR('B', 119, struct minix_bpf_dltlist)

#endif /* !_MINIX_BPF_H */
