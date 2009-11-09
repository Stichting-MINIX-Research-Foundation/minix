/*
 * Test name: test04_srv.c
 *
 * Objective: Test a simple UDP server
 *
 * Description: Implements a simple echo server using the UDP protocol. Instead
 * of blocking on read(), it performs a select call first blocking there 
 * until there is data to be read
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
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

  if ((fd = udp_conf(PORT)) < 0) {
  	fprintf(stderr, "Error configuring UDP connection\n");
    	exit(-1);	
  }
  printf("Waiting for messages on port: %ld\n", PORT);
  fflush(stdout);
  /* Initialize fd_set */
  FD_ZERO(&fds_read);
  FD_SET(fd, &fds_read);

  while (1) 
  {
    /* Wait for data available to be read (no timeout) */
    ret = select(4, &fds_read, NULL, NULL, NULL);
    if (ret < 0) {
    	fprintf(stderr, "Error on select: %d", errno);
    	exit(-1);
    }
    if (!FD_ISSET(fd, &fds_read)) {
    	printf("Error: network fd is not ready (?)\n");
    	exit(-1);
    }
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

  close(fd);
}                       
