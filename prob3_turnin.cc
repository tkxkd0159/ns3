#include "ns3/stats-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"

#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

NS_LOG_COMPONENT_DEFINE ("wifi-tcp");

using namespace ns3;

// Ptr<PacketSink> sink;                         /* Pointer to the packet sink application */
// uint64_t lastTotalRx = 0;                     /* The value of the last total received bytes */

// void
// CalculateThroughput ()
// {
//   Time now = Simulator::Now ();                                         /* Return the simulator's virtual time. */
//   double cur = (sink->GetTotalRx () - lastTotalRx) * (double) 8 / 1e6;     /* Convert Application RX Packets to MBits. */
//   std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
//   lastTotalRx = sink->GetTotalRx ();
//   Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
// }

int
main (int argc, char *argv[])
{
  uint32_t payloadSize = 1472;                       /* Transport layer payload size in bytes. */
  std::string dataRate = "1Mbps";                  /* Application layer datarate. */
  std::string csmaDataRate = "10Mbps";                  /* Application layer datarate. */
  std::string phyRate = "HtMcs7";                    /* Physical layer bitrate. */
  double simulationTime = 10;                        /* Simulation time in seconds. */
  bool pcapTracing = true;                          /* PCAP Tracing is enabled or not. */

  /* Command line argument parser setup. */
  CommandLine cmd (__FILE__);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Application data ate", dataRate);
  cmd.AddValue ("phyRate", "Physical layer bitrate", phyRate);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcap", "Enable/disable PCAP Tracing", pcapTracing);
  cmd.Parse (argc, argv);



  /* Configure TCP Options */
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

  /* Set up Legacy Channel */
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (5e9));

  /* Setup Physical Layer */
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");
  wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue (phyRate),
                                      "ControlMode", StringValue ("HtMcs0"));
  NodeContainer staWifiNodes;
  staWifiNodes.Create (2);
  NodeContainer apWifiNode;
  apWifiNode.Create(1);
  NodeContainer csmaNodes;
  csmaNodes.Add(apWifiNode);
  csmaNodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue(DataRate(csmaDataRate)));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);





  /* Configure AP */
  Ssid ssid = Ssid ("ljs");
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifiHelper.Install (wifiPhy, wifiMac, apWifiNode);

  /* Configure STA */
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer staDevices;
  staDevices = wifiHelper.Install (wifiPhy, wifiMac, staWifiNodes);

  /* Mobility model */

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator", "MinX", DoubleValue (0.0), "MinY",
                                 DoubleValue (0.0), "DeltaX", DoubleValue (5.0), "DeltaY",
                                 DoubleValue (10.0), "GridWidth", UintegerValue (2), "LayoutType",
                                 StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", "Bounds",
                             RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (staWifiNodes);
  // assign constant position to AP node
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apWifiNode);

  /* Internet stack */
  InternetStackHelper stack;
  stack.Install (staWifiNodes);
  stack.Install (csmaNodes);

  OlsrHelper olsr;
  Ipv4ListRoutingHelper routeList;
  routeList.Add(olsr, 5);
  stack.SetRoutingHelper(routeList);
  /* Populate routing table */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ipv4AddressHelper address;
  address.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface;
  staInterface = address.Assign (staDevices);

  address.SetBase("192.100.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterface;
  csmaInterface = address.Assign(csmaDevices);



  /* Install TCP Receiver on the AP and node node5 */
  uint16_t sinkPort = 8080;
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = sinkHelper.Install (apWifiNode);
  ApplicationContainer sinkApp2 = sinkHelper.Install (csmaNodes.Get(2));

  /* Install TCP/UDP Transmitter on the station */
  OnOffHelper server ("ns3::TcpSocketFactory", (InetSocketAddress (apInterface.GetAddress (0), sinkPort)));
  server.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));

  OnOffHelper server2 ("ns3::TcpSocketFactory", (InetSocketAddress (csmaInterface.GetAddress (2), sinkPort)));
  server2.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  server2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server2.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));

  ApplicationContainer sendApp13 = server.Install (staWifiNodes.Get(0));
  ApplicationContainer sendApp25 = server2.Install (staWifiNodes.Get(1));



  /* Start Applications */
  sinkApp.Start (Seconds (0.0));
  sinkApp2.Start (Seconds (0.0));
  sendApp13.Start (Seconds (1.0));
  sendApp25.Start (Seconds (1.0));
  // Simulator::Schedule (Seconds (1.1), &CalculateThroughput);

  /* Enable Traces */
  if (pcapTracing)
    {
      wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11);
      wifiPhy.EnablePcap ("AccessPoint", apDevice);
      wifiPhy.EnablePcap ("Station", staDevices);
    //   csma.EnablePcap ("node5", csmaDevices, true);
    }

  /* Start Simulation */
  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();


  Simulator::Destroy ();

  return 0;
}
