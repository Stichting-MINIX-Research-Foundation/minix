/* $NetBSD: usbroothub_subr.h,v 1.1 2008/02/03 10:57:13 drochner Exp $ */

int usb_makestrdesc(usb_string_descriptor_t *, int, const char *);
int usb_makelangtbl(usb_string_descriptor_t *, int);
