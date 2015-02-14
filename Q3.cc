/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-l4-protocol.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Question3");

void
CwndTracer(uint32_t oldval, uint32_t newval)
{
  NS_LOG_UNCOND ("Congestion window moving from " << oldval << " to " << newval << " at " << Simulator::Now().GetSeconds() << " seconds.");
}
void
Enqueue(std::string context, Ptr<const Packet> p)
{
  NS_LOG_INFO (context <<
                 " Packet Enqueued at " << Simulator::Now ().GetSeconds());
}
void
Dequeue(std::string context, Ptr<const Packet> p)
{
  NS_LOG_INFO (context <<
               " Packet Dequeued at " << Simulator::Now ().GetSeconds());
}
void
Drop(std::string context, Ptr<const Packet> p)
{
  NS_LOG_INFO (context <<
                 " Packet Dropped at " << Simulator::Now ().GetSeconds());
}

void
ReceivePacket (std::string context, Ptr<const Packet> p, const Address& addr)
{
  NS_LOG_INFO (context <<
                 " Packet Received at " << Simulator::Now ().GetSeconds() << "from " << InetSocketAddress::ConvertFrom(addr).GetIpv4 ());
}

int 
main (int argc, char *argv[])
{
  // disable fragmentation
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Set the size of the sending queue
  Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue(uint32_t(10)));
  //LogComponentEnable("Question3", LOG_LEVEL_INFO);
  
  uint32_t nFlows = 5;
  std::string tcpType = "NewReno";
  uint16_t port = 50000;
  CommandLine cmd;

  cmd.AddValue ("Flows", "Number of flows through two nodes", nFlows);
  cmd.AddValue ("Tcp", "Tcp type: 'NewReno', 'Tahoe', 'Reno', or 'Rfc793'", tcpType);
  cmd.Parse (argc, argv);
  
  if(tcpType != "NewReno" && tcpType != "Tahoe" && tcpType != "Reno" && tcpType != "Rfc793"){
    NS_LOG_UNCOND ("The Tcp type must be either 'NewReno', 'Tahoe', 'Reno', or 'Rfc793'.");
    return 1;
  }

  // Set default Socket type to one of the Tcp Sockets
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName ("ns3::Tcp" + tcpType)));

  NS_LOG_INFO ("Creating Topology");

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
  pointToPoint.SetChannelAttribute("Delay", TimeValue(Time("10ms")));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  
  ApplicationContainer serverApp[nFlows];
  ApplicationContainer sinkApp[nFlows];
  
  for(unsigned int i = 0;i < nFlows; i++){
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress (1), port + i));
    sinkApp[i] = packetSinkHelper.Install (nodes.Get (1));
    sinkApp[i].Start(Seconds (1.0));
    sinkApp[i].Stop(Seconds (60.0));
  }
  
  for(unsigned int i = 0; i < nFlows; i++){
    OnOffHelper server("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress (1), port + i));
    server.SetAttribute ("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=50]"));
    server.SetAttribute ("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    server.SetAttribute ("DataRate", DataRateValue (DataRate ("1.5Mbps")));
    server.SetAttribute ("PacketSize", UintegerValue (2000));
    
    serverApp[i] = server.Install (nodes.Get (0));
    serverApp[i].Start(Seconds (1 + (i * 5)));
    serverApp[i].Stop (Seconds (51.0 - (i * 5)));
  }

  NS_LOG_INFO ("Enable static global routing.");
  //
  // Turn on global static routing so we can actually be routed across the star.
  //
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("Question3-nflow.tr"));
  pointToPoint.EnablePcapAll("Question3");
  
  std::string context = "/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/";
  
  Config::Connect (context + "Enqueue", MakeCallback (&Enqueue));
  Config::Connect (context + "DeQueue", MakeCallback (&Dequeue));
  Config::Connect (context + "Drop", MakeCallback (&Drop));
  
  context = "/NodeList/1/ApplicationList/*/$ns3::PacketSink/Rx";
  Config::Connect (context, MakeCallback(&ReceivePacket));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
