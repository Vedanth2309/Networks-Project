#ifndef NETWORK_SCENARIO_H
#define NETWORK_SCENARIO_H

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

class NetworkScenario
{
public:
    NetworkScenario(
        uint32_t numberOfSources,
        double bottleneckRateMbps,
        const std::string& bottleneckDelay);

    void CreateNetwork();

    void InstallInternet();

    void AssignAddresses();

    ns3::NodeContainer GetSourceNodes() const;

    ns3::Ptr<ns3::Node> GetServerNode() const;

    ns3::Ipv4Address GetServerAddress() const;

private:
    uint32_t m_numberOfSources;

    double m_bottleneckRateMbps;

    std::string m_bottleneckDelay;

    ns3::NodeContainer m_sourceNodes;

    ns3::NodeContainer m_routerNodes;

    ns3::NodeContainer m_serverNode;

    ns3::NetDeviceContainer m_sourceDevices;

    ns3::NetDeviceContainer m_bottleneckDevices;

    ns3::NetDeviceContainer m_serverDevices;

    std::vector<ns3::Ipv4InterfaceContainer>
        m_sourceInterfaces;

    ns3::Ipv4InterfaceContainer
        m_bottleneckInterfaces;

    ns3::Ipv4InterfaceContainer
        m_serverInterfaces;

    ns3::Ipv4Address m_serverAddress;
};

#endif