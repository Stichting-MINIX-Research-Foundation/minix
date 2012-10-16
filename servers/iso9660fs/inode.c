
/* This file contains all the function that handle the dir records
 * (inodes) for the ISO9660 filesystem.*/

#include "inc.h"
#include "buf.h"
#include <minix/vfsif.h>


/*===========================================================================*
 *				fs_putnode				     *
 *===========================================================================*/
int fs_putnode()
{
/* Find the inode specified by the request message and decrease its counter. */
  int count;
  struct dir_record *dir = NULL;

  dir = get_dir_record(fs_m_in.REQ_INODE_NR);
  release_dir_record(dir);
  
  count = fs_m_in.REQ_COUNT;

  if (count <= 0) return(EINVAL);
  
  if (count > dir->d_count) {
     printf("put_inode: count too high: %d > %d\n", count, dir->d_count);
     return(EINVAL);
  }

  if (dir->d_count > 1)
	dir->d_count = dir->d_count - count + 1;/*Keep at least one reference*/

  release_dir_record(dir); /* Actual inode release, might be last reference */

  return(OK);
}


/*===========================================================================*
 *				release_dir_record			     *
 *===========================================================================*/
int release_dir_record(dir)
struct dir_record *dir;
{
/* Release a dir record (decrement the counter) */
  if (dir == NULL)
	return(EINVAL);

  if (--dir->d_count == 0) {    
	if (dir->ext_attr != NULL)
		dir->ext_attr->count = 0;
	dir->ext_attr = NULL;
	dir->d_mountpoint = FALSE;

	dir->d_prior = NULL;
	if (dir->d_next != NULL)
		release_dir_record(dir);
	dir->d_next = NULL;
  }
  return(OK);
}


/*===========================================================================*
 *				get_free_dir_record			     *
 *===========================================================================*/
struct dir_record *get_free_dir_record(void)
{
/* Get a free dir record */
  struct dir_record *dir;

  for(dir = dir_records; dir < &dir_records[NR_ATTR_RECS]; dir++) {
	if (dir->d_count == 0) {	/* The record is free */
		dir->d_count = 1;		/* Set count to 1 */
		dir->ext_attr = NULL;
		return(dir);
	}
  }

  return(NULL);
}


/*===========================================================================*
 *				get_dir_record				     *
 *===========================================================================*/
struct dir_record *get_dir_record(id_dir_record)
ino_t id_dir_record;
{
  struct dir_record *dir = NULL;
  u32_t address;
  int i;

  /* Search through the cache if the inode is still present */
  for(i = 0; i < NR_DIR_RECORDS && dir == NULL; ++i) {
	if (dir_records[i].d_ino_nr == id_dir_record
					 && dir_records[i].d_count > 0) {
		dir = dir_records + i;
		dir->d_count++;
	}
  }

  if (dir == NULL) {
	address = (u32_t)id_dir_record;
	dir = load_dir_record_from_disk(address);
  }

  if (dir == NULL) return(NULL);
  
  return(dir);
}


/*===========================================================================*
 *				get_free_ext_attr				     *
 *===========================================================================*/
struct ext_attr_rec *get_free_ext_attr(void) {
/* Get a free extended attribute structure */
  struct ext_attr_rec *dir;
  for(dir = ext_attr_recs; dir < &ext_attr_recs[NR_ATTR_RECS]; dir++) {
	if (dir->count == 0) {	/* The record is free */
		dir->count = 1;
		return(dir);
	}
  }
  
  return(NULL);
}


/*===========================================================================*
 *				create_ext_attr				     *
 *===========================================================================*/
int create_ext_attr(struct ext_attr_rec *ext,char *buffer)
{
/* Fill an extent structure from the data read on the device */
  if (ext == NULL) return(EINVAL);

  /* In input we have a stream of bytes that are physically read from the
   * device. This stream of data is copied to the data structure. */
  memcpy(&ext->own_id,buffer,sizeof(u32_t));
  memcpy(&ext->group_id,buffer + 4,sizeof(u32_t));
  memcpy(&ext->permissions,buffer + 8,sizeof(u16_t));
  memcpy(&ext->file_cre_date,buffer + 10,ISO9660_SIZE_VOL_CRE_DATE);
  memcpy(&ext->file_mod_date,buffer + 27,ISO9660_SIZE_VOL_MOD_DATE);
  memcpy(&ext->file_exp_date,buffer + 44,ISO9660_SIZE_VOL_EXP_DATE);
  memcpy(&ext->file_eff_date,buffer + 61,ISO9660_SIZE_VOL_EFF_DATE);
  memcpy(&ext->rec_format,buffer + 78,sizeof(u8_t));
  memcpy(&ext->rec_attrs,buffer + 79,sizeof(u8_t));
  memcpy(&ext->rec_length,buffer + 80,sizeof(u32_t));
  memcpy(&ext->system_id,buffer + 84,ISO9660_SIZE_SYS_ID);
  memcpy(&ext->system_use,buffer + 116,ISO9660_SIZE_SYSTEM_USE);
  memcpy(&ext->ext_attr_rec_ver,buffer + 180,sizeof(u8_t));
  memcpy(&ext->len_esc_seq,buffer + 181,sizeof(u8_t));

  return(OK);
}


/*===========================================================================*
 *				create_ext_attr				     *
 *===========================================================================*/
int create_dir_record(dir,buffer,address)
struct dir_record *dir;
char *buffer;
u32_t address;
{
/* Fills a dir record structure from the data read on the device */
/* If the flag assign id is active it will return the id associated;
 * otherwise it will return OK. */
  short size;

  size = buffer[0];
  if (dir == NULL) return(EINVAL);
  
  /* The data structure dir record is filled with the stream of data
   * that is read. */
  dir->length = size;
  dir->ext_attr_rec_length = *((u8_t*)buffer + 1);
  memcpy(&dir->loc_extent_l,buffer + 2,sizeof(u32_t));
  memcpy(&dir->loc_extent_m,buffer + 6,sizeof(u32_t));
  memcpy(&dir->data_length_l,buffer + 10,sizeof(u32_t));
  memcpy(&dir->data_length_m,buffer + 14,sizeof(u32_t));
  memcpy(dir->rec_date,buffer + 18, sizeof(dir->rec_date));
  dir->file_flags = *((u8_t*)buffer + 25);
  dir->file_unit_size = *((u8_t*)buffer + 26);
  dir->inter_gap_size = *((u8_t*)buffer + 27);
  dir->vol_seq_number = *((u8_t*)buffer + 28);
  dir->length_file_id = *((u8_t*)buffer + 32);
  memcpy(dir->file_id,buffer + 33,dir->length_file_id);
  dir->ext_attr = NULL;

  /* set memory attrs */
  if ((dir->file_flags & D_TYPE) == D_DIRECTORY)
	dir->d_mode = I_DIRECTORY;
  else
	dir->d_mode = I_REGULAR;

  /* Set permission to read only. Equal for all users. */
  dir->d_mode |= R_BIT | X_BIT;
  dir->d_mode |= R_BIT << 3 | X_BIT << 3;
  dir->d_mode |= R_BIT << 6 | X_BIT << 6;

  dir->d_mountpoint = FALSE;
  dir->d_next = NULL;
  dir->d_prior = NULL;
  dir->d_file_size = dir->data_length_l;

  /* Set physical address of the dir record */
  dir->d_phy_addr = address;
  dir->d_ino_nr = (ino_t) address; /* u32_t e ino_t are the same datatype so
				   * the cast is safe */
  return(OK);
}


/*===========================================================================*
 *			load_dir_record_from_disk			     *
 *===========================================================================*/
struct dir_record *load_dir_record_from_disk(address)
u32_t address;
{
/* This function load a particular dir record from a specific address
 * on the device */
 
  int block_nr, offset, block_size, new_pos;
  struct buf *bp;
  struct dir_record *dir, *dir_next, *dir_parent, *dir_tmp;
  char name[NAME_MAX + 1];
  char old_name[NAME_MAX + 1];
  u32_t new_address, size;

  block_size = v_pri.logical_block_size_l; /* Block size */
  block_nr = address / block_size; /* Block number from the address */
  offset = address % block_size; /* Offset starting from the block */

  bp = get_block(block_nr);	/* Read the block from the device */
  if (bp == NULL)
	return(NULL);

  dir = get_free_dir_record();	/* Get a free record */
  if (dir == NULL)
	return(NULL);

  /* Fill the dir record with the data read from the device */
  create_dir_record(dir,b_data(bp) + offset, address);

  /* In case the file is composed of more file sections, load also the
   * next section into the structure */
  new_pos = offset + dir->length;
  dir_parent = dir;
  new_address = address + dir->length;
  while (new_pos < block_size) {
	dir_next = get_free_dir_record();
	create_dir_record(dir_next, b_data(bp) + new_pos, new_address);

	if (dir_next->length > 0) {
		strncpy(name,dir_next->file_id,dir_next->length_file_id);
		name[dir_next->length_file_id] = '\0';
		strncpy(old_name, dir_parent->file_id,
			dir_parent->length_file_id);
		old_name[dir_parent->length_file_id] = '\0';
      
		if (strcmp(name, old_name) == 0) {
			dir_parent->d_next = dir_next;
			dir_next->d_prior = dir_parent;

			/* Link the dir records */
			dir_tmp = dir_next;
			size = dir_tmp->data_length_l;

			/* Update the file size */
			while (dir_tmp->d_prior != NULL) {
				dir_tmp = dir_tmp->d_prior;
				size += dir_tmp->data_length_l;
				dir_tmp->d_file_size = size;
			}

			new_pos += dir_parent->length;
			new_address += dir_next->length;
			dir_parent = dir_next;
		} else {			/* This is another inode. */
			release_dir_record(dir_next);
			new_pos = block_size;
		}      
	} else {				/* record not valid */
		release_dir_record(dir_next);
		new_pos = block_size;		/* Exit from the while */
	}
  }

  put_block(bp);		/* Release the block read. */
  return(dir);
}

