#ifndef __SYS_VM_H__
#define __SYS_VM_H__

/*
sys/vm.h
*/

/* MIOCMAP */
struct mapreq
{
	void *base;
	size_t size;
	off_t offset;
	int readonly;
};

/* used in ioctl to tty for mapvm map and unmap request. */
struct mapreqvm
{
	int	flags;		/* reserved, must be 0 */
	phys_bytes phys_offset;
	size_t	size;
	int	readonly;
	char	reserved[36];	/* reserved, must be 0 */
	void	*vaddr;		
	void	*vaddr_ret;	
};

#endif /* __SYS_VM_H__ */
