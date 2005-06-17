/*
 * Test name: test07_cli.c
 *
 * Objective: Test an impatient TCP client with timeout which waits input both 
 * from the terminal and from the network connection.
 *
 * Description: Implements a echo client using the TCP protocol with a timeout
 * value. It waites for data on both the terminal and the network connection and
 * prints an angry message asking for more data if there is a timeout.
 * 
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
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/hton.h>

#define PORT 6060L

int tcp_connect(char *host, long port)
{
  /* creates a tcp connection with specified host and port */
  char *tcp_device;
  struct hostent *hp;
  int netfd;
  nwio_tcpcl_t tcpcopt;
  nwio_tcpconf_t tcpconf;
  ipaddr_t dirhost;
  int tries;
  int result;
  
  /* get host address */
  if ((hp = gethostbyname(host)) == (struct hostent*) NULL) 
  {
    fprintf(stderr,"Unknown host\n");
    return(-1);
  }
  memcpy((char *)&dirhost, (char *)hp->h_addr, hp->h_length);
  
  /* Get default TCP device */
  if (( tcp_device = getenv("TCP_DEVICE") ) == NULL) 
    tcp_device = TCP_DEVICE;
  
  /* Establish TCP connection */ 
  if ((netfd = open(tcp_device, O_RDWR)) < 0) 
  {
    fprintf(stderr,"Error opening TCP device\n");
    return -1;
  }
  
  /* Configure TCP connection */
  tcpconf.nwtc_flags=NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
  tcpconf.nwtc_remaddr = dirhost;
  tcpconf.nwtc_remport = (tcpport_t) htons(port);
  
  if ((result = ioctl(netfd, NWIOSTCPCONF, &tcpconf) ) <0) 
  {
    fprintf(stderr, "Error establishing communication\n");
    printf("Error:  %d\n",result);
    close(netfd);
    return -1;
  }
  
  /* Get configuration for TCP comm */ 
  if ((result = ioctl(netfd, NWIOGTCPCONF, &tcpconf) ) < 0) 
  {
    fprintf(stderr,"Error getting configuration\n");
    printf("Error: %d\n", result);
    close(netfd);
    return -1;
  }

  /* Establish connection */
  tcpcopt.nwtcl_flags = 0;
  tries = 0; 
  while (tries < 10) {
  	if ( (result = ioctl(netfd, NWIOTCPCONN, &tcpcopt)) < 0 ) {
  		if (errno != EAGAIN)
  		{
  			fprintf(stderr, "Server is not listening\n");
  			close(netfd);
  			return(-1);
  		}
  		fprintf(stderr, "Unable to connect\n");
  		sleep(1);
  		tries++;
  	}
  	else
  		break;	/* Connection */
 }
 /* Check result value */
 if (result < 0) {
 	fprintf(stderr, "Error connecting\n");
 	fprintf(stderr, "Error: %d\n", result);
 	printf("Number of tries: %d\n", tries);
 	printf("Error: %d\n", errno);
 	close(netfd);
 	return -1;
 }
 return netfd;
}

int main(int argc,char *argv[]) {
  int fd;
  ssize_t data_read;
  char send_buf[1024];
  char recv_buf[1024];
  fd_set fds_read;
  int ret;
  struct timeval timeout;

  /* Check parameters */
  if (argc !=2) {
    fprintf(stderr,"Usage: %s host\n", argv[0]);
    exit(-1);
  }

  if ((fd = tcp_connect(argv[1], PORT) ) < 0) 
    exit(-1);	
  printf("Connected to server\n");

  while (1) 
  {

    /* init fd_set */
    FD_ZERO(&fds_read);
    FD_SET(0, &fds_read); /* stdin */
    FD_SET(fd, &fds_read);
    /* set timeout */
    timeout.tv_sec = 3;
    timeout.tv_usec = 0; 

    printf("Send data: ");
    fflush(stdout);
    /* Wait until it is possible to read with select */
    ret = select(4, &fds_read, NULL, NULL, &timeout);
    if (ret < 0) {
    	fprintf(stderr, "Error on select waiting for write: %d\n", errno);
    	exit(-1);
    }
    /* timeout */
    if (ret == 0) {
    	printf("\nClient says: Hey! I want to send some data!!\n");
    	continue;
    }
    /* handle data from network */
    if (FD_ISSET(fd, &fds_read)) {
    	data_read = read(fd, &recv_buf, 1024);
    	printf("Server says: %s\n\n", recv_buf);	
    }
    /* handle data from terminal */ 
    if (FD_ISSET(0, &fds_read)) {
    	/* Get a string and send it */
    	gets(send_buf);
    	write(fd, &send_buf, strlen(send_buf)+1);
  
    	/* If data sent is exit then break */
    	if (!strcmp(send_buf,"exit")) 
	      	break;
  		
    	/* Get server response */
    	data_read = read(fd, &recv_buf, 1024);
    	printf("Received: %s\n\n", recv_buf);
    }
  }
  
  /* Close TCP communication */
  close(fd);
}
