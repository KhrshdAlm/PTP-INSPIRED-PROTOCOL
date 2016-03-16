
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include <cstdlib>
#include <iostream>
#include <cmath>

using namespace ns3;

class WirelessNode{
public:
	WirelessNode(
	const uint16_t id,
	const uint16_t hop,
	const Ipv4Address ipv4Address
	)
	: node_id(id),
	  hop_num(hop),
	  node_ipv4Address(ipv4Address)
	  {
		// Add some packet information
		pktSentCounter = 0;
		pktRecvCounter = 0;
		nodeState = 0; // 0 - inactive, 1 - active, 2 - waiting( to send Dreq), 3 - Sync
	  	error = ( rand() % 7 ) * 0.093 + 0.87;
	  	isMaster = 0;
	  	syncSendTime = NanoSeconds(0);
		dreqAtMaster = NanoSeconds(0);

	  	for( int j=0; j < 3 * hop; j++ ){
	  		timeStamps.push_back( NanoSeconds(0) );
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
	}

	// called once to set the local time
	void setIntialTime( Time initialtime ){
		localTime = NanoSeconds( 0 );
		simulatorTime = initialtime;
	}

	// called after happening of any event in the network
	void setLocalTime( Time currentSimulatorTime ){
		if( !isMaster ){
			localTime = NanoSeconds ( (currentSimulatorTime.GetNanoSeconds() - this->getSimulatorTime().GetNanoSeconds()) * error 
			                      + localTime.GetNanoSeconds() );
			this->setSimulatorTime( currentSimulatorTime );
		}else{
			localTime = simulatorTime = currentSimulatorTime;
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
		timeStamps[ index ] = NanoSeconds( t.GetNanoSeconds() );
		std::cout << "|    Added " << timeStamps[index] << " to timeVector at index " << index << std::endl;
	}

	void calculateOffset(){
		if( !isMaster ){
			Time temp = NanoSeconds(0);
			int i;
			if( hop_num > 1 ){
				temp = NanoSeconds( timeStamps[3*(hop_num-1)].GetNanoSeconds() + timeStamps[3*(hop_num-1)+2].GetNanoSeconds() );
				for( i=1;i< hop_num-1;i++){
					temp = temp + NanoSeconds( timeStamps[3*(i-1)].GetNanoSeconds() - timeStamps[3*(i-1)+1].GetNanoSeconds() );
				}
				temp = temp - NanoSeconds( syncSendTime.GetNanoSeconds() - dreqAtMaster.GetNanoSeconds() );
				offset = PicoSeconds( NanoSeconds( temp ) / 2 );

			}else{
				offset = PicoSeconds( NanoSeconds (((timeStamps[0].GetNanoSeconds() - syncSendTime.GetNanoSeconds()) + 
						 (dreqAtMaster.GetNanoSeconds() - timeStamps[2].GetNanoSeconds())) ) / 2 );
			}
		}
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

	void incrementPktSentCounter(){
		pktSentCounter++;
	}

	void incrementPktRecvCounter(){
		pktRecvCounter++;
	}

	int getPktRecvCounter(){
		return pktRecvCounter;
	}

	int getPktSentCounter(){
		return pktSentCounter;
	}

	int getTimeVectorSize(){
		return timeStamps.size();
	}

	Time getTimeStamp(int index){
		return timeStamps[index];
	}

	void setWaitingTime(){
		waitingTime = NanoSeconds( std::abs( timeStamps[0].GetNanoSeconds() - syncSendTime.GetNanoSeconds() ) );
	}

	Time getWaitingTime(){
		return waitingTime;
	} 
	

private:
	Time localTime;
	Time simulatorTime;
	Time syncSendTime;
	Time dreqAtMaster;
	Time offset;
	Time waitingTime;
	std::vector< Time > timeStamps;
	int pktSentCounter;
	int pktRecvCounter;
	int nodeState;
	double error;
	const uint32_t node_id;
	const uint32_t hop_num;
	uint32_t isMaster;
	const Ipv4Address node_ipv4Address;
	std::vector< int > neighbourIndex; // index to their sockets in the list storing all the sockets of networks
};