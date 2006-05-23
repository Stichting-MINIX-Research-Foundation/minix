/*
complete.h

Created:	July 1995 by Philip Homburg <philip@cs.vu.nl>
*/

unsigned char complete(EditLine *el, int ch);
unsigned char complete_list(EditLine *el, int ch);
unsigned char complete_or_list(EditLine *el, int ch);
unsigned char complete_expand(EditLine *el, int ch);

/*
 * $PchId: complete.h,v 1.1 2001/05/17 07:12:05 philip Exp $
 */
