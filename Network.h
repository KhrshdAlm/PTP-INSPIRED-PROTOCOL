#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include <sstream>
#include <string>
#include "Node.h"

using namespace ns3;

enum MSG{
  SYNC,
  FOLLOW,
  DREQ,
  DRPLY
};

enum STATE{
  INACTIVE,
  ACTIVE,
  WAITING,
  SYNCED
};

class SocketPoint{
public:
  SocketPoint( 
  const uint32_t id,
  const Ipv4Address txIp,
  const uint16_t txPort,
  const Ipv4Address recvIp,
  const uint16_t recvPort,
  const Ptr<Socket> sock )
  : nodeId(id),
    txIp(txIp),
    txPort(txPort),
    recvIp(recvIp),
    recvPort(recvPort),
    sock(sock)
  {
  }

  uint32_t getNodeId(){
    return this->nodeId;
  }

  uint16_t getTxPort(){
    return this->txPort;
  }

  uint16_t getRecvPort(){
    return this->recvPort;
  }

  Ipv4Address getTxIp(){
    return this->txIp;
  }

  Ipv4Address getRecvIp(){
    return this->recvIp;
  }

  Ptr<Socket> getSocket(){
    return this->sock;
  }

private:
  const uint32_t nodeId;
  const Ipv4Address txIp;
  const uint16_t txPort;
  const Ipv4Address recvIp;
  const uint16_t recvPort;
  const Ptr<Socket> sock;
};


class WirelessNetwork
{
public:
  WirelessNetwork (
    const uint32_t users,
    const std::vector<int> neighbourNode,
    const uint32_t packetSize, 
    const Time interPacketInterval )
    : m_users(users),
      m_neighbourNode(neighbourNode),
      m_packetSize (packetSize),
      m_interPacketInterval (interPacketInterval)
  {
    masterIndex = 0;
  }

  void SetSocketIndex( int* index){
    sock_index = index;
  }

  void SetSocketPoint( std::vector< SocketPoint * > &sinks ){
    socketsInNetwork = sinks;
  }

  void addNodesToNetwork( std::vector< WirelessNode * > &nodesInNetwork){
    nodes = nodesInNetwork;
    globalTime = NanoSeconds( Simulator::Now() );
    int j;
    nodes[masterIndex]->setNodeAsMaster();
    for( j=0; j< nodes.size(); j++){
      nodes[j]->setIntialTime( globalTime );
    }
  }

  WirelessNode * getNode(int index){
    return nodes[index];
  }


  void printClockValuesOfNodes(){
    int nodeId, state;
    Time clockTime, clockOffset;
    std::string nodeState;
    std::cout<<"******Node_Id ======> ClockTime ======> NodeState ======> Offset******"<<std::endl;
    
    for(int j=0;j<m_users;j++){
      
      nodeId = j+1;
      clockTime = this->getNode(j)->getLocalTime();
      state = this->getNode(j)->getState();
      switch( state ){
        case 0: nodeState = "INACTIVE";
                break;
        case 1: nodeState = "ACTIVE";
                break;
        case 2: nodeState = "WAITING";
                break;
        case 3: nodeState = "SYNCED";
                break;
      }

      if( state == 3 ){
        clockOffset = this->getNode(j)->getOffset();
        std::cout << "        " << nodeId << "              "<< clockTime.GetNanoSeconds() << "        "<< nodeState << "           "<< clockOffset.GetNanoSeconds() << std::endl;  
      }else{
        std::cout << "        " << nodeId << "              "<< clockTime.GetNanoSeconds() << "        "<< nodeState << "           "<< "N/A" << std::endl; 
      }
      
    }
    std::cout<<"*************************************************************************" << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;
  }

  
  // Master Starts the Synchronization by Sending SYNC pkt to nodes within transmitting range
  void startProtocol(){
    uint8_t i;
    Ptr<Socket> socketToNeighbour;
    WirelessNode * master = this->getNode(masterIndex);
    master->setState(SYNCED);
    for( i = 0 ; i < master->getNumNeighbour(); i++){
      socketToNeighbour = socketsInNetwork[master->getNeighbour(i)]->getSocket();
      Simulator::Schedule (m_interPacketInterval, &WirelessNetwork::sendSyncFollowPacket, this,
        socketToNeighbour, m_interPacketInterval);
    }
  }

  

  void sendSyncFollowPacket( Ptr<Socket> socket, Time pktInterval ){
    uint16_t i=0, nodeId, nodeIndex;
    std::stringstream msgx, msgy;
    /* Following information is enclosed in the packet : 
              senderHop,
              msgType ( 0 - Sync, 1 - follow up, 2 - Dreq, 3 - Drply ),
              dreqAtMaster,
              syncSendTime,
              recvDreqFromMaster(hop1),
              recvDreqFromSlave(hop1),
              sentDreq(hop1),
              recvDreqFromMaster(hop2),
              recvDreqFromSlave(hop2),
              sentDreq(hop2),
              ....
              ....
    */
    WirelessNode * txNode;
    // Identifying the socket's node
    while( !(socketsInNetwork[i]->getSocket() == socket) ){
            i++;
    }

    nodeId = socketsInNetwork[i]->getNodeId();
    nodeIndex = nodeId - 1;
    txNode = this->getNode(nodeIndex);
    txNode->incrementPktSentCounter();
     
    // Sending the SYNC packet 
    msgx << txNode->getNodeHop() << '#' << SYNC << '#' << txNode->getDreqAtMaster().GetNanoSeconds() << '#' 
    << txNode->getSyncSendTime().GetNanoSeconds();

    std::cout << " At " << txNode->getIpv4Address() << " SYNC msg = " << msgx.str() << std::endl;
    
    Ptr<Packet> sync_pkt = Create<Packet>((uint8_t*) msgx.str().c_str(), m_packetSize);
    socket->Send ( sync_pkt );       
    txNode->setLocalTime(NanoSeconds(Simulator::Now()));
    txNode->setSyncSendTime( txNode->getLocalTime() );
    printClockValuesOfNodes();
      
    // Sending the FOLLOW_UP packet
    msgy << txNode->getNodeHop() << '#' << FOLLOW << '#' << txNode->getDreqAtMaster().GetNanoSeconds() << '#' 
    << txNode->getSyncSendTime().GetNanoSeconds() ;

    std::cout << " At " << txNode->getIpv4Address() << " FOLLOW msg = " << msgy.str() << std::endl; 

    Ptr<Packet> follow_pkt = Create<Packet>((uint8_t*) msgy.str().c_str(), m_packetSize);
    socket->Send ( follow_pkt );       
    txNode->setLocalTime(NanoSeconds(Simulator::Now()));
    printClockValuesOfNodes();
  }

 
 void sendDreqPacket(Ptr<Socket> socket){
    uint16_t i=0, j, nodeId, nodeIndex;
    std::stringstream msgx;
    /* Following information is enclosed in the packet : 
              senderHop,
              msgType ( 0 - Sync, 1 - follow up, 2 - Dreq, 3 - Drply ),
              dreqAtMaster,
              syncSendTime,
              recvDreqFromMaster(hop1),
              recvDreqFromSlave(hop1),
              sentDreq(hop1),
              recvDreqFromMaster(hop2),
              recvDreqFromSlave(hop2),
              sentDreq(hop2),
              ....
              ....
    */
    WirelessNode * txNode;
    Ptr<Socket> socketToNeighbour;    
    // Identifying the socket's node
    while( !(socketsInNetwork[i]->getSocket() == socket) ){
      i++;
    }

    nodeId = socketsInNetwork[i]->getNodeId();
    nodeIndex = nodeId - 1;
    txNode = this->getNode(nodeIndex);
    txNode->incrementPktSentCounter();
    txNode->setState(ACTIVE);
    // Sending the DREQ packet 
    msgx << txNode->getNodeHop() << '#' <<  DREQ << '#' << txNode->getDreqAtMaster().GetNanoSeconds() << '#' 
    << txNode->getSyncSendTime().GetNanoSeconds() ;

    int vectorSize = txNode->getTimeVectorSize();
    for( int m=0; m < vectorSize; m++){
      msgx << '#' << txNode->getTimeStamp(m).GetNanoSeconds() ;
    }

    std::cout << " At " << txNode->getIpv4Address() << " DREQ msg = " << msgx.str() << std::endl;

    Ptr<Packet> dreq_pkt = Create<Packet>((uint8_t*) msgx.str().c_str(), m_packetSize);

    for( j = 0 ; j < txNode->getNumNeighbour(); j++){
        socketToNeighbour = socketsInNetwork[txNode->getNeighbour(j)]->getSocket();
        socketToNeighbour->Send ( dreq_pkt );    
    }
       
    txNode->setLocalTime(NanoSeconds(Simulator::Now()));
    txNode->addTimeStamp( txNode->getLocalTime(), txNode->getNodeHop(), 2);
    printClockValuesOfNodes();
  } 


void sendDrplyPacket(Ptr<Socket> socket){
    uint16_t i=0, j, nodeId, nodeIndex;
    std::stringstream msgx, msgy;
    /* Following information is enclosed in the packet : 
              senderHop,
              msgType ( 0 - Sync, 1 - follow up, 2 - Dreq, 3 - Drply ),
              dreqAtMaster,
              syncSendTime,
              recvDreqFromMaster(hop1),
              recvDreqFromSlave(hop1),
              sentDreq(hop1),
              recvDreqFromMaster(hop2),
              recvDreqFromSlave(hop2),
              sentDreq(hop2),
              ....
              ....
    */
    WirelessNode * txNode;
    Ptr<Socket> socketToNeighbour;
    // Identifying the socket's node
    while( !(socketsInNetwork[i]->getSocket() == socket) ){
      i++;
    }

    nodeId = socketsInNetwork[i]->getNodeId();
    nodeIndex = nodeId - 1;
    txNode = this->getNode(nodeIndex);
    txNode->incrementPktSentCounter();
      
    // Sending the DRPLY packet 
    msgx << txNode->getNodeHop() << '#' <<  DRPLY << '#' << txNode->getDreqAtMaster().GetNanoSeconds() << '#' 
    << txNode->getSyncSendTime().GetNanoSeconds();

    if( txNode->getNodeHop() > 0 ){
      int vectorSize = txNode->getTimeVectorSize();
      for( int m=0; m < vectorSize; m++){
        msgx << '#' << txNode->getTimeStamp(m).GetNanoSeconds();
      }
    }
      
    std::cout << " At " << txNode->getIpv4Address() << " DRPLY msg = " << msgx.str() << std::endl;

    Ptr<Packet> drply_pkt = Create<Packet>((uint8_t*) msgx.str().c_str(), m_packetSize);

    for( j = 0 ; j < txNode->getNumNeighbour(); j++){
        socketToNeighbour = socketsInNetwork[txNode->getNeighbour(j)]->getSocket();
        socketToNeighbour->Send ( drply_pkt );    
    }

    txNode->setLocalTime(NanoSeconds(Simulator::Now()));
    printClockValuesOfNodes();
  } 

  void receivePacket (Ptr<Socket> socket)
  {  
    globalTime = NanoSeconds(Simulator::Now()); // record time when pkt is received at socket
    nodes[masterIndex]->setLocalTime(globalTime);
    Time syncSendTime, dreqAtMaster;
    uint16_t senderHop, myHop;
    uint16_t i = 0, nodeId, nodeIndex;
    int count = 0, MSG_TYPE;
    char delim = '#';
    
    WirelessNode *recvNode;
    Ptr<Socket> socketToNeighbour;
    Ptr<Packet> pkt_received = socket->Recv();
    uint8_t *buffer = new uint8_t[pkt_received->GetSize()]; 
    pkt_received->CopyData (buffer, pkt_received->GetSize());

    while( !(socketsInNetwork[i]->getSocket() == socket) ){
      i++;
    }
    
    // Determine the node of receiving socket
    nodeId = socketsInNetwork[i]->getNodeId();
    nodeIndex = nodeId - 1 ;
    
    // Get Pointer to the node
    recvNode = this->getNode( nodeIndex );
    recvNode->setLocalTime( globalTime );
    recvNode->incrementPktRecvCounter();
    myHop = recvNode->getNodeHop();



    std::vector<Time> recvTimeStamps;
    std::string msg_received(reinterpret_cast<char*>(buffer));
    std::cout << "msg received -> " << msg_received << std::endl; 
    std::stringstream msg_stream( msg_received );
    std::string item;
    
    // Read contents of the packet
    senderHop = buffer[0] - '0';
    MSG_TYPE = buffer[2] - '0';
    std::string msgType;
    switch( MSG_TYPE ){
      case 0 : msgType = "SYNC";
               break;
      case 1 : msgType = "FOLLOW";
               break;
      case 2 : msgType = "DREQ";
               break;
      case 3 : msgType = "DRPLY";
               break;
    }

    // Activating hop-1 Nodes
    if( myHop == 1 && MSG_TYPE == SYNC && recvNode->getState() == INACTIVE ){
      recvNode->setState(ACTIVE);
    }

    // Activating hop-2,3 .. nodes
    if( myHop > 1 && MSG_TYPE == DREQ && recvNode->getState() == INACTIVE ){
      recvNode->setState(ACTIVE);
    }

    std::cout << " ---------------------------------------------------------" << std::endl;
    std::cout << "|    Sender : " << socketsInNetwork[i]->getRecvIp() << "          Receiver : " << socketsInNetwork[i]->getTxIp() << "    |" << std::endl;
    std::cout << "|    Sender-Hop-Number : " << senderHop << "      MSG_TYPE : " << msgType << "    |" << std::endl;
    while (getline(msg_stream, item, delim)) {
        if( count >= 2 ){
        if( count >=4 ){
          recvTimeStamps.push_back( NanoSeconds( stoll( item, nullptr, 10 ) ) );
        }else if( count == 2 ){
          dreqAtMaster = NanoSeconds( stoll( item, nullptr, 10 ) );
          std::cout << "|    dreqAtMaster : " << dreqAtMaster.GetNanoSeconds() ;
        }else{
          syncSendTime = NanoSeconds( stoll( item, nullptr, 10 ) );
          std::cout << "  syncSendTime : " << syncSendTime.GetNanoSeconds() << " |" << std::endl;
        }
      }
      count++;
    }

    for(int g=0;g< recvTimeStamps.size(); g++){
      std::cout << "|    " << recvTimeStamps[g].GetNanoSeconds() << "       |" << std::endl;
    }
    
    if( MSG_TYPE == DREQ ){

      if( senderHop < myHop ){
        // store the timestamps and wait for sometime and then send a DREQ pkt 
        recvNode->copyTimeVector(dreqAtMaster,syncSendTime,recvTimeStamps);
        recvNode->addTimeStamp( recvNode->getLocalTime(), myHop, 0 );
        recvNode->setWaitingTime();
        recvNode->setState(WAITING);
        std::cout << "|    waiting Time = " << recvNode->getWaitingTime() << std::endl;
        Simulator::Schedule (recvNode->getWaitingTime(), &WirelessNetwork::sendDreqPacket, this, socket);

      }else if( senderHop > myHop ){
       // record the timestamp of Dreq pkt and send a DRPLY containing that timestamp 
        if( myHop > 0 ){
          recvNode->addTimeStamp( recvNode->getLocalTime(), myHop, 1 );
        }else{
          recvNode->setDreqAtMaster( recvNode->getLocalTime() );
        }

        Simulator::Schedule ( NanoSeconds(1), &WirelessNetwork::sendDrplyPacket, this, socket);
      
      }else{
        // ignore
      }

    }else if( MSG_TYPE == DRPLY ){
      
      if( senderHop < myHop && recvNode->getState() == ACTIVE ){
        // rply from master, store the timstamp
        if( senderHop > 0 ){
          recvNode->copyTimeVector( dreqAtMaster, syncSendTime, recvTimeStamps );
        }else{
          recvNode->setDreqAtMaster(dreqAtMaster);
          recvNode->setSyncSendTime(syncSendTime);
        }
        recvNode->calculateOffset();
        recvNode->setState(SYNCED);
      }else if( senderHop > myHop ){
        // ignore
      }

    }else if( MSG_TYPE == SYNC ){

      if( myHop == 1 ){ // hop1 nodes
        // store SYNC receive time and wait for follow up
        recvNode->addTimeStamp( recvNode->getLocalTime(), myHop, 0 );
      }

    }else if( MSG_TYPE == FOLLOW ){

      if( myHop == 1 ){ 
        // store SYNC transmit time using timestamp in pkt and send DREQ 
        recvNode->setSyncSendTime(syncSendTime);
        Simulator::Schedule ( m_interPacketInterval, &WirelessNetwork::sendDreqPacket, this, socket);
      }

    }
    printClockValuesOfNodes();    
  }

private:
  const uint32_t m_users;
  const std::vector<int> m_neighbourNode;
  const uint32_t m_packetSize;
  const Time m_interPacketInterval;
  uint16_t masterIndex;
  Time globalTime;
  int* sock_index;
  std::vector< SocketPoint * > socketsInNetwork;
  std::vector< WirelessNode * > nodes;
};