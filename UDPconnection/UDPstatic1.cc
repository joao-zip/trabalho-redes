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

int main(int argc, char *argv[]) {
  // Configuração de logs (não necessariamente usados, mas permanecem)
  // LogComponentEnable("TrabalhoRedes", LOG_LEVEL_INFO);
  // LogComponentEnable("StaWifiMac", LOG_LEVEL_INFO);
  // LogComponentEnable("ApWifiMac", LOG_LEVEL_INFO);

  // Criando o nó servidor (s0)
  NodeContainer serverNode;
  serverNode.Create(1);

  // Criando o nó Access Point (AP)
  NodeContainer apNode;
  apNode.Create(1);

  // Criando os clientes sem fio (por exemplo, 32 clientes)
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(32);

  // Configurando o canal Wi-Fi
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper();
  phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);
  phy.SetChannel(channel.Create());

  // Configurando o dispositivo Wi-Fi
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs7"),
                               "ControlMode", StringValue("HtMcs0"));

  WifiMacHelper mac;
  Ssid ssid = Ssid("EquipeX");

  // Configuração do AP
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  // Configuração dos clientes
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  // Instalando a pilha de Internet
  InternetStackHelper stack;
  stack.Install(serverNode);
  stack.Install(apNode);
  stack.Install(wifiStaNodes);

  // Habilitar IP Forwarding no AP (para roteamento entre interfaces)
  Ptr<Ipv4> ipv4 = apNode.Get(0)->GetObject<Ipv4>();
  ipv4->SetAttribute("IpForward", BooleanValue(true));

  // Atribuindo endereços IP à rede Wi-Fi (192.168.0.0/24)
  Ipv4AddressHelper address;
  address.SetBase("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  // Configurando o link ponto a ponto entre o AP e o Servidor (10.1.1.0/24)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer p2pDevices = p2p.Install(apNode.Get(0), serverNode.Get(0));

  // Atribuindo endereços IP ao link ponto a ponto
  Ipv4AddressHelper p2pAddress;
  p2pAddress.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces = p2pAddress.Assign(p2pDevices);

  // Configurando a mobilidade dos nós
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                // Delta
                                "DeltaX", DoubleValue(2.0),
                                "DeltaY", DoubleValue(4.0),
                                // Tamanho da grade
                                "GridWidth", UintegerValue(8),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);

  // Aplicação UDP no servidor (escuta na porta 9)
  uint16_t port = 9;
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer serverApp = sinkHelper.Install(serverNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(30.0));

  // Aplicação UDP nos clientes – enviar para o servidor (p2pInterfaces.GetAddress(1))
  OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(1), port));
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("DataRate", StringValue("1Mbps"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = onOffHelper.Install(wifiStaNodes);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(30.0));

  // Habilitar roteamento global
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Configurar o Flow Monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  AnimationInterface anim("UDPstatic1.xml");

  Simulator::Stop(Seconds(40.0));
  Simulator::Run();

  flowMonitor->SerializeToXmlFile("flow-monitor.xml", true, true);
  Simulator::Destroy();

  return 0;
}
