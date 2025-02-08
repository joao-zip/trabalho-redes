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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrabalhoRedes");

int main (int argc, char *argv[])
{
  // Configuração de nós
  NodeContainer serverNode;
  serverNode.Create(1);

  NodeContainer apNode;
  apNode.Create(1);

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

  // Configuração do AP (nó fixo)
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  // Configuração dos clientes (STA)
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

  Ipv4AddressHelper p2pAddress;
  p2pAddress.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces = p2pAddress.Assign(p2pDevices);

  // Configurando a mobilidade do AP (estático)
  MobilityHelper mobilityAp;
  // Aqui, definimos o AP para ser colocado em (25,25)
  Ptr<ListPositionAllocator> posAllocAp = CreateObject<ListPositionAllocator>();
  posAllocAp->Add(Vector(25.0, 25.0, 0.0));
  mobilityAp.SetPositionAllocator(posAllocAp);
  mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityAp.Install(apNode);

  // Configurando a mobilidade dos clientes (com mobilidade)
  // Para os clientes vamos usar o ConstantVelocityMobilityModel e depois definir
  // suas posições e velocidades manualmente.
  MobilityHelper mobilitySta;
  mobilitySta.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobilitySta.Install(wifiStaNodes);

  // Parâmetros para a posição inicial na circunferência
  double apX = 25.0;
  double apY = 25.0;
  double radius = 10.0;  // Raio da circunferência (em metros)
  uint32_t nSta = wifiStaNodes.GetN();

  // Velocidade (em m/s) com a qual os clientes vão se afastar
  double speed = 1.0;

  // Para cada cliente, calcule a posição (distribuídos uniformemente na circunferência)
  // e defina a velocidade para que se movam radialmente para fora
  for (uint32_t i = 0; i < nSta; i++) {
    Ptr<Node> node = wifiStaNodes.Get(i);
    // Obtemos o modelo de mobilidade e garantimos que é do tipo ConstantVelocityMobilityModel
    Ptr<ConstantVelocityMobilityModel> mob = node->GetObject<ConstantVelocityMobilityModel>();

    // Distribuição uniforme: cada nó recebe um ângulo diferente
    double angle = 2 * M_PI * i / nSta; // ângulo em radianos
    double x = apX + radius * std::cos(angle);
    double y = apY + radius * std::sin(angle);
    Vector pos(x, y, 0.0);
    mob->SetPosition(pos);

    // Calcula o vetor direção para se afastar do AP (do AP para a posição do nó)
    double dx = x - apX;
    double dy = y - apY;
    double norm = std::sqrt(dx*dx + dy*dy);
    // Normaliza e multiplica pela velocidade
    Vector velocity(speed * dx / norm, speed * dy / norm, 0.0);
    mob->SetVelocity(velocity);

    // (Opcional) Imprimir a posição e velocidade para verificação
    std::cout << "STA " << i << " pos: (" << x << ", " << y << "), vel: ("
              << velocity.x << ", " << velocity.y << ")" << std::endl;
  }

  // Aplicação UDP no servidor (escuta na porta 9)
  uint16_t port = 9;
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer serverApp = sinkHelper.Install(serverNode.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(30.0));

  // Aplicação UDP nos clientes – enviar para o servidor (usando o endereço do servidor no link p2p)
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

  // Configurar o Flow Monitor para coletar estatísticas
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

  Simulator::Stop(Seconds(40.0));
  Simulator::Run();

  flowMonitor->SerializeToXmlFile("UDP-mobility.xml", true, true);
  Simulator::Destroy();

  return 0;
}
