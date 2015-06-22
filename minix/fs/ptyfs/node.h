#ifndef _MINIX_PTYFS_NODE_H
#define _MINIX_PTYFS_NODE_H

typedef unsigned int node_t;

struct node_data {
	dev_t		dev;
	mode_t		mode;
	uid_t		uid;
	gid_t		gid;
	time_t		ctime;
};

void init_nodes(void);
int set_node(node_t index, struct node_data *data);
void clear_node(node_t index);
struct node_data *get_node(node_t index);
node_t get_max_node(void);

#endif /* !_MINIX_PTYFS_NODE_H */
