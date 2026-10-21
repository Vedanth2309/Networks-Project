
#include "traffic-receiver.h"
#include "traffic-packet-tag.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrafficReceiver");
NS_OBJECT_ENSURE_REGISTERED(TrafficReceiver);

TypeId
TrafficReceiver::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3.TrafficReceiver")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<TrafficReceiver>();

    return tid;
}

TrafficReceiver::TrafficReceiver()
    : m_socket(nullptr),
      m_port(9000),
      m_packetsReceived(0),
      m_bytesReceived(0)
{
}

void
TrafficReceiver::Setup(uint16_t port)
{
    m_port = port;
}

void
TrafficReceiver::RegisterDevice(
    uint32_t deviceIndex,
    const std::string& deviceId)
{
    if (m_deviceStats.size() <= deviceIndex)
    {
        m_deviceStats.resize(deviceIndex + 1);
    }

    m_deviceStats[deviceIndex].deviceId = deviceId;
}

uint64_t
TrafficReceiver::GetPacketsReceived() const
{
    return m_packetsReceived;
}

uint64_t
TrafficReceiver::GetBytesReceived() const
{
    return m_bytesReceived;
}

const std::vector<TrafficReceiver::DeviceStats>&
TrafficReceiver::GetDeviceStats() const
{
    return m_deviceStats;
}

void
TrafficReceiver::StartApplication()
{
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(
            GetNode(),
            UdpSocketFactory::GetTypeId());

        const int bindResult = m_socket->Bind(
            InetSocketAddress(Ipv4Address::GetAny(), m_port));

        if (bindResult != 0)
        {
            NS_FATAL_ERROR("TrafficReceiver could not bind UDP port "
                           << m_port);
        }
    }

    m_socket->SetRecvCallback(
        MakeCallback(&TrafficReceiver::HandleRead, this));
}

void
TrafficReceiver::StopApplication()
{
    if (m_socket)
    {
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket->Close();
        m_socket = nullptr;
    }
}

void
TrafficReceiver::HandleRead(Ptr<Socket> socket)
{
    Address from;

    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        const uint32_t packetBytes = packet->GetSize();

        if (packetBytes == 0)
        {
            continue;
        }

        ++m_packetsReceived;
        m_bytesReceived += packetBytes;

        TrafficPacketTag tag;

        if (!packet->RemovePacketTag(tag))
        {
            NS_LOG_WARN("Received packet without TrafficPacketTag");
            continue;
        }

        const uint32_t index = tag.GetDeviceIndex();

        if (index >= m_deviceStats.size())
        {
            NS_LOG_WARN("Packet references unregistered device index "
                        << index);
            continue;
        }

        const uint64_t nowNs =
            static_cast<uint64_t>(Simulator::Now().GetNanoSeconds());

        const uint64_t sendNs = tag.GetSendTimeNs();
        const uint64_t delayNs =
            (nowNs >= sendNs) ? nowNs - sendNs : 0;

        DeviceStats& stats = m_deviceStats[index];

        ++stats.packetsReceived;
        stats.bytesReceived += packetBytes;
        stats.totalDelayNs += delayNs;

        if (delayNs > tag.GetMaxDelayNs())
        {
            ++stats.deadlineMisses;
        }
    }
}

void
TrafficReceiver::WriteDeviceMetrics(
    const std::string& filePath) const
{
    std::ofstream out(filePath, std::ios::trunc);

    if (!out)
    {
        std::cerr << "[Receiver] Could not write metrics to "
                  << filePath << '\n';
        return;
    }

    out << "device_id,packets_received,bytes_received,"
           "average_delay_ms,deadline_misses,deadline_miss_pct\n";

    out << std::fixed << std::setprecision(6);

    for (const DeviceStats& stats : m_deviceStats)
    {
        const double averageDelayMs =
            stats.packetsReceived > 0
                ? static_cast<double>(stats.totalDelayNs) /
                      static_cast<double>(stats.packetsReceived) /
                      1000000.0
                : 0.0;

        const double deadlineMissPct =
            stats.packetsReceived > 0
                ? 100.0 * static_cast<double>(stats.deadlineMisses) /
                      static_cast<double>(stats.packetsReceived)
                : 0.0;

        out << stats.deviceId << ','
            << stats.packetsReceived << ','
            << stats.bytesReceived << ','
            << averageDelayMs << ','
            << stats.deadlineMisses << ','
            << deadlineMissPct << '\n';
    }

    std::cout << "[Receiver] Per-device delivery metrics saved to "
              << filePath << '\n';
}
