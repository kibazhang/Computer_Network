#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>

#define BUFFSIZE 1024
#define LISTENQ 1024

int max(int num1, int num2){
  if(num1 > num2){
    return num1;
  }
  else if(num2 >= num1){
    return num2;
  }
}

int open_listenfd(int sock_type, int port) 
{ 
  int listenfd, optval=1; 
  struct sockaddr_in serveraddr; 
  /* Create a socket descriptor */ 
  if(sock_type == 0){
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
      return -1; 
  }
  else if(sock_type == 1){
    if ((listenfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
      return -1; 
  }
  /* Eliminates "Address already in use" error from bind. */ 
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
		 (const void *)&optval , sizeof(int)) < 0) 
    return -1;
  /* Listenfd will be an endpoint for all requests to port 
     on any IP address for this host */ 
  bzero((char *) &serveraddr, sizeof(serveraddr)); 
  serveraddr.sin_family = AF_INET; 
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
  serveraddr.sin_port = htons((unsigned short)port); 
  if (bind(listenfd,(struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
    return -1; 
  /* Make it a listening socket ready to accept 
     connection requests */ 
  if(!sock_type){
    if (listen(listenfd, LISTENQ) < 0)
      return -1; 
  }
  return listenfd; 
}

int main(int argc, char **argv) 
{
  fd_set rfds;
  int retrival, maxfd, listenfdA, listenfdB, connfd, portA, portB, rs;
  socklen_t  clientlen;
  struct sockaddr_in clientaddr;
  struct hostent *hp;
  char *dotip; 
  char *command;
  char buff[BUFFSIZE];
  ssize_t s_read;

  portA = atoi(argv[1]); /* the server listens on a port passed 
			   on the command line for http service*/
  portB = atoi(argv[2]); /* the server listens on a port passed 
			   on the command line for Ping service*/

  listenfdA = open_listenfd(0, portA); //TCP socket discriptor
  listenfdB = open_listenfd(1, portB); //UDP socket discriptor

  while (1)
    {
      FD_ZERO(&rfds);
      FD_SET(listenfdA, &rfds);
      FD_SET(listenfdB, &rfds);
      maxfd = max(listenfdA, listenfdB) + 1;

      retrival = select(maxfd, &rfds, NULL, NULL, NULL);

      if(FD_ISSET(listenfdA, &rfds)){
	clientlen = sizeof(clientaddr);
	connfd = accept(listenfdA, (struct sockaddr *) &clientaddr, &clientlen);
	hp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	dotip = inet_ntoa(clientaddr.sin_addr);
	printf("Fd %d connected to %s (%s:%d)\n", connfd, hp->h_name, dotip, ntohs(clientaddr.sin_port));
	bzero(buff,BUFFSIZE);
	if (fork() ==0)
	  {
	    close(listenfdA);
	    if(read(connfd,buff,BUFFSIZE) != 0)
	      {
		command = strtok(buff," ");
		if(!strcmp(command,"get") || !strcmp(command,"GET"))
		  {
		    command = strtok(NULL, "/ ");
		    int fd  = open(command, O_RDONLY);
		    if(errno == ENOENT)
		      {
			strcpy(buff,"HTTP/1.0 404 Not Found\r\n\r\n");
			write(connfd,buff,BUFFSIZE);
		      }
		    else if(errno == EACCES)
		      {
			strcpy(buff,"HTTP/1.0 403 Forbidden\r\n\r\n");
			write(connfd,buff,BUFFSIZE);
		      }
		    else 
		      {
			strcpy(buff,"HTTP/1.0 200 OK\r\n\r\n");
			write(connfd,buff,BUFFSIZE);
			do
			  {
			    bzero(buff,BUFFSIZE);
			    rs = read(fd,buff,BUFFSIZE);
			    printf("number of bytes read: %d \n",rs); // for debug purpose only
			    write(connfd,buff,rs);
			  } while (rs > 0);
			close(fd);
		      }
		  } 
	      }
	    exit(0);
	  }
	close(connfd);
      }
      else if(FD_ISSET(listenfdB, &rfds)){
	clientlen = sizeof(clientaddr);
	s_read = recvfrom(listenfdB, buff, BUFFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
	buff[3]++;
	hp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	dotip = inet_ntoa(clientaddr.sin_addr);
	printf("Fd %d connected to %s (%s:%d)\n", connfd, hp->h_name, dotip, ntohs(clientaddr.sin_port));
	if(sendto(listenfdB, buff, s_read, 0, (struct sockaddr *) &clientaddr, clientlen) != s_read){
	  printf("Error with printing response.\n");//debugging only
	}
      }
    }
}
