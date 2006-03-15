#
.sect .text; .sect .rom; .sect .data

! export symbols
.define _imgrd, _imgrd_size

.sect .data
_imgrd:
0:
#include "ramdisk/image.s"
1:

! Use local labels to compute the size of _imgrd.
_imgrd_size:
	.data4	[1b] - [0b]
