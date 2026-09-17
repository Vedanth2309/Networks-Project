#include "network-scenario.h"

#include <sstream>

using namespace ns3;

NetworkScenario::NetworkScenario(
    uint32_t numberOfSources,
    double bottleneckRateMbps,
    const std::string& bottleneckDelay)
    : m_numberOfSources(numberOfSources),
      m_bottleneckRateMbps(bottleneckRateMbps),
      m_bottleneckDelay(bottleneckDelay)
{
}

void
NetworkScenario::CreateNetwork()
{
    /*
     * -----------------------------------------
     * IoT source nodes
     * -----------------------------------------
     */

    m_sourceNodes.Create(
        m_numberOfSources);

    /*
     * -----------------------------------------
     * Router nodes
     * -----------------------------------------
     */

    m_routerNodes.Create(2);

    /*
     * -----------------------------------------
     * Edge server
     * -----------------------------------------
     */

    m_serverNode.Create(1);

    /*
     * -----------------------------------------
     * IoT → Router 0
     * -----------------------------------------
     */

    PointToPointHelper sourceLink;

    sourceLink.SetDeviceAttribute(
        "DataRate",
        StringValue("10Mbps"));

    sourceLink.SetChannelAttribute(
        "Delay",
        StringValue("2ms"));

    /*
     * -----------------------------------------
     * Router 0 → Router 1 bottleneck
     * -----------------------------------------
     */

    PointToPointHelper bottleneckLink;

    bottleneckLink.SetDeviceAttribute(
        "DataRate",
        StringValue(
            std::to_string(
                m_bottleneckRateMbps) +
            "Mbps"));

    bottleneckLink.SetChannelAttribute(
        "Delay",
        StringValue(
            m_bottleneckDelay));

    /*
     * -----------------------------------------
     * Router 1 → Edge Server
     * -----------------------------------------
     */

    PointToPointHelper serverLink;

    serverLink.SetDeviceAttribute(
        "DataRate",
        StringValue("10Mbps"));

    serverLink.SetChannelAttribute(
        "Delay",
        StringValue("2ms"));

    /*
     * Install source links.
     */

    for (uint32_t i = 0;
         i < m_numberOfSources;
         ++i)
    {
        NetDeviceContainer devices =
            sourceLink.Install(
                m_sourceNodes.Get(i),
                m_routerNodes.Get(0));

        m_sourceDevices.Add(
            devices);
    }

    /*
     * Bottleneck.
     */

    m_bottleneckDevices =
        bottleneckLink.Install(
            m_routerNodes.Get(0),
            m_routerNodes.Get(1));

    /*
     * Server link.
     */

    m_serverDevices =
        serverLink.Install(
            m_routerNodes.Get(1),
            m_serverNode.Get(0));
}

void
NetworkScenario::InstallInternet()
{
    InternetStackHelper internet;

    internet.Install(
        m_sourceNodes);

    internet.Install(
        m_routerNodes);

    internet.Install(
        m_serverNode);
}

void
NetworkScenario::AssignAddresses()
{
    Ipv4AddressHelper address;

    /*
     * Each IoT source gets its own subnet.
     */

    uint32_t deviceIndex = 0;

    for (uint32_t i = 0;
         i < m_numberOfSources;
         ++i)
    {
        std::ostringstream subnet;

        subnet
            << "10.1."
            << i + 1
            << ".0";

        address.SetBase(
            Ipv4Address(
                subnet.str().c_str()),
            Ipv4Mask("255.255.255.0"));

        NetDeviceContainer pair;

        pair.Add(
            m_sourceDevices.Get(deviceIndex));

        pair.Add(
            m_sourceDevices.Get(deviceIndex + 1));

        Ipv4InterfaceContainer interfaces =
            address.Assign(pair);

        m_sourceInterfaces.push_back(
            interfaces);

        deviceIndex += 2;
    }

    /*
     * Bottleneck subnet.
     */

    address.SetBase(
        "10.100.0.0",
        "255.255.255.0");

    m_bottleneckInterfaces =
        address.Assign(
            m_bottleneckDevices);

    /*
     * Server subnet.
     */

    address.SetBase(
        "10.200.0.0",
        "255.255.255.0");

    m_serverInterfaces =
        address.Assign(
            m_serverDevices);

    /*
     * Server address.
     *
     * Router1 = 10.200.0.1
     * Server  = 10.200.0.2
     */

    m_serverAddress =
        m_serverInterfaces.GetAddress(1);

    /*
     * Enable routing.
     */

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

NodeContainer
NetworkScenario::GetSourceNodes() const
{
    return m_sourceNodes;
}

Ptr<Node>
NetworkScenario::GetServerNode() const
{
    return m_serverNode.Get(0);
}

Ipv4Address
NetworkScenario::GetServerAddress() const
{
    return m_serverAddress;
}