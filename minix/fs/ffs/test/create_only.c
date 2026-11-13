#include <minixshim.h>
#include <unistd.h>
#include <fcntl.h>
int g_fd;
int fs_mount(dev_t,unsigned,struct fsdriver_node*,unsigned*);
void fs_unmount(void); void fs_sync(void); void init_inode_cache(void);
int fs_create(ino_t,char*,mode_t,uid_t,gid_t,struct fsdriver_node*);
int fs_mkdir(ino_t,char*,mode_t,uid_t,gid_t);
ssize_t fs_readwrite(ino_t,struct fsdriver_data*,size_t,off_t,int);
int fs_putnode(ino_t,unsigned);
int main(int c,char**v){ struct fsdriver_node root,n; unsigned rf;
 g_fd=open(v[1],O_RDWR); init_inode_cache(); fs_mount(1,0,&root,&rf);
 ino_t R=root.fn_ino_nr;
 fs_create(R,"alpha",0100644,0,0,&n); char m[]="alpha contents\n";
 struct fsdriver_data d={m,sizeof m}; fs_readwrite(n.fn_ino_nr,&d,15,0,FSC_WRITE); fs_putnode(n.fn_ino_nr,1);
 fs_mkdir(R,"beta",0040755,0,0);
 fs_sync(); fs_unmount(); return 0; }
