#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrabalhoRedesTCPUDP");

int main (int argc, char *argv[])
{
  NodeContainer serverNode;
  serverNode.Create(1);

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(32);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper();
  phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs7"),
                               "ControlMode", StringValue("HtMcs0"));

  WifiMacHelper mac;
  Ssid ssid = Ssid("EquipeX");

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  InternetStackHelper stack;
  stack.Install(serverNode);
  stack.Install(apNode);
  stack.Install(wifiStaNodes);

  Ptr<Ipv4> ipv4 = apNode.Get(0)->GetObject<Ipv4>();
  ipv4->SetAttribute("IpForward", BooleanValue(true));

  Ipv4AddressHelper address;
  address.SetBase("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer p2pDevices = p2p.Install(apNode.Get(0), serverNode.Get(0));

  Ipv4AddressHelper p2pAddress;
  p2pAddress.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces = p2pAddress.Assign(p2pDevices);

  MobilityHelper mobilityAp;
  Ptr<ListPositionAllocator> posAllocAp = CreateObject<ListPositionAllocator>();
  posAllocAp->Add(Vector(25.0, 25.0, 0.0));
  mobilityAp.SetPositionAllocator(posAllocAp);
  mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityAp.Install(apNode);

  MobilityHelper mobilitySta;
  mobilitySta.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobilitySta.Install(wifiStaNodes);

  double radius = 10.0;
  double speed = 2.0;
  double apX = 25.0, apY = 25.0;

  for (uint32_t i = 0; i < wifiStaNodes.GetN(); i++) {
    Ptr<Node> node = wifiStaNodes.Get(i);
    Ptr<ConstantVelocityMobilityModel> mob = node->GetObject<ConstantVelocityMobilityModel>();
    double angle = 2 * M_PI * i / wifiStaNodes.GetN(); 
    double x = apX + radius * std::cos(angle);
    double y = apY + radius * std::sin(angle);
    mob->SetPosition(Vector(x, y, 0.0));
    double dx = -std::sin(angle) * speed;
    double dy = std::cos(angle) * speed;
    mob->SetVelocity(Vector(dx, dy, 0.0));
  }

  uint16_t tcpPort = 5000;
  uint16_t udpPort = 6000;
  Address tcpServerAddress(InetSocketAddress(p2pInterfaces.GetAddress(1), tcpPort));
  Address udpServerAddress(InetSocketAddress(p2pInterfaces.GetAddress(1), udpPort));

  PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpServerAddress);
  PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", udpServerAddress);
  ApplicationContainer serverApps;
  serverApps.Add(tcpSinkHelper.Install(serverNode.Get(0)));
  serverApps.Add(udpSinkHelper.Install(serverNode.Get(0)));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(30.0));

  OnOffHelper tcpClient("ns3::TcpSocketFactory", tcpServerAddress);
  tcpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  tcpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  tcpClient.SetAttribute("DataRate", StringValue("50Mbps"));
  tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

  OnOffHelper udpClient("ns3::UdpSocketFactory", udpServerAddress);
  udpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  udpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  udpClient.SetAttribute("DataRate", StringValue("50Mbps"));
  udpClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < wifiStaNodes.GetN(); i++) {
    if (i < wifiStaNodes.GetN() / 2) {
      clientApps.Add(tcpClient.Install(wifiStaNodes.Get(i)));
    } else {
      clientApps.Add(udpClient.Install(wifiStaNodes.Get(i)));
    }
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(30.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

  Simulator::Stop(Seconds(40.0));
  Simulator::Run();

  flowMonitor->SerializeToXmlFile("UDP_TCP_mobility_32.xml", true, true);
  Simulator::Destroy();

  return 0;
}
