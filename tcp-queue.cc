//       n1 ---+      +--- n3
//             |      |
//             n5 -- n6
//             |      |
//       n2 ---+      +--- n4
//
// - All links are P2P with 500kb/s and 2ms
// - TCP flow form n1 to n3
// - TCP flow from n2 to n3
// - TCP flow from n2 to n4

#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Problem 1");

class MyApp : public Application
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
  void ChangeRate(DataRate newrate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
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

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
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

void
MyApp::ChangeRate(DataRate newrate)
{
   m_dataRate = newrate;
   return;
}

static void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << Simulator::Now ().GetSeconds () << "\t" << newCwnd <<"\n";
}


int main (int argc, char *argv[])
{
  std::string lat = "2ms";
  std::string rate = "500kb/s"; // P2P link
  bool enableFlowMonitor = false;


  CommandLine cmd;
  cmd.AddValue ("latency", "P2P link Latency in miliseconds", lat);
  cmd.AddValue ("rate", "P2P data rate in bps", rate);
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);

  cmd.Parse (argc, argv);

// Make Topology
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodeGroup; // ALL Nodes
  nodeGroup.Create(6);

  NodeContainer n1n5 = NodeContainer (nodeGroup.Get (0), nodeGroup.Get (4));
  NodeContainer n2n5 = NodeContainer (nodeGroup.Get (1), nodeGroup.Get (4));
  NodeContainer n3n6 = NodeContainer (nodeGroup.Get (2), nodeGroup.Get (5));
  NodeContainer n4n6 = NodeContainer (nodeGroup.Get (3), nodeGroup.Get (5));
  NodeContainer n5n6 = NodeContainer (nodeGroup.Get (4), nodeGroup.Get (5));



// Install Internet Stack
  InternetStackHelper internet;
  internet.Install (nodeGroup);

// Create the channels first without any IP addressing information
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2p.SetChannelAttribute ("Delay", StringValue (lat));
  NetDeviceContainer d1d5 = p2p.Install (n1n5);
  NetDeviceContainer d2d5 = p2p.Install (n2n5);
  NetDeviceContainer d5d6 = p2p.Install (n5n6);
  NetDeviceContainer d3d6 = p2p.Install (n3n6);
  NetDeviceContainer d4d6 = p2p.Install (n4n6);

// Assign IP addresses
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i5 = ipv4.Assign (d1d5);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i5 = ipv4.Assign (d2d5);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i5i6 = ipv4.Assign (d5d6);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i6 = ipv4.Assign (d3d6);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i6 = ipv4.Assign (d4d6);

  NS_LOG_INFO ("Enable static global routing.");


// Turn on global static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  NS_LOG_INFO ("Create Applications.");

//////////////////////////////////////
  // TCP connfection from n1 to n3
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i3i6.GetAddress (0), sinkPort)); // interface of n3
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodeGroup.Get (2)); //n3 as sink
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (100.));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodeGroup.Get (0), TcpSocketFactory::GetTypeId ()); //source at n1
  // Trace Congestion window
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  // Create TCP application at n1
  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkAddress, 1040, 100000, DataRate ("250Kbps"));
  nodeGroup.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (1.));
  app->SetStopTime (Seconds (100.));
////////////////////////////////////////
///////////////////////////////////////
  // TCP connfection from n2 to n3
  // sinkAddress and port are same such as case 1

  Ptr<Socket> ns3TcpSocket2 = Socket::CreateSocket (nodeGroup.Get (1), TcpSocketFactory::GetTypeId ()); //source at n2

  // Trace Congestion window
  ns3TcpSocket2->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  // Create TCP application at n2
  Ptr<MyApp> app2 = CreateObject<MyApp> ();
  app2->Setup (ns3TcpSocket2, sinkAddress, 1040, 100000, DataRate ("250Kbps"));
  nodeGroup.Get (1)->AddApplication (app2);
  app2->SetStartTime (Seconds (15.));
  app2->SetStopTime (Seconds (100.));
////////////////////////////////////////////
///////////////////////////////////////
  // TCP connfection from n2 to n4

  Address sinkAddress2 (InetSocketAddress (i4i6.GetAddress (0), sinkPort)); // interface of n4
  ApplicationContainer sinkApps2 = packetSinkHelper.Install (nodeGroup.Get (3)); //n4 as sink
  sinkApps2.Start (Seconds (0.));
  sinkApps2.Stop (Seconds (100.));

  Ptr<Socket> ns3TcpSocket3 = Socket::CreateSocket (nodeGroup.Get (1), TcpSocketFactory::GetTypeId ()); //source at n2

  // Trace Congestion window
  ns3TcpSocket3->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  // Create TCP application at n2
  Ptr<MyApp> app3 = CreateObject<MyApp> ();
  app3->Setup (ns3TcpSocket3, sinkAddress2, 1040, 100000, DataRate ("250Kbps"));
  nodeGroup.Get (1)->AddApplication (app3);
  app3->SetStartTime (Seconds (15.));
  app3->SetStopTime (Seconds (100.));
////////////////////////////////////////////




  // Flow Monitor
  Ptr<FlowMonitor> flowmon;
  if (enableFlowMonitor)
    {
      FlowMonitorHelper flowmonHelper;
      flowmon = flowmonHelper.InstallAll ();
    }

//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds(100.0));
  Simulator::Run ();
  if (enableFlowMonitor)
    {
	  flowmon->CheckForLostPackets ();
	  flowmon->SerializeToXmlFile("lab-2.flowmon", true, true);
    }
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}