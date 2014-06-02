#include "ne.h"
#include "router.h"

struct route_entry routingTable[MAX_ROUTERS];
int NumRoutes;

void InitRoutingTbl(struct pkt_INIT_RESPONSE *InitResponse, int myID){
  int i;

  NumRoutes = InitResponse -> no_nbr + 1;
  //Set up a link to myself
  routingTable[0].dest_id = myID;
  routingTable[0].next_hop = myID;
  routingTable[0].cost = 0;
  for(i = 1; i < NumRoutes; i++){ 
    routingTable[i].dest_id = (InitResponse -> nbrcost)[i - 1].nbr;
    routingTable[i].next_hop = (InitResponse -> nbrcost)[i - 1].nbr;
    routingTable[i].cost = (InitResponse -> nbrcost)[i - 1].cost;
  }
}

int UpdateRoutes(struct pkt_RT_UPDATE *RecvdUpdatePacket, int costToNbr, int myID){
  int i;
  int j;
  int temp_check = 0;
  unsigned int temp_cost;

  for(i = 0; i < (RecvdUpdatePacket -> no_routes); i++){
    for(j = 0; j < NumRoutes; j++){
      if(routingTable[j].dest_id == (RecvdUpdatePacket -> route)[i].dest_id){
	break;
      }
    }
    //check any unknown route
    if(j >= NumRoutes){
      routingTable[j].dest_id = (RecvdUpdatePacket -> route)[i].dest_id;
      routingTable[j].next_hop = RecvdUpdatePacket -> sender_id;
      if(costToNbr >= INFINITY || (RecvdUpdatePacket -> route)[i].cost >= INFINITY){
	routingTable[j].cost = INFINITY;
      }else{
	routingTable[j].cost = (RecvdUpdatePacket -> route)[i].cost + costToNbr;
      }
      NumRoutes++;
      temp_check = 1;
    }else{
      temp_cost = costToNbr + (RecvdUpdatePacket -> route)[i].cost;
      //inplement the force update and split rule
      if(routingTable[j].next_hop == RecvdUpdatePacket -> sender_id || (temp_cost < routingTable[j].cost && (RecvdUpdatePacket -> route)[i].next_hop != myID)){
	
	if(routingTable[j].cost != temp_cost && temp_cost < INFINITY){
	  routingTable[j].cost = temp_cost;
	  temp_check = 1;
	}else if(routingTable[j].cost != temp_cost && routingTable[j].cost < INFINITY && temp_cost >= INFINITY){
	  routingTable[j].cost = INFINITY;
	  temp_check = 1;
	} else if(routingTable[j].cost != temp_cost && routingTable[j].cost >= INFINITY && temp_cost >= INFINITY){
	  routingTable[j].cost = INFINITY;
	}
	if(routingTable[j].next_hop != RecvdUpdatePacket -> sender_id){
	  routingTable[j].next_hop = RecvdUpdatePacket -> sender_id;
	  temp_check = 1;
	}
      }
    }
  }

  return temp_check;
}
  
void ConvertTabletoPkt(struct pkt_RT_UPDATE *UpdatePacketToSend, int myID){
  UpdatePacketToSend -> sender_id = myID;
  UpdatePacketToSend -> no_routes = NumRoutes;
  int i;
  for(i = 0; i < NumRoutes; i++){
    (UpdatePacketToSend -> route)[i].dest_id = routingTable[i].dest_id;
    (UpdatePacketToSend -> route)[i].next_hop = routingTable[i].next_hop;
    (UpdatePacketToSend -> route)[i].cost = routingTable[i].cost;
  }
}

void PrintRoutes(FILE* Logfile, int myID){
  fprintf(Logfile, "\nRouting Table:\n");
  int i;
  for(i = 0; i < NumRoutes; i++){
    fprintf(Logfile, "R%d -> R%d: R%d, %d\n", myID, routingTable[i].dest_id, routingTable[i].next_hop, routingTable[i].cost);
    fflush(Logfile);
  }
}

void UninstallRoutesOnNbrDeath(int DeadNbr){
  int i;
  for(i = 0; i < NumRoutes; i++){
    if(routingTable[i].next_hop == DeadNbr){
      routingTable[i].cost = INFINITY;
    }
  }
}
