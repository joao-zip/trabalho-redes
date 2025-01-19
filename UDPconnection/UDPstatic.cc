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

void RunScenario(uint32_t numClients, std::ofstream &outputFile) {
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
  p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
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

  // Clientes com posição inicial fixa
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);

  // Aplicação UDP no servidor
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApp = echoServer.Install(serverNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(30.0));

  // Aplicação UDP nos clientes
  UdpEchoClientHelper echoClient(p2pInterfaces.GetAddress(1), 9); // Enviar para o servidor
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(wifiStaNodes);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(30.0));

  // Habilitar o roteamento no AP
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Configurar o Flow Monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  // Iniciar a simulação
  Simulator::Stop(Seconds(30.0));
  Simulator::Run();

  // Analisar métricas do Flow Monitor
  flowMonitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

  outputFile << "NumClients,FlowID,SourceAddress,DestinationAddress,Throughput(Mbps),Delay(s),LostPackets,ReceivedPackets\n";

  for (auto it = stats.begin(); it != stats.end(); ++it) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
    double throughput = 0.0;
    double delay = 0.0;

    if (it->second.rxPackets > 0) {
      throughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
      delay = it->second.delaySum.GetSeconds() / it->second.rxPackets;
    }

    outputFile << numClients << ","
               << it->first << ","
               << t.sourceAddress << ","
               << t.destinationAddress << ","
               << std::fixed << std::setprecision(6) // Mais precisão para throughput e delay
               << throughput << ","
               << delay << ","
               << it->second.lostPackets << ","
               << it->second.rxPackets << "\n";
  }

  outputFile << "\n";

  // Finalizar a simulação
  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  // Criar arquivo para saída dos resultados
  std::ofstream outputFile("simulation_results.csv");

  // Adicionar cabeçalho geral
  outputFile << "NumClients,FlowID,SourceAddress,DestinationAddress,Throughput(Mbps),Delay(s),LostPackets,ReceivedPackets\n";

  // Rodar cenários com diferentes números de clientes
  for (uint32_t numClients : {4, 8, 16, 32}) {
    RunScenario(numClients, outputFile);
  }

  outputFile.close();
  return 0;
}
