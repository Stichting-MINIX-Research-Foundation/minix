/*
 * Test name: test05_srv.c
 *
 * Objective: Test an impatient UDP server with timeouts  
 *
 * Description: Implements an echo server using the UDP protocol. It is 
 * based on test04_srv, but it has a timeout value. 
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/asynchio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <net/netlib.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>
#include <net/hton.h>

#define PORT 6000L

/* type for received data */
typedef struct 
{
  udp_io_hdr_t header;
  char data[1024];
} udp_buffer_t;

int udp_conf(long port) {

  char *udp_device;
  int netfd;
  nwio_udpopt_t udpopt;
                            
  /* Get default UDP device */
  if ((udp_device = getenv("UDP_DEVICE")) == NULL) 
    udp_device = UDP_DEVICE;
                                
  /* Open UDP connection */ 
  if ((netfd = open(udp_device, O_RDWR)) < 0) 
  {
    fprintf(stderr,"Error opening UDP connection\n");
    return -1;
  }               
                                
  /* Configure UDP connection */ 
  udpopt.nwuo_flags = NWUO_COPY | NWUO_LP_SET | NWUO_EN_LOC | NWUO_DI_BROAD 
                   | NWUO_RP_ANY | NWUO_RA_ANY | NWUO_RWDATALL | NWUO_DI_IPOPT;

  udpopt.nwuo_locport = (udpport_t) htons(port);
	                        
  if ((ioctl(netfd, NWIOSUDPOPT, &udpopt))<0) 
  {
    fprintf(stderr,"Error configuring the connection\n");
    close(netfd);
    return -1;
  }               
	                        
  /* Get conf options */
  if ((ioctl(netfd, NWIOGUDPOPT, &udpopt))<0) 
  {
    fprintf(stderr,"Error getting the conf\n");
    close(netfd);
    return -1;
  }               

  return netfd;
}

int main(int argc,char *argv[]) {
  int fd;
  ssize_t data_read;
  udp_buffer_t buffer;
  ipaddr_t tmp_addr;
  udpport_t tmp_port;
  int ret;
  fd_set fds_read;
  struct timeval timeout;
  ipaddr_t client_addr, this_addr;
  udpport_t client_port; 

  if ((fd = udp_conf(PORT)) < 0) {
  	fprintf(stderr, "Error configuring UDP connection\n");
    	exit(-1);	
  }
  printf("Waiting for messages on port: %ld\n", PORT);
  fflush(stdout);

  /* get a first message so we know who is the client and we can harass it
     afterwards */

  /* Initialize fd_set */
  FD_ZERO(&fds_read);
  FD_SET(fd, &fds_read);
  /* Set timeout structure */
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;
  ret = select(4, &fds_read, NULL, NULL, NULL);

  if (ret < 0) {
  	fprintf(stderr, "Error in select\n");
  	exit(-1);
  }
  if (!FD_ISSET(fd, &fds_read)) {
  	fprintf(stderr, "Error: Should be receiving some data from network(?)\n");
  	exit(-1);
  }
  printf("Ready to receive...\n");
  /* Read received data */
  data_read = read(fd, &buffer, sizeof(udp_buffer_t));
  printf("Received data: %s\n", buffer.data);
                                
  /* Can exit if the received string == exit */
  if (!strcmp(buffer.data,"exit")) 
      	exit(0); 

  /* Send data back, swap addresses */
  tmp_addr = buffer.header.uih_src_addr;
  buffer.header.uih_src_addr = buffer.header.uih_dst_addr;
  buffer.header.uih_dst_addr = tmp_addr;
  /* save address of both ends */
  client_addr = tmp_addr;
  this_addr = buffer.header.uih_src_addr;  

  /* Swap ports */
  tmp_port = buffer.header.uih_src_port;
  buffer.header.uih_src_port = buffer.header.uih_dst_port;
  buffer.header.uih_dst_port = tmp_port;
  /* save client port */
  client_port = tmp_port;

  /* Write the same back  */
  write(fd, &buffer, data_read);             

  while (1) 
  {

    /* Initialize fd_set */
    FD_ZERO(&fds_read);
    FD_SET(fd, &fds_read);
    /* Set timeout structure */
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    /* Wait for data available to be read (timeout) */
    ret = select(4, &fds_read, NULL, NULL, &timeout);
    if (ret < 0) {
    	fprintf(stderr, "Error on select: %d", errno);
    	exit(-1);
    }
    /* if timeout */
    if (ret == 0) {
    	/* Send angry msg to client asking for more */
     	printf("Tired of waiting, send client an angry message\n");
     	buffer.header.uih_src_addr = this_addr;
     	buffer.header.uih_dst_addr = client_addr;
     	buffer.header.uih_src_port = PORT;
     	buffer.header.uih_dst_port = client_port;
    	strcpy(buffer.data, "Hey! I want to receive some data!\n");
    	write(fd, &buffer, sizeof(udp_buffer_t));
    }
    /* If receive data from network */
    if (FD_ISSET(fd, &fds_read)) {
	printf("Ready to receive...\n");
    	/* Read received data */
    	data_read = read(fd, &buffer, sizeof(udp_buffer_t));
    	printf("Received data: %s\n", buffer.data);
                                
    	/* Can exit if the received string == exit */
    	if (!strcmp(buffer.data,"exit")) 
      		break; 

    	/* Send data back, swap addresses */
    	tmp_addr = buffer.header.uih_src_addr;
    	buffer.header.uih_src_addr = buffer.header.uih_dst_addr;
    	buffer.header.uih_dst_addr = tmp_addr;
  
    	/* Swap ports */
    	tmp_port = buffer.header.uih_src_port;
    	buffer.header.uih_src_port = buffer.header.uih_dst_port;
    	buffer.header.uih_dst_port = tmp_port;
  
    	/* Write the same back  */
    	write(fd, &buffer, data_read);             
    }
  }               
  close(fd);
}                       
