#define PROF_VER (1)

struct profile {
int version;
int chrvcnt;  char *chrvec; 
int metavcnt; char *metavec;
int extvcnt;  char *extvec; 
int menuvcnt; char *menuvec;
};

struct stored_profile {
unsigned char version[2];
unsigned char chrvcnt[2], chrvec[2]; 
unsigned char metavcnt[2], metavec[2];
unsigned char extvcnt[2], extvec[2]; 
unsigned char menuvcnt[2], menuvec[2];
};

#define prof_pack(p, n)		((p)[0] = (n) & 0xFF, (p)[1] = (n) >> 8)
#define prof_upack(p)		((p)[0] | ((p)[1] << 8))
