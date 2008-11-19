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


/* MIOCMAPVM */
struct mapreqvm
{
	int	flags;		/* reserved, must be 0 */
	off_t	phys_offset;
	size_t	size;
	int	readonly;
	char	reserved[40];	/* reserved, must be 0 */
	void	*vaddr_ret;	/* result vaddr */
};
