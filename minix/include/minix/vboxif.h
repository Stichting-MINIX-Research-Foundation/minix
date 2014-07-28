#ifndef _MINIX_VBOXIF_H
#define _MINIX_VBOXIF_H

/*===========================================================================*
 *			Messages for VBOX device			     *
 *===========================================================================*/

/* Base type for VBOX requests and responses. */
#define VBOX_RQ_BASE	0x1600
#define VBOX_RS_BASE	0x1680

#define IS_VBOX_RQ(type) (((type) & ~0x7f) == VBOX_RQ_BASE)
#define IS_VBOX_RS(type) (((type) & ~0x7f) == VBOX_RS_BASE)

/* Message types for VBOX requests. */
#define VBOX_OPEN	(VBOX_RQ_BASE + 0)	/* open a connection */
#define VBOX_CLOSE	(VBOX_RQ_BASE + 1)	/* close a connection */
#define VBOX_CALL	(VBOX_RQ_BASE + 2)	/* perform a call */
#define VBOX_CANCEL	(VBOX_RQ_BASE + 3)	/* cancel an ongoing call */

/* Message types for VBOX responses. */
#define VBOX_REPLY	(VBOX_RS_BASE + 0)	/* general reply code */

/* Field names for VBOX messages. */
#define VBOX_CONN	m2_i1	/* connection identifier */
#define VBOX_GRANT	m2_i2	/* grant ID of buffer or name */
#define VBOX_COUNT	m2_i3	/* number of bytes or elements */
#define VBOX_RESULT	m2_i1	/* result or error code */
#define VBOX_CODE	m2_i2	/* VirtualBox result code */
#define VBOX_FUNCTION	m2_l1	/* function call number */
#define VBOX_ID		m2_l2	/* opaque request ID */

#endif /* _MINIX_VBOXIF_H */
