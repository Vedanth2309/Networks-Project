#include "traffic-application.h"

#include "ns3/inet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"

#include <algorithm>

using namespace ns3;

NS_OBJECT_ENSURE_REGISTERED(
    TrafficApplication);

TypeId
TrafficApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3.TrafficApplication")
            .SetParent<Application>()
            .SetGroupName("Applications");

    return tid;
}

TrafficApplication::TrafficApplication()
    : m_socket(nullptr),
      m_demandMbps(0.0),
      m_allocatedRateMbps(0.0),
      m_packetSize(1024),
      m_destination(),
      m_port(9000),
      m_running(false),
      m_packetsSent(0),
      m_deviceIndex(0),
      m_maxDelayNs(250000000ULL)
{
}

void
TrafficApplication::Setup(
    double rateMbps,
    uint32_t packetSize,
    Ipv4Address destination,
    uint16_t port,
    uint32_t deviceIndex,
    double maxDelayMs)
{
    m_demandMbps =
        std::max(0.0, rateMbps);

    m_allocatedRateMbps =
        m_demandMbps;

    m_packetSize =
        packetSize;

    m_destination =
        destination;

    m_port =
        port;

    m_deviceIndex = deviceIndex;

    m_maxDelayNs = static_cast<uint64_t>(
        std::max(0.0, maxDelayMs) * 1000000.0);
}

void
TrafficApplication::SetDemand(
    double demandMbps)
{
    m_demandMbps =
        std::max(0.0, demandMbps);

    /*
     * Do not allow allocation to remain
     * above the new demand.
     */
    if (m_allocatedRateMbps >
        m_demandMbps)
    {
        m_allocatedRateMbps =
            m_demandMbps;
    }
}

void
TrafficApplication::SetAllocatedRate(double allocatedRateMbps)
{
    m_allocatedRateMbps = std::max(0.0, allocatedRateMbps);

    // Apply rate changes immediately instead of leaving a
    // pending packet scheduled using the previous rate.
    if (m_running)
    {
        if (m_sendEvent.IsPending())
        {
            Simulator::Cancel(m_sendEvent);
        }

        ScheduleNextPacket();
    }
}

double
TrafficApplication::GetDemand() const
{
    return m_demandMbps;
}

double
TrafficApplication::GetAllocatedRate() const
{
    return m_allocatedRateMbps;
}

double
TrafficApplication::GetEffectiveRate() const
{
    return std::min(
        m_demandMbps,
        m_allocatedRateMbps);
}

uint64_t
TrafficApplication::GetPacketsSent() const
{
    return m_packetsSent;
}

void
TrafficApplication::StartApplication()
{
    m_running = true;

    if (!m_socket)
    {
        m_socket =
            Socket::CreateSocket(
                GetNode(),
                UdpSocketFactory::GetTypeId());

        m_socket->Connect(
            InetSocketAddress(
                m_destination,
                m_port));
    }

    ScheduleNextPacket();
}

void
TrafficApplication::StopApplication()
{
    m_running = false;

    if (m_sendEvent.IsPending())
    {
        Simulator::Cancel(
            m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close();

        m_socket = nullptr;
    }
}

void
TrafficApplication::SendPacket()
{
    if (!m_running ||
        !m_socket)
    {
        return;
    }

    double effectiveRate =
        GetEffectiveRate();

    if (effectiveRate <= 0.0)
    {
        ScheduleNextPacket();
        return;
    }

    Ptr<Packet> packet = Create<Packet>(m_packetSize);

    TrafficPacketTag tag(
        m_deviceIndex,
        static_cast<uint64_t>(
            Simulator::Now().GetNanoSeconds()),
        m_maxDelayNs);

    packet->AddPacketTag(tag);

    int result =
        m_socket->Send(packet);

    if (result >= 0)
    {
        ++m_packetsSent;
    }

    ScheduleNextPacket();
}

void
TrafficApplication::ScheduleNextPacket()
{
    if (!m_running)
    {
        return;
    }

    const double effectiveRate = GetEffectiveRate();

    double interval;

    if (effectiveRate <= 0.0)
    {
        interval = 0.01;
    }
    else
    {
        // Approximate IPv4 (20 bytes), UDP (8 bytes), and
        // point-to-point framing (2 bytes) overhead.
        constexpr double wireOverheadBytes = 30.0;

        const double wirePacketBytes =
            static_cast<double>(m_packetSize) + wireOverheadBytes;

        interval =
            (wirePacketBytes * 8.0) /
            (effectiveRate * 1000000.0);

        if (interval < 0.000001)
        {
            interval = 0.000001;
        }
    }

    m_sendEvent = Simulator::Schedule(
        Seconds(interval),
        &TrafficApplication::SendPacket,
        this);
}