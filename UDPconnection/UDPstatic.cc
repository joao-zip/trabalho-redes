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
  // Criando o nó servidor
  NodeContainer serverNode;
  serverNode.Create(1);

  // Criando o nó Access Point (AP)
  NodeContainer apNode;
  apNode.Create(1);

  // Criando os clientes sem fio
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(numClients);

  // Configurando o canal Wi-Fi
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper();
  phy.SetChannel(channel.Create());

  // Configurando o dispositivo Wi-Fi
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager");

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

  // Atribuindo endereços IP
  Ipv4AddressHelper address;
  address.SetBase("192.168.0.0", "255.255.255.0");
  address.Assign(apDevice);
  address.Assign(staDevices);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer p2pDevices = p2p.Install(apNode.Get(0), serverNode.Get(0));

  Ipv4AddressHelper p2pAddress;
  p2pAddress.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces = p2pAddress.Assign(p2pDevices);

  // Configurando a mobilidade dos nós
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  mobility.Install(wifiStaNodes);

  // Aplicação PacketSink (servidor)
  uint16_t port = 9;
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer serverApp = sinkHelper.Install(serverNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(20.0));

  // Aplicação OnOff nos clientes
  OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(p2pInterfaces.GetAddress(0), port));
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute("DataRate", StringValue("10Mbps"));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = onOffHelper.Install(wifiStaNodes);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  // Habilitar o roteamento no AP
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Configurar o Flow Monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  // Iniciar a simulação
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();

  // Analisar métricas do Flow Monitor
  flowMonitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

  outputFile << "NumClients,FlowID,Throughput(Mbps),AvgDelay(ms),PacketLoss(%)\n";

  for (auto it = stats.begin(); it != stats.end(); ++it) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);

    double throughput = it->second.rxBytes * 8.0 / 
                        (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
    double avgDelay = it->second.rxPackets > 0 ? 
                      (it->second.delaySum.GetSeconds() / it->second.rxPackets * 1000) : 0;
    double packetLoss = (it->second.txPackets > 0) ? 
                        (double(it->second.txPackets - it->second.rxPackets) / it->second.txPackets * 100) : 0;

    outputFile << numClients << "," << it->first << "," 
               << std::fixed << std::setprecision(2) 
               << throughput << "," << avgDelay << "," << packetLoss << "\n";
  }

  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  std::ofstream outputFile("simulation_results.csv");
  outputFile << "NumClients,FlowID,Throughput(Mbps),AvgDelay(ms),PacketLoss(%)\n";

  for (uint32_t numClients : {4, 8, 16, 32}) {
    RunScenario(numClients, outputFile);
  }

  outputFile.close();

  return 0;
}
