#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrabalhoRedes");

int main (int argc, char *argv[])
{
  // Criação dos nós:
  NodeContainer serverNode;
  serverNode.Create(1);

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(4); // Total de 32 clientes

  // Configurando o canal e PHY do Wi-Fi:
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper();
  phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);
  phy.SetChannel(channel.Create());

  // Configurando o dispositivo Wi-Fi:
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs7"),
                               "ControlMode", StringValue("HtMcs0"));

  WifiMacHelper mac;
  Ssid ssid = Ssid("EquipeX");

  // Configuração do AP:
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  // Configuração dos clientes (STA):
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  // Instalando a pilha de protocolos (Internet Stack):
  InternetStackHelper stack;
  stack.Install(serverNode);
  stack.Install(apNode);
  stack.Install(wifiStaNodes);

  // Habilitando o IP Forwarding no AP (para roteamento entre interfaces):
  Ptr<Ipv4> ipv4 = apNode.Get(0)->GetObject<Ipv4>();
  ipv4->SetAttribute("IpForward", BooleanValue(true));

  // Atribuindo endereços IP à rede Wi-Fi (192.168.0.0/24):
  Ipv4AddressHelper address;
  address.SetBase("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  // Configurando o link ponto a ponto entre o AP e o Servidor (10.1.1.0/24):
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer p2pDevices = p2p.Install(apNode.Get(0), serverNode.Get(0));

  Ipv4AddressHelper p2pAddress;
  p2pAddress.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces = p2pAddress.Assign(p2pDevices);

  // Para os clientes: posição estática (pode ser mantida ou adaptada conforme o cenário)
  MobilityHelper mobilitySta;
  mobilitySta.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(0.0),
                                   "MinY", DoubleValue(0.0),
                                   "DeltaX", DoubleValue(2.0),
                                   "DeltaY", DoubleValue(4.0),
                                   "GridWidth", UintegerValue(8),
                                   "LayoutType", StringValue("RowFirst"));
  mobilitySta.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilitySta.Install(apNode);
  mobilitySta.Install(wifiStaNodes);

  // Dividindo os clientes em dois grupos: 50% UDP e 50% TCP
  uint32_t nTotal = wifiStaNodes.GetN();
  uint32_t nUdp = nTotal / 2;
  uint32_t nTcp = nTotal - nUdp;
  NodeContainer udpNodes;
  NodeContainer tcpNodes;
  for (uint32_t i = 0; i < nTotal; i++) {
    if (i < nUdp)
      udpNodes.Add(wifiStaNodes.Get(i));
    else
      tcpNodes.Add(wifiStaNodes.Get(i));
  }

  // Configurando as aplicações no servidor:
  // Sink UDP (porta 9):
  uint16_t portUdp = 9;
  PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), portUdp));
  ApplicationContainer serverUdpApp = udpSink.Install(serverNode.Get(0));
  serverUdpApp.Start(Seconds(1.0));
  serverUdpApp.Stop(Seconds(30.0));

  // Sink TCP (porta 50000):
  uint16_t portTcp = 50000;
  PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), portTcp));
  ApplicationContainer serverTcpApp = tcpSink.Install(serverNode.Get(0));
  serverTcpApp.Start(Seconds(1.0));
  serverTcpApp.Stop(Seconds(30.0));

  // Configurando as aplicações dos clientes:
  // Aplicações UDP nos nós do grupo UDP:
  OnOffHelper udpOnOff("ns3::UdpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(1), portUdp));
  udpOnOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  udpOnOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  udpOnOff.SetAttribute("DataRate", StringValue("5Mbps"));
  udpOnOff.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer udpClientApps = udpOnOff.Install(udpNodes);
  udpClientApps.Start(Seconds(2.0));
  udpClientApps.Stop(Seconds(30.0));

  // Aplicações TCP nos nós do grupo TCP:
  BulkSendHelper tcpBulk("ns3::TcpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(1), portTcp));
  tcpBulk.SetAttribute("MaxBytes", UintegerValue(0)); // envio ilimitado
  ApplicationContainer tcpClientApps = tcpBulk.Install(tcpNodes);
  tcpClientApps.Start(Seconds(2.0));
  tcpClientApps.Stop(Seconds(30.0));

  // Habilitar o roteamento global
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Configurar o Flow Monitor para coleta de estatísticas
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

  // Configurar animação no NetAnim
  AnimationInterface anim("UDP-TCP-hybrid.xml");

  Simulator::Stop(Seconds(40.0));
  Simulator::Run();

  flowMonitor->SerializeToXmlFile("UDP-TCP-Hybrid.xml", true, true);
  Simulator::Destroy();

  return 0;
}
