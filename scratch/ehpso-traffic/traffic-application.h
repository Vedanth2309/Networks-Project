#ifndef TRAFFIC_APPLICATION_H
#define TRAFFIC_APPLICATION_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"

#include <cstdint>

class TrafficApplication : public ns3::Application
{
public:
    static ns3::TypeId GetTypeId();

    TrafficApplication();

    void Setup(
        double rateMbps,
        uint32_t packetSize,
        ns3::Ipv4Address destination,
        uint16_t port);

    void SetDemand(double demandMbps);

    void SetAllocatedRate(double allocatedRateMbps);

    double GetDemand() const;

    double GetAllocatedRate() const;

    double GetEffectiveRate() const;

    uint64_t GetPacketsSent() const;

private:
    void StartApplication() override;

    void StopApplication() override;

    void SendPacket();

    void ScheduleNextPacket();

    ns3::Ptr<ns3::Socket> m_socket;

    double m_demandMbps;

    double m_allocatedRateMbps;

    uint32_t m_packetSize;

    ns3::Ipv4Address m_destination;

    uint16_t m_port;

    ns3::EventId m_sendEvent;

    bool m_running;

    uint64_t m_packetsSent;
};

#endif