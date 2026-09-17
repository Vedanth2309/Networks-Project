#ifndef TRAFFIC_RECEIVER_H
#define TRAFFIC_RECEIVER_H

#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"

#include <cstdint>

class TrafficReceiver : public ns3::Application
{
public:
    static ns3::TypeId GetTypeId();

    TrafficReceiver();

    void Setup(uint16_t port);

    uint64_t GetPacketsReceived() const;

    uint64_t GetBytesReceived() const;

private:
    void StartApplication() override;

    void StopApplication() override;

    void HandleRead(
        ns3::Ptr<ns3::Socket> socket);

    ns3::Ptr<ns3::Socket> m_socket;

    uint16_t m_port;

    uint64_t m_packetsReceived;

    uint64_t m_bytesReceived;
};

#endif