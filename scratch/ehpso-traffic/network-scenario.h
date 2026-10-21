#ifndef NETWORK_SCENARIO_H
#define NETWORK_SCENARIO_H

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <string>
#include <vector>

class NetworkScenario
{
public:
    NetworkScenario(uint32_t numberOfSources,
                    double bottleneck1RateMbps,
                    double bottleneck2RateMbps,
                    const std::string& bottleneckDelay);

    void CreateNetwork();
    void InstallInternet();
    void AssignAddresses();

    ns3::NodeContainer GetSourceNodes() const;
    ns3::Ptr<ns3::Node> GetServerNode() const;
    ns3::Ipv4Address GetServerAddress() const;

private:
    uint32_t m_numberOfSources;
    double m_bottleneck1RateMbps;
    double m_bottleneck2RateMbps;
    std::string m_bottleneckDelay;

    ns3::NodeContainer m_sourceNodes;
    ns3::Ptr<ns3::Node> m_router1;
    ns3::Ptr<ns3::Node> m_router2;
    ns3::Ptr<ns3::Node> m_server;

    std::vector<ns3::NetDeviceContainer> m_accessDevices;
    ns3::NetDeviceContainer m_bottleneck1Devices;
    ns3::NetDeviceContainer m_bottleneck2Devices;

    std::vector<ns3::Ipv4InterfaceContainer> m_accessInterfaces;
    ns3::Ipv4InterfaceContainer m_bottleneck1Interfaces;
    ns3::Ipv4InterfaceContainer m_bottleneck2Interfaces;
    ns3::Ipv4Address m_serverAddress;
};
#endif
