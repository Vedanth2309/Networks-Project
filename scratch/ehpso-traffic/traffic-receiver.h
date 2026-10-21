
#ifndef TRAFFIC_RECEIVER_H
#define TRAFFIC_RECEIVER_H

#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"

#include <cstdint>
#include <string>
#include <vector>

class TrafficReceiver : public ns3::Application
{
public:
    struct DeviceStats
    {
        std::string deviceId;
        uint64_t packetsReceived{0};
        uint64_t bytesReceived{0};
        uint64_t totalDelayNs{0};
        uint64_t deadlineMisses{0};
    };

    static ns3::TypeId GetTypeId();

    TrafficReceiver();

    void Setup(uint16_t port);

    void RegisterDevice(
        uint32_t deviceIndex,
        const std::string& deviceId);

    uint64_t GetPacketsReceived() const;
    uint64_t GetBytesReceived() const;

    const std::vector<DeviceStats>& GetDeviceStats() const;

    void WriteDeviceMetrics(const std::string& filePath) const;

private:
    void StartApplication() override;
    void StopApplication() override;

    void HandleRead(ns3::Ptr<ns3::Socket> socket);

    ns3::Ptr<ns3::Socket> m_socket;
    uint16_t m_port;

    uint64_t m_packetsReceived;
    uint64_t m_bytesReceived;

    std::vector<DeviceStats> m_deviceStats;
};

#endif
