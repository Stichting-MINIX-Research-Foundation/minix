/*	$NetBSD: boot_params.S,v 1.6 2010/01/17 14:54:44 drochner Exp $	*/

/* Default boot parameters - must match struct x86_boot_params in bootblock.h */

#ifdef BOOTPARAM_DEFFLAGS
	.long	BOOTPARAM_DEFFLAGS
#else
	.long	0x0
#endif
	.long	5			/* timeout in seconds */
	.long	0			/* console device 0 => CONSDEV_PC */
#if !defined(__minix)
	.long	9600			/* serial baud rate */
#else
	.long	115200			/* serial baud rate */
#endif /* !defined(__minix) */
	.space	16			/* md5 boot password */
	.space	64			/* keyboard xlat map */
	.long	0			/* console ioaddr */
