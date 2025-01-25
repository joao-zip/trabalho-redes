#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedScenarioWithMobility");

void RunScenario(uint32_t numClients) {
  // Servidor e AP
  NodeContainer serverNode, apNode;
  serverNode.Create(1);
  apNode.Create(1);

  // Clientes UDP e TCP
  uint32_t udpClients = numClients / 2;
  uint32_t tcpClients = numClients - udpClients;
  NodeContainer udpClientsNodes, tcpClientsNodes;
  udpClientsNodes.Create(udpClients);
  tcpClientsNodes.Create(tcpClients);

  // Configuração do Wi-Fi
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  WifiMacHelper mac;
  Ssid ssid = Ssid("MixedNetwork");

  // Configuração do AP
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

  // Configuração dos clientes
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer udpDevices = wifi.Install(phy, mac, udpClientsNodes);
  NetDeviceContainer tcpDevices = wifi.Install(phy, mac, tcpClientsNodes);

  // Pilha de Internet
  InternetStackHelper stack;
  stack.Install(serverNode);
  stack.Install(apNode);
  stack.Install(udpClientsNodes);
  stack.Install(tcpClientsNodes);

  // Atribuição de IPs
  Ipv4AddressHelper address;
  address.SetBase("192.168.0.0", "255.255.255.0");
  address.Assign(apDevices);
  address.Assign(udpDevices);
  address.Assign(tcpDevices);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer p2pDevices = p2p.Install(apNode.Get(0), serverNode.Get(0));
  Ipv4AddressHelper p2pAddress;
  p2pAddress.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces = p2pAddress.Assign(p2pDevices);

  // Mobilidade
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
  mobility.Install(udpClientsNodes);
  mobility.Install(tcpClientsNodes);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  // Aplicação UDP no servidor
  UdpEchoServerHelper udpServer(9);
  ApplicationContainer serverApp = udpServer.Install(serverNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Aplicação TCP no servidor
  uint16_t tcpPort = 8080;
  PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
  ApplicationContainer tcpServerApp = tcpSinkHelper.Install(serverNode.Get(0));
  tcpServerApp.Start(Seconds(1.0));
  tcpServerApp.Stop(Seconds(10.0));

  // Clientes UDP
  UdpEchoClientHelper udpClient(p2pInterfaces.GetAddress(1), 9);
  udpClient.SetAttribute("MaxPackets", UintegerValue(1));
  udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  udpClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer udpClientApps = udpClient.Install(udpClientsNodes);
  udpClientApps.Start(Seconds(2.0));
  udpClientApps.Stop(Seconds(10.0));

  // Clientes TCP
  OnOffHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(1), tcpPort));
  tcpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  tcpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  tcpClient.SetAttribute("DataRate", StringValue("5Mbps"));
  tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer tcpClientApps = tcpClient.Install(tcpClientsNodes);
  tcpClientApps.Start(Seconds(2.0));
  tcpClientApps.Stop(Seconds(10.0));

  // Roteamento
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Monitoramento
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  flowMonitor->SerializeToXmlFile("udp_tcp_mobility_simulation_results.xml", true, true);

  Simulator::Destroy();
}

int main(int argc, char *argv[]) {

  for (uint32_t numClients : {10, 20, 30}) {
    RunScenario(numClients);
  }

  return 0;
}
