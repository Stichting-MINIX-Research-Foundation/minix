/* This file contains some structures used by the Mitsumi cdrom driver.
 *
 *  Feb 13 1995			Author: Michel R. Prevenier 
 */

/* Index into the mss arrays */
#define MINUTES	0
#define SECONDS	1
#define SECTOR	2

struct cd_play_mss
{
	u8_t	begin_mss[3];
	u8_t	end_mss[3];
};

struct cd_play_track
{
	u8_t 	begin_track;
	u8_t 	end_track;
};

struct cd_disk_info
{
	u8_t	first_track;
	u8_t 	last_track;
	u8_t	disk_length_mss[3];
	u8_t	first_track_mss[3];
};

struct cd_toc_entry
{
	u8_t	control_address;
	u8_t	track_nr;
	u8_t	index_nr;
	u8_t	track_time_mss[3];
        u8_t	reserved;
	u8_t	position_mss[3];
};
