/* Description of entry in partition table.  */
#ifndef _PARTITION_H
#define _PARTITION_H

#include <stdint.h>

struct part_entry {
  uint8_t bootind;	/* boot indicator 0/ACTIVE_FLAG	 */
  uint8_t start_head;	/* head value for first sector	 */
  uint8_t start_sec;	/* sector value + cyl bits for first sector */
  uint8_t start_cyl;	/* track value for first sector	 */
  uint8_t sysind;		/* system indicator		 */
  uint8_t last_head;	/* head value for last sector	 */
  uint8_t last_sec;	/* sector value + cyl bits for last sector */
  uint8_t last_cyl;	/* track value for last sector	 */
  uint32_t lowsec;		/* logical first sector		 */
  uint32_t size;		/* size of partition in sectors	 */
};

#define ACTIVE_FLAG	0x80	/* value for active in bootind field (hd0) */
#define NR_PARTITIONS	4	/* number of entries in partition table */
#define	PART_TABLE_OFF	0x1BE	/* offset of partition table in boot sector */

/* Partition types. */
#define NO_PART		0x00	/* unused entry */
#define MINIX_PART	0x81	/* Minix partition type */

#endif /* _PARTITION_H */
