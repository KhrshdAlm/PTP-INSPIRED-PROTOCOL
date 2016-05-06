#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <ctime>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <string>
#include <iomanip>

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


//----------------------------------------Start Of SocketPoint Class-------------------------------------------------------

class SocketPoint{
public:
  SocketPoint( 
  const uint32_t senderId,
  const uint32_t receiverId,
  const Ipv4Address txIp,
  const uint16_t txPort,
  const Ipv4Address recvIp,
  const uint16_t recvPort,
  const Ptr<Socket> sock )
  : txId(senderId),
    recvId(receiverId),
    txIp(txIp),
    txPort(txPort),
    recvIp(recvIp),
    recvPort(recvPort),
    sock(sock)
  {
  }

  uint32_t getTxId(){
    return this->txId;
  }

  uint32_t getRecvId(){
    return this->recvId;
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
  const uint32_t txId;
  const uint32_t recvId;
  const Ipv4Address txIp;
  const uint16_t txPort;
  const Ipv4Address recvIp;
  const uint16_t recvPort;
  const Ptr<Socket> sock;
};

//-------------------------------------------------X--End Of SocketPoint Class--X-----------------------------------------------


//----------------------------------------------------Start Of WirelessNode Class-----------------------------------------------
using namespace ns3;

class WirelessNode{
public:
  WirelessNode(
  const uint16_t id,
  const uint16_t master_id,
  const uint16_t hop,
  const Ipv4Address ipv4Address
  )
  : node_id(id),
    masterId(master_id),
    hop_num(hop),
    node_ipv4Address(ipv4Address)
    {
    // Add some packet information
    nodeState = 0; // 0 - inactive, 1 - active, 2 - waiting( to send Dreq), 3 - Sync
    clockError = ( rand() % 6 ) * 0.012 * hop_num + 0.998;
    oldOffsetError = 0;
    newOffsetError = 0;
    isMaster = 0;
    syncSendTime = NanoSeconds (0);
    dreqAtMaster = NanoSeconds (0);
    
      for( int j=0; j < 3 * hop; j++ ){
        timeStamps.push_back( NanoSeconds (0) );
      }

      for( int j=0; j < 4; j++){
        sentPacket.push_back(0);
        receivedPacket.push_back(0);
        overheardPacket.push_back(0);
      }
    }

  void setState(int i){
    nodeState = i;
  }

  int getState(){
    return nodeState;
  }

  void setNodeAsMaster(){
    isMaster = 1;
    clockError = 1;
  }

  // called once to set the local time
  void setIntialTime( Time initialtime ){
    localTime = NanoSeconds ( 0 );
    simulatorTime = initialtime;
  }

  // called after happening of any event in the network
  void setLocalTime( Time currentSimulatorTime ){
    int newTime;
    if( !isMaster ){
      localTime = NanoSeconds((currentSimulatorTime.GetNanoSeconds () - this->getSimulatorTime().GetNanoSeconds ()) / 5 * clockError 
                            + localTime.GetNanoSeconds());
      this->setSimulatorTime( currentSimulatorTime );
    }else{
      simulatorTime = currentSimulatorTime;
      newTime = currentSimulatorTime.GetNanoSeconds() / 5;
      localTime = NanoSeconds(newTime);
    }
  }

  void copyTimeVector(Time dreqRecvMaster, Time syncSend, std::vector<Time> timeVector){
    dreqAtMaster = dreqRecvMaster;
    syncSendTime = syncSend;
    for( int i=0; i < timeVector.size(); i++){
      timeStamps[i] = timeVector[i];
    }
  }

  void addTimeStamp( Time t, int i, int j){
    int index = 3*(i-1) + j;
    timeStamps[ index ] = NanoSeconds ( t.GetNanoSeconds () );
  }

  void calculateOffset(){
    int clockOffset;
    if( !isMaster ){
      Time temp = NanoSeconds (0);
      int i,j;
      if( hop_num > 1 ){
        temp = NanoSeconds ( timeStamps[3*(hop_num-1)].GetNanoSeconds() + timeStamps[3*(hop_num-1)+2].GetNanoSeconds() );
        for( i=1;i <= hop_num-1;i++){
          temp = temp + NanoSeconds ( timeStamps[3*(i-1)].GetNanoSeconds() - timeStamps[3*(i-1)+1].GetNanoSeconds() );
          j = 3*(i-1);
        }
        temp = temp - NanoSeconds ( syncSendTime.GetNanoSeconds() + dreqAtMaster.GetNanoSeconds() );
        clockOffset = temp.GetNanoSeconds() / 2;
        offset =  NanoSeconds(clockOffset) ;

      }else{
        temp = NanoSeconds((timeStamps[0].GetNanoSeconds() - syncSendTime.GetNanoSeconds()) - 
             (dreqAtMaster.GetNanoSeconds() - timeStamps[2].GetNanoSeconds()));
        clockOffset = temp.GetNanoSeconds() / 2;
        offset =  NanoSeconds(clockOffset) ;
      }
    }
  }

  void setOldOffsetError(Time masterTime){
    oldOffsetError = std::abs((this->getLocalTime().GetNanoSeconds() - masterTime.GetNanoSeconds ())) * 1.0 / masterTime.GetNanoSeconds() ;
  }

  double getOldOffsetError(){
    return oldOffsetError;
  }

  void setNewOffsetError(Time masterTime){
    this->localTime -= offset;
    newOffsetError = std::abs((this->getLocalTime().GetNanoSeconds() - masterTime.GetNanoSeconds ())) * 1.0 / masterTime.GetNanoSeconds () ;
  }

  double getNewOffsetError(){
    return newOffsetError;
  }

  void addChildDreqTime(int childId, Time dreqTime){
    long long childTime = dreqTime.GetNanoSeconds ();
    childDreqTime.insert( std::pair< int , long long >(childId, childTime) );
  }

  Time getChildDreqTime(int childId ){
    Time dreqTime;
    std::map< int, long long >::iterator it;
    it = childDreqTime.find(childId);
      dreqTime = NanoSeconds ( it->second );
      return dreqTime;
  }

  void addDreqSendTime(int nodeId, Time dreqTime){
    long long childTime = dreqTime.GetNanoSeconds ();
    dreqSendTime.insert( std::pair< int , long long >( nodeId, childTime) );
  }

  Time getDreqSendTime(int nodeId ){
    Time dreqTime;
    std::map< int, long long >::iterator it;
    it = childDreqTime.find(nodeId);
      dreqTime = NanoSeconds ( it->second );
      return dreqTime;
  }

  void addDrplyRequest(int id){
    sendDrply.push_back(id);
  }

  int getDrplyId(){
    if( sendDrply.size() != 0 ){
      int id = sendDrply[0];
      sendDrply.erase (sendDrply.begin());
      return id;
    }else{
      return -1;
    }
  }

  uint32_t getMasterId(){
    return masterId;
  }

  Time getOffset(){
    return offset;
  }

  Time getLocalTime(){
    return this->localTime;
  }

  void setSimulatorTime( Time currentTime ){
    simulatorTime = currentTime;
  }

  void setSyncSendTime(Time sendTime){
    syncSendTime = sendTime;
  }

  void setDreqAtMaster(Time dreqTime){
    dreqAtMaster = dreqTime;
  }

  void setSyncStartTime(Time currentTime){
    int newTime = currentTime.GetNanoSeconds() / 5;
    syncStartTime = NanoSeconds(newTime);
  }

  void setSyncEndTime(Time currentTime){
    int newTime = currentTime.GetNanoSeconds() / 5;
    syncEndTime = NanoSeconds(newTime);
  }

  void setSynchronizationTime(){
    std::cout << " syncEndTime = " << syncEndTime.GetNanoSeconds() << "   syncStartTime = " << syncStartTime.GetNanoSeconds() << std::endl;
    synchronizationTime = syncEndTime - syncStartTime;
  }

  Time getSynchronizationTime(){
    return synchronizationTime;
  }

  Time getSyncSendTime(){
    return syncSendTime;
  }

  Time getDreqAtMaster(){
    return dreqAtMaster;
  }

  Time getSimulatorTime(){
    return simulatorTime;
  }

  uint16_t getNodeId(){
    return node_id;
  }

  uint16_t getNodeHop(){
    return hop_num;
  }

  uint16_t isNodeMaster(){
    return isMaster;
  }

  Ipv4Address getIpv4Address(){
    return node_ipv4Address;
  }

  uint16_t getNumNeighbour(){
    return neighbourIndex.size();
  }

  void addNeighbourIndex(int index){
    neighbourIndex.push_back(index);
  }

  int getNeighbour(int index){
    return neighbourIndex[index];
  }

  void incrementSentPacketCounter(int type){
    sentPacket[type]++;
  }

  void incrementReceivedPacketCounter(int type){
    receivedPacket[type]++;
  }

  void incrementOverheardPacketCounter(int type){
    overheardPacket[type]++;
  } 

  int getSentPacketCounter(int type){
    return sentPacket[type];
  }

  int getReceivedPacketCounter(int type){
    return receivedPacket[type];
  }

  int getOverheardPacketCounter(int type){
    return overheardPacket[type];
  }


  int getTimeVectorSize(){
    return timeStamps.size();
  }

  Time getTimeStamp(int index){
    return timeStamps[index];
  }

  void setWaitingTime(){
    waitingTime = NanoSeconds ( std::abs( timeStamps[0].GetNanoSeconds () - syncSendTime.GetNanoSeconds () ) );
  }

  Time getWaitingTime(){
    return waitingTime;
  } 

  void setReplyId(int x){
    replyId = x;
  }

  int getReplyId(){
    return replyId;
  }
  
  double getError(){
    return clockError;
  }



private:
  Time localTime;
  Time simulatorTime;
  Time syncSendTime;
  Time dreqAtMaster;
  Time offset;
  Time waitingTime;
  Time syncStartTime;
  Time syncEndTime;
  Time synchronizationTime;
  std::vector< Time > timeStamps;
  int isMaster;
  int nodeState;
  int replyId;
  double clockError;
  double oldOffsetError;
  double newOffsetError;
  const uint32_t node_id;
  const uint32_t masterId;
  const uint32_t hop_num;
  const Ipv4Address node_ipv4Address;
  std::vector< int > neighbourIndex; // index to their sockets in the list storing all the sockets of networks
  std::vector< int > sentPacket; // vector indexed by packet type(Sync, Follow, Dreq, Drply) and stores num of packets sent out 
  std::vector< int > receivedPacket;// vector indexed by packet type(Sync, Follow, Dreq, Drply) and stores num of packets received
  std::vector< int > overheardPacket;// vector indexed by packet type(Sync, Follow, Dreq, Drply) and stores num of packets overheard and ignored
  std::map< int, long long > childDreqTime; // key - nodeId , value - DreqTime
  std::map< int, long long > dreqSendTime;
  std::vector< int > sendDrply;
};



//-------------------------------------------------X--End Of WirelessNode Class--X-----------------------------------------------


//-------------------------------------------------X--Start of WirelessNetwork Class--X------------------------------------------

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
    eventId = 0;
    eventCounterIndex = 0;
    for (int i = 0; i < 100; ++i)
    {
      eventCounter.push_back(0);
    }
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


  void printClockValuesOfNodes(Ipv4Address senderIp, Ipv4Address receiverIp, uint16_t senderHop, 
    std::string msgType, Time dreqAtMaster, Time syncSendTime, int id){
    int nodeId, state, syncSent, syncRecv, followSent, followRecv, dreqSent, dreqRecv, dreplySent, dreplyRecv, width = 12;
    Time clockTime, clockOffset, presentOffset, synchronizationTime;
    std::string nodeState;
    double oldOffsetError, newOffsetError, clockError;

    std::cout << " -----------------------------------------------------------------------------------------------------------------------------------------------" << std::endl;
    std::cout << "  Sender : " << senderIp << "          Receiver : " << receiverIp << std::endl;
    std::cout << "  Sender-Hop-Number : " << senderHop << "      MSG_TYPE : " << msgType << "      Id : " << id << std::endl;
    std::cout << "  dreqAtMaster :" << std::setw(width) << dreqAtMaster.GetNanoSeconds() << 
                       " syncSendTime : " << std::setw(width) << syncSendTime.GetNanoSeconds() << std::endl;
    std::cout<<"Id => ClockDev. => ErrBeforeSync => ErrAfterSync => Time => State => Curr.Offset => Calc.Offset => Synchronization Time => Sync  => Follow  => Dreq => Drply "<<std::endl;
    
    for(int j=0;j<m_users;j++){
      
      nodeId = j+1;
      clockTime = this->getNode(j)->getLocalTime();
      state = this->getNode(j)->getState();
      syncSent = this->getNode(j)->getSentPacketCounter(SYNC);
      syncRecv = this->getNode(j)->getReceivedPacketCounter(SYNC);
      followSent = this->getNode(j)->getSentPacketCounter(FOLLOW);
      followRecv = this->getNode(j)->getReceivedPacketCounter(FOLLOW);
      dreqSent = this->getNode(j)->getSentPacketCounter(DREQ);
      dreqRecv = this->getNode(j)->getReceivedPacketCounter(DREQ);
      dreplySent = this->getNode(j)->getSentPacketCounter(DRPLY);
      dreplyRecv = this->getNode(j)->getReceivedPacketCounter(DRPLY);
      clockError = this->getNode(j)->getError();
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

      presentOffset = NanoSeconds( clockTime.GetNanoSeconds() - this->getNode(masterIndex)->getLocalTime().GetNanoSeconds() );
      
      if( state == 3 ){
        clockOffset = this->getNode(j)->getOffset();
        oldOffsetError = this->getNode(j)->getOldOffsetError();
        newOffsetError = this->getNode(j)->getNewOffsetError();
        synchronizationTime = this->getNode(j)->getSynchronizationTime();
        std::cout << std::setw(2) << nodeId << "   " << std::setw(6) << clockError << "   " << std::setw(width) << oldOffsetError << "    " << std::setw(width) << newOffsetError << "    " << std::setw(width) << clockTime.GetNanoSeconds() << "   " << std::setw(width) << nodeState << "   "<< std::setw(width) << presentOffset.GetNanoSeconds() << "   " << std::setw(width) << clockOffset.GetNanoSeconds() << "     " <<  synchronizationTime.GetNanoSeconds() << "     " << std::setw(2) << syncSent << "   " << std::setw(2) << syncRecv << std::setw(2) << "   " << followSent << std::setw(2) << "   " << followRecv << std::setw(2) << "   " << dreqSent << std::setw(2) << "   " << dreqRecv << std::setw(2) << "   " << dreplySent << std::setw(2) << "   " << dreplyRecv << std::endl;  
      }else{
        std::cout << std::setw(2) << nodeId << "   " << std::setw(6) << clockError << "   "  << std::setw(width) <<  "N/A"        << "    " << std::setw(width) << "N/A"          << "    " << std::setw(width) << clockTime.GetNanoSeconds() << "   " << std::setw(width) << nodeState << "   "<< std::setw(width) << presentOffset.GetNanoSeconds() << "   " << std::setw(width) << "N/A"                        << "     " <<  "N/A"                                << "     " << std::setw(2) << syncSent << "   " << std::setw(2) << syncRecv << std::setw(2) << "   " << followSent << std::setw(2) << "   " << followRecv << std::setw(2) << "   " << dreqSent << std::setw(2) << "   " << dreqRecv << std::setw(2) << "   " << dreplySent << std::setw(2) << "   " << dreplyRecv << std::endl;
      }
      
    }
    std::cout<<"-------------------------------------------------------------------------------------------------------------------------------------------------" << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;
  }


  void setLocalTimeAtNodes(){

    for(int j=0; j< nodes.size(); j++){
      nodes[j]->setLocalTime( globalTime );
    }
  }


  void startProtocol(){
    uint8_t i;
    Ptr<Socket> socketToNeighbour;
    WirelessNode * master = this->getNode(masterIndex);
    master->setState(SYNCED);
    // Sync and Follow Packet  
    for( i = 0 ; i < master->getNumNeighbour(); i++){
      socketToNeighbour = socketsInNetwork[master->getNeighbour(i)]->getSocket();
      Simulator::Schedule( m_interPacketInterval, &WirelessNetwork::sendSyncFollowPacket, this, master,
        socketToNeighbour, eventId );
    }
    eventId++;
  }

  void sendSyncFollowPacket(WirelessNode * txNode, Ptr<Socket> socket, int id ){
    std::cout << "sendSyncFollowPacket  id ->" << id << "  eventCounterIndex->" << eventCounterIndex << std::endl;
    if( id == eventCounterIndex ){
    std::stringstream msgx, msgy;

    // Sending the SYNC packet 
    
    msgx << txNode->getNodeId() << '#' << 0 << '#' << txNode->getNodeHop() << '#' << SYNC << '#' 
    << id << '#' << txNode->getDreqAtMaster().GetNanoSeconds() << '#' 
    << txNode->getSyncSendTime().GetNanoSeconds();
    
    Ptr<Packet> sync_pkt = Create<Packet>((uint8_t*) msgx.str().c_str(), m_packetSize);
    socket->Send ( sync_pkt );
    eventCounter[id]++;
    globalTime = NanoSeconds(Simulator::Now());       
    setLocalTimeAtNodes();
    txNode->setSyncSendTime( txNode->getLocalTime() );
    std::cout << "sending sync packet" << std::endl;
    txNode->incrementSentPacketCounter(SYNC);
    
    // Sending the FOLLOW_UP packet
    
    msgy <<  txNode->getNodeId() << '#' << 0 << '#' << txNode->getNodeHop() << '#' << FOLLOW << '#' 
    << id << '#' << txNode->getDreqAtMaster().GetNanoSeconds() << '#' 
    << txNode->getSyncSendTime().GetNanoSeconds() ;

    Ptr<Packet> follow_pkt = Create<Packet>((uint8_t*) msgy.str().c_str(), m_packetSize);
    socket->Send ( follow_pkt );  
    eventCounter[id]++;
    globalTime = NanoSeconds(Simulator::Now());       
    setLocalTimeAtNodes();
    txNode->incrementSentPacketCounter(FOLLOW);     
    }
  }

 Ptr<Packet> composeDreqPacket(WirelessNode * txNode, int id){
  std::stringstream msgx;
  int vectorSize;
  msgx << txNode->getNodeId() << '#' << 0 << '#' << txNode->getNodeHop() << '#' <<  DREQ << '#' 
    << id << '#' << txNode->getDreqAtMaster().GetNanoSeconds() << '#' 
    << txNode->getSyncSendTime().GetNanoSeconds() ;

  vectorSize = txNode->getTimeVectorSize();  
  for( int m=0; m < vectorSize; m++){
      msgx << '#' << txNode->getTimeStamp(m).GetNanoSeconds() ;
    }

  Ptr<Packet> dreq_pkt = Create<Packet>((uint8_t*) msgx.str().c_str(), m_packetSize);
  return dreq_pkt;
 }


 void sendDreqPacket( WirelessNode * txNode, Ptr<Socket> socket, int id){
    if( id == eventCounterIndex ){
    
    Ptr<Socket> socketToNeighbour;
    Ptr<Packet> dreq_pkt = composeDreqPacket( txNode, id );
    int numNeighbour = txNode->getNumNeighbour();
    txNode->incrementSentPacketCounter(DREQ);
    txNode->setState(ACTIVE);
    for( int j = 0 ; j < numNeighbour; j++ ){
            socketToNeighbour = socketsInNetwork[txNode->getNeighbour(j)]->getSocket();
            socketToNeighbour->Send( dreq_pkt );
            globalTime = NanoSeconds(Simulator::Now());
            eventCounter[id]++;
    }  

    setLocalTimeAtNodes();
    txNode->addTimeStamp( txNode->getLocalTime(), txNode->getNodeHop(), 2);  

    }else if( id > eventCounterIndex ){
     Simulator::Schedule ( NanoSeconds( (id - eventCounterIndex) * 100000000 ) , &WirelessNetwork::sendDreqPacket, this, txNode, socket, id );
    }
  } 


void sendDrplyPacket(WirelessNode * txNode, Ptr<Socket> socket, int id){
    if( id == eventCounterIndex ){
    Ptr<Socket> socketToNeighbour;
    std::stringstream msgx;
  
    // Sending the DRPLY packet 
    if( txNode->getNodeHop() > 0 ){

        msgx << txNode->getNodeId() << '#' << 0 << '#' << txNode->getNodeHop() << '#' << DRPLY << '#' 
        << id << '#' << txNode->getDreqAtMaster().GetNanoSeconds() << '#' 
        << txNode->getSyncSendTime().GetNanoSeconds();
        
        int vectorSize = txNode->getTimeVectorSize();
        for( int m=0; m < vectorSize; m++){
            
              msgx << '#' << txNode->getTimeStamp(m).GetNanoSeconds();
            
       }

      }else{
        msgx << txNode->getNodeId() << '#' << 0 << '#' << txNode->getNodeHop() << '#' << DRPLY << '#' 
        << id << '#' << txNode->getDreqAtMaster().GetNanoSeconds()  << '#' 
        << txNode->getSyncSendTime().GetNanoSeconds();
      }
      
      Ptr<Packet> drply_pkt = Create<Packet>((uint8_t*) msgx.str().c_str(), m_packetSize);
      for(int j = 0 ; j < txNode->getNumNeighbour(); j++){
          socketToNeighbour = socketsInNetwork[txNode->getNeighbour(j)]->getSocket();
          socketToNeighbour->Send(drply_pkt);    
          eventCounter[id]++; 
      }
      globalTime = NanoSeconds(Simulator::Now());       
      setLocalTimeAtNodes();
      txNode->incrementSentPacketCounter(DRPLY);
    }else if( id > eventCounterIndex ){
      Simulator::Schedule ( NanoSeconds( (id - eventCounterIndex) * 100000000 ), &WirelessNetwork::sendDrplyPacket , this, txNode, socket, id);
    }
  } 

  void receivePacket (Ptr<Socket> socket)
  { 
    globalTime = NanoSeconds(Simulator::Now()); // record time when pkt is received at socket
    nodes[masterIndex]->setLocalTime(globalTime);
    Time syncSendTime, dreqAtMaster;
    Ipv4Address senderIp, receiverIp, recvIp;
    uint16_t senderHop, myHop;
    uint16_t i = 0, nodeId, nodeIndex;
    int count = 0, MSG_TYPE, receiverId, myId, senderId, event_id, numNeighbour, k = 0;
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
    nodeId = socketsInNetwork[i]->getTxId();
    nodeIndex = nodeId - 1 ;
    
    // Get Pointer to the node
    recvNode = this->getNode( nodeIndex );
    setLocalTimeAtNodes();
    myHop = recvNode->getNodeHop();
    myId = recvNode->getNodeId();
    numNeighbour = recvNode->getNumNeighbour();

    std::vector<Time> recvTimeStamps;
    std::string msg_received(reinterpret_cast<char*>(buffer));
    std::stringstream msg_stream( msg_received );
    std::string item;
    
    // Read contents of the packet
    senderId = buffer[0] - '0';
    receiverId = buffer[2] - '0';
    senderHop = buffer[4] - '0';
    MSG_TYPE = buffer[6] - '0';
    
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

    senderIp = socketsInNetwork[i]->getRecvIp();
    receiverIp = socketsInNetwork[i]->getTxIp();

    while (getline(msg_stream, item, delim)){
        if( count >= 4 ){
        if( count >= 7 ){
          recvTimeStamps.push_back( NanoSeconds( stoll( item, nullptr, 10 ) ) );
        }else if( count == 5 ){
          dreqAtMaster = NanoSeconds( stoll( item, nullptr, 10 ) );
        }else if( count == 6 ){
          syncSendTime = NanoSeconds( stoll( item, nullptr, 10 ) );
        }else{
          event_id = stoi( item, nullptr, 10 );
        }
      }
      count++;
    }

    eventCounter[event_id]--;
    if( eventCounter[eventCounterIndex] == 0 ){
      eventCounterIndex++;
    }

    
      if( MSG_TYPE == DREQ ){

        if( senderHop < myHop ){
          // store the timestamps and wait for sometime and then send a DREQ pkt 
          recvNode->setSyncStartTime(globalTime);
          recvNode->copyTimeVector( dreqAtMaster, syncSendTime, recvTimeStamps );
          recvNode->addTimeStamp( recvNode->getLocalTime(), myHop, 0 );
          k = eventId - eventCounterIndex;
          k = k > 0 ? k : 1;
          Simulator::Schedule ( NanoSeconds( k * 1000000 ), &WirelessNetwork::sendDreqPacket, this, recvNode, socketToNeighbour, eventId );
          eventId++;
          printClockValuesOfNodes( senderIp, receiverIp, senderHop, msgType, dreqAtMaster, syncSendTime, event_id); 
        }else if( senderHop > myHop ){
            // record the timestamp of Dreq pkt and send a DRPLY containing that timestamp 
            if( myHop == 0 ){
              recvNode->setDreqAtMaster(recvNode->getLocalTime());
            }else{
              recvNode->addTimeStamp( recvNode->getLocalTime(), myHop, 1 );
            }
            k = eventId - eventCounterIndex;
            k = k > 0 ? k : 1;
            Simulator::Schedule( NanoSeconds( k * 1000000 ), &WirelessNetwork::sendDrplyPacket, this, recvNode, socket, eventId );
            eventId++;
            printClockValuesOfNodes( senderIp, receiverIp, senderHop, msgType, dreqAtMaster, syncSendTime, event_id); 
        }
        recvNode->incrementReceivedPacketCounter(MSG_TYPE);

      }else if( MSG_TYPE == DRPLY ){
        if( senderHop < myHop && recvNode->getState() == ACTIVE ){
          // rply from master, store the timstamp
          if( senderHop > 0 ){
            recvNode->copyTimeVector( dreqAtMaster, syncSendTime, recvTimeStamps );
          }else{
            recvNode->setDreqAtMaster(dreqAtMaster);
            recvNode->setSyncSendTime(syncSendTime);
          }
          recvNode->setSyncEndTime(globalTime);
          recvNode->setSynchronizationTime();
          recvNode->incrementReceivedPacketCounter(MSG_TYPE);
          recvNode->calculateOffset();
          recvNode->setOldOffsetError(this->getNode(masterIndex)->getLocalTime());
          recvNode->setNewOffsetError(this->getNode(masterIndex)->getLocalTime());
          recvNode->setState(SYNCED);
          printClockValuesOfNodes( senderIp, receiverIp, senderHop, msgType, dreqAtMaster, syncSendTime, event_id); 
        }else{
          recvNode->incrementOverheardPacketCounter(MSG_TYPE);
        }
    }else if( MSG_TYPE == SYNC ){
        // hop1 nodes
        // store SYNC receive time and wait for follow up
        recvNode->setSyncStartTime(globalTime);
        recvNode->incrementReceivedPacketCounter(MSG_TYPE);
        recvNode->addTimeStamp( recvNode->getLocalTime(), myHop, 0 );
        printClockValuesOfNodes( senderIp, receiverIp, senderHop, msgType, dreqAtMaster, syncSendTime, event_id); 

    }else if( MSG_TYPE == FOLLOW ){

        // store SYNC Send time contained in pkt and send DREQ 
        recvNode->incrementReceivedPacketCounter(MSG_TYPE);
        recvNode->setSyncSendTime(syncSendTime);
        Simulator::Schedule ( NanoSeconds( k * 10000000 ), &WirelessNetwork::sendDreqPacket, this, recvNode, socket , eventId);
        eventId++; 
        printClockValuesOfNodes( senderIp, receiverIp, senderHop, msgType, dreqAtMaster, syncSendTime, event_id); 
    }
  
 }

private:
  int eventId;
  int eventCounterIndex;
  std::vector< int > eventCounter; 
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


//-------------------------------------------------X--End of WirelessNetwork Class--X------------------------------------------



//-------------------------------------------------X--Start of Main function--X------------------------------------------

int main (int argc, char *argv[])
{
  
  std::string phyMode ("DsssRate1Mbps");
  double rss = -93;  // -dBm
  uint32_t packetSize = 1024; // bytes
  uint8_t interval = 5; // nanoseconds
  uint32_t users = 6; // Number of users
  

  CommandLine cmd;

  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("rss", "received signal strength", rss);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("users", "Number of receivers", users);

  cmd.Parse (argc, argv);

  // Convert to time object
  Time interPacketInterval = NanoSeconds (interval);
  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold",
    StringValue ("2200"));

  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
    StringValue ("2200"));

  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
    StringValue (phyMode));

  // Source and destination
  NodeContainer nodes;
  nodes.Create (users);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b); // OFDM at 2.4 GHz

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // The default error rate model is ns3::NistErrorRateModel

  // This is one parameter that matters when using FixedRssLossModel
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (0));

  // ns-3 supports RadioTap and Prism tracing extensions for 802.11g
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

  // The below FixedRssLossModel will cause the rss to be fixed regardless
  // of the distance between the two stations, and the transmit power
  wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",
    DoubleValue (rss));
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
    "DataMode",StringValue (phyMode), "ControlMode",StringValue (phyMode));

  // Set WiFi type and configuration parameters for MAC
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");

  // Create the net devices
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Note that with FixedRssLossModel, the positions below are not
  // used for received signal strength. However, they are required for the
  // YansWiFiChannelHelper
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc =
    CreateObject<ListPositionAllocator> ();

  for (uint32_t n = 1; n <= users; n++)
    {
      positionAlloc->Add (Vector (5.0, 5.0*n, 0.0));
    }

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4.Assign (devices);
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  char ipv4Add[6][12] = { "10.1.1.1", "10.1.1.2" , "10.1.1.3", "10.1.1.4", "10.1.1.5", "10.1.1.6"};
  uint16_t connectToPortAtNode[6] = { 100, 200, 300, 400, 500, 600 };
  uint16_t hopOfNode[6] = { 0, 1, 2, 3, 4, 5 };
  uint32_t masterOfNode[6] = { 1, 1, 2, 3, 4, 5 };
  int socketIndex[7];

  int neighbourList[6][3] = {
    { 2, -1},
    { 1, 3, -1},
    { 2, 4, -1},
    { 3, 5, -1},
    { 4, 6, -1},
    { 5, -1}
  };

  uint16_t neighbourPort[6][3] = {
    { 200 },
    { 100, 300 },
    { 201, 400 },
    { 301, 500 },
    { 401, 600 },
    { 501 }
  };

  std::vector< WirelessNode * > staticNodes(users);
  std::vector<Ipv4Address> ipv4Address( users );
  std::vector<int> neighbourNode;
  std::vector< std::vector < Ptr<Socket> > >  neighbour( users );
  std::vector< SocketPoint * > socketPoint(10);
  uint16_t i,j,k,l,count_Socket=0,myPort; 

  // create Ipv4 address 
  for( i=0; i < users;i++){
    ipv4Address[i] = Ipv4Address(ipv4Add[i]);
  }

  // pass adjacency list in vector
  for( k=0;k<3;k++){
    l=0;
    while( l < 2 && neighbourList[k][l] != -1 ){
      neighbourNode.push_back( neighbourList[k][l] );
      l++;
    }
    neighbourNode.push_back(-1);
  }

  WirelessNetwork ptpTest(users, neighbourNode, packetSize, interPacketInterval);
  
  socketIndex[0] = -1;
  for ( i = 0; i < users; i++)
    {
      j=0;
      while( neighbourList[i][j] != -1 && j < 3 ){
        neighbour[i].push_back(Socket::CreateSocket (nodes.Get(i), tid) );
        myPort = connectToPortAtNode[i] + j;
        neighbour[i][j]->Bind( InetSocketAddress( ipv4Address[ i ], myPort ) );
        neighbour[i][j]->Connect ( InetSocketAddress( ipv4Address[ neighbourList[i][j] - 1 ], neighbourPort[i][j] ) );
        neighbour[i][j]->SetRecvCallback (MakeCallback (&WirelessNetwork::receivePacket,
        &ptpTest));
        socketPoint[count_Socket] = new SocketPoint(i+1, neighbourList[i][j], ipv4Address[i],myPort,ipv4Address[ neighbourList[i][j] - 1 ], 
          neighbourPort[i][j], neighbour[i][j]);
        count_Socket++;
        j++;
      }
      staticNodes[i] = new WirelessNode( i+1, masterOfNode[i], hopOfNode[i], ipv4Address[i] );
      for( k = socketIndex[i]+1; k < count_Socket; k++ ){
        staticNodes[i]->addNeighbourIndex(k);
      }
      socketIndex[i+1] = count_Socket-1;
    }
    

  ptpTest.SetSocketIndex( socketIndex );
  ptpTest.SetSocketPoint( socketPoint );
  ptpTest.addNodesToNetwork( staticNodes );
  // Turn on global static routing so we can be routed across the network
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  // Pcap tracing
  wifiPhy.EnablePcap ("ptp-wifi-broadcast", devices);

  Simulator::ScheduleWithContext (neighbour[0][0]->GetNode ()->GetId (), Seconds (1.0),
    &WirelessNetwork::startProtocol, &ptpTest);

  AnimationInterface anim( "modified-ptp-test.xml");
  anim.SetConstantPosition( nodes.Get(0), 0.0, 15.0);
  anim.SetConstantPosition( nodes.Get(1), 4.0, 15.0);
  anim.SetConstantPosition( nodes.Get(2), 8.0, 15.0);
  anim.SetConstantPosition( nodes.Get(3), 12.0, 15.0);
  anim.SetConstantPosition( nodes.Get(4), 16.0, 15.0);
  anim.SetConstantPosition( nodes.Get(5), 20.0, 15.0);
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
