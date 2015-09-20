/*
 * isoread.c
 *
 * isoread reads a file system in ISO9660 or HIGH SIERRA format from
 * a given device.
 *
 * Apr  5 1995    Michel R. Prevenier 
 * Nov 16 1996    Kees J. Bot        -- bug fix: isoread filename matching 
 * Dec  7 1997    Albert S. Woodhull -- bug fix: return values
 *                                       "   " : isodir filename handling
 *                                   -- added  : isoread -a option  
 * Mar 21 2000    Michael A. Temari  -- bug fix: look_up only searched first
 *                                             : block of directory
 *                                             : stack overflow in recurse_dir
 *                                             : and various other bugs
 * Apr 14 2002    Michael A. Temari  -- bug fix: fixed recursing directories
 *                                             : and printing dates 2000 and 
 *                                             : later
 * May 14 2002    Kees J. Bot        -- bug fix: fixed error messages
 * Mar 14 2003    Kees J. Bot        -- added  : iso{dir,read} -B option
 * Jul 24 2003    Michael A. Temari  -- bug fix: bytes to blocks roundup fix
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/times.h>
#include <unistd.h>

/*             
 *  definitions used by the ISO9660 and HIGH SIERRA file system
 */

#define ISO9660_ID	"CD001"
#define HIGH_SIERRA_ID	"CDROM"
#define BLOCK_SIZE	2048
#define BLOCK_SHIFT	11


/* Fields in a ISO9660 volume descriptor */
struct iso9660_descriptor
{
  char type[1];		
  char id[5];			
  char version[1];		
  char reserved1[1];		
  char system_id[32];		
  char volume_id[32];		
  char reserved2[8];		
  char volume_size[8];	
  char reserved3[32];		
  char volume_set_size[4];	
  char volume_seq_nr[4];
  char block_size[4];	
  char path_table_size[8];	
  char type_l_path_table[4];	
  char opt_type_l_path_table[4];	
  char type_m_path_table[4];	
  char opt_type_m_path_table[4];	
  char root_dir_entry[34];
  char vol_set_id[128];	
  char publ_id[128];
  char prep_id[128];	
  char appl_id[128];	
  char copyright_file_id[37];	
  char abstract_file_id[37];	
  char bibl_file_id[37];
  char creation_date[17];	
  char mod_date[17];	
  char exp_date[17];	
  char eff_date[17];	
  char file_struc_version[1];
  char reserved4[1];		
  char appl_data[512];	
  char reserved5[653];			
};


/* Fields in a High Sierra volume descriptor */
struct high_sierra_descriptor
{
  char reserved1[8];
  char type[1];	
  char id[5];		
  char version[1];	
  char reserved2[1];	
  char system_id[32];
  char volume_id[32];
  char reserved3[8];	
  char volume_size[8];	
  char reserved4[32];
  char vol_set_size[4];
  char volume_seq_nr[4];	
  char block_size[4];	
  char path_table_size[8];
  char type_l_path_table[4];
  char reserved5[28];		
  char root_dir_entry[34];
};


/* Fields in a directory entry */
struct dir_entry 
{
  char length[1];	
  char ext_attr_length[1];
  char first_block[8];
  char size[8];	
  char date[7];
  char flags[1];	
  char file_unit_size[1];
  char interleave[1];
  char volume_seq_nr[4];	
  char name_length[1];
  char name[1];
};


#define STDOUT		stdout
#define STDERR		stderr
#define NULL_DIR	(struct dir_entry *) 0
#define MAX_NAME_LENGTH	255
#define MAX_PATH_LENGTH	1024

#define NR_OF_CHARS	13 
#define NR_OF_BLANKS	2
#define NR_OF_COLS	(80 / (NR_OF_CHARS + NR_OF_BLANKS))

/* This macro always returns a lower case character */
#define LOWER_CASE(CHR) (CHR >= 'A' && CHR <= 'Z' ? CHR | 0x20 : CHR) 

/* Macro's for determining . , .. and normal directory entries */
#define IS_DOT(PTR) (PTR->name_length[0] == 1 && PTR->name[0] == 0 ? 1 : 0)
#define IS_DOT_DOT(PTR) (PTR->name_length[0] == 1 && PTR->name[0] == 1 ? 1 : 0)
#define IS_DIR(PTR) (PTR->flags[-High_Sierra] & 2 ? 1 : 0)


int main(int argc, char **argv);
int iso_cmp(char *name, struct dir_entry *dir_ptr, int dir_flag);
void list_dir(struct dir_entry *dir_ptr);
void list_file(struct dir_entry *dir_ptr);
struct dir_entry *look_up(char *name);
void recurse_dir(char *path, struct dir_entry *dir_ptr);
void read_device(long offset, int nr_of_bytes, char *buffer);
int valid_fs(void);
void usage(void);
void print_date(char *date);
void print_dir_date(char *date);
void iso_info(struct iso9660_descriptor *vol_desc);
void hs_info(struct high_sierra_descriptor *vol_desc);
int iso_711(char *c);
int iso_712(char *c);
int iso_721(char *c);
int iso_722(char *c);
int iso_723(char *c);
long iso_731(char *c);
long iso_732(char *c);
long iso_733(char *c);


char Buffer[BLOCK_SIZE];	            /* buffer to hold read data */
int Device;				    /* global file descriptor */
struct iso9660_descriptor *Iso_Vol_Desc;    /* iso9660 volume descriptor */
struct high_sierra_descriptor *Hs_Vol_Desc; /* high sierra volume descriptor */
int High_Sierra = 0;                        /* 1 = high sierra format */
int Iso9660 = 0;                            /* 1 = iso9660 format */

/* This comes in handy when printing the date */
char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

/* Flags displaying what to do  */
int Read_File = 0;      /* 1 = Read file */
int Read_Dir = 0;	/* 1 = Read directory entry */
int Read_Info = 0;      /* 1 = Read volume descriptor */
int Recurse = 0;        /* 1 = Recursively descend directories */
int Verbose = 0;        /* 1 = Print all info on directories */
int ByteOffset = 0;     /* 1 = Print byte offset and length of files */
int Aflag = 0;          /* 1 = Suppress output of \r  */

int iso_cmp(name, dir_ptr, dir_flag)
char *name;
struct dir_entry *dir_ptr;
int dir_flag;
{
/* Compare name with directory entries, looking for match with a dirname.
 * An iso9660 filename is terminated by ";n", where n will probably 
 * be 1. A directory name is not terminated by anything special, it may be
 * followed by a \0 if padding is needed to put the following directory 
 * entry on an even address.
 */
  int i;
  int len;

  /* First match the filename */
  len = strlen(name);
  if (len > iso_711(dir_ptr->name_length)) return 1;
  for (i = 0; i < len; i++)
  {
    if (dir_ptr->name[i] == ';') return 1;	/* found end of a filename */ 
    if (name[i] != LOWER_CASE(dir_ptr->name[i])) return 1; /* match failed */
  }
  if (dir_ptr->name[i] != ';' && i != len) return 1; /* incomplete match */

  /* The filename is ok, now look at the file type */
  if (dir_flag && !IS_DIR(dir_ptr)) return 1;  /* File type not correct */

  return 0; 
}


void usage()
{
  if (Read_Dir)
   fprintf (STDERR, "Usage: isodir [-lrB] inputfile [dir]\n");
  else if (Read_Info)
   fprintf (STDERR, "Usage: isoinfo inputfile\n");
  else
   fprintf (STDERR, "Usage: isoread [-a] inputfile file\n");
  exit(1);
}


int main(argc, argv)
int argc;
char **argv;
{
  struct dir_entry *entry;
  char path[MAX_PATH_LENGTH];
  char *input_file;
  char *basename;
  char *file_name;
  int i,j;

  /* Read arguments */
  basename = argv[0];
  while (*argv[0] != '\0') 
    if (*argv[0]++ == '/') basename = argv[0];

  if (strcmp(basename,"isodir") == 0) Read_Dir = 1;
  else if (strcmp(basename,"isoinfo") == 0) Read_Info = 1;
  else Read_File = 1;

  if ((argc > 5 && Read_Dir) || (argc != 2 && Read_Info) ||
     (argc > 4 && Read_File)) usage();

  i = 1;

  while (i < argc && argv[i][0] == '-')
  {
    char *opt = argv[i++] + 1;

    if (opt[0] == '-' && opt[1] == '\0') break;

    while (*opt != '\0')
    {
      if (Read_Info) usage();
      if (Read_Dir)
      switch (*opt++)
      {
	case 'r':	Recurse = 1; break;  
	case 'l':	Verbose = 1; break;
	case 'B':	ByteOffset = 1; break;
	default:	usage();
      }
      if (Read_File)
      switch (*opt++)
      {
	case 'a':	Aflag = 1; break;    
	case 'B':	ByteOffset = 1; break;
	default:	usage();
      }
    }
  }

  if (i >= argc) usage();
  input_file = argv[i++];

  if (Read_File)
  {
    if (i >= argc) usage();
    file_name = argv[i++];
  }

  if (Read_Dir)
  {
    file_name = "/";
    if (i < argc)
    {
      file_name = argv[i++];
    }
  }

  if (i < argc) usage();
  
  if (Read_File || Read_Dir)
  {
    for (i=0; file_name[i] != '\0'; i++) 
      path[i] = LOWER_CASE(file_name[i]);
    path[i] = '\0';
  }
 
  /* Open file system (file or device) */
  if ((Device = open(input_file, O_RDONLY)) < 0) 
  {
    fprintf (STDERR, "cannot open %s: %s\n", input_file, strerror(errno));
    exit(-1);
  }

  
  if (!valid_fs())
  {
    fprintf (STDERR, "File system not in ISO9660 or HIGH SIERRA format \n");
    exit(-1);
  }

  
  if (Read_Info)
  {
    if (Iso9660) 
      iso_info(Iso_Vol_Desc);
    else 
      hs_info(Hs_Vol_Desc);
    exit(0);
  }

  /* Lookup file */
  if ((entry = look_up(path)) != NULL_DIR)
  {
    if (Read_Dir)
      if (Recurse) recurse_dir(path,entry);
      else list_dir(entry);
    else
      list_file(entry);
  }
  else
  {
    if (Read_Dir)
      fprintf (STDERR, "Directory");
    else
      fprintf (STDERR, "File");
    fprintf (STDERR, " %s not found\n", path);
    exit(-1);
  }
  return 0;
}


struct dir_entry *look_up(path)
char *path;
{
  /* Lookup a file name */

  struct dir_entry *dir_ptr;
  long block;
  int nr_of_blocks;
  int offset;
  char name[MAX_NAME_LENGTH + 1];
  int name_index = 0;
  int last_in_path = 0;
  int found;
  int i,j;

  /* Get the right dir entry structure */
  if (Iso9660)
    dir_ptr = (struct dir_entry *) Iso_Vol_Desc->root_dir_entry;  
  else
    dir_ptr = (struct dir_entry *) Hs_Vol_Desc->root_dir_entry;  

  /* If we look for the root we already have the right entry */
  if (path[0] == '/') {
    if (strlen(path) == 1) return dir_ptr;
    else name_index = 1; /* first name in path */
  }

  /* Keep searching for the path elements until all are found */
  while (!last_in_path)
  {
    /* Get next name in path */ 
    for (i = name_index; i < strlen(path); i++)
    {
      if (path[i] == '/') break;
      name[i - name_index] = path[i];
    }
    last_in_path = 
           (i == strlen(path) || (i == strlen(path) - 1 && path[i] == '/'));
    name[i-name_index] = '\0';
    name_index = i + 1;
   
    /* Get block of next directory */
    block = iso_733(dir_ptr->first_block) + iso_711(dir_ptr->ext_attr_length);
    nr_of_blocks = (iso_733(dir_ptr->size) + (BLOCK_SIZE-1)) >> BLOCK_SHIFT;

    /* Search for file in dir entry */
    found = 0;
    for (j=0; j < nr_of_blocks && !found; j++) 
    {
      /* Read a directory block */
      read_device(block*BLOCK_SIZE, BLOCK_SIZE, Buffer);
      block++;

      dir_ptr = (struct dir_entry *) Buffer;

      offset = 0;
      /* Compare with all entries in this block */
      while (iso_711(dir_ptr->length) > 0 && offset < BLOCK_SIZE)
      {
        if (iso_cmp(name, dir_ptr,
            (Read_Dir || !last_in_path)) == 0)
        {
          found = 1;
          break;
        }
        /* Next entry */
        offset += iso_711(dir_ptr->length);
        dir_ptr = (struct dir_entry *) (Buffer + offset);
      }
    }
    if (!found) return NULL_DIR;   /* path element not found */ 
  }
  return dir_ptr;
}
  

void recurse_dir(path, dir_ptr)
char *path;
struct dir_entry *dir_ptr;
{
  /* Recursively descend all directories starting with dir_ptr */

  char tmp_path[MAX_PATH_LENGTH];
  int i,j, path_length;
  long block, saveblock, dblock;
  int nr_of_blocks;
  int offset = 0; 

  
  /* Save block number and nr of blocks of current dir entry because 
   * list_dir changes dir_ptr 
   */
  block = iso_733(dir_ptr->first_block) + iso_711(dir_ptr->ext_attr_length);
  nr_of_blocks = (iso_733(dir_ptr->size) + (BLOCK_SIZE-1)) >> BLOCK_SHIFT;

  /* Add a trailing / to path if necessary */
  path_length = strlen(path);
  if (path[path_length-1] != '/')
  {
    path[path_length++] = '/';
    path[path_length] = '\0';
  }

  /* Print current path of directory, and list contents of directory */
  fprintf(STDOUT,"directory %s:\n\n", path);
  list_dir(dir_ptr);
  fprintf(STDOUT,"\n\n");
  
  for (j=0; j < nr_of_blocks; j++) 
  {
    read_device(block*BLOCK_SIZE, BLOCK_SIZE, Buffer);
    saveblock = block++;

    /* Save buffer, because the next recursive call destroys 
     * the global Buffer 
     */
    dir_ptr = (struct dir_entry *) Buffer;

    /* Search this dir entry for directories */
    offset = 0;
    while (iso_711(dir_ptr->length) != 0 && offset < BLOCK_SIZE)
    {
      /* Is current file a directory and not the . or .. entries */
      if (IS_DIR(dir_ptr)  && !IS_DOT(dir_ptr) && !IS_DOT_DOT(dir_ptr))
      {
        /* setup path for next recursive call */
        for (i=0; i<path_length; i++) tmp_path[i] = path[i]; 
        for (i=0;i<iso_711(dir_ptr->name_length) && dir_ptr->name[i] != ';';i++)
          tmp_path[i+path_length] = LOWER_CASE(dir_ptr->name[i]);
        tmp_path[i+path_length] = '/';
        tmp_path[i+1+path_length] = '\0';
  
        /* Read block of directory we found */
        dblock = iso_733(dir_ptr->first_block);
        read_device(dblock*BLOCK_SIZE, BLOCK_SIZE, Buffer);
  
        /* And start all over again with this entry */
        recurse_dir(tmp_path, (struct dir_entry *) Buffer);

        /* get the block we were looking at */
        read_device(saveblock*BLOCK_SIZE, BLOCK_SIZE, Buffer);
      }

      /* Go to the next file in this directory */
      offset += iso_711(dir_ptr->length);
      dir_ptr = (struct dir_entry *) (Buffer + offset);
    }
  }
}
    

void list_dir(dir_ptr)
struct dir_entry *dir_ptr;
{
  /* List all entries in a directory */
  int tty;
  long block;
  int nr_of_blocks;
  int i,j;
  int offset = 0;
  char name[NR_OF_CHARS+NR_OF_BLANKS+1];
  int name_len;
  int column = 0;
  int skip = 0;

  tty = isatty(STDOUT_FILENO);
  /* Get first block of directory */
  block = iso_733(dir_ptr->first_block) + iso_711(dir_ptr->ext_attr_length);
  nr_of_blocks = (iso_733(dir_ptr->size) + (BLOCK_SIZE-1)) >> BLOCK_SHIFT;

  /* Read all directory blocks and display their contents */
  for (j=0; j < nr_of_blocks; j++) 
  {
    read_device(block*BLOCK_SIZE, BLOCK_SIZE, Buffer);
    block++;

    dir_ptr = (struct dir_entry *) (Buffer);
    offset = 0;
    while (iso_711(dir_ptr->length) != 0 && offset < BLOCK_SIZE)
    {
      name_len = 0;
      if (IS_DOT(dir_ptr))
      {
        name[name_len++] =  '.';
        if (!Verbose) skip = 1;
      }
      else
      {
        if (IS_DOT_DOT(dir_ptr))
        { 
          name[name_len++] =  '.';
          name[name_len++] =  '.';
          if (!Verbose) skip = 1;
        }
        else
        {
          for (i=0; i<iso_711(dir_ptr->name_length) &&
                    i<NR_OF_CHARS; i++) 
          {
            if (dir_ptr->name[i] == ';') break;
            name[name_len++] = LOWER_CASE(dir_ptr->name[i]);
          }
          if (IS_DIR(dir_ptr) && tty) name[name_len++] = '/';
        }
      }
      if (!skip)
      {
        if (ByteOffset)
        {
          fprintf (STDOUT, "%10ld ",
            (iso_733(dir_ptr->first_block) + iso_711(dir_ptr->ext_attr_length))
              * BLOCK_SIZE);
        }
        if (Verbose || ByteOffset)
        {
          fprintf (STDOUT, "%10ld ",  iso_733(dir_ptr->size));
        }
        if (Verbose)
        {
          print_dir_date(dir_ptr->date);
          fprintf (STDOUT, " ");
        }
	if(!tty)
		name[name_len] = '\0';
	else {
	        for(i=name_len; i<(NR_OF_CHARS+NR_OF_BLANKS); i++) name[i] = ' ';
	        name[NR_OF_CHARS+NR_OF_BLANKS] = '\0';
	}
        fprintf(STDOUT, "%s", name);
        if (!(Verbose || ByteOffset))
        {
          column++;
          if (column >= NR_OF_COLS || !tty) 
          {
            column = 0;
            fprintf(STDOUT,"\n");
          }
        }
        else fprintf(STDOUT,"\n");
      }
      skip = 0;
      offset += iso_711(dir_ptr->length);
      dir_ptr = (struct dir_entry *) (Buffer+offset);
    }
  }
  if (!Verbose && column) fprintf(STDOUT,"\n");
}


void print_dir_date(date)
char *date;
{
  /* Print date in a directory entry */

  int m;

  m = iso_711(&date[1]) - 1;
  if(m < 0 || m > 11)
  	fprintf(STDOUT, "   ");
  else
	fprintf(STDOUT,"%.3s",&months[m*3]);

  fprintf (STDOUT, " %02d %04d %02d:%02d:%02d",
           date[2],
           1900+date[0],
           date[3],
           date[4],
           date[5]);
}


void list_file(dir_ptr)
struct dir_entry *dir_ptr;
{
  /* List contents of a file */

  int i;
  long block;
  long size;
  char c;

  block = iso_733(dir_ptr->first_block);
  size = iso_733(dir_ptr->size);

  if (ByteOffset) {
    fprintf(STDOUT, "%ld %ld\n", block*BLOCK_SIZE, size);
    return;
  }

  while (size > 0)
  if (Aflag == 1) {
    read_device(block*BLOCK_SIZE, BLOCK_SIZE, Buffer);
      for (i=0; ((i < size) && (i < BLOCK_SIZE)); i++)
        if (Buffer[i] != '\r') fprintf(STDOUT, "%c", Buffer[i]);
    size-= BLOCK_SIZE;
    block++;
  } else {
    read_device(block*BLOCK_SIZE, BLOCK_SIZE, Buffer);
      for (i=0; ((i < size) && (i < BLOCK_SIZE)); i++)
        fprintf(STDOUT, "%c", Buffer[i]);
    size-= BLOCK_SIZE;
    block++;
  }
}


void print_date(date)
char *date;
{
  /* Print the date in a volume descriptor */

  fprintf (STDOUT, "%c%c-%c%c-%c%c%c%c %c%c:%c%c:%c%c",
           date[4],
           date[5],
           date[6],
           date[7],
           date[0],
           date[1],
           date[2],
           date[3],
           date[8],
           date[9],
           date[10],
           date[11],
           date[12],
           date[13]);
}

void iso_info(vol_desc)
struct iso9660_descriptor *vol_desc;
{
  int i;

  fprintf (STDOUT, "Format: ISO9660 \n");
  fprintf (STDOUT, "System id: ");
  for (i=0; i< sizeof(vol_desc->system_id); i++) 
    fprintf(STDOUT, "%c", vol_desc->system_id[i]);
  fprintf (STDOUT, "\n");
  fprintf (STDOUT, "Volume id: ");
  for (i=0; i< sizeof(vol_desc->volume_id); i++) 
    fprintf(STDOUT, "%c", vol_desc->volume_id[i]);
  fprintf (STDOUT, "\n");
  fprintf (STDOUT, "Volume size: %ld Kb\n", iso_733(vol_desc->volume_size)*2);
  fprintf (STDOUT, "Block size: %d bytes \n", iso_723(vol_desc->block_size));
  fprintf (STDOUT, "Creation date: ");
  print_date(vol_desc->creation_date); 
  fprintf(STDOUT, "\n");
  fprintf (STDOUT, "Modification date: ");
  print_date(vol_desc->mod_date); 
  fprintf (STDOUT, "\n");
  fprintf (STDOUT, "Expiration date: ");
  print_date(vol_desc->exp_date); 
  fprintf (STDOUT, "\n");
  fprintf (STDOUT, "Effective date: ");
  print_date(vol_desc->eff_date); 
  fprintf (STDOUT, "\n");
}


void hs_info(vol_desc)
struct high_sierra_descriptor *vol_desc;
{
  int i;

  fprintf (STDOUT, "Format: HIGH SIERRA \n");
  fprintf (STDOUT, "System id: ");
  for (i=0; i< sizeof(vol_desc->system_id); i++) 
    fprintf(STDOUT, "%c", vol_desc->system_id[i]);
  fprintf (STDOUT, "\n");
  fprintf (STDOUT, "Volume id: ");
  for (i=0; i< sizeof(vol_desc->volume_id); i++) 
    fprintf(STDOUT, "%c", vol_desc->volume_id[i]);
  fprintf (STDOUT, "\n");
  fprintf (STDOUT, "Volume size: %ld Kb\n", (iso_733(vol_desc->volume_size)*2));
  fprintf (STDOUT, "Block size: %d bytes \n", iso_723(vol_desc->block_size));
}
  

int valid_fs()               
{

  int i;

  /* search for a volume descriptor */
  for (i=16; i<100; i++)
  {
  
    read_device((long)(i)*BLOCK_SIZE, BLOCK_SIZE, Buffer);

    Iso_Vol_Desc = (struct iso9660_descriptor *) Buffer; 
    Hs_Vol_Desc = (struct high_sierra_descriptor *) Buffer; 
    
    if (strncmp(Iso_Vol_Desc->id, ISO9660_ID, sizeof Iso_Vol_Desc->id) == 0)
    {
      /* iso_info(Iso_Vol_Desc); */
      Iso9660 = 1;
      break;  
    }

    if (strncmp(Hs_Vol_Desc->id, HIGH_SIERRA_ID, sizeof Hs_Vol_Desc->id) == 0)
    {
      /* hs_info(Hs_Vol_Desc); */
      High_Sierra = 1; 
      break;  
    }
  }

  if (i >= 100) return 0;
  return 1;
}


void read_device(offset, nr_of_bytes, buffer)
long offset;
int nr_of_bytes;
char *buffer;
{
  int bytes_read;

  if (lseek(Device, offset, SEEK_SET) == -1) 
  {
	fflush (stdout);
	fprintf (STDERR, "seek error: %s\n", strerror(errno));
	exit(1);
  }

  bytes_read = read(Device, buffer, nr_of_bytes);
  if (bytes_read != nr_of_bytes) 
  {
  	fprintf (STDERR, "read error: %s\n",
	    bytes_read >= 0 ? "Short read" : strerror(errno));
  	exit (1);
  }
}


/* The ISO9660 functions */

int iso_711 (c)
char *c;
{
  return (*c & 0xff);
}


int iso_712 (c)
char *c;
{
  int n;
	
  n = *c;
  if (n & 0x80) n |= 0xffffff00;
  return n;
}

int iso_721 (c)
char *c;
{
  return ((c[0] & 0xff) | ((c[1] & 0xff) << 8));
}

int iso_722 (c)
char *c;
{
  return (((c[0] & 0xff) << 8) | (c[1] & 0xff));
}

int iso_723 (c)
char *c;
{
  if (c[0] != c[3] || c[1] != c[2]) 
  {
    fprintf (STDERR, "Invalid ISO 7.2.3 number\n");
    exit (1);
  }
  return (iso_721 (c));
}

long iso_731 (c)
char *c;
{
  return ((long)(c[0] & 0xff)
       | ((long)(c[1] & 0xff) << 8)
       | ((long)(c[2] & 0xff) << 16)
       | ((long)(c[3] & 0xff) << 24));
}


long iso_732 (c)
char *c;
{
  return (((long)(c[0] & 0xff) << 24)
        | (((long)c[1] & 0xff) << 16)
        | (((long)c[2] & 0xff) << 8)
        | ((long)c[3] & 0xff));
}

long iso_733 (c)
char *c;
{
int i;

  for (i = 0; i < 4; i++) 
  {
    if (c[i] != c[7-i]) 
    {
      fprintf (STDERR, "Invalid ISO 7.3.3 number\n");
      exit (1);
    }
  }
  return (iso_731(c));
}
