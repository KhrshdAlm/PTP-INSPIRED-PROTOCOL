// Simulation includes
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <ctime>
#include "ns3/netanim-module.h"
#include "network.h" // Contains the broadcast topology class

using namespace ns3;

int main (int argc, char *argv[])
{
  //! [4]
  std::string phyMode ("DsssRate1Mbps");
  double rss = -93;  // -dBm
  uint32_t packetSize = 1024; // bytes
  uint8_t interval = 5; // nanoseconds
  uint32_t num_packets = 50;
  uint32_t users = 6; // Number of users
  

  CommandLine cmd;

  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("rss", "received signal strength", rss);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("num_packets", "number of packets",
                num_packets);
  cmd.AddValue ("users", "Number of receivers", users);

  cmd.Parse (argc, argv);

  // Convert to time object
  Time interPacketInterval = NanoSeconds(interval);
  
  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold",
    StringValue ("2200"));

  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
    StringValue ("2200"));

  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
    StringValue (phyMode));
  //! [6]
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
  //! [7]
  // Add a non-QoS upper mac, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
    "DataMode",StringValue (phyMode), "ControlMode",StringValue (phyMode));

  // Set WiFi type and configuration parameters for MAC
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");

  // Create the net devices
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);
  //! [8]
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
  //! [9]
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4.Assign (devices);
  //! [10]
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  // List of Ipv4 Address

  //int hopOfNode[ 3 ] ={ 0, 1, 1 };
  char ipv4Add[6][12] = { "10.1.1.1", "10.1.1.2" , "10.1.1.3",
                          "10.1.1.4", "10.1.1.5" , "10.1.1.6" };

  uint16_t connectToPortAtNode[6] = { 100, 200, 300, 400, 500, 600};
  uint16_t hopOfNode[6] = { 0, 1, 1, 2, 2, 2};
  int socketIndex[7];

  int neighbourList[6][3] = {
    { 2, 3, -1},
    { 1, 4, 5 },
    { 1, 6 , -1},
    { 2, -1},
    { 2, -1},
    { 3 , -1}
  };

  uint16_t neighbourPort[6][3] = {
    { 200, 300 },
    { 100, 400, 500},
    { 101, 600},
    { 201 },
    { 202 },
    { 301 }
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
        socketPoint[count_Socket] = new SocketPoint(i+1,ipv4Address[i],myPort,ipv4Address[ neighbourList[i][j] - 1 ], 
          neighbourPort[i][j], neighbour[i][j]);
        std::cout<< "Connecting "<<ipv4Address[i]<<" , "<<myPort<<" to "<<ipv4Address[ neighbourList[i][j] - 1 ]<<" , "<<neighbourPort[i][j]<<std::endl;
        count_Socket++;
        j++;
      }
      staticNodes[i] = new WirelessNode( i+1, hopOfNode[i], ipv4Address[i] );
      for( k = socketIndex[i]+1; k < count_Socket; k++){
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

  AnimationInterface anim( "my-wifi-broadcast.xml");
  anim.SetConstantPosition( nodes.Get(0), 0.0, 4.0);
  anim.SetConstantPosition( nodes.Get(1), -2.0, 0.0);
  anim.SetConstantPosition( nodes.Get(2), -4.0, 4.0);
  anim.SetConstantPosition( nodes.Get(3), 1.0, 4.0);
  anim.SetConstantPosition( nodes.Get(4), -4.0, 0.0);
  anim.SetConstantPosition( nodes.Get(5), -6.0, 4.0);

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
  //! [14]
}
