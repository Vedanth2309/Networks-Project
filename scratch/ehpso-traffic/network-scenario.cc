#include "network-scenario.h"

#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/string.h"
#include "ns3/data-rate.h"

#include <stdexcept>

using namespace ns3;

NetworkScenario::NetworkScenario(uint32_t numberOfSources,
                                 double bottleneck1RateMbps,
                                 double bottleneck2RateMbps,
                                 const std::string& bottleneckDelay)
    : m_numberOfSources(numberOfSources),
      m_bottleneck1RateMbps(bottleneck1RateMbps),
      m_bottleneck2RateMbps(bottleneck2RateMbps),
      m_bottleneckDelay(bottleneckDelay)
{
    if (numberOfSources == 0 || bottleneck1RateMbps <= 0 || bottleneck2RateMbps <= 0)
        throw std::invalid_argument("Source count and bottleneck rates must be positive.");
}

void NetworkScenario::CreateNetwork()
{
    m_sourceNodes.Create(m_numberOfSources);
    m_router1 = CreateObject<Node>();
    m_router2 = CreateObject<Node>();
    m_server = CreateObject<Node>();

    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    access.SetChannelAttribute("Delay", StringValue("1ms"));
    for (uint32_t i = 0; i < m_numberOfSources; ++i)
        m_accessDevices.push_back(access.Install(m_sourceNodes.Get(i), m_router1));

    PointToPointHelper b1;
    b1.SetDeviceAttribute("DataRate", StringValue(std::to_string(m_bottleneck1RateMbps) + "Mbps"));
    b1.SetChannelAttribute("Delay", StringValue(m_bottleneckDelay));
    m_bottleneck1Devices = b1.Install(m_router1, m_router2);

    PointToPointHelper b2;

    b2.SetDeviceAttribute(
        "DataRate",
        StringValue(std::to_string(m_bottleneck2RateMbps) + "Mbps"));

    b2.SetChannelAttribute(
        "Delay",
        StringValue("5ms"));

    b2.SetQueue(
        "ns3::DropTailQueue<Packet>",
        "MaxSize",
        QueueSizeValue(QueueSize("5p")));

    m_bottleneck2Devices = b2.Install(m_router2, m_server);
}

void NetworkScenario::InstallInternet()
{
    InternetStackHelper internet;
    internet.Install(m_sourceNodes);
    internet.Install(m_router1);
    internet.Install(m_router2);
    internet.Install(m_server);
}

void NetworkScenario::AssignAddresses()
{
    Ipv4AddressHelper address;
    for (uint32_t i = 0; i < m_accessDevices.size(); ++i)
    {
        address.SetBase(("10." + std::to_string((i / 250) + 1) + "." + std::to_string((i % 250) + 1) + ".0").c_str(), "255.255.255.0");
        m_accessInterfaces.push_back(address.Assign(m_accessDevices[i]));
    }

    address.SetBase("172.16.0.0", "255.255.255.0");
    m_bottleneck1Interfaces = address.Assign(m_bottleneck1Devices);
    address.SetBase("172.17.0.0", "255.255.255.0");
    m_bottleneck2Interfaces = address.Assign(m_bottleneck2Devices);
    m_serverAddress = m_bottleneck2Interfaces.GetAddress(1);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

NodeContainer NetworkScenario::GetSourceNodes() const { return m_sourceNodes; }
Ptr<Node> NetworkScenario::GetServerNode() const { return m_server; }
Ipv4Address NetworkScenario::GetServerAddress() const { return m_serverAddress; }
