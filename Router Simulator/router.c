#include "ne.h"
#include "router.h"
#include <sys/select.h>

typedef struct{
  unsigned int nbr_id;
  int update_counter;
  int flag;
}nbr_c;

void initial_nbr(nbr_c* nbr_check, unsigned int no_nbr, struct pkt_INIT_RESPONSE* response){
  int i;
  for(i = 0; i < no_nbr; i++){
    nbr_check[i].nbr_id = (response -> nbrcost)[i].nbr;
    nbr_check[i].update_counter = 0;
    nbr_check[i].flag = 1;
  }
}

void inc_nbr(nbr_c* nbr_check, unsigned int no_nbr){
  int i;
  for(i = 0; i < no_nbr; i++){
    nbr_check[i].update_counter += UPDATE_INTERVAL;
    //printf("%d\n", nbr_check[i].update_counter);
  }
}

void update_nbr(nbr_c* nbr_check, unsigned int no_nbr, unsigned id){
  int i;
  for(i = 0; i < no_nbr; i++){
    if(nbr_check[i].nbr_id == id){
      nbr_check[i].update_counter = 0;
    }
  }
}

int final_nbr(nbr_c* nbr_check, unsigned int no_nbr, FILE* LOG_FILE, unsigned int route_id){
  int i;
  int check = 0;
  for(i = 0; i < no_nbr; i++){
    //printf("id = %d\n", nbr_check[i].nbr_id);
    //printf("%d\n", nbr_check[i].update_counter);
    if(nbr_check[i].update_counter >= FAILURE_DETECTION){
      if(nbr_check[i].flag){
	UninstallRoutesOnNbrDeath(nbr_check[i].nbr_id);
	PrintRoutes(LOG_FILE, route_id);
	check += 1;
	nbr_check[i].flag *= 0;
      }
    }else{
      if(!nbr_check[i].flag){
	check += 0;
	nbr_check[i].flag = 1;
      }
    }
  }
  return check; 
}
 
int main (int argc, char **argv)
{
  int clientfd, port_host, port_route;
  int count = 0;
  int total_count = 0;
  int temp1, temp2, temp_nbr_cost;
  int update_check;//check for update
  nbr_c nbr_check[MAX_ROUTERS];//check for neighbor update
  int i; //loop invariant
  unsigned int no_nbr;//number of connected neighbors
  unsigned int route_id;
  char *host;
  struct timeval tv;
  struct pkt_RT_UPDATE Update_Packet;
  struct pkt_RT_UPDATE Recvd_Packet;
  fd_set rfds;
  int retval;
  route_id = atoi(argv[1]);
  host = argv[2];
  port_host = atoi(argv[3]);
  port_route = atoi(argv[4]);

  //open a file discriptor
  struct hostent *hp; 
  struct sockaddr_in serveraddr;
  socklen_t serverlen;

  struct sockaddr_in routeraddr;
  
  struct pkt_INIT_REQUEST request;
  struct pkt_INIT_RESPONSE response;

  if ((clientfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    return -1; /* check errno for cause of error */ 
  /* Fill in the server's IP address and port */ 
  if ((hp = gethostbyname(host)) == NULL) 
    return -2; /* check h_errno for cause of error */ 
  bzero((char *) &serveraddr, sizeof(serveraddr)); 
  serveraddr.sin_family = AF_INET; 
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)port_host);

  bzero((char *) &routeraddr, sizeof(routeraddr)); 
  routeraddr.sin_family = AF_INET; 
  routeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  routeraddr.sin_port = htons((unsigned short)port_route);

  //Bind the client
  if(bind(clientfd, (struct sockaddr *)&routeraddr, sizeof(routeraddr)) < 0)
    return -1;
  
  /*Fill in the port of the route and covert the route_id to net work format*/
  request.router_id = htonl(route_id);
  //Send the request
  serverlen = sizeof(serveraddr);
  sendto(clientfd, &request, sizeof(request), 0, (struct sockaddr *) &serveraddr, serverlen);
  //Recieve the request
  recvfrom(clientfd, &response, sizeof(response), 0, (struct sockaddr *) &serveraddr, &serverlen);
  //Convert the response to host format
  ntoh_pkt_INIT_RESPONSE(&response);
  //Initialize the routing table
  InitRoutingTbl(&response, route_id);
  no_nbr = response.no_nbr;
  initial_nbr(nbr_check, no_nbr, &response);
  //Just for testing purpose to print the log file
  char log_file_name[15];
  sprintf(log_file_name, "router%d.log", route_id);
  FILE* LOG_FILE = fopen(log_file_name, "w+");
  if(LOG_FILE == NULL){
    printf("\n Unable to open the log file.\n");
  } else {
    PrintRoutes(LOG_FILE, route_id);
  }
  tv.tv_sec = UPDATE_INTERVAL; tv.tv_usec = 0; //wait for update


  //send the update in case
  for(i = 0; i < no_nbr; i++){
    ConvertTabletoPkt(&Update_Packet, route_id); 
    Update_Packet.dest_id = (response.nbrcost)[i].nbr;
    hton_pkt_RT_UPDATE(&Update_Packet);
    sendto(clientfd, &Update_Packet, sizeof(Update_Packet), 0, (struct sockaddr *) &serveraddr, serverlen);
  }

  //Update the routing table in side an infinity while loop
  while(1){
    FD_ZERO(&rfds);
    FD_SET(clientfd, &rfds);
    
    retval = select(clientfd + 1, &rfds, NULL, NULL, &tv);
    
    if(FD_ISSET(clientfd, &rfds)){
      recvfrom(clientfd, &Recvd_Packet, sizeof(Recvd_Packet), 0, (struct sockaddr *) &serveraddr, &serverlen);
      ntoh_pkt_RT_UPDATE(&Recvd_Packet);
      update_nbr(nbr_check, no_nbr, Recvd_Packet.sender_id);
      for(temp1 = 0; temp1 < Recvd_Packet.no_routes; temp1++){
	if((Recvd_Packet.route)[temp1].dest_id == route_id){
	  break;
	} 
      }

      for(temp2 = 0; temp2 < no_nbr; temp2++){
	/*
		*/
	if(Recvd_Packet.sender_id == (response.nbrcost)[temp2].nbr){
	  break;
	}
      }
      temp_nbr_cost = (response.nbrcost)[temp2].cost;
      /*
      if((Recvd_Packet.route)[temp1].cost == INFINITY){
	temp_nbr_cost = INFINITY;
      }
      */
      //if(UpdateRoutes(&Recvd_Packet, (Recvd_Packet.route)[temp].cost, route_id) == 1){
      if(UpdateRoutes(&Recvd_Packet, temp_nbr_cost, route_id) == 1){
	PrintRoutes(LOG_FILE, route_id);
	update_check += 1;
      }
      //printf("ii - %d\n", nbr_check[i].update_counter);
      if(final_nbr(nbr_check, no_nbr, LOG_FILE, route_id)){
	update_check += 1;
      }

    }else{
      //send the package here
      for(i = 0; i < no_nbr; i++){
	ConvertTabletoPkt(&Update_Packet, route_id); 
	Update_Packet.dest_id = (response.nbrcost)[i].nbr;
	hton_pkt_RT_UPDATE(&Update_Packet);
	sendto(clientfd, &Update_Packet, sizeof(Update_Packet), 0, (struct sockaddr *) &serveraddr, serverlen);
      }
      inc_nbr(nbr_check, no_nbr);
      total_count += UPDATE_INTERVAL;
      if(!update_check){
	count += UPDATE_INTERVAL;
      }else{
	count = 0;
      }
      update_check = 0;
      //printf("count = %d\n", count);
      if(count == CONVERGE_TIMEOUT){
	fprintf(LOG_FILE, "%d:Converged", total_count);
	fflush(LOG_FILE);     
      }      
      tv.tv_sec = UPDATE_INTERVAL; tv.tv_usec = 0; //wait for update 
    }
  }

  //close the log file
  fclose(LOG_FILE);
  //close the socket
  close(clientfd);
  exit(0);
}

