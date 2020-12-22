#include <iomanip>
#include <iostream>
#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("problem 1");

std::ofstream cwndStream;
std::ofstream cwndStream2;
std::ofstream pacingRateStream;
std::ofstream ssThreshStream;
std::ofstream packetTraceStream;

static void
CwndTracer (uint32_t oldval, uint32_t newval)
{
  cwndStream << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << std::setw (12) << newval << std::endl;
}
static void
CwndTracer2 (uint32_t oldval, uint32_t newval)
{
  cwndStream2 << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << std::setw (12) << newval << std::endl;
}

static void
PacingRateTracer (DataRate oldval, DataRate newval)
{
  pacingRateStream << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << std::setw (12) << newval.GetBitRate () / 1e6 << std::endl;
}

static void
SsThreshTracer (uint32_t oldval, uint32_t newval)
{
  ssThreshStream << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << std::setw (12) << newval << std::endl;
}

static void
TxTracer (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface)
{
  packetTraceStream << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << " tx " << p->GetSize () << std::endl;
}

static void
RxTracer (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface)
{
  packetTraceStream << std::fixed << std::setprecision (6) << Simulator::Now ().GetSeconds () << " rx " << p->GetSize () << std::endl;
}

void
ConnectSocketTraces (void)
{
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer2));
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/PacingRate", MakeCallback (&PacingRateTracer));
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold", MakeCallback (&SsThreshTracer));
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::Ipv4L3Protocol/Tx", MakeCallback (&TxTracer));
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::Ipv4L3Protocol/Rx", MakeCallback (&RxTracer));
}

int
main (int argc, char *argv[])
{
  bool tracing = false;

  uint32_t maxBytes = 0; // value of zero corresponds to unlimited send

  Time simulationEndTime = Seconds (30);
  DataRate bottleneckBandwidth ("10Mbps");
  Time bottleneckDelay = MilliSeconds (40);
  DataRate regLinkBandwidth = DataRate (4 * bottleneckBandwidth.GetBitRate ());
  Time regLinkDelay = MilliSeconds (5);
  DataRate maxPacingRate ("4Gbps");

  bool isPacingEnabled = false;
  bool useEcn = true;
  bool useQueueDisc = true;
  bool shouldPaceInitialWindow = false;

  // Configure defaults that are not based on explicit command-line arguments
  // They may be overridden by general attribute configuration of command line
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName ("ns3::TcpNewReno")));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));

  CommandLine cmd (__FILE__);
  cmd.AddValue ("tracing", "Flag to enable/disable Ascii and Pcap tracing", tracing);
  cmd.AddValue ("maxBytes", "Total number of bytes for application to send", maxBytes);
  cmd.AddValue ("isPacingEnabled", "Flag to enable/disable pacing in TCP", isPacingEnabled);
  cmd.AddValue ("maxPacingRate", "Max Pacing Rate", maxPacingRate);
  cmd.AddValue ("useEcn", "Flag to enable/disable ECN", useEcn);
  cmd.AddValue ("useQueueDisc", "Flag to enable/disable queue disc on bottleneck", useQueueDisc);
  cmd.AddValue ("shouldPaceInitialWindow", "Flag to enable/disable pacing of TCP initial window", shouldPaceInitialWindow);
  cmd.Parse (argc, argv);

  // Configure defaults based on command-line arguments
  Config::SetDefault ("ns3::TcpSocketState::EnablePacing", BooleanValue (isPacingEnabled));
  Config::SetDefault ("ns3::TcpSocketState::PaceInitialWindow", BooleanValue (shouldPaceInitialWindow));
  Config::SetDefault ("ns3::TcpSocketBase::UseEcn", (useEcn ? EnumValue (TcpSocketState::On) : EnumValue (TcpSocketState::Off)));
  Config::SetDefault ("ns3::TcpSocketState::MaxPacingRate", DataRateValue (maxPacingRate));

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (6);

  NS_LOG_INFO ("Create channels.");
  NodeContainer n1n5 = NodeContainer (nodes.Get (0), nodes.Get (2));
  NodeContainer n2n5 = NodeContainer (nodes.Get (1), nodes.Get (2));

  NodeContainer n5n6 = NodeContainer (nodes.Get (2), nodes.Get (3));

  NodeContainer n6n3 = NodeContainer (nodes.Get (3), nodes.Get (4));
  NodeContainer n6n4 = NodeContainer (nodes.Get (3), nodes.Get (5));

  //Define Node link properties
  PointToPointHelper leftAccessLink;
  leftAccessLink.SetDeviceAttribute ("DataRate", DataRateValue (regLinkBandwidth));
  leftAccessLink.SetChannelAttribute ("Delay", TimeValue (regLinkDelay));
  leftAccessLink.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));
  NetDeviceContainer d1d5 = leftAccessLink.Install (n1n5);
  NetDeviceContainer d2d5 = leftAccessLink.Install (n2n5);

  PointToPointHelper rightAccessLink;
  rightAccessLink.SetDeviceAttribute ("DataRate", DataRateValue (regLinkBandwidth));
  rightAccessLink.SetChannelAttribute ("Delay", TimeValue (regLinkDelay));

  NetDeviceContainer d6d3 = rightAccessLink.Install (n6n3);
  NetDeviceContainer d6d4 = rightAccessLink.Install (n6n4);

  PointToPointHelper bottleNeckLink;
  bottleNeckLink.SetDeviceAttribute ("DataRate", DataRateValue (bottleneckBandwidth));
  bottleNeckLink.SetChannelAttribute ("Delay", TimeValue (bottleneckDelay));
  bottleNeckLink.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("50p"));

  NetDeviceContainer d5d6 = bottleNeckLink.Install (n5n6);

  //Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Install traffic control
  if (useQueueDisc)
    {
      TrafficControlHelper tchQ;
      tchQ.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
      tchQ.Install (d1d5);
      tchQ.Install (d2d5);
      tchQ.Install (d5d6);
    }

  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer regLinkInterface0 = ipv4.Assign (d1d5);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer regLinkInterface1 = ipv4.Assign (d2d5);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer bottleneckInterface = ipv4.Assign (d5d6);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer regLinkInterface4 = ipv4.Assign (d6d3);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer regLinkInterface5 = ipv4.Assign (d6d4);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");

  // Two Sink Applications at n4 and n5
  uint16_t sinkPort = 8080;
  Address sinkAddress3 (InetSocketAddress (regLinkInterface4.GetAddress (1), sinkPort)); // interface of n3
  Address sinkAddress4 (InetSocketAddress (regLinkInterface5.GetAddress (1), sinkPort)); // interface of n4
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps3 = packetSinkHelper.Install (nodes.Get (4)); //n3 as sink
  ApplicationContainer sinkApps4 = packetSinkHelper.Install (nodes.Get (5)); //n4 as sink

  sinkApps3.Start (Seconds (0));
  sinkApps3.Stop (simulationEndTime);
  sinkApps4.Start (Seconds (0));
  sinkApps4.Stop (simulationEndTime);

  // Randomize the start time between 0 and 1ms
  Ptr<UniformRandomVariable> uniformRv = CreateObject<UniformRandomVariable> ();
  uniformRv->SetStream (0);

  // Two Source Applications at n1 and n2
  BulkSendHelper source13 ("ns3::TcpSocketFactory", sinkAddress3);
  BulkSendHelper source23 ("ns3::TcpSocketFactory", sinkAddress3);
  BulkSendHelper source24 ("ns3::TcpSocketFactory", sinkAddress4);
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source13.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  source23.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  source24.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApps13 = source13.Install (nodes.Get (0));
  ApplicationContainer sourceApps23 = source23.Install (nodes.Get (1));
  ApplicationContainer sourceApps24 = source24.Install (nodes.Get (1));

  sourceApps13.Start (MicroSeconds (uniformRv->GetInteger (0, 1000)));
  sourceApps13.Stop (simulationEndTime);
  sourceApps24.Start (Seconds (15));
  sourceApps24.Stop (simulationEndTime);
  sourceApps24.Start (Seconds (15));
  sourceApps24.Stop (simulationEndTime);

  if (tracing)
    {
      AsciiTraceHelper ascii;
      leftAccessLink.EnableAsciiAll (ascii.CreateFileStream ("tcp-dynamic-pacing.tr"));
      leftAccessLink.EnablePcapAll ("tcp-dynamic-pacing", false);
    }

  cwndStream.open ("tcp-dynamic-pacing-cwnd.dat", std::ios::out);
  cwndStream << "#Time(s) Congestion Window (B)" << std::endl;
  cwndStream2.open ("tcp-dynamic-pacing-cwnd2.dat", std::ios::out);
  cwndStream2 << "#Time(s) Congestion Window (B)" << std::endl;


  pacingRateStream.open ("tcp-dynamic-pacing-pacing-rate.dat", std::ios::out);
  pacingRateStream << "#Time(s) Pacing Rate (Mb/s)" << std::endl;

  ssThreshStream.open ("tcp-dynamic-pacing-ssthresh.dat", std::ios::out);
  ssThreshStream << "#Time(s) Slow Start threshold (B)" << std::endl;

  packetTraceStream.open ("tcp-dynamic-pacing-packet-trace.dat", std::ios::out);
  packetTraceStream << "#Time(s) tx/rx size (B)" << std::endl;

  Simulator::Schedule (MicroSeconds (1001), &ConnectSocketTraces);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();


  leftAccessLink.EnablePcap("left-side", d1d5);

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (simulationEndTime);
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / simulationEndTime.GetSeconds () / 1000000  << " Mbps\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simulationEndTime.GetSeconds () / 1000000  << " Mbps\n";
    }


  cwndStream.close ();
  cwndStream2.close ();
  pacingRateStream.close ();
  ssThreshStream.close ();
  Simulator::Destroy ();
}
