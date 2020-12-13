#include "ns3/stats-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"

#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Problem 3");

class MyApp : public Application
{
public:
    MyApp();
    virtual ~MyApp();

    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
    void ChangeRate(DataRate newrate);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void ScheduleTx(void);
    void SendPacket(void);

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
};

MyApp::MyApp()
    : m_socket(0),
      m_peer(),
      m_packetSize(0),
      m_nPackets(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0)
{
}

MyApp::~MyApp()
{
    m_socket = 0;
}

void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
}

void MyApp::StartApplication(void)
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
}

void MyApp::StopApplication(void)
{
    m_running = false;

    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close();
    }
}

void MyApp::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx();
    }
}

void MyApp::ScheduleTx(void)
{
    if (m_running)
    {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
    }
}

void MyApp::ChangeRate(DataRate newrate)
{
    m_dataRate = newrate;
    return;
}

int main(int argc, char *argv[])
{

    uint32_t nCsma = 3;
    uint32_t nWifi = 2;

    CommandLine cmd;
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.Parse(argc, argv);
    // std::cout << rate << std::endl;
    NS_LOG_INFO("Create NOdes");

    NodeContainer csmaNodes;
    csmaNodes.Create(nCsma);
    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);


    NodeContainer wifiEdgeNodes;
    wifiEdgeNodes.Create (nWifi);
    NodeContainer wifiApNode = csmaNodes.Get(0);
    
    // Use YANS model
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");
    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("wireless-network");

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer edgeDevices;
    edgeDevices = wifi.Install(wifiPhy, wifiMac, wifiEdgeNodes);

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(wifiPhy, wifiMac, wifiApNode);

    ////////////////////////////////////
    // give mobility to wireless edge nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (5.0),
                                   "DeltaY", DoubleValue (10.0),
                                   "GridWidth", UintegerValue (2),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
    mobility.Install (wifiEdgeNodes);
    // Remote the mobility of AP node
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);
    //////////////////////////////////////

    // Install the routing protocol
    OlsrHelper olsr;
    Ipv4ListRoutingHelper list;
    list.Add (olsr, 10);
    InternetStackHelper stack;
    stack.SetRoutingHelper(list);
    stack.Install(csmaNodes);
    stack.Install(wifiEdgeNodes);
    // stack.Install(wifiApNode);

    Ipv4AddressHelper ipv4Addr;
    ipv4Addr.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer Icsma;
    Icsma = ipv4Addr.Assign(csmaDevices);
    
    ipv4Addr.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer Iap;
    Iap = ipv4Addr.Assign(apDevice);
    Ipv4InterfaceContainer Iedge;
    Iedge = ipv4Addr.Assign(edgeDevices);

    // Install TCP receiver on the AP and Node5
    uint16_t sinkPort = 8080;
    Address apAddress(InetSocketAddress(Iap.GetAddress(0), sinkPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(wifiApNode);

    Address node5Address(InetSocketAddress(Icsma.GetAddress(2), sinkPort));
    ApplicationContainer sinkApp2 = sinkHelper.Install(csmaNodes.Get(2));
    
    sinkApp.Start(Seconds(0.));
    sinkApp.Stop(Seconds(20.));
    sinkApp2.Start(Seconds(0.));
    sinkApp2.Stop(Seconds(20.));
    



    Ptr<Socket> tcpSocket1_3 = Socket::CreateSocket(wifiEdgeNodes.Get(0), TcpSocketFactory::GetTypeId());
    Ptr<MyApp> app = CreateObject<MyApp> ();
    app->Setup(tcpSocket1_3, apAddress, 1040, 100000, DataRate("1Mbps"));
    wifiEdgeNodes.Get(0) ->AddApplication(app);

    Ptr<Socket> tcpSocket2_5 = Socket::CreateSocket(wifiEdgeNodes.Get(1), TcpSocketFactory::GetTypeId());
    Ptr<MyApp> app2 = CreateObject<MyApp> ();
    app2->Setup(tcpSocket2_5, apAddress, 1040, 100000, DataRate("1Mbps"));
    wifiEdgeNodes.Get(1) ->AddApplication(app2);


    app->SetStartTime(Seconds(0.));
    app->SetStopTime(Seconds(20.));
    app2->SetStartTime(Seconds(0.));
    app2->SetStopTime(Seconds(20.));



    csma.EnablePcap("csma", csmaDevices.Get(1), true);
    wifiPhy.EnablePcapAll("wifi");


    Simulator::Stop(Seconds(20.));
    Simulator::Run();
    Simulator::Destroy();

    return 0;

}
