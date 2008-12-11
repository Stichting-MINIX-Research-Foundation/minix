/*
sys/vm.h
*/

/* used in ioctl to tty for mapvm map and unmap request. */
struct mapreqvm
{
	int	flags;		/* reserved, must be 0 */
	off_t	phys_offset;	
	size_t	size;
	int	readonly;
	char	reserved[36];	/* reserved, must be 0 */
	void	*vaddr;		
	void	*vaddr_ret;	
};

