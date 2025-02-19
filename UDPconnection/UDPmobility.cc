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

NS_LOG_COMPONENT_DEFINE("TrabalhoRedes");

void RunScenario(uint32_t numClients) {
  // Criando o nó servidor (s0)
  NodeContainer serverNode;
  serverNode.Create(1);

  // Criando o nó Access Point (AP) separadamente
  NodeContainer apNode;
  apNode.Create(1);

  // Criando os clientes sem fio
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(numClients); // Número variável de clientes

  // Configurando o canal Wi-Fi
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper();
  phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);
  phy.SetChannel(channel.Create());

  // Configurando o dispositivo Wi-Fi
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
  
  WifiMacHelper mac;
  Ssid ssid = Ssid("EquipeX");
  
  // Configuração do AP
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  // Configuração dos clientes
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  // Instalando a pilha de Internet
  InternetStackHelper stack;
  stack.Install(serverNode);
  stack.Install(apNode);
  stack.Install(wifiStaNodes);

  // Atribuindo endereços IP aos dispositivos Wi-Fi
  Ipv4AddressHelper address;
  address.SetBase("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  // Configurando o link ponto a ponto entre o AP e o Servidor
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

  // AP estático
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  // Configurando a mobilidade dos clientes
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                            "Distance", DoubleValue(10.0));
  mobility.Install(wifiStaNodes);

  // Aplicação UDP no servidor
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApp = echoServer.Install(serverNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Aplicação UDP nos clientes
  UdpEchoClientHelper echoClient(p2pInterfaces.GetAddress(1), 9); // Enviar para o servidor
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(wifiStaNodes);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Habilitar o roteamento no AP
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Configurar o Flow Monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  // Iniciar a simulação
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  flowMonitor->SerializeToXmlFile("udp_mobility_simulation_results.xml", true, true);

  // Finalizar a simulação
  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  
  // Rodar cenários com diferentes números de clientes
  for (uint32_t numClients : {4, 8, 16, 32}) {
    RunScenario(numClients);
  }
  
  return 0;
}
