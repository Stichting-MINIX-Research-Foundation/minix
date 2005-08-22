/*
 * Test name: test05_cli.c
 *
 * Objective: Test a impatient UDP client with timeout and incoming data from
 * network and terminal.
 *
 * Description: Implements a echo client using the UDP protocol. It is 
 * based on test04_cli, but the difference is that it uses timeout and waits
 * for data both from terminal (stdin) and network connection.
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

/* Type for received data */
typedef struct
{
  udp_io_hdr_t header;
  char data[1024];
} udp_buffer_t;

int udp_conf(char *host, long port, udp_io_hdr_t *header)
{
  /* configures UDP connection */
  char *udp_device;
  struct hostent *hp;
  int netfd;
  nwio_udpopt_t udpopt;
  ipaddr_t dirhost;
  int result;
  
  /* get host address */
  if ((hp = gethostbyname(host)) == (struct hostent*) NULL) 
  {
    fprintf(stderr,"Unknown host\n");
    return(-1);
  }
  memcpy((char *)&dirhost, (char *)hp->h_addr, hp->h_length);
  
  /* Get UDP device */
  if (( udp_device = getenv("UDP_DEVICE") ) == NULL) 
    udp_device = UDP_DEVICE;
  
  /* Get UDP connection */ 
  if ((netfd = open(udp_device, O_RDWR)) < 0) 
  {
    fprintf(stderr,"Error opening UDP device\n");
    return -1;
  }
  
  /* Configure UDP connection */
  udpopt.nwuo_flags = NWUO_COPY | NWUO_LP_SEL | NWUO_EN_LOC | NWUO_DI_BROAD 
                   | NWUO_RP_SET | NWUO_RA_SET | NWUO_RWDATALL | NWUO_DI_IPOPT;
  udpopt.nwuo_remaddr = dirhost;
  udpopt.nwuo_remport = (udpport_t) htons(port);
  
  if ((result = ioctl(netfd, NWIOSUDPOPT, &udpopt) ) <0) 
  {
    fprintf(stderr, "Error establishing communication\n");
    printf("Error:  %d\n",result);
    close(netfd);
    return -1;
  }
  
  /* Get configuration for UDP comm */ 
  if ((result = ioctl(netfd, NWIOGUDPOPT, &udpopt) ) < 0) 
  {
    fprintf(stderr,"Error getting configuration\n");
    printf("Error: %d\n", result);
    close(netfd);
    return -1;
  }

  header->uih_src_addr = udpopt.nwuo_locaddr;
  header->uih_dst_addr = udpopt.nwuo_remaddr;
  header->uih_src_port = udpopt.nwuo_locport;
  header->uih_dst_port = udpopt.nwuo_remport;
 
  return netfd;
}

int main(int argc,char *argv[]) {
  int fd;
  ssize_t data_read;
  udp_buffer_t buffer_send, buffer_rec;
  fd_set fds_read;
  int ret;
  struct timeval timeout;

  /* Check parameters */
  if (argc !=2) {
    fprintf(stderr,"Usage: %s host\n", argv[0]);
    exit(-1);
  }

  if ((fd = udp_conf(argv[1], PORT, &buffer_send.header) ) < 0) 
    exit(-1);	

  while (1) 
  {

    /* init fd_set */
    FD_ZERO(&fds_read);
    FD_SET(0, &fds_read);
    FD_SET(fd, &fds_read); 

    /* set timeval */
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    printf("Send data: ");
    fflush(stdout);
    /* Wait until it is possible to write with select */
    ret = select(4, &fds_read, NULL, NULL, &timeout);
    if (ret < 0) {
    	fprintf(stderr, "Error on select waiting for read: %d\n", errno);
    	exit(-1);
    }
    if (ret == 0) {
    	printf("\nClient says: Hey! I want to send data!!\n");
    	fflush(stdout);
    	continue;
    }
    /* if got message from server */
    if (FD_ISSET(fd, &fds_read)) {
    	data_read = read(fd, &buffer_rec, sizeof(udp_buffer_t));
    	printf("Server says: %s\n\n", buffer_rec.data);
    	fflush(stdout);
    }
    /* if got data from terminal */
    if (FD_ISSET(0, &fds_read)) {
    	/* Get a string and send it */
    	gets(buffer_send.data);
    	write(fd, &buffer_send, sizeof(udp_buffer_t));

    	/* If data sent is exit then break */
    	if (!strcmp(buffer_send.data,"exit")) 
      		break;
    	/* Get server response */
    	data_read = read(fd, &buffer_rec, sizeof(udp_buffer_t));
    	printf("Received: %s\n\n", buffer_rec.data);
    	fflush(stdout);
    } 
  }
  
  /* Close UDP communication */
  close(fd);
}
