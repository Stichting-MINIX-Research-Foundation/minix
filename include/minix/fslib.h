/* V1 and V2 file system disk to/from memory support functions. */

int bitmapsize(bit_t _nr_bits, int block_size);
unsigned conv2(int _norm, int _w);
long conv4(int _norm, long _x);
void new_icopy(struct inode *_rip, d2_inode *_dip, int _direction, int _norm);
