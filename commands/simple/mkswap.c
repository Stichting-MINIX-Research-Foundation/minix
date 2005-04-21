/*	mkswap 1.0 - Initialize a swap partition or file
 *							Author: Kees J. Bot
 *								6 Jan 2001
 */
#define nil ((void*)0)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/u64.h>
#include <minix/partition.h>
#include <minix/swap.h>
#include <servers/fs/const.h>
#include <servers/fs/type.h>
#include <servers/fs/super.h>

static void usage(void)
{
    fprintf(stderr, "Usage: mkswap [-f] device-or-file [size[km]]\n");
    exit(1);
}

int main(int argc, char **argv)
{
    int first;
    int i;
    char *file;
    unsigned long offset, size, devsize;
    int fd;
    struct stat st;
    ssize_t r;
    struct super_block super;
    swap_hdr_t swap_hdr;
    static u8_t block[MAX_BLOCK_SIZE];

    first= 0;
    i= 1;
    while (i < argc && argv[i][0] == '-') {
	char *opt= argv[i++]+1;

	if (opt[0] == '-' && opt[1] == 0) break;	/* -- */

	while (*opt != 0) switch (*opt++) {
	case 'f':	first= 1;	break;
	default:	usage();
	}
    }
    if (i == argc) usage();
    file= argv[i++];

    size= 0;
    if (i < argc) {
	char *end;
	unsigned long m;

	size= strtoul(argv[i], &end, 10);
	if (end == argv[i]) usage();
	m= 1024;
	if (*end != 0) {
	    switch (*end) {
	    case 'm':	case 'M':	m *= 1024;	/*FALL THROUGH*/
	    case 'k':	case 'K':	end++;		break;
	    }
	}
	if (*end != 0 || size == -1
	    || (size * m) / m != size || (size *= m) <= SWAP_OFFSET
	) {
	    fprintf(stderr, "mkswap: %s: Bad size\n", argv[i]);
	    exit(1);
	}
	i++;
    }
    if (i != argc) usage();

    /* Open the device or file. */
    if ((fd= open(file, O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0) {
	fprintf(stderr, "mkswap: Can't open %s: %s\n", file, strerror(errno));
	exit(1);
    }

    /* File or device? */
    (void) fstat(fd, &st);
    if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) {
	struct partition part;

	/* How big is the partition? */
	if (ioctl(fd, DIOCGETP, &part) < 0) {
	    fprintf(stderr, "mkswap: Can't determine the size of %s: %s\n",
		file, strerror(errno));
	    exit(1);
	}
	devsize= cv64ul(part.size);
	offset= 0;

	if (!first) {
	    /* Is there a file system? */
	    r= -1;
	    if (lseek(fd, SUPER_BLOCK_BYTES, SEEK_SET) == -1
		|| (r= read(fd, block, STATIC_BLOCK_SIZE)) < STATIC_BLOCK_SIZE
	    ) {
		fprintf(stderr, "mkswap: %s: %s\n",
		    file, r >= 0 ? "End of file" : strerror(errno));
		exit(1);
	    }
	    memcpy(&super, block, sizeof(super));
	    if (super.s_magic == SUPER_MAGIC) {
		offset= (unsigned long) super.s_nzones * STATIC_BLOCK_SIZE;
	    } else
	    if (super.s_magic == SUPER_V2) {
		offset= (unsigned long) super.s_zones * STATIC_BLOCK_SIZE;
	    } else if (super.s_magic == SUPER_V3) {
		offset= (unsigned long) super.s_zones * super.s_block_size;
	    } else {
		first= 1;
	    }
	}
	if (size == 0) size= devsize - offset;
	if (size == 0 || offset + size > devsize) {
	    fprintf(stderr, "mkswap: There is no room on %s for ", file);
	    if (size > 0) fprintf(stderr, "%lu kilobytes of ", size/1024);
	    fprintf(stderr, "swapspace\n");
	    if (offset > 0) {
		fprintf(stderr, "(Use the -f flag to wipe the file system)\n");
	    }
	    exit(1);
	}
    } else
    if (S_ISREG(st.st_mode)) {
	/* Write to the swap file to guarantee space for MM. */
	unsigned long n;

	if (size == 0) {
	    fprintf(stderr, "mkswap: No size specified for %s\n", file);
	    usage();
	}

	memset(block, 0, sizeof(block));
	for (n= 0; n < size; n += r) {
	    r= size > sizeof(block) ? sizeof(block) : size;
	    if ((r= write(fd, block, r)) <= 0) {
		fprintf(stderr, "mkswap: %s: %s\n",
		    file, r == 0 ? "End of file" : strerror(errno));
		exit(1);
	    }
	}
	first= 1;
    } else {
	fprintf(stderr, "mkswap: %s is not a device or a file\n", file);
	exit(1);
    }

    if (offset < SWAP_OFFSET) {
	offset += SWAP_OFFSET;
	if (size < SWAP_OFFSET) size= 0; else size -= SWAP_OFFSET;
    }
    swap_hdr.sh_magic[0]= SWAP_MAGIC0;
    swap_hdr.sh_magic[1]= SWAP_MAGIC1;
    swap_hdr.sh_magic[2]= SWAP_MAGIC2;
    swap_hdr.sh_magic[3]= SWAP_MAGIC3;
    swap_hdr.sh_version= SH_VERSION;
    swap_hdr.sh_priority= 0;
    swap_hdr.sh_offset= offset;
    swap_hdr.sh_swapsize= size;

    r= -1;
    if (lseek(fd, SWAP_BOOTOFF, SEEK_SET) == -1
	|| (r= read(fd, block, sizeof(block))) < sizeof(block)
    ) {
	fprintf(stderr, "mkswap: %s: %s\n", file,
	    file, r >= 0 ? "End of file" : strerror(errno));
	exit(1);
    }

    r= (first ? SWAP_BOOTOFF : OPTSWAP_BOOTOFF) - SWAP_BOOTOFF;
    memcpy(block + r, &swap_hdr, sizeof(swap_hdr));

    r= -1;
    if (lseek(fd, SWAP_BOOTOFF, SEEK_SET) == -1
	|| (r= write(fd, block, sizeof(block))) < sizeof(block)
    ) {
	fprintf(stderr, "mkswap: %s: %s\n", file,
	    file, r >= 0 ? "End of file" : strerror(errno));
	exit(1);
    }
    printf("%s: swapspace at offset %lu, size %lu kilobytes\n",
	file, offset / 1024, size / 1024);
    return 0;
}
