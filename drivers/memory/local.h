/*
local defines and declarations 
*/

extern unsigned char _binary_imgrd_mfs_start, _binary_imgrd_mfs_end;

#define	imgrd	&_binary_imgrd_mfs_start
#define	imgrd_size \
	(((size_t) &_binary_imgrd_mfs_end - (size_t)&_binary_imgrd_mfs_start))
