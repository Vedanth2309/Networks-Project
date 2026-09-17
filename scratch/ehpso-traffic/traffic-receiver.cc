#include "traffic-receiver.h"

#include "ns3/inet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"

using namespace ns3;

NS_OBJECT_ENSURE_REGISTERED(
    TrafficReceiver);

TypeId
TrafficReceiver::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3.TrafficReceiver")
            .SetParent<Application>()
            .SetGroupName("Applications");

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
TrafficReceiver::Setup(
    uint16_t port)
{
    m_port = port;
}

void
TrafficReceiver::StartApplication()
{
    if (!m_socket)
    {
        m_socket =
            Socket::CreateSocket(
                GetNode(),
                UdpSocketFactory::GetTypeId());

        InetSocketAddress local =
            InetSocketAddress(
                Ipv4Address::GetAny(),
                m_port);

        m_socket->Bind(local);

        m_socket->SetRecvCallback(
            MakeCallback(
                &TrafficReceiver::HandleRead,
                this));
    }
}

void
TrafficReceiver::StopApplication()
{
    if (m_socket)
    {
        m_socket->Close();

        m_socket = nullptr;
    }
}

void
TrafficReceiver::HandleRead(
    Ptr<Socket> socket)
{
    Address from;

    while (Ptr<Packet> packet =
               socket->RecvFrom(from))
    {
        if (packet->GetSize() == 0)
        {
            break;
        }

        ++m_packetsReceived;

        m_bytesReceived +=
            packet->GetSize();
    }
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