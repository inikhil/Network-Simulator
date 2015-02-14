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

#include <fstream>

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Question1");

int main (int argc, char *argv[])
{
  double lat = 2.0;
  uint64_t rate = 5000000; // Data rate in bps
  double interval = 0.05;

  CommandLine cmd;
  cmd.AddValue ("latency", "P2P link Latency in miliseconds", lat);
  cmd.AddValue ("rate", "P2P data rate in bps", rate);
  cmd.AddValue ("interval", "UDP client packet interval", interval);

  cmd.Parse (argc, argv);

//
// Enable logging for UdpClient and UdpServer
//
  //LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  //LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer n;
  n.Create (2);

  NS_LOG_INFO ("Create channels.");
//
// Explicitly create the channels required by the topology (shown above).
//
  PointToPointHelper p2p;
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (lat)));
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (rate)));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  NetDeviceContainer dev;
  dev = p2p.Install (n);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//

//
// Install Internet Stack
//
  InternetStackHelper internet;
  internet.Install (n);
  Ipv4AddressHelper ipv4;

  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (dev);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");
//
// Create one udpServer application on node one.
//
  uint16_t port1 = 8000; // Need different port numbers to ensure there is no conflict
  uint16_t port2 = 8001;
  UdpServerHelper server1 (port1);
  UdpServerHelper server2 (port2);
  ApplicationContainer servapps;
  servapps = server1.Install (n.Get (1));
  //apps = server2.Install (n.Get (1));

  servapps.Start (Seconds (1.0));
  servapps.Stop (Seconds (10.0));

//
// Create one UdpClient application to send UDP datagrams from node zero to
// node one.
//
  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (interval);
  uint32_t maxPacketCount = 320;
  ApplicationContainer cliapps;
  UdpClientHelper client1 (i.GetAddress (1), port1);
  //UdpClientHelper client2 (i2.GetAddress (1), port2);

  client1.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client1.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client1.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));

  //client2.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  //client2.SetAttribute ("Interval", TimeValue (interPacketInterval));
  //client2.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));

  cliapps = client1.Install (n.Get (0));
  //apps = client2.Install (n.Get (0));

  cliapps.Start (Seconds (2.0));
  cliapps.Stop (Seconds (10.0));

//
// Tracing
//
  AsciiTraceHelper ascii;
  p2p.EnableAscii(ascii.CreateFileStream ("Question1.tr"), dev);
  p2p.EnablePcap("Question1", dev, false);

//
// Calculate Throughput using Flowmonitor
//
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds(11.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if ((t.sourceAddress=="10.1.1.1" && t.destinationAddress == "10.1.1.2"))
      {
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
          double thpt = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ());
          thpt /= (1024 * 1024);
      	  std::cout << "  Throughput: " << thpt  << " Mbps\n";
      }
     }

  monitor->SerializeToXmlFile("Question1.flowmon", true, true);

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
