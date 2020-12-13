// Network topology
//
//
//        n1 --- n2 --- n3
//
//
// - All links are wireless IEEE 802.11b with OLSR (newwork layer)
// - UDP connection from node1 to node3
// - n2 moving

#include "ns3/stats-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"


NS_LOG_COMPONENT_DEFINE ("Problem 2");

using namespace ns3;

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets,
              DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
};

MyApp::MyApp ()
    : m_socket (0),
      m_peer (),
      m_packetSize (0),
      m_nPackets (0),
      m_dataRate (0),
      m_sendEvent (),
      m_running (false),
      m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets,
              DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

static void
SetPosition (Ptr<Node> node, double x)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  Vector pos = mobility->GetPosition ();
  pos.x = x;
  mobility->SetPosition (pos);
}

void
ReceivePacket (Ptr<const Packet> p, const Address &addr)
{
  std::cout << Simulator::Now ().GetSeconds () << "\t" << p->GetSize () << "\n";
}

int
main (int argc, char *argv[])
{

  std::string phyMode ("DsssRate1Mbps");

  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.Parse (argc, argv);

  //
  // Explicitly create the nodes required by the topology (shown above).
  //
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodeGroup; // ALL Nodes
  nodeGroup.Create (3);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel", "SystemLoss",
                                  DoubleValue (1), "HeightAboveZ", DoubleValue (1.5));

  wifiPhy.Set ("TxGain", DoubleValue (1));
  wifiPhy.Set ("RxGain", DoubleValue (1));

  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  // Set up WiFi
  WifiHelper wifi;
  // Set 802.11b standard
  wifi.SetStandard (WIFI_STANDARD_80211b);

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));

  NetDeviceContainer devices;
  devices = wifi.Install (wifiPhy, wifiMac, nodeGroup);

  //  Enable OLSR
  OlsrHelper olsr;

  // Install the routing protocol
  Ipv4ListRoutingHelper list;
  list.Add (olsr, 10);

  // Set up internet stack
  InternetStackHelper internet;
  internet.SetRoutingHelper (list);
  internet.Install (nodeGroup);

  // Set up Addresses
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifcont = ipv4.Assign (devices);

  NS_LOG_INFO ("Create Applications.");

  // UDP connfection from node 1 to node 3

  uint16_t sinkPort = 6;
  Address sinkAddress (InetSocketAddress (ifcont.GetAddress (2), sinkPort)); // interface of node 3
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodeGroup.Get (2)); //node 3 as sink
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (60.));

  Ptr<Socket> ns3UdpSocket =
      Socket::CreateSocket (nodeGroup.Get (0), UdpSocketFactory::GetTypeId ()); //source at node 1

  // Create UDP application at node 1
  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3UdpSocket, sinkAddress, 1040, 100000, DataRate ("250Kbps"));
  nodeGroup.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (1.));
  app->SetStopTime (Seconds (60.));

  // Set Mobility for all nodes

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0, 0, 0)); // node1
  positionAlloc->Add (Vector (1000, 0, 0)); // node2
  positionAlloc->Add (Vector (500, 0, 0)); // node3
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodeGroup);

  // node 2 comes in the communication range of both
  Simulator::Schedule (Seconds (20.0), &SetPosition, nodeGroup.Get (1), 100.0);

  // node 2 goes out of the communication range of both
  Simulator::Schedule (Seconds (35.0), &SetPosition, nodeGroup.Get (1), 1000.0);

  // Trace Received Packets
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                                 MakeCallback (&ReceivePacket));

  // Trace devices (pcap)
  wifiPhy.EnablePcap ("prob-2-new", devices, true);

  std::string probeType;
  std::string tracePath;
  probeType = "ns3::ApplicationPacketProbe";
  tracePath = "/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx";
  GnuplotHelper plotHelper;
  plotHelper.ConfigurePlot ("olsr-manet", "the number of bytes received versus time at Node 3",
                            "Time (Seconds", "the number of bytes");
  plotHelper.PlotProbe (probeType, tracePath, "OutputBytes", "Packet Byte Count",
                        GnuplotAggregator::KEY_BELOW);

  FileHelper fileHelper;
  fileHelper.ConfigureFile("olsr-manet", FileAggregator::FORMATTED);
  fileHelper.Set2dFormat ("Time (Seconds) = %.3e\tPacket Byte Count = %.0f");
  fileHelper.WriteProbe (probeType,
                         tracePath,
                         "OutputBytes");


  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (60.0));
  Simulator::Run ();

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}