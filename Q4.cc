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

NS_LOG_COMPONENT_DEFINE ("Question4");

static const double timeToRun = 50.5;
static double node2BytesRcv = 0;
static double node3BytesRcv = 0;

void
ReceiveNode2Packet (std::string context, Ptr<const Packet> p, const Address& addr)
{
  NS_LOG_INFO (context <<
                 " Packet Received from Node 2 at " << Simulator::Now ().GetSeconds());
  node2BytesRcv += p->GetSize ();
}

void
ReceiveNode3Packet (std::string context, Ptr<const Packet> p, const Address& addr)
{
  NS_LOG_INFO (context <<
                 " Packet Received from Node 3 at " << Simulator::Now ().GetSeconds() << "from " << InetSocketAddress::ConvertFrom(addr).GetIpv4 ());
  node3BytesRcv += p->GetSize ();
}

void
ChangeDelay (Ptr<PointToPointRemoteChannel> & linkChannel)
{
  std::ostringstream new_delay;
  TimeValue val(Time("0ms"));
  
  linkChannel->GetAttribute("Delay", val);
  NS_LOG_INFO("The previous delay of the bottleneck link was " << val.Get().GetMilliSeconds() << "ms");
  
  new_delay << val.Get().GetMilliSeconds() + 1 << "ms";
  val.Set (Time (new_delay.str()));
  linkChannel->SetAttribute("Delay", val);
  linkChannel->GetAttribute("Delay", val);
  NS_LOG_INFO("The new delay of the bottleneck link is " << val.Get().GetMilliSeconds() << "ms");

  Simulator::Schedule (Seconds (1.0), &ChangeDelay, linkChannel);
}

int 
main (int argc, char *argv[])
{
  std::string tcpType = "NewReno";
  uint16_t port = 50000;
  CommandLine cmd;

  cmd.AddValue ("Tcp", "Tcp type: 'NewReno', 'Tahoe', 'Reno', or 'Rfc793'", tcpType);
  cmd.Parse (argc, argv);
  
  if(tcpType != "NewReno" && tcpType != "Tahoe" && tcpType != "Reno" && tcpType != "Rfc793"){
    NS_LOG_UNCOND ("The Tcp type must be either 'NewReno', 'Tahoe', 'Reno', or 'Rfc793'.");
    return 1;
  }
  
  // disable fragmentation
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Set tcp type
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName ("ns3::Tcp" + tcpType)));
  // Set maximum queue size
  Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue(uint32_t(10)));
  //LogComponentEnable("Question4", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Creating Topology");

  NodeContainer nodes;
  nodes.Create (4);
  NodeContainer n0n1 (nodes.Get (0), nodes.Get (1));
  NodeContainer n2n0 (nodes.Get (2), nodes.Get (0));
  NodeContainer n3n0 (nodes.Get (3), nodes.Get (0));

  PointToPointHelper link;
  link.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
  link.SetChannelAttribute("Delay", TimeValue(Time("10ms")));
  
  NetDeviceContainer d0d1 = link.Install (n0n1);
  NetDeviceContainer d2d0 = link.Install(n2n0);
  NetDeviceContainer d3d0 = link.Install(n3n0);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i0 = address.Assign (d2d0);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i0 = address.Assign (d3d0);
  
  ApplicationContainer apps;
  
  OnOffHelper source("ns3::TcpSocketFactory", InetSocketAddress(i0i1.GetAddress (1), port));
  source.SetAttribute ("OnTime", RandomVariableValue (ConstantVariable (50)));
  source.SetAttribute ("OffTime", RandomVariableValue (ConstantVariable (0)));
  source.SetAttribute ("DataRate", DataRateValue (DataRate ("1.5Mbps")));
  source.SetAttribute ("PacketSize", UintegerValue (2000));

  apps.Add (source.Install (nodes.Get (0)));
  
  source.SetAttribute ("Remote", AddressValue(InetSocketAddress(i0i1.GetAddress (1), port + 1)));
  apps.Add (source.Install (nodes.Get (3)));
  
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress(i0i1.GetAddress (1), port));
  apps.Add(sink.Install (nodes.Get (1)));
  
  sink.SetAttribute("Local", AddressValue(InetSocketAddress(i0i1.GetAddress(1), port + 1)));
  apps.Add (sink.Install (nodes.Get (1)));
  
  apps.Start(Seconds (0.5));
  apps.Stop(Seconds (timeToRun));

  NS_LOG_INFO ("Enable static global routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  Ptr<PointToPointRemoteChannel> linkChannel ((PointToPointRemoteChannel*) &(*(nodes.Get(3)->GetDevice (0)->GetChannel ())));
  Simulator::Schedule (Seconds (1.5), &ChangeDelay, linkChannel);
  
  std::string context = "/NodeList/1/ApplicationList/0/$ns3::PacketSink/Rx";
  Config::Connect (context, MakeCallback(&ReceiveNode2Packet));
  
  context = "/NodeList/1/ApplicationList/1/$ns3::PacketSink/Rx";
  Config::Connect (context, MakeCallback(&ReceiveNode3Packet));
  
  AsciiTraceHelper ascii;
  link.EnableAsciiAll (ascii.CreateFileStream ("Question4.tr"));
  link.EnablePcapAll ("Question4");

  Simulator::Stop(Seconds(timeToRun));
  Simulator::Run ();
  Simulator::Destroy ();
  
  std::cout << "Throughput from Node 2: " << (node2BytesRcv * 8 / 1000000) / (timeToRun - 0.5) << " Mbps" << std::endl;
  std::cout << "Throughput from Node 3: " << (node3BytesRcv * 8 / 1000000) / (timeToRun - 0.5) << " Mbps" << std::endl;
  
  return 0;
}
